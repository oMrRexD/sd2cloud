/* SD2Cloud's regression tests -- the Library (library.c): saves kept off the cards as .psu files, as the engine
 * (mcfs.c) takes it ("lib:", "lib:<game ID>"): saves put into it, listed by game, their icons and sizes, copied from it
 * into a card's file and back, deleted, and its list read again from its file. */
#include "t.h"

#define ENT 512
#define LIB "sd/SD2Cloud/Library/"
#define SAVE "BASLUS-99999LIB"
#define APP "APP_TEST"

static void entry(unsigned char *e, unsigned int mode, unsigned int len, const char *name, int minute)
{
    static unsigned char when[8] = {0, 5, 4, 3, 2, 1, 0xEA, 0x07};   /* 2026-01-02 03:04:05 */
    memset(e, 0, ENT);
    e[0] = mode, e[1] = mode >> 8;
    e[4] = len, e[5] = len >> 8, e[6] = len >> 16, e[7] = len >> 24;
    when[2] = minute;
    memcpy(e + 8, when, 8);
    memcpy(e + 24, when, 8);
    snprintf((char *)e + 64, 32, "%s", name);
}

/* a .psu of a save with these files (name, bytes); the folder's dates at that minute */
static void psu_with(const char *path, const char *folder, int minute, const char *const *names, const buffer_t *data, int n)
{
    static const unsigned char pad[1024];
    unsigned char e[ENT];
    buffer_t b = {0};
    int i;
    entry(e, 0x8427, n + 2, folder, minute);
    buf_append(&b, e, ENT);
    entry(e, 0x8427, 0, ".", minute);
    buf_append(&b, e, ENT);
    entry(e, 0x8427, 0, "..", minute);
    buf_append(&b, e, ENT);
    for (i = 0; i < n; i++) {
        entry(e, 0x8497, (unsigned int)data[i].len, names[i], minute);
        buf_append(&b, e, ENT);
        if (data[i].len)
            buf_append(&b, data[i].data, data[i].len);
        if (data[i].len % 1024)
            buf_append(&b, pad, 1024 - data[i].len % 1024);
    }
    t_write(path, b.data, b.len);
    buf_free(&b);
}

static buffer_t filled(size_t n, unsigned int seed)
{
    buffer_t b = {0};
    unsigned char *d = malloc(n + 1);
    t_fill(d, n, seed);
    if (n)
        buf_append(&b, d, n);
    free(d);
    return b;
}

/* SAVE (a game's, by its folder's name): three files, the second empty; APP (no game's): an icon and a program its
 * title.cfg names, both put into the Library */
