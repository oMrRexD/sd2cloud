/* SD2Cloud's regression tests -- the list of the microSD's cards (cards.c): which files are cards, of which kind,
 * and what the settings and the state make of each. */
#include "t.h"

static void put_card(const char *folder, const char *file)
{
    char p[300];
    snprintf(p, sizeof(p), "sd/MemoryCards/PS2/%s", folder);
    t_mkdir(p);
    snprintf(p, sizeof(p), "sd/MemoryCards/PS2/%s/%s", folder, file);
    if (mcfs_new_card(p, NULL) != MCFS_OK) {
        fprintf(stderr, "the tests can't make %s\n", p);
        exit(2);
    }
}

static card_t *card(const char *id)
{
    int i;
    for (i = 0; i < nCards; i++)
        if (!strcmp(cards[i].id, id))
            return &cards[i];
    return NULL;
}

static void no_progress(int i, int n, const card_t *c)
{
    (void)i;
    (void)n;
    (void)c;
}

static void cards_are_listed(void)
{
    card_t *c;
    char title[64];
    put_card("Card1", "Card1-1.mcd");
    put_card("Card1", "Card1-2.mcd");
    put_card("Card2", "Card2-1.mcd");
    put_card("SLUS-21065", "SLUS-21065-1.mcd");
    put_card("BOOT", "BootCard-1.mcd");
    put_card("MyStuff", "MyStuff-1.mcd");
    /* what is in those folders and is no card */
    t_text("sd/MemoryCards/PS2/Card1/notes.txt", "not a card");
    t_text("sd/MemoryCards/PS2/Card1/Card1-3.bak", "not a card either");
    t_mkdir("sd/MemoryCards/PS2/Empty");
    config_read();
    state_read();

    CHECK_INT(cards_scan(), 6);
    CHECK_INT(nCards, 6);
    c = card("Card1/Card1-2");
    CHECK(c != NULL);
    if (c) {
        CHECK_INT(c->type, TYPE_NORMAL);
        CHECK_INT(c->channel, 2);
        CHECK_STR(c->folder, "Card1");
        CHECK_STR(c->base, "Card1-2");
        CHECK_STR(c->path, "sd/MemoryCards/PS2/Card1/Card1-2.mcd");
        CHECK_INT(c->size, 8 * 1024 * 1024);
        CHECK_INT(c->included, 1);
    }
    c = card("SLUS-21065/SLUS-21065-1");
    CHECK(c != NULL);
    if (c) {
        CHECK_INT(c->type, TYPE_GAMEID);
        CHECK_STR(c->game, "A Game For The Tests");
        CHECK_INT(c->included, 1);
    }
    c = card("MyStuff/MyStuff-1");
    CHECK(c != NULL && c->type == TYPE_NAMED && c->included);
    c = card("BOOT/BootCard-1");   /* the boot cards are left out of the sync unless the settings ask for them */
    CHECK(c != NULL && c->type == TYPE_BOOT && !c->included);

    /* a card seen for the first time is new, and its index was read */
    cards_check(no_progress);
    c = card("Card1/Card1-1");
    CHECK(c != NULL && c->status == ST_NEW && strlen(c->fingerprint) == 64);

    CHECK(is_game_id("SLUS-21065"));
    CHECK(!is_game_id("Card1"));
    CHECK(!is_game_id("MyStuff"));
    CHECK_INT(game_title("SLES-50000", title, sizeof(title)), 1);
    CHECK_STR(title, "Another Game");
    CHECK_INT(game_title("SLUS-00000", title, sizeof(title)), 0);
}

static void settings_choose_the_cards(void)
{
    card_t *c;
    put_card("Card1", "Card1-1.mcd");
    put_card("Card2", "Card2-1.mcd");
    put_card("BOOT", "BootCard-1.mcd");
    t_text("sd/SD2Cloud/sd2cloud.ini", "[cards]\ntypes = normal, boot\nexclude = Card2/Card2-1\n");
    config_read();
    state_read();
    CHECK_INT(cards_scan(), 3);
    c = card("Card1/Card1-1");
    CHECK(c != NULL && c->included);
    c = card("Card2/Card2-1");
    CHECK(c != NULL && !c->included);
    c = card("BOOT/BootCard-1");
    CHECK(c != NULL && c->included);

    /* a card whose fingerprint is the one of its last backup is up to date; one that changed since is not */
    cards_check(no_progress);
    c = card("Card1/Card1-1");
    CHECK(c != NULL);
    if (c) {
        card_state_t *e = state_card(c->id, 1);
        snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", c->fingerprint);
        snprintf(e->sha, sizeof(e->sha), "%s", "anything");
        e->version = MCFS_VERSION;
        cards_recheck(c);
        CHECK_INT(c->status, ST_UP_TO_DATE);
        snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", "another");
        cards_recheck(c);
        CHECK_INT(c->status, ST_CHANGED);
    }

    /* how many channels a folder has: 8, until its .ini says otherwise */
    CHECK_INT(max_channels("Card1"), 8);
    CHECK_INT(max_channels_set("Card1", 9), 0);
    CHECK_INT(max_channels("Card1"), 9);
}

