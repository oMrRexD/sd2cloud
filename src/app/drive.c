/* SD2Cloud -- the backups on Drive, in the layout in use (core/drivemap.h): which one it is, each card's folder and
 * its Saves' in the second one, found or made; and the move from the first layout to the second, offered once at
 * startup to whoever has backups in the first, and in the settings until it is done. */
#include "app.h"

#ifdef DEBUG_BUILD
#define DRY debugDry
#else
#define DRY 0
#endif

#define COPY(dest, v) snprintf(dest, sizeof(dest), "%s", v)

/* -------- the layout */

int drive_layout(void) { return cfg.drive_layout == 2 ? 2 : 1; }

/* does the app remember backups of the first layout? (its main folder's id, or a card's last backup) */
static int first_layout_used(void) { return state_folder(cfg.drive_folder) != NULL || !state_empty(); }

void drive_layout_decide(void)
{
    if (cfg.drive_layout || first_layout_used())
        return;   /* (said in sd2cloud.ini already; or the first one's, until the move to the second is offered) */
    cfg.drive_layout = 2;
    config_set("drive", "layout", "2");
}

/* the main folder's name in the second layout: the one in sd2cloud.ini, or SD2Cloud for the first layout's own */
static void root_name(char *out, size_t size)
{
    const char *f = cfg.drive_folder;
    size_t n = strlen(DRIVE_FOLDER_OLD);
    if (!strncmp(f, DRIVE_FOLDER_OLD, n) && (!f[n] || !strcmp(f + n, " (debug)")))
        snprintf(out, size, "%s%s", DRIVE_FOLDER_NEW, f + n);
    else
        snprintf(out, size, "%s", f);
}

/* -------- the folders of the second layout */

/* the ones found or made in this run, not asked about again */
static struct {
    char key[DRIVE_KEY], id[48];
} seen[64];
static int nSeen;

static const char *seen_id(const char *key)
{
    int i;
    for (i = 0; i < nSeen; i++)
        if (!strcmp(seen[i].key, key))
            return seen[i].id;
    return NULL;
}

static void seen_set(const char *key, const char *id)
{
    int i = nSeen < (int)(sizeof(seen) / sizeof(seen[0])) ? nSeen++ : (int)(sizeof(seen) / sizeof(seen[0])) - 1;
    COPY(seen[i].key, key);
    COPY(seen[i].id, id);
}

void drive_session_start(void) { nSeen = 0; }

/* a game's folder: "<its name> [<its ID>]" (only the ID when the name isn't known; the Library's saves of no game,
 * "Other saves") */
static void game_place_name(const char *id, char *out, size_t size)
{
    char title[96];
    if (!strcmp(id, LIBRARY_OTHER))
        snprintf(out, size, "%s", T(T_LIBRARY_OTHER));
    else if (game_title(id, title, sizeof(title)))
        snprintf(out, size, "%s [%s]", title, id);
    else
        snprintf(out, size, "%s", id);
}

/* a folder's name, in the screens' language */
static void place_name(const drive_card_t *c, const drive_place_t *p, char *out, size_t size)
{
    switch (p->level) {
    case DL_DEVICE:
        snprintf(out, size, "%s", c->device);
        break;
    case DL_GROUP:
        if (c->kind == DK_LIBRARY)
            snprintf(out, size, "%s", T(T_TAB_LIBRARY));
        else if (c->kind == DK_VMC && !strcmp(c->group, VMC_FOLDER))
            snprintf(out, size, "%s", T(T_TAB_VMC));
        else if (c->kind == DK_VMC)   /* (the VMCs of a folder added in the settings: its device's) */
            snprintf(out, size, "%s (%s)", T(T_TAB_VMC), vmc_place_name(c->group));
        else   /* (a game's ID: its name; a numbered card's group, or one of the user's: as it is) */
            game_place_name(c->group, out, size);
        break;
    case DL_CARD:
        if (c->kind == DK_SLOT)
            snprintf(out, size, "%s", c->group);
        else if (c->kind == DK_LIBRARY)
            game_place_name(c->group, out, size);
        else
            snprintf(out, size, "%s", c->base);
        break;
    default:
        snprintf(out, size, "%s", T(T_DRIVE_SAVES));
        break;
    }
}

/* what a card is, for where it goes (save: the save's folder, for the Library's, whose saves go into their game's;
 * NULL = the Library's own folder) */
