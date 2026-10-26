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

/* -------- The ATA driver started without waiting for it. It looks for its disk as it starts: on a console with the
   network adapter and no disk in it (or PCSX2 with its disk turned off) that took it a minute, and the screen that
   looks for the hard disk can be given up meanwhile. It goes on then by itself, and the IOP's loader takes one module
   at a time: whatever loads another one waits for it first (drivers_settle) */

extern SifRpcClientData_t _lf_cd;   /* the SDK's own binding to the IOP's loader (loadfile.c) */
static struct _lf_module_buffer_load_arg bgArg __attribute__((aligned(64)));
static void *bgIop;
static int bgOn;   /* started, and not answered yet */

/* the module on its way: copied into the IOP and started there, the answer not waited for. 0 = on its way */
static int bg_start(void *irx, unsigned int size)
{
    SifDmaTransfer_t t;
    unsigned int q;
    size = (size + 15) & ~15u;
    if (SifLoadFileInit() < 0 || !(bgIop = SifAllocIopHeap(size)))
        return -1;
    t.src = irx;
    t.dest = bgIop;
    t.size = size;
    t.attr = 0;
    SifWriteBackDCache(irx, size);
    if (!(q = SifSetDma(&t, 1))) {
        SifFreeIopHeap(bgIop);
        return -1;
    }
    while (SifDmaStat(q) >= 0)
        ;
    memset(&bgArg, 0, sizeof(bgArg));
    bgArg.p.ptr = bgIop;
    if (SifCallRpc(&_lf_cd, LF_F_MOD_BUF_LOAD, SIF_RPC_M_NOWAIT, &bgArg, sizeof(bgArg), &bgArg, 8, NULL, NULL) < 0) {
        SifFreeIopHeap(bgIop);
        return -1;
    }
    bgOn = 1;
    return 0;
}

/* its answer, waited for; stop (NULL = none) gives up meanwhile, leaving it on its way. 0 = it answered (the module's
 * id and what its start returned); 1 = given up */
static int bg_wait(int (*stop)(void), int *id, int *ret)
{
    while (SifCheckStatRpc(&_lf_cd)) {
        if (stop && stop())
            return 1;
        sleep_ms(50);
    }
    InvalidDCache(&bgArg, (char *)&bgArg + 63);   /* (what the IOP wrote there, not what the cache had) */
    *id = bgArg.p.result;
    *ret = bgArg.q.modres;
    SifFreeIopHeap(bgIop);
    bgOn = 0;
    return 0;
}

/* the ATA driver: 0 = not started; 1 = on its way; 2 = started, -1 = it failed (its answer: ata_answered) */
static int ataState;

static void ata_answered(int id, int ret)
{
    log_msg("hdd: ata_bd loaded (%d, %d)", id, ret);
    ataState = id < 0 || ret == 1 ? -1 : 2;
}

void drivers_settle(void)
{
    int id, ret;
    if (bgOn && bg_wait(NULL, &id, &ret) == 0)
        ata_answered(id, ret);
}

static int load(const char *what, const char *name, void *irx, unsigned int size, int argLen, const char *args)
{
    int ret = 0, id;
    drivers_settle();
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

/* The hard disk's drivers after the BDM's: the ATA driver (started in the background when the screen can give it up:
 * stop), then the APA side. 0 = ok, -1 = failed, 1 = given up (the ATA driver goes on by itself) */
static int hdd_drivers(int (*stop)(void))
{
    static const char hddArgs[] = "-o\0" "4\0" "-n\0" "20";
    static const char pfsArgs[] = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
    int id, ret;
    if (!ataState) {
        if (!stop)
            ataState = load("hdd", "ata_bd", ata_bd_irx, size_ata_bd_irx, 0, NULL) == 0 ? 2 : -1;
        else if (bg_start(ata_bd_irx, size_ata_bd_irx) == 0) {
            ataState = 1;
            log_msg("hdd: loading ata_bd (the screen can give it up meanwhile)");
        } else
            ataState = -1;
    }
    if (ataState == 1) {
        if (bg_wait(stop, &id, &ret)) {
            log_msg("hdd: given up while ata_bd looks for its disk");
            return 1;
        }
        ata_answered(id, ret);
    }
    if (ataState < 0)
        return -1;
    /* (the APA side is a part of it, not the whole: a disk with none is still read as exFAT. The drive settles first:
     * ps2hdd started at once can hang on a disk still spinning up, as NHDDL and OPL have found) */
    sleep_ms(1000);
    if (load("hdd", "ps2hdd", ps2hdd_irx, size_ps2hdd_irx, sizeof(hddArgs), hddArgs) != 0 ||
        load("hdd", "ps2fs", ps2fs_irx, size_ps2fs_irx, sizeof(pfsArgs), pfsArgs) != 0)
        log_msg("hdd: its APA partitions can't be read");
    return 0;
}

/* 0 = they are in, -1 = they failed, 1 = given up (stop) */
static int drivers(int drive, int (*stop)(void))
{
    int ok = 1, id, ret;
    if (loaded[drive])
        return loaded[drive] > 0 ? 0 : -1;
    if (drive != DRIVE_HDD && bgOn) {   /* (the hard disk given up on: its ATA driver is still on its way) */
        if (bg_wait(stop, &id, &ret))
            return 1;
        ata_answered(id, ret);
    }
#ifdef DEBUG_BUILD
    debug_log_memory("before a drive's drivers");
#endif
    if (drive != DRIVE_HDD || !ataState) {   /* (the hard disk looked for before and given up on: from its ATA driver on) */
        if (drive == DRIVE_HDD && init_dev9_driver() != DEV9_INIT_STATUS_OK) {
            log_msg("hdd: the DEV9 driver didn't load");
            loaded[drive] = -1;
            return -1;
        }
        ok = bdm_load(what[drive]) == 0;
    }
    if (ok && drive == DRIVE_USB)
        ok = load("usb", "usbd", usbd_irx, size_usbd_irx, 0, NULL) == 0 &&
             load("usb", "usbmass_bd", usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL) == 0;
    else if (ok && drive == DRIVE_MX4SIO)
        ok = load("mx4sio", "mx4sio_bd", mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL) == 0;
    else if (ok) {
        int r = hdd_drivers(stop);
        if (r > 0)
            return 1;
        ok = r == 0;
    }
    bdmKinds += ok;   /* (one whose driver didn't load names no drive: mass0: may still be the other kind's) */
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

int drive_open(int drive, int ms, char *root, size_t size, int (*stop)(void))
{
    static const char *const own[3] = {"usb0:/", "mx4sio0:/", "ata0:/"};
    int t, r;
    if ((r = drivers(drive, stop)) != 0)
        return r > 0 ? -2 : -1;
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
        if (stop && stop()) {
            log_msg("%s: given up after %d ms", what[drive], t);
            return -2;
        }
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
    bgOn = 0;   /* (a driver still on its way goes with the IOP, which is reset next) */
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
