/* SD2Cloud -- Google Drive in the Files group: the main folder of the backups on Drive, browsed as a device's folders
 * are (the app only sees what it made itself there: its backups and the saves it sent). X on a file downloads it into
 * a folder of a device, picked as a save's copy picks one (triangle there). */
#include <fcntl.h>
#include <unistd.h>
#include "app.h"

#define CL_MAX    1000   /* entries of a folder (the ones past it are left out) */
#define CL_DEPTH  8
#define CL_ROWS   9
#define CL_X      70
#define CL_RIGHT  570
#define CL_Y0     114
#define CL_ROW    26
#define CL_NAME_W 360

#ifdef DEBUG_BUILD
int cloudFake;   /* (script "d": made-up folders and files, PCSX2 having no Google account) */
#endif

static struct {
    drive_node_t *list;   /* the folder shown: its folders, then its files, each by name */
    int n, cursor, top, depth, error;
    char id[CL_DEPTH][48], name[CL_DEPTH][128];
    char title[400], selected[260];
} cl;

int cloud_listed(void)
{
#ifdef DEBUG_BUILD
    if (cloudFake)
        return 1;
#endif
    return google_has_access();
}

static void cl_select(void)
{
    cl.selected[0] = 0;
    if (cl.n) {
        snprintf(cl.selected, sizeof(cl.selected), "%s", cl.list[cl.cursor].name);
        fit(FONT_TEXT, cl.selected, sizeof(cl.selected), CL_NAME_W);
    }
}

static void cl_scroll(void)
{
    if (cl.cursor < cl.top)
        cl.top = cl.cursor;
    if (cl.cursor >= cl.top + CL_ROWS)
        cl.top = cl.cursor - CL_ROWS + 1;
}

static void scene_cloud(float t)
{
    int i, y, lh = ui_line_height(FONT_TEXT);
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_title(CL_X, 82, cl.title, 0);
    for (i = cl.top, y = CL_Y0; i < cl.n && i < cl.top + CL_ROWS; i++, y += CL_ROW) {
        const drive_node_t *e = &cl.list[i];
        float my = y + lh / 2.0f + 1;
        if (e->folder) {   /* a small folder, as a device's */
            ui_rect(CL_X, my - 8, 7, 3, 0xD9B95C, 0x58);
            ui_rect(CL_X, my - 6, 16, 12, 0xD9B95C, 0x58);
        } else
            ui_image(IMG_MINICARD, CL_X + 2, my - 8, 13, 15, 0xFFFFFF, 0x80);
        if (i == cl.cursor)
            look_item(CL_X + 28, y, cl.selected, 1, 0);
        else
            ui_text_fit(FONT_TEXT, CL_X + 28, y, CL_NAME_W, COLOR_ITEM, e->name);
        if (!e->folder && e->size >= 0) {
            char s[24];
            format_size(e->size, s, sizeof(s));
            ui_text_right(FONT_SMALL, CL_RIGHT, y + 2, COLOR_DIM, s);
        }
    }
    if (!cl.n)
        ui_text_center(FONT_TEXT, W / 2.0f, 200, cl.error ? COLOR_WARN : COLOR_DIM, T(cl.error ? T_DIR_ERROR : T_DIR_EMPTY));
    if (cl.top > 0)
        look_arrow(W / 2.0f, CL_Y0 - 12, 0, 0x6E9AE0);
    if (cl.top + CL_ROWS < cl.n)
        look_arrow(W / 2.0f, CL_Y0 + CL_ROWS * CL_ROW + 2, 1, 0x6E9AE0);
    if (cl.n) {
        char s[24];
        snprintf(s, sizeof(s), "%d/%d", cl.cursor + 1, cl.n);
        ui_text_right(FONT_SMALL, CL_RIGHT, CL_Y0 + CL_ROWS * CL_ROW + 2, 0x8E98AA, s);
    }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(cl.n && !cl.list[cl.cursor].folder ? T_DOWNLOAD : T_OPEN)}};
        look_legend(l, cl.n ? 2 : 1, 0);
    }
}

/* (folders first, then files; each by name) */
static int by_name(const void *a, const void *b)
{
    const drive_node_t *x = a, *y = b;
    if (x->folder != y->folder)
        return y->folder - x->folder;
    return strcasecmp(x->name, y->name);
}

