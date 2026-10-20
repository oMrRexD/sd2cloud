/* SD2Cloud -- the Library: saves kept off the cards (library.c) */
#ifndef LIBRARY_H
#define LIBRARY_H

#include <stddef.h>

/* The Library is a card with no end to its room, as far as the rest of the program can tell: mcfs reads and writes it
 * through the calls a Memory Card in a slot goes through. Its path is LIBRARY_PATH for all of its saves, and
 * LIBRARY_PATH followed by a game's ID ("lib:SLUS-21065", or LIBRARY_OTHER for the saves whose folder names no game)
 * for one game's. A save copied into it goes to its own game's folder, whichever of those paths it was given */
#define LIBRARY_PATH  "lib:"
#define LIBRARY_OTHER "OTHER"
#define LIBRARY_MAX   512   /* saves (as many as a list of saves takes) */

/* the Library's folder ("mmce0:/SD2Cloud/Library/"; NULL = there is none: the data folder has no room for it), and
 * its list read. Called again when the data folder may be another one */
void library_init(const char *dir);
int library_on(void);            /* it is there */
int library_count(void);         /* how many saves it has */
/* its games, by name: how many; each one's name (the game's, its ID when it isn't known, "" for LIBRARY_OTHER: the
 * screens name that one), its ID (or LIBRARY_OTHER) and how many saves it has */
int library_games(void);
const char *library_game_name(int k);
const char *library_game_id(int k);
int library_game_saves(int k);
/* the game a save of that folder is kept under: its ID, or LIBRARY_OTHER */
void library_game_of(const char *folder, char id[12]);
/* is that a path of the Library? (0 = a card's) */
int library_path(const char *path);
/* that save's signature, as the backup by save takes a save's (FNV-1a of its .psu), kept since it went into the
 * Library: nothing is read for it. 0 = it has none (it isn't there, or its file can't be read) */
unsigned long long library_signature(const char *folder);

#endif
