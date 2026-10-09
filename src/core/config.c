/* SD2Cloud -- sd2cloud.ini (written by the user). Without the file, or without a key, the default applies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "common.h"

config_t cfg;
int configExists;

#define COPY(dest, v) snprintf(dest, sizeof(dest), "%s", v)

static int parse_types(const char *v)
{
    int t = 0;
    char copy[128], *p, *save;
    COPY(copy, v);
    for (p = strtok_r(copy, ", ", &save); p; p = strtok_r(NULL, ", ", &save)) {
        if (!strcasecmp(p, "normal")) t |= TYPE_NORMAL;
        else if (!strcasecmp(p, "gameid")) t |= TYPE_GAMEID;
        else if (!strcasecmp(p, "boot")) t |= TYPE_BOOT;
        else if (!strcasecmp(p, "named")) t |= TYPE_NAMED;
        else if (!strcasecmp(p, "all")) t |= TYPE_NORMAL | TYPE_GAMEID | TYPE_BOOT | TYPE_NAMED;
    }
    return t;
}

static void on_config(const char *s, const char *k, const char *v, void *u)
{
    (void)u;
    if (!strcasecmp(s, "general")) {
        if (!strcasecmp(k, "language")) COPY(cfg.language, v);
        else if (!strcasecmp(k, "ask_connect")) cfg.no_ask_connect = !strcasecmp(v, "no");
        else if (!strcasecmp(k, "drive_folder") && *v && !strpbrk(v, "'\"\\")) {
            COPY(cfg.drive_folder, v);
            utf8_fix(cfg.drive_folder, sizeof(cfg.drive_folder));
        }
    } else if (!strcasecmp(s, "cards")) {
        if (!strcasecmp(k, "mode")) cfg.list_mode = !strcasecmp(v, "list");
        else if (!strcasecmp(k, "types")) cfg.types = parse_types(v);
        else if (!strcasecmp(k, "include")) COPY(cfg.include, v);
        else if (!strcasecmp(k, "exclude")) COPY(cfg.exclude, v);
        else if (!strcasecmp(k, "format")) cfg.ps2 = !strcasecmp(v, "ps2") || !strcasecmp(v, ".ps2");
    } else if (!strcasecmp(s, "retention")) {
        int n = atoi(v);
        if (n < 0)
            n = 0;
        if (!strcasecmp(k, "default"))
            cfg.keep = n;
        else if (cfg.n_rules < MAX_RULES) {
            COPY(cfg.rules[cfg.n_rules].id, k);
            cfg.rules[cfg.n_rules++].keep = n;
        }
    } else if (!strcasecmp(s, "igr")) {
        if (!strcasecmp(k, "return")) COPY(cfg.igr_return, v);
        else if (!strcasecmp(k, "name")) {
            COPY(cfg.igr_name, v);
            utf8_fix(cfg.igr_name, sizeof(cfg.igr_name));
        } else if (!strcasecmp(k, "auto_sync")) cfg.no_auto_sync = !strcasecmp(v, "no");
        else if (!strcasecmp(k, "settle_seconds")) cfg.igr_settle = atoi(v);
        else if (!strcasecmp(k, "summary_seconds")) cfg.igr_summary = atoi(v);
    } else if (!strcasecmp(s, "app")) {   /* written by the app itself */
        if (!strcasecmp(k, "app_path")) COPY(cfg.app_path, v);
        else if (!strcasecmp(k, "app_build")) COPY(cfg.app_build, v);
        else if (!strcasecmp(k, "app_drivers")) COPY(cfg.app_drivers, v);
        else if (!strcasecmp(k, "app_version")) COPY(cfg.app_version, v);
        else if (!strcasecmp(k, "igr_auto")) COPY(cfg.igr_auto, v);
    } else if (!strcasecmp(s, "manual")) {
        if (!strcasecmp(k, "return")) COPY(cfg.manual_return, v);
        else if (!strcasecmp(k, "name")) {
            COPY(cfg.manual_name, v);
            utf8_fix(cfg.manual_name, sizeof(cfg.manual_name));
        }
    } else if (!strcasecmp(s, "templates")) {
        if (!strcasecmp(k, "after_game")) cfg.no_tpl_after_game = !strcasecmp(v, "no");
    } else if (!strcasecmp(s, "update")) {
        if (!strcasecmp(k, "repo") && *v && !strpbrk(v, " '\"\\")) COPY(cfg.repo, v);
        else if (!strcasecmp(k, "channel")) cfg.beta = !strcasecmp(v, "beta");
    }
}

