#ifndef TARGET_H
#define TARGET_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "audioMixer.h"
#include "IR_receiver.h"
#include "LED_RINGS.h"
#include <stdatomic.h>

typedef struct _Entity
{
    // From left to right what target is it.
    uint8_t TargetNumber;

    bool active; // Is the entity active
    bool hit;   // Has the entity been hit

    // controls the time that it is activated
    uint64_t Actived_timestamp;
    uint64_t Current_timeStamp;

    //Sounds
    wavedata_t* HitSound;
    wavedata_t* ActivateSound;

    IRReceiver* rec;
    LED* led;

    uint64_t PointValue;

    atomic_bool worker_running;
    uint64_t deadline_ms;
    int ir_result;

} TargetEntity;

extern uint64_t P1_points;
extern uint64_t P2_points;

void EntityInit( TargetEntity* Entity, IRReceiver* rec , LED* led_addr , wavedata_t* hit_sound , wavedata_t* activate_sound);
void ActivateTarget();
void DeactivateTarget();
void Closetarget();

#endif