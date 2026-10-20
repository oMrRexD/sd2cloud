/*
 * SD2Cloud -- the Library: saves kept off the cards, each one a .psu file in the data folder (Library/<game ID>/<the
 * save's folder>.psu; Library/OTHER/ for a save whose folder names no game), listed in Library/library.ini so that
 * the list is had without opening a file per save (on an sd2psx's microSD each one takes tens of milliseconds).
 *
 * To the rest of the program it is a card with no end to its room: mcfs reads and writes it through the calls a
 * Memory Card in a slot goes through (mcfs_library), so its saves are listed, opened, copied, moved and deleted on the
 * screens a card's are. A save copied into it goes to its own game's folder, whatever path it was copied to; one of a
 * folder the Library has already takes that one's place, as a save does on a card.
 *
 * The .psu files are read here, not through mcfs's own reading of them: mcfs is in the middle of copying a save
 * (into its one save held in memory) while it asks the Library for that save's files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include "common.h"

#define ENT 512
#define DF_DIRECTORY 0x0020
#define DF_EXISTS    0x8000
#define INDEX "library.ini"

static char libDir[160];   /* "" = there is no Library */
static struct {
    char folder[33], game[12];   /* game: its ID, or LIBRARY_OTHER */
    unsigned char head[64];      /* the folder's entry as a card has it, before its name: its mode, its entries, its dates */
    unsigned long long sig;      /* its .psu's signature (0 = not known yet) */
} lib[LIBRARY_MAX];
static int nLib;
static struct {
    char id[12], name[64];
    int saves;
} games[LIBRARY_MAX];
static int nGames;

/* the save whose .psu was read last, kept: its files are asked for one after the other */
static char heldFolder[33];
static buffer_t held;

static unsigned int le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }
static unsigned int le16(const unsigned char *p) { return p[0] | p[1] << 8; }

/* the game a save's folder names (BASLUS-21065...): its ID in id, or LIBRARY_OTHER */
void library_game_of(const char *folder, char id[12])
{
    snprintf(id, 12, "%.10s", strlen(folder) >= 12 && folder[0] == 'B' && isupper((unsigned char)folder[1]) ? folder + 2 : "");
    if (!id[0] || !is_game_id(id))
        snprintf(id, 12, "%s", LIBRARY_OTHER);
}

/* FNV-1a of a .psu's bytes, 64 bits (never 0): the signature the backup by save takes a save's .psu by */
static unsigned long long sign(const buffer_t *b)
{
    unsigned long long h = 1469598103934665603ULL;
    size_t k;
    for (k = 0; k < b->len; k++)
        h = (h ^ b->data[k]) * 1099511628211ULL;
    return h ? h : 1;
}

static int find(const char *folder)
{
    int i;
    for (i = 0; i < nLib && strcmp(lib[i].folder, folder); i++)
        ;
    return i < nLib ? i : -1;
}

static void file_of(int i, char *path, size_t size) { snprintf(path, size, "%s%s/%s.psu", libDir, lib[i].game, lib[i].folder); }

/* the games, by name (the ones no game is named by last) */
static void group(void)
{
    int i, k;
    nGames = 0;
    for (i = 0; i < nLib; i++) {
        for (k = 0; k < nGames && strcmp(games[k].id, lib[i].game); k++)
            ;
        if (k == nGames) {
            snprintf(games[k].id, sizeof(games[k].id), "%s", lib[i].game);
            games[k].name[0] = 0;
            if (strcmp(lib[i].game, LIBRARY_OTHER) && !game_title(lib[i].game, games[k].name, sizeof(games[k].name)))
                snprintf(games[k].name, sizeof(games[k].name), "%s", lib[i].game);
            games[k].saves = 0;
            nGames++;
        }
        games[k].saves++;
    }
    for (i = 1; i < nGames; i++)   /* (a few dozen at most) */
        for (k = i; k > 0 && (!games[k - 1].name[0] ||
                              (games[k].name[0] && strcasecmp(games[k - 1].name, games[k].name) > 0)); k--) {
            char t[sizeof(games[0])];
            memcpy(t, &games[k], sizeof(t));
            memcpy(&games[k], &games[k - 1], sizeof(t));
            memcpy(&games[k - 1], t, sizeof(t));
        }
}

