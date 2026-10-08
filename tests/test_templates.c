/* SD2Cloud's regression tests -- templates (templates.c): sets of saves kept on the microSD as .psu files, made
 * from the saves of cards and put into other cards. */
#include "t.h"

#define CARD_A "sd/MemoryCards/PS2/Card1/Card1-1.mcd"
#define CARD_B "sd/MemoryCards/PS2/SLUS-21065/SLUS-21065-1.mcd"
#define NET    "BWNETCNF"
#define GAME   "BASLUS-21065SAVE"
#define ROOT   "sd/SD2Cloud/templates/"

static const unsigned int SIZES[2] = {700, 3000};

static void new_card(const char *path)
{
    char dir[200];
    snprintf(dir, sizeof(dir), "%.*s", (int)(strrchr(path, '/') - path), path);
    t_mkdir(dir);
    if (mcfs_new_card(path, NULL) != MCFS_OK) {
        fprintf(stderr, "the tests can't make %s\n", path);
        exit(2);
    }
}

/* a save of that folder into a card: its files hold what the seed says */
static void put_save(const char *card, const char *folder, unsigned int seed)
{
    make_psu("in.psu", folder, SIZES, 2, seed);
    CHECK_INT(mcfs_import_psu("in.psu", card, NULL), MCFS_OK);
}

/* is the card's save of that folder, as a .psu, the same as that file? */
static int save_is(const char *card, const char *folder, const char *psu)
{
    buffer_t b = {0};
    int same = 0;
    if (mcfs_export_psu(card, folder, &b) == MCFS_OK) {
        t_write("out.psu", b.data, b.len);
        same = t_same("out.psu", psu);
    }
    buf_free(&b);
    return same;
}

static int has_save(const char *card, const char *folder)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    int n = mcfs_list_saves(card, list, MCFS_MAX_SAVES, NULL), i;
    for (i = 0; i < n; i++)
        if (!strcmp(list[i].folder, folder))
            return 1;
    return 0;
}

static void templates_are_made(void)
{
    template_t *t;
    new_card(CARD_A);
    put_save(CARD_A, NET, 1);
    put_save(CARD_A, GAME, 2);
    CHECK_INT(templates_scan(), 0);

    /* a name is what a folder can be called, and no other template's */
    CHECK_INT(template_name_check("Online", NULL), TPL_OK);
    CHECK_INT(template_name_check("", NULL), TPL_ERR_NAME);
    CHECK_INT(template_name_check("a/b", NULL), TPL_ERR_NAME);
    CHECK_INT(template_name_check(" x", NULL), TPL_ERR_NAME);
    CHECK_INT(template_name_check("a name that is longer than it may be", NULL), TPL_ERR_NAME);
    t = template_new("Online");
    CHECK(t != NULL);
    if (!t)
        return;
    CHECK_INT(nTemplates, 1);
    CHECK_INT(template_name_check("online", NULL), TPL_ERR_TAKEN);
    CHECK_INT(template_name_check("online", t), TPL_OK);
    CHECK(template_new("ONLINE") == NULL);

    /* a save goes in as a .psu, the same one the card would export */
    CHECK_INT(template_add(t, CARD_A, NET), MCFS_OK);
    CHECK_INT(t->n, 1);
    CHECK_STR(t->saves[0].folder, NET);
    CHECK_STR(t->saves[0].file, NET ".psu");
    CHECK_INT(t->saves[0].bytes, 3700);
    CHECK(save_is(CARD_A, NET, ROOT "Online/" NET ".psu"));
    CHECK_INT(template_add(t, CARD_A, "BASLUS-00000NONE"), MCFS_ERR_NOT_FOUND);
    CHECK_INT(t->n, 1);
    CHECK_INT(template_add(t, CARD_A, GAME), MCFS_OK);
    CHECK_INT(t->n, 2);
    CHECK_INT(t->bytes, 7400);
    CHECK_INT(template_find_save(t, GAME), 1);
    CHECK_INT(template_find_save(t, "BASLUS-00000NONE"), -1);

    /* listed again from the microSD, it is what it was; a .psu put there by hand is part of it, anything else isn't */
    make_psu(ROOT "Online/by hand.psu", "BASLES-50000HAND", SIZES, 2, 9);
    t_text(ROOT "Online/notes.txt", "not a save");
    t_text(ROOT "Online/broken.psu", "not a save either");
    t_text(ROOT "a file.txt", "not a template");
    CHECK_INT(templates_scan(), 1);
    t = template_find("online");
    CHECK(t != NULL && t->n == 3);
    if (!t || t->n != 3)
        return;
    CHECK(template_find_save(t, NET) >= 0 && template_find_save(t, GAME) >= 0 && template_find_save(t, "BASLES-50000HAND") >= 0);
    CHECK_INT(t->bytes, 3 * 3700);

    /* the save changed on the card: put again, it takes the place of the one the template had */
    CHECK_INT(mcfs_delete_save(CARD_A, NET), MCFS_OK);
    put_save(CARD_A, NET, 50);
    CHECK(!save_is(CARD_A, NET, ROOT "Online/" NET ".psu"));
    CHECK_INT(template_add(t, CARD_A, NET), MCFS_OK);
    CHECK_INT(t->n, 3);
    CHECK(save_is(CARD_A, NET, ROOT "Online/" NET ".psu"));
    CHECK_INT(t_size(ROOT "Online/" NET ".psu.new"), -1);

    /* a save taken out of it */
    CHECK_INT(template_remove(t, template_find_save(t, "BASLES-50000HAND")), 0);
    CHECK_INT(t->n, 2);
    CHECK_INT(t_size(ROOT "Online/by hand.psu"), -1);
    CHECK_INT(template_remove(t, 7), -1);
    CHECK_INT(templates_scan(), 1);
    CHECK_INT(templates[0].n, 2);
}

