/* SD2Cloud's regression tests -- a memory card in a slot of the console ("mc1:"), as the engine (mcfs.c) takes it:
 * its saves listed, their icons and sizes, a save copied from it into a card's file and back, exported, imported and
 * deleted. The card in the slot is made up here, in memory, and given to mcfs_slot as the platform's calls would be
 * (the PS2's mcman, in helper.c, isn't on the PC). */
#include "t.h"

#define ENT 512
#define SAVE "BASLUS-99999SLOT"
#define APP "APP_TEST"

static unsigned int get32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

/* ------------------------------------------------------------ the card in the slot, made up */

#define FAKE_SAVES 8
#define FAKE_FILES 8
static struct {
    int used, n;
    unsigned char ent[ENT];   /* the folder's own entry */
    unsigned char file[FAKE_FILES][ENT];
    buffer_t data[FAKE_FILES];
} fake[FAKE_SAVES];
static int fakePuts;

static int fake_find(const char *folder)
{
    int i;
    for (i = 0; i < FAKE_SAVES; i++)
        if (fake[i].used && !strncmp((const char *)fake[i].ent + 64, folder, 32))
            return i;
    return -1;
}

static void dot(unsigned char *e, const char *name)
{
    memset(e, 0, ENT);
    e[0] = 0x27, e[1] = 0x84;
    strcpy((char *)e + 64, name);
}

static int fake_dir(int port, const char *folder, unsigned char (*ent)[MCFS_ENT], int max)
{
    int i, n = 0, k;
    if (port != 1 || max < 2)
        return -1;
    dot(ent[n++], ".");   /* (mcman lists these too) */
    dot(ent[n++], "..");
    if (!folder) {
        for (i = 0; i < FAKE_SAVES && n < max; i++)
            if (fake[i].used)
                memcpy(ent[n++], fake[i].ent, ENT);
        return n;
    }
    if ((k = fake_find(folder)) < 0)
        return -1;
    for (i = 0; i < fake[k].n && n < max; i++)
        memcpy(ent[n++], fake[k].file[i], ENT);
    return n;
}

static int fake_read(int port, const char *folder, const char *name, buffer_t *out)
{
    int k = fake_find(folder), i;
    buf_free(out);
    if (port != 1 || k < 0)
        return -1;
    for (i = 0; i < fake[k].n; i++)
        if (!strncmp((const char *)fake[k].file[i] + 64, name, 32))
            return fake[k].data[i].len && buf_append(out, fake[k].data[i].data, fake[k].data[i].len) ? -1 : 0;
    return -1;
}

static long long fake_free(int port) { return port == 1 ? 1000000 : -1; }

/* (mcfs_psu_files, as helper.c's mc_put_save writes a .psu into the card in a slot) */
static int put_piece(const unsigned char *entry, const unsigned char *data, unsigned int len, void *u)
{
    int k = *(int *)u, n;
    if (!data && (entry[0] & 0x20)) {
        if (fake_find((const char *)entry + 64) >= 0)
            return MCFS_ERR_EXISTS;
        memcpy(fake[k].ent, entry, ENT);
        return 0;
    }
    if ((n = fake[k].n) >= FAKE_FILES)
        return MCFS_ERR_FULL;
    memcpy(fake[k].file[n], entry, ENT);
    buf_free(&fake[k].data[n]);
    if (len)
        buf_append(&fake[k].data[n], data, len);
    fake[k].n++;
    return 0;
}

static int fake_put(int port, const char *psu)
{
    int k, r;
    if (port != 1)
        return MCFS_ERR_IO;
    for (k = 0; k < FAKE_SAVES && fake[k].used; k++)
        ;
    if (k == FAKE_SAVES)
        return MCFS_ERR_FULL;
    fake[k].n = 0;
    if ((r = mcfs_psu_files(psu, put_piece, &k)) != 0)
        return r;
    fake[k].used = 1;
    fakePuts++;
    return MCFS_OK;
}