static void card_of(const card_t *c, const char *save, drive_card_t *d)
{
    memset(d, 0, sizeof(*d));
    if (mcfs_slot_port(c->path) >= 0) {
        d->kind = DK_SLOT;
        COPY(d->group, c->folder);
    } else if (library_path(c->path)) {
        d->kind = DK_LIBRARY;
        if (save)
            library_game_of(save, d->group);
    } else {
        d->kind = c->type == TYPE_VMC ? DK_VMC : DK_DEVICE;
        COPY(d->device, dev->name);
        COPY(d->group, c->folder);
        COPY(d->base, c->base);
    }
}

/* the main folder: found by its tag, or made in My Drive. 0 = its id in id */
static int root_folder(char *id, size_t size)
{
    char key[DRIVE_KEY], name[80];
    const char *known, *cache;
    int r;
    drive_root_key(key);
    if ((known = seen_id(key)) != NULL) {
        snprintf(id, size, "%s", known);
        return 0;
    }
    snprintf(id, size, "%s", (cache = state_folder("v2:root")) ? cache : "");
    root_name(name, sizeof(name));
    if ((r = google_folder_key(key, name, "root", id, size)) != 0)
        return -1;
    state_set_folder("v2:root", id);
    seen_set(key, id);
    return 0;
}

/* the folder of place k of the card's path: known in this run, or remembered (and still there), or found by its tag,
 * or made in its parent's (made first, in turn). 0 = its id in id */
static int place_folder(const drive_card_t *c, const drive_place_t *p, int k, char *id, size_t size)
{
    char cacheKey[DRIVE_KEY + 4], name[160], parent[48];
    const char *known = seen_id(p[k].key), *cache;
    int r;
    if (known) {
        snprintf(id, size, "%s", known);
        return 0;
    }
    snprintf(cacheKey, sizeof(cacheKey), "v2:%s", p[k].key);
    snprintf(id, size, "%s", (cache = state_folder(cacheKey)) ? cache : "");
    if ((r = google_folder_key(p[k].key, NULL, NULL, id, size)) < 0)
        return -1;
    if (r == 1) {   /* not there: made, in its parent */
        if (k ? place_folder(c, p, k - 1, parent, sizeof(parent)) : root_folder(parent, sizeof(parent)))
            return -1;
        place_name(c, &p[k], name, sizeof(name));
        if (google_folder_key(p[k].key, name, parent, id, size) != 0)
            return -1;
    }
    state_set_folder(cacheKey, id);
    seen_set(p[k].key, id);
    return 0;
}

int drive_folder_of(const card_t *c, int saves, const char *save, char *id, size_t size)
{
    drive_card_t d;
    drive_place_t p[DRIVE_DEPTH];
    int n;
    card_of(c, save, &d);
    if (d.kind == DK_LIBRARY && !save) {   /* the Library's own folder (its saves go into their games') */
        n = drive_path(&d, 0, p);
        return place_folder(&d, p, 0, id, size);
    }
    n = drive_path(&d, saves, p);
    return place_folder(&d, p, n - 1, id, size);
}

/* -------- the move from the first layout to the second
 *
 * Everything in the first layout's main folder is listed (its groups' folders, their Saves), and each file goes to
 * its place in the second one (drive_plan_file): the folders it needs are made, a level at a time, and the files are
 * moved, DRIVE_BATCH at a time, each in one call of a batch. Nothing is sent again, and their names and tags stay as
 * they are. Then the first layout's folders, empty, go to Drive's trash, and its main folder is the second's (renamed,
 * and tagged). Once it is agreed to, sd2cloud.ini says the second layout is in use and that the move is under way
 * ([drive] moving = yes): if it stops halfway, what is left of it is done at the next startup. */

#define MOVE_NODES 6000
#define MOVE_PLACES 600

static drive_node_t *nodes;
static int nNodes;
static struct {
    char key[DRIVE_KEY], parent[DRIVE_KEY], name[128], id[48];
    int level;
} *places;
static int nPlaces;

/* everything under a main folder, as deep as the first layout goes (and a little more, for a move that stopped
 * halfway: the second layout's folders are in it then too) */
static int list_tree(const char *root)
{
    const char **level = malloc(sizeof(char *) * MOVE_NODES);
    int nLevel = 1, depth, i, first, n;
    if (!level)
        return -1;
    level[0] = root;
    for (depth = 0; depth < 5 && nLevel && nNodes < MOVE_NODES; depth++) {
        first = nNodes;
        if ((n = google_nodes(level, nLevel, nodes + nNodes, MOVE_NODES - nNodes)) < 0) {
            free(level);
            return -1;
        }
        nNodes += n;
        for (i = first, nLevel = 0; i < nNodes; i++)
            if (nodes[i].folder)
                level[nLevel++] = nodes[i].id;
    }
    free(level);
    return 0;
}

