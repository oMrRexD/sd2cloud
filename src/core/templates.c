/* SD2Cloud -- templates: sets of saves the user picked, kept on the microSD to be put into cards (a card the device
 * makes for a new game comes empty; a template gives it the saves every card should have, as the network settings).
 * Each one is a folder of <data folder>/templates with a .psu file for each save. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "common.h"
#include "templates.h"

template_t templates[TPL_MAX];
int nTemplates;

static void root_dir(char *out, size_t size) { snprintf(out, size, "%stemplates/", dataDir); }

/* ------------------------------------------------------------ templates.ini
 *
 *   [templates]
 *   main = <the main template's name>
 *   [cards]
 *   <card id> = <signature of the main template the card is settled with>
 */

char tplMain[TPL_NAME + 1];
static struct {
    char id[96], sig[9];
} settled[MAX_CARDS];
static int nSettled;

static void ini_path(char *out, size_t size) { snprintf(out, size, "%stemplates/templates.ini", dataDir); }

static void on_setting(const char *section, const char *key, const char *value, void *u)
{
    (void)u;
    if (!strcmp(section, "templates") && !strcmp(key, "main"))
        snprintf(tplMain, sizeof(tplMain), "%s", value);
    else if (!strcmp(section, "cards") && nSettled < MAX_CARDS && value[0]) {
        snprintf(settled[nSettled].id, sizeof(settled[0].id), "%s", key);
        snprintf(settled[nSettled].sig, sizeof(settled[0].sig), "%s", value);
        nSettled++;
    }
}

static void read_settings(void)
{
    char path[80];
    tplMain[0] = 0;
    nSettled = 0;
    ini_path(path, sizeof(path));
    ini_read(path, on_setting, NULL);
}

int templates_main_set(void)
{
    read_settings();
    return tplMain[0] != 0;
}

int templates_save(void)
{
    buffer_t b = {0};
    char path[80], line[160], dir[64];
    int i, r;
    snprintf(line, sizeof(line), "; What SD2Cloud keeps about its templates. It writes this file itself.\n[templates]\nmain = %s\n\n[cards]\n",
             tplMain);
    r = buf_append(&b, line, strlen(line));
    for (i = 0; i < nSettled && r == 0; i++) {
        snprintf(line, sizeof(line), "%s = %s\n", settled[i].id, settled[i].sig);
        r = buf_append(&b, line, strlen(line));
    }
    root_dir(dir, sizeof(dir));
    ensure_dir(dir);
    ini_path(path, sizeof(path));
    if (r == 0)
        r = file_replace(path, b.data, b.len);
    buf_free(&b);
    return r;
}

template_t *template_main(void) { return tplMain[0] ? template_find(tplMain) : NULL; }

void template_set_main(const template_t *t) { snprintf(tplMain, sizeof(tplMain), "%s", t ? t->name : ""); }

