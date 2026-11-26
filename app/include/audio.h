#ifndef AUDIO_H
#define AUDIO_H

#include "audioMixer.h"

typedef enum {
    AUDIO_GAME_MUSIC,
    AUDIO_CELEBRATION,
    AUDIO_JAR_HIT,
    AUDIO_MUMMY_HIT,
    AUDIO_SOUND_COUNT
} AudioSound;

extern wavedata_t sounds[AUDIO_SOUND_COUNT];

void audio_init(void);

#endif
