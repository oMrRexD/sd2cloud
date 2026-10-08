/* What files.c uses of the PS2 SDK's fileXio: listing a folder. host.c does it with the PC's own folders */
#ifndef FILEXIO_RPC_H
#define FILEXIO_RPC_H

#include "iox_stat.h"

int fileXioDopen(const char *name);
int fileXioDread(int fd, iox_dirent_t *e);
int fileXioDclose(int fd);

#endif
