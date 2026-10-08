/* What files.c uses of the PS2 SDK's iox_stat.h: a folder's entry, as fileXio gives it */
#ifndef IOX_STAT_H
#define IOX_STAT_H

#define FIO_S_IFDIR 0x1000
#define FIO_S_ISDIR(m) (((m) & 0xF000) == FIO_S_IFDIR)

typedef struct {
    unsigned int mode, attr, size;
    unsigned char ctime[8], atime[8], mtime[8];
    unsigned int hisize;
} iox_stat_t;

typedef struct {
    iox_stat_t stat;
    char name[256];
} iox_dirent_t;

#endif
