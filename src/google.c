/*
 * SD2Cloud -- Google: sign-in with a code (the "TVs and limited input devices" flow), access renewal and the Drive v3
 * API. Scope drive.file: the app only sees the files it created itself.
 *
 * The client ID and secret come from credentials.h (generated at build time from the Google Cloud JSON, kept out of
 * git). For installed apps Google says the secret "is not treated as a secret": each person's access lives only in
 * their own token.dat.
 *
 * HTTPS is always verified, with the wolfSSL/curl from ports4096/ (the SDK's wolfSSL is built with SP_INT_BITS=3072
 * and rejects Google's 4096-bit RSA roots with error -155).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <curl/curl.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include "common.h"
#include "credentials.h"

extern unsigned char cacert_pem[];
extern unsigned int size_cacert_pem;

#define URL_TOKEN  "https://oauth2.googleapis.com/token"
#define URL_DEVICE "https://oauth2.googleapis.com/device/code"
#define URL_REVOKE "https://oauth2.googleapis.com/revoke"
#define URL_DRIVE  "https://www.googleapis.com/drive/v3/files"
#define URL_UPLOAD "https://www.googleapis.com/upload/drive/v3/files"
#define SCOPE      "https://www.googleapis.com/auth/drive.file"

char googleError[256];
static CURL *curl;
static void (*pollHook)(void);
static char curlError[CURL_ERROR_SIZE];
static char accessToken[2048];

typedef struct {
    long http;
    buffer_t body;
    char location[1024];   /* Location */
    char range[64];        /* Range */
} response_t;

/* ------------------------------------------------------------ curl */

static size_t on_body(void *p, size_t size, size_t n, void *u) { return buf_append(u, p, size * n) ? 0 : size * n; }

static void copy_header(const char *line, size_t n, const char *name, char *out, size_t size)
{
    size_t k = strlen(name), v;
    if (n <= k || strncasecmp(line, name, k) != 0)
        return;
    line += k;
    n -= k;
    while (n && (*line == ' ' || *line == '\t')) {
        line++;
        n--;
    }
    while (n && (line[n - 1] == '\r' || line[n - 1] == '\n'))
        n--;
    v = n < size - 1 ? n : size - 1;
    memcpy(out, line, v);
    out[v] = 0;
}

static size_t on_header(char *p, size_t size, size_t n, void *u)
{
    response_t *r = u;
    copy_header(p, size * n, "location:", r->location, sizeof(r->location));
    copy_header(p, size * n, "range:", r->range, sizeof(r->range));
    return size * n;
}

static CURLcode on_ssl_ctx(CURL *c, void *ctx, void *u)
{
    (void)c;
    (void)u;
    if (wolfSSL_CTX_load_verify_buffer_ex((WOLFSSL_CTX *)ctx, cacert_pem, size_cacert_pem, WOLFSSL_FILETYPE_PEM, 0,
                                          WOLFSSL_LOAD_FLAG_IGNORE_ERR) != WOLFSSL_SUCCESS)
        return CURLE_SSL_CACERT_BADFILE;
    return CURLE_OK;
}

static int (*sendProgress)(long long done, long long total);   /* google_upload_buffer's, while it sends */

/* curl calls it many times a second while transferring: lets the app watch the controller (and, for a file sent
 * from memory, show how much went; != 0 stops) */
static int on_xfer(void *u, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    (void)u;
    (void)dltotal;
    (void)dlnow;
    if (pollHook)
        pollHook();
    if (sendProgress && ultotal > 0)
        return sendProgress((long long)ulnow, (long long)ultotal);
    return 0;
}

void google_set_poll(void (*poll)(void)) { pollHook = poll; }

