#include "Target.h"
#include "audioMixer.h"
#include "errno.h"
#include "LED_RINGS.h"
#include <stdatomic.h>

pthread_mutex_t PointMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t LedID = 0;

uint64_t P1_points = 0;
uint64_t P2_points = 0;

atomic_bool stop = false;


static void *target_ir_worker(void *arg);


void EntityInit( TargetEntity* target , IRReceiver* rec , LED* led_addr , wavedata_t* hit_sound , wavedata_t* activate_sound)
{
    target->rec = rec;
    target->led = led_addr;
    target->ActivateSound = activate_sound;
    target->HitSound = hit_sound;
    target->TargetNumber = led_addr->RING_NUMBER;
    
    target->Actived_timestamp = 0;
    target->active = 0;

    target->PointValue = 10;

    target->IR_thread = 0;

    set_led(target->TargetNumber , BLUE);
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

int ActivateTarget(TargetEntity* target , uint64_t duration_ms , uint8_t points )
{
    //reset the state

    pthread_create( &target->IR_thread , NULL , target_ir_worker , NULL);
    pthread_join(target->IR_thread , NULL);

    return 0;
}



static void *target_ir_worker(void *arg)
{

    TargetEntity *target = arg;
    int accepted = NO_HIT;

    AudioMixer_queueSound(target->ActivateSound);

    while (monotonic_ns() < target->deadline_ns) 
    {
        int result = read_IR_REC(target->rec,
                                target->Actived_timestamp,
                                target->deadline_ns);

        if (result < 0) {
            accepted = -1;
            break;
        }

        if (result == P1_HIT || result == P2_HIT) {
            accepted = result;
            break;
        }
    }
    

    target->ir_result = accepted;

    if (accepted == P1_HIT || accepted == P2_HIT) {
        pthread_mutex_lock(&PointMutex);

        AudioMixer_queueSound(target->HitSound);

        if (accepted == P1_HIT) {
            P1_points += target->PointValue;
        } else {
            P2_points += target->PointValue;
        }
        pthread_mutex_unlock(&PointMutex);

        if (accepted == P1_HIT) {
           flash_seq(target, BLUE, GREEN);
        } else {
            flash_seq(target, YELLOW, WHITE);
        }
    }

    DeactivateTarget(target);
    return NULL;
}

 void DeactivateTarget(TargetEntity* target)
 {
    set_led(target->TargetNumber, BLUE);
    target->active = false;
 }

// Call from main after starting all the targets you want active.

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