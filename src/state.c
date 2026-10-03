/*
 * SD2Cloud -- what the app remembers from one run to the next, in its data folder
 * (in <microSD>SD2Cloud/, apart from the program: updating the APPS/SD2Cloud folder doesn't touch it):
 *   state.ini -- ids of the Drive folders and, per card, the fingerprint and the SHA-256 of the last verified backup
 *   token.dat -- the access to Google (refresh token). Deleting it = connecting again.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "common.h"

#ifndef TEST
#define STATE_FILE "state.ini"
#else
#define STATE_FILE "state-test.ini"   /* the test build doesn't touch the real state */
#endif
#define MAX_FOLDERS (2 * MAX_CARDS + 8)   /* each card folder and its Saves folder, and the main one */

static card_state_t states[MAX_CARDS];
static int nStates;
static struct {
    char name[128];   /* "<main folder>/<card folder>/Saves" whole: cut short, two folders would share a place */
    char id[80];
} folders[MAX_FOLDERS];
static int nFolders;
char refreshToken[512];

#define COPY(dest, v) snprintf(dest, sizeof(dest), "%s", v)

card_state_t *state_card(const char *id, int create)
{
    int i;
    for (i = 0; i < nStates; i++)
        if (!strcasecmp(states[i].id, id))
            return &states[i];
    if (!create || nStates >= MAX_CARDS)
        return NULL;
    memset(&states[nStates], 0, sizeof(states[0]));
    COPY(states[nStates].id, id);
    return &states[nStates++];
}

const char *state_folder(const char *name)
{
    int i;
    for (i = 0; i < nFolders; i++)
        if (!strcmp(folders[i].name, name))
            return folders[i].id;
    return NULL;
}

void state_set_folder(const char *name, const char *id)
{
    int i;
    for (i = 0; i < nFolders; i++)
        if (!strcmp(folders[i].name, name)) {
            COPY(folders[i].id, id);
            return;
        }
    if (nFolders < MAX_FOLDERS) {
        COPY(folders[nFolders].name, name);
        COPY(folders[nFolders++].id, id);
    }
}

int state_empty(void)
{
    int i;
    for (i = 0; i < nStates; i++)
        if (states[i].sha[0])
            return 0;
    return 1;
}

static void on_state(const char *s, const char *k, const char *v, void *u)
{
    (void)u;
    if (!strcasecmp(s, "folders")) {
        state_set_folder(k, v);
        return;
    }
    if (strchr(s, '/')) {   /* [Card1/Card1-1] */
        card_state_t *e = state_card(s, 1);
        if (!e)
            return;
        if (!strcasecmp(k, "fingerprint")) COPY(e->fingerprint, v);
        else if (!strcasecmp(k, "fingerprint_version")) e->version = atoi(v);
        else if (!strcasecmp(k, "sha256")) COPY(e->sha, v);
        else if (!strcasecmp(k, "when")) COPY(e->when, v);
    }
}

void state_read(void)
{
    char path[260];
    nStates = nFolders = 0;
    snprintf(path, sizeof(path), "%s" STATE_FILE, dataDir);
    ini_read(path, on_state, NULL);
}

int state_write(void)
{
    buffer_t b = {0};
    char path[260], t[512];
    int i, r;
    const char *head = "; SD2Cloud state -- written by the app. Deleting it is safe: SD2Cloud finds its Drive folders again\r\n"
                       "; and backs up every card once more.\r\n";
    buf_append(&b, head, strlen(head));
    buf_append(&b, "[folders]\r\n", 11);
    for (i = 0; i < nFolders; i++) {
        snprintf(t, sizeof(t), "%s = %s\r\n", folders[i].name, folders[i].id);
        buf_append(&b, t, strlen(t));
    }
    for (i = 0; i < nStates; i++) {
        snprintf(t, sizeof(t), "[%s]\r\nfingerprint = %s\r\nfingerprint_version = %d\r\nsha256 = %s\r\nwhen = %s\r\n", states[i].id,
                 states[i].fingerprint, states[i].version, states[i].sha, states[i].when);
        buf_append(&b, t, strlen(t));
    }
    snprintf(path, sizeof(path), "%s" STATE_FILE, dataDir);
    ensure_data_dir();
    r = file_replace(path, b.data, b.len);
    buf_free(&b);
    return r;
}

static void on_token(const char *s, const char *k, const char *v, void *u)
{
    (void)s;
    (void)u;
    if (!strcasecmp(k, "refresh_token"))
        COPY(refreshToken, v);
}

void token_read(void)
{
    char path[260];
    refreshToken[0] = 0;
    snprintf(path, sizeof(path), "%stoken.dat", dataDir);
    ini_read(path, on_token, NULL);
}

int token_write(void)
{
    char path[260], t[800];
    snprintf(t, sizeof(t),
             "; SD2Cloud access to Google Drive (only to the files SD2Cloud creates). Delete it to link again.\r\n"
             "refresh_token = %s\r\n",
             refreshToken);
    snprintf(path, sizeof(path), "%stoken.dat", dataDir);
    ensure_data_dir();
    return file_replace(path, (unsigned char *)t, strlen(t));
}
