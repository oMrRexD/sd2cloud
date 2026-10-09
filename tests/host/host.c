/* What the engine's modules take from the rest of SD2Cloud, stood in for on a PC: where the microSD is (the folder
 * sd/ of where the tests run), the device, the log, the clock, the texts, a folder's listing, and a malloc that can
 * be told to refuse what is too big. */
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#include <fileXio_rpc.h>

char googleError[256];
char sdRoot[16] = "sd/";
char dataDir[32] = "sd/SD2Cloud/";
static const device_t sd2psx = {"sd2psx", "sd2psx", "MemoryCards/PS2", ".mcd", "Card", 1};
const device_t *dev = &sd2psx;
int cardTold = 1;

/* the log: on the screen when SD2CLOUD_TEST_LOG is set, to see what a failing test did */
void log_raw(const char *d, size_t n)
{
    if (getenv("SD2CLOUD_TEST_LOG"))
        fwrite(d, 1, n, stderr);
}

void log_msg(const char *fmt, ...)
{
    va_list ap;
    if (!getenv("SD2CLOUD_TEST_LOG"))
        return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

u64 now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (u64)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void sleep_ms(int ms) { usleep(ms * 1000); }

/* always the same moment: what is made with it (a .zip's dates) comes out the same on every run */
void local_time(datetime_t *t)
{
    static const datetime_t when = {2026, 1, 2, 3, 4, 5};
    *t = when;
}

/* the texts, in English */
const char *T(int id)
{
    static const char *const text[] = {
        "",
#define X(id, en, pt) en,
#include "messages.def"
#undef X
    };
    return id > 0 && id < T_COUNT ? text[id] : "";
}

/* no network in the tests */
int google_download(const char *id, int (*sink)(const unsigned char *d, size_t n, void *u), void *u)
{
    (void)id;
    (void)sink;
    (void)u;
    return -1;
}

/* the files the build embeds in the program: here, just enough of each */
unsigned char asset_gamenames_txt[] = "SLUS-21065\tA Game For The Tests\nSLES-50000\tAnother Game\n"
    /* three series, for the folders Game2Folder.ini gives to more than one game */
    "SLES-30001\tRacing Series - First Lap\nSLUS-30001\tRacing Series - First Lap\n"
    "SLUS-30002\tRacing Series - Second Lap [Gold Edition]\nSLUS-30003\tRacing Series - The Third Lap Of Them All\n"
    "SLES-30011\tZombie Zone\nSLES-30012\tZombie Hunters\n"
    "SLUS-30021\tBurnout 3 - Takedown\nSLUS-30022\tNFL Street 2\nSLUS-30023\tBlack [Review]\n"
    "SLUS-30024\tA Game With A Name That Goes On And On\n"
    "SLPM-30031\tOneechanbara\nSLES-30032\tZombie Zone Deluxe\n"
    "SLUS-30041\tAvatar - The Last One\nSLUS-30042\tAvatar - The Legend That Goes On And On And On\n";
unsigned int size_asset_gamenames_txt = sizeof(asset_gamenames_txt) - 1;
unsigned char ini_default[] = "; SD2Cloud settings\n[general]\nlanguage = auto\n\n[igr]\nreturn = auto\n";
unsigned int size_ini_default = sizeof(ini_default) - 1;

/* a folder's entries, as fileXio gives them: one folder open at a time is all files.c asks for */
static DIR *openDir;
static char openPath[400];

int fileXioDopen(const char *name)
{
    if (openDir)
        return -1;
    snprintf(openPath, sizeof(openPath), "%s", name);
    return (openDir = opendir(name)) ? 1 : -1;
}

int fileXioDread(int fd, iox_dirent_t *e)
{
    char c[700];
    struct dirent *d;
    struct stat st;
    (void)fd;
    if (!openDir || !(d = readdir(openDir)))
        return 0;
    memset(e, 0, sizeof(*e));
    snprintf(e->name, sizeof(e->name), "%s", d->d_name);
    snprintf(c, sizeof(c), "%s/%s", openPath, d->d_name);
    if (stat(c, &st) == 0) {
        e->stat.mode = S_ISDIR(st.st_mode) ? FIO_S_IFDIR : 0x2000;
        e->stat.size = (unsigned int)st.st_size;
        e->stat.hisize = (unsigned int)((unsigned long long)st.st_size >> 32);
    }
    return 1;
}

int fileXioDclose(int fd)
{
    (void)fd;
    if (openDir)
        closedir(openDir);
    openDir = NULL;
    return 0;
}

/* SD2Cloud asks for a whole card's worth of memory and goes another way when there isn't that much: the tests say
 * how much is "too much" (the build wraps malloc, as the program's own does) */
size_t hostMallocLimit;
void *__real_malloc(size_t n);

void *__wrap_malloc(size_t n)
{
    if (hostMallocLimit && n > hostMallocLimit)
        return NULL;
    return __real_malloc(n);
}
