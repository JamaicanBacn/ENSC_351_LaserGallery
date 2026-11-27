#include "Target.h"
#include "audioMixer.h"
#include "errno.h"
#include "LED_RINGS.h"
#include <inttypes.h>
#include <stdatomic.h>

pthread_mutex_t PointMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t target_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t target_finished = PTHREAD_COND_INITIALIZER;

// Protected by PointMutex. Separate histories preserve simultaneous shots from both players.
static uint64_t last_hit_ns[PLAYERS][RINGS];

static void *target_ir_worker(void *arg);

pthread_t LedID = 0;

uint64_t P1_points = 0;
uint64_t P2_points = 0;

atomic_bool stop = false;


void EntityInit( TargetEntity* target , IRReceiver* rec , LED* led_addr , wavedata_t* hit_sound , wavedata_t* activate_sound)
{
    target->rec = rec;
    target->led = led_addr;
    target->ActivateSound = activate_sound;
    target->HitSound = hit_sound;
    target->TargetNumber = led_addr->RING_NUMBER;
    
    target->active = false;
    target->ir_result = NO_HIT;

    target->PointValue = 10;

    set_led(target->TargetNumber, BLUE);
}

static void sleepForMs(long long delayInMs)
{
    const long long NS_PER_MS = 1000 * 1000;
    const long long NS_PER_SECOND = 1000000000;
    long long delayNs = delayInMs * NS_PER_MS;
    int seconds = delayNs / NS_PER_SECOND;
    int nanoseconds = delayNs % NS_PER_SECOND;
    struct timespec reqDelay = {seconds, nanoseconds};
    nanosleep(&reqDelay, (struct timespec *) NULL);
}

static uint64_t monotonic_ns(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    return (uint64_t)now.tv_sec * UINT64_C(1000000000)
         + (uint64_t)now.tv_nsec;
}

int ActivateTarget(TargetEntity *target, uint64_t duration_ms, uint8_t points)
{
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (error != 0) {
        return error;
    }
    error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (error != 0) {
        pthread_attr_destroy(&attr);
        return error;
    }

    pthread_mutex_lock(&target_lock);
    if (target->active) {
        error = EBUSY;
    } else {
        target->active = true;
        target->duration_ms = duration_ms;
        target->PointValue = points;
        target->ir_result = NO_HIT;

        pthread_t thread;
        error = pthread_create(&thread, &attr, target_ir_worker, target);
        if (error != 0) {
            target->active = false;
        }
    }
    pthread_mutex_unlock(&target_lock);
    pthread_attr_destroy(&attr);
    return error;
}

static bool score_hit(const TargetEntity *target, int player, uint64_t stamp_ns)
{
    pthread_mutex_lock(&PointMutex);
    for (int ring = 0; ring < RINGS; ++ring) {
        uint64_t previous_ns = last_hit_ns[player][ring];
        uint64_t gap_ns = stamp_ns >= previous_ns
            ? stamp_ns - previous_ns
            : previous_ns - stamp_ns;

        // Two receivers can decode the same packet with slightly different edge timings.
        // A 1 ms window is shorter than one complete IR packet.
        if (previous_ns != 0 && gap_ns <= _1MS) {
            pthread_mutex_unlock(&PointMutex);
            return false;
        }
    }

    last_hit_ns[player][target->TargetNumber] = stamp_ns;
    if (player == P1_HIT) {
        P1_points += target->PointValue;
    } else {
        P2_points += target->PointValue;
    }
    pthread_mutex_unlock(&PointMutex);
    return true;
}

static void *target_ir_worker(void *arg)
{
    TargetEntity *target = arg;
    int accepted = NO_HIT;
    if (ir_receiver_prepare(target->rec) < 0) {
        accepted = -1;
        goto finished;
    }

    uint64_t activated_ns = monotonic_ns();
    set_led(target->TargetNumber, RED);
    uint64_t deadline_ns = monotonic_ns() + target->duration_ms * UINT64_C(1000000);
    AudioMixer_queueSound(target->ActivateSound);
    const bool trace_hits = getenv("GALLERY_TRACE_HITS") != NULL;

    while (monotonic_ns() < deadline_ns)
    {
        uint64_t hit_stamp_ns;
        int result = read_IR_REC(target->rec, activated_ns, deadline_ns, &hit_stamp_ns);

        if (result < 0) {
            accepted = -1;
            break;
        }

        if (result == P1_HIT || result == P2_HIT) {
            bool scored = score_hit(target, result, hit_stamp_ns);
            if (trace_hits) {
                fprintf(stderr, "Target %u, GPIO %u: player %d hit at %" PRIu64 " ns (%s)\n",
                        (unsigned int)target->TargetNumber + 1, target->rec->offset,
                        result, hit_stamp_ns, scored ? "flash" : "duplicate");
            }
            if (scored) {
                accepted = result;
                break;
            }
        }
    }
    

    if (accepted == P1_HIT || accepted == P2_HIT) {
        AudioMixer_queueSound(target->HitSound);

        if (accepted == P1_HIT) {
           flash_seq(target, BLUE, GREEN);
        } else {
            flash_seq(target, YELLOW, WHITE);
        }
    }

finished:
    DeactivateTarget(target);
    pthread_mutex_lock(&target_lock);
    target->ir_result = accepted;
    target->active = false;
    pthread_cond_broadcast(&target_finished);
    pthread_mutex_unlock(&target_lock);
    return NULL;
}

int WaitForTarget(TargetEntity *target)
{
    pthread_mutex_lock(&target_lock);
    while (target->active) {
        pthread_cond_wait(&target_finished, &target_lock);
    }
    int result = target->ir_result;
    pthread_mutex_unlock(&target_lock);
    return result;
}

 void DeactivateTarget(TargetEntity* target)
 {
    set_led(target->TargetNumber, BLUE);
 }

void flash_seq( TargetEntity* target , RGB colour1 , RGB colour2)
{
    for( int i = 0; i < 5; i++)
    {
        set_led( target->TargetNumber ,  colour1);
        sleepForMs(100);
        set_led(target->TargetNumber , colour2);
        sleepForMs(100);
        
    }

    return;
}

void END()
{
    led_close();

}