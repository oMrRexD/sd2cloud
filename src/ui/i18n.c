/* SD2Cloud -- the texts on screen, in each language of lang/ (built into the program: langFiles), read the first time
 * one is needed. A text a language doesn't have is the English one. "auto" follows the PS2's language when there is
 * a file for it, else English */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <osd_config.h>
#include "common.h"
#include "lang.h"
#include "font.h"

#define MAX_LANGS 16

static const char *const ids[T_COUNT] = {
    [T_NONE] = "",
#define X(id) [id] = #id,
#include "messages.def"
#undef X
};

static struct {
    const char *code, *name;
    char date[6];                    /* how a date is written: D/M/Y, M/D/Y, Y-M-D, D.M.Y... */
    int circle;                      /* circle confirms and cross goes back, as in Japan (CONFIRM_BUTTON = CIRCLE) */
    int japanese;                    /* some text has Japanese characters (they are in a font of their own) */
    const char *text[T_COUNT];
    const char *renamed[T_COUNT];    /* the texts that name the sd2psx, naming another device (i18n_device) */
} langs[MAX_LANGS];
static int nLangs, current, english;

/* a text of the file being read, put where its id says. The files list them in the order of the ids, so the next id
 * is looked at first */
typedef struct {
    int k, next;
} reading_t;

/* a kana, kanji or their punctuation (U+3000..U+9FFF, U+FF00..U+FFEF): the Japanese font's (font.c) */
static int has_japanese(const char *s)
{
    const unsigned char *p;
    for (p = (const unsigned char *)s; *p; p++)
        if ((*p >= 0xE3 && *p <= 0xE9) || (*p == 0xEF && p[1] >= 0xBC && p[1] <= 0xBF))
            return 1;
    return 0;
}

static void on_text(const char *key, const char *value, void *u)
{
    reading_t *r = u;
    int id;
    if (!strcmp(key, "LANGUAGE_NAME")) {
        langs[r->k].name = value;
        return;
    }
    if (!strcmp(key, "DATE_FORMAT")) {   /* DMY, MDY and YMD with their usual separators, or one of its own: D.M.Y */
        if (strlen(value) == 3)
            snprintf(langs[r->k].date, sizeof(langs[0].date), "%c%c%c%c%c", value[0], value[0] == 'Y' ? '-' : '/',
                     value[1], value[0] == 'Y' ? '-' : '/', value[2]);
        else if (strlen(value) == 5)
            strcpy(langs[r->k].date, value);
        return;
    }
    if (!strcmp(key, "CONFIRM_BUTTON")) {
        langs[r->k].circle = !strcasecmp(value, "CIRCLE");
        return;
    }
    if (!value[0])   /* (not translated yet) */
        return;
    if (r->next < T_COUNT && !strcmp(ids[r->next], key))
        id = r->next;
    else
        for (id = 1; id < T_COUNT && strcmp(ids[id], key); id++)
            ;
    if (id < T_COUNT) {
        langs[r->k].text[id] = value;
        langs[r->k].japanese |= has_japanese(value);
        r->next = id + 1;
    }
}

static void load(void)
{
    int k, id;
    if (nLangs)
        return;
    for (k = 0; k < nLangFiles && nLangs < MAX_LANGS; k++) {
        /* read where it is, not copied: the values are left in the text, each one ended by a 0 (a copy of each
         * language was memory a big card file opened in Files needs) */
        unsigned int n = *langFiles[k].size - 1;
        char *text = (char *)langFiles[k].text;
        reading_t r = {nLangs, 1};
        langs[nLangs].code = langs[nLangs].name = langFiles[k].code;
        strcpy(langs[nLangs].date, "Y-M-D");
        lang_parse(text, n, on_text, &r);
        if (!strcmp(langFiles[k].code, "en"))
            english = nLangs;
        nLangs++;
    }
    for (k = 0; k < nLangs; k++)
        for (id = 1; id < T_COUNT; id++)
            if (!langs[k].text[id])
                langs[k].text[id] = langs[english].text[id];
    current = english;
}

int i18n_count(void)
{
    load();
    return nLangs;
}

const char *i18n_code(int k) { return k >= 0 && k < i18n_count() ? langs[k].code : ""; }
const char *i18n_name(int k) { return k >= 0 && k < i18n_count() ? langs[k].name : ""; }

int i18n_find(const char *code)
{
    int k;
    size_t n = strcspn(code, "-_");   /* ("pt-BR" is pt) */
    for (k = 0; k < i18n_count(); k++)
        if (strlen(langs[k].code) == n && !strncasecmp(langs[k].code, code, n))
            return k;
    return -1;
}

/* the language the PS2 is set to (its settings, in the console's browser) */
static const char *console_language(void)
{
    switch (configGetLanguage()) {
    case LANGUAGE_JAPANESE: return "ja";
    case LANGUAGE_FRENCH: return "fr";
    case LANGUAGE_SPANISH: return "es";
    case LANGUAGE_GERMAN: return "de";
    case LANGUAGE_ITALIAN: return "it";
    case LANGUAGE_DUTCH: return "nl";
    case LANGUAGE_PORTUGUESE: return "pt";
    default: return "en";
    }
}

void i18n_select(const char *language)
{
    int k = strcmp(language, "auto") ? i18n_find(language) : -1;
    if (k < 0)
        k = i18n_find(console_language());
    current = k < 0 ? english : k;
    pad_set_circle_confirms(langs[current].circle);
    if (langs[current].japanese)
        font_japanese(0);   /* (the screen is locked, or not drawn yet) */
}

int i18n_japanese(void)
{
    load();
    return langs[current].japanese;
}

void i18n_device(const char *name)
{
    static const char old[] = "sd2psx";
    int k, id;
    load();
    for (k = 0; k < nLangs; k++)
        for (id = 1; id < T_COUNT; id++) {   /* (the ones of the device before) */
            free((char *)langs[k].renamed[id]);
            langs[k].renamed[id] = NULL;
        }
    if (!strcmp(name, old))
        return;
    for (k = 0; k < nLangs; k++)
        for (id = 1; id < T_COUNT; id++) {
            const char *s = langs[k].text[id], *p;
            char *out, *q;
            size_t n = 0;
            for (p = s; p && (p = strstr(p, old)) != NULL; p += sizeof(old) - 1)
                n++;
            if (!n || !(out = malloc(strlen(s) + n * strlen(name) + 1)))
                continue;
            for (p = s, q = out; *p;)
                if (!strncmp(p, old, sizeof(old) - 1)) {
                    strcpy(q, name);
                    q += strlen(name);
                    p += sizeof(old) - 1;
                } else
                    *q++ = *p++;
            *q = 0;
            langs[k].renamed[id] = out;
        }
}

const char *T(int id)
{
    load();
    if (id <= 0 || id >= T_COUNT || !langs[current].text[id])
        return "?";
    return langs[current].renamed[id] ? langs[current].renamed[id] : langs[current].text[id];
}

void i18n_date(char *out, size_t size, int year, int month, int day)
{
    const char *f;
    int v[3], i;
    load();
    f = langs[current].date;   /* D/M/Y: a letter, the separator, a letter... */
    for (i = 0; i < 3; i++)
        v[i] = f[2 * i] == 'Y' ? year : f[2 * i] == 'M' ? month : day;
    snprintf(out, size, "%0*d%c%0*d%c%0*d", f[0] == 'Y' ? 4 : 2, v[0], f[1], f[2] == 'Y' ? 4 : 2, v[1], f[3],
             f[4] == 'Y' ? 4 : 2, v[2]);
}
