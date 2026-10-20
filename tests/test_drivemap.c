/* SD2Cloud's regression tests -- the places on Drive in its second layout (drivemap.c): each kind of card's folders
 * and their keys, where each file of the first layout goes, and a batch of calls and its answer. */
#include "t.h"

static void drive_paths(void)
{
    drive_place_t p[DRIVE_DEPTH];
    drive_card_t c = {DK_DEVICE, "sd2psx", "Card1", "Card1-1"};
    char root[DRIVE_KEY];
    drive_root_key(root);
    CHECK_STR(root, "root");
    CHECK_INT(drive_path(&c, 1, p), 4);
    CHECK_STR(p[0].key, "d:sd2psx");
    CHECK_STR(p[0].parent, "");
    CHECK_INT(p[0].level, DL_DEVICE);
    CHECK_STR(p[1].key, "g:sd2psx/Card1");
    CHECK_STR(p[1].parent, "d:sd2psx");
    CHECK_STR(p[2].key, "c:sd2psx/Card1/Card1-1");
    CHECK_STR(p[2].parent, "g:sd2psx/Card1");
    CHECK_INT(p[2].level, DL_CARD);
    CHECK_STR(p[3].key, "s:sd2psx/Card1/Card1-1");
    CHECK_STR(p[3].parent, "c:sd2psx/Card1/Card1-1");
    CHECK_INT(p[3].level, DL_SAVES);
    CHECK_INT(drive_path(&c, 0, p), 3);   /* (a backup: the card's own folder) */
    c.base[0] = 0;
    CHECK_INT(drive_path(&c, 1, p), 2);   /* (no card: as far as its group, which has no Saves) */
    /* the Memory Card in a slot: its folder beside the device's */
    c = (drive_card_t){DK_SLOT, "", "Memory Card 1", ""};
    CHECK_INT(drive_path(&c, 1, p), 2);
    CHECK_STR(p[0].key, "c:slot/Memory Card 1");
    CHECK_STR(p[0].parent, "");
    CHECK_STR(p[1].key, "s:slot/Memory Card 1");
    /* the Library: each game's folder takes its saves */
    c = (drive_card_t){DK_LIBRARY, "", "SLUS-21065", ""};
    CHECK_INT(drive_path(&c, 1, p), 2);
    CHECK_STR(p[0].key, "g:library");
    CHECK_STR(p[1].key, "c:library/SLUS-21065");
    CHECK_STR(p[1].parent, "g:library");
    /* the OPL's VMCs: their group beside the device's */
    c = (drive_card_t){DK_VMC, "", "OPL-VMC-HDD", "generic_0"};
    CHECK_INT(drive_path(&c, 1, p), 3);
    CHECK_STR(p[0].key, "g:vmc/OPL-VMC-HDD");
    CHECK_STR(p[1].key, "c:vmc/OPL-VMC-HDD/generic_0");
    CHECK_STR(p[2].key, "s:vmc/OPL-VMC-HDD/generic_0");
}

/* the first layout as Drive lists it: the main folder R; Card1 (its backup, its Saves with a save of another of its
 * cards, a file tagged as nothing), a game's group of a MemCard PRO2, the Memory Card 1 and its Saves, the Library,
 * the OPL's VMCs; and a card's folder of the second layout already, with a backup in it */
