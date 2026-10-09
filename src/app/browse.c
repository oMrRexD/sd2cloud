/* SD2Cloud -- the Files group: the folders of the microSD and of a USB drive, a .psu file imported into a card, a
 * save or a whole card written to a folder. */
#include "app.h"

struct fbGive_s fbGive;
card_t *fbCard;   /* a whole card is being copied to a device: triangle in files_screen writes it there */

/* -------- the Files group: a device's folders and files. X on a .psu file opens the save it holds, and square
 * installs it in a card right away; X on a card file opens the card it holds, and square installs it on the microSD.
 * The same screen picks the folder a save or a whole card is copied to ("Copy" on a save's page, "Copy to a device"
 * in a card's options): triangle then writes it into the folder shown */

#define FB_MAX     512    /* entries of a folder (the ones past it are left out) */
#define FB_ROWS    9
#define FB_X       70
#define FB_RIGHT   570
#define FB_Y0      114
#define FB_ROW     26
#define FB_NAME_W  360    /* a name's room: the file's size goes on its right */
#define FB_DIM     0x6F7C94   /* a file that isn't a .psu: listed, but there's nothing to do with it */

struct fb_s fb;

static int is_psu(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && !strcasecmp(name + n - 4, ".psu");
}

/* a whole card: a .mcd, a MemCard PRO2's .mc2, a .ps2, one of OPL's virtual memory cards (a .bin: so much else is
 * called that, that only a file of a size a card has counts), or the .zip "Copy to a device" writes with a card
 * inside (what the file really is gets checked when it's opened) */
static int is_card_file(const char *name, long long size)
{
    size_t n = strlen(name);
    const char *e = n > 4 ? name + n - 4 : "";
    if (!strcasecmp(e, ".bin")) {
        if ((size & (size - 1)) && size % 528 == 0)   /* with the ECC bytes: 528 for each 512 */
            size = size / 528 * 512;
        return size >= 512 * 1024 && !(size & (size - 1));
    }
    return !strcasecmp(e, ".mcd") || !strcasecmp(e, ".mc2") || !strcasecmp(e, ".ps2") || !strcasecmp(e, ".zip");
}

/* What X does with an entry of the folder shown: FB_ENTER a folder, FB_PSU or FB_CARD show what the file holds, FB_ELF
 * runs it (SD2Cloud leaves), 0 =
 * nothing (a file of no use here; or one that isn't opened while a save or a card is on its way to this folder: a
 * save's own page is waiting underneath) */
enum { FB_ENTER = 1, FB_PSU, FB_CARD, FB_ELF };
int fbExit;   /* the folders were opened from "Exit to": the program run from them is the choice kept for next time */
int fbPick;   /* or to choose a program without running it (the one to open after IGR): X on an ELF gives it, in fbPicked */
char fbPicked[200];

static int is_elf(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && !strcasecmp(name + n - 4, ".elf");
}

static int fb_opens(const dir_entry_t *e)
{
    if (e->dir)
        return FB_ENTER;
    if (is_elf(e->name) && !fbGive.c && !fbCard)
        return FB_ELF;
    if (is_psu(e->name) && !fbGive.c)
        return FB_PSU;
    if (is_card_file(e->name, e->size) && !fbGive.c && !fbCard)
        return FB_CARD;
    return 0;
}

/* with ui_lock held: they measure with the fonts */
static void fb_select(void)
{
    fb.selected[0] = 0;
    if (fb.cursor < fb.n) {
        snprintf(fb.selected, sizeof(fb.selected), "%s", fb.list[fb.cursor].name);
        utf8_fix(fb.selected, sizeof(fb.selected));
        fit(FONT_TEXT, fb.selected, sizeof(fb.selected), FB_NAME_W);
    }
}