void config_read(void)
{
    char path[260];
    memset(&cfg, 0, sizeof(cfg));
    COPY(cfg.language, "auto");
    COPY(cfg.drive_folder, "PS2 Memory Card Backups");
    /* by default: CardN, the game cards (Game ID spreads the saves) and the named ones (the user's). The BootCards are
     * left out: the factory microSD comes with 8 exploit cards (FMCB, PS2BBL...), which would take time for nothing;
     * whoever wants them adds "boot" */
    cfg.types = TYPE_NORMAL | TYPE_GAMEID | TYPE_NAMED;
    cfg.keep = 10;
    COPY(cfg.igr_return, "auto");
    cfg.igr_settle = 3;
    cfg.igr_summary = 3;
    COPY(cfg.app_path, "mmce?:/APPS/SD2Cloud/SD2CLOUD.ELF");   /* where the IGR helper looks without being told */
    COPY(cfg.manual_return, "auto");
    COPY(cfg.repo, "oMrRexD/sd2cloud");
    snprintf(path, sizeof(path), "%ssd2cloud.ini", dataDir);
    configExists = ini_read(path, on_config, NULL) == 0;
    if (!configExists)
        log_msg("no %s: using the defaults", path);
#ifdef DEBUG_BUILD
    /* the debug build uses another Drive folder and keeps only 3, to exercise the rotation without touching real backups */
    snprintf(cfg.drive_folder + strlen(cfg.drive_folder), sizeof(cfg.drive_folder) - strlen(cfg.drive_folder), " (debug)");
    cfg.keep = 3;
    cfg.n_rules = 0;
#endif
    if (cfg.igr_settle < 0 || cfg.igr_settle > 30)
        cfg.igr_settle = 3;
    if (cfg.igr_summary < 0 || cfg.igr_summary > 60)
        cfg.igr_summary = 3;
}

/* The sd2cloud.ini the app creates when there is none: package/sd2cloud.ini, the very file that comes in the zip,
 * embedded by the build. It only has the options at their defaults; what each one means is in
 * package/sd2cloud.example.ini, which goes in the zip next to it */
extern unsigned char ini_default[];
extern unsigned int size_ini_default;

void config_write_template(void)
{
    buffer_t b = {0};
    char path[260];
    unsigned int i, from = 0;
    /* with Windows line endings, so it opens properly in Notepad */
    for (i = 0; i <= size_ini_default; i++)
        if (i == size_ini_default || ini_default[i] == '\n' || ini_default[i] == '\r') {
            buf_append(&b, ini_default + from, i - from);
            if (i < size_ini_default && ini_default[i] == '\n')
                buf_append(&b, "\r\n", 2);
            from = i + 1;
        }
    snprintf(path, sizeof(path), "%ssd2cloud.ini", dataDir);
    ensure_data_dir();
    if (file_replace(path, b.data, b.len) == 0) {
        configExists = 1;
        log_msg("created %s", path);
    }
    buf_free(&b);
}

/* inserts s at pos in the buffer */
static void insert_at(buffer_t *b, size_t pos, const char *s)
{
    size_t n = strlen(s), tail = b->len - pos;
    if (buf_append(b, s, n) != 0)
        return;
    memmove(b->data + pos + n, b->data + pos, tail);
    memcpy(b->data + pos, s, n);
}

/* changes one setting in sd2cloud.ini and keeps everything else as the user left it (comments, other keys, rules).
 * A key that isn't there goes at the end of its section; a missing section, at the end of the file. Without the file,
 * the commented one is created first. 0 = written */
int config_set(const char *section, const char *key, const char *value)
{
    char path[260];
    int r;
    snprintf(path, sizeof(path), "%ssd2cloud.ini", dataDir);
    if (!file_exists(path))
        config_write_template();
    r = file_exists(path) ? ini_set(path, section, key, value) : -1;
    log_msg("settings: [%s] %s = %s (%d)", section, key, value, r);
    return r;
}

