/*
 * SD2Cloud -- updating from the app itself, from the latest GitHub release.
 *
 * The release must have SD2CLOUD.ELF (and SD2CLOUD-IGR.ELF) attached LOOSE, besides the zip: the PS2 doesn't open zips.
 * The SHA-256 of each one comes from the API's "digest" field (without it, nothing is installed). Each file is
 * downloaded to memory, verified, written as .new, read back and only then replaces the old one: if anything fails
 * halfway, the current version stays whole. The settings (SD2Cloud/sd2cloud.ini) are never touched.
 *
 * A program that lives on a memory card (the Save Application System package) is updated right there (helper.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "common.h"

/* names of the loose files in the release (changeable only to test against another project's release) */
#ifndef ASSET_APP
#define ASSET_APP "SD2CLOUD.ELF"
#endif
#ifndef ASSET_IGR
#define ASSET_IGR "SD2CLOUD-IGR.ELF"
#endif

typedef struct {
    char tag[32], published[32];   /* "v1.6", "2026-10-07T21:33:59Z" */
    char url_app[512], sha_app[65];
    char url_igr[512], sha_igr[65];
} release_t;

static release_t latest;

/* "v0.2.1" vs "0.1": >0 if a is newer */
static int compare_versions(const char *a, const char *b)
{
    while (*a && !isdigit((unsigned char)*a))
        a++;
    while (*b && !isdigit((unsigned char)*b))
        b++;
    while (*a || *b) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
        if (x != y)
            return x > y ? 1 : -1;
        if (*a == '.')
            a++;
        if (*b == '.')
            b++;
        if (*a && !isdigit((unsigned char)*a))
            break;   /* "-beta" etc.: the rest doesn't count */
        if (*b && !isdigit((unsigned char)*b))
            break;
    }
    return 0;
}

static void on_asset(const char *obj, size_t n, void *u)
{
    char name[64], url[512], digest[80], state[16];
    (void)u;
    if (js_get_string(obj, n, "name", name, sizeof(name)) || js_get_string(obj, n, "browser_download_url", url, sizeof(url)))
        return;
    js_get_string(obj, n, "state", state, sizeof(state));
    js_get_string(obj, n, "digest", digest, sizeof(digest));
    if (strcmp(state, "uploaded") != 0 || strncmp(digest, "sha256:", 7) != 0 || strlen(digest + 7) != 64)
        return;
    if (!strcasecmp(name, ASSET_APP)) {
        snprintf(latest.url_app, sizeof(latest.url_app), "%s", url);
        snprintf(latest.sha_app, sizeof(latest.sha_app), "%s", digest + 7);
    } else if (!strcasecmp(name, ASSET_IGR)) {
        snprintf(latest.url_igr, sizeof(latest.url_igr), "%s", url);
        snprintf(latest.sha_igr, sizeof(latest.sha_igr), "%s", digest + 7);
    }
}

int update_check(void)
{
    char url[200];
    buffer_t b = {0};
    long h;
    memset(&latest, 0, sizeof(latest));
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", cfg.repo);
    h = github_get(url, &b, 1);
    if (h != 200 || !b.data) {
        log_msg("update: GitHub answered %ld (%s)", h, cfg.repo);
        buf_free(&b);
        return -1;
    }
    js_get_string((char *)b.data, b.len, "tag_name", latest.tag, sizeof(latest.tag));
    js_get_string((char *)b.data, b.len, "published_at", latest.published, sizeof(latest.published));
    js_each((char *)b.data, b.len, "assets", on_asset, NULL);
    buf_free(&b);
    log_msg("update: latest release %s (this is %s)%s", latest.tag, APP_VERSION, latest.url_app[0] ? "" : ", without a loose SD2CLOUD.ELF");
    if (!latest.tag[0] || !latest.url_app[0] || compare_versions(latest.tag, APP_VERSION) <= 0) {
        latest.tag[0] = 0;
        return 0;
    }
    return 1;
}

const char *update_tag(void) { return latest.tag; }

/* downloads a file of the release to memory and checks GitHub's SHA-256. 0 = it is in b, whole */
static int download(const char *url, const char *sha, const char *name, buffer_t *b)
{
    char h[65];
    if (github_get(url, b, 1) != 200 || !b->data) {
        log_msg("update: couldn't download %s (%s)", name, googleError);
        buf_free(b);
        return -1;
    }
    sha256_hex(b->data, b->len, h);
    if (strcasecmp(h, sha) != 0) {
        log_msg("update: %s doesn't match the published file (SHA-256)", name);
        buf_free(b);
        return -1;
    }
    return 0;
}

/* downloads, checks and replaces the file (file_replace: .new -> verify -> replace -> verify) */
static int install_file(const char *url, const char *sha, const char *name)
{
    char target[260];
    buffer_t b = {0};
    int ok;
    if (download(url, sha, name, &b) != 0)
        return -1;
    snprintf(target, sizeof(target), "%s%s", appDir, name);
    ok = file_replace(target, b.data, b.len) == 0;   /* read back and compared there */
    log_msg("update: %s %s (%u bytes)", name, ok ? "installed and verified" : "FAILED", (unsigned)b.len);
    buf_free(&b);
    return ok ? 0 : -1;
}

/* The program on a memory card: both files are downloaded and checked before the card is touched, then written to the
 * program's folder there (card_app_update), whose title.cfg tells the version and the day it came out, as the Save
 * Application System writes it ("October 7, 2026") */
static int install_on_card(void (*progress)(long long done, long long total))
{
    static const char *const months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                                         "October", "November", "December"};
    buffer_t app = {0}, igr = {0};
    char released[40] = "";
    const char *version = latest.tag;
    int y, m, d, r = -1;
    while (*version && !isdigit((unsigned char)*version))
        version++;
    if (sscanf(latest.published, "%d-%d-%d", &y, &m, &d) == 3 && m >= 1 && m <= 12)
        snprintf(released, sizeof(released), "%s %d, %d", months[m - 1], d, y);
    if ((!latest.url_igr[0] || download(latest.url_igr, latest.sha_igr, ASSET_IGR, &igr) == 0)
        && download(latest.url_app, latest.sha_app, ASSET_APP, &app) == 0)
        r = card_app_update(&app, &igr, version, released, progress);
    log_msg("update: on the memory card, %s (%d)", r == 0 ? "installed and verified" : "FAILED", r);
    buf_free(&app);
    buf_free(&igr);
    return r;
}

int update_install(void (*progress)(long long done, long long total))
{
    char c[260];
    static const char title[] = "title=SD2Cloud\nboot=SD2CLOUD.ELF\n";
    if (!latest.tag[0])
        return -1;
    if (appOnCard)
        return install_on_card(progress);
    /* started from another device with no SD2Cloud on the microSD: the update is what puts it there (with the title.cfg
     * that makes OPL list it) */
    ensure_dir(appDir);
    /* the helper first: if it fails, the app doesn't change; the new app then offers to update the memory card's one */
    if (latest.url_igr[0] && install_file(latest.url_igr, latest.sha_igr, "SD2CLOUD-IGR.ELF") != 0)
        return -1;
    if (install_file(latest.url_app, latest.sha_app, "SD2CLOUD.ELF") != 0)
        return -1;
    snprintf(c, sizeof(c), "%stitle.cfg", appDir);
    if (appElsewhere && !file_exists(c))
        file_write(c, (const unsigned char *)title, sizeof(title) - 1);
    return 0;
}
