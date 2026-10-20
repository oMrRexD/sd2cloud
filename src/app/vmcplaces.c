/* SD2Cloud -- the OPL's VMC folders on other devices than the microSD: found when the cards are looked for, named on
 * the screens, and added or taken out in the settings ([vmc] in sd2cloud.ini). */
#include "app.h"

/* each key's group (VMC_FOLDER-...), its drive and its name on the screens */
static const char *const groups[VMC_KEYS] = {"HDD", "APA", "MX4SIO", "USB", "OTHER"};
static const int keyDrive[VMC_KEYS] = {DRIVE_HDD, DRIVE_HDD, DRIVE_MX4SIO, DRIVE_USB, -1};
static const int keyText[VMC_KEYS] = {T_DEV_HDD, T_VMC_PLACE_APA, T_DEV_MX4SIO, T_DEV_USB, T_VMC_PLACE_OTHER};

/* "pfs1:/VMC" -> "pfs1:/CFG": the OPL's settings of its games, beside its VMC folder */
static void cfg_beside(const char *dir, char *out, size_t size)
{
    const char *p = strrchr(dir, '/');
    if (!p)
        p = strchr(dir, ':');
    snprintf(out, size, "%.*sCFG", p ? (int)(p - dir + 1) : 0, dir);
}

/* (cards.c's vmcMore) Each folder the user added, on its device: its drivers loaded and the device waited for a
 * moment (the hard disk longer: it may have to spin up); on the APA side, its partition mounted and kept so. A device
 * that isn't there is left out, and so is an MX4SIO while a memory card is in slot 2 (fdev_list's rule) */
static int find_places(vmc_place_t *more, int max)
{
    int k, n = 0;
    if (cfg.vmc[VMC_MX4SIO][0])
        fdev_refresh();
    for (k = 0; k < VMC_KEYS && n < max; k++) {
        const char *v = cfg.vmc[k], *slash;
        char root[64], part[40];
        vmc_place_t *p = &more[n];
        size_t len;
        if (!v[0])
            continue;
        if (k == VMC_MX4SIO && !mx4sio_listed()) {
            log_msg("VMCs: the MX4SIO's left out (a memory card in slot 2)");
            continue;
        }
        if (keyDrive[k] >= 0 && drive_open(keyDrive[k], keyDrive[k] == DRIVE_HDD ? 12000 : 3000, root, sizeof(root)) != 0)
            continue;
        memset(p, 0, sizeof(*p));
        if (k == VMC_APA) {
            slash = strchr(v, '/');
            snprintf(part, sizeof(part), "%.*s", slash ? (int)(slash - v) : (int)strlen(v), v);
            if (hdd_mount_kept(part, root, sizeof(root)) != 0)
                continue;
            snprintf(p->dir, sizeof(p->dir), "%s%s", root, slash ? slash + 1 : "");
        } else if (k == VMC_OTHER)
            snprintf(p->dir, sizeof(p->dir), "%s", v);
        else if (!strcmp(root, "hdd0:"))   /* (a disk with APA partitions only: it has no exFAT one) */
            continue;
        else
            snprintf(p->dir, sizeof(p->dir), "%s%s", root, v + (v[0] == '/'));
        if ((len = strlen(p->dir)) > 1 && p->dir[len - 1] == '/')
            p->dir[len - 1] = 0;
        cfg_beside(p->dir, p->cfg, sizeof(p->cfg));
        snprintf(p->key, sizeof(p->key), "%s", vmcKeys[k]);
        snprintf(p->folder, sizeof(p->folder), "%s-%s", VMC_FOLDER, groups[k]);
        n++;
    }
    return n;
}

void vmc_places_init(void) { vmcMore = find_places; }

/* the key of a group of VMCs (-1 = the microSD's, or none) */
static int key_of(const char *folder)
{
    int k;
    for (k = 0; k < VMC_KEYS; k++)
        if (!strncmp(folder, VMC_FOLDER "-", sizeof(VMC_FOLDER)) && !strcmp(folder + sizeof(VMC_FOLDER), groups[k]))
            return k;
    return -1;
}

const char *vmc_place_name(const char *folder)
{
    int k = key_of(folder);
    return k >= 0 ? T(keyText[k]) : T(standalone ? storage_text() : T_DEV_SD);
}

const vmc_place_t *vmc_place_pick(void)
{
    static const char *items[VMC_PLACES];
    int i, k;
    if (nVmcPlaces < 2)
        return nVmcPlaces ? &vmcPlaces[0] : NULL;
    for (i = 0; i < nVmcPlaces; i++)
        items[i] = vmc_place_name(vmcPlaces[i].folder);
    return (k = choose(T(T_VMC_WHERE), items, nVmcPlaces, 0)) < 0 ? NULL : &vmcPlaces[k];
}

/* -------- Settings > "OPL VMC folders": the microSD's, always read, then the ones added, and "Add a folder" */

static int addKey;    /* the folder picked in the files screen: for that key */
static int added;

/* (the files screen's fbHere) △ in a folder of the device: it is the one. Given the device's root or the OPL's folder,
 * its VMC folder, when it has one */
