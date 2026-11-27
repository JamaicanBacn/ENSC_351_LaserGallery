#include "Target.h"
#include "audioMixer.h"
#include "errno.h"


void EntityInit( TargetEntity* Entity , IRReceiver* rec , LED* led_addr , wavedata_t* hit_sound , wavedata_t* activate_sound)
{
    Entity->rec = rec;
    Entity->led = led_addr;
    Entity->ActivateSound = activate_sound;
    Entity->HitSound = hit_sound;
    Entity->TargetNumber = led_addr->RING_NUMBER;
    
    Entity->Actived_timestamp = 0;
    Entity->Current_timeStamp = 0;
    Entity->active = 0;
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

    DeactivateTarget(target);
    return NULL;
}


void DeactivateTarget( TargetEntity* target)
{
    if (!target->ir_thread_started ||
        pthread_join(target->ir_thread, NULL) != 0) {
        return -1;
    }

    target->ir_thread_started = false;
    target->active = false;
    return target->ir_result;
}

void Closetarget()
{

}