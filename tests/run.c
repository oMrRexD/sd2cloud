/* SD2Cloud's regression tests -- runs every suite and says how it went. It exits with an error when a check fails,
 * which is what stops a build on GitHub. Run from the folder it may fill and empty (the Makefile uses build/work). */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "t.h"

int tChecks, tFailed;
const char *tName = "";

void t_fail(const char *file, int line, const char *what)
{
    tFailed++;
    fprintf(stderr, "FAILED  %s (%s:%d): %s\n", tName, file, line, what);
}

static void wipe(const char *path)
{
    char c[600];
    struct dirent *e;
    DIR *d = opendir(path);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        struct stat st;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        snprintf(c, sizeof(c), "%s/%s", path, e->d_name);
        if (lstat(c, &st) == 0 && S_ISDIR(st.st_mode)) {
            wipe(c);
            rmdir(c);
        } else
            unlink(c);
    }
    closedir(d);
}

void t_fresh(void)
{
    wipe(".");
    t_mkdir("sd/SD2Cloud");
    hostMallocLimit = 0;
    googleError[0] = 0;
}

int main(void)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } suites[] = {{"host", suite_host},   {"json", suite_json},   {"ini", suite_ini},             {"state", suite_state},
                  {"mcfs", suite_mcfs},   {"cards", suite_cards}, {"card file", suite_card_file}};
    size_t i;
    for (i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        int before = tChecks, failed = tFailed;
        suites[i].run();
        printf("%-10s %4d checks%s\n", suites[i].name, tChecks - before, tFailed > failed ? "   <-- FAILED" : "");
    }
    printf("%d checks, %d failed\n", tChecks, tFailed);
    return tFailed ? 1 : 0;
}
