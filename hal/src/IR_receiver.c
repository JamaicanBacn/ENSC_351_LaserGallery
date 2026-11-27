#include "IR_receiver.h"
#include <gpiod.h>
#include <inttypes.h>

#define MS_IN_NS 1000000
// The default kernel queue holds 16 events for one requested line.
#define IR_EDGE_BATCH_SIZE 16

enum {
    // Keep a gap between the accepted 400 us and 800 us bit widths.
    IR_PULSE_TOLERANCE_NS = 150 * US,
    IR_GAP_TOLERANCE_NS = 250 * US
};

IRReceiver receivers[RINGS] = {0};

static bool in_range(uint64_t target, uint64_t value, uint64_t tolerance)
{
    return target - tolerance <= value && value <= target + tolerance;
}

int ir_receiver_init(IRReceiver *rec, char *chip_path, unsigned int offset)
{
    *rec = (IRReceiver){0};
    rec->gpio.gpio_path = chip_path;
    rec->offset = offset;
    rec->gpio.offsets = &rec->offset;

    // Request exactly one GPIO line.
    if (gpio_init(&rec->gpio, &rec->offset, 1) < 0) {
        gpio_cleanup(&rec->gpio);
        return -1;
    }

    rec->events = gpiod_edge_event_buffer_new(IR_EDGE_BATCH_SIZE);
    if (rec->events == NULL) {
        perror("gpiod_edge_event_buffer_new");
        gpio_cleanup(&rec->gpio);
        return -1;
    }

    return 0;
}

void ir_receiver_cleanup(IRReceiver *rec)
{
    if (rec->events != NULL) {
        gpiod_edge_event_buffer_free(rec->events);
        rec->events = NULL;
    }
    gpio_cleanup(&rec->gpio);
}

int ir_receiver_prepare(IRReceiver *rec)
{
    rec->edges_read = 0;
    resetReceiver(rec);

    // Discard the inactive target's queued edges before opening its shot window.
    int ready = gpiod_line_request_wait_edge_events(rec->gpio.request, 0);
    if (ready < 0 ||
        (ready > 0 && gpiod_line_request_read_edge_events(
            rec->gpio.request, rec->events, IR_EDGE_BATCH_SIZE) < 0)) {
        perror("clear queued IR edges");
        return -1;
    }
    return 0;
}

int read_IR_REC(IRReceiver *rec, uint64_t activated_ns, uint64_t deadline_ns,
                uint64_t *hit_stamp_ns)
{
    // The receiver is active-low: a falling edge starts each pulse.
    int ready = gpiod_line_request_wait_edge_events(
        rec->gpio.request, 100 * MS_IN_NS);

    if (ready < 0) {
        perror("wait for IR edge");
        return -1;
    }

    if (ready == 0) {
        resetReceiver(rec);
        return NO_HIT;
    }

    // If there was a GPIO edge event, read the queued events.
    // count is the number of GPIO events read into the buffer.
    int count = gpiod_line_request_read_edge_events(
        rec->gpio.request, rec->events, IR_EDGE_BATCH_SIZE);

    if (count < 0) {
        perror("read IR edges");
        return -1;
    }

    rec->edges_read += count;
    for (int i = 0; i < count; i++) {
        // Get the event and its type.
        struct gpiod_edge_event *event =
            gpiod_edge_event_buffer_get_event(rec->events, i);
        enum gpiod_edge_event_type type =
            gpiod_edge_event_get_event_type(event);

        // The timestamp for the current event.
        uint64_t stamp_ns = gpiod_edge_event_get_timestamp_ns(event);

        // GPIO queues edges even while the target is inactive.
        if (stamp_ns < activated_ns || stamp_ns >= deadline_ns) {
            resetReceiver(rec);
            continue;
        }

        uint64_t edge_time_diff = stamp_ns - rec->prev_stamp_ns;

        switch ( rec->state )
        {
            case IDLE_STATE:
                if( type == GPIOD_EDGE_EVENT_FALLING_EDGE)
                {
                    rec->prev_stamp_ns = stamp_ns;
                    rec->state = WAIT_PREAMBLE_RISING;
                }
                break;
            case WAIT_PREAMBLE_RISING:
                if( type == GPIOD_EDGE_EVENT_RISING_EDGE)
                {
                    if (in_range(_500US, edge_time_diff, IR_PULSE_TOLERANCE_NS))
                    {       
                        rec->prev_stamp_ns = stamp_ns;
                        rec->state = WAIT_PREABMLE_GAP;
                    } 
                    else
                    {
                        rec->prev_stamp_ns = 0;
                        rec->state = IDLE_STATE;
                    }
                }
                break;
            case WAIT_PREABMLE_GAP:
                if( type == GPIOD_EDGE_EVENT_FALLING_EDGE)
                {
                    if (in_range(_2MS, edge_time_diff, IR_GAP_TOLERANCE_NS))
                    {
                        rec->state = WAIT_BIT_RISING;
                        rec->prev_stamp_ns = stamp_ns;
                    }
                    else
                    {
                        // This edge can start a new preamble after a bad gap.
                        rec->state = WAIT_PREAMBLE_RISING;
                        rec->prev_stamp_ns = stamp_ns;
                    }
                }
                break;
            case WAIT_BIT_RISING:
                
                if( type == GPIOD_EDGE_EVENT_RISING_EDGE)
                {
                    uint8_t bit = 0;

                    if (in_range(_800US, edge_time_diff, IR_PULSE_TOLERANCE_NS))
                    {
                        bit = 1;
                    }
                    else if (in_range(_400US, edge_time_diff, IR_PULSE_TOLERANCE_NS))
                    {
                        bit = 0;
                    }
                    else{
                        rec->state = IDLE_STATE;
                        rec->playerID = 0;
                        rec->bits_received = 0;
                        rec->prev_stamp_ns = 0;
                        break;
                    }

                    rec->playerID = (rec->playerID << 1) | bit;
                    ++rec->bits_received;

                    if (rec->bits_received == ID_LENGTH) {

                        uint8_t playerID = rec->playerID;
                        resetReceiver(rec);
                        
                        if (playerID == PLAYER1_CODE)
                        {
                            *hit_stamp_ns = stamp_ns;
                            return P1_HIT;
                        }

                        if( playerID == PLAYER2_CODE)
                        {
                            *hit_stamp_ns = stamp_ns;
                            return P2_HIT;
                        }

                    } else {
                        rec->prev_stamp_ns = stamp_ns;
                        rec->state = WAIT_BIT_FALLING;
                    }
                }
                break;
                case WAIT_BIT_FALLING:
                    if (type == GPIOD_EDGE_EVENT_FALLING_EDGE) {
                        if (in_range(_1MS, edge_time_diff, IR_GAP_TOLERANCE_NS)) {
                            rec->state = WAIT_BIT_RISING;
                        } else {
                            // Reuse this falling edge as a possible preamble start.
                            rec->playerID = 0;
                            rec->bits_received = 0;
                            rec->state = WAIT_PREAMBLE_RISING;
                        }
                        rec->prev_stamp_ns = stamp_ns;
                    }
                    break;
        }  // End of switch.
    }      // End of the event for loop.

    return NO_HIT;
}

void resetReceiver(IRReceiver* rec)
{
    rec->state = IDLE_STATE;
    rec->prev_stamp_ns = 0;
    rec->playerID = 0;
    rec->bits_received = 0;

    return;
}