static int on_debug(CURL *c, curl_infotype type, char *d, size_t n, void *u)
{
    (void)c;
    (void)u;
    if (type == CURLINFO_TEXT) {
        log_raw("   * ", 5);
        log_raw(d, n);
    } else if (type == CURLINFO_HEADER_IN && n > 5 && (!strncasecmp(d, "HTTP/", 5) || !strncasecmp(d, "range:", 6))) {
        log_raw("   < ", 5);
        log_raw(d, n);
    }
    return 0;
}

int google_init(void)
{
    if (curl)
        return 0;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK || !(curl = curl_easy_init()))
        return -1;
    curl_easy_setopt(curl, CURLOPT_USERAGENT, APP_NAME "/" APP_VERSION " (PS2)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, igrMode ? 15L : 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 256L);   /* stalled for 45 s = give up */
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 45L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);
    curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
    curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);
    curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, on_ssl_ctx);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, on_debug);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_xfer);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    return 0;
}

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(googleError, sizeof(googleError), fmt, ap);
    va_end(ap);
    log_msg("ERROR: %s", googleError);
}

static CURLcode request(const char *method, const char *url, struct curl_slist *hdr, const void *body, size_t nbody, response_t *r)
{
    CURLcode c;
    buf_free(&r->body);
    r->http = 0;
    r->location[0] = r->range[0] = 0;
    curlError[0] = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);   /* clears the method and body of the previous request */
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    if (body || !strcmp(method, "PUT") || !strcmp(method, "POST") || !strcmp(method, "PATCH")) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)(body ? nbody : 0));
    }
    if (strcmp(method, "GET") && strcmp(method, "POST"))
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r->body);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, r);
    c = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r->http);
    return c;
}

static const char *curl_text(CURLcode c) { return curlError[0] ? curlError : curl_easy_strerror(c); }

/* the reason Google gave: {"error": {"message": ...}} from the API or {"error": "...", "error_description": ...} from
 * OAuth */
static void reason(const response_t *r, char *out, size_t size)
{
    const char *t = (const char *)r->body.data, *sub;
    size_t n;
    out[0] = 0;
    if (!t)
        return;
    if (js_get_object(t, r->body.len, "error", &sub, &n) == 0)
        js_get_string(sub, n, "message", out, size);
    else if (js_get_string(t, r->body.len, "error_description", out, size) != 0)
        js_get_string(t, r->body.len, "error", out, size);
}

/* API request with the access token; retries on network failures and on 429/5xx, and once with a new access token when
 * Google says this one expired (it lasts an hour: syncing many cards can take longer). Returns the HTTP status
 * (0 = network) */
static long api(const char *method, const char *url, const char *body, response_t *r)
{
    char auth[2100];
    int attempt, max = igrMode ? 2 : 3, renewed = 0;
    CURLcode c = CURLE_OK;
    for (attempt = 1; attempt <= max; attempt++) {
        struct curl_slist *hdr = NULL;
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", accessToken);
        hdr = curl_slist_append(hdr, auth);
        hdr = curl_slist_append(hdr, "Expect:");
        if (body)
            hdr = curl_slist_append(hdr, "Content-Type: application/json; charset=UTF-8");
        c = request(method, url, hdr, body, body ? strlen(body) : 0, r);
        curl_slist_free_all(hdr);
        if (c == CURLE_OK && r->http == 401 && !renewed && google_refresh() == 0) {
            renewed = 1;
            attempt--;
            continue;
        }
        if (c == CURLE_OK && r->http != 429 && r->http < 500)
            return r->http;
        log_msg("%s %s: %s", method, url, c != CURLE_OK ? curl_text(c) : "429/5xx");
        if (attempt < max)
            sleep_ms(3000);
    }
    if (c != CURLE_OK)
        fail("%s", curl_text(c));
    return c == CURLE_OK ? r->http : 0;
}

/* ------------------------------------------------------------ access (OAuth) */

