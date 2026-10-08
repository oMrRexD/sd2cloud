/* SD2Cloud -- the texts on screen, in the language of the console or of the settings (i18n.c) */
#ifndef I18N_H
#define I18N_H

#include <stddef.h>
#include <tamtypes.h>

#include "messages.h"
void i18n_select(const char *language);    /* "auto", "en" or "pt" */
const char *T(int id);
/* the texts name the sd2psx: on another device, they name that one instead (once, before the first screen) */
void i18n_device(const char *name);
int i18n_is_pt(void);                       /* is the screen in Portuguese? */

#endif
