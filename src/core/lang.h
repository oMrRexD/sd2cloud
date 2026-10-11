/* SD2Cloud -- a file of lang/ read: the texts of one language (lang.c) */
#ifndef LANG_H
#define LANG_H

#include <stddef.h>

/* Each "key = value" of text, n bytes with room for a 0 after them, handed to cb. The text is changed in place (the
 * escapes undone, a 0 after each key and each value), so what cb gets lasts as long as text does. Returns how many */
int lang_parse(char *text, size_t n, void (*cb)(const char *key, const char *value, void *u), void *u);

#endif