/* joins "key=value&..." escaping the values. the list ends with NULL */
static char *form(const char *k1, ...)
{
    va_list ap;
    buffer_t b = {0};
    const char *k = k1;
    va_start(ap, k1);
    while (k) {
        const char *v = va_arg(ap, const char *);
        char *e = curl_easy_escape(curl, v, 0);
        if (b.len)
            buf_append(&b, "&", 1);
        buf_append(&b, k, strlen(k));
        buf_append(&b, "=", 1);
        if (e) {
            buf_append(&b, e, strlen(e));
            curl_free(e);
        }
        k = va_arg(ap, const char *);
    }
    va_end(ap);
    return (char *)b.data;
}

static long oauth(const char *url, char *fields, response_t *r)
{
    CURLcode c = CURLE_OK;
    int attempt;
    if (!fields)
        return 0;   /* out of memory */
    for (attempt = 1; attempt <= 3; attempt++) {
        struct curl_slist *hdr = curl_slist_append(NULL, "Content-Type: application/x-www-form-urlencoded");
        hdr = curl_slist_append(hdr, "Expect:");
        c = request("POST", url, hdr, fields, strlen(fields), r);
        curl_slist_free_all(hdr);
        if (c == CURLE_OK && r->http < 500)
            break;
        if (attempt < 3)
            sleep_ms(3000);
    }
    free(fields);
    if (c != CURLE_OK)
        fail("%s", curl_text(c));
    return c == CURLE_OK ? r->http : 0;
}

int google_has_access(void) { return refreshToken[0] != 0; }

/* forgets the access to Google: asks Google to revoke it (online = there is network) and clears token.dat. Without
 * network the access stays valid on Google's side until it is removed at myaccount.google.com/permissions */
void google_logout(int online)
{
    if (online && refreshToken[0]) {
        response_t r = {0};
        long h = oauth(URL_REVOKE, form("token", refreshToken, NULL), &r);
        log_msg("Google: access revoked (HTTP %ld)", h);
        buf_free(&r.body);
    }
    refreshToken[0] = 0;
    accessToken[0] = 0;
    token_write();
}

int google_refresh(void)
{
    response_t r = {0};
    char m[200];
    long h = oauth(URL_TOKEN, form("client_id", GOOGLE_CLIENT_ID, "client_secret", GOOGLE_CLIENT_SECRET,
                                   "refresh_token", refreshToken, "grant_type", "refresh_token", NULL), &r);
    int res = -1;
    if (h == 200 && r.body.data && js_get_string((char *)r.body.data, r.body.len, "access_token", accessToken, sizeof(accessToken)) == 0) {
        res = 0;
    } else if (h == 400 || h == 401) {
        char e[64] = "";
        if (r.body.data)
            js_get_string((char *)r.body.data, r.body.len, "error", e, sizeof(e));
        reason(&r, m, sizeof(m));
        if (!strcmp(e, "invalid_grant"))
            res = -2;
        else
            fail("OAuth %ld: %s", h, m);
    } else if (h) {
        reason(&r, m, sizeof(m));
        fail("OAuth %ld: %s", h, m);
    }
    buf_free(&r.body);
    return res;
}

