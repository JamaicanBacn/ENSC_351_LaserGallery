

#include "LED_RINGS.h"
#include "IR_receiver.h"
#include "audioMixer.h"
#include "Target.h"
#include "audio.h"

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>

#define GPIO_PATH   "/dev/gpiochip2"
#define SECOND 1000000


const GPIOCHIP0_OFFSETS gpiochip0_offsets = {
    .GPIO_2_OFFSET  = 18,
    .GPIO_3_OFFSET  = 17,
    .GPIO_7_OFFSET  = 9,
    .GPIO_8_OFFSET  = 0,
    .GPIO_9_OFFSET  = 4,
    .GPIO_10_OFFSET = 3,
    .GPIO_11_OFFSET = 2,
    .GPIO_23_OFFSET = 7,
    .GPIO_24_OFFSET = 10
};

const GPIOCHIP1_OFFSETS gpiochip1_offsets = {
    .GPIO_4_OFFSET  = 38,
    .GPIO_22_OFFSET = 41,
    .GPIO_25_OFFSET = 42,
    .GPIO_26_OFFSET = 36,
    .GPIO_27_OFFSET = 33
};

const GPIOCHIP2_OFFSETS gpiochip2_offsets = {
    .GPIO_5_OFFSET  = 15,
    .GPIO_6_OFFSET  = 17,
    .GPIO_12_OFFSET = 16,
    .GPIO_13_OFFSET = 18,
    .GPIO_14_OFFSET = 14,
    .GPIO_15_OFFSET = 13,
    .GPIO_16_OFFSET = 7,
    .GPIO_17_OFFSET = 8,
    .GPIO_18_OFFSET = 11,
    .GPIO_19_OFFSET = 12,
    .GPIO_20_OFFSET = 10,
    .GPIO_21_OFFSET = 9
};

TargetEntity Targets[RINGS];

void CompleteGameClean();

void GameSetup(uint32_t* offsets);

void Game1();
void Game2();
void Game3();
void Game5();
void Game6();

int main()
{
 //make an array of the offsets for the rotary encoder
    uint32_t offsets[RINGS] = { gpiochip2_offsets.GPIO_14_OFFSET,
                                gpiochip2_offsets.GPIO_15_OFFSET,
                                gpiochip2_offsets.GPIO_18_OFFSET,
                                gpiochip2_offsets.GPIO_17_OFFSET,
                                gpiochip2_offsets.GPIO_12_OFFSET };

    //Init the gpiochup structure


    GameSetup(offsets);

    
    int status = EXIT_SUCCESS;
    if (ActivateTarget(&Targets[0], 5000, 50) < 0) {
        fprintf(stderr, "Could not read IR from target 1\n");
        status = EXIT_FAILURE;
    }
    if (ActivateTarget(&Targets[4], 3000, 10) < 0) {
        fprintf(stderr, "Could not read IR from target 5\n");
        status = EXIT_FAILURE;
    }

    CompleteGameClean();    


    printf("FINAL SCORES: Player1 -> %" PRIu64 ", Player2 -> %" PRIu64 "\n", P1_points, P2_points);
    return status;
}

void CompleteGameClean()
{
    audio_cleanup();

    for (int i = 0; i < RINGS; i++) 
    {
        if (receivers[i].events != NULL) 
        {
            gpiod_edge_event_buffer_free(receivers[i].events);
            receivers[i].events = NULL;
        }

        gpio_cleanup(&receivers[i].gpio);
    }

    END();
}


void GameSetup( uint32_t* offsets)
{

    audio_init();
    led_init();
    AudioMixer_queueSound(&sounds[AUDIO_GAME_MUSIC]);
 

    //Init all of the receivers

    for( int i = 0; i < RINGS ; i++)
    {
        if( ir_receiver_init( &receivers[i] , GPIOCHIP_2_PATH , offsets[i]) < 0 )
        {
            // if the Ir rec fails then end the program
            exit(1);
        }
    }

    EntityInit(&Targets[0] , &receivers[0] , &LED_RINGS[0] , &sounds[AUDIO_MUMMY_HIT] , &sounds[AUDIO_MUMMY_SPAWN]);
    EntityInit(&Targets[1] , &receivers[1] , &LED_RINGS[1] , &sounds[AUDIO_POT_BREAK] , &sounds[AUDIO_POT_SPAWN]);
    EntityInit(&Targets[2] , &receivers[2] , &LED_RINGS[2] , &sounds[AUDIO_POT_BREAK] , &sounds[AUDIO_POT_SPAWN]);
    EntityInit(&Targets[3] , &receivers[3] , &LED_RINGS[3] , &sounds[AUDIO_MUMMY_HIT] , &sounds[AUDIO_MUMMY_SPAWN]);
    EntityInit(&Targets[4] , &receivers[4] , &LED_RINGS[4] , &sounds[AUDIO_POT_BREAK] , &sounds[AUDIO_POT_SPAWN]);


}