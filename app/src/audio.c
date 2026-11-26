#include "audio.h"

wavedata_t sounds[AUDIO_SOUND_COUNT];

void audio_init(void)
{
    static char *const wavPaths[AUDIO_SOUND_COUNT] = {
        [AUDIO_GAME_MUSIC]  = "wave-files/game_music.wav",
        [AUDIO_CELEBRATION] = "wave-files/game_celebration.wav",
        [AUDIO_JAR_HIT]     = "wave-files/jar_hit.wav",
        [AUDIO_MUMMY_HIT]   = "wave-files/mummy_hit.wav",
    };

    AudioMixer_init();
    for (int i = 0; i < AUDIO_SOUND_COUNT; ++i) {
        AudioMixer_readWaveFileIntoMemory(wavPaths[i], &sounds[i]);
    }
}
