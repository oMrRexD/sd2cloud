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
    if (strcasecmp(s, "PS2") == 0 && *v && strlen(mapped) + strlen(v) + 2 < sizeof(mapped)) {
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
 * line: tools/make_gamenames.py). A folder that isn't an ID in it (one mapped in Game2Folder.ini) keeps no name */
extern unsigned char asset_gamenames_txt[];
extern unsigned int size_asset_gamenames_txt;

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

static void set_included(card_t *c)
{
    c->included = cfg.list_mode ? list_has(cfg.include, c->id) : ((cfg.types & c->type) && !list_has(cfg.exclude, c->id));
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

card_t *cards_add(const char *folder, const char *file)
{
    char base[96], id[96];
    int i;
    snprintf(base, sizeof(base), "%s%s", sdRoot, dev->cards);
    if (!add_file(base, folder, file))
        return NULL;
    fill_cards(base, folder, nCards - 1);
    set_included(&cards[nCards - 1]);
    snprintf(id, sizeof(id), "%s", cards[nCards - 1].id);
    qsort(cards, nCards, sizeof(cards[0]), compare);
    game_names();
    for (i = 0; i < nCards && strcmp(cards[i].id, id); i++)
        ;
    log_msg("%s joined the cards: %d on the microSD", id, nCards);
    return i < nCards ? &cards[i] : NULL;
}

/* fingerprint of one card and its status against the last backup; migrated = an old fingerprint was adopted */
static void check_one(card_t *c, int *migrated)
{
    card_state_t *e;
    int saves = 0;
    /* (the root folder's signature comes from the same reading, for a device whose card in use is found by it) */
    if (mcfs_fingerprint_root(c->path, c->fingerprint, &saves, cardTold ? NULL : c->rootSig) != 0) {
        c->status = ST_ERROR;
        log_msg("%s: couldn't read the index", c->id);
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