/* a place a file goes to: in the list (with its parent before it), once */
static int add_place(const drive_card_t *c, const drive_place_t *p)
{
    int i;
    for (i = 0; i < nPlaces; i++)
        if (!strcmp(places[i].key, p->key))
            return i;
    if (nPlaces >= MOVE_PLACES)
        return -1;
    memset(&places[nPlaces], 0, sizeof(places[0]));
    COPY(places[nPlaces].key, p->key);
    COPY(places[nPlaces].parent, p->parent);
    place_name(c, p, places[nPlaces].name, sizeof(places[0].name));
    places[nPlaces].level = p->level;
    return nPlaces++;
}

static const char *place_id(const char *key)
{
    int i;
    char root[DRIVE_KEY];
    drive_root_key(root);
    if (!key[0])
        key = root;
    for (i = 0; i < nPlaces; i++)
        if (!strcmp(places[i].key, key))
            return places[i].id[0] ? places[i].id : NULL;
    return NULL;
}

/* the screen while it goes */
static void move_step(const char *text)
{
    message(COLOR_TITLE, T(T_DRIVE_NEW_TITLE), COLOR_TEXT, text);
}

/* the calls of a batch, DRIVE_BATCH at a time: n of them, each one's method, path and JSON from make(k). Returns how
 * many went well (2xx), each one's id in ids[k] */
static int batched(int n, void (*make)(int k, char *method, char *path, char *json), char (*ids)[48], const char *what)
{
    static char method[DRIVE_BATCH][8], path[DRIVE_BATCH][300], json[DRIVE_BATCH][900], got[DRIVE_BATCH][48];
    const char *m[DRIVE_BATCH], *p[DRIVE_BATCH], *j[DRIVE_BATCH];
    int status[DRIVE_BATCH], k, i, done = 0, part;
    char t[160];
    for (k = 0; k < n; k += part) {
        part = n - k < DRIVE_BATCH ? n - k : DRIVE_BATCH;
        snprintf(t, sizeof(t), what, k + part, n);
        move_step(t);
        for (i = 0; i < part; i++) {
            json[i][0] = 0;
            make(k + i, method[i], path[i], json[i]);
            m[i] = method[i], p[i] = path[i], j[i] = json[i][0] ? json[i] : NULL;
        }
        if (google_batch(part, m, p, j, status, got) < 0)
            continue;
        for (i = 0; i < part; i++)
            if (status[i] >= 200 && status[i] < 300) {
                done++;
                if (ids)
                    snprintf(ids[k + i], sizeof(ids[0]), "%s", got[i]);
            }
    }
    return done;
}

/* (the calls) */
static int *todo, nTodo;   /* the places to make, or the nodes to move or to trash, in this round */
static char (*dest)[48];   /* each file's folder in the second layout */

static void make_folder(int k, char *method, char *path, char *json)
{
    char nj[300], kj[DRIVE_KEY + 40];
    int i = todo[k];
    strcpy(method, "POST");
    strcpy(path, "/drive/v3/files?fields=id");
    js_escape(places[i].name, nj, sizeof(nj));
    js_escape(places[i].key, kj, sizeof(kj));
    snprintf(json, 900, "{\"name\":\"%s\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"%s\"],"
             "\"appProperties\":{\"sd2cloud\":\"1\",\"folder\":\"%s\"}}", nj, place_id(places[i].parent), kj);
}

static void make_move(int k, char *method, char *path, char *json)
{
    int i = todo[k];
    strcpy(method, "PATCH");
    snprintf(path, 300, "/drive/v3/files/%s?addParents=%s&removeParents=%s&fields=id", nodes[i].id, dest[i], nodes[i].parent);
    strcpy(json, "{}");
}

static void make_trash(int k, char *method, char *path, char *json)
{
    strcpy(method, "PATCH");
    snprintf(path, 300, "/drive/v3/files/%s?fields=id", nodes[todo[k]].id);
    strcpy(json, "{\"trashed\":true}");
}

