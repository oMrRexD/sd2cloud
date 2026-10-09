/* SD2Cloud -- what the files of src/platform share: the PS2 SDK they are written on, and what each has that another
 * one uses. What the rest of the program sees of them is in common.h. */
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdio.h>
#include <stdlib.h>
#ifdef DEBUG_BUILD
#include <malloc.h>
#include <audsrv.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <kernel.h>
#include <timer.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <elf-loader.h>
#include <strings.h>
#include <libcdvd.h>
#include <osd_config.h>
/* only for fileXioDevctl (the MMCE commands), which doesn't touch newlib's file descriptors */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <ps2_fileXio_driver.h>
#include <ps2_sio2man_driver.h>
#include <ps2_joystick_driver.h>
#include <ps2_network_driver.h>
#include <netman.h>
#include <ps2ip.h>
#include "common.h"
#include "ui.h"
#include "sound.h"


extern unsigned char mcman_irx[];
extern unsigned int size_mcman_irx;
extern unsigned char mcserv_irx[];
extern unsigned int size_mcserv_irx;
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbmass_bd_irx[];
extern unsigned int size_usbmass_bd_irx;
extern unsigned char bdm_irx[];
extern unsigned int size_bdm_irx;
extern unsigned char bdmfs_fatfs_irx[];
extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned char mx4sio_bd_irx[];
extern unsigned int size_mx4sio_bd_irx;
extern unsigned char ata_bd_irx[];
extern unsigned int size_ata_bd_irx;
extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;
extern unsigned char ps2atad_irx[];
extern unsigned int size_ps2atad_irx;
extern unsigned char ps2hdd_irx[];
extern unsigned int size_ps2hdd_irx;
extern unsigned char ps2fs_irx[];
extern unsigned int size_ps2fs_irx;
extern unsigned char mmceman_irx[];
extern unsigned int size_mmceman_irx;
extern unsigned char iomanX_irx[];
extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];
extern unsigned int size_fileXio_irx;
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;

/* ------------------------------------------------------------ boot.c */
void iop_reset(void);
int has_dir(const char *root, const char *dir);
void iop_cleanup(int card);

/* ------------------------------------------------------------ debug.c */
#ifdef DEBUG_BUILD
void script_read(void);
u32 script_next_button(u32 mask);
extern char script[256];
extern int scriptPos, hasScript;
extern int nRescue, rescueSeconds, pastEnd;
void debug_ask_iop(const char *when);
void ask_netman(void *a);
void ask_heap(void *a);
void ask_files(void *a);
void ask_sound(void *a);
void ask(const char *who, void (*f)(void *), int n);
extern volatile int askDone;
extern int askResult;
#endif
#ifdef DEBUG_BUILD
void load_sd_drivers(int sd);
void rescue(const char *why) __attribute__((noreturn));
void on_watch_alarm(s32 id, u16 time, void *common);
void log_stack(int thread);
void watch_loop(void *arg);
void rescue_init(void);
void list_to_log(const char *dir);
void run_probes(void);
void probes_read(void);
void rescue_stop(void);
extern char rescuePaths[4][200];
extern int mainThread, watchThread, watchAlarm;
extern volatile int rescuing;
extern u64 rescueAt;
extern char probes[6][80];
extern int nProbes, probing;
#endif

/* ------------------------------------------------------------ input.c */
extern char padArea[256] __attribute__((aligned(64)));
extern int padOpen;

/* ------------------------------------------------------------ launch.c */
int open_device(int dev, char *path, size_t size, char *part, size_t partSize);
int device_has(const char *place);

/* ------------------------------------------------------------ log.c */
extern int logFd;
#ifdef DEBUG_BUILD
extern buffer_t logRam;
#endif
extern int logSema;

/* ------------------------------------------------------------ mmce.c */
void find_device(void);

#endif
