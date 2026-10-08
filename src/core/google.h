/* SD2Cloud -- Google Drive: signing in, folders, uploads, downloads (google.c) */
#ifndef GOOGLE_H
#define GOOGLE_H

#include <stddef.h>
#include <tamtypes.h>
#include "system.h"
#include "files.h"
#include "cards.h"
#include "stream.h"

int google_init(void);
int google_has_access(void);                /* there is a refresh token */
void google_logout(int online);             /* revokes the access (online) and forgets it */
void google_forget(void);                   /* the access in use was another account's (another microSD): asked for again */
int google_refresh(void);                   /* 0 ok, -2 = must sign in again, -1 error */
/* sign-in with a code: calls show(url, code) and waits; cancel() != 0 gives up. 0 = ok, -1 = given up, else the id
 * of the message that says why it failed */
int google_login(void (*show)(const char *url, const char *code, int seconds_left), int (*cancel)(void));
int google_folder(const char *name, const char *parent, char *id, size_t idsize);   /* finds or creates */
typedef struct {
    char id[80];
    char name[128];
    char created[32];
    long long size;        /* of the .zip */
    char sha_mcd[65];      /* SHA-256 of the .mcd inside (appProperties; may be empty) */
} drive_file_t;
int google_list(const char *folder, const char *card, drive_file_t *list, int max);   /* oldest first */
int google_delete(const char *id);
/* uploads a card, streamed, to the folder; progress(read_mcd, total_mcd). id = the file created */
int google_upload(const card_t *c, const char *folder, const char *name, const datetime_t *t, stream_t *s,
                  int (*progress)(long long done, long long total), char *id, size_t idsize);
/* a small file from memory (a single save). 0 = ok */
int google_upload_buffer(const char *folder, const char *name, const char *description, const unsigned char *d, size_t n,
                         int (*progress)(long long done, long long total), char *id, size_t idsize);
/* downloads a Drive file, handing each piece to sink (!= 0 stops). 0 = ok, -2 = stopped by sink, -1 = error */
int google_download(const char *id, int (*sink)(const unsigned char *d, size_t n, void *u), void *u);
/* called while curl transfers (to watch the controller during an upload or download) */
void google_set_poll(void (*poll)(void));
extern char googleError[256];               /* why the last request failed, for the screen */
/* GET from GitHub (the API or a release asset; follow = follow redirects). returns the HTTP status, 0 = network */
long github_get(const char *url, buffer_t *b, int follow);

#endif
