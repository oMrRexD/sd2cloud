/* SD2Cloud -- the drives of the file browser besides the microSD: a USB drive, an MX4SIO, the internal hard disk.
 * Each one's drivers are loaded the first time it is asked for: the backups, and IGR, never pay for them. */
#include "platform.h"
#include <ps2_dev9_driver.h>
#include <hdd-ioctl.h>

/* ------------------------------------------------------------ the drivers

   The three go through the BDM, which names each kind of drive on its own (usb0:, mx4sio0:, ata0:), whatever order
   they came in; a BDM that only knows "mass" numbers them in that order, so its mass0: is only taken for a drive
   when no other kind was loaded. The hard disk's APA partitions are read by ps2hdd through that same ATA driver
   (ata_bd is the ATA driver with the BDM's side added): hdd0:, whose partitions are mounted with ps2fs, each at a
   pfsN: of its own while the program runs. Not through the BDM (ps2hdd-bdm): on a disk the PSBBN's installer left
   with an exFAT partition beside the APA ones, the BDM gives the disk to its MBR reader, which only hands on the
   exFAT partition, and the APA ones are never seen. The DEV9 (the network adapter's bus, which the hard disk is on
   too) comes from the same library as the network's, which loads it once for both */

static int load(const char *what, const char *name, void *irx, unsigned int size, int argLen, const char *args)
{
    int ret = 0, id;
#ifdef DEBUG_BUILD
    log_msg("%s: loading %s", what, name);
    debug_log_save();   /* (a driver that hangs the IOP takes the rescue with it: what was reached is on the microSD) */
#endif
    id = SifExecModuleBuffer(irx, size, argLen, args, &ret);
    log_msg("%s: %s loaded (%d, %d)", what, name, id, ret);
    return id < 0 || ret == 1 ? -1 : 0;
}

static int bdmKinds;   /* how many kinds of drive have their BDM driver loaded */

static int bdm_load(const char *what)
{
    static int state;   /* 0 = not yet; 1 = loaded; -1 = it failed (the ones before it are in memory already) */
    if (!state)
        state = load(what, "bdm", bdm_irx, size_bdm_irx, 0, NULL) == 0 &&
                load(what, "bdmfs_fatfs", bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL) == 0 ? 1 : -1;
    return state > 0 ? 0 : -1;
}

static int opens(const char *dir)
{
    int fd = fileXioDopen(dir);
    if (fd < 0)
        return 0;
    fileXioDclose(fd);
    return 1;
}

/* the drive's folders open, by its own name or (the only kind loaded) as mass0: */
static int bdm_root(const char *own, char *root, size_t size)
{
    if (opens(own))
        snprintf(root, size, "%s", own);
    else if (bdmKinds == 1 && opens("mass0:/"))
        snprintf(root, size, "mass0:/");
    else
        return 0;
    return 1;
}

static int loaded[3];   /* DRIVE_*: 1 = its drivers are in, -1 = they failed */
static const char *const what[3] = {"usb", "mx4sio", "hdd"};

static int drivers(int drive)
{
    static const char hddArgs[] = "-o\0" "4\0" "-n\0" "20";
    static const char pfsArgs[] = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
    int ok;
    if (loaded[drive])
        return loaded[drive] > 0 ? 0 : -1;
#ifdef DEBUG_BUILD
    debug_log_memory("before a drive's drivers");
#endif
    if (drive == DRIVE_HDD && init_dev9_driver() != DEV9_INIT_STATUS_OK) {
        log_msg("hdd: the DEV9 driver didn't load");
        loaded[drive] = -1;
        return -1;
    }
    ok = bdm_load(what[drive]) == 0;
    if (ok && drive == DRIVE_USB)
        ok = load("usb", "usbd", usbd_irx, size_usbd_irx, 0, NULL) == 0 &&
             load("usb", "usbmass_bd", usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL) == 0;
    else if (ok && drive == DRIVE_MX4SIO)
        ok = load("mx4sio", "mx4sio_bd", mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL) == 0;
    else if (ok && (ok = load("hdd", "ata_bd", ata_bd_irx, size_ata_bd_irx, 0, NULL) == 0)) {
        /* (the APA side is a part of it, not the whole: a disk with none is still read as exFAT. The drive settles
         * first: ps2hdd started at once can hang on a disk still spinning up, as NHDDL and OPL have found) */
        sleep_ms(1000);
        if (load("hdd", "ps2hdd", ps2hdd_irx, size_ps2hdd_irx, sizeof(hddArgs), hddArgs) != 0 ||
            load("hdd", "ps2fs", ps2fs_irx, size_ps2fs_irx, sizeof(pfsArgs), pfsArgs) != 0)
            log_msg("hdd: its APA partitions can't be read");
    }
    bdmKinds++;
    loaded[drive] = ok ? 1 : -1;
#ifdef DEBUG_BUILD
    debug_log_memory("after a drive's drivers");
#endif
    return ok ? 0 : -1;
}

int hddBoth;

