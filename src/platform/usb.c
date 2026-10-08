/* SD2Cloud -- a USB drive, for the file browser: its drivers are loaded the first time it is asked for. */
#include "platform.h"

/* ------------------------------------------------------------ a USB drive, for the file browser */

static int usb_module(const char *name, void *irx, unsigned int size)
{
    int ret = 0, id = SifExecModuleBuffer(irx, size, 0, NULL, &ret);
    log_msg("usb: %s loaded (%d, %d)", name, id, ret);
    return id < 0 || ret == 1 ? -1 : 0;
}

int usb_open(int ms)
{
    static int loaded;
    int t, fd;
    if (!loaded) {   /* only when a USB drive is first asked for: the backups, and IGR, never pay for these. Once only,
                        even if one fails (the ones before it are in memory already) */
        loaded = 1;
#ifdef DEBUG_BUILD
        debug_log_memory("before the USB drivers");
#endif
        if (usb_module("usbd", usbd_irx, size_usbd_irx) != 0 || usb_module("bdm", bdm_irx, size_bdm_irx) != 0 ||
            usb_module("bdmfs_fatfs", bdmfs_fatfs_irx, size_bdmfs_fatfs_irx) != 0 ||
            usb_module("usbmass_bd", usbmass_bd_irx, size_usbmass_bd_irx) != 0)
            return -1;
#ifdef DEBUG_BUILD
        debug_log_memory("after the USB drivers");
#endif
    }
    for (t = 0;; t += 100) {   /* a drive takes a moment to be ready after its drivers load, or after it's plugged in */
        if ((fd = fileXioDopen("mass0:/")) >= 0) {
            fileXioDclose(fd);
            log_msg("usb: a drive is there (after %d ms)", t);
            return 0;
        }
        if (t >= ms)
            break;
        sleep_ms(100);
    }
    log_msg("usb: no drive (%d)", fd);
    return -1;
}