static int folder_here(const char *dir)
{
    static dir_entry_t one[1];
    char path[300], v[120];
    const char *part = hdd_partition(dir), *in;
    size_t n;
    snprintf(path, sizeof(path), "%s", dir);   /* (it ends in a /) */
    n = strlen(path);
    if (n < 4 || strcasecmp(path + n - 4, "VMC/") != 0) {
        snprintf(path + n, sizeof(path) - n, "VMC");
        if (dir_list(path, one, 1) < 0)
            path[n] = 0;
    }
    if ((n = strlen(path)) > 1 && path[n - 1] == '/')
        path[n - 1] = 0;
    if (!strcasecmp(path, vmcPlaces[0].dir)) {   /* (the microSD's own: read already) */
        message_wait(0, NULL, COLOR_WARN, T(T_VMC_FOLDER_THERE));
        return 0;
    }
    in = strchr(path, ':') ? strchr(path, ':') + 1 : path;   /* (the path in the device, or in the partition) */
    in += *in == '/';
    if (part[0])
        addKey = VMC_APA;
    snprintf(v, sizeof(v), "%s/%s", part[0] ? part : "", in);
    if ((n = strlen(v)) > 1 && v[n - 1] == '/')
        v[n - 1] = 0;
    snprintf(cfg.vmc[addKey], sizeof(cfg.vmc[0]), "%s", v);
    config_set("vmc", vmcKeys[addKey], v);
    added = 1;
    return 1;
}

/* a device to add a folder of: the ones the files screen offers, but the microSD (its folder is always read) and the
 * ones that have a folder already. 1 = one was added */
static int add_folder(void)
{
    static const int fdevKey[FDEVS] = {-1, VMC_USB, VMC_HDD, VMC_MX4SIO};
    static const char *items[FDEVS];
    int list[FDEVS], devs[FDEVS], n = 0, i, k, nd;
    fdev_refresh();
    nd = fdev_list(list);
    for (i = 0; i < nd; i++) {
        k = fdevKey[list[i]];
        if (k >= 0 && (!cfg.vmc[k][0] || (k == VMC_HDD && !cfg.vmc[VMC_APA][0]))) {
            devs[n] = list[i];
            items[n++] = T(deviceText[list[i]]);
        }
    }
    if (!n) {
        message_wait(0, NULL, COLOR_TEXT, T(T_VMC_FOLDER_ALL));
        return 0;
    }
    if ((i = choose(T(T_VMC_FOLDER_DEVICE), items, n, 0)) < 0)
        return 0;
    addKey = fdevKey[devs[i]];
    added = 0;
    fbHere = folder_here;
    fbHereText = T_VMC_FOLDER_USE;
    files_screen(devs[i]);
    fbHere = NULL;
    return added;
}

/* how many folders are read: the microSD's and the ones added */
int vmc_places_count(void)
{
    int k, n = 1;
    for (k = 0; k < VMC_KEYS; k++)
        n += cfg.vmc[k][0] != 0;
    return n;
}

void vmc_places_screen(void)
{
    static char text[1 + VMC_KEYS + 1][160];
    static const char *items[1 + VMC_KEYS + 1];
    int keyOf[1 + VMC_KEYS + 1], n, k, at = 0, changed = 0;
    for (;;) {
        snprintf(text[0], sizeof(text[0]), "%s:  /VMC", vmc_place_name(VMC_FOLDER));
        keyOf[0] = -1;
        for (n = 1, k = 0; k < VMC_KEYS; k++)
            if (cfg.vmc[k][0]) {
                snprintf(text[n], sizeof(text[0]), "%s:  %s%s", T(keyText[k]), k == VMC_APA ? "/" : "", cfg.vmc[k]);
                keyOf[n++] = k;
            }
        snprintf(text[n], sizeof(text[0]), "%s", T(T_VMC_FOLDER_ADD));
        keyOf[n++] = -2;
        for (k = 0; k < n; k++)
            items[k] = text[k];
        if ((at = choose(T(T_SET_VMC_FOLDERS), items, n, at)) < 0)
            break;
        if (keyOf[at] == -1)
            message_wait(0, NULL, COLOR_TEXT, T(T_VMC_FOLDER_FIXED));
        else if (keyOf[at] == -2)
            changed |= add_folder();
        else {   /* a folder added: out of the list, asked first (its VMCs stay where they are) */
            dlg_new(COLOR_WARN, T(T_VMC_FOLDER_REMOVE_ASK));
            dlg_line(FONT_TEXT, COLOR_TEXT, 8, text[at]);
            dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_VMC_FOLDER_REMOVE_TEXT));
            dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_VMC_FOLDER_REMOVE);
            next.wide = 1;
            dlg_show();
            if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
                sound_play(SND_BACK);
                continue;
            }
            sound_play(SND_CONFIRM);
            k = keyOf[at];
            cfg.vmc[k][0] = 0;
            config_set("vmc", vmcKeys[k], "");
            changed = 1;
            at = 0;
        }
    }
    if (changed)   /* (the cards read again, with the VMCs of the folders as they are now) */
        reload_all();
}
