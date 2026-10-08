/* SD2Cloud -- updating the program from its GitHub releases (update.c) */
#ifndef UPDATE_H
#define UPDATE_H

#include <stddef.h>
#include <tamtypes.h>

/* Asks GitHub for the release of the channel in use: the latest one (stable) or the "beta" pre-release. 1 = it has
 * something to install (SD2CLOUD.ELF, with GitHub's SHA-256), 0 = not, -1 = no answer. Stable: a newer version, or
 * this very version with another file (a beta of it is what is installed). Beta: any file other than the one
 * installed. back = 1 (the beta channel was just left): the stable release counts whenever the file installed is not
 * its own, even if that one says it is newer */
int update_check(int back);
const char *update_tag(void);               /* what was found: "v1.6", or "beta 63cdae2" */
/* 0 = installed and verified (the app then reopens the new version); -3 = the program is on a memory card with no
 * room for the new one (helperNeedKb, helperFreeKb). progress: how far writing to that card is (elsewhere it isn't
 * called) */
int update_install(void (*progress)(long long done, long long total));

#endif
