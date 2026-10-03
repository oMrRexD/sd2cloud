/*
 * SD2Cloud -- finds the sd2psx memory cards on the microSD (sd2psXtd firmware, src/ps2/ps2_cardman.c):
 *   MemoryCards/PS2/<folder>/<folder>-<channel>.mcd     folder = CardN, a game ID (Game ID) or any name
 *   MemoryCards/PS2/BOOT/BootCard-<channel>.mcd          (or BootCard.mcd, the old layout)
 * Each channel's name may be in <folder>/<folder>.ini (BootCard.ini in BOOT), section [ChannelName].
 * The folders mapped in .sd2psx/Game2Folder.ini count as game cards.
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
static int is_game_id(const char *p)
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
    if (strncasecmp(p, "Card", 4) != 0 || !p[4])
        return 0;
    for (p += 4; *p; p++)
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
        d = atoi(x->folder + 4) - atoi(y->folder + 4);
        if (d)
            return d;
    }
    d = strcasecmp(x->folder, y->folder);
    return d ? d : x->channel - y->channel;
}

int cards_scan(void)
{
    static char folders[MAX_NAMES][64];
    char base[96], path[260];
    int nFolders, i, j;

    nCards = 0;
    mapped[0] = 0;
    snprintf(path, sizeof(path), "%s.sd2psx/Game2Folder.ini", sdRoot);
    ini_read(path, on_game2folder, NULL);

    snprintf(base, sizeof(base), "%sMemoryCards/PS2", sdRoot);
    nFolders = list_dir(base);
    if (nFolders < 0)
        return -1;
    memcpy(folders, names, sizeof(names[0]) * nFolders);

    for (i = 0; i < nFolders && nCards < MAX_CARDS; i++) {
        const char *folder = folders[i];
        int boot = !strcasecmp(folder, "BOOT"), n, first = nCards;
        range_t r;
        snprintf(path, sizeof(path), "%s/%s", base, folder);
        if ((n = list_dir(path)) <= 0)
            continue;   /* a loose file or an empty folder */
        for (j = 0; j < n && nCards < MAX_CARDS; j++) {
            const char *file = names[j], *dash;
            size_t len = strlen(file);
            char prefix[64];
            card_t *c;
            int channel;
            if (len < 5 || strcasecmp(file + len - 4, ".mcd") != 0)
                continue;
            snprintf(prefix, sizeof(prefix), "%s", boot ? "BootCard" : folder);
            if (boot && !strcasecmp(file, "BootCard.mcd"))
                channel = 1;   /* old layout, a single channel */
            else {
                dash = file + strlen(prefix);
                if (strncasecmp(file, prefix, strlen(prefix)) != 0 || *dash != '-' || !isdigit((unsigned char)dash[1]))
                    continue;
                channel = atoi(dash + 1);
            }
            c = &cards[nCards];
            memset(c, 0, sizeof(*c));
            snprintf(c->folder, sizeof(c->folder), "%s", folder);
            snprintf(c->base, sizeof(c->base), "%.*s", (int)(len - 4), file);
            snprintf(c->id, sizeof(c->id), "%s/%.*s", folder, (int)(len - 4), file);
            snprintf(c->path, sizeof(c->path), "%s/%s/%s", base, folder, file);
            c->channel = channel;
            c->type = boot ? TYPE_BOOT : is_cardn(folder) ? TYPE_NORMAL
                      : (is_game_id(folder) || list_has(mapped, folder)) ? TYPE_GAMEID : TYPE_NAMED;
            nCards++;
        }
        /* the size of each one (one file open at a time) and the channel names */
        for (j = first; j < nCards; j++) {
            int fd = open(cards[j].path, O_RDONLY);
            if (fd >= 0) {
                cards[j].size = lseek(fd, 0, SEEK_END);
                close(fd);
            }
        }
        r.first = first;
        r.end = nCards;
        snprintf(path, sizeof(path), "%s/%s/%s.ini", base, folder, boot ? "BootCard" : folder);
        if (nCards > first)
            ini_read(path, on_channel, &r);
    }
    if (nCards == MAX_CARDS)
        log_msg("more memory cards than SD2Cloud handles (%d): the rest are left out", MAX_CARDS);
    qsort(cards, nCards, sizeof(cards[0]), compare);
    for (i = 0; i < nCards; i++) {
        card_t *c = &cards[i];
        c->included = cfg.list_mode ? list_has(cfg.include, c->id) : ((cfg.types & c->type) && !list_has(cfg.exclude, c->id));
        c->status = ST_NEW;
    }
    log_msg("%d card(s) on the microSD", nCards);
    return nCards;
}

/* fingerprint of one card and its status against the last backup; migrated = an old fingerprint was adopted */
static void check_one(card_t *c, int *migrated)
{
    card_state_t *e;
    int saves = 0;
    if (mcfs_fingerprint(c->path, c->fingerprint, &saves) != 0) {
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
