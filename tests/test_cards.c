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

void suite_cards(void)
{
    RUN(cards_are_listed);
    RUN(settings_choose_the_cards);
}
