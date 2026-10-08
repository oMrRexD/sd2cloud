/*
 * SD2Cloud -- minimal JSON: just enough for the answers from Google and GitHub. It builds no tree: it walks the text
 * and skips whatever it doesn't need.
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

static void js_ws(json_t *j)
{
    while (j->p < j->end && (*j->p == ' ' || *j->p == '\n' || *j->p == '\r' || *j->p == '\t'))
        j->p++;
}

static int js_eat(json_t *j, char c)
{
    js_ws(j);
    if (j->p < j->end && *j->p == c) {
        j->p++;
        return 1;
    }
    return 0;
}

/* the 4 hex digits of a \uXXXX; -1 = not that */
static int js_hex4(json_t *j)
{
    int i, v = 0;
    if (j->end - j->p < 4)
        return -1;
    for (i = 0; i < 4; i++) {
        int c = (unsigned char)*j->p++;
        if (!isxdigit(c))
            return -1;
        v = v * 16 + (isdigit(c) ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}

/* reads a string; if out != NULL, copies it with the escapes decoded (\uXXXX becomes UTF-8: Google writes the
 * characters ' & < > = that way) */
int js_string(json_t *j, char *out, size_t size)
{
    size_t n = 0;
    if (!js_eat(j, '"'))
        return -1;
    while (j->p < j->end && *j->p != '"') {
        char c = *j->p++;
        if (c == '\\') {
            if (j->p >= j->end)
                return -1;
            c = *j->p++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c == 'u') {
                char utf8[4];
                int u = js_hex4(j), low, k = 0, i;
                if (u < 0)
                    return -1;
                /* two of them in a row are one character past U+FFFF (an emoji) */
                if (u >= 0xD800 && u < 0xDC00 && j->end - j->p >= 6 && j->p[0] == '\\' && j->p[1] == 'u') {
                    j->p += 2;
                    if ((low = js_hex4(j)) < 0)
                        return -1;
                    u = 0x10000 + ((u - 0xD800) << 10) + ((low - 0xDC00) & 0x3FF);
                }
                if (u < 0x80)
                    utf8[k++] = u;
                else if (u < 0x800)
                    utf8[k++] = 0xC0 | (u >> 6), utf8[k++] = 0x80 | (u & 0x3F);
                else if (u < 0x10000)
                    utf8[k++] = 0xE0 | (u >> 12), utf8[k++] = 0x80 | ((u >> 6) & 0x3F), utf8[k++] = 0x80 | (u & 0x3F);
                else
                    utf8[k++] = 0xF0 | (u >> 18), utf8[k++] = 0x80 | ((u >> 12) & 0x3F), utf8[k++] = 0x80 | ((u >> 6) & 0x3F),
                    utf8[k++] = 0x80 | (u & 0x3F);
                if (out && n + k < size)   /* whole or not at all */
                    for (i = 0; i < k; i++)
                        out[n++] = utf8[i];
                continue;
            }
        }
        if (out && n + 1 < size)
            out[n++] = c;
    }
    if (j->p >= j->end)
        return -1;
    j->p++;
    if (out)
        out[n] = 0;
    return 0;
}

/* skips any value */
int js_skip(json_t *j, int depth)
{
    char c, close;
    js_ws(j);
    if (j->p >= j->end || depth > 64)
        return -1;
    c = *j->p;
    if (c == '"')
        return js_string(j, NULL, 0);
    if (c != '{' && c != '[') {   /* number, true, false, null */
        while (j->p < j->end && !strchr(",}] \t\r\n", *j->p))
            j->p++;
        return 0;
    }
    close = c == '{' ? '}' : ']';
    j->p++;
    if (js_eat(j, close))
        return 0;
    for (;;) {
        if (c == '{' && (js_string(j, NULL, 0) || !js_eat(j, ':')))
            return -1;
        if (js_skip(j, depth + 1))
            return -1;
        if (js_eat(j, ','))
            continue;
        return js_eat(j, close) ? 0 : -1;
    }
}

int js_find(const char *txt, size_t n, const char *key, json_t *v)
{
    json_t j = {txt, txt + n};
    char k[64];
    if (!js_eat(&j, '{') || js_eat(&j, '}'))
        return -1;
    for (;;) {
        if (js_string(&j, k, sizeof(k)) || !js_eat(&j, ':'))
            return -1;
        js_ws(&j);
        if (strcmp(k, key) == 0) {
            *v = j;
            return 0;
        }
        if (js_skip(&j, 0))
            return -1;
        if (js_eat(&j, ','))
            continue;
        return -1;
    }
}

int js_get_string(const char *txt, size_t n, const char *key, char *out, size_t size)
{
    json_t v;
    if (size)
        out[0] = 0;
    if (js_find(txt, n, key, &v) || v.p >= v.end || *v.p != '"')
        return -1;
    return js_string(&v, out, size);
}

int js_get_number(const char *txt, size_t n, const char *key, long long *out)
{
    json_t v;
    char t[32];
    size_t k = 0;
    if (js_find(txt, n, key, &v))
        return -1;
    if (v.p < v.end && *v.p == '"')   /* Drive sends "size" as a string */
        return js_string(&v, t, sizeof(t)) ? -1 : (*out = strtoll(t, NULL, 10), 0);
    while (v.p < v.end && k + 1 < sizeof(t) && (isdigit((unsigned char)*v.p) || *v.p == '-'))
        t[k++] = *v.p++;
    t[k] = 0;
    if (!k)
        return -1;
    *out = strtoll(t, NULL, 10);
    return 0;
}

int js_get_object(const char *txt, size_t n, const char *key, const char **start, size_t *len)
{
    json_t v;
    if (js_find(txt, n, key, &v) || v.p >= v.end || *v.p != '{')
        return -1;
    *start = v.p;
    if (js_skip(&v, 0))
        return -1;
    *len = v.p - *start;
    return 0;
}

int js_each(const char *txt, size_t n, const char *key, void (*cb)(const char *obj, size_t n, void *u), void *u)
{
    json_t v;
    if (js_find(txt, n, key, &v) || !js_eat(&v, '['))
        return -1;
    if (js_eat(&v, ']'))
        return 0;
    for (;;) {
        const char *start;
        js_ws(&v);
        start = v.p;
        if (v.p >= v.end || *v.p != '{' || js_skip(&v, 0))
            return -1;
        cb(start, v.p - start, u);
        if (js_eat(&v, ','))
            continue;
        return js_eat(&v, ']') ? 0 : -1;
    }
}
