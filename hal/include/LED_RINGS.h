
#ifndef LED_RINGS_H
#define LED_RINGS_H

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>



#define RING_COUNT 1
#define RGB_SIZE 3


// Some base colours for RGB STRUCT
#define RED     (RGB){ 255 , 0   , 0     }
#define GREEN   (RGB){ 0   , 255 , 0     }
#define BLUE    (RGB){ 0   , 0   , 255   }


#define LED_PRINT_FORMAT " fd : %d\n USB_START : %d\n USB_CMD : %d\n USB_SIZE : %d\n RING : %d\n RED : %d\n GREEN :  %d\n BLUE : %d\n"

enum {
    RINGS = 4,
    PIXELS_PER_RING = 8,
    RESET_BYTES = 128
};



typedef struct 
{

    uint8_t R;
    uint8_t G;
    uint8_t B;

} RGB;

typedef struct 
{
    uint8_t RING_NUMBER;
    RGB rgb;

} LED;

extern pthread_mutex_t led_lock;
extern RGB LED_RINGS[RINGS];
extern uint8_t frame[2 * RESET_BYTES + RINGS * PIXELS_PER_RING * 9];

void led_init();
void set_led( LED led);
void update_leds();
void led_close();

#endif