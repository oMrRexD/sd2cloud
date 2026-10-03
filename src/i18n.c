/* SD2Cloud -- English and Portuguese. "auto" follows the PS2's language (Portuguese -> pt, anything else -> en) */
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
    return (id > 0 && id < T_COUNT && current[id]) ? current[id] : "?";
}