static void fb_title(void)
{
    char path[400];
    const char *p = path, *q;
    size_t n;
    snprintf(path, sizeof(path), "/%s", fb.dir + strlen(fb.root));
    if ((n = strlen(path)) > 1)
        path[n - 1] = 0;
    utf8_fix(path, sizeof(path));
    snprintf(fb.title, sizeof(fb.title), "%s:  %s", T(deviceText[fb.dev]), p);
    while (ui_measure(FONT_TEXT, fb.title) > FB_RIGHT - FB_X && (q = strchr(p + 1, '/')) != NULL) {
        p = q;
        snprintf(fb.title, sizeof(fb.title), "%s:  ...%s", T(deviceText[fb.dev]), p);
    }
}

static void fb_scroll(void)
{
    if (fb.cursor < fb.top)
        fb.top = fb.cursor;
    if (fb.cursor >= fb.top + FB_ROWS)
        fb.top = fb.cursor - FB_ROWS + 1;
}

/* reads the folder shown; on = the name the cursor goes to (NULL = the first one) */
static void fb_load(const char *on)
{
    int n, i, cursor = 0;
    ui_lock();   /* the list isn't drawn while it's being read into */
    fb.n = 0;
    fb.loading = 1;
    ui_unlock();
    n = dir_list(fb.dir, fb.list, FB_MAX);
    for (i = 0; on && i < n; i++)
        if (!strcmp(fb.list[i].name, on))
            cursor = i;
    ui_lock();
    fb.error = n < 0;
    fb.n = n < 0 ? 0 : n;
    fb.cursor = cursor;
    fb.top = 0;
    fb_scroll();
    fb_title();
    fb_select();
    fb.loading = 0;
    ui_unlock();
    log_msg("files: %s: %d entries", fb.dir, n);
#ifdef DEBUG_BUILD
    for (i = 0; i < n && i < 60; i++)   /* what a script has to walk through to reach a file */
        log_msg("  %d: %s%s (%lld bytes)", i, fb.list[i].name, fb.list[i].dir ? "/" : "", fb.list[i].size);
#endif
}

static void fb_enter(const char *name)
{
    size_t n = strlen(fb.dir);
    if (n + strlen(name) + 2 > sizeof(fb.dir)) {
        message_wait(0, NULL, COLOR_ERROR, T(T_DIR_ERROR));
        return;
    }
    snprintf(fb.dir + n, sizeof(fb.dir) - n, "%s/", name);
    fb_load(NULL);
}

/* to the folder above, with the cursor on the one it came from. 0 = it's the device's root already */
static int fb_up(void)
{
    char from[256], *p;
    size_t n = strlen(fb.dir);
    if (n <= strlen(fb.root))
        return 0;
    fb.dir[n - 1] = 0;
    p = strrchr(fb.dir, '/');
    if (!p)
        p = strrchr(fb.dir, ':');
    p = p && p + 1 >= fb.dir + strlen(fb.root) ? p + 1 : fb.dir + strlen(fb.root);
    snprintf(from, sizeof(from), "%s", p);
    *p = 0;
    fb_load(from);
    return 1;
}