#ifdef DEBUG_BUILD
/* the made-up folders: the second layout's, with a card's backups and its saves */
static int fake_list(const char *id, drive_node_t *out, int max)
{
    static const struct {
        const char *parent, *id, *name;
        int folder;
        long long size;
    } fake[] = {
        {"root", "dev", "sd2psx", 1, 0}, {"root", "vmc", "VMCs do OPL", 1, 0}, {"root", "slot", "Memory Card 1", 1, 0},
        {"dev", "card1", "Card1", 1, 0}, {"dev", "nfs", "Need for Speed - Underground 2 [SLUS-21065]", 1, 0},
        {"card1", "card11", "Card1-1", 1, 0},
        {"card11", "z1", "Card1-1 2026-10-19 21h40.zip", 0, 412368}, {"card11", "z2", "Card1-1 2026-10-20 09h12.zip", 0, 415022},
        {"card11", "saves", "Saves", 1, 0}, {"saves", "p1", "BASLUS-21065ALPHA 2026-10-20 09h12m31.psu", 0, 6656},
    };
    int i, n = 0;
    for (i = 0; i < (int)(sizeof(fake) / sizeof(fake[0])) && n < max; i++)
        if (!strcmp(fake[i].parent, id)) {
            memset(&out[n], 0, sizeof(out[0]));
            snprintf(out[n].id, sizeof(out[0].id), "%s", fake[i].id);
            snprintf(out[n].name, sizeof(out[0].name), "%s", fake[i].name);
            out[n].folder = fake[i].folder;
            out[n].size = fake[i].size;
            n++;
        }
    return n;
}
#endif

/* the folder at the top of the path, listed; the cursor on at (a name), or on the first */
static void cl_load(const char *at)
{
    const char *parent[1] = {cl.id[cl.depth - 1]};
    int i, n, k;
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
#ifdef DEBUG_BUILD
    if (cloudFake)
        n = fake_list(parent[0], cl.list, CL_MAX);
    else
#endif
        n = google_nodes(parent, 1, cl.list, CL_MAX);
    ui_lock();
    cl.error = n < 0;
    cl.n = n < 0 ? 0 : n;
    qsort(cl.list, cl.n, sizeof(cl.list[0]), by_name);
    cl.cursor = cl.top = 0;
    for (i = 0; at && i < cl.n; i++)
        if (!strcmp(cl.list[i].name, at))
            cl.cursor = i;
    cl_scroll();
    cl_select();
    snprintf(cl.title, sizeof(cl.title), "%s: ", T(T_SET_G_DRIVE));
    for (k = 0; k < cl.depth; k++)
        snprintf(cl.title + strlen(cl.title), sizeof(cl.title) - strlen(cl.title), "/%s", cl.name[k]);
    fit(FONT_TEXT, cl.title, sizeof(cl.title), CL_RIGHT - CL_X);
    ui_unlock();
    log_msg("drive: %s, %d entries", cl.name[cl.depth - 1], cl.n);
}

/* the main folder of the backups: the second layout's (by its tag) or the first's (by its name). 0 = found */
static int cl_root(void)
{
    char key[DRIVE_KEY];
    const char *top[1] = {"root"};
    drive_node_t *found;
    int i, n;
#ifdef DEBUG_BUILD
    if (cloudFake) {
        snprintf(cl.id[0], sizeof(cl.id[0]), "root");
        snprintf(cl.name[0], sizeof(cl.name[0]), "%s", DRIVE_FOLDER_NEW);
        return 0;
    }
#endif
    drive_root_key(key);
    cl.id[0][0] = 0;
    if ((i = google_folder_key(key, NULL, NULL, cl.id[0], sizeof(cl.id[0]))) < 0)
        return -1;
    if (i == 0) {   /* (its name, as Drive has it) */
        if ((n = google_nodes(top, 1, cl.list, CL_MAX)) < 0)
            return -1;
        for (i = 0; i < n; i++)
            if (!strcmp(cl.list[i].id, cl.id[0]))
                snprintf(cl.name[0], sizeof(cl.name[0]), "%s", cl.list[i].name);
        return 0;
    }
    if ((n = google_nodes(top, 1, cl.list, CL_MAX)) < 0)
        return -1;
    for (i = 0, found = NULL; i < n; i++)
        if (cl.list[i].folder && !strcmp(cl.list[i].name, cfg.drive_folder))
            found = &cl.list[i];
    if (!found)
        return 1;
    snprintf(cl.id[0], sizeof(cl.id[0]), "%s", found->id);
    snprintf(cl.name[0], sizeof(cl.name[0]), "%s", found->name);
    return 0;
}

/* -------- a file downloaded into a folder of a device */

static drive_node_t dlFile;
static struct {
    int fd;
    long long done, total;
    u64 shown;
} dl;

static void dl_show(void)
{
    char a[24], b[24], t[160];
    format_size(dl.done, a, sizeof(a));
    format_size(dl.total, b, sizeof(b));
    snprintf(t, sizeof(t), T(T_CLOUD_DOWNLOADING), a, b);
    message(COLOR_TITLE, dlFile.name, COLOR_TEXT, t);
    dl.shown = now_ms();
}

static int dl_sink(const unsigned char *d, size_t n, void *u)
{
    (void)u;
    while (n) {
        int w = write(dl.fd, d, n > 64 * 1024 ? 64 * 1024 : n);
        if (w <= 0)
            return -1;
        d += w, n -= w, dl.done += w;
    }
    if (now_ms() - dl.shown > 500)
        dl_show();
    return 0;
}

