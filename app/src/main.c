

#include "LED_RINGS.h"
#include "IR_receiver.h"

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

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



int main()
{
 //make an array of the offsets for the rotary encoder
    uint32_t offsets[RINGS] = { gpiochip2_offsets.GPIO_18_OFFSET,
                                gpiochip2_offsets.GPIO_15_OFFSET,
                                gpiochip2_offsets.GPIO_14_OFFSET,
                                gpiochip2_offsets.GPIO_17_OFFSET };

    //Init the gpiochup structure

    /*
    for( int i = 0; i < RINGS ; i++)
    {
        ir_receiver_init( &receivers[i] , GPIOCHIP_2_PATH , offsets[i]);
    }
    */
    
    IRReceiver receiver;
    IRReceiver* rec_ptr = &receiver;

    ir_receiver_init(rec_ptr , GPIO_PATH , offsets[0]);

    while(1)
    {
        if( read_IR_REC(rec_ptr) == 1)
        {
            printf("PLAYER1 HIT\n");
        }
    }
    
}