/* SD2Cloud -- leaving: which program is opened when SD2Cloud closes (OPL, or the one chosen in the settings), found on
 * the microSD and started. */
#include "app.h"

/* ------------------------------------------------------------ leaving */

/* a title.cfg is read the way OPL reads it: "title" and "boot" exactly, in lowercase. The same file may also have a
 * "Title", the description some packages add for OPL's info page */
static void on_title_cfg(const char *s, const char *k, const char *v, void *u)
{
    (void)s;
    if (!strcmp(k, "boot"))
        snprintf((char *)u, 64, "%s", v);
}

static int looks_like_opl(const char *dir, const char *elf)
{
    return strcasestr(dir, "OPL") || strcasestr(elf, "OPL") || strcasestr(elf, "OPNPS2LD");
}

/* does any .cfg in that OPL folder point the "IGR Path" (exit_path) to SD2Cloud's helper? */
static int points_to_helper(const char *dir)
{
    static char cfgs[16][64];
    char c[260];
    DIR *d;
    struct dirent *e;
    int n = 0, i, found = 0;
    if ((d = opendir(dir)) != NULL) {   /* read the whole list and close it before opening anything else (sd2psx) */
        while ((e = readdir(d)) != NULL && n < 16) {
            size_t len = strlen(e->d_name);
            if (len > 4 && !strcasecmp(e->d_name + len - 4, ".cfg"))
                snprintf(cfgs[n++], sizeof(cfgs[0]), "%s", e->d_name);
        }
        closedir(d);
    }
    for (i = 0; i < n && !found; i++) {
        buffer_t b = {0};
        char *l, *nl;
        snprintf(c, sizeof(c), "%s/%s", dir, cfgs[i]);
        if (file_read(c, &b) == 0 && b.data)
            for (l = (char *)b.data; l && *l && !found; l = nl ? nl + 1 : NULL) {
                if ((nl = strchr(l, '\n')) != NULL)
                    *nl = 0;   /* one line at a time */
                if (!strncmp(l, "exit_path", 9) && strcasestr(l, "SD2CLOUD-IGR"))
                    found = 1;
            }
        buf_free(&b);
    }
    return found;
}

/* "auto": the OPL to return to, without the user saying which. 1) the OPL in APPS whose settings point the "IGR Path"
 * to our helper (that's the one using SD2Cloud, even with other OPLs installed); 2) the first OPL found in APPS (by its
 * title.cfg and name); 3) a loose APPS/OPNPS2LD.ELF. 0 = found, in out */
int find_opl(char *out, size_t size)
{
    static char dirs[64][64];
    char base[64], dir[160], c[260], elf[64], first[260] = "";
    DIR *d;
    struct dirent *e;
    int n = 0, i;
    snprintf(base, sizeof(base), "%sAPPS", sdRoot);
    if ((d = opendir(base)) != NULL) {   /* read the whole list and close it before opening anything else (sd2psx) */
        while ((e = readdir(d)) != NULL && n < 64)
            if (e->d_name[0] != '.' && strcasecmp(e->d_name, "SD2Cloud"))
                snprintf(dirs[n++], sizeof(dirs[0]), "%s", e->d_name);
        closedir(d);
    }
    for (i = 0; i < n; i++) {
        elf[0] = 0;
        snprintf(dir, sizeof(dir), "%s/%s", base, dirs[i]);
        snprintf(c, sizeof(c), "%s/title.cfg", dir);
        ini_read(c, on_title_cfg, elf);
        if (!elf[0] || !looks_like_opl(dirs[i], elf))
            continue;
        snprintf(c, sizeof(c), "%s/%s", dir, elf);
        if (!file_exists(c))
            continue;
        if (points_to_helper(dir)) {
            log_msg("OPL to return to: %s (its IGR Path points to SD2Cloud)", c);
            snprintf(out, size, "%s", c);
            return 0;
        }
        if (!first[0])
            snprintf(first, sizeof(first), "%s", c);
    }
    if (!first[0]) {
        snprintf(c, sizeof(c), "%sAPPS/OPNPS2LD.ELF", sdRoot);
        if (file_exists(c))
            snprintf(first, sizeof(first), "%s", c);
    }
    if (!first[0])
        return -1;
    log_msg("OPL to return to: %s (the first one found in APPS)", first);
    snprintf(out, size, "%s", first);
    return 0;
}