int google_login(void (*show)(const char *url, const char *code, int seconds_left), int (*cancel)(void))
{
    response_t r = {0};
    char device[256], code[32], url[128], m[200];
    long long expires = 1800, interval = 5;
    u64 end, next;
    long h;

    h = oauth(URL_DEVICE, form("client_id", GOOGLE_CLIENT_ID, "scope", SCOPE, NULL), &r);
    if (h != 200 || !r.body.data || js_get_string((char *)r.body.data, r.body.len, "device_code", device, sizeof(device)) ||
        js_get_string((char *)r.body.data, r.body.len, "user_code", code, sizeof(code))) {
        reason(&r, m, sizeof(m));
        if (h)
            fail("device/code %ld: %s", h, m);
        buf_free(&r.body);
        return T_LOGIN_ERROR;
    }
    if (js_get_string((char *)r.body.data, r.body.len, "verification_url", url, sizeof(url)))
        strcpy(url, "https://www.google.com/device");
    js_get_number((char *)r.body.data, r.body.len, "expires_in", &expires);
    js_get_number((char *)r.body.data, r.body.len, "interval", &interval);
    if (interval < 1)
        interval = 5;
    log_msg("login: open %s and enter %s", url, code);

    end = now_ms() + (u64)expires * 1000;
    next = now_ms() + (u64)interval * 1000;
    while (now_ms() < end) {
        char e[64] = "";
        show(url, code, (int)((end - now_ms()) / 1000));
        if (cancel()) {
            buf_free(&r.body);
            return -1;
        }
        if (now_ms() < next) {
            sleep_ms(100);
            continue;
        }
        next = now_ms() + (u64)interval * 1000;
        h = oauth(URL_TOKEN, form("client_id", GOOGLE_CLIENT_ID, "client_secret", GOOGLE_CLIENT_SECRET, "device_code",
                                  device, "grant_type", "urn:ietf:params:oauth:grant-type:device_code", NULL), &r);
        if (h == 0)
            continue;   /* network: try again at the next interval */
        if (h == 200 && r.body.data &&
            js_get_string((char *)r.body.data, r.body.len, "refresh_token", refreshToken, sizeof(refreshToken)) == 0 &&
            js_get_string((char *)r.body.data, r.body.len, "access_token", accessToken, sizeof(accessToken)) == 0) {
            buf_free(&r.body);
            if (token_write() != 0)
                log_msg("signed in, but couldn't write token.dat");
            return 0;
        }
        if (r.body.data)
            js_get_string((char *)r.body.data, r.body.len, "error", e, sizeof(e));
        if (!strcmp(e, "authorization_pending"))
            continue;
        if (!strcmp(e, "slow_down")) {
            interval += 5;
            continue;
        }
        buf_free(&r.body);
        log_msg("login: %ld %s", h, e);
        return !strcmp(e, "access_denied") ? T_LOGIN_DENIED : !strcmp(e, "expired_token") ? T_LOGIN_EXPIRED : T_LOGIN_ERROR;
    }
    buf_free(&r.body);
    return T_LOGIN_EXPIRED;
}

/* ------------------------------------------------------------ folders */

/* a value for a Drive query: single quotes and backslashes escaped */
static void quote(const char *s, char *out, size_t size)
{
    size_t n = 0;
    while (*s && n + 3 < size) {
        if (*s == '\'' || *s == '\\')
            out[n++] = '\\';
        out[n++] = *s++;
    }
    out[n] = 0;
}

/* text for inside JSON (quotes and backslashes escaped; control characters dropped) */
static void json_escape(const char *s, char *out, size_t size)
{
    size_t n = 0;
    while (*s && n + 3 < size) {
        if (*s == '"' || *s == '\\')
            out[n++] = '\\';
        if ((unsigned char)*s >= 0x20)
            out[n++] = *s;
        s++;
    }
    out[n] = 0;
}

static char *url_escape(const char *s)
{
    char *e = curl_easy_escape(curl, s, 0);
    return e ? e : strdup("");
}

static int first_id(const response_t *r, char *id, size_t idsize)
{
    json_t v;
    id[0] = 0;
    if (!r->body.data || js_find((char *)r->body.data, r->body.len, "files", &v) || v.p >= v.end || *v.p != '[')
        return -1;
    v.p++;
    while (v.p < v.end && isspace((unsigned char)*v.p))
        v.p++;
    if (v.p < v.end && *v.p == '{') {
        const char *start = v.p;
        if (js_skip(&v, 0) == 0)
            return js_get_string(start, v.p - start, "id", id, idsize);
    }
    return -1;
}