static int fake_remove(int port, const char *folder)
{
    int k = fake_find(folder), i;
    if (port != 1)
        return MCFS_ERR_IO;
    if (k < 0)
        return MCFS_ERR_NOT_FOUND;
    for (i = 0; i < fake[k].n; i++)
        buf_free(&fake[k].data[i]);
    fake[k].used = 0;
    return MCFS_OK;
}

static const mcfs_slot_t FAKE = {fake_dir, fake_read, fake_free, fake_put, fake_remove};

static void fake_empty(void)
{
    int k, i;
    for (k = 0; k < FAKE_SAVES; k++) {
        for (i = 0; i < FAKE_FILES; i++)
            buf_free(&fake[k].data[i]);
        memset(&fake[k], 0, sizeof(fake[k]));
    }
    fakePuts = 0;
    mcfs_slot(&FAKE);
}

/* ------------------------------------------------------------ saves made up */

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

/* SAVE: three files, the second empty; APP: an icon (icon.sys naming it) and a program its title.cfg names */
static void two_saves_in_the_slot(void)
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
    CHECK_INT(mcfs_import_psu("save.psu", "mc1:", NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu("app.psu", "mc1:", NULL), MCFS_OK);
    for (i = 0; i < 3; i++)
        buf_free(&s[i]);
    for (i = 0; i < 4; i++)
        buf_free(&a[i]);
}

/* ------------------------------------------------------------ the tests */

static void slot_paths(void)
{
    CHECK_INT(mcfs_slot_port("mc0:"), 0);
    CHECK_INT(mcfs_slot_port("mc1:"), 1);
    CHECK_INT(mcfs_slot_port("mc2:"), -1);
    CHECK_INT(mcfs_slot_port("mc1:/APP_TEST"), -1);
    CHECK_INT(mcfs_slot_port("mmce0:/MemoryCards/PS2/Card1/Card1-1.mcd"), -1);
    CHECK_INT(mcfs_slot_port("sd/a.mcd"), -1);
}

static void slot_saves_listed(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    buffer_t iconsys = {0}, ico = {0};
    long long freeBytes = 0, bytes = 0;
    char boot[40];
    int files = 0;
    fake_empty();
    two_saves_in_the_slot();
    CHECK_INT(fakePuts, 2);
    CHECK_INT(mcfs_list_saves("mc1:", list, MCFS_MAX_SAVES, &freeBytes), 2);
    CHECK_STR(list[0].folder, APP);   /* the newest first */
    CHECK_STR(list[1].folder, SAVE);
    CHECK_INT(freeBytes, 1000000);
    CHECK_INT(mcfs_list_saves("mc0:", list, MCFS_MAX_SAVES, NULL), -1);   /* (no card in that slot) */
    CHECK_INT(mcfs_save_info("mc1:", SAVE, &bytes, &files), MCFS_OK);
    CHECK_INT(files, 3);
    CHECK_INT(bytes, 73000);
    CHECK_INT(mcfs_save_icon("mc1:", &list[0], &iconsys, &ico), 0);
    CHECK_INT(iconsys.len, 964);
    CHECK_INT(ico.len, 4000);
    CHECK(mcfs_save_icon("mc1:", &list[1], &iconsys, &ico) != 0 && !iconsys.len && !ico.len);   /* (one without) */
    CHECK_INT(mcfs_save_app("mc1:", &list[0], boot, sizeof(boot)), 1);
    CHECK_STR(boot, "Program.ELF");   /* the name as the folder has it */
    CHECK_INT(mcfs_save_app("mc1:", &list[1], boot, sizeof(boot)), 0);
    buf_free(&iconsys);
    buf_free(&ico);
    mcfs_slot(NULL);
    CHECK_INT(mcfs_list_saves("mc1:", list, MCFS_MAX_SAVES, NULL), -1);
}

