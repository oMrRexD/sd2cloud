/* SD2Cloud -- the interface sounds (sound.c) */
#ifndef SOUND_H
#define SOUND_H

enum {
    SND_STARTUP,   /* the app opens */
    SND_EXIT,      /* leaving (after a backup at IGR, or out of the menu) */
    SND_MOVE,      /* the cursor moves */
    SND_CONFIRM,   /* X */
    SND_BACK,      /* circle: back */
    SND_COUNT
};

int sound_init(void);      /* 0 = ok; without sound the app goes on in silence */
void sound_play(int id);
void sound_wait(int id);   /* until the sound that was played last with this id has finished */
void sound_end(void);

#endif
