/*
 * SD2Cloud -- finds the sd2psx memory cards on the microSD (sd2psXtd firmware, src/ps2/ps2_cardman.c):
 *   MemoryCards/PS2/<folder>/<folder>-<channel>.mcd     folder = CardN, a game ID (Game ID) or any name
 *   MemoryCards/PS2/BOOT/BootCard-<channel>.mcd          (or BootCard.mcd, the old layout)
 * Each channel's name may be in <folder>/<folder>.ini (BootCard.ini in BOOT), section [ChannelName].
 * The folders mapped in .sd2psx/Game2Folder.ini count as game cards.
 * A MemCard PRO2 keeps them the same way under other names (dev, in system.c): PS2/<folder>/<folder>-<channel>.mc2,
 * folder = MemoryCardN or a game ID, with none of the sd2psx's .ini files and no folder of boot cards.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include "common.h"

card_t cards[MAX_CARDS];
int nCards;

#define MAX_NAMES 512
static char names[MAX_NAMES][64];
static char mapped[1024];   /* folders from Game2Folder.ini, "a,b,c" */

/* reads the names of a whole folder and closes it before opening anything else (one operation at a time on the
 * sd2psx) */
static int list_dir(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *e;
    int n = 0;
    if (!d)
        return -1;
    while ((e = readdir(d)) != NULL && n < MAX_NAMES) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        snprintf(names[n++], sizeof(names[0]), "%s", e->d_name);
    }
    closedir(d);
    return n;
}

/* SLUS-21065, SCES-50490, SLPM-12345... */
int is_game_id(const char *p)
{
    int i;
    if (strlen(p) != 10 || p[4] != '-')
        return 0;
    for (i = 0; i < 4; i++)
        if (!isupper((unsigned char)p[i]))
            return 0;
    for (i = 5; i < 10; i++)
        if (!isdigit((unsigned char)p[i]))
            return 0;
    return 1;
}

static int is_cardn(const char *p)
{
    size_t n = strlen(dev->numbered);
    if (strncasecmp(p, dev->numbered, n) != 0 || !p[n])
        return 0;
    for (p += n; *p; p++)
        if (!isdigit((unsigned char)*p))
            return 0;
    return 1;
}

static void on_game2folder(const char *s, const char *k, const char *v, void *u)
{
    (void)k;
    (void)u;
    if (strcasecmp(s, "PS2") == 0 && *v && !list_has(mapped, v) && strlen(mapped) + strlen(v) + 2 < sizeof(mapped)) {
        strcat(mapped, v);
        strcat(mapped, ",");
    }
}

/* channel names from <folder>.ini: the ini_read callback with the range of that folder's cards */
typedef struct {
    int first, end;
} range_t;
static void on_channel(const char *s, const char *k, const char *v, void *u)
{
    range_t *r = u;
    int i, channel;
    if (strcasecmp(s, "ChannelName") != 0)
        return;
    channel = atoi(k);
    for (i = r->first; i < r->end; i++)
        if (cards[i].channel == channel) {
            snprintf(cards[i].name, sizeof(cards[i].name), "%s", v);
            utf8_fix(cards[i].name, sizeof(cards[i].name));
        }
}

static int type_order(int t) { return t == TYPE_NORMAL ? 0 : t == TYPE_GAMEID ? 1 : t == TYPE_NAMED ? 2 : 3; }

static int compare(const void *a, const void *b)
{
    const card_t *x = a, *y = b;
    int d = type_order(x->type) - type_order(y->type);
    if (d)
        return d;
    if (x->type == TYPE_NORMAL) {   /* Card2 before Card10 */
        d = atoi(x->folder + strlen(dev->numbered)) - atoi(y->folder + strlen(dev->numbered));
        if (d)
            return d;
    }
    d = strcasecmp(x->folder, y->folder);
    return d ? d : x->channel - y->channel;
}

/* The game of each game card. The sd2psx names a game's folder by its ID and shows the game's name on its own
 * screen, from a list in its firmware; the same list is embedded here (assets/gamenames.txt, one "ID<TAB>name" per
 * line: tools/make_gamenames.py). A folder that isn't an ID in it is named as the group of games Game2Folder.ini
 * gives it to (group_names, below), or keeps no name */
extern unsigned char asset_gamenames_txt[];
extern unsigned int size_asset_gamenames_txt;

