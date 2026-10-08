/* SD2Cloud's regression tests -- a card in a file (restore.c, stream.c): installed as a card of the microSD in every
 * way there is (a new card or over one that is smaller, the same size or bigger; from memory or, when it doesn't
 * fit there, straight from its file; a .mcd, a .ps2 with its ECC bytes, a .zip), with what can go wrong on the way
 * (a file that stops reading, giving up), and a card's way out to a .zip and back. */
#include <fcntl.h>
#include <unistd.h>
#include "t.h"

#define CARD_BYTES (8 * 1024 * 1024)
#define SAVE "BASLUS-99999TESTS"
#define DEST "sd/MemoryCards/PS2/Card1/Card1-1.mcd"

/* a card with a save in it, as the file to install; a copy of it is kept to put the file back from */
static void make_source(const char *path)
{
    static const unsigned int sizes[] = {900, 250000, 4096};
    buffer_t b;
    t_mkdir("sd/MemoryCards/PS2/Card1");
    make_psu("save.psu", SAVE, sizes, 3, 31);
    CHECK_INT(mcfs_new_card(path, NULL), MCFS_OK);
    CHECK_INT(mcfs_import_psu("save.psu", path, NULL), MCFS_OK);
    b = t_read(path);
    t_write("source.keep", b.data, b.len);
    buf_free(&b);
}

static card_t dest_card(void)
{
    card_t c;
    memset(&c, 0, sizeof(c));
    snprintf(c.folder, sizeof(c.folder), "Card1");
    snprintf(c.base, sizeof(c.base), "Card1-1");
    snprintf(c.id, sizeof(c.id), "Card1/Card1-1");
    snprintf(c.path, sizeof(c.path), DEST);
    c.type = TYPE_NORMAL;
    c.channel = 1;
    return c;
}

/* what the progress is told, and what it is to answer */
static int seen;                 /* the phases told, one bit each */
static int stopAt = -1;          /* the phase that is given up halfway (-1 = none) */
static const char *cutFile;      /* the file that is cut short halfway through the writing: it stops reading */
static int lostTimes, lostGiveUp;

static void reset(void)
{
    seen = lostTimes = lostGiveUp = 0;
    stopAt = -1;
    cutFile = NULL;
}

static int on_progress(int phase, long long done, long long total)
{
    seen |= 1 << phase;
    if (phase == RESTORE_LOST) {   /* the file didn't read: put back whole (as a drive plugged in again), or given up */
        buffer_t b;
        lostTimes++;
        if (lostGiveUp)
            return 1;
        b = t_read("source.keep");
        t_write(cutFile, b.data, b.len);
        buf_free(&b);
        cutFile = NULL;
        return 0;
    }
    if (phase == RESTORE_WRITE && cutFile && !lostTimes && done * 2 >= total)
        CHECK_INT(truncate(cutFile, (off_t)done), 0);
    return phase == stopAt && done * 2 >= total;
}

static int install(const char *file, card_t *to)
{
    card_file_t f;
    int r = card_file_open(&f, file, on_progress);
    if (r != 0)
        return 100 + r;
    r = card_file_install(&f, to, on_progress);
    card_file_close(&f);
    return r;
}

#define BIT(phase) (1 << (phase))

static void from_memory(void)
{
    card_t to = dest_card();
    unsigned char *junk = malloc(2 * CARD_BYTES);
    make_source("card.mcd");
    /* a new card */
    reset();
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    CHECK_INT(to.size, CARD_BYTES);
    CHECK_INT(seen, BIT(RESTORE_DOWNLOAD) | BIT(RESTORE_WRITE) | BIT(RESTORE_VERIFY));
    /* over a card of the same size, a bigger one and a smaller one */
    t_fill(junk, 2 * CARD_BYTES, 99);
    t_write(DEST, junk, CARD_BYTES);
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    t_write(DEST, junk, 2 * CARD_BYTES);
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    t_write(DEST, junk, 5000);
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    /* given up while the file is read: the card it would replace is as it was */
    t_write(DEST, junk, CARD_BYTES);
    t_write("before.mcd", junk, CARD_BYTES);
    reset();
    stopAt = RESTORE_DOWNLOAD;
    CHECK_INT(install("card.mcd", &to), -2);
    CHECK(t_same(DEST, "before.mcd"));
    free(junk);
}