static void scene_files(float t)
{
    int i, y, lh = ui_line_height(FONT_TEXT);
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_title(FB_X, 82, fb.title, 0);
    for (i = fb.top, y = FB_Y0; i < fb.n && i < fb.top + FB_ROWS; i++, y += FB_ROW) {
        const dir_entry_t *e = &fb.list[i];
        int elf = !e->dir && is_elf(e->name);
        int usable = e->dir || elf || is_psu(e->name) || is_card_file(e->name, e->size);
        float my = y + lh / 2.0f + 1;
        if (e->dir) {   /* a small folder */
            ui_rect(FB_X, my - 8, 7, 3, 0xD9B95C, 0x58);
            ui_rect(FB_X, my - 6, 16, 12, 0xD9B95C, 0x58);
        } else if (elf)   /* a program: a small page, lit */
            ui_rect(FB_X + 3, my - 7, 10, 13, 0x8FB4E8, 0x60);
        else if (usable)
            ui_image(IMG_MINICARD, FB_X + 2, my - 8, 13, 15, 0xFFFFFF, 0x80);
        else
            ui_rect(FB_X + 3, my - 7, 10, 13, FB_DIM, 0x38);
        if (i == fb.cursor) {
            if (usable)
                look_item(FB_X + 28, y, fb.selected, 1, 0);
            else
                look_glow_text(FONT_TEXT, FB_X + 28, y, FB_DIM, 0xA8B2C6, fb.selected);
        } else {
            char name[260];
            snprintf(name, sizeof(name), "%s", e->name);
            utf8_fix(name, sizeof(name));
            ui_text_fit(FONT_TEXT, FB_X + 28, y, FB_NAME_W, usable ? COLOR_ITEM : FB_DIM, name);
        }
        if (!e->dir) {
            char s[24];
            format_size(e->size, s, sizeof(s));
            ui_text_right(FONT_SMALL, FB_RIGHT, y + 2, usable ? COLOR_DIM : FB_DIM, s);
        }
    }
    if (!fb.n && !fb.loading)
        ui_text_center(FONT_TEXT, W / 2.0f, 200, fb.error ? COLOR_WARN : COLOR_DIM, T(fb.error ? T_DIR_ERROR : T_DIR_EMPTY));
    if (fb.top > 0)
        look_arrow(W / 2.0f, FB_Y0 - 12, 0, 0x6E9AE0);
    if (fb.top + FB_ROWS < fb.n)
        look_arrow(W / 2.0f, FB_Y0 + FB_ROWS * FB_ROW + 2, 1, 0x6E9AE0);
    if (fb.n) {
        char s[24];
        snprintf(s, sizeof(s), "%d/%d", fb.cursor + 1, fb.n);
        ui_text_right(FONT_SMALL, FB_RIGHT, FB_Y0 + FB_ROWS * FB_ROW + 2, 0x8E98AA, s);
    }
    {
        /* only what can be done with the selected entry is offered */
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}};
        const dir_entry_t *e = fb.n ? &fb.list[fb.cursor] : NULL;
        int n = 1;
        if (e && fb_opens(e))
            l[n].button = BUTTON_CROSS, l[n++].text = T(fb_opens(&fb.list[fb.cursor]) == FB_ELF ? T_RUN : T_OPEN);
        if (fbCard || fbGive.c)   /* a card or a save is on its way to the folder shown */
            l[n].button = BUTTON_TRIANGLE, l[n++].text = T(T_EXPORT);
        else if (e && !e->dir && is_psu(e->name))   /* what the selected file is good for */
            l[n].button = BUTTON_SQUARE, l[n++].text = T(T_INSTALL_SAVE);
        else if (e && !e->dir && is_card_file(e->name, e->size))
            l[n].button = BUTTON_SQUARE, l[n++].text = T(T_INSTALL);
        look_legend(l, n, 0);
    }
}

/* the save a .psu file of the folder shown holds, as the saves' screens show one (its icon and the name in it, when
 * it has them). 1 = it is one (psu_close when done with it); else it is said that it isn't */
struct psu_s psu;

static int psu_open(const char *file)
{
    buffer_t iconsys = {0}, ico = {0};
    icon_t *ic;
    int r;
    snprintf(psu.path, sizeof(psu.path), "%s%s", fb.dir, file);
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    r = mcfs_psu_info(psu.path, &psu.info, &iconsys, &ico);
    log_msg("files: %s: %d (%s, %lld bytes in %d files)", psu.path, r, psu.info.folder, psu.info.bytes, psu.info.files);
    if (r != MCFS_OK) {
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_PSU));
        return 0;
    }
    ic = iconsys.len ? icon_load(&iconsys, &ico) : NULL;
    buf_free(&iconsys);
    buf_free(&ico);
    ui_lock();
    memset(&psu.v, 0, sizeof(psu.v));
    snprintf(psu.v.s.folder, sizeof(psu.v.s.folder), "%s", psu.info.folder);
    psu.v.s.when = psu.info.when;
    psu.v.icon = ic;
    psu.v.tried = 1;
    save_title(&psu.v, ic);
    ui_unlock();
    return 1;
}

