

#ifndef IR_REC_H
#define IR_REC_H

#include "gpio.h"
#include "LED_RINGS.h"

#define ID_LENGTH 3
#define PLAYER1_CODE 0x1
#define PLAYER2_CODE 0x2

#define US      1000
#define _100US  100 * US
#define _500US  5 * _100US
#define _2MS    20 * _100US
#define _200US  2 * _100US
#define _400US  4 * _100US
#define _1MS    10 * _100US
#define _800US  8 * _100US



enum IR_STATES : uint8_t
{
    IDLE_STATE,
    WAIT_PREAMBLE_RISING,
    WAIT_PREABMLE_GAP,

    WAIT_BIT_RISING,
    WAIT_BIT_FALLING, 
    IR_STATES

};

typedef struct {
    GPIOCHIP gpio;  // gpio.request belongs to this target.
    unsigned int offset;
    struct gpiod_edge_event_buffer *events;
    uint64_t prev_stamp_ns;
    uint8_t state;

    bool have_start;

    uint8_t playerID;
    uint8_t bits_received;

} IRReceiver;

extern IRReceiver receivers[RINGS];

int ir_receiver_init( IRReceiver *rec , char* chip_path , unsigned int offset);
int read_IR_REC(IRReceiver *rec);

#endif