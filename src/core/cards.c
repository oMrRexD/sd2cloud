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
#include <zlib.h>
#include "common.h"

card_t cards[MAX_CARDS];
int cardsOther;
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

static int type_order(int t) { return t == TYPE_NORMAL ? 0 : t == TYPE_GAMEID ? 1 : t == TYPE_NAMED ? 2 : t == TYPE_VMC ? 4 : 3; }

static int compare(const void *a, const void *b)
{
    const card_t *x = a, *y = b;
    int d = type_order(x->type) - type_order(y->type);
    if (d)
        return d;
    if (x->type == TYPE_VMC)   /* (the VMCs by place, then by name) */
        return (d = strcmp(x->folder, y->folder)) ? d : strcasecmp(x->base, y->base);
    if (x->type == TYPE_NORMAL) {   /* Card2 before Card10 */
        d = atoi(x->folder + strlen(dev->numbered)) - atoi(y->folder + strlen(dev->numbered));
        if (d)
            return d;
    }
    d = strcasecmp(x->folder, y->folder);
    return d ? d : x->channel - y->channel;
}

/* The game of each game card. The sd2psx names a game's folder by its ID and shows the game's name on its own
 * screen, from a list in its firmware; the same list is embedded here (assets/gamenames.bin: tools/make_gamenames.py).
 * A folder that isn't an ID in it is named as the group of games Game2Folder.ini gives it to (group_names, below), or
 * keeps no name.
 * The list: lines "ID<TAB>name", sorted by ID, in blocks of about 16 KB, each compressed by itself. A block is
 * unpacked when it is needed, into one place (the last one stays there): 20 KB of memory instead of the whole list's
 * 619 KB, which a big card file opened in Files needs for its saves' icons */
extern unsigned char asset_gamenames_bin[];
extern unsigned int size_asset_gamenames_bin;
#define NAMES_TEXT 20000
static char namesText[NAMES_TEXT];
static int namesAt = -1;   /* the block in namesText */

static int names_blocks(void)
{
    return size_asset_gamenames_bin >= 8 && !memcmp(asset_gamenames_bin, "GNB1", 4) ? (int)le32(asset_gamenames_bin + 4) : 0;
}

/* block b of the list (its first ID, then where its data is, how long, and how long unpacked) */
static const unsigned char *names_entry(int b) { return asset_gamenames_bin + 8 + b * 22; }

/* block b's text, unpacked (NULL = it can't be) */
static const char *names_block(int b, size_t *len)
{
    const unsigned char *e = names_entry(b);
    unsigned int at = le32(e + 10), size = le32(e + 14), raw = le32(e + 18);
    uLongf out = raw;
    if (raw > NAMES_TEXT || at + size > size_asset_gamenames_bin)
        return NULL;
    if (namesAt != b) {
        namesAt = -1;
        if (size == raw)   /* (stored as it is) */
            memcpy(namesText, asset_gamenames_bin + at, raw);
        else if (uncompress((Bytef *)namesText, &out, asset_gamenames_bin + at, size) != Z_OK || out != raw)
            return NULL;
        namesAt = b;
    }
    *len = raw;
    return namesText;
}

/* the lines of the list, one after the other (it starts as {0}). 1 = one more: its ID and its name, not ended by a 0 */
typedef struct {
    int b;
    const char *p, *end;
} names_t;