/* a folder Game2Folder.ini gives to several games is a game card, named after those games */
static void group_folders_are_named(void)
{
    card_t *c;
    put_card("MCCG-10001", "MCCG-10001-1.mcd");
    put_card("MCCG-10001", "MCCG-10001-2.mcd");
    put_card("MCCG-10002", "MCCG-10002-1.mcd");
    put_card("MCCG-10003", "MCCG-10003-1.mcd");
    put_card("MCCG-10004", "MCCG-10004-1.mcd");
    put_card("MCCG-10005", "MCCG-10005-1.mcd");
    put_card("MCCG-10006", "MCCG-10006-1.mcd");
    put_card("Fighters", "Fighters-1.mcd");
    put_card("SLUS-21065", "SLUS-21065-1.mcd");
    t_mkdir("sd/.sd2psx");
    t_text("sd/.sd2psx/Game2Folder.ini",
           "# a list of groups\n[PS1]\nSLUS-30011=MCCG-10001\n[PS2]\n"
           "SLES-30001=MCCG-10001\nSLUS-30001=MCCG-10001\nSLUS-30002=MCCG-10001\nSLUS-30003=MCCG-10001\n"
           "SLES-30011=MCCG-10002\nSLES-30012=MCCG-10002\n"
           "SLUS-30021=MCCG-10003\nSLUS-30022=MCCG-10003\nSLUS-30023=MCCG-10003\nSLUS-30024=MCCG-10003\n"
           "SLUS-39998=MCCG-10004\nSLUS-39999=MCCG-10004\n"
           "SLPM-30031=MCCG-10005\nSLES-30032=MCCG-10005\n"
           "SLUS-30041=MCCG-10006\nSLUS-30042=MCCG-10006\n"
           "SLES-30012=Fighters\n");
    config_read();
    state_read();
    CHECK_INT(cards_scan(), 9);
    /* what its games' names all start with, when that is more than a word */
    c = card("MCCG-10001/MCCG-10001-2");
    CHECK(c != NULL && c->type == TYPE_GAMEID);
    if (c)
        CHECK_STR(c->game, "Racing Series");
    /* the names themselves, when that is short */
    c = card("MCCG-10002/MCCG-10002-1");
    CHECK(c != NULL);
    if (c)
        CHECK_STR(c->game, "Zombie Zone, Zombie Hunters");
    /* as many of them as fit, when they have nothing in common (and an edition isn't part of a name) */
    c = card("MCCG-10003/MCCG-10003-1");
    CHECK(c != NULL);
    if (c)
        CHECK_STR(c->game, "Burnout 3 - Takedown, NFL Street 2, Black...");
    /* games the list doesn't know: no name */
    c = card("MCCG-10004/MCCG-10004-1");
    CHECK(c != NULL && c->type == TYPE_GAMEID);
    if (c)
        CHECK_STR(c->game, "");
    /* a game of America or of Europe before one of Japan, whatever the order of the list */
    c = card("MCCG-10005/MCCG-10005-1");
    CHECK(c != NULL);
    if (c)
        CHECK_STR(c->game, "Zombie Zone Deluxe, Oneechanbara");
    /* the one word they start with, when the names are too long to say: not the word that only leads to the rest */
    c = card("MCCG-10006/MCCG-10006-1");
    CHECK(c != NULL);
    if (c)
        CHECK_STR(c->game, "Avatar");
    /* a folder with a name of its own that a game is given is a game card too, with that game's name */
    c = card("Fighters/Fighters-1");
    CHECK(c != NULL && c->type == TYPE_GAMEID);
    if (c)
        CHECK_STR(c->game, "Zombie Hunters");
    /* and a game's own folder keeps the game's name */
    c = card("SLUS-21065/SLUS-21065-1");
    CHECK(c != NULL);
    if (c)
        CHECK_STR(c->game, "A Game For The Tests");
}

void suite_cards(void)
{
    RUN(cards_are_listed);
    RUN(group_folders_are_named);
    RUN(settings_choose_the_cards);
}
