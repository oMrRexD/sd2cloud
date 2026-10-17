/* SD2Cloud -- the safety copies: a card's file kept in the data folder (Safety/) before it is deleted, made again of
 * another size, or has its saves moved off it, the newest ten; and the Tools' list of them, where one is put back. */
#include "app.h"

#define SAFETY_KEEP 10
#define SAFETY_MAX  64

/* <data>Safety/<folder>/<base> <yyyy-mm-dd hhhmmmss><ext>: the card's folder and name, and when it was kept */
typedef struct {
    char folder[48], file[120];
} copy_t;
static copy_t copies[SAFETY_MAX];
static int nCopies;

#define STAMP 19   /* "2026-10-17 03h05m12" */

/* the moment in a copy's name (NULL = it isn't one of them) */
static const char *stamp_of(const char *file)
{
    const char *dot = strrchr(file, '.');
    size_t n = dot ? (size_t)(dot - file) : strlen(file);
    return n > STAMP + 1 && file[n - STAMP - 1] == ' ' ? file + n - STAMP : NULL;
}

static int newest_first(const void *a, const void *b)
{
    return strncmp(stamp_of(((const copy_t *)b)->file), stamp_of(((const copy_t *)a)->file), STAMP);
}

static int list_copies(void)
{
    static dir_entry_t folders[SAFETY_MAX], files[SAFETY_MAX];
    char dir[200], sub[260];
    int nf, n, i, j;
    nCopies = 0;
    snprintf(dir, sizeof(dir), "%sSafety/", dataDir);
    if ((nf = dir_list(dir, folders, SAFETY_MAX)) < 0)
        return 0;
    for (i = 0; i < nf; i++) {
        if (!folders[i].dir)
            continue;
        snprintf(sub, sizeof(sub), "%s%s/", dir, folders[i].name);
        n = dir_list(sub, files, SAFETY_MAX);
        for (j = 0; j < n && nCopies < SAFETY_MAX; j++)
            if (!files[j].dir && stamp_of(files[j].name)) {
                snprintf(copies[nCopies].folder, sizeof(copies[0].folder), "%s", folders[i].name);
                snprintf(copies[nCopies].file, sizeof(copies[0].file), "%s", files[j].name);
                nCopies++;
            }
    }
    qsort(copies, nCopies, sizeof(copies[0]), newest_first);
    return nCopies;
}

static void copy_path(const copy_t *k, char *out, size_t size) { snprintf(out, size, "%sSafety/%s/%s", dataDir, k->folder, k->file); }

/* its card's name, and when it was kept, as the screen says it */
static void copy_names(const copy_t *k, char *base, size_t baseSize, char *when, size_t whenSize)
{
    const char *s = stamp_of(k->file);
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    snprintf(base, baseSize, "%.*s", (int)(s - 1 - k->file), k->file);
    sscanf(s, "%d-%d-%d %dh%d", &y, &mo, &d, &h, &mi);
    format_when(y, mo, d, h, mi, when, whenSize);
}

int safety_copy(const card_t *c)
{
    char dir[200], path[320];
    const char *ext = strrchr(c->path, '.');
    datetime_t t;
    int i;
    if (mcfs_slot_port(c->path) >= 0 || card_file_card(c) || !ext)
        return 0;   /* (not one of the microSD's cards) */
    snprintf(dir, sizeof(dir), "%sSafety/%s/", dataDir, c->folder);
    ensure_dir(dir);
    local_time(&t);
    snprintf(path, sizeof(path), "%s%s %04d-%02d-%02d %02dh%02dm%02d%s", dir, c->base, t.year, t.month, t.day, t.hour,
             t.minute, t.second, ext);
    message(0, NULL, COLOR_TEXT, T(T_SAFETY_MAKING));
    if (file_copy(c->path, path) != 0) {
        log_msg("safety copy of %s: not made", c->id);
        return confirm(T(T_SAFETY_FAILED), T(T_SAFETY_FAILED_TEXT), T_CONTINUE) ? 0 : -1;
    }
    log_msg("safety copy of %s: %s", c->id, path);
    list_copies();
    for (i = nCopies - 1; i >= SAFETY_KEEP; i--) {   /* (the newest ten stay) */
        copy_path(&copies[i], path, sizeof(path));
        log_msg("safety copy %s: let go of (%d)", path, unlink(path));
    }
    return 0;
}

