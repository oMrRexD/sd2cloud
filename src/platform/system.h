/* SD2Cloud -- the PS2 itself: where the program is, the clock, the controller, the log, the network, the MMCE device, other programs (src/platform) */
#ifndef SYSTEM_H
#define SYSTEM_H

#include <stddef.h>
#include <tamtypes.h>

extern char appDir[200];    /* where the program lives: mmce0:/APPS/SD2Cloud/ (host:APPS/SD2Cloud/ on PCSX2) */
extern char appPath[260];   /* the program itself, as it was started: mmce?:/APPS/SD2Cloud/SD2CLOUD.ELF (mmce?: =
                               whichever slot the sd2psx is in). Kept in the settings for the IGR helper */
/* the program was started from another device (USB, MX4SIO...) and there is no SD2Cloud on the microSD to take
 * over: appDir is then where it would be on the microSD (APPS/SD2Cloud), which is where an update installs it */
extern int appElsewhere;   /* the program is on a device other than the sd2psx and a memory card (USB, MX4SIO, HDD) */
extern int appTookOver;     /* started by a copy of the program on another device, which handed over to this one */
/* the program was started from a save folder of a memory card (the Save Application System package) and there is no
 * SD2Cloud on the microSD to take over: that folder (appCardDir, as mcman names it: "/APP_SD2CLOUD") of the card in
 * that slot (appCardPort, 0 = slot 1) has the program and its IGR helper, and is what an update is written to */
extern int appOnCard, appCardPort;
extern char appCardDir[64];
extern char sdRoot[16];     /* root of the sd2psx microSD: mmce0:/ or mmce1:/ (host: on PCSX2) */
extern char dataDir[32];    /* the user's data: <sdRoot>SD2Cloud/ (sd2cloud.ini, state.ini, token.dat) */
extern int igrMode;         /* started by the IGR helper (-igr) */

void system_init(int argc, char *argv[]);
u64 now_ms(void);
void sleep_ms(int ms);
u32 pad_buttons(void);                      /* buttons held right now (PAD_*) */
u32 wait_button(u32 mask, int seconds);     /* 0 = timed out; seconds = 0 waits forever */
u32 wait_nav(u32 mask);                     /* for lists: like wait_button, but the arrows repeat while held */
u32 wait_nav_ms(u32 mask, int ms);          /* the same, giving up after ms (0 = no press) */
extern void (*idleHook)(void);              /* called over and over while one of those waits (NULL = nothing to call) */
typedef struct {
    int year, month, day, hour, minute, second;
} datetime_t;
void local_time(datetime_t *t);
/* a memory card's time (the PS2 writes it in the console clock's Japan time) in the console's time zone */
void card_time_local(int year, int month, int day, int hour, int minute, int second, datetime_t *t);
void log_raw(const char *d, size_t n);
void log_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* a sync started by IGR failed: what the log said of that run goes to <data>sync-error.txt (the last failure only) */
void log_save_sync_error(void);
int network_up(void);                       /* 0 = ok, else the id of the message to show */
void run_elf(const char *path) __attribute__((noreturn));   /* falls back to the OSD if it can't */
/* where a program to open is: the sd2psx (or PCSX2's host:), a memory card, USB, MX4SIO, the HDD exFAT or APA */
enum { DEV_SD, DEV_MC, DEV_USB, DEV_MX4SIO, DEV_ATA, DEV_HDD };
int device_of(const char *path);
#include "files.h"
void drivers_store(const char *path);       /* that device's drivers, on the microSD for the IGR helper */
void run_elf_update(const char *path, const buffer_t *app, const buffer_t *igr) __attribute__((noreturn));
/* the card the sd2psx is emulating right now: its number (0 = BootCard, a game card or a named folder) and channel.
 * -1 = couldn't ask; -2 = not on an MMCE device (testing on PCSX2) */
int mmce_active_card(int *channel);
int mmce_card_now(int *channel);            /* the same, without a word in the log (asked while the screens wait) */
int mmce_ping(void);                      /* does the device answer? < 0 = no (taken out of the console) */
extern int devicePings;                     /* it answered when it was looked for: one that never does can't be watched */
/* the device is back after being taken out: which one it is and the settings on its microSD, read again */
void system_reload(void);
/* Tell the sd2psx to emulate another card, as its own buttons do: it writes what it holds of the current one to the
 * microSD, closes it and opens the other, a moment later. Each one only asks (0 = the sd2psx took the request):
 * whether the card changed has to be seen in the slot afterwards. A numbered card (its first channel), or with boot
 * the BootCard, which the sd2psx only goes to with Autoboot on, in the channel it keeps for it; another channel of
 * the card it is on; the card of a game, by its ID, which it only goes to with Game ID on */
int mmce_set_card(int boot, int number);
int mmce_set_channel(int channel);
int mmce_set_gameid(const char *id);
/* The MMCE device the microSD is in. They all take the same commands, but each keeps the PS2 cards its own way */
typedef struct {
    const char *name;       /* what it is called on screen */
    const char *brief;      /* the same where there is little room (next to a format's name) */
    const char *cards;      /* the folder of its PS2 cards, from the root of the microSD */
    const char *ext;        /* what a card's file ends with */
    const char *numbered;   /* what the folders of its numbered cards start with: Card1, MemoryCard1 */
    int sd2psx;             /* it runs the sd2psx's firmware: boot cards in BOOT, an .ini in each folder (the channels'
                               names, how many there are), Game2Folder.ini, and SD2Cloud moves it off a card to change it */
} device_t;
extern const device_t *dev;
/* 1 = the number and channel the device gives for its card are known to mean what the sd2psx's do. Else (a device
 * SD2Cloud was never tried on) the card in use is only told by the root folder the PS2 sees in the slot */
extern int cardTold;
/* a USB drive (mass0:), for the file browser: loads its drivers the first time, then waits up to ms for the drive.
 * 0 = it's there */
int usb_open(int ms);
void go_osd(void) __attribute__((noreturn));
#ifdef DEBUG_BUILD
int debug_has_script(void);
int debug_take(char mark);                   /* the next letter of the script is this one: consume it */
int debug_digit(void);                       /* the next letter is a digit: consume it (-1 = it isn't) */
void debug_capture_if(char mark);            /* the next letter of the script is this one: capture and stop */
void debug_capture_and_stop(void) __attribute__((noreturn));
extern int debugNoLinkOnce, debugNoDhcpOnce, debugNoZero;
void debug_log_memory(const char *when);
#endif

#endif