void psu_close(void)
{
    ui_scene(scene_frame);   /* off the screen before the icon goes */
    ui_lock();
    icon_free(psu.v.icon);
    psu.v.icon = NULL;
    ui_unlock();
}

/* that save into a card, which is picked next; asked before it is written */
static void psu_import(void)
{
    char name[160], t[300], game[12];
    card_t *to;
    int r;
    if (!nCards && !save_game_id(psu.info.folder, game)) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS_SD));
        return;
    }
    if (!(to = choose_dest(NULL, T_IMPORT_TO, psu.info.bytes, psu.info.folder)))
        return;
    save_name(&psu.v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_CONFIRM_IMPORT), name, to->base);
    if (!confirm(t, to == &destNew ? T(T_NEWCARD_NOTE) : NULL, T_IMPORT_YES) || !(to = dest_real(to)) || card_free(to, NULL))
        return;
    save_into(to, &psu.v, T_WORKING_IMPORT);
    r = mcfs_import_psu(psu.path, to->path, save_progress);
    save_into_done();
    log_msg("import %s into %s: %d", psu.path, to->id, r);
    cards_recheck(to);
    card_back();
    if (r != MCFS_ERR_CANCELLED)
        op_result(r, T_DONE_IMPORT, to);
}

/* X on a .psu file: the page of the save it holds, from where it can be imported into a card. install = square on
 * it: straight to the card it goes into */
static void psu_screen(const char *file, int install)
{
    char name[260];
    if (!psu_open(file))
        return;
    if (install)
        psu_import();
    else {
        snprintf(name, sizeof(name), "%s", file);
        utf8_fix(name, sizeof(name));
        save_page(&psu.v, name, psu.info.bytes, psuOptions, 1);
        while (save_page_choice())
            psu_import();
    }
    psu_close();
}

/* A file of that name is already in the folder shown. 0 = circle; 1 = it is replaced; 2 = both stay: file becomes
 * "name (2).ext", or the first number after that which is free */
static int ask_existing(char *file, size_t size)
{
    char t[300], path[480], stem[72], ext[8];
    const char *dot = strrchr(file, '.');
    u32 b;
    int n;
    snprintf(t, sizeof(t), T(T_EXPORT_REPLACE), file);
    dlg_new(COLOR_TITLE, t);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_REPLACE);
    dlg_button(BUTTON_SQUARE, T_KEEP_BOTH);
    dlg_show();
    b = wait_button(PAD_CROSS | PAD_CIRCLE | PAD_SQUARE, 0);
    if (!(b & (PAD_CROSS | PAD_SQUARE)) || (b & PAD_CIRCLE)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    if (b & PAD_CROSS)
        return 1;
    snprintf(ext, sizeof(ext), "%s", dot ? dot : "");
    snprintf(stem, sizeof(stem), "%.*s", dot ? (int)(dot - file) : (int)strlen(file), file);
    for (n = 2; n < 100; n++) {
        snprintf(file, size, "%s (%d)%s", stem, n, ext);
        snprintf(path, sizeof(path), "%s%s", fb.dir, file);
        if (!file_exists(path))
            return 2;
    }
    return 0;
}

/* the name a save's .psu file has: the save's folder */
static void export_name(const char *folder, char file[48])
{
    size_t i;
    snprintf(file, 48, "%s.psu", folder);
    for (i = 0; file[i]; i++)   /* what a file's name can't have on FAT */
        if ((unsigned char)file[i] < 0x20 || strchr("\\/:*?\"<>|", file[i]))
            file[i] = '_';
}

/* 1 = the folder shown has no file of that name, or the user said what becomes of the one it has (it is replaced,
 * or kept: file then has a number after the name); 0 = circle */
static int export_free(char file[48])
{
    char path[460];
    snprintf(path, sizeof(path), "%s%s", fb.dir, file);
    return !file_exists(path) || ask_existing(file, 48);
}

/* a save of a card written as that file of the folder shown, read back and compared, under the screen of a save on
 * its way (the caller's). 1 = written */
