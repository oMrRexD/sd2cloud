/*
 * SD2Cloud -- the files of lang/, one per language: "ID = text" lines, in the format of Java's .properties files,
 * which translation tools know. A line starting with # or ! is a comment. The key ends at "=", ":" or a space, and
 * so does what separates it from the text. In the text, \n is a line break, \t a tab, \uXXXX a character by its
 * code and \ followed by anything else that character (a text that starts with a space starts with "\ "); a \ at
 * the end of a line goes on in the next one. tools/check_messages.py reads them the same way.
 */
#include <string.h>
#include "lang.h"

static int hex(char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 0x20) >= 'a' && (c | 0x20) <= 'f' ? (c | 0x20) - 'a' + 10 : -1; }

int lang_parse(char *text, size_t n, void (*cb)(const char *key, const char *value, void *u), void *u)
{
    char *p = text, *end = text + n;
    int count = 0;
    if (n >= 3 && !memcmp(p, "\xEF\xBB\xBF", 3))   /* (a file saved by Notepad starts with the UTF-8 mark) */
        p += 3;
    while (p < end) {
        char *key, *keyEnd, *value, *w;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\f'))
            p++;
        if (p < end && (*p == '#' || *p == '!' || *p == '\r' || *p == '\n')) {   /* a comment, or an empty line */
            while (p < end && *p != '\n')
                p++;
            p++;
            continue;
        }
        if (p >= end)
            break;
        key = p;
        while (p < end && *p != '=' && *p != ':' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            p++;
        keyEnd = p;
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p < end && (*p == '=' || *p == ':'))
            p++;
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        value = w = p;   /* (written over itself: what is written is never longer than what was read) */
        while (p < end && *p != '\r' && *p != '\n') {
            char c = *p++;
            if (c != '\\') {
                *w++ = c;
                continue;
            }
            if (p >= end)
                break;
            c = *p++;
            if (c == '\r' || c == '\n') {   /* the text goes on in the next line, without its first spaces */
                if (c == '\r' && p < end && *p == '\n')
                    p++;
                while (p < end && (*p == ' ' || *p == '\t'))
                    p++;
            } else if (c == 'u' && end - p >= 4 && hex(p[0]) >= 0 && hex(p[1]) >= 0 && hex(p[2]) >= 0 && hex(p[3]) >= 0) {
                unsigned int v = hex(p[0]) << 12 | hex(p[1]) << 8 | hex(p[2]) << 4 | hex(p[3]);
                p += 4;
                if (v < 0x80)
                    *w++ = (char)v;
                else if (v < 0x800) {
                    *w++ = (char)(0xC0 | v >> 6);
                    *w++ = (char)(0x80 | (v & 0x3F));
                } else {
                    *w++ = (char)(0xE0 | v >> 12);
                    *w++ = (char)(0x80 | ((v >> 6) & 0x3F));
                    *w++ = (char)(0x80 | (v & 0x3F));
                }
            } else
                *w++ = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
        }
        if (p < end && *p == '\r')
            p++;
        if (p < end && *p == '\n')
            p++;
        *w = 0;        /* (where the line ended, or the room after the text) */
        *keyEnd = 0;   /* (before the text, or the same 0 when there is none) */
        cb(key, value, u);
        count++;
    }
    return count;
}
