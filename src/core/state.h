/* SD2Cloud -- what is remembered from one run to the next: state.ini and token.dat (state.c) */
#ifndef STATE_H
#define STATE_H

#include <stddef.h>
#include <tamtypes.h>

#define MAX_CARDS 512
/* version of the fingerprint (mcfs.c). 2 = without the "history" of the B?DATA-SYSTEM folders; 3 = without those
 * folders at all */
#define MCFS_VERSION 3
typedef struct {
    char id[96];
    char fingerprint[65];
    int version;           /* MCFS_VERSION the fingerprint was computed with (0 = the first one, before 2026-10-02) */
    char sha[65];
    char when[20];
} card_state_t;
card_state_t *state_card(const char *id, int create);
const char *state_folder(const char *name);              /* Drive folder id (cache) */
void state_set_folder(const char *name, const char *id);
void state_read(void);
int state_write(void);
int state_empty(void);                                   /* nothing was ever backed up */
extern char refreshToken[512];
void token_read(void);
int token_write(void);

#endif
