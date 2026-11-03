/* SD2Cloud -- the safety copies, when they are turned on (Settings > Cards; [cards] safety = yes): a card's file kept in
 * the data folder (Safety/) before it is deleted, made again of another size, or has its saves moved off it, the newest
 * ten; and a save, as a .psu, before it is deleted or replaced, the newest thirty. And the Tools' list of them, where
 * one is put back. */
#include "app.h"

#define SAFETY_KEEP       10   /* cards */
#define SAFETY_KEEP_SAVES 30   /* saves */
#define SAFETY_MAX        96
#define SAVE_MARK         " ~ "   /* between a save copy's card and its own folder, in its name */

/* <data>Safety/<folder>/<n> <base> <yyyy-mm-dd hhhmmmss><ext>: the card's folder and name, and when it was kept (a save:
 * <n> <base> ~ <its folder> <yyyy-...>.psu, its card's name and its own). The
 * moment is the console clock's, which may be anything (years ahead, or back to 2000 with a flat battery): the copies
 * are told apart in age by n, one more than the newest's each time, so the clock never decides which ones go */
typedef struct {
    char folder[48], file[120];
    int n;   /* 0 = a copy made before they were numbered */
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

/* the number a copy's name starts with ("0012 Card1-1 ..."), and how long it is with its space; 0 if it has none */
static int number_of(const char *file, int *len)
{
    int i = 0;
    while (file[i] >= '0' && file[i] <= '9')
        i++;
    if (i < 4 || file[i] != ' ')
        return 0;
    if (len)
        *len = i + 1;
    return atoi(file);
}

static int newest_first(const void *a, const void *b)
{
    const copy_t *x = a, *y = b;
    if (x->n != y->n)
        return y->n - x->n;
    return strncmp(stamp_of(y->file), stamp_of(x->file), STAMP);
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
                copies[nCopies].n = number_of(files[j].name, NULL);
                nCopies++;
            }
    }
    qsort(copies, nCopies, sizeof(copies[0]), newest_first);
    return nCopies;
}

static void copy_path(const copy_t *k, char *out, size_t size) { snprintf(out, size, "%sSafety/%s/%s", dataDir, k->folder, k->file); }

static int is_save_copy(const copy_t *k) { return file_ext(k->file, ".psu"); }

/* its card's name, and when it was kept, as the screen says it (a save's: its card's name, and its own folder in save,
 * when save isn't NULL) */
static void copy_names_of(const copy_t *k, char *base, size_t baseSize, char *when, size_t whenSize, char *save, size_t saveSize)
{
    const char *s = stamp_of(k->file), *name = k->file, *mark;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, len = 0;
    if (number_of(name, &len))
        name += len;
    snprintf(base, baseSize, "%.*s", (int)(s - 1 - name), name);
    if (save)
        save[0] = 0;
    if (is_save_copy(k) && (mark = strstr(base, SAVE_MARK)) != NULL) {
        if (save)
            snprintf(save, saveSize, "%s", mark + strlen(SAVE_MARK));
        base[mark - base] = 0;
    }
    sscanf(s, "%d-%d-%d %dh%d", &y, &mo, &d, &h, &mi);
    format_when(y, mo, d, h, mi, when, whenSize);
}

static void copy_names(const copy_t *k, char *base, size_t baseSize, char *when, size_t whenSize)
{
    copy_names_of(k, base, baseSize, when, whenSize, NULL, 0);
}

/* the copies past the newest ones kept, of cards or of saves, let go of */
static void let_go_of_old(void)
{
    char path[320];
    int i, cardsKept = 0, savesKept = 0;
    list_copies();
    for (i = 0; i < nCopies; i++) {
        int save = is_save_copy(&copies[i]);
        if (save ? ++savesKept <= SAFETY_KEEP_SAVES : ++cardsKept <= SAFETY_KEEP)
            continue;
        copy_path(&copies[i], path, sizeof(path));
        log_msg("safety copy %s: let go of (%d)", path, unlink(path));
    }
}

/* a name's letters that a folder of the microSD can't have, as _ */
static void name_safe(char *s)
{
    for (; *s; s++)
        if (strchr("\\/:*?\"<>|", *s))
            *s = '_';
}

