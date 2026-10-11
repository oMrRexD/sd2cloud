/* SD2Cloud -- the texts on screen, in the language of the console or of the settings (i18n.c) */
#ifndef I18N_H
#define I18N_H

#include <stddef.h>
#include <tamtypes.h>

#include "messages.h"
/* the files of lang/ built into the program, one per language (the Makefile writes the list: langs.c) */
typedef struct {
    const char *code;            /* the file's name: en, pt, es... (what sd2cloud.ini's language takes) */
    const unsigned char *text;
    const unsigned int *size;
} lang_file_t;
extern const lang_file_t langFiles[];
extern const int nLangFiles;

void i18n_select(const char *language);    /* "auto" (the PS2's language) or a language's code; also which button
                                               confirms, the language's CONFIRM_BUTTON */
const char *T(int id);
/* the texts name the sd2psx: on another device, they name that one instead (once, before the first screen) */
void i18n_device(const char *name);
int i18n_count(void);                       /* the languages there are, for the settings: */
const char *i18n_code(int k);               /* each one's code */
const char *i18n_name(int k);               /* and its name, in itself ("Español") */
int i18n_find(const char *code);            /* the one of that code (-1 = none) */
/* a date as the screen's language writes it: 31/12/2026, 12/31/2026 or 2026-12-31 */
void i18n_date(char *out, size_t size, int year, int month, int day);

#endif
