#include "Target.h"
#include "audioMixer.h"


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

void ActivateTarget(TargetEntity* Entity)
{
    Entity->active = true; // activate the target
    pthread_t IrThread;

    // play activate sound
    AudioMixer_queueSound(Entity->ActivateSound);

    pthread_create( IrThread , );
}

void DeactivateTarget()
{

}

void Closetarget()
{

}