int google_folder(const char *name, const char *parent, char *id, size_t idsize)
{
    response_t r = {0};
    char q[400], nq[160], url[900], body[400], nj[160], m[200];
    char *e;
    long h;

    /* does the cached one still exist? (it may have been deleted on Drive) */
    if (id[0]) {
        json_t v;
        int ok;
        snprintf(url, sizeof(url), URL_DRIVE "/%s?fields=id,trashed", id);
        h = api("GET", url, NULL, &r);
        ok = h == 200 && r.body.data &&
             !(js_find((char *)r.body.data, r.body.len, "trashed", &v) == 0 && v.p < v.end && *v.p == 't');
        buf_free(&r.body);
        if (ok)
            return 0;
        if (h == 0)
            return -1;
        id[0] = 0;
    }
    quote(name, nq, sizeof(nq));
    snprintf(q, sizeof(q), "name='%s' and mimeType='application/vnd.google-apps.folder' and trashed=false and '%s' in parents", nq, parent);
    e = url_escape(q);
    snprintf(url, sizeof(url), URL_DRIVE "?q=%s&fields=files(id)&spaces=drive&pageSize=10", e);
    curl_free(e);
    h = api("GET", url, NULL, &r);
    if (h != 200) {
        reason(&r, m, sizeof(m));
        if (h)
            fail("Drive %ld: %s", h, m);
        buf_free(&r.body);
        return -1;
    }
    if (first_id(&r, id, idsize) == 0 && id[0]) {
        buf_free(&r.body);
        return 0;
    }
    json_escape(name, nj, sizeof(nj));
    snprintf(body, sizeof(body), "{\"name\":\"%s\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"%s\"]}", nj, parent);
    h = api("POST", URL_DRIVE "?fields=id", body, &r);
    if (h != 200 || !r.body.data || js_get_string((char *)r.body.data, r.body.len, "id", id, idsize)) {
        reason(&r, m, sizeof(m));
        if (h)
            fail("Drive %ld: %s", h, m);
        buf_free(&r.body);
        return -1;
    }
    log_msg("created the \"%s\" folder on Drive", name);
    buf_free(&r.body);
    return 0;
}

/* ------------------------------------------------------------ list and delete */

typedef struct {
    drive_file_t *list;
    int max, n;
} collect_t;

static void on_file(const char *obj, size_t n, void *u)
{
    collect_t *c = u;
    drive_file_t *f;
    if (c->n >= c->max)
        return;
    f = &c->list[c->n];
    memset(f, 0, sizeof(*f));
    if (js_get_string(obj, n, "id", f->id, sizeof(f->id)))
        return;
    js_get_string(obj, n, "name", f->name, sizeof(f->name));
    js_get_string(obj, n, "createdTime", f->created, sizeof(f->created));
    js_get_number(obj, n, "size", &f->size);
    {
        const char *props;
        size_t np;
        if (js_get_object(obj, n, "appProperties", &props, &np) == 0)
            js_get_string(props, np, "sha256mcd", f->sha_mcd, sizeof(f->sha_mcd));
    }
    c->n++;
}

int google_list(const char *folder, const char *card, drive_file_t *list, int max)
{
    response_t r = {0};
    char q[400], cq[160], url[900], m[200];
    collect_t c = {list, max, 0};
    char *e;
    long h;
    int i;
    quote(card, cq, sizeof(cq));
    snprintf(q, sizeof(q), "'%s' in parents and trashed=false and appProperties has { key='card' and value='%s' }", folder, cq);
    e = url_escape(q);
    /* newest first: a card kept with "all" can have more backups than one page (or the list) holds, and the ones left
     * out must be the oldest */
    snprintf(url, sizeof(url), URL_DRIVE "?q=%s&orderBy=createdTime%%20desc&pageSize=200&spaces=drive&fields=files(id,name,createdTime,size,appProperties)", e);
    curl_free(e);
    h = api("GET", url, NULL, &r);
    if (h != 200 || !r.body.data || js_each((char *)r.body.data, r.body.len, "files", on_file, &c)) {
        reason(&r, m, sizeof(m));
        if (h)
            fail("Drive %ld: %s", h, m);
        buf_free(&r.body);
        return -1;
    }
    buf_free(&r.body);
    for (i = 0; i < c.n / 2; i++) {   /* handed out oldest first */
        drive_file_t t = list[i];
        list[i] = list[c.n - 1 - i];
        list[c.n - 1 - i] = t;
    }
    return c.n;
}