static void on_index(const char *section, const char *key, const char *value, void *u)
{
    int i = find(section), k;
    (void)u;
    if (!section[0] || strlen(section) > 32)
        return;
    if (i < 0) {
        if (nLib == LIBRARY_MAX)
            return;
        i = nLib++;
        memset(&lib[i], 0, sizeof(lib[i]));
        snprintf(lib[i].folder, sizeof(lib[i].folder), "%s", section);
        library_game_of(section, lib[i].game);
    }
    if (!strcmp(key, "sig"))
        lib[i].sig = strtoull(value, NULL, 16);
    else if (!strcmp(key, "head") && strlen(value) == 2 * sizeof(lib[i].head))
        for (k = 0; k < (int)sizeof(lib[i].head); k++) {
            unsigned int b;
            sscanf(value + 2 * k, "%2x", &b);
            lib[i].head[k] = (unsigned char)b;
        }
}

/* the list written again, whole (written beside it and read back first: file_replace) */
static int write_index(void)
{
    buffer_t b = {0};
    char path[200], line[200];
    int i, k, r;
    for (i = 0; i < nLib; i++) {
        snprintf(line, sizeof(line), "[%s]\r\nhead = ", lib[i].folder);
        buf_append(&b, line, strlen(line));
        for (k = 0; k < (int)sizeof(lib[i].head); k++) {
            snprintf(line, sizeof(line), "%02x", lib[i].head[k]);
            buf_append(&b, line, 2);
        }
        snprintf(line, sizeof(line), "\r\nsig = %016llx\r\n\r\n", lib[i].sig);
        buf_append(&b, line, strlen(line));
    }
    snprintf(path, sizeof(path), "%s%s", libDir, INDEX);
    r = file_replace(path, b.data ? b.data : (unsigned char *)"", b.len);
    buf_free(&b);
    return r;
}

/* the .psu of that save, read (kept for its next file). 0 = it is in held */
static int read_held(const char *folder)
{
    char path[260];
    int i = find(folder);
    if (i >= 0 && held.data && !strcmp(heldFolder, folder))
        return 0;
    buf_free(&held);
    heldFolder[0] = 0;
    if (i < 0)
        return -1;
    file_of(i, path, sizeof(path));
    if (file_read(path, &held) != 0 || held.len < 3 * ENT) {
        log_msg("library: %s couldn't be read", path);
        buf_free(&held);
        return -1;
    }
    snprintf(heldFolder, sizeof(heldFolder), "%s", folder);
    return 0;
}

/* Through the files of the .psu held: each file's entry into ent (up to max), or the one called want's bytes into out.
 * Returns how many entries, or 1 when want was found; -1 = the file isn't a .psu that can be read */
static int walk_held(unsigned char (*ent)[ENT], int max, const char *want, buffer_t *out)
{
    size_t at = ENT;
    unsigned int n = le32(held.data + 4), i, len;
    int k = 0;
    for (i = 0; i < n; i++) {
        const unsigned char *e = held.data + at;
        if (at + ENT > held.len)
            return -1;
        at += ENT;
        if (i < 2 || (le16(e) & DF_DIRECTORY))   /* ("." and "..": nothing after them) */
            continue;
        if ((len = le32(e + 4)) > held.len - at)
            return -1;
        if (want && !strncmp((const char *)e + 64, want, 32))
            return buf_append(out, held.data + at, len) ? -1 : 1;
        if (ent && k < max)
            memcpy(ent[k++], e, ENT);
        at += (len + 1023) & ~1023u;   /* (each file padded to 1 KB, the last one's padding maybe missing) */
    }
    return want ? -1 : k;
}

/* -------- what mcfs calls (as for a memory card in a slot) */

static int lib_dir(int port, const char *folder, unsigned char (*ent)[ENT], int max)
{
    const char *scope = mcfs_library_scope();
    int i, k = 0;
    (void)port;
    if (!libDir[0])
        return -1;
    if (folder)   /* a save's files */
        return read_held(folder) != 0 ? -1 : walk_held(ent, max, NULL, NULL);
    for (i = 0; i < nLib && k < max; i++) {   /* the saves: a folder each, as at a card's root */
        if (scope[0] && strcmp(scope, lib[i].game))
            continue;
        memset(ent[k], 0, ENT);
        memcpy(ent[k], lib[i].head, sizeof(lib[i].head));
        memcpy(ent[k] + 64, lib[i].folder, strlen(lib[i].folder));
        k++;
    }
    return k;
}

static int lib_read(int port, const char *folder, const char *name, buffer_t *out)
{
    (void)port;
    buf_free(out);
    return read_held(folder) == 0 && walk_held(NULL, 0, name, out) == 1 ? 0 : -1;
}

