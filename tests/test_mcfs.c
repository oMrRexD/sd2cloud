/* SD2Cloud's regression tests -- the memory card's file system (mcfs.c): a new card, a save's way in from a .psu and
 * out to one, from card to card, and off a card; what a card's fingerprint and its root's signature tell apart.
 * The cards and the saves are made here: nothing in them comes from a game. */
#include "t.h"

#define ENT 512
#define CARD_BYTES (8 * 1024 * 1024)
#define SAVE "BASLUS-99999TESTS"

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

static unsigned int get32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

/* an entry as a memory card keeps it (and a .psu after it): 0x8427 = a folder, 0x8497 = a file */
static void entry(unsigned char *e, unsigned int mode, unsigned int len, const char *name)
{
    static const unsigned char when[8] = {0, 5, 4, 3, 2, 1, 0xEA, 0x07};   /* 2026-01-02 03:04:05 */
    memset(e, 0, ENT);
    e[0] = mode;
    e[1] = mode >> 8;
    put32(e + 4, len);
    memcpy(e + 8, when, 8);
    memcpy(e + 24, when, 8);
    snprintf((char *)e + 64, 32, "%s", name);
}

/* a .psu of that save folder, with n files (file0.bin...) of those sizes: file i holds t_fill(seed + i) */
void make_psu(const char *path, const char *folder, const unsigned int *sizes, int n, unsigned int seed)
{
    static const unsigned char pad[1024];
    unsigned char e[ENT];
    buffer_t b = {0};
    char name[32];
    int i;
    entry(e, 0x8427, n + 2, folder);
    buf_append(&b, e, ENT);
    entry(e, 0x8427, 0, ".");
    buf_append(&b, e, ENT);
    entry(e, 0x8427, 0, "..");
    buf_append(&b, e, ENT);
    for (i = 0; i < n; i++) {
        unsigned char *d = malloc(sizes[i] + 1);
        snprintf(name, sizeof(name), "file%d.bin", i);
        entry(e, 0x8497, sizes[i], name);
        buf_append(&b, e, ENT);
        t_fill(d, sizes[i], seed + i);
        buf_append(&b, d, sizes[i]);
        if (sizes[i] % 1024)
            buf_append(&b, pad, 1024 - sizes[i] % 1024);
        free(d);
    }
    t_write(path, b.data, b.len);
    buf_free(&b);
}

/* does that .psu (in memory) hold exactly the files make_psu put in one? */
static int psu_holds(const buffer_t *b, const char *folder, const unsigned int *sizes, int n, unsigned int seed)
{
    size_t at = 3 * ENT;
    char name[32];
    int i, ok = 1;
    if (b->len < at || strcmp((const char *)b->data + 64, folder) != 0 || get32(b->data + 4) != (unsigned int)n + 2)
        return 0;
    /* where an entry was on the card it came from (its cluster, its folder's entry) doesn't go into a .psu */
    for (i = 0; i < 3; i++)
        if (get32(b->data + i * ENT + 16) || get32(b->data + i * ENT + 20))
            return 0;
    for (i = 0; i < n && ok; i++) {
        unsigned char *d = malloc(sizes[i] + 1);
        snprintf(name, sizeof(name), "file%d.bin", i);
        t_fill(d, sizes[i], seed + i);
        ok = at + ENT + sizes[i] <= b->len && !strcmp((const char *)b->data + at + 64, name) && get32(b->data + at + 4) == sizes[i] &&
             !get32(b->data + at + 16) && !get32(b->data + at + 20) && !memcmp(b->data + at + ENT, d, sizes[i]);
        at += ENT + ((sizes[i] + 1023) & ~1023u);
        free(d);
    }
    return ok;
}

static const unsigned int SIZES[] = {10, 1024, 70000, 0, 5000};
#define FILES ((int)(sizeof(SIZES) / sizeof(SIZES[0])))

static int saves_of(const char *card, long long *freeBytes)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    return mcfs_list_saves(card, list, MCFS_MAX_SAVES, freeBytes);
}

