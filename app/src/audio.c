#include "audio.h"

wavedata_t sounds[AUDIO_SOUND_COUNT];

void audio_init(void)
{
    static char *const wavPaths[AUDIO_SOUND_COUNT] = {
        [AUDIO_GAME_MUSIC]  = "wave-files/game_music.wav",
        [AUDIO_CELEBRATION] = "wave-files/game_celebration.wav",
        [AUDIO_POT_BREAK]   = "wave-files/pot_break.wav",
        [AUDIO_MUMMY_DEATH] = "wave-files/mummy_death .wav",
        [AUDIO_POT_SPAWN]   = "wave-files/pot_spawn.wav",
        [AUDIO_MUMMY_SPAWN] = "wave-files/mummy_spawn.wav",
    };

    AudioMixer_init();
    for (int i = 0; i < AUDIO_SOUND_COUNT; ++i) {
        AudioMixer_readWaveFileIntoMemory(wavPaths[i], &sounds[i]);
    }
}

void audio_cleanup(void)
{
    AudioMixer_cleanup();
    for (int i = 0; i < AUDIO_SOUND_COUNT; ++i) {
        AudioMixer_freeWaveFileData(&sounds[i]);
    }
}