static int by_text(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

/* of a template's saves: the folders they are of, whatever their order. Eight characters are plenty to tell two
 * templates of one user apart */
static void signature(const template_t *t, char sig[9])
{
    const char *names[TPL_SAVES];
    buffer_t b = {0};
    char hex[65] = "";
    int i;
    for (i = 0; i < t->n; i++)
        names[i] = t->saves[i].folder;
    qsort(names, t->n, sizeof(names[0]), by_text);
    for (i = 0; i < t->n; i++)
        buf_append(&b, names[i], strlen(names[i]) + 1);
    sha256_hex(b.data ? b.data : (const unsigned char *)"", b.len, hex);
    buf_free(&b);
    snprintf(sig, 9, "%.8s", hex);
}

int template_settled(const template_t *t, const char *cardId)
{
    char sig[9];
    int i;
    signature(t, sig);
    for (i = 0; i < nSettled; i++)
        if (!strcmp(settled[i].id, cardId))
            return !strcmp(settled[i].sig, sig);
    return 0;
}

void template_settle(const template_t *t, const char *cardId)
{
    int i;
    for (i = 0; i < nSettled && strcmp(settled[i].id, cardId); i++)
        ;
    if (i == MAX_CARDS)
        return;
    if (i == nSettled)
        snprintf(settled[nSettled++].id, sizeof(settled[0].id), "%s", cardId);
    signature(t, settled[i].sig);
}

void template_path(const template_t *t, int i, char *out, size_t size)
{
    char root[64];
    root_dir(root, sizeof(root));
    snprintf(out, size, "%s%s/%s", root, t->name, i < 0 ? "" : t->saves[i].file);
}

static int is_psu(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && !strcasecmp(name + n - 4, ".psu");
}

static int by_name(const void *a, const void *b) { return strcasecmp(((const template_t *)a)->name, ((const template_t *)b)->name); }

static void sum(template_t *t)
{
    int i;
    t->bytes = 0;
    for (i = 0; i < t->n; i++)
        t->bytes += t->saves[i].bytes;
}

/* what a .psu of the template's folder holds, as its save k. 0 = it is a save */
static int read_save(template_t *t, int k, const char *file)
{
    char path[400];
    mcfs_psu_t info;
    buffer_t a = {0}, b = {0};
    tpl_save_t *s = &t->saves[k];
    int r;
    snprintf(s->file, sizeof(s->file), "%s", file);
    template_path(t, k, path, sizeof(path));
    r = mcfs_psu_info(path, &info, &a, &b);
    buf_free(&a);
    buf_free(&b);
    if (r != MCFS_OK)
        return r;
    snprintf(s->folder, sizeof(s->folder), "%s", info.folder);
    s->when = info.when;
    s->bytes = info.bytes;
    return MCFS_OK;
}

static void read_saves(template_t *t)
{
    static dir_entry_t files[TPL_SAVES * 2];
    char dir[200];
    int n, i;
    template_path(t, -1, dir, sizeof(dir));
    t->n = 0;
    n = dir_list(dir, files, TPL_SAVES * 2);
    for (i = 0; i < n && t->n < TPL_SAVES; i++) {
        if (files[i].dir || !is_psu(files[i].name) || strlen(files[i].name) >= sizeof(t->saves[0].file))
            continue;
        /* (a second .psu of the same save, put there by hand: the first one is the template's) */
        if (read_save(t, t->n, files[i].name) == MCFS_OK && template_find_save(t, t->saves[t->n].folder) < 0)
            t->n++;
    }
    sum(t);
}

int templates_scan(void)
{
    static dir_entry_t dirs[TPL_MAX * 2];
    char root[64];
    int n, i;
    root_dir(root, sizeof(root));
    nTemplates = 0;
    n = dir_list(root, dirs, TPL_MAX * 2);
    for (i = 0; i < n && nTemplates < TPL_MAX; i++) {
        template_t *t = &templates[nTemplates];
        if (!dirs[i].dir || strlen(dirs[i].name) > TPL_NAME)
            continue;
        memset(t, 0, sizeof(*t));
        snprintf(t->name, sizeof(t->name), "%s", dirs[i].name);
        read_saves(t);
        nTemplates++;
    }
    qsort(templates, nTemplates, sizeof(templates[0]), by_name);
    read_settings();
    log_msg("templates: %d%s%s", nTemplates, tplMain[0] ? ", the main one is " : "", tplMain);
    return nTemplates;
}

template_t *template_find(const char *name)
{
    int i;
    for (i = 0; i < nTemplates; i++)
        if (!strcasecmp(templates[i].name, name))
            return &templates[i];
    return NULL;
}

int template_name_check(const char *name, const template_t *self)
{
    const template_t *t;
    size_t n = strlen(name), i;
    if (!n || n > TPL_NAME || name[0] == ' ' || name[n - 1] == ' ' || name[0] == '.' || name[n - 1] == '.')
        return TPL_ERR_NAME;
    for (i = 0; i < n; i++)   /* what a folder's name can't have on FAT */
        if ((unsigned char)name[i] < 0x20 || strchr("\\/:*?\"<>|", name[i]))
            return TPL_ERR_NAME;
    if ((t = template_find(name)) != NULL && t != self)
        return TPL_ERR_TAKEN;
    return !self && nTemplates >= TPL_MAX ? TPL_ERR_MANY : TPL_OK;
}

template_t *template_new(const char *name)
{
    char dir[200];
    template_t *t = &templates[nTemplates];
    dir_entry_t probe;
    if (template_name_check(name, NULL) != TPL_OK)
        return NULL;
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name);
    template_path(t, -1, dir, sizeof(dir));
    ensure_dir(dir);
    if (dir_list(dir, &probe, 1) < 0) {
        log_msg("templates: the folder of %s couldn't be made", name);
        return NULL;
    }
    nTemplates++;
    qsort(templates, nTemplates, sizeof(templates[0]), by_name);
    return template_find(name);
}

