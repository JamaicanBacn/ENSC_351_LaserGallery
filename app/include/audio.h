#ifndef AUDIO_H
#define AUDIO_H

typedef enum {
    AUDIO_GAME_MUSIC,
    AUDIO_CELEBRATION,
    AUDIO_JAR_HIT,
    AUDIO_MUMMY_HIT,
    AUDIO_SOUND_COUNT
} AudioSound;

// Load the gallery WAVs and start the mixer. Returns -1 if a WAV cannot be loaded.
int audio_init(void);
// Queue one playback of a loaded sound.
void audio_play(AudioSound sound);
// Stop playback and release the WAV data.
void audio_cleanup(void);

#endif