/* -------- A folder Game2Folder.ini gives to several games (a series whose games read each other's saves: the list
 * most people use, SD2PSX-VMC-Groups, has over a hundred of them, "MCCG-10045" and so on) is no game's ID, and the
 * list of games has no name for it. It is named after the games it is for: what their names all start with, when
 * that is more than a word ("Need for Speed"); else the names one after the other, when that is short ("Zombie
 * Zone, Zombie Hunters"); else the one word they start with ("Tekken"); else as many of the names as fit, and
 * "...". The games of America and of Europe come first: the same game has another name in Japan */
#define GROUP_MAX   48    /* folders like that among the cards */
#define GROUP_IDS   512   /* the IDs Game2Folder.ini gives them, all together */
#define GROUP_NAMES 8     /* the names kept of each one's games */
#define GROUP_SHORT 40    /* names one after the other are short up to this many characters */

typedef struct {
    const char *folder;
    char names[GROUP_NAMES][64], prefix[64];   /* its games, each name once; what all of them start with */
    int n, more;                               /* names kept, and whether its games have others */
} group_t;
static group_t groups[GROUP_MAX];
static struct {
    char id[11];
    unsigned char group;
} groupIds[GROUP_IDS];
static unsigned char groupKey[1000];   /* is an ID that ends in these three digits one of groupIds? */
static int nGroups, nGroupIds;

static int id_key(const char *id) { return (id[7] - '0') * 100 + (id[8] - '0') * 10 + (id[9] - '0'); }

static void on_group_id(const char *s, const char *k, const char *v, void *u)
{
    int i;
    (void)u;
    if (strcasecmp(s, "PS2") != 0 || !is_game_id(k))
        return;
    for (i = 0; i < nCards; i++)   /* a card in that game's own folder: the device has another one for the game now */
        if (cards[i].type == TYPE_GAMEID && strcasecmp(v, k) != 0 && !strcasecmp(cards[i].folder, k))
            snprintf(cards[i].moved, sizeof(cards[i].moved), "%s", v);
    if (nGroupIds == GROUP_IDS)
        return;
    for (i = 0; i < nGroups; i++)
        if (!strcasecmp(groups[i].folder, v)) {
            snprintf(groupIds[nGroupIds].id, sizeof(groupIds[0].id), "%s", k);
            groupIds[nGroupIds++].group = i;
            groupKey[id_key(k)] = 1;
            return;
        }
}

static int word_char(char c) { return isalnum((unsigned char)c) || ((unsigned char)c & 0x80); }

/* one more game of a group: its name without what tells an edition of it apart ("[Black Edition]"), once */
static void group_add(group_t *g, const char *name, int len)
{
    char n[64], *cut;
    size_t same;
    int i;
    snprintf(n, sizeof(n), "%.*s", len, name);
    if ((cut = strstr(n, " [")) != NULL)
        *cut = 0;
    utf8_fix(n, sizeof(n));
    for (i = 0; i < g->n; i++)
        if (!strcasecmp(g->names[i], n))
            return;
    if (!g->n)
        snprintf(g->prefix, sizeof(g->prefix), "%s", n);
    if (g->n < GROUP_NAMES)
        snprintf(g->names[g->n++], sizeof(g->names[0]), "%s", n);
    else
        g->more = 1;
    /* what they all start with: up to where this one goes another way, and never to the middle of a word or of a
     * letter of more than one byte */
    for (same = 0; g->prefix[same] && g->prefix[same] == n[same]; same++)
        ;
    while (same > 0 && ((unsigned char)g->prefix[same] & 0xC0) == 0x80)
        same--;
    if (same > 0 && word_char(g->prefix[same - 1]) && (word_char(g->prefix[same]) || word_char(n[same])))
        while (same > 0 && word_char(g->prefix[same - 1]))
            same--;
    g->prefix[same] = 0;
}

/* What the names of a group all start with, as a name of its own: without what joined it to the rest (" - ") and
 * without a word that only leads to the rest ("Avatar - The"). Returns how many words are left of it */