/* -------- Tools > "Safety copies": each one put back as its card (in its place, or in place of it) or deleted */

/* the card a copy is of: where its file goes, and the card when it is there */
static card_t *copy_card(const copy_t *k, const char *base, char *target, size_t size, char *file, size_t fileSize)
{
    const char *ext = strrchr(k->file, '.');
    int i;
    if (!strcmp(k->folder, VMC_FOLDER)) {
        snprintf(file, fileSize, "%s.bin", base);
        snprintf(target, size, "%sVMC/%s", sdRoot, file);
    } else {
        snprintf(file, fileSize, "%s%s", base, ext ? ext : dev->ext);
        snprintf(target, size, "%s%s/%s/%s", sdRoot, dev->cards, k->folder, file);
    }
    for (i = 0; i < nCards; i++)
        if (!strcasecmp(cards[i].path, target))
            return &cards[i];
    return NULL;
}

static void restore_copy(const copy_t *k)
{
    char base[64], when[40], from[320], target[260], file[80], tmp[280], t[300], dir[200];
    card_t *c;
    int r;
    copy_names(k, base, sizeof(base), when, sizeof(when));
    copy_path(k, from, sizeof(from));
    c = copy_card(k, base, target, sizeof(target), file, sizeof(file));
    snprintf(t, sizeof(t), T(c ? T_SAFETY_REPLACE_ASK : T_SAFETY_RESTORE_ASK), base, when);
    if (!confirm(t, T(c ? T_SAFETY_REPLACE_TEXT : T_SAFETY_RESTORE_TEXT), T_RESTORE))
        return;
    if (c && card_free(c, NULL))
        return;
    message(0, NULL, COLOR_TEXT, T(T_RESTORE_STEP_WRITE));
    if (c) {   /* (written beside it first, and only then put in its place) */
        snprintf(tmp, sizeof(tmp), "%s.new", target);
        unlink(tmp);
        r = file_copy(from, tmp);
        if (r == 0 && (r = unlink(target)) != 0 && !file_exists(target))
            r = 0;
        if (r == 0)
            r = file_move(tmp, target);
        if (r == 0)
            cards_recheck(c);
        else
            unlink(tmp);
    } else {
        snprintf(dir, sizeof(dir), "%.*s", (int)(strrchr(target, '/') - target + 1), target);
        ensure_dir(dir);
        r = file_copy(from, target);
        if (r == 0 && (c = !strcmp(k->folder, VMC_FOLDER) ? vmc_append(base) : cards_append(k->folder, file)) != NULL) {
            cards_recheck(c);
            cardsUnsorted = 1;
        }
    }
    log_msg("safety copy %s put back at %s: %d", from, target, r);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_SAFETY_RESTORED : T_SAFETY_RESTORE_FAILED));
}

void safety_screen(void)
{
    static const char *items[SAFETY_MAX];
    static char text[SAFETY_MAX][120];
    const char *acts[2];
    char base[64], when[40], path[320], t[300];
    int i, k = 0, a;
    for (;;) {
        message(0, NULL, COLOR_TEXT, T(T_LOADING));
        if (!list_copies()) {
            message_wait(0, NULL, COLOR_TEXT, T(T_SAFETY_NONE));
            return;
        }
        for (i = 0; i < nCopies; i++) {
            copy_names(&copies[i], base, sizeof(base), when, sizeof(when));
            snprintf(text[i], sizeof(text[0]), "%s · %s", base, when);
            items[i] = text[i];
        }
        if ((k = choose(T(T_SAFETY_TITLE), items, nCopies, k < nCopies ? k : 0)) < 0)
            return;
        acts[0] = T(T_RESTORE);
        acts[1] = T(T_DELETE);
        if ((a = choose(text[k], acts, 2, 0)) == 0)
            restore_copy(&copies[k]);
        else if (a == 1) {
            copy_names(&copies[k], base, sizeof(base), when, sizeof(when));
            snprintf(t, sizeof(t), T(T_SAFETY_DELETE_ASK), base, when);
            if (confirm(t, NULL, T_DELETE)) {
                copy_path(&copies[k], path, sizeof(path));
                log_msg("safety copy %s deleted: %d", path, unlink(path));
            }
        }
    }
}