/* the path in the .ini (the main way: the user says where the OPL is), "osd" = the PS2 menu, "auto" = look for it
 * (find_opl). If a path on the sd2psx doesn't exist (typo, OPL moved), look for it before falling back to the menu; a
 * path on another device (USB, MX4SIO, HDD, memory card) is checked by run_elf, after loading that device's drivers.
 * In out: the program to run, or "osd" */
void resolve_target(const char *target, char *out, size_t size)
{
    char *p;
    snprintf(out, size, "%s", target);
    if ((p = strstr(out, "mmce?:")) != NULL)   /* mmce?: becomes the microSD's slot */
        p[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
    if (strcasecmp(out, "osd") != 0 && strcasecmp(out, "auto") != 0 && device_of(out) == DEV_SD && !file_exists(out)) {
        log_msg("the path in the .ini doesn't exist (%s): looking for the OPL", out);
        snprintf(out, size, "auto");
    }
    if (!strcasecmp(out, "auto") && find_opl(out, size) != 0)
        snprintf(out, size, "osd");
}
/* runs what resolve_target found */
void run_target(const char *resolved)
{
    log_msg("returning to: %s", resolved);
    if (strcasecmp(resolved, "osd") != 0)
        run_elf(resolved);
    go_osd();
}

static void return_to(const char *target) __attribute__((noreturn));
static void return_to(const char *target)
{
    char c[260];
    resolve_target(target, c, sizeof(c));
    run_target(c);
}
/* leaving with the exit sound: it plays to the end before the next program takes over */
void leave(const char *target)
{
    sound_play(SND_EXIT);
    sound_wait(SND_EXIT);
#ifdef DEBUG_BUILD
    debug_capture_if('F');   /* the last screen, before leaving */
#endif
    return_to(target);
}
app_t apps[MAX_APPS];
int nApps, appsListed;

static void on_app_cfg(const char *s, const char *k, const char *v, void *u)
{
    app_t *a = u;
    (void)s;
    if (!strcmp(k, "title"))   /* not "Title": see on_title_cfg */
        snprintf(a->title, sizeof(a->title), "%s", v);
    else if (!strcmp(k, "boot"))
        snprintf(a->path, sizeof(a->path), "%s", v);
}

void list_apps(void)
{
    static char dirs[MAX_APPS][64];
    char base[64], c[260];
    DIR *d;
    struct dirent *e;
    int n = 0, i;
    nApps = 0;
    snprintf(base, sizeof(base), "%sAPPS", sdRoot);
    if ((d = opendir(base)) != NULL) {   /* read the whole list and close it before opening anything else (sd2psx) */
        while ((e = readdir(d)) != NULL && n < MAX_APPS)
            if (e->d_name[0] != '.' && strcasecmp(e->d_name, "SD2Cloud"))
                snprintf(dirs[n++], sizeof(dirs[0]), "%s", e->d_name);
        closedir(d);
    }
    for (i = 0; i < n; i++) {
        app_t *a = &apps[nApps];
        memset(a, 0, sizeof(*a));
        snprintf(c, sizeof(c), "%s/%s/title.cfg", base, dirs[i]);
        if (ini_read(c, on_app_cfg, a) != 0 || !a->path[0] || strchr(a->path, '/'))
            continue;
        snprintf(c, sizeof(c), "%s/%s/%s", base, dirs[i], a->path);
        if (!file_exists(c))
            continue;
        snprintf(a->path, sizeof(a->path), "%s", c);
        if (!a->title[0])
            snprintf(a->title, sizeof(a->title), "%s", dirs[i]);
        nApps++;
    }
    appsListed = 1;
    log_msg("%d programs in APPS", nApps);
}

/* a path as the .ini keeps it ("mmce?:" = the microSD's slot, which can change) and as it is on this console */
void target_for_ini(const char *path, char *out, size_t size)
{
    snprintf(out, size, "%s", path);
    if (!strncmp(out, "mmce", 4) && out[4] && out[5] == ':')
        out[4] = '?';
}

void target_on_sd(const char *target, char *out, size_t size)
{
    char *p;
    snprintf(out, size, "%s", target);
    if ((p = strstr(out, "mmce?:")) != NULL)
        p[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
}

/* is there a sync to do at IGR? Not with the automatic sync turned off in the settings, and not without a Google
 * account: the choice in the settings is kept as it is meanwhile, for when an account is connected again */
int auto_sync_on(void) { return !cfg.no_auto_sync && google_has_access(); }

/* With no sync to do, the IGR helper starts what comes after IGR by itself, without loading SD2Cloud (igr/igr.c). It
 * reads the settings as they are, but can't look for the OPL that "auto" stands for: the one found here is left for
 * it in the settings ([app] igr_auto), written only when it changes; with no OPL found the line is left empty, and
 * the helper starts SD2Cloud, which looks again. found = what "auto" led to just now; NULL = look for it, which is
 * only done while the helper has a use for it */
void note_igr_auto(const char *found)
{
    char c[260], t[200] = "";
    if (strcasecmp(cfg.igr_return, "auto") != 0)
        return;
    if (!found) {
        if (auto_sync_on())
            return;
        resolve_target("auto", c, sizeof(c));
        found = c;
    }
    if (strcasecmp(found, "osd") != 0)
        target_for_ini(found, t, sizeof(t));
    if (strcasecmp(t, cfg.igr_auto) != 0) {
        snprintf(cfg.igr_auto, sizeof(cfg.igr_auto), "%s", t);
        config_set("app", "igr_auto", t);
    }
}

/* the name the user gave in sd2cloud.ini ("name", under [manual] or [igr]) to the program that section's "return"
 * points to, when target is that program; NULL = none */
const char *given_name(const char *target)
{
    char q[260], w[260];
    target_on_sd(target, q, sizeof(q));
    target_on_sd(cfg.manual_return, w, sizeof(w));
    if (cfg.manual_name[0] && !strcasecmp(q, w))
        return cfg.manual_name;
    target_on_sd(cfg.igr_return, w, sizeof(w));
    if (cfg.igr_name[0] && !strcasecmp(q, w))
        return cfg.igr_name;
    return NULL;
}

/* what the settings show for a target: the name the user gave it, the program's title, "Automatic" or the PS2 menu */
void target_name(const char *target, char *out, size_t size)
{
    char q[260], *p, *s;
    int i;
    if (!strcasecmp(target, "auto") || !strcasecmp(target, "osd")) {
        snprintf(out, size, "%s", T(!strcasecmp(target, "auto") ? T_AUTO : T_RET_OSD));
        return;
    }
    if (given_name(target)) {
        snprintf(out, size, "%s", given_name(target));
        return;
    }
    target_on_sd(target, q, sizeof(q));
    for (i = 0; i < nApps; i++)
        if (!strcasecmp(apps[i].path, q)) {
            snprintf(out, size, "%s", apps[i].title);
            return;
        }
    /* a path written by hand: its folder in APPS, or the file; on another device, which one */
    if ((p = strcasestr(q, "APPS/")) != NULL && (s = strchr(p + 5, '/')) != NULL) {
        *s = 0;
        snprintf(out, size, "%s", p + 5);
    } else
        snprintf(out, size, "%s", (p = strrchr(q, '/')) != NULL ? p + 1 : (p = strrchr(q, ':')) != NULL ? p + 1 : q);
    i = device_of(target);
    if (i != DEV_SD) {
        size_t n = strlen(out);
        snprintf(out + n, size - n, " (%s)", i == DEV_MC ? T(T_DEV_MC) : i == DEV_USB ? "USB" : i == DEV_MX4SIO ? "MX4SIO" : T(T_DEV_HDD));
    }
}