static int group_prefix(const group_t *g, char *out, size_t size)
{
    static const char *const leads[] = {"the", "of", "and", "a", "an", "to", "no", "vol.", "vol", NULL};
    size_t p = strlen(g->prefix), w;
    int i, words = 0;
    snprintf(out, size, "%s", g->prefix);
    for (;;) {
        while (p > 0 && strchr(" -:,&(/", out[p - 1]))
            p--;
        out[p] = 0;
        for (w = p; w > 0 && out[w - 1] != ' '; w--)
            ;
        for (i = 0; leads[i] && strcasecmp(out + w, leads[i]); i++)
            ;
        if (!leads[i] || !p)
            break;
        p = w;
    }
    for (w = 0; w < p; w++)
        words += out[w] != ' ' && (!w || out[w - 1] == ' ');
    return words;
}

static void group_label(const group_t *g, char *out, size_t size)
{
    char all[GROUP_NAMES * 66], start[64];
    int i, words = group_prefix(g, start, sizeof(start));
    all[0] = 0;
    for (i = 0; i < g->n; i++)
        snprintf(all + strlen(all), sizeof(all) - strlen(all), "%s%s", i ? ", " : "", g->names[i]);
    if (g->n == 1 && !g->more)
        snprintf(out, size, "%s", g->names[0]);
    else if (words >= 2)
        snprintf(out, size, "%s", start);
    else if (!g->more && strlen(all) <= GROUP_SHORT)
        snprintf(out, size, "%s", all);
    else if (strlen(start) >= 4)
        snprintf(out, size, "%s", start);
    else
        out[0] = 0;
    if (out[0])
        return;
    for (i = 0; i < g->n && strlen(out) + (i ? 2 : 0) + strlen(g->names[i]) + 4 <= size; i++)
        snprintf(out + strlen(out), size - strlen(out), "%s%s", i ? ", " : "", g->names[i]);
    if (!i)
        snprintf(out, size - 3, "%s", g->names[0]);
    if (i < g->n || g->more)
        snprintf(out + strlen(out), size - strlen(out), "...");
}

/* The game cards the list of games has no name for: named as their group, when Game2Folder.ini has them as one.
 * And, from the same reading, the cards that file left behind: the ones in a game's own folder, when it gives the
 * game another folder (moved) */
static void group_names(void)
{
    const char *p = (const char *)asset_gamenames_txt, *end = p + size_asset_gamenames_txt, *tab, *nl;
    char path[64];
    int i, k, pass, ids = 0;
    nGroups = nGroupIds = 0;
    for (i = 0; i < nCards; i++) {
        cards[i].moved[0] = 0;
        ids |= cards[i].type == TYPE_GAMEID && is_game_id(cards[i].folder);
        if (cards[i].type != TYPE_GAMEID || cards[i].game[0])
            continue;
        for (k = 0; k < nGroups && strcasecmp(groups[k].folder, cards[i].folder); k++)
            ;
        if (k == nGroups && nGroups < GROUP_MAX) {
            memset(&groups[k], 0, sizeof(groups[0]));
            groups[nGroups++].folder = cards[i].folder;
        }
    }
    if ((!nGroups && !ids) || !dev->sd2psx)
        return;
    memset(groupKey, 0, sizeof(groupKey));
    snprintf(path, sizeof(path), "%s.sd2psx/Game2Folder.ini", sdRoot);
    ini_read(path, on_group_id, NULL);
    if (!nGroupIds)
        return;
    for (pass = 0; pass < 2; pass++)   /* (SLUS, SCES and the like first) */
        for (p = (const char *)asset_gamenames_txt; p < end && (tab = memchr(p, '\t', end - p)) != NULL; p = nl + 1) {
            if (!(nl = memchr(tab, '\n', end - tab)))
                nl = end;
            if (tab - p != 10 || (p[2] == 'U' || p[2] == 'E') == pass || !isdigit((unsigned char)p[7]) ||
                !isdigit((unsigned char)p[8]) || !isdigit((unsigned char)p[9]) || !groupKey[id_key(p)])
                continue;
            for (k = 0; k < nGroupIds; k++)
                if (!strncasecmp(groupIds[k].id, p, 10))
                    group_add(&groups[groupIds[k].group], tab + 1, (int)(nl - tab - 1));
        }
    for (i = 0; i < nCards; i++)
        for (k = 0; k < nGroups && cards[i].type == TYPE_GAMEID && !cards[i].game[0]; k++)
            if (groups[k].n && !strcasecmp(groups[k].folder, cards[i].folder)) {
                group_label(&groups[k], cards[i].game, sizeof(cards[i].game));
                break;
            }
}