static void straight_from_the_file(void)
{
    card_t to = dest_card();
    unsigned char *junk = malloc(2 * CARD_BYTES);
    make_source("card.mcd");
    t_fill(junk, 2 * CARD_BYTES, 77);
    hostMallocLimit = 1024 * 1024;   /* a card doesn't fit in memory */
    /* a new card: no reading first, and what is written is read back */
    reset();
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    CHECK_INT(seen, BIT(RESTORE_WRITE) | BIT(RESTORE_VERIFY));
    /* over the same size (written over where it is), a bigger one (cut first) and a smaller one */
    t_write(DEST, junk, CARD_BYTES);
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    t_write(DEST, junk, 2 * CARD_BYTES);
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    t_write(DEST, junk, 5000);
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    /* a file on another device than the card (it stays open from start to end) */
    t_mkdir("usb:");
    {
        buffer_t b = t_read("card.mcd");
        t_write("usb:/card.mcd", b.data, b.len);
        buf_free(&b);
    }
    t_write(DEST, junk, CARD_BYTES);
    CHECK_INT(install("usb:/card.mcd", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    free(junk);
}

static void a_file_that_stops_reading(void)
{
    card_t to = dest_card();
    unsigned char *junk = malloc(CARD_BYTES);
    buffer_t b;
    make_source("card.mcd");
    t_mkdir("usb:");
    b = t_read("card.mcd");
    t_write("usb:/card.mcd", b.data, b.len);
    buf_free(&b);
    t_fill(junk, CARD_BYTES, 55);
    hostMallocLimit = 1024 * 1024;

    /* tried again once the file is whole again: the card comes out right (both ways the file is read) */
    t_write(DEST, junk, CARD_BYTES);
    reset();
    cutFile = "card.mcd";
    CHECK_INT(install("card.mcd", &to), 0);
    CHECK_INT(lostTimes, 1);
    CHECK(t_same(DEST, "source.keep"));
    t_write(DEST, junk, CARD_BYTES);
    reset();
    cutFile = "usb:/card.mcd";
    CHECK_INT(install("usb:/card.mcd", &to), 0);
    CHECK_INT(lostTimes, 1);
    CHECK(t_same(DEST, "source.keep"));

    /* given up: it failed, it says why, and the card that was half written over is left empty */
    t_write(DEST, junk, CARD_BYTES);
    reset();
    cutFile = "card.mcd";
    lostGiveUp = 1;
    CHECK_INT(install("card.mcd", &to), -1);
    CHECK_INT(lostTimes, 1);
    CHECK_STR(googleError, T(T_ERR_READ_FILE));
    CHECK_INT(t_size(DEST), 0);
    b = t_read("source.keep");
    t_write("card.mcd", b.data, b.len);
    buf_free(&b);

    /* a new card given up on while it is written, and while it is read back: -2, for the caller to delete it */
    unlink(DEST);
    reset();
    stopAt = RESTORE_WRITE;
    CHECK_INT(install("card.mcd", &to), -2);
    CHECK_INT(t_size(DEST), 0);
    unlink(DEST);
    reset();
    stopAt = RESTORE_VERIFY;
    CHECK_INT(install("card.mcd", &to), -2);
    free(junk);
}

static void with_ecc_and_what_is_no_card(void)
{
    card_t to = dest_card();
    card_file_t f;
    buffer_t b, ps2 = {0};
    static const unsigned char ecc[16] = {0xEC, 0xEC, 0xEC};
    size_t at;
    make_source("card.mcd");
    /* a .ps2: 16 bytes of ECC after each 512, whatever the file is called */
    b = t_read("card.mcd");
    for (at = 0; at < b.len; at += 512) {
        buf_append(&ps2, b.data + at, 512);
        buf_append(&ps2, ecc, 16);
    }
    t_write("card.ps2", ps2.data, ps2.len);
    buf_free(&ps2);
    CHECK_INT(card_file_open(&f, "card.ps2", NULL), 0);
    CHECK_INT(f.ecc, 1);
    CHECK_INT(f.size, CARD_BYTES);
    card_file_close(&f);
    reset();
    CHECK_INT(install("card.ps2", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    hostMallocLimit = 1024 * 1024;
    unlink(DEST);
    CHECK_INT(install("card.ps2", &to), 0);
    CHECK(t_same(DEST, "card.mcd"));
    hostMallocLimit = 0;

    /* what isn't a memory card isn't opened: some other file, a card cut short, a card with bytes too many */
    t_text("text.mcd", "this is no memory card");
    CHECK(card_file_open(&f, "text.mcd", NULL) != 0);
    t_write("short.mcd", b.data, b.len - 512);
    CHECK(card_file_open(&f, "short.mcd", NULL) != 0);
    t_write("long.mcd", b.data, b.len);
    {
        FILE *more = fopen("long.mcd", "ab");
        fwrite(ecc, 1, sizeof(ecc), more);
        fclose(more);
    }
    CHECK(card_file_open(&f, "long.mcd", NULL) != 0);
    CHECK(card_file_open(&f, "missing.mcd", NULL) != 0);
    buf_free(&b);
}

static void out_to_a_zip_and_back(void)
{
    card_t from = dest_card(), to = dest_card();
    card_file_t f;
    buffer_t b;
    int ps2;
    make_source(DEST);
    from.size = CARD_BYTES;
    snprintf(to.path, sizeof(to.path), "sd/MemoryCards/PS2/Card1/Card1-2.mcd");
    for (ps2 = 0; ps2 < 2; ps2++) {
        const char *zip = ps2 ? "card-ps2.zip" : "card-mcd.zip";
        CHECK_INT(card_export(&from, zip, ps2, NULL), 0);
        CHECK(t_size(zip) > 1000 && t_size(zip) < CARD_BYTES / 4);   /* a card is mostly empty */
        CHECK_INT(card_file_open(&f, zip, NULL), 0);
        CHECK_INT(f.zip, 1);
        CHECK_INT(f.ecc, ps2);
        CHECK_INT(f.size, CARD_BYTES);
        CHECK(f.image != NULL);
        unlink(to.path);
        CHECK_INT(card_file_install(&f, &to, on_progress), 0);
        card_file_close(&f);
        CHECK(t_same(to.path, DEST));
        /* (kept beside the folder the tests empty, for check_zip.py to open with another program's zip reader) */
        b = t_read(zip);
        t_write(ps2 ? "../exported-ps2.zip" : "../exported-mcd.zip", b.data, b.len);
        buf_free(&b);
    }
    /* a .zip that is damaged isn't opened */
    b = t_read("card-mcd.zip");
    b.data[b.len / 2] ^= 0x55;
    t_write("damaged.zip", b.data, b.len);
    buf_free(&b);
    CHECK(card_file_open(&f, "damaged.zip", NULL) != 0);
}

void suite_card_file(void)
{
    RUN(from_memory);
    RUN(straight_from_the_file);
    RUN(a_file_that_stops_reading);
    RUN(with_ecc_and_what_is_no_card);
    RUN(out_to_a_zip_and_back);
}