/* a disk opened at its exFAT partition: has it APA ones too? (ps2hdd read the disk while it was loaded) */
static void hdd_apa_too(void)
{
    static iox_dirent_t e;
    int fd = fileXioDopen("hdd0:"), n = 0;
    if (fd < 0) {
        log_msg("hdd: no APA partitions (%d)", fd);
        return;
    }
    hddBoth = 1;
    while (fileXioDread(fd, &e) > 0)
        if (e.stat.mode == 0x0100 && !(e.stat.attr & 1)) {
            log_msg("hdd: APA partition %s, %u MB", e.name, (unsigned)(e.stat.size / 2048));
            n++;
        }
    fileXioDclose(fd);
    log_msg("hdd: APA too, %d partition(s)", n);
}

int drive_open(int drive, int ms, char *root, size_t size)
{
    static const char *const own[3] = {"usb0:/", "mx4sio0:/", "ata0:/"};
    int t;
    if (drivers(drive) != 0)
        return -1;
    for (t = 0;; t += 100) {   /* a drive takes a moment to be ready after its drivers load, or after it's plugged in */
        int there = bdm_root(own[drive], root, size);
        if (!there && drive == DRIVE_HDD && opens("hdd0:")) {   /* (a hard disk formatted APA: its partitions) */
            snprintf(root, size, "hdd0:");
            there = 1;
        }
        if (there) {
            log_msg("%s: there, at %s (after %d ms)", what[drive], root, t);
            if (drive == DRIVE_HDD && strcmp(root, "hdd0:") != 0)
                hdd_apa_too();   /* (an exFAT partition and APA ones, as the PSBBN's installer leaves a disk) */
            return 0;
        }
        if (t >= ms)
            break;
        sleep_ms(100);
    }
    log_msg("%s: not there", what[drive]);
    return -1;
}

/* ------------------------------------------------------------ the hard disk's partitions (APA) */

#define PFS_SLOTS 4   /* as many as ps2fs was told it may have (-m 4) */
static struct {
    char partition[33];   /* "" = free */
    unsigned use;         /* when it was last asked for: the oldest one gives its place */
    int kept;             /* never gives it: cards are read from it (hdd_mount_kept) */
} pfs[PFS_SLOTS];
static unsigned pfsUse;

static int mount(const char *partition, char *root, size_t size, int keep)
{
    char dev[8], block[48];
    int i, k = -1;
    for (i = 0; i < PFS_SLOTS; i++)
        if (!strcmp(pfs[i].partition, partition))
            break;
    if (i == PFS_SLOTS) {   /* not mounted: the first free place, or the one asked for longest ago */
        for (i = 0; i < PFS_SLOTS && pfs[i].partition[0]; i++)
            if (!pfs[i].kept && (k < 0 || pfs[i].use < pfs[k].use))
                k = i;
        if (i == PFS_SLOTS && k < 0)
            return -1;
        if (i == PFS_SLOTS)
            i = k;
        snprintf(dev, sizeof(dev), "pfs%d:", i);
        if (pfs[i].partition[0]) {
            fileXioUmount(dev);
            log_msg("hdd: %s unmounted from %s", pfs[i].partition, dev);
            pfs[i].partition[0] = 0;
        }
        snprintf(block, sizeof(block), "hdd0:%s", partition);
        if (fileXioMount(dev, block, FIO_MT_RDWR) < 0) {
            log_msg("hdd: %s couldn't be mounted", partition);
            return -1;
        }
        snprintf(pfs[i].partition, sizeof(pfs[i].partition), "%s", partition);
        log_msg("hdd: %s mounted at %s", partition, dev);
    }
    pfs[i].use = ++pfsUse;
    pfs[i].kept |= keep;
    snprintf(root, size, "pfs%d:/", i);
    return 0;
}

int hdd_mount(const char *partition, char *root, size_t size) { return mount(partition, root, size, 0); }
int hdd_mount_kept(const char *partition, char *root, size_t size) { return mount(partition, root, size, 1); }

const char *hdd_partition(const char *path)
{
    if (strncmp(path, "pfs", 3) != 0 || path[3] < '0' || path[3] >= '0' + PFS_SLOTS || path[4] != ':')
        return "";
    return pfs[path[3] - '0'].partition;
}

void drives_close(void)
{
    int i;
    char dev[8];
    if (loaded[DRIVE_HDD] > 0)
        fileXioDevctl("pfs:", PDIOC_CLOSEALL, NULL, 0, NULL, 0);
    for (i = 0; i < PFS_SLOTS; i++)
        if (pfs[i].partition[0]) {   /* (what ps2fs still holds of them is written as they are let go of) */
            snprintf(dev, sizeof(dev), "pfs%d:", i);
            fileXioUmount(dev);
            pfs[i].partition[0] = 0;
            pfs[i].kept = 0;
        }
    /* the disk at rest and the DEV9 off, as OPL leaves them: an IOP reset while ps2hdd still held the disk, with a
     * partition of it mounted, hung a console (the next program never started) */
    if (loaded[DRIVE_HDD] > 0) {
        int idle = fileXioDevctl("hdd0:", HDIOC_IDLEIMM, NULL, 0, NULL, 0);
        log_msg("hdd: let go of (idle %d, DEV9 off %d)", idle, fileXioDevctl("dev9x:", DDIOC_OFF, NULL, 0, NULL, 0));
        loaded[DRIVE_HDD] = 0;
    }
}