static void game_names(void)
{
    const char *p = (const char *)asset_gamenames_txt, *end = p + size_asset_gamenames_txt, *tab, *nl;
    int i, any = 0;
    for (i = 0; i < nCards; i++)
        any |= cards[i].type == TYPE_GAMEID;
    for (; any && p < end && (tab = memchr(p, '\t', end - p)) != NULL; p = nl + 1) {
        if (!(nl = memchr(tab, '\n', end - tab)))
            nl = end;
        for (i = 0; i < nCards; i++) {
            card_t *c = &cards[i];
            if (c->type != TYPE_GAMEID || strlen(c->folder) != (size_t)(tab - p) || strncasecmp(c->folder, p, tab - p))
                continue;
            snprintf(c->game, sizeof(c->game), "%.*s", (int)(nl - tab - 1), tab + 1);
            utf8_fix(c->game, sizeof(c->game));
        }
    }
    if (any)
        group_names();
}

int game_title(const char *id, char *out, size_t size)
{
    const char *p = (const char *)asset_gamenames_txt, *end = p + size_asset_gamenames_txt, *tab, *nl;
    size_t n = strlen(id);
    for (; p < end && (tab = memchr(p, '\t', end - p)) != NULL; p = nl + 1) {
        if (!(nl = memchr(tab, '\n', end - tab)))
            nl = end;
        if ((size_t)(tab - p) == n && !strncasecmp(id, p, n)) {
            snprintf(out, size, "%.*s", (int)(nl - tab - 1), tab + 1);
            utf8_fix(out, size);
            return 1;
        }
    }
    out[0] = 0;
    return 0;
}

/* Game2Folder.ini: [PS2], "ID = folder" */
typedef struct {
    const char *id;
    char *out;
    size_t size;
} folder_of_t;
static void on_folder_of(const char *s, const char *k, const char *v, void *u)
{
    folder_of_t *f = u;
    if (!strcmp(s, "PS2") && !strcmp(k, f->id) && *v)
        snprintf(f->out, f->size, "%s", v);
}

void game_folder(const char *id, char *out, size_t size)
{
    folder_of_t f = {id, out, size};
    char path[64];
    snprintf(out, size, "%s", id);
    snprintf(path, sizeof(path), "%s.sd2psx/Game2Folder.ini", sdRoot);
    if (dev->sd2psx)
        ini_read(path, on_folder_of, &f);
}

static void on_max_channels(const char *s, const char *k, const char *v, void *u)
{
    if (!strcmp(s, "Settings") && !strcmp(k, "MaxChannels") && atoi(v) > 0 && atoi(v) <= 255)
        *(int *)u = atoi(v);
}

/* a folder's .ini, which the sd2psx reads: BootCard.ini for its boot cards, else named after the folder */
static void folder_ini(const char *folder, char *out, size_t size)
{
    snprintf(out, size, "%s%s/%s/%s.ini", sdRoot, dev->cards, folder, strcasecmp(folder, "BOOT") ? folder : "BootCard");
}

int max_channels(const char *folder)
{
    char path[260];
    int n = 8;
    folder_ini(folder, path, sizeof(path));
    if (dev->sd2psx)
        ini_read(path, on_max_channels, &n);
    return n;
}

int max_channels_set(const char *folder, int n)
{
    char path[260], v[8];
    int r;
    folder_ini(folder, path, sizeof(path));
    snprintf(v, sizeof(v), "%d", n);
    r = ini_set(path, "Settings", "MaxChannels", v);
    log_msg("%s: MaxChannels = %d (%d)", path, n, r);
    return r;
}

