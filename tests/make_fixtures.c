/* SD2Cloud's regression tests -- makes what the PCSX2 scenarios start from (tests/pcsx2/fixtures), with the program's
 * own code: an empty card, a card with two saves of one game, a save as a .psu file, and another version of one of
 * the card's saves as a .psu. They are always the same bytes, and nothing in them comes from a game. Run by
 * "make -C tests fixtures", in the folder they go to. */
#include "t.h"

static void must(int r, const char *what)
{
    if (r != MCFS_OK) {
        fprintf(stderr, "make_fixtures: %s (%d)\n", what, r);
        exit(1);
    }
}

int main(void)
{
    static const unsigned int alpha[] = {600, 3000}, beta[] = {40000}, gamma[] = {1200, 100, 9000};
    must(mcfs_new_card("empty.mcd", NULL), "empty.mcd");
    /* (the folders are named as a game names its saves: "BA" + the game's ID + its own name) */
    make_psu("alpha.psu", "BASLUS-21065ALPHA", alpha, 2, 1);
    make_psu("beta.psu", "BASLUS-21065BETA", beta, 1, 2);
    make_psu("save.psu", "BASLUS-21065GAMMA", gamma, 3, 3);
    make_psu("alpha-other.psu", "BASLUS-21065ALPHA", alpha, 2, 7);   /* (game.mcd's first save, with other bytes) */
    must(mcfs_new_card("game.mcd", NULL), "game.mcd");
    must(mcfs_import_psu("alpha.psu", "game.mcd", NULL), "the first save of game.mcd");
    must(mcfs_import_psu("beta.psu", "game.mcd", NULL), "the second save of game.mcd");
    remove("alpha.psu");
    remove("beta.psu");
    return 0;
}