static long long lib_free(int port)
{
    (void)port;
    return -1;   /* (no end to it: as much as the drive has, which isn't asked) */
}

static int lib_put(int port, const char *psu)
{
    buffer_t b = {0};
    char folder[33], game[12], path[260];
    int i, r;
    (void)port;
    if (!libDir[0] || mcfs_psu_bytes(psu, &b) != MCFS_OK)
        return MCFS_ERR_IO;
    if (b.len < 3 * ENT || !(le16(b.data) & DF_DIRECTORY) || !b.data[64]) {
        buf_free(&b);
        return MCFS_ERR_BAD;
    }
    snprintf(folder, sizeof(folder), "%.32s", (const char *)b.data + 64);
    if ((i = find(folder)) < 0 && nLib == LIBRARY_MAX) {
        buf_free(&b);
        return MCFS_ERR_FULL;
    }
    library_game_of(folder, game);
    snprintf(path, sizeof(path), "%s%s/", libDir, game);
    ensure_dir(path);
    snprintf(path, sizeof(path), "%s%s/%s.psu", libDir, game, folder);
    r = file_replace(path, b.data, b.len) == 0 ? MCFS_OK : MCFS_ERR_IO;   /* (read back and compared there) */
    if (r == MCFS_OK) {
        if (i < 0) {
            i = nLib++;
            memset(&lib[i], 0, sizeof(lib[i]));
            snprintf(lib[i].folder, sizeof(lib[i].folder), "%s", folder);
            snprintf(lib[i].game, sizeof(lib[i].game), "%s", game);
        }
        memcpy(lib[i].head, b.data, sizeof(lib[i].head));
        lib[i].head[1] |= DF_EXISTS >> 8;
        lib[i].sig = sign(&b);
        if (!strcmp(heldFolder, folder))
            buf_free(&held), heldFolder[0] = 0;
        if (write_index() != 0)
            r = MCFS_ERR_IO;
        group();
    }
    log_msg("library: %s kept, %u bytes: %d", path, (unsigned)b.len, r);
    buf_free(&b);
    return r;
}

static int lib_remove(int port, const char *folder)
{
    char path[260];
    int i = find(folder), r;
    (void)port;
    if (i < 0)
        return MCFS_ERR_NOT_FOUND;
    file_of(i, path, sizeof(path));
    if (unlink(path) != 0 && file_exists(path))
        return MCFS_ERR_IO;
    lib[i] = lib[--nLib];
    if (!strcmp(heldFolder, folder))
        buf_free(&held), heldFolder[0] = 0;
    r = write_index() == 0 ? MCFS_OK : MCFS_ERR_IO;
    group();
    log_msg("library: %s let go of: %d", path, r);
    return r;
}

static const mcfs_slot_t ops = {lib_dir, lib_read, lib_free, lib_put, lib_remove};

/* -------- the rest of the program's */

void library_init(const char *dir)
{
    char path[200];
    nLib = nGames = 0;
    buf_free(&held);
    heldFolder[0] = 0;
    snprintf(libDir, sizeof(libDir), "%s", dir ? dir : "");
    mcfs_library(libDir[0] ? &ops : NULL);
    if (!libDir[0])
        return;
    snprintf(path, sizeof(path), "%s%s", libDir, INDEX);
    ini_read(path, on_index, NULL);
    group();
    if (nLib)   /* (said when it has saves: an empty one is every program's that hasn't used it) */
        log_msg("library: %s, %d save(s) of %d game(s)", libDir, nLib, nGames);
}

int library_on(void) { return libDir[0] != 0; }
int library_count(void) { return nLib; }
int library_games(void) { return nGames; }
const char *library_game_name(int k) { return k >= 0 && k < nGames ? games[k].name : ""; }
const char *library_game_id(int k) { return k >= 0 && k < nGames ? games[k].id : LIBRARY_OTHER; }
int library_game_saves(int k) { return k >= 0 && k < nGames ? games[k].saves : 0; }
int library_path(const char *path) { return path && !strncmp(path, LIBRARY_PATH, sizeof(LIBRARY_PATH) - 1); }

unsigned long long library_signature(const char *folder)
{
    int i = find(folder);
    if (i >= 0 && !lib[i].sig && read_held(folder) == 0) {   /* (a list from before signatures: read once, kept) */
        lib[i].sig = sign(&held);
        write_index();
    }
    return i >= 0 ? lib[i].sig : 0;
}