/* a file of a folder of cards, as the next of cards[] when it is the .mcd of one of that folder's channels (1) */
static int add_file(const char *base, const char *folder, const char *file)
{
    int boot = dev->sd2psx && !strcasecmp(folder, "BOOT"), channel;
    const char *prefix = boot ? "BootCard" : folder, *dash;
    size_t len = strlen(file);
    card_t *c;
    if (nCards == MAX_CARDS || len < 5 || strcasecmp(file + len - 4, dev->ext) != 0)
        return 0;
    if (boot && !strcasecmp(file, "BootCard.mcd"))
        channel = 1;   /* old layout, a single channel */
    else {
        dash = file + strlen(prefix);
        if (strncasecmp(file, prefix, strlen(prefix)) != 0 || *dash != '-' || !isdigit((unsigned char)dash[1]))
            return 0;
        channel = atoi(dash + 1);
    }
    c = &cards[nCards++];
    memset(c, 0, sizeof(*c));
    snprintf(c->folder, sizeof(c->folder), "%s", folder);
    snprintf(c->base, sizeof(c->base), "%.*s", (int)(len - 4), file);
    snprintf(c->id, sizeof(c->id), "%s/%.*s", folder, (int)(len - 4), file);
    snprintf(c->path, sizeof(c->path), "%s/%s/%s", base, folder, file);
    c->channel = channel;
    c->type = boot ? TYPE_BOOT : is_cardn(folder) ? TYPE_NORMAL
              : (is_game_id(folder) || list_has(mapped, folder)) ? TYPE_GAMEID : TYPE_NAMED;
    return 1;
}

/* the size of each card from first on (one file open at a time) and their channel names, from their folder's .ini */
static void fill_cards(const char *base, const char *folder, int first)
{
    range_t r = {first, nCards};
    char path[260];
    int j;
    for (j = first; j < nCards; j++) {
        int fd = open(cards[j].path, O_RDONLY);
        if (fd >= 0) {
            cards[j].size = lseek(fd, 0, SEEK_END);
            close(fd);
        }
    }
    snprintf(path, sizeof(path), "%s/%s/%s.ini", base, folder, strcasecmp(folder, "BOOT") ? folder : "BootCard");
    if (nCards > first && dev->sd2psx)
        ini_read(path, on_channel, &r);
}

/* is that card part of the sync, by the settings? One in exclude never is; one in include is; any other is by its
 * kind, unless only the listed ones are (mode = list) */
static int wanted(const card_t *c)
{
    if (list_has(cfg.exclude, c->id))
        return 0;
    return list_has(cfg.include, c->id) || (!cfg.list_mode && (cfg.types & c->type));
}

static void set_included(card_t *c)
{
    c->included = wanted(c);
    c->status = ST_NEW;
}

int cards_scan(void)
{
    static char folders[MAX_NAMES][64];
    char base[96], path[260];
    int nFolders, i, j;

    nCards = 0;
    mapped[0] = 0;
    snprintf(path, sizeof(path), "%s.sd2psx/Game2Folder.ini", sdRoot);
    if (dev->sd2psx)
        ini_read(path, on_game2folder, NULL);

    snprintf(base, sizeof(base), "%s%s", sdRoot, dev->cards);
    nFolders = list_dir(base);
    if (nFolders < 0)
        return -1;
    memcpy(folders, names, sizeof(names[0]) * nFolders);

    for (i = 0; i < nFolders && nCards < MAX_CARDS; i++) {
        int n, first = nCards;
        snprintf(path, sizeof(path), "%s/%s", base, folders[i]);
        if ((n = list_dir(path)) <= 0)
            continue;   /* a loose file or an empty folder */
        for (j = 0; j < n; j++)
            add_file(base, folders[i], names[j]);
        fill_cards(base, folders[i], first);
    }
    if (nCards == MAX_CARDS)
        log_msg("more memory cards than SD2Cloud handles (%d): the rest are left out", MAX_CARDS);
    qsort(cards, nCards, sizeof(cards[0]), compare);
    for (i = 0; i < nCards; i++)
        set_included(&cards[i]);
    game_names();
    log_msg("%d card(s) on the microSD", nCards);
    return nCards;
}

card_t *cards_append(const char *folder, const char *file)
{
    char base[96];
    snprintf(base, sizeof(base), "%s%s", sdRoot, dev->cards);
    if (!add_file(base, folder, file))
        return NULL;
    fill_cards(base, folder, nCards - 1);
    set_included(&cards[nCards - 1]);
    game_names();
    log_msg("%s joined the cards: %d on the microSD", cards[nCards - 1].id, nCards);
    return &cards[nCards - 1];
}

void cards_sort(void) { qsort(cards, nCards, sizeof(cards[0]), compare); }

