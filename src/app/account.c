/* SD2Cloud -- the Google account: connecting it (the code and the QR on the TV), and its place in the settings. */
#include "app.h"

/* ------------------------------------------------------------ Google */

static struct {
    char url[128], code[32];
    int minutes;
} login;

static void scene_login(float t)
{
    int x = 64, y = 100, w = W - 128, h = 252, pad = 26, side, qx, tw, lh;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_panel(x, y, w, h);
    look_title(x + pad, y + pad, T(T_LOGIN_TITLE), 0);
    y += pad + ui_line_height(FONT_TEXT) + 16;
    tw = w - 2 * pad - 170;
    ui_text_fit(FONT_TEXT, x + pad, y, tw, COLOR_TEXT, T(T_LOGIN_OPEN));
    y += ui_line_height(FONT_TEXT) + 4;
    {   /* "google.com/device": without https:// and www. it fits in big letters, and works the same typed */
        const char *u = !strncmp(login.url, "https://", 8) ? login.url + 8 : login.url;
        if (!strncmp(u, "www.", 4))
            u += 4;
        ui_text_fit(FONT_TITLE, x + pad + 10, y, tw - 10, COLOR_ACCENT, u);
    }
    y += ui_line_height(FONT_TITLE) + 10;
    ui_text_fit(FONT_TEXT, x + pad, y, tw, COLOR_TEXT, T(T_LOGIN_ENTER));
    y += ui_line_height(FONT_TEXT) + 6;
    lh = ui_line_height(FONT_TITLE);
    ui_rect(x + pad, y, ui_measure(FONT_TITLE, login.code) + 32, lh + 12, 0x02060E, 0x50);
    ui_text_glow(FONT_TITLE, x + pad + 16, y + 6, COLOR_TITLE, 0x4A3E08, login.code);
    /* the QR on the right, its caption below it */
    qx = x + w - pad - 148;
    side = ui_qr(login.url, qx, 100 + pad, 4);
    if (side)
        ui_text_center(FONT_SMALL, qx + side / 2.0f, 100 + pad + side + 6, COLOR_DIM, T(T_LOGIN_QR));
    {
        char e[96];
        snprintf(e, sizeof(e), T(T_LOGIN_EXPIRES), login.minutes);
        ui_text(FONT_SMALL, x + pad, 100 + h - pad - ui_line_height(FONT_SMALL), COLOR_DIM, e);
    }
    {
        legend_t l = {BUTTON_CIRCLE, T(T_LATER)};
        look_legend(&l, 1, 0);
    }
}

static void show_login(const char *url, const char *code, int seconds)
{
    ui_lock();
    snprintf(login.url, sizeof(login.url), "%s", url);
    snprintf(login.code, sizeof(login.code), "%s", code);
    login.minutes = (seconds + 59) / 60;
    ui_unlock();
    ui_scene(scene_login);
#ifdef DEBUG_BUILD
    /* on PCSX2 the login screen is captured and the test stops there (see debug_capture_and_stop in system.c) */
    if (!strncmp(appDir, "host:", 5))
        debug_capture_and_stop();
#endif
}

static int cancel_login(void) { return (pad_buttons() & PAD_CIRCLE) != 0; }

/* network + access to Google. 0 = ok, -1 = the user backed out of the sign-in (nothing to say), else the id of the
 * error message */
int ensure_google(int allowLogin)
{
    int r;
    if (!networkUp) {
        message(0, NULL, COLOR_TEXT, T(T_NET_STARTING));
        if ((r = network_up()) != 0)
            return r;
        networkUp = 1;
    }
    if (google_init() != 0)
        return T_ERR_INTERNET;
    if (google_has_access()) {
        message(0, NULL, COLOR_TEXT, T(T_SIGNING_IN));
        r = google_refresh();
        if (r == 0)
            return 0;
        if (r == -1)
            return T_ERR_INTERNET;
        refreshToken[0] = 0;   /* revoked or expired */
        if (!allowLogin)
            return T_IGR_NO_LOGIN;
        message(0, NULL, COLOR_WARN, T(T_LOGIN_REVOKED));
        sleep_ms(3000);
    }
    if (!allowLogin)
        return T_IGR_NO_LOGIN;
    r = google_login(show_login, cancel_login);
    if (r != 0)
        return r;
    message(0, NULL, COLOR_OK, T(T_LOGIN_OK));
    sound_play(SND_CONFIRM);
    sleep_ms(1500);
    return 0;
}

/* connected: disconnects (after asking); not connected: signs in */
void account_screen(void)
{
    int r;
    if (!google_has_access()) {
        googleError[0] = 0;
        if ((r = ensure_google(1)) > 0) {
            char t[400];
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            message_wait(COLOR_ERROR, T(T_LOGIN_ERROR), COLOR_TEXT, t);
        } else if (r == 0)
            offer_auto_sync();
        return;
    }
    if (!confirm(T(T_LOGOUT_ASK), T(T_LOGOUT_TEXT), T_LOGOUT_YES))
        return;
    if (!networkUp) {
        message(0, NULL, COLOR_TEXT, T(T_NET_STARTING));
        networkUp = network_up() == 0;
    }
    google_logout(networkUp && google_init() == 0);
    note_igr_auto(NULL);
    message_wait(0, NULL, COLOR_OK, T(T_LOGOUT_DONE));
}

/* no Google account yet: connect now (network + the code on the TV) or later (straight to the menu, without waiting for
 * the network; the account can be connected in the settings). 1 = now */
int ask_connect(void)
{
    u32 b;
    dlg_new(COLOR_TITLE, T(T_SET_ACCOUNT));
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_CONNECT_ASK));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_CONNECT_HINT));
    /* three buttons: not now, never again (kept in sd2cloud.ini) and connect */
    next.nlegend = 3;
    next.legend[0].button = BUTTON_CIRCLE, next.legend[0].text = T(T_LATER);
    next.legend[1].button = BUTTON_TRIANGLE, next.legend[1].text = T(T_NEVER_ASK);
    next.legend[2].button = BUTTON_CROSS, next.legend[2].text = T(T_CONNECT);
    dlg_show();
    b = wait_button(PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE, 0);
    if (b & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        return 1;
    }
    sound_play(SND_BACK);
    if (b & PAD_TRIANGLE) {
        cfg.no_ask_connect = 1;
        config_set("general", "ask_connect", "no");
        message_wait(0, NULL, COLOR_TEXT, T(T_CONNECT_WHERE));
    }
    log_msg("no Google account: %s", (b & PAD_TRIANGLE) ? "don't ask again" : "later");
    return 0;
}
