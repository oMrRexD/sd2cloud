/* SD2Cloud's regression tests -- the memory card's file system (mcfs.c): a new card, a save's way in from a .psu (or
 * a folder) and out to one, from card to card, and off a card; what a card's fingerprint and its root's signature tell apart.
 * The cards and the saves are made here: nothing in them comes from a game. */
#include <utime.h>
#include "t.h"

#define ENT 512
#define CARD_BYTES (8 * 1024 * 1024)
#define SAVE "BASLUS-99999TESTS"

static unsigned int get32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

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
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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
    CHECK_INT(mcfs_new_card("sd/b.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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

/* a new card of every size a memory card can be: what the card says of itself, its room, a save in and out of it */
static void new_card_sizes(void)
{
    static const unsigned int one[] = {3000};
    long long mb, freeBytes = 0;
    char fp[65];
    buffer_t b;
    int page = 0;
    make_psu("save.psu", SAVE, one, 1, 3);
    for (mb = 1; mb <= 128; mb *= 2) {
        long long size = mb << 20;
        CHECK_INT(mcfs_new_card("sd/s.mcd", size, NULL), MCFS_OK);
        CHECK_INT(t_size("sd/s.mcd"), size);
        b = t_read("sd/s.mcd");
        CHECK_INT(mcfs_card_size(b.data, &page), size);
        /* the superblock as the PS2's own format writes it: clusters, where the allocatable area starts and ends,
         * the backup blocks, the indirect FAT's clusters, the root folder's first entry */
        {
            unsigned int clusters = (unsigned int)(size / 1024), fatLen = (clusters * 4 + 1023) / 1024,
                         ifc = (fatLen * 4 + 1023) / 1024, alloc = 16 + ifc + fatLen, b2 = clusters / 8 - 2;
            CHECK_INT(get32(b.data + 0x30), clusters);
            CHECK_INT(get32(b.data + 0x34), alloc);
            CHECK_INT(get32(b.data + 0x38), b2 * 8 - alloc);
            CHECK_INT(get32(b.data + 0x40), b2 + 1);
            CHECK_INT(get32(b.data + 0x44), b2);
            CHECK_INT(get32(b.data + 0x50 + (ifc - 1) * 4), 16 + ifc - 1);
            CHECK_INT(b.data[(size_t)alloc * 1024 + 64], '.');
        }
        buf_free(&b);
        CHECK_INT(saves_of("sd/s.mcd", &freeBytes), 0);
        CHECK(freeBytes >= mcfs_card_room(size));
        CHECK_INT(mcfs_import_psu("save.psu", "sd/s.mcd", NULL), MCFS_OK);
        CHECK_INT(saves_of("sd/s.mcd", NULL), 1);
        CHECK_INT(mcfs_fingerprint("sd/s.mcd", fp, NULL), 0);
        CHECK_INT(mcfs_delete_save("sd/s.mcd", SAVE), MCFS_OK);
        CHECK_INT(saves_of("sd/s.mcd", NULL), 0);
    }
    /* sizes no card is */
    CHECK_INT(mcfs_new_card("sd/x.mcd", 3LL << 20, NULL), MCFS_ERR_BAD);
    CHECK_INT(mcfs_new_card("sd/x.mcd", 256LL << 20, NULL), MCFS_ERR_BAD);
    CHECK_INT(mcfs_new_card("sd/x.mcd", 512 * 1024, NULL), MCFS_ERR_BAD);
    CHECK_INT(t_size("sd/x.mcd"), -1);
    /* the room the PS2 tells of: 8000 KB on an 8 MB card; a 1 MB card has its whole allocatable area but the root */
    CHECK_INT(mcfs_card_room(MCFS_CARD_8MB), 8000 * 1024);
    CHECK_INT(mcfs_card_room(1 << 20), 986 * 1024);
    CHECK_INT(mcfs_card_room(2 << 20), 2000 * 1024);
    CHECK_INT(mcfs_card_room(3 << 20), 0);
    /* a save of 3 files and 100 KB: 3 clusters of entries, 100 of data, up to 3 lost to rounding, 1 in the root */
    CHECK_INT(mcfs_save_room(100 * 1024, 3), 107 * 1024);
    CHECK_INT(mcfs_save_room(0, 0), 2 * 1024);
    /* the size for a new card: twice what goes into it, at least */
    CHECK_INT(mcfs_recommended_mb(107 * 1024, 1, 64), 1);
    CHECK_INT(mcfs_recommended_mb(600 * 1024, 1, 64), 2);
    CHECK_INT(mcfs_recommended_mb(600 * 1024, 8, 128), 8);
    CHECK_INT(mcfs_recommended_mb(5000LL * 1024, 1, 64), 16);
    CHECK_INT(mcfs_recommended_mb(40000LL * 1024, 1, 64), 64);   /* (only fits) */
    CHECK_INT(mcfs_recommended_mb(70000LL * 1024, 1, 64), 0);
    CHECK_INT(mcfs_recommended_mb(70000LL * 1024, 8, 128), 128);
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

    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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
    CHECK_INT(mcfs_new_card("sd/b.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    CHECK_INT(mcfs_new_card("sd/b.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
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

/* a save copied as a folder, as a file manager copies one off a card: taken as its .psu would be, dated as the newest
 * of its files, what a computer leaves in it left out; and folders that aren't one */
static void save_from_a_folder(void)
{
    char path[80], before[65], after[65];
    mcfs_psu_t info;
    buffer_t out = {0}, none = {0};
    struct utimbuf u;
    int i;
    t_mkdir("usb/" SAVE);
    for (i = 0; i < FILES; i++) {
        unsigned char *d = malloc(SIZES[i] + 1);
        t_fill(d, SIZES[i], 9 + i);
        snprintf(path, sizeof(path), "usb/" SAVE "/file%d.bin", i);
        t_write(path, d, SIZES[i]);
        free(d);
        u.actime = u.modtime = 1700000000 + i * 3600;   /* 2023-11-14 22:13:20 UTC, an hour later each */
        CHECK_INT(utime(path, &u), 0);
    }
    t_text("usb/" SAVE "/Thumbs.db", "a computer's");
    t_text("usb/" SAVE "/._file0.bin", "a computer's");
    CHECK_INT(mcfs_psu_info("usb/" SAVE "/", &info, &none, &none), 0);
    CHECK_STR(info.folder, SAVE);
    CHECK_INT(info.files, FILES);
    CHECK_INT(info.bytes, 10 + 1024 + 70000 + 0 + 5000);
    CHECK_INT(info.when, (2023ULL << 40) | (11ULL << 32) | (15 << 24) | (2 << 16) | (13 << 8) | 20);   /* file4.bin's */
    CHECK_INT(mcfs_new_card("sd/a.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu("usb/" SAVE "/", "sd/a.mcd", NULL), MCFS_OK);
    CHECK_INT(mcfs_export_psu("sd/a.mcd", SAVE, &out), MCFS_OK);
    CHECK(psu_holds(&out, SAVE, SIZES, FILES, 9));
    buf_free(&out);
    /* files with no date that is one (1970, before the cards' 1980): dated as the clock says now (host.c) */
    u.actime = u.modtime = 0;
    t_mkdir("usb/BASLUS-99999OLD");
    t_text("usb/BASLUS-99999OLD/icon.sys", "x");
    CHECK_INT(utime("usb/BASLUS-99999OLD/icon.sys", &u), 0);
    CHECK_INT(mcfs_psu_info("usb/BASLUS-99999OLD/", &info, &none, &none), 0);
    CHECK_INT(info.when, (2026ULL << 40) | (1ULL << 32) | (2 << 24) | (3 << 16) | (4 << 8) | 5);

    /* a folder inside it; a name no card can hold; no files; no folder at all: the card is left as it was */
    CHECK_INT(mcfs_new_card("sd/b.mcd", MCFS_CARD_8MB, NULL), MCFS_OK);
    CHECK_INT(mcfs_fingerprint("sd/b.mcd", before, NULL), 0);
    t_mkdir("usb/" SAVE "/inner");
    CHECK_INT(mcfs_import_psu("usb/" SAVE "/", "sd/b.mcd", NULL), MCFS_ERR_BAD);
    t_mkdir("usb/BASLUS-99999A-NAME-LONGER-THAN-32");
    t_text("usb/BASLUS-99999A-NAME-LONGER-THAN-32/icon.sys", "x");
    CHECK_INT(mcfs_import_psu("usb/BASLUS-99999A-NAME-LONGER-THAN-32/", "sd/b.mcd", NULL), MCFS_ERR_BAD);
    t_mkdir("usb/EMPTY");
    CHECK_INT(mcfs_import_psu("usb/EMPTY/", "sd/b.mcd", NULL), MCFS_ERR_BAD);
    CHECK_INT(mcfs_import_psu("usb/MISSING/", "sd/b.mcd", NULL), MCFS_ERR_IO);
    CHECK_INT(mcfs_fingerprint("sd/b.mcd", after, NULL), 0);
    CHECK_STR(after, before);
}

void suite_mcfs(void)
{
    RUN(new_card);
    RUN(new_card_sizes);
    RUN(save_in_and_out);
    RUN(save_from_card_to_card);
    RUN(save_given_up_and_card_full);
    RUN(psu_that_is_not_one);
    RUN(save_from_a_folder);
}
