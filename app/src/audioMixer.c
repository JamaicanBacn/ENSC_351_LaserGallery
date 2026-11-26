// Incomplete implementation of an audio mixer. Search for "REVISIT" to find things
// which are left as incomplete.
// Note: Generates low latency audio on BeagleBone Black; higher latency found on host.
#include "audioMixer.h"
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h> // needed for mixer


static snd_pcm_t *handle;

#define DEFAULT_VOLUME 80

#define SAMPLE_RATE 48000
#define NUM_CHANNELS 1
#define SAMPLE_SIZE (sizeof(short)) 			// bytes per sample
// Sample size note: This works for mono files because each sample ("frame') is 1 value.
// If using stereo files then a frame would be two samples.

static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL;


// Currently active (waiting to be played) sound bites
#define MAX_SOUND_BITES 30
typedef struct {
	// A pointer to a previously allocated sound bite (wavedata_t struct).
	// Note that many different sound-bite slots could share the same pointer
	// (overlapping cymbal crashes, for example)
	wavedata_t *pSound;

	// The offset into the pData of pSound. Indicates how much of the
	// sound has already been played (and hence where to start playing next).
	int location;
} playbackSound_t;
static playbackSound_t soundBites[MAX_SOUND_BITES];

// Playback threading
void* playbackThread(void* arg);
static atomic_bool stopping;
static pthread_t playbackThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t volumeMutex = PTHREAD_MUTEX_INITIALIZER;
static int volume = 0;

void AudioMixer_init(void)
{
	AudioMixer_setVolume(DEFAULT_VOLUME);

	memset(soundBites, 0, sizeof soundBites);
	atomic_store(&stopping, false);

	// Open the PCM output
	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Configure parameters of PCM output
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// 0.05 seconds per buffer
	if (err < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Allocate this software's playback buffer to be the same size as the
	// the hardware's playback buffers for efficient data transfers.
	// ..get info on the hardware buffers:
 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	// ..allocate playback buffer:
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

	// Launch playback thread:
	if (pthread_create(&playbackThreadId, NULL, playbackThread, NULL) != 0) {
		fprintf(stderr, "Unable to start the audio playback thread.\n");
		snd_pcm_close(handle);
		free(playbackBuffer);
		exit(EXIT_FAILURE);
	}
}


static uint32_t read32le(const unsigned char *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8
        | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

// Find the PCM data by chunk so LIST and other metadata are skipped.
int AudioMixer_readWaveFileIntoMemory(const char *fileName, wavedata_t *pSound)
{
    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        perror(fileName);
        return -1;
    }

    unsigned char riff[12];
    unsigned char chunk[8];
    bool haveFormat = false;
    unsigned int channels = 0;

    if (fread(riff, 1, sizeof riff, file) != sizeof riff ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        goto invalid;
    }

    while (fread(chunk, 1, sizeof chunk, file) == sizeof chunk) {
        uint32_t size = read32le(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char format[16];
            if (size < sizeof format ||
                fread(format, 1, sizeof format, file) != sizeof format) {
                goto invalid;
            }

            channels = format[2];
            if (format[0] != 1 || format[1] != 0 ||
                (channels != 1 && channels != 2) || format[3] != 0 ||
                read32le(format + 4) != SAMPLE_RATE ||
                format[14] != 16 || format[15] != 0) {
                goto invalid;
            }

            haveFormat = true;
            size -= sizeof format;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!haveFormat || size == 0 || size % (SAMPLE_SIZE * channels) != 0) {
                goto invalid;
            }

            size_t sampleCount = size / SAMPLE_SIZE;
            short *samples = malloc(size);
            if (samples == NULL) {
                perror(fileName);
                fclose(file);
                return -1;
            }

            if (fread(samples, SAMPLE_SIZE, sampleCount, file) != sampleCount) {
                free(samples);
                goto invalid;
            }

            if (channels == 2) {
                sampleCount /= 2;
                for (size_t i = 0; i < sampleCount; ++i) {
                    samples[i] = (short)(((int)samples[2 * i] + samples[2 * i + 1]) / 2);
                }
            }

            pSound->pData = samples;
            pSound->numSamples = (int)sampleCount;
            fclose(file);
            return 0;
        }

        if (fseek(file, (long)size + (size & 1u), SEEK_CUR) != 0) {
            goto invalid;
        }
    }

invalid:
    fprintf(stderr, "%s: expected a 48 kHz, 16-bit PCM mono or stereo WAV.\n", fileName);
    fclose(file);
    return -1;
}