/* the move itself (the user agreed to it). 0 = all of it was done */
static int move_files(void)
{
    char r1[48] = "", r2[48] = "", key[DRIVE_KEY], name[80], json[400], kj[DRIVE_KEY + 40], nj[160], t[200];
    const char *top[1] = {"root"}, *roots[2];
    int i, k, round, nRoots = 0, moved, total = 0, left, r;
    drive_card_t c;
    drive_place_t p[DRIVE_DEPTH];
    move_step(T(T_DRIVE_MOVE_READING));
    /* the first layout's main folder (by its name, in My Drive), and the second's (by its tag) */
    if ((r = google_nodes(top, 1, nodes, MOVE_NODES)) < 0)
        return -1;
    for (i = 0; i < r; i++)
        if (nodes[i].folder && !nodes[i].key[0] && !strcmp(nodes[i].name, cfg.drive_folder))
            COPY(r1, nodes[i].id);
    drive_root_key(key);
    if (google_folder_key(key, NULL, NULL, r2, sizeof(r2)) < 0)
        return -1;
    if (!r2[0] && r1[0]) {   /* the first's main folder is the second's: renamed, and tagged */
        root_name(name, sizeof(name));
        js_escape(name, nj, sizeof(nj));
        js_escape(key, kj, sizeof(kj));
        snprintf(json, sizeof(json), "{\"name\":\"%s\",\"appProperties\":{\"sd2cloud\":\"1\",\"folder\":\"%s\"}}", nj, kj);
        if (google_patch(r1, NULL, json) != 0)
            return -1;
        COPY(r2, r1);
    }
    if (!r2[0])
        return 0;   /* (no backups on Drive: nothing to move) */
    state_set_folder("v2:root", r2);
    seen_set(key, r2);
    roots[nRoots++] = r2;
    if (r1[0] && strcmp(r1, r2) != 0)
        roots[nRoots++] = r1;
    /* everything in them; where each file goes */
    nNodes = nPlaces = 0;
    for (i = 0; i < nRoots; i++)
        if (list_tree(roots[i]) != 0)
            return -1;
    memset(&places[nPlaces], 0, sizeof(places[0]));
    drive_root_key(places[nPlaces].key);
    COPY(places[nPlaces].id, r2);
    nPlaces++;
    for (i = 0; i < nNodes; i++)   /* (the second layout's folders that are there already) */
        if (nodes[i].folder && nodes[i].key[0] && nPlaces < MOVE_PLACES) {
            memset(&places[nPlaces], 0, sizeof(places[0]));
            COPY(places[nPlaces].key, nodes[i].key);
            COPY(places[nPlaces].id, nodes[i].id);
            nPlaces++;
        }
    for (i = 0, nTodo = 0; i < nNodes; i++) {
        int saves, n, at = -1, rootOf;
        dest[i][0] = 0;
        for (rootOf = 0; rootOf < nRoots; rootOf++)   /* (each file is planned against its own main folder) */
            if (drive_plan_file(nodes, nNodes, i, roots[rootOf], dev->name, &c, &saves))
                break;
        if (rootOf == nRoots)
            continue;
        n = drive_path(&c, saves, p);
        for (k = 0; k < n; k++)
            at = add_place(&c, &p[k]);
        if (at < 0)
            return -1;
        todo[nTodo++] = i;
        total++;
        snprintf(dest[i], sizeof(dest[0]), "%d", at);   /* (its place, for now: its folder's id once it is made) */
    }
    log_msg("drive: %d nodes listed, %d files to move, %d places", nNodes, total, nPlaces);
    /* the folders, a level at a time (a parent before its children) */
    for (round = 0; round < DRIVE_DEPTH; round++) {
        static char made[MOVE_PLACES][48];
        int nMake = 0;
        for (i = 0; i < nPlaces; i++)
            if (!places[i].id[0] && place_id(places[i].parent))
                todo[nMake++] = i;
        if (!nMake)
            break;
        for (i = 0; i < nMake; i++)
            made[i][0] = 0;
        batched(nMake, make_folder, made, T(T_DRIVE_MOVE_FOLDERS));
        for (i = 0; i < nMake; i++)
            COPY(places[todo[i]].id, made[i]);
    }
    for (i = 1; i < nPlaces; i++)   /* (remembered: the next sync doesn't look for them; [0] is the main folder) */
        if (places[i].id[0]) {
            char cacheKey[DRIVE_KEY + 4];
            snprintf(cacheKey, sizeof(cacheKey), "v2:%s", places[i].key);
            state_set_folder(cacheKey, places[i].id);
        }
    /* the files */
    for (i = 0, nTodo = 0; i < nNodes; i++)
        if (dest[i][0]) {
            const char *id = places[atoi(dest[i])].id;
            if (!id[0])
                return -1;   /* (its folder couldn't be made) */
            snprintf(dest[i], sizeof(dest[0]), "%s", id);
            todo[nTodo++] = i;
        }
    moved = nTodo ? batched(nTodo, make_move, NULL, T(T_DRIVE_MOVE_FILES)) : 0;
    left = nTodo - moved;
    log_msg("drive: %d of %d files moved", moved, nTodo);
    if (left) {
        snprintf(googleError, sizeof(googleError), T(T_DRIVE_MOVE_LEFT), left);
        return -1;
    }
    /* the first layout's folders, empty now, to the trash (and its main folder, when it wasn't the second's) */
    for (i = 0, nTodo = 0; i < nNodes; i++)
        if (nodes[i].folder && !nodes[i].key[0])
            todo[nTodo++] = i;
    if (nTodo)
        batched(nTodo, make_trash, NULL, T(T_DRIVE_MOVE_TIDY));
    if (nRoots > 1 && google_patch(r1, NULL, "{\"trashed\":true}") != 0)
        log_msg("drive: the first layout's main folder stays");
    snprintf(t, sizeof(t), "drive: the move is done, %d files", total);
    log_msg("%s", t);
    return 0;
}