int google_delete(const char *id)
{
    response_t r = {0};
    char url[256];
    long h;
    snprintf(url, sizeof(url), URL_DRIVE "/%s", id);
    h = api("DELETE", url, NULL, &r);
    buf_free(&r.body);
    return (h == 204 || h == 200 || h == 404) ? 0 : -1;   /* 404: already gone */
}

/* ------------------------------------------------------------ upload (streamed) */

typedef struct {
    char session[1024];
    long long pos;            /* bytes of the .zip that Drive has confirmed */
    response_t r;
    int done;
    stream_t *s;
} upload_t;

/* next byte Drive wants, from the "Range: bytes=0-N" header (no Range = nothing yet) */
static long long next_byte(const response_t *r)
{
    const char *t = strchr(r->range, '-');
    return t ? strtoll(t + 1, NULL, 10) + 1 : 0;
}

static long put(upload_t *up, const unsigned char *d, size_t n, long long start, const char *total)
{
    struct curl_slist *hdr = NULL;
    char range[96];
    CURLcode c;
    if (n)
        snprintf(range, sizeof(range), "Content-Range: bytes %lld-%lld/%s", start, start + (long long)n - 1, total);
    else
        snprintf(range, sizeof(range), "Content-Range: bytes */%s", total);
    hdr = curl_slist_append(hdr, range);
    hdr = curl_slist_append(hdr, "Expect:");
    c = request("PUT", up->session, hdr, n ? d : NULL, n, &up->r);
    curl_slist_free_all(hdr);
    if (c != CURLE_OK) {
        log_msg("PUT: %s", curl_text(c));
        fail("%s", curl_text(c));
        return 0;
    }
    return up->r.http;
}

/* one chunk of the .zip (from stream_zip). If the network drops, it asks Drive where it stopped and sends the rest */
static int on_chunk(const unsigned char *d, size_t n, int last, void *u)
{
    upload_t *up = u;
    long long start = up->s->sent - (long long)n;   /* where this chunk starts in the .zip */
    char total[24];
    int failures = 0;
    if (last)
        snprintf(total, sizeof(total), "%lld", up->s->sent);
    else
        strcpy(total, "*");
    while (up->pos < start + (long long)n || (last && !up->done)) {
        long long from = up->pos - start;
        long h;
        if (from < 0 || from > (long long)n) {
            fail(T(T_ERR_POSITION), up->pos);
            return -1;
        }
        h = put(up, d + from, n - from, up->pos, total);
        if (h == 200 || h == 201) {
            up->pos = start + n;
            up->done = 1;
            return 0;
        }
        if (h == 308) {
            long long nb = next_byte(&up->r);
            if (nb > up->pos) {
                up->pos = nb;
                failures = 0;
                continue;
            }
        }
        if (h == 404 || h == 410 || ++failures > (igrMode ? 3 : 6)) {
            char m[200];
            reason(&up->r, m, sizeof(m));
            if (h)
                fail("Drive %ld: %s", h, m);
            return -1;
        }
        /* ask where it stopped */
        sleep_ms(2000);
        h = put(up, NULL, 0, 0, total);
        if (h == 200 || h == 201) {
            up->pos = start + n;
            up->done = 1;
            return 0;
        }
        if (h == 308)
            up->pos = next_byte(&up->r);
    }
    return 0;
}

