/* SD2Cloud -- the places on Drive in its second layout, and how the first layout's files go into them. Nothing here
 * talks to Drive: the folders' keys, where a file of the first layout goes, and the text of a batch of calls and of
 * its answer (google.c sends them; app/drive.c names the folders and moves the files). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "common.h"

#ifdef DEBUG_BUILD
const char *drivePrefix = "debug:";
#else
const char *drivePrefix = "";
#endif

#define COPY(dest, v) snprintf(dest, sizeof(dest), "%s", v)

void drive_root_key(char out[DRIVE_KEY]) { snprintf(out, DRIVE_KEY, "%sroot", drivePrefix); }

static void place(drive_place_t *p, int level, const char *parent, const char *fmt, const char *a, const char *b,
                  const char *c)
{
    char k[DRIVE_KEY];
    snprintf(k, sizeof(k), fmt, a, b, c);
    snprintf(p->key, sizeof(p->key), "%s%s", drivePrefix, k);
    snprintf(p->parent, sizeof(p->parent), "%s", parent);
    p->level = level;
}

int drive_path(const drive_card_t *c, int saves, drive_place_t out[DRIVE_DEPTH])
{
    int n = 0;
    switch (c->kind) {
    case DK_SLOT:   /* (the Memory Card in a slot: its own folder is its group's, beside the device's) */
        place(&out[n++], DL_CARD, "", "c:slot/%s", c->group, "", "");
        break;
    case DK_LIBRARY:   /* (each game's folder takes its saves: there are no others) */
        place(&out[n++], DL_GROUP, "", "g:library", "", "", "");
        place(&out[n], DL_CARD, out[n - 1].key, "c:library/%s", c->group, "", "");
        n++;
        return n;
    case DK_VMC:
        place(&out[n++], DL_GROUP, "", "g:vmc/%s", c->group, "", "");
        if (c->base[0]) {
            place(&out[n], DL_CARD, out[n - 1].key, "c:vmc/%s/%s", c->group, c->base, "");
            n++;
        }
        break;
    default:
        place(&out[n++], DL_DEVICE, "", "d:%s", c->device, "", "");
        place(&out[n], DL_GROUP, out[n - 1].key, "g:%s/%s", c->device, c->group, "");
        n++;
        if (c->base[0]) {
            place(&out[n], DL_CARD, out[n - 1].key, "c:%s/%s/%s", c->device, c->group, c->base);
            n++;
        }
        break;
    }
    if (saves && out[n - 1].level == DL_CARD) {   /* its Saves: the card's own, under its folder */
        const char *k = out[n - 1].key + strlen(drivePrefix);
        place(&out[n], DL_SAVES, out[n - 1].key, "s:%s", k + 2, "", "");
        n++;
    }
    return n;
}

/* -------- the move from the first layout */

static const drive_node_t *node(const drive_node_t *nodes, int n, const char *id)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(nodes[i].id, id))
            return &nodes[i];
    return NULL;
}

/* "G/B" -> G and B (at the last '/') */
static void split(const char *s, char *g, size_t gs, char *b, size_t bs)
{
    const char *slash = strrchr(s, '/');
    snprintf(g, gs, "%.*s", slash ? (int)(slash - s) : (int)strlen(s), s);
    snprintf(b, bs, "%s", slash ? slash + 1 : "");
}

/* the device a card's backups are of: its own .zip's description says it; else another one of its group's; else the
 * one in use */
static const char *zip_device(const drive_node_t *nodes, int n, const char *card, const char *group, const char *device)
{
    int i;
    for (i = 0; i < n; i++)
        if (!nodes[i].folder && nodes[i].device[0] && !strcmp(nodes[i].card, card))
            return nodes[i].device;
    for (i = 0; i < n; i++)
        if (!nodes[i].folder && nodes[i].device[0] && group && !strcmp(nodes[i].parent, group))
            return nodes[i].device;
    return device;
}