int template_find_save(const template_t *t, const char *folder)
{
    int i;
    for (i = 0; i < t->n; i++)
        if (!strcmp(t->saves[i].folder, folder))
            return i;
    return -1;
}

int template_add(template_t *t, const char *card, const char *folder)
{
    buffer_t psu = {0};
    char path[400], file[80];
    int k = template_find_save(t, folder), r, n;
    size_t i;
    if (k < 0 && t->n >= TPL_SAVES)
        return MCFS_ERR_FULL;
    if ((r = mcfs_export_psu(card, folder, &psu)) != MCFS_OK) {
        buf_free(&psu);
        return r;
    }
    if (k >= 0)
        snprintf(file, sizeof(file), "%s", t->saves[k].file);
    else {
        /* named after the save's folder; one whose name is taken (two folders that differ only in what a file's name
         * can't have) gets a number after it */
        k = t->n;
        for (n = 1; n < 100; n++) {
            if (n == 1)
                snprintf(file, sizeof(file), "%s.psu", folder);
            else
                snprintf(file, sizeof(file), "%s (%d).psu", folder, n);
            for (i = 0; file[i]; i++)
                if ((unsigned char)file[i] < 0x20 || strchr("\\/:*?\"<>|", file[i]))
                    file[i] = '_';
            snprintf(t->saves[k].file, sizeof(t->saves[k].file), "%s", file);
            template_path(t, k, path, sizeof(path));
            if (!file_exists(path))
                break;
        }
    }
    snprintf(t->saves[k].file, sizeof(t->saves[k].file), "%s", file);
    template_path(t, k, path, sizeof(path));
    /* the one it takes the place of is only let go of once the new one is written and read back */
    r = (k < t->n ? file_replace(path, psu.data, psu.len) == 0 : file_write_checked(path, psu.data, psu.len)) ? MCFS_OK : MCFS_ERR_IO;
    buf_free(&psu);
    if (r == MCFS_OK && (r = read_save(t, k, file)) == MCFS_OK && k == t->n)
        t->n++;
    if (r != MCFS_OK && k == t->n)
        unlink(path);   /* half a file is no save */
    sum(t);
    log_msg("templates: %s of %s into %s: %d", folder, card, t->name, r);
    return r;
}

int template_remove(template_t *t, int i)
{
    char path[400];
    if (i < 0 || i >= t->n)
        return -1;
    template_path(t, i, path, sizeof(path));
    if (unlink(path) != 0 && file_exists(path))
        return -1;
    log_msg("templates: %s out of %s", t->saves[i].folder, t->name);
    memmove(&t->saves[i], &t->saves[i + 1], sizeof(t->saves[0]) * (t->n - i - 1));
    t->n--;
    sum(t);
    return 0;
}

/* a template's folder and whatever it has (a file left there would keep the folder, and the template would be back
 * the next time they are listed). 0 = gone */
static int delete_folder(const template_t *t)
{
    static dir_entry_t files[TPL_SAVES * 2];
    char dir[200], path[480];
    int n, i;
    template_path(t, -1, dir, sizeof(dir));
    n = dir_list(dir, files, TPL_SAVES * 2);
    for (i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "%s%s", dir, files[i].name);
        if (files[i].dir)
            rmdir(path);
        else
            unlink(path);
    }
    dir[strlen(dir) - 1] = 0;
    rmdir(dir);
    return dir_list(dir, files, 1) >= 0 ? -1 : 0;
}