/* the move, from the question's answer to what came of it */
static void drive_move(void)
{
    char t[400];
    int r;
    if (!cfg.drive_moving) {   /* (from here on, the second layout: what is left of the move is done next time) */
        cfg.drive_layout = 2;
        cfg.drive_moving = 1;
        config_set("drive", "layout", "2");
        config_set("drive", "moving", "yes");
    }
    if (DRY) {
        log_msg("drive: the move to the second layout (dry)");
        r = 0;
    } else {
        googleError[0] = 0;
        if ((r = ensure_google(1)) != 0) {
            if (r > 0) {
                snprintf(t, sizeof(t), "%s %s", T(r), googleError);
                message_wait(0, NULL, COLOR_ERROR, t);
            }
            return;
        }
        drive_session_start();
        nodes = malloc(sizeof(drive_node_t) * MOVE_NODES);
        places = malloc(sizeof(places[0]) * MOVE_PLACES);
        todo = malloc(sizeof(int) * MOVE_NODES);
        dest = malloc(sizeof(dest[0]) * MOVE_NODES);
        r = nodes && places && todo && dest ? move_files() : -1;
        free(nodes);
        free(places);
        free(todo);
        free(dest);
        nodes = NULL, places = NULL, todo = NULL, dest = NULL;
        state_write();
    }
    if (r != 0) {
        snprintf(t, sizeof(t), "%s %s", T(T_DRIVE_MOVE_FAILED), googleError);
        message_wait(COLOR_TITLE, T(T_DRIVE_NEW_TITLE), COLOR_ERROR, t);
        return;
    }
    cfg.drive_moving = 0;
    cfg.drive_asked = 1;
    config_set("drive", "moving", "no");
    config_set("drive", "asked", "yes");
    message_wait(COLOR_TITLE, T(T_DRIVE_NEW_TITLE), COLOR_OK, T(T_DRIVE_MOVE_DONE));
}

/* the question: what the second layout is, and that the move takes little and sends nothing again. later = at
 * startup (circle: "Not now"; the settings keep it); else from the settings (circle: back). 1 = yes */
static int ask_move(int later)
{
    dlg_new(COLOR_TITLE, T(T_DRIVE_NEW_TITLE));
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_DRIVE_NEW_TEXT));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(later ? T_DRIVE_NEW_LATER : T_DRIVE_NEW_NOTE));
    dlg_buttons(BUTTON_CIRCLE, later ? T_LATER : T_BACK, BUTTON_CROSS, T_DRIVE_ORGANIZE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    return 1;
}

void drive_offer_startup(void)
{
#ifdef DEBUG_BUILD
    if (debug_take('m')) {   /* (script "m": offered as to a user of the first layout, and moved dry: PCSX2 has no
                                Google account) */
        log_msg("[script] m: the move to the second layout, offered");
        cfg.drive_layout = cfg.drive_asked = 0;
        debugDry = 1;
        if (ask_move(1))
            drive_move();
        else
            drive_offer_declined();
        debugDry = 0;
        return;
    }
#endif
    if (!google_has_access())
        return;
    if (cfg.drive_moving) {   /* (agreed to already: what is left of it) */
        drive_move();
        return;
    }
    if (cfg.drive_layout || cfg.drive_asked)
        return;
    if (ask_move(1))
        drive_move();
    else
        drive_offer_declined();
}

void drive_offer_declined(void)
{
    cfg.drive_layout = 1;
    cfg.drive_asked = 1;
    config_set("drive", "layout", "1");
    config_set("drive", "asked", "yes");
}

void drive_offer_settings(void)
{
    if (ask_move(0))
        drive_move();
}