void AudioMixer_freeWaveFileData(wavedata_t *pSound)
{
	pSound->numSamples = 0;
	free(pSound->pData);
	pSound->pData = NULL;
}

void AudioMixer_queueSound(wavedata_t *pSound)
{
	// Ensure we are only being asked to play "good" sounds:
	assert(pSound->numSamples > 0);
	assert(pSound->pData);

	// Insert the sound by searching for an empty sound bite spot
	/*
	 * REVISIT: Implement this:
	 * 1. Since this may be called by other threads, and there is a thread
	 *    processing the soundBites[] array, we must ensure access is threadsafe.
	 * 2. Search through the soundBites[] array looking for a free slot.
	 * 3. If a free slot is found, place the new sound file into that slot.
	 *    Note: You are only copying a pointer, not the entire data of the wave file!
	 * 4. After searching through all slots, if no free slot is found then print
	 *    an error message to the console (and likely just return vs asserting/exiting
	 *    because the application most likely doesn't want to crash just for
	 *    not being able to play another wave file.
	 */

	 int free_index = -1;

	 pthread_mutex_lock(&audioMutex);

	for(int i = 0; i < MAX_SOUND_BITES ; i++)
	{

		if( soundBites[i].pSound == NULL)
		{
			
			soundBites[i].pSound = pSound;
			soundBites[i].location = 0;
			pthread_mutex_unlock(&audioMutex);
			return;
			
		}
	}

	pthread_mutex_unlock(&audioMutex);
	return;






}

void AudioMixer_cleanup(void)
{


	// Stop the PCM generation thread
	atomic_store(&stopping, true);
	pthread_join(playbackThreadId, NULL);

	// Shutdown the PCM output, allowing any pending sound to play out (drain)
	snd_pcm_drain(handle);
	snd_pcm_close(handle);

	// Free playback buffer
	// (note that any wave files read into wavedata_t records must be freed
	//  in addition to this by calling AudioMixer_freeWaveFileData() on that struct.)
	free(playbackBuffer);
	playbackBuffer = NULL;


	fflush(stdout);
}


int AudioMixer_getVolume()
{
	// Return the cached volume; good enough unless someone is changing
	// the volume through other means and the cached value is out of date.
	pthread_mutex_lock(&volumeMutex);
	int current = volume;
	pthread_mutex_unlock(&volumeMutex);
	return current;
}

