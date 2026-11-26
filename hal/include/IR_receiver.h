

#ifndef IR_REC_H
#define IR_REC_H

#include "gpio.h"
#include "LED_RINGS.h"

typedef struct {
    GPIOCHIP gpio;  // gpio.request belongs to this target.
    unsigned int offset;
    struct gpiod_edge_event_buffer *events;
    uint64_t falling_ns;
    bool have_start;
} IRReceiver;

extern IRReceiver receivers[RINGS];

int ir_receiver_init( IRReceiver *rec , char* chip_path , unsigned int offset);
int read_IR_REC(IRReceiver *rec);

#endif