int ini_set(const char *path, const char *section, const char *key, const char *value)
{
    char line[300], l[300];
    buffer_t in = {0}, out = {0};
    const char *p, *end;
    size_t klen = strlen(key), after = 0;
    int inSection = 0, seen = 0, done = 0, r;
    /* a file that isn't there yet starts empty; one that is there and can't be read is left alone, never written
     * again from nothing (a folder's .ini has its channels' names) */
    if (file_read(path, &in) != 0 && file_exists(path)) {
        buf_free(&in);
        return -1;
    }
    snprintf(line, sizeof(line), "%s = %s\r\n", key, value);
    for (p = (const char *)in.data, end = p + in.len; p && p < end;) {
        const char *nl = memchr(p, '\n', end - p);
        size_t len = nl ? (size_t)(nl - p + 1) : (size_t)(end - p);
        snprintf(l, sizeof(l), "%.*s", (int)(len < sizeof(l) ? len : sizeof(l) - 1), p);
        trim(l);
        if (l[0] == '[') {
            if (inSection && !done) {   /* the section ends without the key: it goes after its last line */
                insert_at(&out, after, line);
                done = 1;
            }
            inSection = !strncasecmp(l + 1, section, strlen(section)) && l[1 + strlen(section)] == ']';
            seen |= inSection;
        } else if (inSection && !done && l[0] != ';' && l[0] != '#' && !strncasecmp(l, key, klen)) {
            const char *q = l + klen;
            while (*q == ' ' || *q == '\t')
                q++;
            if (*q == '=') {   /* the key itself (not one that only starts the same way): replaced */
                buf_append(&out, line, strlen(line));
                done = 1;
                p += len;
                continue;
            }
        }
        buf_append(&out, p, len);
        if (inSection && (l[0] == '[' || (l[0] && l[0] != ';' && l[0] != '#')))
            after = out.len;
        p += len;
    }
    if (inSection && !done) {
        insert_at(&out, after, line);
        done = 1;
    }
    if (!seen) {
        char head[80];
        snprintf(head, sizeof(head), "%s[%s]\r\n", !out.len ? "" : out.data[out.len - 1] != '\n' ? "\r\n\r\n" : "\r\n", section);
        buf_append(&out, head, strlen(head));
        buf_append(&out, line, strlen(line));
    }
    r = file_replace(path, out.data, out.len);
    buf_free(&in);
    buf_free(&out);
    return r;
}

/* a rule matches the whole card ("Card1/Card1-1"), just the file name ("Card1-1") or the folder ("Card1", for all
 * its channels) */
static int rule_matches(const char *rule, const char *id)
{
    const char *slash = strchr(id, '/');
    if (!strcasecmp(rule, id))
        return 1;
    if (slash && !strcasecmp(rule, slash + 1))
        return 1;
    if (slash && strlen(rule) == (size_t)(slash - id) && !strncasecmp(rule, id, slash - id))
        return 1;
    return 0;
}

int list_has(const char *list, const char *id)
{
    char copy[1024], *p, *save;
    COPY(copy, list);
    for (p = strtok_r(copy, ",", &save); p; p = strtok_r(NULL, ",", &save)) {
        trim(p);
        if (*p && rule_matches(p, id))
            return 1;
    }
    return 0;
}

int list_add(char *list, size_t size, const char *id)
{
    if (list_has(list, id))
        return 0;
    if (strlen(list) + strlen(id) + 3 > size)
        return -1;
    snprintf(list + strlen(list), size - strlen(list), "%s%s", list[0] ? ", " : "", id);
    return 0;
}

void list_remove(char *list, size_t size, const char *id)
{
    char copy[1024], *p, *save;
    const char *base = strchr(id, '/');
    base = base ? base + 1 : id;
    COPY(copy, list);
    list[0] = 0;
    for (p = strtok_r(copy, ",", &save); p; p = strtok_r(NULL, ",", &save)) {
        trim(p);
        if (*p && strcasecmp(p, id) != 0 && strcasecmp(p, base) != 0)
            snprintf(list + strlen(list), size - strlen(list), "%s%s", list[0] ? ", " : "", p);
    }
}

int config_keep(const char *id)
{
    int i, found = -1;
    /* the most specific rule wins: the whole card before the folder */
    for (i = 0; i < cfg.n_rules; i++)
        if (rule_matches(cfg.rules[i].id, id)) {
            if (!strcasecmp(cfg.rules[i].id, id))
                return cfg.rules[i].keep;
            found = cfg.rules[i].keep;
        }
    return found >= 0 ? found : cfg.keep;
}
