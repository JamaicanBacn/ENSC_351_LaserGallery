#include "Target.h"
#include "audioMixer.h"
#include "errno.h"
#include "LED_RINGS.h"

pthread_mutex_t PointLock;

uint64_t P1_points = 0;
uint64_t P2_points = 0;


void EntityInit( TargetEntity* target , IRReceiver* rec , LED* led_addr , wavedata_t* hit_sound , wavedata_t* activate_sound)
{
    target->rec = rec;
    target->led = led_addr;
    target->ActivateSound = activate_sound;
    target->HitSound = hit_sound;
    target->TargetNumber = led_addr->RING_NUMBER;
    
    target->Actived_timestamp = 0;
    target->Current_timeStamp = 0;
    target->active = 0;

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

static uint64_t monotonic_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

void ActivateTarget(TargetEntity* target , uint64_t duration_ms , uint8_t points )
{
    if (target->ir_thread_started) {
        return EBUSY;
    }

    target->ir_result = NO_HIT;
    target->deadline_ms = monotonic_ms() + duration_ms;


    int err = pthread_create(&target->ir_thread, NULL,
                             target_ir_worker, target);
    if (err != 0) {
        return err;
    }

    target->ir_thread_started = true;
    target->active = true;
    AudioMixer_queueSound(target->ActivateSound);
    
    return 0;

}

static void *target_ir_worker(void *arg)
{
    TargetEntity *target = arg;

    set_led(target->TargetNumber , RED);

    while (monotonic_ms() < target->deadline_ms) { // checks we are in the deadline
        int result = read_IR_REC(target->rec);

        if (result < 0) {
            target->ir_result = -1;
            break;
        }

        if (result != NO_HIT && monotonic_ms() < target->deadline_ms) {
            target->ir_result = result;
            break;
        }
    }

    if( target->ir_result == P1_HIT )
    {
    
    }
    else if( target->ir_result == P2_HIT)
    {

    }
    else
    {
        
    }

    return NULL;
}


// Call from main after starting all the targets you want active.
int FinishTarget(TargetEntity *target)
{
    if (!target->ir_thread_started) {
        return -1;
    }

    if (pthread_join(target->ir_thread, NULL) != 0) {
        return -1;
    }

    target->ir_thread_started = false;
    return target->ir_result;
}

void Closetarget()
{

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