int google_upload(const card_t *c, const char *folder, const char *name, const datetime_t *t, stream_t *s,
                  int (*progress)(long long done, long long total), char *id, size_t idsize)
{
    upload_t *up = calloc(1, sizeof(upload_t));
    char body[900], nj[160], cj[160], dj[120], url[256], checksum[80] = "", m[200];
    long h;
    int r = -1;
    if (!up)
        return -1;
    up->s = s;
    s->progress = progress;
    json_escape(name, nj, sizeof(nj));
    json_escape(c->id, cj, sizeof(cj));
    json_escape(c->name[0] ? c->name : c->base, dj, sizeof(dj));
    snprintf(body, sizeof(body),
             "{\"name\":\"%s\",\"mimeType\":\"application/zip\",\"parents\":[\"%s\"],"
             "\"description\":\"%s (%s, %s/%s%s) - " APP_NAME " " APP_VERSION "\","
             "\"appProperties\":{\"sd2cloud\":\"1\",\"card\":\"%s\"}}",
             nj, folder, dj, dev->name, dev->cards, cj, dev->ext, cj);
    h = api("POST", URL_UPLOAD "?uploadType=resumable&fields=id,size,sha256Checksum,md5Checksum", body, &up->r);
    if (h != 200 || !up->r.location[0]) {
        reason(&up->r, m, sizeof(m));
        if (h)
            fail("Drive %ld: %s", h, m);
        goto out;
    }
    snprintf(up->session, sizeof(up->session), "%s", up->r.location);

    if (stream_zip(c, t, s, on_chunk, up) != 0 || !up->done) {
        if (!googleError[0])
            fail("%s", T(T_ERR_UPLOAD_STOPPED));
        goto out;
    }
    if (!up->r.body.data || js_get_string((char *)up->r.body.data, up->r.body.len, "id", id, idsize)) {
        fail("%s", T(T_ERR_NO_FILE_ID));
        goto out;
    }
    /* verification: the SHA-256 Drive computed must be the one of the .zip built here */
    js_get_string((char *)up->r.body.data, up->r.body.len, "sha256Checksum", checksum, sizeof(checksum));
    if (strcasecmp(checksum, s->sha_zip) != 0) {
        fail("%s", T(T_ERR_CHECKSUM));
        google_delete(id);
        goto out;
    }
    snprintf(url, sizeof(url), URL_DRIVE "/%s?fields=id", id);
    snprintf(body, sizeof(body), "{\"appProperties\":{\"sha256mcd\":\"%s\",\"fingerprint\":\"%s\"}}", s->sha_mcd, c->fingerprint);
    if (api("PATCH", url, body, &up->r) != 200)
        log_msg("warning: the appProperties PATCH failed (the backup itself is verified)");
    r = 0;
out:
    buf_free(&up->r.body);
    free(up);
    return r;
}

/* a small file from memory (a single save, .psu): a resumable session and the whole file in one PUT, then the
 * SHA-256 Drive computed must be the one of the data. 0 = ok */
