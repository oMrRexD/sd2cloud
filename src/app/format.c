/* SD2Cloud -- how things are said on screen: sizes, dates, a card's status, a text cut to fit its place. */
#include "app.h"

/* ------------------------------------------------------------ small things */

void format_size(long long bytes, char *out, size_t size)
{
    if (bytes >= 10LL * 1024 * 1024)
        snprintf(out, size, "%lld MB", (bytes + 512 * 1024) / (1024 * 1024));
    else if (bytes < 1024 * 1024)
        snprintf(out, size, "%lld KB", (bytes + 1023) / 1024);
    else
        snprintf(out, size, "%lld.%lld MB", bytes / (1024 * 1024), (bytes % (1024 * 1024)) * 10 / (1024 * 1024));
}

/* the file a card's backup or copy holds: the device's own (the sd2psx's .mcd), or the card with its ECC bytes, which
 * PCSX2 opens */
const char *format_name(int ps2)
{
    static char own[32];
    snprintf(own, sizeof(own), "%s (%s)", dev->ext, dev->brief);
    return ps2 ? ".ps2 (PCSX2)" : own;
}

/* a date for the screen, as its language writes one (02/10/2026 21:10, 2026-10-02 21:10) */
static void format_when(int year, int month, int day, int hour, int minute, char *out, size_t size)
{
    char date[16];
    i18n_date(date, sizeof(date), year, month, day);
    snprintf(out, size, "%s %02d:%02d", date, hour, minute);
}

/* when a save was saved (a card's time, as mcfs_save_t has it) in the console's time zone: 12/10/2026 13:05 */
void format_save_when(unsigned long long w, char *out, size_t size)
{
    datetime_t d;
    card_time_local((int)(w >> 40), (w >> 32) & 0xFF, (w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF, &d);
    format_when(d.year, d.month, d.day, d.hour, d.minute, out, size);
}

/* the date of a backup, from its name ("<card> YYYY-MM-DD HHhMM.zip"); else from Drive's createdTime (UTC) */
void backup_when(const card_t *c, const drive_file_t *f, char *out, size_t size)
{
    int y, mo, d, h, mi;
    size_t n = strlen(c->base);
    if ((!strncmp(f->name, c->base, n) && sscanf(f->name + n, " %d-%d-%d %dh%d", &y, &mo, &d, &h, &mi) == 5) ||
        sscanf(f->created, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) == 5)
        format_when(y, mo, d, h, mi, out, size);
    else
        snprintf(out, size, "%s", f->name);
}

/* the last backup of a card, for the screen ("" = never) */
void last_backup(const card_t *c, char *out, size_t size)
{
    card_state_t *e = state_card(c->id, 0);
    int yr, mo, d, h, mi;
    out[0] = 0;
    if (e && e->sha[0] && sscanf(e->when, "%d-%d-%d %d:%d", &yr, &mo, &d, &h, &mi) == 5)
        format_when(yr, mo, d, h, mi, out, size);
}

u32 status_color(const card_t *c)
{
    return !c->included ? COLOR_DIM : c->status == ST_UP_TO_DATE ? COLOR_OK : c->status == ST_ERROR ? COLOR_ERROR : COLOR_WARN;
}

const char *status_text(const card_t *c)
{
    const card_state_t *e;
    if (c->included && c->status == ST_CHANGED && (!(e = state_card(c->id, 0)) || !e->sha[0]))
        return T(T_ST_NEW);   /* changed since it was first seen, but never synced */
    return !c->included ? T(T_ST_SKIPPED) : c->status == ST_UP_TO_DATE ? T(T_ST_UP_TO_DATE)
           : c->status == ST_CHANGED ? T(T_ST_CHANGED) : c->status == ST_ERROR ? T(T_ST_ERROR) : T(T_ST_NEW);
}

/* shortens a text with "..." until it fits maxw (with ui_lock held: it measures with the fonts) */
void fit(int font, char *s, size_t size, int maxw)
{
    size_t n = strlen(s);
    if (ui_measure(font, s) <= maxw)
        return;
    if (n > size - 4)
        n = size - 4;
    while (n > 0) {
        do
            n--;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80);   /* not in the middle of a UTF-8 letter */
        strcpy(s + n, "...");
        if (ui_measure(font, s) <= maxw)
            return;
    }
}