static void slot_to_card_and_back(void)
{
    buffer_t fromSlot = {0}, fromCard = {0}, again = {0};
    long long bytes = 0;
    int files = 0;
    fake_empty();
    two_saves_in_the_slot();
    CHECK_INT(mcfs_export_psu("mc1:", SAVE, &fromSlot), MCFS_OK);
    CHECK_INT(get32(fromSlot.data + 4), 5);   /* ".", ".." and the three files */
    /* into a card's file: the same files, the same dates */
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    CHECK_INT(mcfs_copy_save("mc1:", SAVE, "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_save_info("sd/a.mcd", SAVE, &bytes, &files), MCFS_OK);
    CHECK_INT(files, 3);
    CHECK_INT(bytes, 73000);
    CHECK_INT(mcfs_export_psu("sd/a.mcd", SAVE, &fromCard), MCFS_OK);
    CHECK(fromCard.len == fromSlot.len && !memcmp(fromCard.data, fromSlot.data, fromCard.len));
    CHECK_INT(mcfs_copy_save("mc1:", SAVE, "sd/a.mcd", NULL), MCFS_ERR_EXISTS);
    CHECK_INT(mcfs_copy_save("mc1:", "NOT-THERE", "sd/a.mcd", NULL), MCFS_ERR_NOT_FOUND);
    /* out of the slot, and back into it from the card */
    CHECK_INT(mcfs_delete_save("mc1:", SAVE), MCFS_OK);
    CHECK_INT(mcfs_delete_save("mc1:", SAVE), MCFS_ERR_NOT_FOUND);
    CHECK_INT(mcfs_save_info("mc1:", SAVE, &bytes, &files), MCFS_ERR_IO);
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, "mc1:", NULL), MCFS_OK);
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, "mc1:", NULL), MCFS_ERR_EXISTS);
    CHECK_INT(mcfs_export_psu("mc1:", SAVE, &again), MCFS_OK);
    CHECK(again.len == fromSlot.len && !memcmp(again.data, fromSlot.data, again.len));
    /* a .psu held in memory is read as its file is */
    mcfs_psu_memory(fromSlot.data, fromSlot.len);
    CHECK_INT(mcfs_import_psu(MCFS_PSU_MEMORY, "sd/a.mcd", NULL), MCFS_ERR_EXISTS);
    CHECK_INT(mcfs_new_card("sd/b.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu(MCFS_PSU_MEMORY, "sd/b.mcd", NULL), MCFS_OK);
    mcfs_psu_memory(NULL, 0);
    CHECK_INT(mcfs_import_psu(MCFS_PSU_MEMORY, "sd/b.mcd", NULL), MCFS_ERR_IO);
    /* never from the slot to itself */
    CHECK_INT(mcfs_copy_save("mc1:", APP, "mc1:", NULL), MCFS_ERR_IO);
    buf_free(&fromSlot);
    buf_free(&fromCard);
    buf_free(&again);
}

static int stopAt = -1, told;
static int on_step(int phase, long long done, long long total)
{
    (void)done;
    (void)total;
    told |= 1 << phase;
    return phase == stopAt;
}

static void slot_copy_given_up(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    fake_empty();
    two_saves_in_the_slot();
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    /* given up while it is read from the slot: the card is left without it */
    stopAt = MCFS_STEP_READ, told = 0;
    CHECK_INT(mcfs_copy_save("mc1:", SAVE, "sd/a.mcd", on_step), MCFS_ERR_CANCELLED);
    CHECK_INT(mcfs_list_saves("sd/a.mcd", list, MCFS_MAX_SAVES, NULL), 0);
    stopAt = -1, told = 0;
    CHECK_INT(mcfs_copy_save("mc1:", SAVE, "sd/a.mcd", on_step), MCFS_OK);
    CHECK(told & (1 << MCFS_STEP_READ));
    CHECK(told & (1 << MCFS_STEP_WRITE));
    /* into the slot: told as being read back (nothing to give up there), whatever is answered */
    CHECK_INT(mcfs_delete_save("mc1:", SAVE), MCFS_OK);
    stopAt = MCFS_STEP_CHECK, told = 0;
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, "mc1:", on_step), MCFS_OK);
    CHECK(told & (1 << MCFS_STEP_CHECK));
    stopAt = -1;
}

void suite_slot(void)
{
    RUN(slot_paths);
    RUN(slot_saves_listed);
    RUN(slot_to_card_and_back);
    RUN(slot_copy_given_up);
    fake_empty();
    mcfs_slot(NULL);
}
