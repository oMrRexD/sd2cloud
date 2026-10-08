/* SD2Cloud -- templates: sets of saves kept on the microSD, to be put into cards (templates.c) */
#ifndef TEMPLATES_H
#define TEMPLATES_H

#include <stddef.h>
#include <tamtypes.h>
#include "mcfs.h"

/* A template is a folder of <data folder>/templates, named after it, with one .psu file for each of its saves: the
 * format the program already reads and writes, so a .psu put there with a PC is part of the template too */
#define TPL_MAX   16
#define TPL_SAVES 32
#define TPL_NAME  24             /* how long a name can be */
typedef struct {
    char folder[33];             /* the save's folder, as a card has it */
    char file[80];               /* its .psu, in the template's folder */
    unsigned long long when;     /* when it was last saved (as mcfs_save_t has it) */
    long long bytes;             /* its files' sizes added up */
} tpl_save_t;
typedef struct {
    char name[TPL_NAME + 1];
    int n;
    tpl_save_t saves[TPL_SAVES];
    long long bytes;             /* its saves' sizes added up */
} template_t;
extern template_t templates[TPL_MAX];
extern int nTemplates;

int templates_scan(void);        /* the templates on the microSD, by name. Returns how many */
template_t *template_find(const char *name);
/* where the template's save i is (i < 0: its folder, ending in /) */
void template_path(const template_t *t, int i, char *out, size_t size);
/* can a template be given that name? 0, or why not: it has a character a folder's name can't have (or none at
 * all, or too many), another template has it, or there is no room for one more template */
enum { TPL_OK = 0, TPL_ERR_NAME = -20, TPL_ERR_TAKEN = -21, TPL_ERR_MANY = -22 };
int template_name_check(const char *name, const template_t *self);   /* self = the template being renamed, if one is */
/* Each of these leaves templates[] in order: a template may have moved in it afterwards */
template_t *template_new(const char *name);                    /* an empty one. NULL = it couldn't be made */
template_t *template_rename(template_t *t, const char *name);  /* NULL = it keeps the name it had */
int template_delete(template_t *t);                            /* with its saves. 0 = gone */
int template_find_save(const template_t *t, const char *folder);   /* which of its saves is that folder's (-1 = none) */
/* a save of a card into the template, as a .psu written and read back. It takes the place of the one of that folder
 * the template may have. 0 = ok, else MCFS_ERR_* (MCFS_ERR_FULL = the template has TPL_SAVES already) */
int template_add(template_t *t, const char *card, const char *folder);
int template_remove(template_t *t, int i);                     /* that save out of it. 0 = ok */
/* what a card lacks of a template: lacks[i] = 1 for each save of it the card doesn't have. Returns how many (-1 = the
 * card couldn't be read); bytes = those saves' sizes added up, freeBytes = the card's free space */
int template_lacking(const template_t *t, const char *card, unsigned char lacks[TPL_SAVES], long long *bytes, long long *freeBytes);
/* puts into a card (a .mcd the device is NOT using) the saves of the template it lacks, each one read back; a save
 * the card has is never touched. before(t, i) is told which save comes next and stops it all by answering != 0;
 * progress is mcfs's. Returns 0, or what stopped it (MCFS_ERR_*: the saves put before that stay); put = how many
 * went in */
int template_apply(const template_t *t, const char *card, int (*before)(const template_t *t, int i), mcfs_step_cb progress,
                   int *put);

/* The main template is the one every game card should have. What is settled about a card is remembered with it, in
 * templates.ini: the card was given the main template as it is now, or the user asked not to be told that it lacks
 * it. Only a card that isn't settled is looked into again; and a template that gets other saves is another one,
 * for that: its signature tells */
extern char tplMain[TPL_NAME + 1];                 /* the main template's name ("" = there is none) */
int templates_main_set(void);                      /* reads templates.ini alone: is there a main template? */
template_t *template_main(void);                   /* NULL = none (or its folder is gone) */
void template_set_main(const template_t *t);       /* NULL = none */
int template_settled(const template_t *t, const char *cardId);
void template_settle(const template_t *t, const char *cardId);
int templates_save(void);                          /* templates.ini written with what those two changed. 0 = written */

#endif