static int export_write(const card_t *c, const char *folder, const char *file)
{
    char path[460];
    buffer_t psu = {0};
    int r, ok = 0;
    snprintf(path, sizeof(path), "%s%s", fb.dir, file);
    save_into_fixed();
    r = mcfs_export_psu(c->path, folder, &psu);
    upload_screen(400, 1000);   /* read; to be written and read back */
    if (r == MCFS_OK && !(ok = file_write_checked(path, psu.data, psu.len)))
        unlink(path);   /* half a file, or one that reads back different, is no use to anyone */
    log_msg("export %s of %s to %s (%u bytes): %d, %s", folder, c->id, path, (unsigned)psu.len, r,
            ok ? "written and read back" : "not written");
    buf_free(&psu);
    return ok;
}

/* a save of a card as a .psu file in the folder shown. 1 = written */
static int export_save(card_t *c, save_view_t *v)
{
    char name[160], file[48], t[300];
    int ok;
    export_name(v->s.folder, file);
    save_name(v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_CONFIRM_EXPORT), name);
    snprintf(name, sizeof(name), T(T_EXPORT_FILE), file);
    if (!confirm(t, name, T_EXPORT) || !export_free(file))
        return 0;
    save_into(c, v, T_WORKING_EXPORT);   /* (the card it comes from, here) */
    ok = export_write(c, v->s.folder, file);
    save_into_done();
    if (ok) {
        snprintf(t, sizeof(t), T(T_DONE_EXPORT), file);
        message_wait(0, NULL, COLOR_OK, t);
    } else
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_EXPORT));
    return ok;
}

/* the marked saves (marks.c: a save's "Copy", with a device picked as where they go), each as a .psu file in the
 * folder shown. One whose file is there already, and isn't to take its place or stay beside it, is left out.
 * 1 = done, and said: how many were written */
static int export_marked(void)
{
    char file[48], t[300];
    int i, n = 0;
    snprintf(t, sizeof(t), T(T_CONFIRM_EXPORT_N), marks.n);
    if (!confirm(t, NULL, T_EXPORT))
        return 0;
    for (i = 0; i < marks.n; i++) {
        export_name(marks.save[i].s.folder, file);
        if (!export_free(file))
            continue;
        mark_show(NULL, i, T_WORKING_EXPORT);
        n += export_write(marks.save[i].card, marks.save[i].s.folder, file);
    }
    mark_show_done();
    if (n)
        marks_report(n, marks.n - n);
    else
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_EXPORT));
    return 1;
}

static int card_export_progress(long long done, long long total)
{
    upload_screen(done, total);
#ifdef DEBUG_BUILD
    if (done * 2 >= total)
        debug_capture_if('e');
#endif
    return 0;
}

/* a whole card as a .zip in the folder shown, with the card inside as a .mcd or a .ps2 (asked, starting on the
 * format the backups use), read back and compared. 1 = written */
static int export_card(card_t *c)
{
    const char *formats[2];
    char file[72], path[480], t[300], f[100];
    int ps2, r;
    formats[0] = format_name(0);
    formats[1] = format_name(1);
    if ((ps2 = choose(c->base, formats, 2, cfg.ps2)) < 0)
        return 0;
    snprintf(file, sizeof(file), "%s.zip", c->base);
    snprintf(path, sizeof(path), "%s%s", fb.dir, file);
    snprintf(t, sizeof(t), T(T_CONFIRM_EXPORT), c->base);
    snprintf(f, sizeof(f), T(T_EXPORT_FILE), file);
    if (!confirm(t, f, T_EXPORT))
        return 0;
    if (file_exists(path)) {
        if (!ask_existing(file, sizeof(file)))
            return 0;
        snprintf(path, sizeof(path), "%s%s", fb.dir, file);
    }
    card_work(c, 1, T(T_WORKING_EXPORT), 1);
    r = card_export(c, path, ps2, card_export_progress);
    card_work_done();
    log_msg("export card %s to %s: %d", c->id, path, r);
    if (r == 0) {
        snprintf(t, sizeof(t), T(T_DONE_EXPORT), file);
        message_wait(0, NULL, COLOR_OK, t);
    } else
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_EXPORT));
    return r == 0;
}

