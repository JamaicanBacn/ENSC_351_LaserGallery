#include "audio.h"
#include "audioMixer.h"

#include <stdbool.h>

static const char *const wavPaths[AUDIO_SOUND_COUNT] = {
    [AUDIO_GAME_MUSIC] = "wave-files/game_music.wav",
    [AUDIO_CELEBRATION] = "wave-files/game_celebration.wav",
    [AUDIO_JAR_HIT] = "wave-files/jar_hit.wav",
    [AUDIO_MUMMY_HIT] = "wave-files/mummy_hit.wav",
};

static wavedata_t sounds[AUDIO_SOUND_COUNT];
static bool initialized;

int audio_init(void)
{
    if (initialized) {
        return 0;
    }

    for (int i = 0; i < AUDIO_SOUND_COUNT; ++i) {
        if (AudioMixer_readWaveFileIntoMemory(wavPaths[i], &sounds[i]) < 0) {
            for (int loaded = 0; loaded < i; ++loaded) {
                AudioMixer_freeWaveFileData(&sounds[loaded]);
            }
            return -1;
        }
    }

    AudioMixer_init();
    initialized = true;
    return 0;
}

void audio_play(AudioSound sound)
{
    if (initialized && (unsigned int)sound < AUDIO_SOUND_COUNT) {
        AudioMixer_queueSound(&sounds[sound]);
    }
}

void audio_cleanup(void)
{
    if (initialized) {
        AudioMixer_cleanup();
        for (int i = 0; i < AUDIO_SOUND_COUNT; ++i) {
            AudioMixer_freeWaveFileData(&sounds[i]);
        }
        initialized = false;
    }
}