// Function copied from:
// http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code
// Written by user "trenki".
static void setVolumeLocked(int newVolume)
{
	// Ensure volume is reasonable; If so, cache it for later getVolume() calls.
	if (newVolume < 0 || newVolume > AUDIOMIXER_MAX_VOLUME) {
		fprintf(stderr, "ERROR: Volume must be between 0 and 100.\n");
		return;
	}
	volume = newVolume;

    long min, max;
    snd_mixer_t *mixerHandle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    // const char *selem_name = "PCM";	// For ZEN cape
    const char *selem_name = "Speaker";	// For USB Audio

    if (snd_mixer_open(&mixerHandle, 0) < 0) {
        return;
    }
    if (snd_mixer_attach(mixerHandle, card) < 0 ||
        snd_mixer_selem_register(mixerHandle, NULL, NULL) < 0 ||
        snd_mixer_load(mixerHandle) < 0) {
        snd_mixer_close(mixerHandle);
        return;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(mixerHandle, sid);

    if (elem != NULL && snd_mixer_selem_has_playback_volume(elem)) {
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_set_playback_volume_all(elem, min + volume * (max - min) / 100);
    }

    snd_mixer_close(mixerHandle);
}


void AudioMixer_setVolume(int value)
{
    pthread_mutex_lock(&volumeMutex);
    setVolumeLocked(value);
    pthread_mutex_unlock(&volumeMutex);
}

void AudioMixer_adjustVolume(int delta)
{
    pthread_mutex_lock(&volumeMutex);
    int value = volume + delta;
    if (value < 0) value = 0;
    if (value > AUDIOMIXER_MAX_VOLUME) value = AUDIOMIXER_MAX_VOLUME;
    if (value != volume) setVolumeLocked(value);
    pthread_mutex_unlock(&volumeMutex);
}

// Fill the buff array with new PCM values to output.
//    buff: buffer to fill with new PCM data from sound bites.
//    size: the number of *values* to store into buff
static void fillPlaybackBuffer(short *buff, int size)
{
	/*
	 * REVISIT: Implement this
	 * 1. Wipe the buff to all 0's to clear any previous PCM data.
	 *    Hint: use memset(); read the docs about its use of size.
	 * 2. Since this is called from a background thread, and soundBites[] array
	 *    may be used by any other thread, must synchronize this.
	 * 3. Loop through each slot in soundBites[], which are sounds that are either
	 *    waiting to be played, or partially already played:
	 *    - If the sound bite slot is unused, do nothing for this slot.
	 *    - Otherwise "add" this sound bite's data to the play-back buffer
	 *      (other sound bites needing to be played back will also add to the same data).
	 *      * Record that this portion of the sound bite has been played back by incrementing
	 *        the location inside the data where play-back currently is.
	 *      * If you have now played back the entire sample, free the slot in the
	 *        soundBites[] array.
	 *
	 * Notes on "adding" PCM samples:
	 * - PCM is stored as signed shorts (between SHRT_MIN and SHRT_MAX).
	 * - When adding values, ensure there is not an overflow. Any values which would
	 *   greater than SHRT_MAX should be clipped to SHRT_MAX; likewise for underflow.
	 * - Don't overflow any arrays!
	 * - Efficiency matters here! The compiler may do quite a bit for you, but it doesn't
	 *   hurt to keep it in mind. Here are some tips for efficiency and readability:
	 *   * If, for each pass of the loop which "adds" you need to change a value inside
	 *     a struct inside an array, it may be faster to first load the value into a local
	 *      variable, increment this variable as needed throughout the loop, and then write it
	 *     back into the struct inside the array after. For example:
	 *           int offset = myArray[someIdx].value;
	 *           for (int i =...; i < ...; i++) {
	 *               offset ++;
	 *           }
	 *           myArray[someIdx].value = offset;
	 *   * If you need a value in a number of places, try loading it into a local variable
	 *          int someNum = myArray[someIdx].value;
	 *          if (someNum < X || someNum > Y || someNum != Z) {
	 *              someNum = 42;
	 *          }
	 *          ... use someNum vs myArray[someIdx].value;
	 *
	 */

	pthread_mutex_lock(&audioMutex);
	
	memset( buff , 0 , size * sizeof(short));

	

	for( int i = 0; i < MAX_SOUND_BITES; i++)
	{
		playbackSound_t* current_sound = &soundBites[i];
		int location = current_sound->location;

		wavedata_t* pSound = current_sound->pSound;
		
		if( pSound == NULL) continue;

		int samples = pSound->numSamples;

		for(int j = 0; j < size && location < samples; j++ )
		{

			

			int PCM = buff[j] + pSound->pData[location];

			if( PCM >= SHRT_MAX )
			{
				buff[j] = SHRT_MAX;
			}
			else if( PCM <= SHRT_MIN )
			{
				buff[j] = SHRT_MIN;
			}
			else{

			buff[j] = PCM;
			}

			location++;
			
		}

		if( location == samples )
		{
			current_sound->pSound = NULL;
			current_sound->location = 0;
		}
		else
		{
			current_sound->location = location;
		}
	}

	pthread_mutex_unlock(&audioMutex);


}


void* playbackThread(void* _arg)
{
	while (!atomic_load(&stopping)) {
		// Generate next block of audio
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);

		// Output the audio
		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);

		// Check for (and handle) possible error conditions on output
		if (frames < 0) {
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n",
					frames);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && frames < playbackBufferSize) {
			fprintf(stderr, "Short write (expected %li, wrote %li)\n",
					playbackBufferSize, frames);
		}
	}

	return NULL;
}