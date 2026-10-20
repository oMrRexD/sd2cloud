/* SD2Cloud -- the little of JSON that Google's and GitHub's answers need (json.c) */
#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include <tamtypes.h>

typedef struct {
    const char *p, *end;
} json_t;
int js_string(json_t *j, char *out, size_t size);
int js_skip(json_t *j, int depth);
int js_find(const char *txt, size_t n, const char *key, json_t *v);
int js_get_string(const char *txt, size_t n, const char *key, char *out, size_t size);
int js_get_number(const char *txt, size_t n, const char *key, long long *out);
int js_get_object(const char *txt, size_t n, const char *key, const char **start, size_t *len);   /* nested object */
int js_each(const char *txt, size_t n, const char *key, void (*cb)(const char *obj, size_t n, void *u), void *u);
/* text for inside a JSON string: quotes and backslashes escaped, control characters dropped; what doesn't fit is
 * left out a whole character at a time (half of a kanji's UTF-8 would make the JSON invalid) */
void js_escape(const char *s, char *out, size_t size);

#endif