static int names_next(names_t *it, const char **id, int *idLen, const char **name, int *nameLen)
{
    const char *tab, *nl;
    size_t len;
    for (;;) {
        if (it->p >= it->end) {
            if (it->b >= names_blocks() || !(it->p = names_block(it->b++, &len)))
                return 0;
            it->end = it->p + len;
            continue;
        }
        if (!(tab = memchr(it->p, '\t', it->end - it->p))) {
            it->p = it->end;
            continue;
        }
        if (!(nl = memchr(tab, '\n', it->end - tab)))
            nl = it->end;
        *id = it->p;
        *idLen = (int)(tab - it->p);
        *name = tab + 1;
        *nameLen = (int)(nl - tab - 1);
        it->p = nl + 1;
        return 1;
    }
}

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
    const char *p, *name;
    char path[64];
    int i, k, pass, ids = 0, n, nameLen;
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
    for (pass = 0; pass < 2; pass++) {   /* (SLUS, SCES and the like first) */
        names_t it = {0};
        while (names_next(&it, &p, &n, &name, &nameLen)) {
            if (n != 10 || (p[2] == 'U' || p[2] == 'E') == pass || !isdigit((unsigned char)p[7]) ||
                !isdigit((unsigned char)p[8]) || !isdigit((unsigned char)p[9]) || !groupKey[id_key(p)])
                continue;
            for (k = 0; k < nGroupIds; k++)
                if (!strncasecmp(groupIds[k].id, p, 10))
                    group_add(&groups[groupIds[k].group], name, nameLen);
        }
    }
    for (i = 0; i < nCards; i++)
        for (k = 0; k < nGroups && cards[i].type == TYPE_GAMEID && !cards[i].game[0]; k++)
            if (groups[k].n && !strcasecmp(groups[k].folder, cards[i].folder)) {
                group_label(&groups[k], cards[i].game, sizeof(cards[i].game));
                break;
            }
}

/* each game card's game, looked up by its folder (game_title: the cards come in the order of their folders, so each
 * block of the list is unpacked once at most); then the groups' names */
static void game_names(void)
{
    int i, any = 0;
    for (i = 0; i < nCards; i++)
        if (cards[i].type == TYPE_GAMEID) {
            any = 1;
            game_title(cards[i].folder, cards[i].game, sizeof(cards[i].game));
        }
    if (any)
        group_names();
}

