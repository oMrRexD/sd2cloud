/* SD2Cloud -- English and Portuguese. "auto" follows the PS2's language (Portuguese -> pt, anything else -> en) */
#include <stdlib.h>
#include <string.h>
#include <osd_config.h>
#include "common.h"

static const char *const english[T_COUNT] = {
    [T_NONE] = "",
#define X(id, en, pt) [id] = en,
#include "messages.def"
#undef X
};

static const char *const portuguese[T_COUNT] = {
    [T_NONE] = "",
#define X(id, en, pt) [id] = pt,
#include "messages.def"
#undef X
};

static const char *const *current = english;

/* the texts for a device that isn't the sd2psx: the ones that name it, with that device's name instead */
static const char *renamed[2][T_COUNT];
static int otherDevice;

void i18n_device(const char *name)
{
    static const char old[] = "sd2psx";
    const char *const *table[2] = {english, portuguese};
    int k, id;
    for (k = 0; k < 2; k++)   /* (the ones of the device before) */
        for (id = 1; id < T_COUNT; id++) {
            free((char *)renamed[k][id]);
            renamed[k][id] = NULL;
        }
    otherDevice = 0;
    if (!strcmp(name, old))
        return;
    for (k = 0; k < 2; k++)
        for (id = 1; id < T_COUNT; id++) {
            const char *s = table[k][id], *p;
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
            renamed[k][id] = out;
        }
    otherDevice = 1;
}

void i18n_select(const char *language)
{
    if (strcmp(language, "pt") == 0 || strcmp(language, "pt-BR") == 0)
        current = portuguese;
    else if (strcmp(language, "en") == 0)
        current = english;
    else
        current = (configGetLanguage() == LANGUAGE_PORTUGUESE) ? portuguese : english;
}

int i18n_is_pt(void) { return current == portuguese; }

const char *T(int id)
{
    if (id <= 0 || id >= T_COUNT || !current[id])
        return "?";
    if (otherDevice && renamed[current == portuguese][id])
        return renamed[current == portuguese][id];
    return current[id];
}
