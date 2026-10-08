/* SD2Cloud -- the log: kept only by the debug build and, in a sync started by IGR, for the file that tells why it
 * failed. */
#include "platform.h"

int logFd = -1;          /* host:log.txt, only when testing on PCSX2 */
#ifdef DEBUG_BUILD
buffer_t logRam;         /* on the console the log goes to the card only at the end, in a single file */
#endif

/* ------------------------------------------------------------ log */

int logSema = -1;          /* the log is written from more than one thread */

/* is anyone keeping the log? On the console only the debug build does: the release doesn't even format the lines,
 * except in a sync started by IGR, which nobody is watching: what log_msg says of that run is kept in memory, and goes
 * to a file if the sync fails (log_save_sync_error) */
static buffer_t syncLog;
static int log_kept(void)
{
#ifdef DEBUG_BUILD
    return 1;
#else
    return logFd >= 0 || igrMode;
#endif
}

void log_raw(const char *d, size_t n)
{
    if (!log_kept())
        return;
    if (logSema >= 0)
        WaitSema(logSema);
    if (logFd >= 0)
        write(logFd, d, n);
#ifdef DEBUG_BUILD
    if (logRam.len < 4 * 1024 * 1024)
        buf_append(&logRam, d, n);
#endif
    if (logSema >= 0)
        SignalSema(logSema);
}

void log_msg(const char *fmt, ...)
{
    va_list ap;
    char t[600];
    if (!log_kept())
        return;
    va_start(ap, fmt);
    vsnprintf(t, sizeof(t), fmt, ap);
    va_end(ap);
    log_raw(t, strlen(t));
    log_raw("\r\n", 2);
    if (igrMode) {   /* (only these lines: never what a transfer prints through log_raw) */
        if (logSema >= 0)
            WaitSema(logSema);
        if (syncLog.len < 32 * 1024) {
            buf_append(&syncLog, t, strlen(t));
            buf_append(&syncLog, "\r\n", 2);
        }
        if (logSema >= 0)
            SignalSema(logSema);
    }
}

void log_save_sync_error(void)
{
    char c[64];
    snprintf(c, sizeof(c), "%ssync-error.txt", dataDir);
    if (syncLog.len)
        file_write(c, syncLog.data, syncLog.len);
}
