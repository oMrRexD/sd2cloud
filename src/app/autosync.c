/* SD2Cloud -- the automatic sync: what runs, with nobody at the controller, when a game is left by IGR. */
#include "app.h"

/* ------------------------------------------------------------ IGR */

void igr(void)
{
    int i, n = 0;
    message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_DIM, T(T_SEARCHING));
    /* the sd2psx writes the game's card to the microSD when it goes idle (or when switching to the BootCard): wait a
     * little */
    sleep_ms(cfg.igr_settle * 1000);
    if (cards_scan() < 0) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_ERROR, T(T_NO_SD));
        sleep_ms(3000);
        leave(cfg.igr_return);
    }
    cards_check(checking_progress);
    for (i = 0; i < nCards; i++)
        n += is_selected(&cards[i], 0);
    /* at IGR what matters is getting back to OPL quickly: nothing changed = say so and return; everything went well =
     * "backup sent" with the exit sound and return. A failure stays on screen a little longer (summary_seconds), else
     * it would go unnoticed. Cancelling returns right away. */
    if (!n) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_TEXT, T(T_IGR_NOTHING));
        leave(cfg.igr_return);
    }
    run_backup(0, 1);
    if (backupCancelled)
        leave(cfg.igr_return);
    if (failed) {
        datetime_t d;
        local_time(&d);
        for (i = 0; i < nResults; i++)
            log_msg("result: %s", results[i].text);
        log_msg("automatic sync failed, %04d-%02d-%02d %02d:%02d", d.year, d.month, d.day, d.hour, d.minute);
        log_save_sync_error();   /* nobody was watching: what happened is kept, to tell why later */
        if (cfg.igr_summary > 0)
            summary_screen(cfg.igr_summary);
        leave(cfg.igr_return);
    }
    dlg_new(COLOR_OK, T(T_IGR_DONE));
    for (i = nResults > DLG_LINES ? nResults - DLG_LINES : 0; i < nResults; i++)
        dlg_line(FONT_SMALL, COLOR_TEXT, 2, results[i].text);
    dlg_show();
    leave(cfg.igr_return);
}