static int asked[TPL_SAVES], nAsked, stopAt;

static int before(const template_t *t, int i)
{
    (void)t;
    asked[nAsked++] = i;
    return nAsked - 1 == stopAt;
}

static void template_into_cards(void)
{
    static const unsigned int big[1] = {3 * 1024 * 1024};
    unsigned char lacks[TPL_SAVES];
    char fp[65], fp2[65];
    long long bytes = 0, freeBytes = 0, freeBefore = 0;
    template_t *t;
    int put = -1;
    new_card(CARD_A);
    new_card(CARD_B);
    put_save(CARD_A, NET, 1);
    put_save(CARD_A, GAME, 2);
    CHECK_INT(templates_scan(), 0);
    t = template_new("Online");
    CHECK(t != NULL);
    if (!t)
        return;
    CHECK_INT(template_add(t, CARD_A, NET), MCFS_OK);
    CHECK_INT(template_add(t, CARD_A, GAME), MCFS_OK);

    /* an empty card lacks all of it, and gets all of it, each save as the template has it */
    CHECK_INT(template_lacking(t, CARD_B, lacks, &bytes, &freeBytes), 2);
    CHECK(lacks[0] && lacks[1]);
    CHECK_INT(bytes, 7400);
    CHECK(freeBytes > 7 * 1024 * 1024);
    nAsked = 0, stopAt = -1;
    CHECK_INT(template_apply(t, CARD_B, before, NULL, &put), MCFS_OK);
    CHECK_INT(put, 2);
    CHECK_INT(nAsked, 2);
    CHECK(save_is(CARD_B, NET, ROOT "Online/" NET ".psu"));
    CHECK(save_is(CARD_B, GAME, ROOT "Online/" GAME ".psu"));
    /* and nothing more the second time */
    CHECK_INT(template_lacking(t, CARD_B, lacks, &bytes, NULL), 0);
    CHECK_INT(bytes, 0);
    CHECK_INT(mcfs_fingerprint(CARD_B, fp, NULL), 0);
    CHECK_INT(template_apply(t, CARD_B, before, NULL, &put), MCFS_OK);
    CHECK_INT(put, 0);
    CHECK_INT(mcfs_fingerprint(CARD_B, fp2, NULL), 0);
    CHECK_STR(fp2, fp);

    /* a save the card has is its own: it stays as it is, and only what is lacking goes in */
    new_card(CARD_B);
    put_save(CARD_B, NET, 77);
    make_psu("own.psu", NET, SIZES, 2, 77);
    CHECK_INT(template_lacking(t, CARD_B, lacks, NULL, NULL), 1);
    CHECK(!lacks[template_find_save(t, NET)] && lacks[template_find_save(t, GAME)]);
    CHECK_INT(template_apply(t, CARD_B, NULL, NULL, &put), MCFS_OK);
    CHECK_INT(put, 1);
    CHECK(save_is(CARD_B, NET, "own.psu"));
    CHECK(save_is(CARD_B, GAME, ROOT "Online/" GAME ".psu"));

    /* stopped before a save: the ones before it stay, the rest doesn't go */
    new_card(CARD_B);
    nAsked = 0, stopAt = 1;
    CHECK_INT(template_apply(t, CARD_B, before, NULL, &put), MCFS_ERR_CANCELLED);
    CHECK_INT(put, 1);
    CHECK_INT(has_save(CARD_B, t->saves[0].folder), 1);
    CHECK_INT(has_save(CARD_B, t->saves[1].folder), 0);

    /* a card with no room for a save keeps what it had, and says so */
    make_psu("big.psu", "BASLUS-99999BIG1", big, 1, 21);
    CHECK_INT(mcfs_import_psu("big.psu", CARD_A, NULL), MCFS_OK);
    CHECK_INT(template_add(t, CARD_A, "BASLUS-99999BIG1"), MCFS_OK);
    new_card(CARD_B);
    make_psu("big.psu", "BASLUS-99999BIG2", big, 1, 22);
    CHECK_INT(mcfs_import_psu("big.psu", CARD_B, NULL), MCFS_OK);
    make_psu("big.psu", "BASLUS-99999BIG3", big, 1, 23);
    CHECK_INT(mcfs_import_psu("big.psu", CARD_B, NULL), MCFS_OK);
    CHECK_INT(template_apply(t, CARD_B, NULL, NULL, &put), MCFS_ERR_FULL);
    CHECK_INT(put, 2);
    CHECK_INT(has_save(CARD_B, NET), 1);
    CHECK_INT(has_save(CARD_B, "BASLUS-99999BIG1"), 0);
    mcfs_list_saves(CARD_B, NULL, 0, &freeBefore);
    CHECK_INT(template_apply(t, CARD_B, NULL, NULL, &put), MCFS_ERR_FULL);
    CHECK_INT(put, 0);
    mcfs_list_saves(CARD_B, NULL, 0, &freeBytes);
    CHECK_INT(freeBytes, freeBefore);

    /* a card that isn't there */
    CHECK_INT(template_lacking(t, "sd/none.mcd", lacks, NULL, NULL), -1);
    CHECK_INT(template_apply(t, "sd/none.mcd", NULL, NULL, &put), MCFS_ERR_IO);
}