int safety_save(const card_t *c, const char *folder, int ask)
{
    buffer_t b = {0};
    char dir[200], path[400], who[64], base[64], save[40];
    datetime_t t;
    int n, r;
    if (!cfg.safety)
        return 0;
    snprintf(who, sizeof(who), "%s", c->folder[0] ? c->folder : c->base[0] ? c->base : "Other");
    snprintf(base, sizeof(base), "%s", c->base[0] ? c->base : who);
    snprintf(save, sizeof(save), "%s", folder);
    name_safe(who);
    name_safe(base);
    name_safe(save);
    r = mcfs_export_psu(c->path, folder, &b);
    if (r == MCFS_ERR_NOT_FOUND) {   /* (none there: nothing to keep) */
        buf_free(&b);
        return 0;
    }
    if (r == MCFS_OK) {
        snprintf(dir, sizeof(dir), "%sSafety/%s/", dataDir, who);
        ensure_dir(dir);
        n = list_copies() ? copies[0].n + 1 : 1;
        local_time(&t);
        snprintf(path, sizeof(path), "%s%04d %s" SAVE_MARK "%s %04d-%02d-%02d %02dh%02dm%02d.psu", dir, n, base, save,
                 t.year, t.month, t.day, t.hour, t.minute, t.second);
        r = file_write_checked(path, b.data, b.len) ? MCFS_OK : MCFS_ERR_IO;
    }
    buf_free(&b);
    if (r != MCFS_OK) {
        log_msg("safety copy of %s of %s: not made (%d)", folder, c->id, r);
        return !ask || confirm(T(T_SAFETY_FAILED), T(T_SAFETY_FAILED_TEXT), T_CONTINUE) ? 0 : -1;
    }
    log_msg("safety copy of %s of %s: %s", folder, c->id, path);
    let_go_of_old();
    return 0;
}

int safety_copy(const card_t *c)
{
    char dir[200], path[320];
    const char *ext = strrchr(c->path, '.');
    datetime_t t;
    int n, r;
    if (!cfg.safety || mcfs_slot_port(c->path) >= 0 || card_file_card(c) || !ext)
        return 0;   /* (turned off; or not one of the microSD's cards) */
    snprintf(dir, sizeof(dir), "%sSafety/%s/", dataDir, c->folder);
    ensure_dir(dir);
    n = list_copies() ? copies[0].n + 1 : 1;
    local_time(&t);
    snprintf(path, sizeof(path), "%s%04d %s %04d-%02d-%02d %02dh%02dm%02d%s", dir, n, c->base, t.year, t.month,
             t.day, t.hour, t.minute, t.second, ext);
    card_work(c, 0, T(T_SAFETY_MAKING), 1);
    r = file_copy_shown(c->path, path, upload_screen);
    card_work_done();
    if (r != 0) {
        log_msg("safety copy of %s: not made", c->id);
        return confirm(T(T_SAFETY_FAILED), T(T_SAFETY_FAILED_TEXT), T_CONTINUE) ? 0 : -1;
    }
    log_msg("safety copy of %s: %s", c->id, path);
    let_go_of_old();
    return 0;
}

/* a save's copy put back: into the card picked, as a .psu of the Files tab goes (replacing the one of its folder there,
 * when it has one, if that is agreed to) */
static void restore_save_copy(const copy_t *k)
{
    char from[320];
    copy_path(k, from, sizeof(from));
    if (!psu_load(from))
        return;
    psu_import();
    psu_close();
}

/* what a copy is called: in the list (when), when it was kept; beside it (note), what it is of: a whole card, or a save
 * of a card, by its game (or folder); over its options (title), the card's name, or the save's game */
static void copy_text(const copy_t *k, char *when, size_t whenSize, char *note, size_t noteSize, char *title, size_t titleSize)
{
    char base[64], save[40], id[12], game[96];
    copy_names_of(k, base, sizeof(base), when, whenSize, save, sizeof(save));
    if (is_save_copy(k) && (!save_game_id(save, id) || !game_title(id, game, sizeof(game))))
        snprintf(game, sizeof(game), "%s", save);
    if (is_save_copy(k))
        snprintf(note, noteSize, T(T_SAFETY_SAVE_OF), game, base);
    else
        snprintf(note, noteSize, T(T_SAFETY_CARD_OF), base);
    snprintf(title, titleSize, "%s", is_save_copy(k) ? game : base);
}

static char copyNotes[SAFETY_MAX][200];
static const char *copy_note(int i) { return copyNotes[i]; }

/* -------- Tools > "Safety copies": each one put back as its card (in its place, or in place of it) or deleted */

