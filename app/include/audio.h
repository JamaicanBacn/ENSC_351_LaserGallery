#ifndef AUDIO_H
#define AUDIO_H

#include "audioMixer.h"

typedef enum {
    AUDIO_GAME_MUSIC,
    AUDIO_CELEBRATION,
    AUDIO_POT_BREAK,
    AUDIO_MUMMY_DEATH,
    AUDIO_POT_SPAWN,
    AUDIO_MUMMY_SPAWN,
    AUDIO_SOUND_COUNT,

    // Existing callers use the hit names.
    AUDIO_JAR_HIT = AUDIO_POT_BREAK,
    AUDIO_MUMMY_HIT = AUDIO_MUMMY_DEATH
} AudioSound;

extern wavedata_t sounds[AUDIO_SOUND_COUNT];

void audio_init(void);
void audio_cleanup(void);

#endif
