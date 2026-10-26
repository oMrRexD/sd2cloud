/* SD2Cloud -- the automatic sync: what runs, with nobody at the controller, when a game is left by IGR. */
#include "app.h"

/* ------------------------------------------------------------ IGR */

/* Circle gives the sync up from IGR's first screen on, and each screen says so: asked at once, and yes = straight back,
 * nothing sent (what changed is still changed at the next IGR). Once the cards are sent the sync asks it itself
 * (stop_asked), while it gets online too */
static void cancel_point(void)
{
    watch_cancel();
    if (cancelLatched && confirm_cancel(T_CANCEL_TITLE, T_CANCEL_TEXT)) {
        log_msg("IGR: cancelled before the sync");
        leave(cfg.igr_return);
    }
}

/* circle pressed while a card is read: kept for the next cancel_point (mcfs_set_break; the reading goes on) */
static int keep_press(void)
{
    watch_cancel();
    return 0;
}

static void checking(int i, int n, const card_t *c)
{
    checking_progress(i, n, c);
    cancel_point();
}

void igr(void)
{
    int i, n = 0;
    u64 end;
    dlg_new(COLOR_TITLE, T(T_IGR_TITLE));
    dlg_line(FONT_TEXT, COLOR_DIM, 0, T(T_SEARCHING));
    dlg_buttons(BUTTON_CIRCLE, T_CANCEL, 0, 0);
    dlg_show();
#ifdef DEBUG_BUILD
    debug_capture_if('s');   /* (script: IGR's first screen) */
#endif
    /* the sd2psx writes the game's card to the microSD when it goes idle (or when switching to the BootCard): wait a
     * little */
    for (end = now_ms() + cfg.igr_settle * 1000; now_ms() < end; sleep_ms(50))
        cancel_point();
    if (cards_scan() < 0) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_ERROR, T(T_NO_SD));
        sleep_ms(3000);
        leave(cfg.igr_return);
    }
    if (!auto_sync_on()) {   /* no sync to do: main.c let IGR come this far for the templates alone */
        templates_after_game(1);
        leave(cfg.igr_return);
    }
    mcfs_set_break(keep_press);
    cards_check(checking);
    cards_retry();
    mcfs_set_break(NULL);
#ifdef DEBUG_BUILD
    if (debug_take('N'))   /* (script: the network's first try finds no cable, as in the menu) */
        debugNoLinkOnce = 1;
#endif
    cancel_point();
    templates_after_game(0);   /* the game cards about to be sent get their templates first: what is sent has them */
    cancel_point();
    for (i = 0; i < nCards; i++)
        n += is_selected(&cards[i], 0);
    /* at IGR what matters is getting back to OPL quickly: nothing changed = say so and return; everything went well =
     * "backup sent" with the exit sound and return. A failure stays on screen a little longer (summary_seconds), else
     * it would go unnoticed. Cancelling returns right away. */
    if (!n && !(cfg.save_backup && memcard_there())) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_TEXT, T(T_IGR_NOTHING));
        leave(cfg.igr_return);
    }
    run_backup(0, 1);   /* (with no card changed, the Memory Card's row alone: its saves may have) */
    if (backupCancelled)
        leave(cfg.igr_return);
    if (!n && !sent && !failed) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_TEXT, T(T_IGR_NOTHING));
        leave(cfg.igr_return);
    }
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