/* the card a copy is of: where its file goes, and the card when it is there */
static card_t *copy_card(const copy_t *k, const char *base, char *target, size_t size, char *file, size_t fileSize)
{
    const char *ext = strrchr(k->file, '.');
    const vmc_place_t *p = vmc_place(k->folder);
    int i;
    target[0] = 0;
    if (p) {   /* (a VMC: in its place's folder) */
        snprintf(file, fileSize, "%s.bin", base);
        snprintf(target, size, "%s/%s", p->dir, file);
    } else if (!strncmp(k->folder, VMC_FOLDER "-", sizeof(VMC_FOLDER)))   /* (a VMC of a folder not read now) */
        return NULL;
    else {
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
    static card_t shown;   /* (on the screen while it is written: the card, or its name when it isn't there) */
    char base[64], when[40], from[320], target[260], file[80], tmp[280], t[300], dir[200];
    card_t *c;
    int r;
    copy_names(k, base, sizeof(base), when, sizeof(when));
    copy_path(k, from, sizeof(from));
    c = copy_card(k, base, target, sizeof(target), file, sizeof(file));
    if (!target[0]) {
        message_wait(0, NULL, COLOR_WARN, T(T_VMC_PLACE_GONE));
        return;
    }
    snprintf(t, sizeof(t), T(c ? T_SAFETY_REPLACE_ASK : T_SAFETY_RESTORE_ASK), base, when);
    if (!confirm(t, T(c ? T_SAFETY_REPLACE_TEXT : T_SAFETY_RESTORE_TEXT), T_RESTORE))
        return;
    if (c && card_free(c, NULL))
        return;
    if (c)
        shown = *c;
    else {
        memset(&shown, 0, sizeof(shown));
        snprintf(shown.base, sizeof(shown.base), "%s", base);
    }
    card_work(&shown, 0, T(T_RESTORE_STEP_WRITE), 1);
    if (c) {   /* (written beside it first, and only then put in its place) */
        snprintf(tmp, sizeof(tmp), "%s.new", target);
        unlink(tmp);
        r = file_copy_shown(from, tmp, upload_screen);
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
        r = file_copy_shown(from, target, upload_screen);
    }
    card_work_done();
    if (r == 0 && !c && (c = vmc_place(k->folder) ? vmc_append(k->folder, base) : cards_append(k->folder, file)) != NULL) {
        cards_recheck(c);
        cardsUnsorted = 1;
    }
    log_msg("safety copy %s put back at %s: %d", from, target, r);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_SAFETY_RESTORED : T_SAFETY_RESTORE_FAILED));
}

void safety_screen(void)
{
    static const char *items[SAFETY_MAX];
    static char text[SAFETY_MAX][120];
    const char *acts[2];
    char base[64], when[40], path[320], t[300], whole[160];
    int i, k = 0, a;
    for (;;) {
        message(0, NULL, COLOR_TEXT, T(T_LOADING));
        if (!list_copies()) {
            message_wait(0, NULL, COLOR_TEXT, T(T_SAFETY_NONE));
            return;
        }
        for (i = 0; i < nCopies; i++) {   /* (each by when it was kept; beside it, what it is of) */
            copy_text(&copies[i], text[i], sizeof(text[0]), copyNotes[i], sizeof(copyNotes[0]), whole, sizeof(whole));
            items[i] = text[i];
        }
        if ((k = choose_noted(T(T_SAFETY_TITLE), items, nCopies, k < nCopies ? k : 0, copy_note)) < 0)
            return;
        acts[0] = T(T_RESTORE);
        acts[1] = T(T_DELETE);
        copy_text(&copies[k], t, sizeof(t), path, sizeof(path), whole, sizeof(whole));
        fit(FONT_TEXT, whole, sizeof(whole), 320);
        if ((a = choose(whole, acts, 2, 0)) == 0) {
            if (is_save_copy(&copies[k]))
                restore_save_copy(&copies[k]);
            else
                restore_copy(&copies[k]);
        } else if (a == 1) {
            copy_names(&copies[k], base, sizeof(base), when, sizeof(when));
            if (is_save_copy(&copies[k]))   /* (a save's: by its game, or its folder) */
                snprintf(base, sizeof(base), "%s", whole);
            snprintf(t, sizeof(t), T(T_SAFETY_DELETE_ASK), base, when);
            if (confirm(t, NULL, T_DELETE)) {
                copy_path(&copies[k], path, sizeof(path));
                log_msg("safety copy %s deleted: %d", path, unlink(path));
            }
        }
    }
}