void files_screen(int dev)
{
    if (dev == FDEV_USB) {
        message(0, NULL, COLOR_TEXT, T(T_USB_SEARCHING));
        if (usb_open(6000) != 0) {
            message_wait(0, NULL, COLOR_WARN, T(T_USB_NONE));
            return;
        }
    }
    if (!fb.list && !(fb.list = calloc(FB_MAX, sizeof(dir_entry_t))))
        return;
    fb.dev = dev;
    snprintf(fb.root, sizeof(fb.root), "%s", dev == FDEV_USB ? "mass0:/" : sdRoot);
    snprintf(fb.dir, sizeof(fb.dir), "%s", fb.root);
    fb_load(NULL);
    for (;;) {
        u32 b;
        ui_scene(scene_files);
        b = wait_nav(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_SQUARE);
        if (b & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {   /* left and right: a screenful at a time */
            int k = fb.cursor;
            if (!fb.n)
                continue;
            if (b & PAD_UP)
                k = (k + fb.n - 1) % fb.n;
            else if (b & PAD_DOWN)
                k = (k + 1) % fb.n;
            else if (b & PAD_LEFT)
                k = k >= FB_ROWS ? k - FB_ROWS : 0;
            else
                k = k + FB_ROWS < fb.n ? k + FB_ROWS : fb.n - 1;
            if (k != fb.cursor) {
                ui_lock();
                fb.cursor = k;
                fb_scroll();
                fb_select();
                ui_unlock();
                sound_play(SND_MOVE);
            }
        } else if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            if (!fb_up())
                return;
        } else if (b & PAD_CROSS) {
            char name[256];
            if (!fb.n)
                continue;
            int k = fb_opens(&fb.list[fb.cursor]);
            snprintf(name, sizeof(name), "%s", fb.list[fb.cursor].name);
            sound_play(k ? SND_CONFIRM : SND_BACK);
            if (k == FB_ENTER)
                fb_enter(name);
            else if (k == FB_PSU)
                psu_screen(name, 0);
            else if (k == FB_CARD)
                card_file_screen(name, 0);
            else if (k == FB_ELF) {   /* SD2Cloud leaves, as for a program picked in "Exit to" */
                char path[660], target[200];
                snprintf(path, sizeof(path), "%s%s", fb.dir, name);
                if (strlen(path) < sizeof(target)) {
                    target_for_ini(path, target, sizeof(target));
                    if (fbPick) {
                        snprintf(fbPicked, sizeof(fbPicked), "%s", target);
                        return;
                    }
                    if (fbExit && strcasecmp(target, cfg.manual_return) != 0) {
                        if (cfg.manual_name[0])
                            config_set("manual", "name", "");
                        config_set("manual", "return", target);
                    }
                    leave(target);
                }
            }
        } else if ((b & PAD_SQUARE) && fb.n && !fb.list[fb.cursor].dir && !fbGive.c && !fbCard) {
            char name[256];   /* the file itself, without opening it first */
            snprintf(name, sizeof(name), "%s", fb.list[fb.cursor].name);
            if (is_psu(name)) {
                sound_play(SND_CONFIRM);
                psu_screen(name, 1);
            } else if (is_card_file(name, fb.list[fb.cursor].size)) {
                sound_play(SND_CONFIRM);
                card_file_screen(name, 1);
            }
        } else if ((b & PAD_TRIANGLE) && !fb.error && fbCard) {   /* from a card's options: the whole card goes into this folder */
            sound_play(SND_CONFIRM);
            if (export_card(fbCard))
                return;
        } else if ((b & PAD_TRIANGLE) && !fb.error && fbGive.c) {   /* from a save's "Copy": into this folder */
            sound_play(SND_CONFIRM);
            if (fbGive.v ? export_save(fbGive.c, fbGive.v) : export_marked()) {
                fbGive.done = 1;
                return;   /* back to the saves they are of */
            }
        }
    }
}
