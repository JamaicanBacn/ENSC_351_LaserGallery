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

    uint64_t duration_ms;

    //Sounds
    wavedata_t* HitSound;
    wavedata_t* ActivateSound;

    IRReceiver* rec;
    LED* led;

    uint64_t PointValue;

    int ir_result;

} TargetEntity;

extern uint64_t P1_points;
extern uint64_t P2_points;

extern atomic_bool stop;
extern pthread_t LedID;

void EntityInit( TargetEntity* Entity, IRReceiver* rec , LED* led_addr , wavedata_t* hit_sound , wavedata_t* activate_sound);
// Start a target asynchronously. Returns zero or a pthread error code.
int ActivateTarget(TargetEntity* target , uint64_t duration_ms , uint8_t points );
// Wait for this target to finish and return its IR result.
int WaitForTarget(TargetEntity *target);
void DeactivateTarget(TargetEntity* target);
void flash_seq( TargetEntity* target , RGB colour1 , RGB colour2);

void END(void);

#endif