int drive_plan_file(const drive_node_t *nodes, int n, int file, const char *root, const char *device, drive_card_t *c,
                    int *saves)
{
    const drive_node_t *f = &nodes[file], *p = node(nodes, n, f->parent), *g = NULL;
    char owner[140], what[64];
    memset(c, 0, sizeof(*c));
    *saves = 0;
    if (f->folder || (p && p->key[0]) || (!p && strcmp(f->parent, root) != 0))
        return 0;   /* (a folder; a file in a folder of the second layout already; or not under the main folder) */
    if (p) {   /* the first layout's group: the parent, or its parent when the parent is the group's Saves */
        g = p;
        if (strcmp(p->parent, root) != 0) {
            const drive_node_t *pp = node(nodes, n, p->parent);
            if (!pp || pp->key[0] || strcmp(pp->parent, root) != 0)
                return 0;   /* (deeper than the first layout goes: not one of its files) */
            g = pp;
        }
    }
    if (!strncmp(f->card, "save:", 5)) {   /* a save sent by itself: "save:<its card>/<its folder>" */
        split(f->card + 5, owner, sizeof(owner), what, sizeof(what));
        *saves = 1;
        if (!strcmp(owner, "library")) {
            c->kind = DK_LIBRARY;
            library_game_of(what, c->group);
            return 1;
        }
    } else if (f->card[0])   /* a card's backup: "<group>/<card>" */
        COPY(owner, f->card);
    else {   /* tagged as nothing: into its group's folder, as far as it can be told */
        if (!g)
            return 0;
        COPY(c->group, g->name);
        if (!strcmp(g->name, "Library")) {
            char name[128], *cut;
            COPY(name, f->name);
            if ((cut = strpbrk(name, " .")) != NULL)
                *cut = 0;
            c->kind = DK_LIBRARY;
            library_game_of(name, c->group);
        } else if (!strncmp(g->name, "Memory Card", 11))
            c->kind = DK_SLOT;
        else if (!strncmp(g->name, "OPL-VMC", 7))
            c->kind = DK_VMC;
        else {
            c->kind = DK_DEVICE;
            COPY(c->device, zip_device(nodes, n, "", g->id, device));
        }
        return 1;
    }
    if (!strncmp(owner, "slot/", 5)) {   /* the Memory Card in a slot: its group's folder is its own */
        if (!g)
            return 0;
        c->kind = DK_SLOT;
        COPY(c->group, g->name);
        return 1;
    }
    split(owner, c->group, sizeof(c->group), c->base, sizeof(c->base));
    if (!strncmp(c->group, "OPL-VMC", 7))
        c->kind = DK_VMC;
    else {
        c->kind = DK_DEVICE;
        COPY(c->device, f->device[0] ? f->device : zip_device(nodes, n, owner, g ? g->id : NULL, device));
    }
    return 1;
}

/* -------- a batch */

size_t drive_batch_body(char *out, size_t size, const char *boundary, int n, const char *const *method,
                        const char *const *path, const char *const *json)
{
    size_t len = 0;
    int k, w;
    for (k = 0; k < n; k++) {
        w = snprintf(out + len, size - len,
                     "--%s\r\nContent-Type: application/http\r\nContent-ID: <item-%d>\r\n\r\n%s %s HTTP/1.1\r\n%s\r\n%s%s",
                     boundary, k + 1, method[k], path[k],
                     json && json[k] ? "Content-Type: application/json; charset=UTF-8\r\n" : "",
                     json && json[k] ? json[k] : "", json && json[k] ? "\r\n" : "");
        if (w < 0 || (size_t)w >= size - len)
            return 0;
        len += w;
    }
    w = snprintf(out + len, size - len, "--%s--\r\n", boundary);
    if (w < 0 || (size_t)w >= size - len)
        return 0;
    return len + w;
}

/* where s (n bytes) has t, or NULL */
static const char *find(const char *s, size_t n, const char *t)
{
    size_t k = strlen(t), i;
    for (i = 0; i + k <= n; i++)
        if (!memcmp(s + i, t, k))
            return s + i;
    return NULL;
}

int drive_batch_read(const char *body, size_t len, int n, int *status, char (*id)[48])
{
    char boundary[120];
    const char *p = body, *end = body + len, *eol;
    int k, got = 0;
    for (k = 0; k < n; k++)
        status[k] = 0, id[k][0] = 0;
    /* the boundary: the answer's first line ("--batch_..."), each part starts after one */
    while (p < end && (*p == '\r' || *p == '\n'))
        p++;
    if (!(eol = memchr(p, '\n', end - p)) || p[0] != '-' || p[1] != '-')
        return 0;
    snprintf(boundary, sizeof(boundary), "%.*s", (int)(eol - p - (eol > p && eol[-1] == '\r')), p);
    for (p = eol + 1; p < end;) {
        const char *next = find(p, end - p, boundary), *cid, *http, *json;
        size_t part = next ? (size_t)(next - p) : (size_t)(end - p);
        int which;
        if ((cid = find(p, part, "response-item-")) != NULL && (which = atoi(cid + 14)) >= 1 && which <= n &&
            (http = find(p, part, "HTTP/1.1 ")) != NULL) {
            status[which - 1] = atoi(http + 9);
            got++;
            if ((json = find(http, part - (http - p), "\r\n\r\n")) != NULL || (json = find(http, part - (http - p), "\n\n")) != NULL)
                js_get_string(json, part - (json - p), "id", id[which - 1], sizeof(id[0]));
        }
        if (!next)
            break;
        p = next + strlen(boundary);
    }
    return got;
}