static drive_node_t tree[] = {
    {"g1", "R", "Card1", "", "", "", 1},
    {"s1", "g1", "Saves", "", "", "", 1},
    {"g2", "R", "SLUS-21065", "", "", "", 1},
    {"g3", "R", "Memory Card 1", "", "", "", 1},
    {"s3", "g3", "Saves", "", "", "", 1},
    {"g4", "R", "Library", "", "", "", 1},
    {"g5", "R", "OPL-VMC", "", "", "", 1},
    {"k1", "R", "Card1-1", "c:sd2psx/Card1/Card1-1", "", "", 1},
    {"z1", "g1", "Card1-1 2026-10-17 14h03.zip", "", "Card1/Card1-1", "sd2psx", 0},
    {"p1", "s1", "BASLUS-21065ALPHA 2026-10-17 14h03m10.psu", "", "save:Card1/Card1-2/BASLUS-21065ALPHA", "", 0},
    {"u1", "g1", "notes.txt", "", "", "", 0},
    {"z2", "g2", "SLUS-21065-1 2026-10-17 14h03.zip", "", "SLUS-21065/SLUS-21065-1", "MemCard PRO2", 0},
    {"p2", "s1", "BASLUS-99999 2026-10-17 14h03m10.psu", "", "save:SLUS-21065/SLUS-21065-1/BASLUS-99999", "", 0},
    {"z3", "g3", "Memory Card 1 2026-10-17 14h03.zip", "", "slot/memcard1", "sd2psx", 0},
    {"p3", "s3", "BASLUS-20001 2026-10-17 14h03m10.psu", "", "save:slot/memcard1/BASLUS-20001", "", 0},
    {"l1", "g4", "BASLUS-21065ALPHA 2026-10-17 14h03m10.psu", "", "save:library/BASLUS-21065ALPHA", "", 0},
    {"v1", "g5", "generic_0 2026-10-17 14h03.zip", "", "OPL-VMC/generic_0", "sd2psx", 0},
    {"x1", "k1", "Card1-1 2026-10-18 10h00.zip", "", "Card1/Card1-1", "sd2psx", 0},
    {"o1", "elsewhere", "Card1-1 2026-10-18 10h00.zip", "", "Card1/Card1-1", "sd2psx", 0},
};

static int at(const char *id)
{
    int i;
    for (i = 0; i < (int)(sizeof(tree) / sizeof(tree[0])); i++)
        if (!strcmp(tree[i].id, id))
            return i;
    return -1;
}

static void drive_plan(void)
{
    const int n = sizeof(tree) / sizeof(tree[0]);
    drive_card_t c;
    int saves;
    CHECK_INT(drive_plan_file(tree, n, at("z1"), "R", "MemCard PRO2", &c, &saves), 1);   /* (its description says) */
    CHECK_INT(c.kind, DK_DEVICE);
    CHECK_STR(c.device, "sd2psx");
    CHECK_STR(c.group, "Card1");
    CHECK_STR(c.base, "Card1-1");
    CHECK_INT(saves, 0);
    /* a save of a card with no backup of its own: the device of its group's */
    CHECK_INT(drive_plan_file(tree, n, at("p1"), "R", "MemCard PRO2", &c, &saves), 1);
    CHECK_INT(c.kind, DK_DEVICE);
    CHECK_STR(c.device, "sd2psx");
    CHECK_STR(c.group, "Card1");
    CHECK_STR(c.base, "Card1-2");
    CHECK_INT(saves, 1);
    /* a save of another group's card, in this one's Saves: its own card's, and that card's device */
    CHECK_INT(drive_plan_file(tree, n, at("p2"), "R", "sd2psx", &c, &saves), 1);
    CHECK_STR(c.device, "MemCard PRO2");
    CHECK_STR(c.group, "SLUS-21065");
    CHECK_STR(c.base, "SLUS-21065-1");
    CHECK_INT(saves, 1);
    /* tagged as nothing: its group's folder */
    CHECK_INT(drive_plan_file(tree, n, at("u1"), "R", "MemCard PRO2", &c, &saves), 1);
    CHECK_INT(c.kind, DK_DEVICE);
    CHECK_STR(c.device, "sd2psx");
    CHECK_STR(c.group, "Card1");
    CHECK_STR(c.base, "");
    CHECK_INT(saves, 0);
    CHECK_INT(drive_plan_file(tree, n, at("z2"), "R", "sd2psx", &c, &saves), 1);
    CHECK_STR(c.device, "MemCard PRO2");
    /* the Memory Card: its backups and its saves, in its group's own folder */
    CHECK_INT(drive_plan_file(tree, n, at("z3"), "R", "sd2psx", &c, &saves), 1);
    CHECK_INT(c.kind, DK_SLOT);
    CHECK_STR(c.group, "Memory Card 1");
    CHECK_INT(saves, 0);
    CHECK_INT(drive_plan_file(tree, n, at("p3"), "R", "sd2psx", &c, &saves), 1);
    CHECK_INT(c.kind, DK_SLOT);
    CHECK_STR(c.group, "Memory Card 1");
    CHECK_INT(saves, 1);
    /* the Library's: their game's folder */
    CHECK_INT(drive_plan_file(tree, n, at("l1"), "R", "sd2psx", &c, &saves), 1);
    CHECK_INT(c.kind, DK_LIBRARY);
    CHECK_STR(c.group, "SLUS-21065");
    CHECK_INT(drive_plan_file(tree, n, at("v1"), "R", "sd2psx", &c, &saves), 1);
    CHECK_INT(c.kind, DK_VMC);
    CHECK_STR(c.group, "OPL-VMC");
    CHECK_STR(c.base, "generic_0");
    /* in place already, folders, and what isn't under the main folder: nothing to move */
    CHECK_INT(drive_plan_file(tree, n, at("x1"), "R", "sd2psx", &c, &saves), 0);
    CHECK_INT(drive_plan_file(tree, n, at("g1"), "R", "sd2psx", &c, &saves), 0);
    CHECK_INT(drive_plan_file(tree, n, at("o1"), "R", "sd2psx", &c, &saves), 0);
}