int google_upload_buffer(const char *folder, const char *name, const char *description, const unsigned char *d, size_t n,
                         int (*progress)(long long done, long long total), char *id, size_t idsize)
{
    response_t r = {0};
    char body[900], nj[160], dj[300], range[80], sha[65], checksum[80] = "", m[200], session[1024];
    struct curl_slist *hdr = NULL;
    CURLcode cc;
    long h;
    json_escape(name, nj, sizeof(nj));
    json_escape(description, dj, sizeof(dj));
    snprintf(body, sizeof(body),
             "{\"name\":\"%s\",\"mimeType\":\"application/octet-stream\",\"parents\":[\"%s\"],\"description\":\"%s\","
             "\"appProperties\":{\"sd2cloud\":\"1\",\"save\":\"1\"}}",
             nj, folder, dj);
    h = api("POST", URL_UPLOAD "?uploadType=resumable&fields=id,sha256Checksum", body, &r);
    if (h != 200 || !r.location[0]) {
        reason(&r, m, sizeof(m));
        if (h)
            fail("Drive %ld: %s", h, m);
        buf_free(&r.body);
        return -1;
    }
    snprintf(session, sizeof(session), "%s", r.location);
    snprintf(range, sizeof(range), "Content-Range: bytes 0-%u/%u", (unsigned)n - 1, (unsigned)n);
    hdr = curl_slist_append(hdr, range);
    hdr = curl_slist_append(hdr, "Expect:");
    sendProgress = progress;
    cc = request("PUT", session, hdr, d, n, &r);
    sendProgress = NULL;
    curl_slist_free_all(hdr);
    if (cc != CURLE_OK || (r.http != 200 && r.http != 201) || !r.body.data ||
        js_get_string((char *)r.body.data, r.body.len, "id", id, idsize)) {
        reason(&r, m, sizeof(m));
        if (cc != CURLE_OK)
            fail("%s", curl_text(cc));
        else
            fail("Drive %ld: %s", r.http, m);
        buf_free(&r.body);
        return -1;
    }
    js_get_string((char *)r.body.data, r.body.len, "sha256Checksum", checksum, sizeof(checksum));
    buf_free(&r.body);
    sha256_hex(d, n, sha);
    if (strcasecmp(checksum, sha) != 0) {
        fail("%s", T(T_ERR_CHECKSUM));
        google_delete(id);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------ download (restore.c) */

typedef struct {
    int (*sink)(const unsigned char *d, size_t n, void *u);
    void *u;
    buffer_t errorBody;     /* the body of an error answer (JSON), for the reason */
    int stopped;
} download_t;

static size_t on_download(void *p, size_t size, size_t n, void *u)
{
    download_t *d = u;
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    if (http != 200)
        return buf_append(&d->errorBody, p, size * n) ? 0 : size * n;
    if (d->sink(p, size * n, d->u)) {
        d->stopped = 1;
        return 0;   /* curl stops with CURLE_WRITE_ERROR */
    }
    return size * n;
}

int google_download(const char *id, int (*sink)(const unsigned char *d, size_t n, void *u), void *u)
{
    response_t r = {0};
    download_t d = {sink, u, {0}, 0};
    struct curl_slist *hdr = NULL;
    char url[256], auth[2100], m[200];
    CURLcode c;
    snprintf(url, sizeof(url), URL_DRIVE "/%s?alt=media", id);
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", accessToken);
    hdr = curl_slist_append(hdr, auth);
    curlError[0] = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_download);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &r);
    c = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.http);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_slist_free_all(hdr);
    if (d.stopped) {
        buf_free(&d.errorBody);
        return -2;
    }
    if (c != CURLE_OK || r.http != 200) {
        r.body = d.errorBody;   /* reason() reads the JSON from it */
        reason(&r, m, sizeof(m));
        if (c != CURLE_OK)
            fail("%s", curl_text(c));
        else
            fail("Drive %ld: %s", r.http, m);
        buf_free(&r.body);
        return -1;
    }
    buf_free(&d.errorBody);
    return 0;
}

/* ------------------------------------------------------------ GitHub (update.c) */

/* Asset downloads go from github.com to release-assets.githubusercontent.com, whose chain goes through a 4096-bit RSA
 * root: with the wolfSSL from ports4096/ the certificate is verified normally (the SDK's one couldn't). */
long github_get(const char *url, buffer_t *b, int follow)
{
    response_t r = {0};
    struct curl_slist *hdr = NULL;
    CURLcode c;
    buf_free(b);
    if (google_init() != 0)
        return 0;
    if (strstr(url, "api.github.com"))
        hdr = curl_slist_append(hdr, "Accept: application/vnd.github+json");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L);
    c = request("GET", url, hdr, NULL, 0, &r);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_slist_free_all(hdr);
    if (c != CURLE_OK) {
        fail("%s", curl_text(c));
        buf_free(&r.body);
        return 0;
    }
    *b = r.body;   /* the body goes to the caller */
    return r.http;
}