static void templates_renamed_and_deleted(void)
{
    template_t *t, *other;
    char name[32];
    int i;
    new_card(CARD_A);
    put_save(CARD_A, NET, 1);
    put_save(CARD_A, GAME, 2);
    CHECK_INT(templates_scan(), 0);
    t = template_new("Online");
    CHECK(t != NULL);
    if (!t)
        return;
    CHECK_INT(template_add(t, CARD_A, NET), MCFS_OK);
    CHECK_INT(template_add(t, CARD_A, GAME), MCFS_OK);
    other = template_new("Another");
    CHECK(other != NULL);

    /* under another name: the same saves in a folder of that name, and the old folder gone */
    CHECK(template_rename(template_find("Online"), "Another") == NULL);
    CHECK(template_rename(template_find("Online"), "a/b") == NULL);
    t = template_rename(template_find("Online"), "Network");
    CHECK(t != NULL);
    if (!t)
        return;
    CHECK_STR(t->name, "Network");
    CHECK_INT(t->n, 2);
    CHECK_INT(nTemplates, 2);
    CHECK(save_is(CARD_A, NET, ROOT "Network/" NET ".psu"));
    CHECK(save_is(CARD_A, GAME, ROOT "Network/" GAME ".psu"));
    CHECK_INT(t_size(ROOT "Online/" NET ".psu"), -1);
    CHECK(template_find("Online") == NULL);
    CHECK_INT(templates_scan(), 2);
    CHECK_STR(templates[0].name, "Another");
    CHECK_STR(templates[1].name, "Network");
    CHECK_INT(templates[1].n, 2);

    /* deleted, with whatever its folder has */
    t_text(ROOT "Network/notes.txt", "left by hand");
    CHECK_INT(template_delete(template_find("Network")), 0);
    CHECK_INT(nTemplates, 1);
    CHECK_INT(t_size(ROOT "Network/" NET ".psu"), -1);
    CHECK_INT(t_size(ROOT "Network/notes.txt"), -1);
    CHECK_INT(templates_scan(), 1);

    /* as many as the list holds, and no more; one of a full list can still be renamed */
    for (i = nTemplates; i < TPL_MAX; i++) {
        snprintf(name, sizeof(name), "T%02d", i);
        CHECK(template_new(name) != NULL);
    }
    CHECK_INT(nTemplates, TPL_MAX);
    CHECK_INT(template_name_check("One more", NULL), TPL_ERR_MANY);
    CHECK(template_new("One more") == NULL);
    CHECK(template_rename(template_find("Another"), "Zed") != NULL);
    CHECK_INT(nTemplates, TPL_MAX);
    CHECK_INT(templates_scan(), TPL_MAX);
    CHECK_STR(templates[TPL_MAX - 1].name, "Zed");
}