static void drive_batch(void)
{
    static const char *const method[] = {"PATCH", "DELETE"};
    static const char *const path[] = {"/drive/v3/files/A?addParents=B&removeParents=C&fields=id", "/drive/v3/files/D"};
    static const char *const json[] = {"{}", NULL};
    char body[2000], id[3][48];
    int status[3];
    size_t n = drive_batch_body(body, sizeof(body), "sd2cloud", 2, method, path, json);
    const char *answer =
        "--batch_X1\r\nContent-Type: application/http\r\nContent-ID: <response-item-2>\r\n\r\n"
        "HTTP/1.1 404 Not Found\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{\"error\": {\"code\": 404}}\r\n"
        "--batch_X1\r\nContent-Type: application/http\r\nContent-ID: <response-item-1>\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{\n \"id\": \"A\"\n}\r\n"
        "--batch_X1--\r\n";
    CHECK(n > 0);
    CHECK(strstr(body, "--sd2cloud\r\nContent-Type: application/http\r\nContent-ID: <item-1>\r\n\r\n"
                       "PATCH /drive/v3/files/A?addParents=B&removeParents=C&fields=id HTTP/1.1\r\n"
                       "Content-Type: application/json; charset=UTF-8\r\n\r\n{}\r\n") != NULL);
    CHECK(strstr(body, "Content-ID: <item-2>\r\n\r\nDELETE /drive/v3/files/D HTTP/1.1\r\n\r\n--sd2cloud--\r\n") != NULL);
    CHECK_INT(drive_batch_body(body, 100, "sd2cloud", 2, method, path, json), 0);   /* (too small: nothing) */
    CHECK_INT(drive_batch_read(answer, strlen(answer), 3, status, id), 2);   /* (the answers in any order) */
    CHECK_INT(status[0], 200);
    CHECK_STR(id[0], "A");
    CHECK_INT(status[1], 404);
    CHECK_STR(id[1], "");
    CHECK_INT(status[2], 0);   /* (none came for it) */
    CHECK_INT(drive_batch_read("nothing", 7, 3, status, id), 0);
}

void suite_drivemap(void)
{
    RUN(drive_paths);
    RUN(drive_plan);
    RUN(drive_batch);
}