/* (fbHere: triangle in the folder shown) 1 = done with the folders */
static int download_here(const char *dir)
{
    char path[700], part[710], t[800];
    int r;
    snprintf(path, sizeof(path), "%s%s", dir, dlFile.name);
    snprintf(part, sizeof(part), "%s.part", path);
    if (file_exists(path)) {
        snprintf(t, sizeof(t), T(T_CLOUD_EXISTS), dlFile.name);
        if (!confirm(t, NULL, T_REPLACE))
            return 0;
    }
    memset(&dl, 0, sizeof(dl));
    dl.total = dlFile.size > 0 ? dlFile.size : 0;
    dl_show();
    if ((dl.fd = open(part, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        r = -1;
    else {
#ifdef DEBUG_BUILD
        if (cloudFake) {   /* (made up: as many zeroes as it says it has) */
            static unsigned char zero[4096];
            long long left = dl.total;
            r = 0;
            while (left > 0 && r == 0) {
                size_t k = left > (long long)sizeof(zero) ? sizeof(zero) : (size_t)left;
                r = dl_sink(zero, k, NULL);
                left -= k;
            }
        } else
#endif
            r = google_download(dlFile.id, dl_sink, NULL);
        if (close(dl.fd) < 0 && r == 0)
            r = -1;
    }
    if (r == 0) {
        unlink(path);
        r = rename(part, path) == 0 ? 0 : file_move(part, path);
    }
    if (r != 0)
        unlink(part);
    log_msg("drive: %s downloaded to %s: %d (%lld bytes)", dlFile.name, dir, r, dl.done);
    if (r == 0) {   /* (the folder as the browser names it: "MMCE:  /Backups") */
        snprintf(t, sizeof(t), T(T_CLOUD_SAVED), fb.title);
        message_wait(COLOR_TITLE, dlFile.name, COLOR_OK, t);
        return 1;
    }
    snprintf(t, sizeof(t), "%s %s", T(T_CLOUD_DL_FAILED), googleError);
    message_wait(COLOR_TITLE, dlFile.name, COLOR_ERROR, t);
    return 0;
}

/* X on a file: the device, then its folder, where it goes */
static void download(const drive_node_t *f)
{
    const char *devices[FDEVS];
    int list[FDEVS], n, i;
    dlFile = *f;
    fdev_refresh();
    n = fdev_list(list);
    for (i = 0; i < n; i++)
        devices[i] = T(deviceText[list[i]]);
    if ((i = choose(T(T_CLOUD_SAVE_TO), devices, n, 0)) < 0)
        return;
    fbHere = download_here;
    fbHereText = T_SAVE_HERE;
    files_screen(list[i]);
    fbHere = NULL;
}

void cloud_screen(void)
{
    char t[400], up[128];
    int r;
    googleError[0] = 0;
#ifdef DEBUG_BUILD
    if (!cloudFake)
#endif
    if ((r = ensure_google(1)) != 0) {
        if (r > 0) {
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            message_wait(0, NULL, COLOR_ERROR, t);
        }
        return;
    }
    memset(&cl, 0, sizeof(cl));
    if (!(cl.list = malloc(sizeof(drive_node_t) * CL_MAX)))
        return;
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    if ((r = cl_root()) != 0) {
        if (r < 0)
            snprintf(t, sizeof(t), "%s %s", T(T_ERR_INTERNET), googleError);
        message_wait(0, NULL, r < 0 ? COLOR_ERROR : COLOR_TEXT, r < 0 ? t : T(T_CLOUD_EMPTY));
        free(cl.list);
        return;
    }
    cl.depth = 1;
    cl_load(NULL);
    for (;;) {
        u32 b;
        ui_scene(scene_cloud);
        b = wait_nav(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_CIRCLE);
        if (b & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {   /* left and right: a screenful at a time */
            int k = cl.cursor;
            if (!cl.n)
                continue;
            if (b & PAD_UP)
                k = (k + cl.n - 1) % cl.n;
            else if (b & PAD_DOWN)
                k = (k + 1) % cl.n;
            else if (b & PAD_LEFT)
                k = k >= CL_ROWS ? k - CL_ROWS : 0;
            else
                k = k + CL_ROWS < cl.n ? k + CL_ROWS : cl.n - 1;
            if (k != cl.cursor) {
                ui_lock();
                cl.cursor = k;
                cl_scroll();
                cl_select();
                ui_unlock();
                sound_play(SND_MOVE);
            }
        } else if (b & PAD_CIRCLE) {   /* up a folder; from the main one, out */
            sound_play(SND_BACK);
            if (cl.depth == 1)
                break;
            snprintf(up, sizeof(up), "%s", cl.name[--cl.depth]);
            cl_load(up);
        } else if ((b & PAD_CROSS) && cl.n) {
            drive_node_t *e = &cl.list[cl.cursor];
            sound_play(SND_CONFIRM);
            if (e->folder && cl.depth < CL_DEPTH) {
                snprintf(cl.id[cl.depth], sizeof(cl.id[0]), "%s", e->id);
                snprintf(cl.name[cl.depth], sizeof(cl.name[0]), "%s", e->name);
                cl.depth++;
                cl_load(NULL);
            } else if (!e->folder)
                download(e);
        }
    }
    ui_scene(scene_frame);   /* (off the screen before the list goes) */
    ui_lock();
    free(cl.list);
    cl.list = NULL;
    cl.n = 0;
    ui_unlock();
}