int game_title(const char *id, char *out, size_t size)
{
    const char *p, *end, *tab, *nl;
    size_t n = strlen(id), len;
    int lo = 0, hi = names_blocks() - 1, mid;
    while (lo < hi) {   /* the last block whose first ID isn't after it */
        mid = (lo + hi + 1) / 2;
        if (strncasecmp((const char *)names_entry(mid), id, 10) <= 0)
            lo = mid;
        else
            hi = mid - 1;
    }
    out[0] = 0;
    if (hi < 0 || !(p = names_block(lo, &len)))
        return 0;
    for (end = p + len; p < end && (tab = memchr(p, '\t', end - p)) != NULL; p = nl + 1) {
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

/* the sd2psx's own settings, which it reads from its microSD when it starts (and writes there when they are changed on
 * it): of them, the size it makes a card it is sent to and doesn't find */
static void on_card_size(const char *s, const char *k, const char *v, void *u)
{
    int mb = atoi(v);
    if (!strcmp(s, "PS2") && !strcmp(k, "CardSize") && mb >= MCFS_MIN_MB && mb <= MCFS_MAX_MB && !(mb & (mb - 1)))
        *(int *)u = mb;
}

int device_card_mb(void)
{
    char path[64];
    int mb = 0;
    snprintf(path, sizeof(path), "%s.sd2psx/settings.ini", sdRoot);
    if (dev->sd2psx)
        ini_read(path, on_card_size, &mb);
    return mb;
}

int device_card_mb_set(int mb)
{
    char path[64], v[8];
    int r;
    snprintf(path, sizeof(path), "%s.sd2psx/", sdRoot);
    ensure_dir(path);   /* (the sd2psx makes it when it first starts with that microSD: it may not have yet) */
    snprintf(path, sizeof(path), "%s.sd2psx/settings.ini", sdRoot);
    snprintf(v, sizeof(v), "%d", mb);
    r = ini_set(path, "PS2", "CardSize", v);
    log_msg("%s: CardSize = %d (%d)", path, mb, r);
    return r;
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

/* A folder's listing, each file's size in it (one folder at a time: the cards found, a folder renamed or deleted).
 * The size a card's file has there spares opening each card to ask it, which on the sd2psx takes tens of
 * milliseconds a card */
#define FOLDER_MAX MAX_NAMES
static dir_entry_t folderList[FOLDER_MAX];

/* a size from a folder's listing that can be a card's (else the file is asked: known_size) */
static long long listed_size(long long size) { return size > 0 && size <= (1LL << 30) ? size : 0; }

/* the file's size, opened to ask it when the listing didn't say (0 = it can't be opened) */
static long long known_size(const char *path, long long size)
{
    int fd;
    if (size > 0 || (fd = open(path, O_RDONLY)) < 0)
        return size;
    size = lseek(fd, 0, SEEK_END);
    close(fd);
    return size;
}

/* a file of a folder of cards, as the next of cards[] when it is the .mcd of one of that folder's channels (1). size =
 * the file's, from the folder's listing (0 = not known: fill_cards asks the file) */
static int add_file(const char *base, const char *folder, const char *file, long long size)
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
    c->size = listed_size(size);
    c->type = boot ? TYPE_BOOT : is_cardn(folder) ? TYPE_NORMAL
              : (is_game_id(folder) || list_has(mapped, folder)) ? TYPE_GAMEID : TYPE_NAMED;
    return 1;
}

/* the size of each card from first on that the listing didn't give (one file open at a time) and their channel names,
 * from their folder's .ini */
static void fill_cards(const char *base, const char *folder, int first)
{
    range_t r = {first, nCards};
    char path[260];
    int j;
    for (j = first; j < nCards; j++)
        cards[j].size = known_size(cards[j].path, cards[j].size);
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

/* ------------------------------------------------------------ the OPL's VMCs */

vmc_place_t vmcPlaces[VMC_PLACES];
int nVmcPlaces;
int (*vmcMore)(vmc_place_t *more, int max);

const vmc_place_t *vmc_place(const char *folder)
{
    int i;
    for (i = 0; i < nVmcPlaces; i++)
        if (!strcmp(vmcPlaces[i].folder, folder))
            return &vmcPlaces[i];
    return NULL;
}

/* a file of a place's VMC folder as the next of cards[], when it is a card (1 to 128 MB, a power of 2). size = the
 * file's, from the folder's listing (0 = not known: the file is asked). 1 = it is */
static int add_vmc(const vmc_place_t *p, const char *file, long long size)
{
    size_t len = strlen(file);
    card_t *c = &cards[nCards];
    if (nCards == MAX_CARDS || !file_ext(file, ".bin") || len - 4 >= sizeof(c->base))
        return 0;
    memset(c, 0, sizeof(*c));
    snprintf(c->path, sizeof(c->path), "%s/%s", p->dir, file);
    c->size = known_size(c->path, listed_size(size));
    if (c->size < (1LL << 20) || c->size > (128LL << 20) || (c->size & (c->size - 1)))
        return 0;
    snprintf(c->folder, sizeof(c->folder), "%s", p->folder);
    snprintf(c->base, sizeof(c->base), "%.*s", (int)(len - 4), file);
    snprintf(c->id, sizeof(c->id), "%s/%.*s", p->folder, (int)(len - 4), file);
    c->type = TYPE_VMC;
    nCards++;
    return 1;
}

static struct {
    char vmc[56], id[12];
    int place;   /* (vmcPlaces[]'s) */
} links[256];
static int nLinks = -1;   /* (-1 = not read yet) */
static int linksDone;     /* every place's settings read: the counts can be told */
static char (*linkFiles)[64];   /* the place's .cfg files still to read (copied: list_dir's names are everyone's) */
static int linkFileN, linkFileAt;
static int linksPlace;    /* (the place whose settings are being read) */

/* the VMCs of each place: the microSD's, then the folders the user added (a place whose folder isn't there, or is
 * the microSD's own, is left out). What the games' settings said of them is read again when it is asked for */
static void add_vmcs(void)
{
    vmc_place_t *p = &vmcPlaces[0];
    int n, i, k, more;
    nLinks = -1;
    linksDone = 0;
    free(linkFiles);
    linkFiles = NULL;
    linkFileN = linkFileAt = 0;
    memset(p, 0, sizeof(*p));
    snprintf(p->folder, sizeof(p->folder), "%s", VMC_FOLDER);
    snprintf(p->dir, sizeof(p->dir), "%sVMC", sdRoot);
    snprintf(p->cfg, sizeof(p->cfg), "%sCFG", sdRoot);
    more = vmcMore ? vmcMore(vmcPlaces + 1, VMC_PLACES - 1) : 0;
    for (k = nVmcPlaces = 0; k < 1 + more; k++) {
        if (k && !strcasecmp(vmcPlaces[k].dir, vmcPlaces[0].dir))
            continue;
        if ((n = dir_list(vmcPlaces[k].dir, folderList, FOLDER_MAX)) < 0) {
            if (k)
                log_msg("VMCs: %s can't be read (%s)", vmcPlaces[k].dir, vmcPlaces[k].key);
            continue;
        }
        if (k != nVmcPlaces)
            vmcPlaces[nVmcPlaces] = vmcPlaces[k];
        p = &vmcPlaces[nVmcPlaces++];
        for (i = 0; i < n; i++)
            if (!folderList[i].dir)
                add_vmc(p, folderList[i].name, folderList[i].size);
        if (k)
            log_msg("VMCs: %s, as %s", p->dir, p->folder);
    }
}

card_t *vmc_append(const char *folder, const char *name)
{
    const vmc_place_t *p = vmc_place(folder);
    char file[80];
    snprintf(file, sizeof(file), "%s.bin", name);
    if (!p || !add_vmc(p, file, 0))
        return NULL;
    set_included(&cards[nCards - 1]);
    log_msg("%s joined the cards: %d on the microSD", cards[nCards - 1].id, nCards);
    return &cards[nCards - 1];
}


/* SLUS_210.65, the name of an OPL's settings file (the game's startup), as the sd2psx names the game: SLUS-21065 */
static void startup_id(const char *s, char *id, size_t size)
{
    size_t n = 0;
    for (; *s && *s != ';' && n + 1 < size && strcasecmp(s, ".cfg") != 0; s++)
        if (*s != '.')
            id[n++] = *s == '_' ? '-' : *s;
    id[n] = 0;
}

static void on_cfg(const char *section, const char *key, const char *value, void *u)
{
    (void)section;
    if ((!strcmp(key, "$VMC_0") || !strcmp(key, "$VMC_1")) && value[0] && nLinks < (int)(sizeof(links) / sizeof(links[0]))) {
        snprintf(links[nLinks].vmc, sizeof(links[0].vmc), "%s", value);
        snprintf(links[nLinks].id, sizeof(links[0].id), "%s", (const char *)u);
        links[nLinks++].place = linksPlace;
    }
}

int vmc_links_step(int ms)
{
    u64 until = now_ms() + ms;
    char path[200], id[12];
    if (linksDone)
        return 1;
    if (nLinks < 0) {   /* (from the first place on) */
        nLinks = 0;
        linksPlace = -1;
    }
    for (;;) {
        if (linkFileAt == linkFileN) {   /* this place's files read: the next place's listed */
            int n, i;
            free(linkFiles);
            linkFiles = NULL;
            linkFileN = linkFileAt = 0;
            if (++linksPlace >= nVmcPlaces) {
                linksDone = 1;
                log_msg("the OPL's games' settings: %d use a VMC", nLinks);
                return 1;
            }
            n = list_dir(vmcPlaces[linksPlace].cfg);
            if (n > 0 && (linkFiles = malloc(n * sizeof(linkFiles[0]))) != NULL)
                for (i = 0; i < n; i++) {
                    size_t len = strlen(names[i]);
                    if (len >= 5 && !strcasecmp(names[i] + len - 4, ".cfg"))
                        memcpy(linkFiles[linkFileN++], names[i], sizeof(linkFiles[0]));
                }
        } else {
            const char *f = linkFiles[linkFileAt++];
            startup_id(f, id, sizeof(id));
            snprintf(path, sizeof(path), "%s/%s", vmcPlaces[linksPlace].cfg, f);
            ini_read(path, on_cfg, id);
        }
        if (now_ms() >= until)
            return 0;
    }
}

void vmc_links_read(void)
{
    while (!vmc_links_step(1000))
        ;
}

/* the place a VMC is in (-1 = none is) */
static int place_of(const card_t *c)
{
    const vmc_place_t *p = vmc_place(c->folder);
    return p ? (int)(p - vmcPlaces) : -1;
}

int vmc_games(const card_t *c, char (*ids)[12], int max)
{
    int i, j, n = 0, place = place_of(c);
    if (!linksDone)
        return -1;
    for (i = 0; i < nLinks; i++) {
        if (links[i].place != place || strcmp(links[i].vmc, c->base) != 0)
            continue;
        for (j = 0; j < i && !(links[j].place == place && !strcmp(links[j].vmc, c->base) &&
                               !strcmp(links[j].id, links[i].id)); j++)
            ;
        if (j < i)
            continue;   /* (a game that uses it in both slots: once) */
        if (ids && n < max)
            snprintf(ids[n], sizeof(ids[0]), "%s", links[i].id);
        n++;
    }
    return n;
}

/* a game's settings, with its lines "$VMC_N=old" made "$VMC_N=name" (written again only when it had one) */
static void cfg_rename_vmc(const char *path, const char *old, const char *name)
{
    buffer_t b = {0};
    char *out, *p, *end;
    size_t n = 0, len = strlen(old);
    int changed = 0;
    if (file_read(path, &b) != 0 || !(out = malloc(b.len + 2 * strlen(name) + 2))) {
        buf_free(&b);
        return;
    }
    for (p = (char *)b.data, end = p + b.len; p < end;) {
        char *eol = memchr(p, '\n', end - p), *stop = eol ? eol : end;
        size_t line = stop - p, value = line;
        if (line > 7 && (!strncmp(p, "$VMC_0=", 7) || !strncmp(p, "$VMC_1=", 7))) {
            value = line - 7 - (stop[-1] == '\r');
            if (value == len && !strncmp(p + 7, old, len)) {
                memcpy(out + n, p, 7);
                n += 7;
                memcpy(out + n, name, strlen(name));
                n += strlen(name);
                p += 7 + len;   /* (the rest of the line as it was: its \r) */
                changed = 1;
                continue;
            }
        }
        memcpy(out + n, p, line + (eol != NULL));
        n += line + (eol != NULL);
        p = stop + (eol != NULL);
    }
    if (changed)
        log_msg("%s: the VMC %s is %s now (%d)", path, old, name, file_replace(path, (unsigned char *)out, n));
    free(out);
    buf_free(&b);
}

int vmc_rename(card_t *c, const char *name)
{
    char to[200], path[200], id[96], old[56];
    int n, i, place = place_of(c);
    if (place < 0)
        return -1;
    snprintf(to, sizeof(to), "%s/%s.bin", vmcPlaces[place].dir, name);
    if (file_exists(to))
        return -2;
    if (file_move(c->path, to) != 0) {
        log_msg("%s couldn't be called %s", c->id, name);
        return -1;
    }
    snprintf(old, sizeof(old), "%s", c->base);
    n = list_dir(vmcPlaces[place].cfg);   /* (the games that use it, by its new name: the place's) */
    for (i = 0; i < n; i++) {
        size_t len = strlen(names[i]);
        if (len < 5 || strcasecmp(names[i] + len - 4, ".cfg") != 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", vmcPlaces[place].cfg, names[i]);
        cfg_rename_vmc(path, old, name);
    }
    for (i = 0; i < nLinks; i++)
        if (links[i].place == place && !strcmp(links[i].vmc, old))
            snprintf(links[i].vmc, sizeof(links[0].vmc), "%s", name);
    snprintf(id, sizeof(id), "%s", c->id);
    snprintf(c->base, sizeof(c->base), "%s", name);
    snprintf(c->id, sizeof(c->id), "%s/%s", c->folder, name);
    snprintf(c->path, sizeof(c->path), "%s", to);
    state_rename(id, c->id);
    state_write();
    log_msg("%s is %s now", id, c->id);
    return 0;
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
    /* With no device in the slots, none: an sd2psx's folders on the drive the program is on (a copy of a microSD) are
     * no cards in use, only files (the Files group opens them). Its VMCs, if it has them */
    nFolders = standalone ? 0 : list_dir(base);
    if (nFolders < 0)
        return -1;
    memcpy(folders, names, sizeof(names[0]) * nFolders);

    for (i = 0; i < nFolders && nCards < MAX_CARDS; i++) {
        int n, first = nCards;
        snprintf(path, sizeof(path), "%s/%s", base, folders[i]);
        if ((n = dir_list(path, folderList, FOLDER_MAX)) <= 0)
            continue;   /* a loose file or an empty folder */
        for (j = 0; j < n; j++)
            if (!folderList[j].dir)
                add_file(base, folders[i], folderList[j].name, folderList[j].size);
        fill_cards(base, folders[i], first);
    }
    if (!cardsOther)
        add_vmcs();
    if (nCards == MAX_CARDS)
        log_msg("more memory cards than SD2Cloud handles (%d): the rest are left out", MAX_CARDS);
    qsort(cards, nCards, sizeof(cards[0]), compare);
    for (i = 0; i < nCards; i++) {
        set_included(&cards[i]);
        cards[i].included &= !cardsOther;
    }
    game_names();
    log_msg("%d card(s) on the microSD%s", nCards, cardsOther ? ", of the device that isn't in the slot" : "");
    return nCards;
}

card_t *cards_append(const char *folder, const char *file)
{
    char base[96];
    snprintf(base, sizeof(base), "%s%s", sdRoot, dev->cards);
    if (!add_file(base, folder, file, 0))
        return NULL;
    fill_cards(base, folder, nCards - 1);
    set_included(&cards[nCards - 1]);
    cards[nCards - 1].included &= !cardsOther;
    game_names();
    log_msg("%s joined the cards: %d on the microSD", cards[nCards - 1].id, nCards);
    return &cards[nCards - 1];
}

void cards_sort(void) { qsort(cards, nCards, sizeof(cards[0]), compare); }

/* the card out of cards[] and out of the state (whose backups stay on Drive) */
static void cards_drop(card_t *c)
{
    int i = (int)(c - cards);
    if (!cardsOther)
        state_forget(c->id);
    memmove(&cards[i], &cards[i + 1], (nCards - i - 1) * sizeof(cards[0]));
    nCards--;
}

int card_set_name(card_t *c, const char *name)
{
    char path[260], ch[8];
    int r;
    folder_ini(c->folder, path, sizeof(path));
    snprintf(ch, sizeof(ch), "%d", c->channel);
    r = ini_set(path, "ChannelName", ch, name);
    log_msg("%s: [ChannelName] %s = %s (%d)", path, ch, name, r);
    if (r == 0)
        snprintf(c->name, sizeof(c->name), "%s", name);
    return r;
}

int folder_renamable(const char *folder)
{
    size_t n = strlen(dev->numbered);
    return dev->sd2psx && strcasecmp(folder, "BOOT") && !is_game_id(folder) &&
           !(!strncasecmp(folder, dev->numbered, n) && folder[n] && strspn(folder + n, "0123456789") == strlen(folder + n));
}

/* Game2Folder.ini's games of a folder given to the one it is called now */
static void game2folder_rename(const char *from, const char *to)
{
    char path[64], *line, *eq;
    buffer_t b = {0};
    int ps2 = 0;
    snprintf(path, sizeof(path), "%s.sd2psx/Game2Folder.ini", sdRoot);
    if (file_read(path, &b) != 0 || !b.len) {
        buf_free(&b);
        return;
    }
    for (line = strtok((char *)b.data, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char l[200], key[100], value[100];
        snprintf(l, sizeof(l), "%s", line);
        trim(l);
        if (l[0] == '[')
            ps2 = !strncasecmp(l, "[PS2]", 5);
        else if (ps2 && (eq = strchr(l, '=')) != NULL) {
            snprintf(key, sizeof(key), "%.*s", (int)(eq - l), l);
            snprintf(value, sizeof(value), "%s", eq + 1);
            trim(key);
            trim(value);
            if (!strcasecmp(value, from))
                ini_set(path, "PS2", key, to);
        }
    }
    buf_free(&b);
}

int cards_rename_folder(const char *folder, const char *name)
{
    static char dest[256][64];   /* each file's new name */
    dir_entry_t *list = folderList;
    char from[200], to[200], a[520], b[520], old[48];
    size_t len;
    int n, i, k, r = 0, whole;
    snprintf(old, sizeof(old), "%s", folder);
    len = strlen(old);
    snprintf(from, sizeof(from), "%s%s/%s/", sdRoot, dev->cards, old);
    snprintf(to, sizeof(to), "%s%s/%s/", sdRoot, dev->cards, name);
    if (strlen(name) > 15 || !folder_renamable(name) || dir_list(to, list, 1) >= 0 || (n = dir_list(from, list, 256)) < 0)
        return -1;
    for (i = 0; i < n; i++)   /* (Exemplo-1.mcd, Exemplo.ini, Exemplo-1.bin: Novo-1.mcd, Novo.ini, Novo-1.bin) */
        snprintf(dest[i], sizeof(dest[0]), "%s%s", strncasecmp(list[i].name, old, len) ? "" : name,
                 list[i].name + (strncasecmp(list[i].name, old, len) ? 0 : len));
    /* the folder itself renamed, where that can be: then only its files, which are named after it, are left */
    from[strlen(from) - 1] = to[strlen(to) - 1] = 0;
    whole = rename(from, to) == 0;
    strcat(from, "/");
    strcat(to, "/");
    if (!whole)
        ensure_dir(to);
    /* without the folder renamed, every file is copied first, and only when all of them are is the old folder let
     * go of */
    for (i = 0; i < n && r == 0; i++) {
        if (list[i].dir)
            continue;
        snprintf(a, sizeof(a), "%s%s", whole ? to : from, list[i].name);
        snprintf(b, sizeof(b), "%s%s", to, dest[i]);
        if (whole ? strcmp(a, b) && file_move(a, b) != 0 : file_copy(a, b) != 0)
            r = -1;
    }
    if (r != 0 && !whole) {   /* (the copies go: the folder is as it was) */
        for (k = 0; k < i; k++) {
            snprintf(b, sizeof(b), "%s%s", to, dest[k]);
            if (!list[k].dir)
                unlink(b);
        }
        to[strlen(to) - 1] = 0;
        rmdir(to);
        log_msg("%s couldn't be called %s: as it was", old, name);
        return -1;
    }
    if (!whole) {
        for (i = 0; i < n; i++) {
            snprintf(a, sizeof(a), "%s%s", from, list[i].name);
            if (!list[i].dir)
                unlink(a);
        }
        from[strlen(from) - 1] = 0;
        rmdir(from);
    }
    game2folder_rename(old, name);
    /* its cards, under their new names: and what is known of each, which goes on being theirs */
    for (i = 0; i < nCards; i++)
        if (!strcasecmp(cards[i].folder, old)) {
            char id[96];
            snprintf(id, sizeof(id), "%s", cards[i].id);
            snprintf(cards[i].folder, sizeof(cards[i].folder), "%s", name);
            snprintf(cards[i].base, sizeof(cards[i].base), "%s-%d", name, cards[i].channel);
            snprintf(cards[i].id, sizeof(cards[i].id), "%s/%s", name, cards[i].base);
            snprintf(cards[i].path, sizeof(cards[i].path), "%s%s/%s/%s%s", sdRoot, dev->cards, name, cards[i].base, dev->ext);
            if (!cardsOther)
                state_rename(id, cards[i].id);
        }
    if (!cardsOther)
        state_write();
    log_msg("%s is called %s now (%s)%s", old, name, whole ? "renamed" : "copied", r ? ", not all of its files" : "");
    return r;
}

int cards_delete(card_t *c)
{
    char id[96];
    int r;
    snprintf(id, sizeof(id), "%s", c->id);
    if ((r = unlink(c->path)) != 0 && file_exists(c->path)) {
        log_msg("%s couldn't be deleted (%d)", id, r);
        return -1;
    }
    cards_drop(c);
    if (!cardsOther)
        state_write();
    log_msg("%s deleted: %d on the microSD", id, nCards);
    return 0;
}

int cards_delete_folder(const char *folder)
{
    dir_entry_t *list = folderList;
    char dir[260], path[520], name[48];
    int n, i, left = 0;
    snprintf(name, sizeof(name), "%s", folder);   /* (it may be a card's, which goes) */
    snprintf(dir, sizeof(dir), "%s%s/%s/", sdRoot, dev->cards, name);
    if ((n = dir_list(dir, list, FOLDER_MAX)) < 0)
        return -1;
    for (i = 0; i < n; i++) {   /* everything in it: its cards, its .ini, its pictures (and a folder in it, empty) */
        snprintf(path, sizeof(path), "%s%s", dir, list[i].name);
        if ((list[i].dir ? rmdir(path) : unlink(path)) != 0 && (list[i].dir || file_exists(path)))
            left++;
    }
    for (i = nCards - 1; i >= 0; i--)   /* the cards that were its, whatever became of the rest */
        if (!strcasecmp(cards[i].folder, name)) {
            if (file_exists(cards[i].path))
                continue;
            cards_drop(&cards[i]);
        }
    if (!cardsOther)
        state_write();
    dir[strlen(dir) - 1] = 0;   /* (the folder itself, without its last /) */
    if (left || rmdir(dir) != 0) {
        log_msg("%s couldn't be deleted whole (%d left in it)", name, left);
        return -1;
    }
    log_msg("%s deleted, folder and all: %d cards on the microSD", name, nCards);
    return 0;
}

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
    if (cardsOther) {   /* (the other device's: what is known is of the device's own cards of the same name) */
        c->status = ST_NO_BACKUP;
        c->resolved = 0;
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
    c->resolved = e && e->resolved[0] && !strcmp(e->resolved, c->fingerprint);
    if (c->resolved)
        log_msg("%s: marked as resolved as it is", c->id);
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
    if (mcfs_slot_port(c->path) >= 0 || library_path(c->path))
        return;   /* (the Memory Card in the other slot, the Library: not one of the device's cards, no backups) */
    check_one(c, &migrated);
    if (migrated)
        state_write();
}

void card_keep_synced(card_t *c, int synced, int resolved)
{
    card_state_t *e = cardsOther ? NULL : state_card(c->id, 0);
    if (!e || c->status == ST_ERROR || (!synced && !resolved))
        return;
    if (synced) {
        snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", c->fingerprint);
        e->version = MCFS_VERSION;
        c->status = e->sha[0] ? ST_UP_TO_DATE : ST_NO_BACKUP;
    }
    if (resolved) {
        snprintf(e->resolved, sizeof(e->resolved), "%s", c->fingerprint);
        c->resolved = 1;
    }
    state_write();
    log_msg("%s: changed with nothing a backup needs: still %s", c->id, synced ? "synced" : "resolved");
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
