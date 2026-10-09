/* SD2Cloud -- the program opened after SD2Cloud: found on the sd2psx, a memory card, a USB drive, an MX4SIO or the
 * hard disk, with the drivers that device needs, and started. */
#include "platform.h"

/* ------------------------------------------------------------ the program opened after SD2Cloud

   Where it is, the device first (the notation PS2 homebrew launchers use):
     mmce?:/... mmce0:/... mmce1:/...   the sd2psx (the drivers SD2Cloud already uses)
     mc?:/... mc0:/... mc1:/...         a memory card
     mass:/... usb:/...                 a USB drive
     mx4sio:/...                        an MX4SIO
     ata:/...                           the internal HDD formatted exFAT
     hdd0:PARTITION:pfs:/...            the internal HDD formatted APA (also hdd0:PARTITION/...)
   Only the drivers of that device are loaded, after the IOP reset and only at that moment: the app itself, and IGR,
   never pay for the others. */

static int starts(const char *s, const char *prefix) { return strncasecmp(s, prefix, strlen(prefix)) == 0; }

static int deviceWrite;   /* the device about to be opened is written to (the hard disk's partition is mounted so) */

int device_of(const char *path)
{
    if (starts(path, "mc?:") || starts(path, "mc0:") || starts(path, "mc1:"))
        return DEV_MC;
    if (starts(path, "mass") || starts(path, "usb"))
        return DEV_USB;
    if (starts(path, "mx4sio"))
        return DEV_MX4SIO;
    if (starts(path, "ata"))
        return DEV_ATA;
    if (starts(path, "hdd"))
        return DEV_HDD;
    return DEV_SD;   /* mmce, host: on PCSX2, and anything else */
}

static void load_irx(const void *irx, unsigned int size, int argLen, const char *args)
{
    int ret = 0, id = SifExecModuleBuffer((void *)irx, size, argLen, args, &ret);
    if (id < 0 || ret == 1)   /* the log is closed by now: only for whoever runs it on PCSX2 with a debugger */
        printf("SD2Cloud: module not loaded (%d, %d)\n", id, ret);
}

/* waits for a file to appear (USB, MX4SIO and the HDD take a moment to be ready); 0 = it's there */
static int wait_for_file(const char *path, int ms)
{
    int t;
    for (t = 0; t <= ms; t += 100) {
        if (file_exists(path))
            return 0;
        sleep_ms(100);
    }
    return -1;
}

/* loads the drivers of the program's device and turns its path into one the IOP can read (in path); for the APA HDD
 * also the partition, as the next program expects to see it in argv[0] ("hdd0:__common:"). 0 = the file is there */