static void main_template(void)
{
    static const char *const card = "SLUS-21065/SLUS-21065-1";
    template_t *t;
    new_card(CARD_A);
    put_save(CARD_A, NET, 1);
    put_save(CARD_A, GAME, 2);
    CHECK_INT(templates_scan(), 0);
    t = template_new("Online");
    CHECK(t != NULL);
    if (!t)
        return;
    CHECK_INT(template_add(t, CARD_A, NET), MCFS_OK);

    /* there is none until one is made it, and that is kept on the microSD */
    CHECK_INT(templates_main_set(), 0);
    CHECK(template_main() == NULL);
    template_set_main(t);
    CHECK(template_main() == t);
    CHECK_INT(templates_save(), 0);
    CHECK_INT(templates_main_set(), 1);
    CHECK_STR(tplMain, "Online");

    /* a card is settled with the template as it is, and that is kept too */
    CHECK_INT(template_settled(t, card), 0);
    template_settle(t, card);
    CHECK_INT(template_settled(t, card), 1);
    CHECK_INT(template_settled(t, "Card1/Card1-1"), 0);
    CHECK_INT(templates_save(), 0);
    CHECK_INT(templates_scan(), 1);
    t = template_main();
    CHECK(t != NULL && !strcmp(t->name, "Online"));
    if (!t)
        return;
    CHECK_INT(template_settled(t, card), 1);
    CHECK_INT(template_settled(t, "Card1/Card1-1"), 0);

    /* with another save it is another template, for that: the card isn't settled any more */
    CHECK_INT(template_add(t, CARD_A, GAME), MCFS_OK);
    CHECK_INT(template_settled(t, card), 0);
    template_settle(t, card);
    CHECK_INT(template_settled(t, card), 1);
    CHECK_INT(template_remove(t, template_find_save(t, GAME)), 0);
    CHECK_INT(template_settled(t, card), 0);
    template_settle(t, card);

    /* renamed, it is still the main one, and the cards are as settled as they were; deleted, there is none */
    t = template_rename(t, "Network");
    CHECK(t != NULL);
    CHECK_STR(tplMain, "Network");
    CHECK_INT(templates_scan(), 1);
    CHECK_STR(tplMain, "Network");
    t = template_main();
    CHECK(t != NULL);
    if (!t)
        return;
    CHECK_INT(template_settled(t, card), 1);
    CHECK_INT(template_delete(t), 0);
    CHECK(template_main() == NULL);
    CHECK_INT(templates_main_set(), 0);
    /* (templates.ini is no template) */
    CHECK_INT(templates_scan(), 0);
}

void suite_templates(void)
{
    RUN(templates_are_made);
    RUN(template_into_cards);
    RUN(templates_renamed_and_deleted);
    RUN(main_template);
}