static void new_card(void)
{
    char fp[65], fp2[65], sig[65], sig2[65];
    long long freeBytes = 0;
    buffer_t b;
    int page = 0, saves = -1;
    CHECK_INT(mcfs_new_card("sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(t_size("sd/a.mcd"), CARD_BYTES);
    b = t_read("sd/a.mcd");
    CHECK_INT(mcfs_card_size(b.data, &page), CARD_BYTES);
    CHECK_INT(page, 512);
    buf_free(&b);
    CHECK_INT(saves_of("sd/a.mcd", &freeBytes), 0);
    CHECK(freeBytes > 7 * 1024 * 1024 && freeBytes < CARD_BYTES);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", fp, &saves), 0);
    CHECK_INT(saves, 0);
    CHECK_INT(mcfs_root_signature("sd/a.mcd", sig), 0);
    /* another new card is the same card: what tells two cards apart is what is saved to them */
    CHECK_INT(mcfs_new_card("sd/b.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_fingerprint("sd/b.mcd", fp2, NULL), 0);
    CHECK_INT(mcfs_root_signature("sd/b.mcd", sig2), 0);
    CHECK_STR(fp, fp2);
    CHECK_STR(sig, sig2);
    /* what isn't a card */
    t_text("sd/not.mcd", "this is no memory card");
    CHECK(mcfs_fingerprint("sd/not.mcd", fp, NULL) != 0);
    CHECK(saves_of("sd/not.mcd", NULL) < 0);
    CHECK(mcfs_fingerprint("sd/missing.mcd", fp, NULL) != 0);
}

static void save_in_and_out(void)
{
    char empty[65], with[65], again[65], sigEmpty[65], sigWith[65];
    long long freeEmpty = 0, freeWith = 0, bytes = 0;
    mcfs_psu_t info;
    buffer_t out = {0}, out2 = {0}, none = {0};
    int saves = 0, files = 0;
    make_psu("save.psu", SAVE, SIZES, FILES, 7);
    CHECK_INT(mcfs_psu_info("save.psu", &info, &none, &none), 0);
    CHECK_STR(info.folder, SAVE);
    CHECK_INT(info.files, FILES);
    CHECK_INT(info.bytes, 10 + 1024 + 70000 + 0 + 5000);

    CHECK_INT(mcfs_new_card("sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", empty, NULL), 0);
    CHECK_INT(mcfs_root_signature("sd/a.mcd", sigEmpty), 0);
    CHECK_INT(saves_of("sd/a.mcd", &freeEmpty), 0);

    CHECK_INT(mcfs_import_psu("save.psu", "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(saves_of("sd/a.mcd", &freeWith), 1);
    CHECK(freeWith < freeEmpty - 70000 && freeWith > freeEmpty - 200000);
    CHECK_INT(mcfs_save_info("sd/a.mcd", SAVE, &bytes, &files), MCFS_OK);
    CHECK_INT(files, FILES);
    CHECK_INT(bytes, info.bytes);
    CHECK_INT(t_size("sd/a.mcd"), CARD_BYTES);

    /* the card tells: its fingerprint and its root's signature are others now, and stay those while nothing changes */
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", with, &saves), 0);
    CHECK_INT(saves, 1);
    CHECK(strcmp(with, empty) != 0);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", again, NULL), 0);
    CHECK_STR(again, with);
    CHECK_INT(mcfs_root_signature("sd/a.mcd", sigWith), 0);
    CHECK(strcmp(sigWith, sigEmpty) != 0);

    /* the same save again: refused, and the card is left as it was */
    CHECK_INT(mcfs_import_psu("save.psu", "sd/a.mcd", NULL), MCFS_ERR_EXISTS);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", again, NULL), 0);
    CHECK_STR(again, with);

    /* out again as a .psu: every file as it went in; and that .psu, into another card and out of it, is the same */
    CHECK_INT(mcfs_export_psu("sd/a.mcd", SAVE, &out), MCFS_OK);
    CHECK(psu_holds(&out, SAVE, SIZES, FILES, 7));
    t_write("out.psu", out.data, out.len);
    CHECK_INT(mcfs_new_card("sd/b.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu("out.psu", "sd/b.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_export_psu("sd/b.mcd", SAVE, &out2), MCFS_OK);
    CHECK(out.len == out2.len && !memcmp(out.data, out2.data, out.len));
    CHECK_INT(mcfs_export_psu("sd/a.mcd", "NOT-THERE", &out2), MCFS_ERR_NOT_FOUND);
    buf_free(&out);
    buf_free(&out2);
}

static void save_from_card_to_card(void)
{
    char fpA[65], fpA2[65];
    long long freeEmpty = 0, freeNow = 0;
    buffer_t a = {0}, b = {0};
    make_psu("save.psu", SAVE, SIZES, FILES, 11);
    CHECK_INT(mcfs_new_card("sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_new_card("sd/b.mcd", NULL), MCFS_OK);
    CHECK_INT(saves_of("sd/b.mcd", &freeEmpty), 0);
    CHECK_INT(mcfs_import_psu("save.psu", "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", fpA, NULL), 0);

    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, "sd/b.mcd", NULL), MCFS_OK);
    CHECK_INT(saves_of("sd/b.mcd", NULL), 1);
    CHECK_INT(mcfs_export_psu("sd/a.mcd", SAVE, &a), MCFS_OK);
    CHECK_INT(mcfs_export_psu("sd/b.mcd", SAVE, &b), MCFS_OK);
    CHECK(psu_holds(&b, SAVE, SIZES, FILES, 11));
    CHECK(a.len == b.len && !memcmp(a.data, b.data, a.len));
    /* the card it came from is untouched */
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", fpA2, NULL), 0);
    CHECK_STR(fpA2, fpA);
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, "sd/b.mcd", NULL), MCFS_ERR_EXISTS);
    CHECK_INT(mcfs_copy_save("sd/a.mcd", "NOT-THERE", "sd/b.mcd", NULL), MCFS_ERR_NOT_FOUND);

    /* deleted: the card has no save and its room again, but for the cluster the root folder grew by to list it
     * (a folder never shrinks on a memory card) */
    CHECK_INT(mcfs_delete_save("sd/b.mcd", SAVE), MCFS_OK);
    CHECK_INT(saves_of("sd/b.mcd", &freeNow), 0);
    CHECK_INT(freeNow, freeEmpty - 1024);
    CHECK_INT(mcfs_delete_save("sd/b.mcd", SAVE), MCFS_ERR_NOT_FOUND);
    /* it takes the save once more, in the place the other left: deleted again, no room was lost */
    CHECK_INT(mcfs_copy_save("sd/a.mcd", SAVE, "sd/b.mcd", NULL), MCFS_OK);
    CHECK_INT(saves_of("sd/b.mcd", NULL), 1);
    CHECK_INT(mcfs_delete_save("sd/b.mcd", SAVE), MCFS_OK);
    CHECK_INT(saves_of("sd/b.mcd", &freeNow), 0);
    CHECK_INT(freeNow, freeEmpty - 1024);
    buf_free(&a);
    buf_free(&b);
}

static int stopAt;   /* the phase a save's way in is given up at (-1 = it isn't) */
static int phases;   /* the phases that were told */

static int on_step(int phase, long long done, long long total)
{
    phases |= 1 << phase;
    return phase == stopAt && done * 2 >= total;
}

static void save_given_up_and_card_full(void)
{
    static const unsigned int big[] = {3 * 1024 * 1024};
    char before[65], after[65];
    long long freeBefore = 0, freeAfter = 0;
    make_psu("save.psu", SAVE, SIZES, FILES, 3);
    CHECK_INT(mcfs_new_card("sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(saves_of("sd/a.mcd", &freeBefore), 0);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", before, NULL), 0);

    /* given up while it is written: the card has no save, and no room taken */
    phases = 0;
    stopAt = MCFS_STEP_WRITE;
    CHECK_INT(mcfs_import_psu("save.psu", "sd/a.mcd", on_step), MCFS_ERR_CANCELLED);
    CHECK_INT(saves_of("sd/a.mcd", &freeAfter), 0);
    CHECK_INT(freeAfter, freeBefore);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", after, NULL), 0);
    CHECK_STR(after, before);
    /* and nothing stops it otherwise: read, written, read back */
    phases = 0;
    stopAt = -1;
    CHECK_INT(mcfs_import_psu("save.psu", "sd/a.mcd", on_step), MCFS_OK);
    CHECK_INT(phases, (1 << MCFS_STEP_READ) | (1 << MCFS_STEP_WRITE) | (1 << MCFS_STEP_CHECK));

    /* a card with no room for a save says so and keeps what it had */
    make_psu("big1.psu", "BASLUS-99999BIG1", big, 1, 21);
    make_psu("big2.psu", "BASLUS-99999BIG2", big, 1, 22);
    make_psu("big3.psu", "BASLUS-99999BIG3", big, 1, 23);
    CHECK_INT(mcfs_import_psu("big1.psu", "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu("big2.psu", "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(saves_of("sd/a.mcd", &freeBefore), 3);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", before, NULL), 0);
    CHECK_INT(mcfs_import_psu("big3.psu", "sd/a.mcd", NULL), MCFS_ERR_FULL);
    CHECK_INT(saves_of("sd/a.mcd", &freeAfter), 3);
    CHECK_INT(freeAfter, freeBefore);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", after, NULL), 0);
    CHECK_STR(after, before);
}

static void psu_that_is_not_one(void)
{
    buffer_t whole;
    char before[65], after[65];
    CHECK_INT(mcfs_new_card("sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", before, NULL), 0);
    t_text("short.psu", "not a save");
    CHECK_INT(mcfs_import_psu("short.psu", "sd/a.mcd", NULL), MCFS_ERR_BAD);
    /* cut off in the middle of a file */
    make_psu("save.psu", SAVE, SIZES, FILES, 5);
    whole = t_read("save.psu");
    t_write("cut.psu", whole.data, 3 * ENT + ENT + 1024 + ENT + 1024 + ENT + 30000);
    buf_free(&whole);
    CHECK_INT(mcfs_import_psu("cut.psu", "sd/a.mcd", NULL), MCFS_ERR_BAD);
    CHECK_INT(mcfs_import_psu("missing.psu", "sd/a.mcd", NULL), MCFS_ERR_IO);
    CHECK_INT(mcfs_fingerprint("sd/a.mcd", after, NULL), 0);
    CHECK_STR(after, before);
}

void suite_mcfs(void)
{
    RUN(new_card);
    RUN(save_in_and_out);
    RUN(save_from_card_to_card);
    RUN(save_given_up_and_card_full);
    RUN(psu_that_is_not_one);
}