int open_device(int dev, char *path, size_t size, char *part, size_t partSize)
{
    static const char hddArgs[] = "-o\0" "4\0" "-n\0" "20";
    static const char pfsArgs[] = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
    char rest[200], *colon = strchr(path, ':');
    snprintf(rest, sizeof(rest), "%s", colon ? colon + 1 : path);
    part[0] = 0;
    switch (dev) {
    case DEV_MC:
        load_irx(sio2man_irx, size_sio2man_irx, 0, NULL);
        load_irx(mcman_irx, size_mcman_irx, 0, NULL);
        load_irx(mcserv_irx, size_mcserv_irx, 0, NULL);
        if (path[2] == '?') {   /* mc?: = the first slot that has it */
            path[2] = '0';
            if (wait_for_file(path, 0) == 0)
                return 0;
            path[2] = '1';
        }
        return wait_for_file(path, 1000);
    case DEV_USB:
    case DEV_MX4SIO:
    case DEV_ATA:
        if (dev == DEV_ATA)
            load_irx(ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
        if (dev == DEV_MX4SIO)
            load_irx(sio2man_irx, size_sio2man_irx, 0, NULL);
        if (dev == DEV_USB)
            load_irx(usbd_irx, size_usbd_irx, 0, NULL);
        load_irx(bdm_irx, size_bdm_irx, 0, NULL);
        load_irx(bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL);
        if (dev == DEV_USB)
            load_irx(usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL);
        else if (dev == DEV_MX4SIO)
            load_irx(mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL);
        else
            load_irx(ata_bd_irx, size_ata_bd_irx, 0, NULL);
        snprintf(path, size, "mass0:%s", rest);   /* the only block device loaded: the first one */
        return wait_for_file(path, 6000);
    default: {   /* DEV_HDD: hdd0:PARTITION:pfs:/path or hdd0:PARTITION/path */
        char *p = rest, *end = p + strcspn(p, ":/");
        int t;
        if (!*end)
            return -1;
        snprintf(part, partSize, "hdd0:%.*s:", (int)(end - p), p);
        p = (*end == ':' && starts(end, ":pfs:")) ? end + 5 : end;
        load_irx(ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
        load_irx(ps2atad_irx, size_ps2atad_irx, 0, NULL);
        load_irx(ps2hdd_irx, size_ps2hdd_irx, sizeof(hddArgs), hddArgs);
        load_irx(ps2fs_irx, size_ps2fs_irx, sizeof(pfsArgs), pfsArgs);
        for (t = 0; t < 30; t++) {   /* the drive takes a moment to answer */
            char block[80];
            snprintf(block, sizeof(block), "%.*s", (int)strlen(part) - 1, part);   /* without the last ':' */
            if (fileXioMount("pfs0:", block, deviceWrite ? FIO_MT_RDWR : FIO_MT_RDONLY) >= 0)
                break;
            sleep_ms(100);
        }
        snprintf(path, size, "pfs0:%s%s", *p == '/' ? "" : "/", p);
        if (wait_for_file(path, 0) != 0)
            return -1;
        snprintf(path, size, "pfs:%s%s", *p == '/' ? "" : "/", p);   /* how the ELF loader wants it */
        return 0;
    }
    }
}

void run_elf(const char *path)
{
    static char c[260], part[80];
    char *q;
    int dev = device_of(path);
    snprintf(c, sizeof(c), "%s", path);
    /* mmce?: becomes the microSD's slot */
    if ((q = strstr(c, "mmce?:")) != NULL)
        q[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
    log_msg("(end: running %s)", c);
#ifdef DEBUG_BUILD
    run_probes();
#endif
    if (dev == DEV_SD) {
        iop_cleanup(1);
        LoadELFFromFile(c, 0, NULL);
    } else {
        iop_cleanup(0);
        SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
        SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
        fileXioExit();   /* the RPC binding is from before the reset: made again */
        fileXioInit();
        if (open_device(dev, c, sizeof(c), part, sizeof(part)) == 0) {
            if (part[0])
                LoadELFFromFileWithPartition(c, part, 0, NULL);
            else
                LoadELFFromFile(c, 0, NULL);
        }
    }
    /* only comes back here if it couldn't run it: the PS2 menu */
    ExecOSD(0, NULL);
    for (;;)
        ;
}

/* ------------------------------------------------------------ SD2Cloud itself on one of those devices

   The IGR helper and the shortcut, on the memory card, carry only the sd2psx's driver. To start a program that is
   on a USB drive, an MX4SIO or the hard disk they load that device's drivers from the microSD, where SD2Cloud
   leaves them (SD2Cloud/drivers/, one file per module, under the module's name): the ones open_device loads for
   that device, which the helper loads in the same order. */

/* Written when the settings don't say they are there, of this build, for that device ([app] app_drivers) */
void drivers_store(const char *path)
{
    static const struct {
        const char *name;
        unsigned char *irx;
        unsigned int *size;
    } all[] = {{"usbd", usbd_irx, &size_usbd_irx},          {"bdm", bdm_irx, &size_bdm_irx},
               {"bdmfs_fatfs", bdmfs_fatfs_irx, &size_bdmfs_fatfs_irx}, {"usbmass_bd", usbmass_bd_irx, &size_usbmass_bd_irx},
               {"mx4sio_bd", mx4sio_bd_irx, &size_mx4sio_bd_irx}, {"ata_bd", ata_bd_irx, &size_ata_bd_irx},
               {"ps2dev9", ps2dev9_irx, &size_ps2dev9_irx},    {"ps2atad", ps2atad_irx, &size_ps2atad_irx},
               {"ps2hdd", ps2hdd_irx, &size_ps2hdd_irx},       {"ps2fs", ps2fs_irx, &size_ps2fs_irx}};
    static const char *const needs[] = {"", "", " usbd bdm bdmfs_fatfs usbmass_bd ", " bdm bdmfs_fatfs mx4sio_bd ",
                                        " ps2dev9 bdm bdmfs_fatfs ata_bd ", " ps2dev9 ps2atad ps2hdd ps2fs "};
    char mark[sizeof(cfg.app_drivers)], c[260], name[32];
    int dev = device_of(path), r = 0;
    unsigned int i;
    if (dev < DEV_USB || dev > DEV_HDD)
        return;
    snprintf(mark, sizeof(mark), "%d %s %s", dev, APP_VERSION, APP_COMMIT);
    if (!strcmp(mark, cfg.app_drivers))
        return;
    snprintf(c, sizeof(c), "%sdrivers/", dataDir);
    ensure_dir(c);
    for (i = 0; i < sizeof(all) / sizeof(all[0]) && r == 0; i++) {
        snprintf(name, sizeof(name), " %s ", all[i].name);
        if (!strstr(needs[dev], name))
            continue;
        snprintf(c, sizeof(c), "%sdrivers/%s.irx", dataDir, all[i].name);
        r = file_replace(c, all[i].irx, *all[i].size);
    }
    log_msg("the drivers of %s, on the microSD for the IGR helper: %d", path, r);
    if (r != 0)
        return;
    snprintf(cfg.app_drivers, sizeof(cfg.app_drivers), "%s", mark);
    config_set("app", "app_drivers", mark);
}

/* Is the program at that place? Its device's drivers are loaded for that, alone: the IOP is as run_elf leaves it,
 * and has to be set up again after (boot.c) */
int device_has(const char *place)
{
    static char c[260], part[80];
    iop_reset();
    SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    fileXioExit();
    fileXioInit();
    snprintf(c, sizeof(c), "%s", place);
    return open_device(device_of(c), c, sizeof(c), part, sizeof(part)) == 0;
}

/* The program on one of those devices, updated: what was downloaded is written over it there (the helper that goes
 * with it too, when it has one next to it), and it is started. The network and the screen are gone by then, as that
 * device's drivers take the IOP, so nothing can be said from here on: a file that can't be written leaves the one
 * that was there (file_replace), and that one is started */
void run_elf_update(const char *path, const buffer_t *app, const buffer_t *igr)
{
    static char c[260], part[80], f[300];
    char *slash;
    int dev = device_of(path);
    snprintf(c, sizeof(c), "%s", path);
    log_msg("(end: updating %s)", c);
    iop_cleanup(0);
    SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    fileXioExit();
    fileXioInit();
    deviceWrite = 1;
    if (open_device(dev, c, sizeof(c), part, sizeof(part)) == 0) {
        /* (the hard disk's file system is read and written as pfs0:, and named pfs: to the loader) */
        if (part[0])
            snprintf(f, sizeof(f) - 20, "pfs0:%s", strchr(c, ':') + 1);
        else
            snprintf(f, sizeof(f) - 20, "%s", c);
        file_replace(f, app->data, app->len);
        if (igr && igr->len && (slash = strrchr(f, '/')) != NULL) {
            strcpy(slash + 1, "SD2CLOUD-IGR.ELF");
            if (file_exists(f))
                file_replace(f, igr->data, igr->len);
        }
        if (part[0]) {   /* what was written is on the disk before the loader reads it: mounted again as run_elf has it */
            snprintf(f, sizeof(f), "%.*s", (int)strlen(part) - 1, part);
            fileXioUmount("pfs0:");
            fileXioMount("pfs0:", f, FIO_MT_RDONLY);
            LoadELFFromFileWithPartition(c, part, 0, NULL);
        } else
            LoadELFFromFile(c, 0, NULL);
    }
    ExecOSD(0, NULL);
    for (;;)
        ;
}