static void two_saves_in_the_library(void)
{
    static const char *const saveNames[] = {"DATA.BIN", "EMPTY", "BIG.BIN"};
    static const char *const appNames[] = {"icon.sys", "app.ico", "title.cfg", "Program.ELF"};
    buffer_t s[3], a[4];
    int i;
    s[0] = filled(3000, 1), s[1] = filled(0, 2), s[2] = filled(70000, 3);
    psu_with("save.psu", SAVE, 10, saveNames, s, 3);
    a[0] = filled(964, 4);
    memcpy(a[0].data, "PS2D", 4);
    memset(a[0].data + 260, 0, 64);
    strcpy((char *)a[0].data + 260, "app.ico");
    a[1] = filled(4000, 5);
    a[2] = (buffer_t){0};
    buf_append(&a[2], "title=Test\nboot=PROGRAM.ELF\n", 28);
    a[3] = filled(20000, 6);
    psu_with("app.psu", APP, 20, appNames, a, 4);
    CHECK_INT(mcfs_import_psu("save.psu", LIBRARY_PATH, NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu("app.psu", LIBRARY_PATH "SLUS-99999", NULL), MCFS_OK);   /* (to its own game's anyway) */
    for (i = 0; i < 3; i++)
        buf_free(&s[i]);
    for (i = 0; i < 4; i++)
        buf_free(&a[i]);
}

/* ------------------------------------------------------------ the tests */

static void library_kept(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    buffer_t iconsys = {0}, ico = {0};
    long long freeBytes = 0, bytes = 0;
    char boot[40];
    int files = 0;
    library_init(LIB);
    CHECK(library_on());
    CHECK_INT(library_count(), 0);
    CHECK_INT(mcfs_list_saves(LIBRARY_PATH, list, MCFS_MAX_SAVES, NULL), 0);
    two_saves_in_the_library();
    CHECK_INT(library_count(), 2);
    CHECK(t_same("save.psu", LIB "SLUS-99999/" SAVE ".psu"));   /* (the .psu as it came) */
    CHECK(t_same("app.psu", LIB LIBRARY_OTHER "/" APP ".psu"));
    CHECK_INT(library_games(), 2);
    CHECK_STR(library_game_id(0), "SLUS-99999");   /* (a game, by its name: its ID, as it isn't in the list) */
    CHECK_STR(library_game_name(0), "SLUS-99999");
    CHECK_INT(library_game_saves(0), 1);
    CHECK_STR(library_game_id(1), LIBRARY_OTHER);   /* (the saves of no game last) */
    CHECK_STR(library_game_name(1), "");
    CHECK_INT(mcfs_list_saves(LIBRARY_PATH, list, MCFS_MAX_SAVES, &freeBytes), 2);
    CHECK_STR(list[0].folder, APP);   /* the newest first */
    CHECK_STR(list[1].folder, SAVE);
    CHECK_INT(freeBytes, -1);         /* (no end to its room) */
    CHECK_INT(mcfs_list_saves(LIBRARY_PATH "SLUS-99999", list, MCFS_MAX_SAVES, NULL), 1);
    CHECK_STR(list[0].folder, SAVE);
    CHECK_INT(mcfs_list_saves(LIBRARY_PATH LIBRARY_OTHER, list, MCFS_MAX_SAVES, NULL), 1);
    CHECK_STR(list[0].folder, APP);
    CHECK_INT(mcfs_save_info(LIBRARY_PATH, SAVE, &bytes, &files), MCFS_OK);
    CHECK_INT(files, 3);
    CHECK_INT(bytes, 73000);
    CHECK_INT(mcfs_save_icon(LIBRARY_PATH, &list[0], &iconsys, &ico), 0);
    CHECK_INT(iconsys.len, 964);
    CHECK_INT(ico.len, 4000);
    CHECK_INT(mcfs_save_app(LIBRARY_PATH, &list[0], boot, sizeof(boot)), 0);   /* (nothing is started from it) */
    buf_free(&iconsys);
    buf_free(&ico);
    library_init(NULL);   /* none: nothing of it answers */
    CHECK(!library_on());
    CHECK_INT(mcfs_list_saves(LIBRARY_PATH, list, MCFS_MAX_SAVES, NULL), -1);
}

static void library_to_card_and_back(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    buffer_t fromLib = {0}, fromCard = {0};
    library_init(LIB);
    two_saves_in_the_library();
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    CHECK_INT(mcfs_copy_save(LIBRARY_PATH "SLUS-99999", SAVE, "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_list_saves("sd/a.mcd", list, MCFS_MAX_SAVES, NULL), 1);
    CHECK_STR(list[0].folder, SAVE);
    CHECK_INT(mcfs_export_psu(LIBRARY_PATH, SAVE, &fromLib), MCFS_OK);
    CHECK_INT(mcfs_export_psu("sd/a.mcd", SAVE, &fromCard), MCFS_OK);
    CHECK(fromLib.len && fromLib.len == fromCard.len && !memcmp(fromLib.data, fromCard.data, fromLib.len));
    /* out of the Library, and back into it from the card */
    CHECK_INT(mcfs_delete_save(LIBRARY_PATH, SAVE), MCFS_OK);
    CHECK_INT(library_count(), 1);
    CHECK(t_size(LIB "SLUS-99999/" SAVE ".psu") < 0);
    CHECK_INT(mcfs_delete_save(LIBRARY_PATH, SAVE), MCFS_ERR_NOT_FOUND);
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, LIBRARY_PATH, NULL), MCFS_OK);
    CHECK_INT(library_count(), 2);
    buf_free(&fromLib);
    CHECK_INT(mcfs_export_psu(LIBRARY_PATH, SAVE, &fromLib), MCFS_OK);
    CHECK(fromLib.len == fromCard.len && !memcmp(fromLib.data, fromCard.data, fromLib.len));
    /* the same save again: in the place of the one there */
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, LIBRARY_PATH, NULL), MCFS_OK);
    CHECK_INT(library_count(), 2);
    buf_free(&fromLib);
    buf_free(&fromCard);
}

/* FNV-1a of a file, as the backup by save signs a .psu */
static unsigned long long fnv(const char *path)
{
    buffer_t b = t_read(path);
    unsigned long long h = 1469598103934665603ULL;
    size_t k;
    for (k = 0; k < b.len; k++)
        h = (h ^ b.data[k]) * 1099511628211ULL;
    buf_free(&b);
    return h;
}

static void library_list_read_again(void)
{
    static mcfs_save_t before[MCFS_MAX_SAVES], after[MCFS_MAX_SAVES];
    unsigned long long sig;
    int n, i;
    library_init(LIB);
    two_saves_in_the_library();
    n = mcfs_list_saves(LIBRARY_PATH, before, MCFS_MAX_SAVES, NULL);
    sig = library_signature(SAVE);
    CHECK(sig && sig == fnv("save.psu"));   /* (the .psu's, kept since it went in) */
    CHECK(library_signature("BASLUS-00000NONE") == 0);
    library_init(LIB);   /* (as when the program starts again) */
    CHECK(library_signature(SAVE) == sig);
    CHECK_INT(library_count(), 2);
    CHECK_INT(mcfs_list_saves(LIBRARY_PATH, after, MCFS_MAX_SAVES, NULL), n);
    for (i = 0; i < n; i++) {
        CHECK_STR(after[i].folder, before[i].folder);
        CHECK(after[i].when == before[i].when);
    }
    /* a file that isn't a .psu isn't kept */
    t_text("not.psu", "hello");
    CHECK(mcfs_import_psu("not.psu", LIBRARY_PATH, NULL) != MCFS_OK);
    CHECK_INT(library_count(), 2);
    library_init(NULL);
}

void suite_library(void)
{
    RUN(library_kept);
    RUN(library_to_card_and_back);
    RUN(library_list_read_again);
}
