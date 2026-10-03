/*
 * SD2Cloud -- the interface sounds.
 *
 * Each sound is a short ADPCM sample (the .adp files in assets/sounds, made by tools/make_sounds.py with the SDK's
 * adpenc) that audsrv sends once to the sound processor (SPU2), which then plays it by itself whenever it is asked
 * to. If the audio modules don't load, the app simply stays silent.
 */
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <audsrv.h>
#include <ps2_audio_driver.h>
#include "common.h"
#include "sound.h"

#define ASSET(name) extern unsigned char name[]; extern unsigned int size_##name;
ASSET(asset_startup_adp)
ASSET(asset_exit_adp)
ASSET(asset_move_adp)
ASSET(asset_confirm_adp)
ASSET(asset_back_adp)
#undef ASSET

typedef struct {
    unsigned char *data;
    unsigned int *size;
    audsrv_adpcm_t sample;
    int loaded, lengthMs;
    u64 playedAt;   /* when it was last started (0 = never) */
} sound_t;

static sound_t sounds[SND_COUNT] = {
    [SND_STARTUP] = {asset_startup_adp, &size_asset_startup_adp},
    [SND_EXIT] = {asset_exit_adp, &size_asset_exit_adp},
    [SND_MOVE] = {asset_move_adp, &size_asset_move_adp},
    [SND_CONFIRM] = {asset_confirm_adp, &size_asset_confirm_adp},
    [SND_BACK] = {asset_back_adp, &size_asset_back_adp},
};
static int audioOn;

static unsigned int le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

/* how long a sample lasts, from its .adp header: "APCM", version, channels, loop flag, then the pitch (4096 = 48 kHz)
 * at byte 8 and the number of samples at byte 12 */
static int length_ms(const unsigned char *header)
{
    unsigned int pitch = le32(header + 8), samples = le32(header + 12);
    return pitch ? (int)((u64)samples * 1000 * 4096 / (48000ULL * pitch)) : 0;
}

static sound_t *playable(int id)
{
    return (audioOn && id >= 0 && id < SND_COUNT && sounds[id].loaded) ? &sounds[id] : NULL;
}

int sound_init(void)
{
    int i, r;
    if ((r = init_audio_driver()) < 0 || audsrv_adpcm_init() != 0) {
        log_msg("sound: no audio (%d): the app stays silent", r);
        return -1;
    }
    for (i = 0; i < 24; i++)   /* the SPU2's 24 voices, full volume, centered */
        audsrv_adpcm_set_volume_and_pan(i, MAX_VOLUME, 0);
    for (i = 0; i < SND_COUNT; i++) {
        sound_t *s = &sounds[i];
        s->lengthMs = length_ms(s->data);
        FlushCache(0);   /* the sample goes to the IOP by DMA, read straight from memory */
        s->loaded = audsrv_load_adpcm(&s->sample, s->data, *s->size) == 0;
        if (!s->loaded)
            log_msg("sound: sample %d didn't load", i);
    }
    audioOn = 1;
    log_msg("sound: ready (startup %d ms, exit %d ms)", sounds[SND_STARTUP].lengthMs, sounds[SND_EXIT].lengthMs);
    return 0;
}

void sound_play(int id)
{
    sound_t *s = playable(id);
    if (!s)
        return;
    audsrv_ch_play_adpcm(-1, &s->sample);   /* -1: any free voice */
    s->playedAt = now_ms();
}

void sound_wait(int id)
{
    sound_t *s = playable(id);
    u64 until;
    if (!s || !s->playedAt)
        return;
    /* until the sample has had time to finish, never more than 3 s */
    until = s->playedAt + (s->lengthMs < 3000 ? s->lengthMs : 3000) + 60;
    while (now_ms() < until)
        sleep_ms(20);
}

void sound_end(void)
{
    if (audioOn) {
        deinit_audio_driver();
        audioOn = 0;
    }
}