card_t *cards_add(const char *folder, const char *file)
{
    char id[96];
    int i;
    if (!cards_append(folder, file))
        return NULL;
    snprintf(id, sizeof(id), "%s", cards[nCards - 1].id);
    cards_sort();
    for (i = 0; i < nCards && strcmp(cards[i].id, id); i++)
        ;
    return i < nCards ? &cards[i] : NULL;
}

/* fingerprint of one card and its status against the last backup; migrated = an old fingerprint was adopted */
static void check_one(card_t *c, int *migrated)
{
    card_state_t *e;
    int saves = 0;
    /* (the root folder's signature comes from the same reading: it tells whether a card may be the one in the device) */
    if (mcfs_fingerprint_root(c->path, c->fingerprint, &saves, c->rootSig) != 0) {
        c->status = ST_ERROR;
        log_msg("%s: couldn't read the index: %s", c->id, mcfs_last_error());
        return;
    }
    e = state_card(c->id, 0);
    if (e && e->fingerprint[0] && e->version != MCFS_VERSION) {
        /* fingerprint from an older SD2Cloud: it can't be compared. Adopt the new one without counting it as a
         * change (otherwise the update would upload every card again at the next IGR) */
        snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", c->fingerprint);
        e->version = MCFS_VERSION;
        *migrated = 1;
        log_msg("%s: old fingerprint, adopted the version %d one", c->id, MCFS_VERSION);
    }
    if (!e || !e->fingerprint[0])
        c->status = ST_NEW;
    else if (strcmp(e->fingerprint, c->fingerprint) != 0)
        c->status = ST_CHANGED;
    else
        c->status = e->sha[0] ? ST_UP_TO_DATE : ST_NO_BACKUP;
    log_msg("%s: %d folders, fingerprint %.16s, %s", c->id, saves, c->fingerprint,
            c->status == ST_NEW ? "new" : c->status == ST_CHANGED ? "changed" : c->status == ST_UP_TO_DATE ? "up to date" : "no backup");
}

void cards_check(void (*progress)(int i, int n, const card_t *c))
{
    int i, k = 0, total = 0, migrated = 0;
    for (i = 0; i < nCards; i++)
        total += cards[i].included;
    for (i = 0; i < nCards; i++) {
        if (!cards[i].included)
            continue;
        if (progress)
            progress(k++, total, &cards[i]);
        check_one(&cards[i], &migrated);
    }
    if (migrated)
        state_write();
}

void cards_recheck(card_t *c)
{
    int migrated = 0;
    check_one(c, &migrated);
    if (migrated)
        state_write();
}

int card_set_included(card_t *c, int on)
{
    char include[sizeof(cfg.include)], exclude[sizeof(cfg.exclude)];
    int i, r = 0;
    snprintf(include, sizeof(include), "%s", cfg.include);
    snprintf(exclude, sizeof(exclude), "%s", cfg.exclude);
    if (!on) {
        list_remove(cfg.include, sizeof(cfg.include), c->id);
        if (wanted(c))
            r = list_add(cfg.exclude, sizeof(cfg.exclude), c->id);
    } else {
        list_remove(cfg.exclude, sizeof(cfg.exclude), c->id);
        if (list_has(cfg.exclude, c->id)) {   /* its whole folder is left out: the other cards of it stay out */
            list_remove(cfg.exclude, sizeof(cfg.exclude), c->folder);
            for (i = 0; i < nCards && r == 0; i++)
                if (&cards[i] != c && !strcmp(cards[i].folder, c->folder))
                    r = list_add(cfg.exclude, sizeof(cfg.exclude), cards[i].id);
        }
        if (r == 0 && !wanted(c))
            r = list_add(cfg.include, sizeof(cfg.include), c->id);
    }
    if (r == 0 && strcmp(include, cfg.include) != 0)
        r = config_set("cards", "include", cfg.include);
    if (r == 0 && strcmp(exclude, cfg.exclude) != 0)
        r = config_set("cards", "exclude", cfg.exclude);
    log_msg("%s: %s the sync by the user (%d)", c->id, on ? "part of" : "left out of", r);
    if (r != 0) {   /* nothing is taken as changed */
        snprintf(cfg.include, sizeof(cfg.include), "%s", include);
        snprintf(cfg.exclude, sizeof(cfg.exclude), "%s", exclude);
        return -1;
    }
    c->included = wanted(c);
    if (c->included)   /* (a card left out of the sync was never read) */
        cards_recheck(c);
    return 0;
}