int template_delete(template_t *t)
{
    int k = (int)(t - templates);
    if (delete_folder(t) != 0) {
        log_msg("templates: %s couldn't be deleted", t->name);
        read_saves(t);
        return -1;
    }
    log_msg("templates: %s deleted", t->name);
    if (!strcasecmp(tplMain, t->name)) {   /* (the main one: there is none from now on) */
        tplMain[0] = 0;
        templates_save();
    }
    memmove(&templates[k], &templates[k + 1], sizeof(templates[0]) * (nTemplates - k - 1));
    nTemplates--;
    return 0;
}

/* The device can't rename: the template is made again under the new name, each save written and read back, and only
 * then is the old one deleted */
template_t *template_rename(template_t *t, const char *name)
{
    static template_t old;
    template_t *fresh;
    char from[400], to[400];
    int i, ok;
    if (template_name_check(name, t) != TPL_OK)
        return NULL;
    if (!strcmp(t->name, name))
        return t;
    if (!strcasecmp(t->name, name)) {   /* only its capitals change: on FAT that is the same folder */
        log_msg("templates: %s can't become %s (the same folder)", t->name, name);
        return NULL;
    }
    /* (the list may be full: the old one leaves it for the new one to be made) */
    old = *t;
    memmove(t, t + 1, sizeof(templates[0]) * (nTemplates - (t - templates) - 1));
    nTemplates--;
    ok = (fresh = template_new(name)) != NULL;
    for (i = 0; ok && i < old.n; i++) {
        buffer_t b = {0};
        template_path(&old, i, from, sizeof(from));
        fresh->saves[i] = old.saves[i];
        template_path(fresh, i, to, sizeof(to));
        ok = file_read(from, &b) == 0 && file_write_checked(to, b.data, b.len);
        buf_free(&b);
        if (ok)
            fresh->n = i + 1;
    }
    log_msg("templates: %s renamed to %s: %s", old.name, name, ok ? "ok" : "failed");
    if (!ok) {   /* the new one goes, and the old one is back in the list as it was */
        if (fresh)
            template_delete(fresh);
        templates[nTemplates++] = old;
        qsort(templates, nTemplates, sizeof(templates[0]), by_name);
        return NULL;
    }
    sum(fresh);
    if (!strcasecmp(tplMain, old.name)) {   /* (the main one stays the main one) */
        template_set_main(fresh);
        templates_save();
    }
    if (delete_folder(&old) != 0)
        log_msg("templates: the folder of %s is still there", old.name);
    return fresh;
}

int template_lacking(const template_t *t, const char *card, unsigned char lacks[TPL_SAVES], long long *bytes, long long *freeBytes)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    long long f = -1;
    int n = mcfs_list_saves(card, list, MCFS_MAX_SAVES, &f), i, k, count = 0;
    memset(lacks, 0, TPL_SAVES);
    if (bytes)
        *bytes = 0;
    if (freeBytes)
        *freeBytes = f;
    if (n < 0)
        return -1;
    for (i = 0; i < t->n; i++) {
        for (k = 0; k < n && strcmp(list[k].folder, t->saves[i].folder); k++)
            ;
        if (k < n)
            continue;
        lacks[i] = 1;
        count++;
        if (bytes)
            *bytes += t->saves[i].bytes;
    }
    return count;
}

int template_apply(const template_t *t, const char *card, int (*before)(const template_t *t, int i), mcfs_step_cb progress,
                   int *put)
{
    unsigned char lacks[TPL_SAVES];
    char path[400];
    int i, r = MCFS_OK, n = 0;
    if (put)
        *put = 0;
    if (template_lacking(t, card, lacks, NULL, NULL) < 0)
        return MCFS_ERR_IO;
    for (i = 0; i < t->n && r == MCFS_OK; i++) {
        if (!lacks[i])
            continue;
        if (before && before(t, i)) {
            r = MCFS_ERR_CANCELLED;
            break;
        }
        template_path(t, i, path, sizeof(path));
        r = mcfs_import_psu(path, card, progress);
        if (r == MCFS_OK)
            n++;
        else if (r == MCFS_ERR_EXISTS)   /* (a folder the list of saves doesn't show: it is there, and it stays) */
            r = MCFS_OK;
    }
    if (put)
        *put = n;
    log_msg("templates: %s into %s: %d save(s) put, %d", t->name, card, n, r);
    return r;
}
