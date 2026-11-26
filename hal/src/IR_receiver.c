
#include "IR_receiver.h"
#include "time.h"
#include <gpiod.h>

#define MS_IN_NS 1000000

IRReceiver receivers[RINGS] = {0};

static long long getTimeInMs(void)
{
    struct timespec spec;
    clock_gettime( CLOCK_REALTIME, &spec);
    long long seconds = spec.tv_sec;
    long long nanoSeconds = spec.tv_nsec;
    long long milliSeconds = seconds * 1000 + nanoSeconds / 1000000;

    return milliSeconds;

}

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

bool in_range( uint64_t target , uint64_t value)
{
    return target - _100US <= value && target + _100US >= value;
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

    rec->events = gpiod_edge_event_buffer_new(16);
    if (rec->events == NULL) {
        perror("gpiod_edge_event_buffer_new");
        gpio_cleanup(&rec->gpio);
        return -1;
    }

    return 0;
}

int read_IR_REC(IRReceiver *rec)
{
    /*
        Pulse:
            GPIO low for 450-800 us is detected as a shot.

        1:
            Check for a falling edge and record when it happens.
    */


    while( 1 )
    {
        
        int ready = gpiod_line_request_wait_edge_events(
            rec->gpio.request, 100 * MS_IN_NS);

        if (ready < 0) {
            perror("wait for IR edge");
            return -1;
        }

        if (ready == 0) {
            rec->state = IDLE_STATE;
            rec->prev_stamp_ns = 0;
            rec->playerID = 0;
            rec->bits_received = 0;
            continue;
        }

        // If there was a GPIO edge event, read the queued events.
        // count is the number of GPIO events read into the buffer.
        int count = gpiod_line_request_read_edge_events(
            rec->gpio.request, rec->events, 16);

        if (count < 0) {
            perror("read IR edges");
            return -1;
        }

        int hits = 0;

        for (int i = 0; i < count; i++) {
            // Get the event and its type.
            struct gpiod_edge_event *event =
                gpiod_edge_event_buffer_get_event(rec->events, i);
            enum gpiod_edge_event_type type =
                gpiod_edge_event_get_event_type(event);

            // The timestamp for the current event.
            uint64_t stamp_ns = gpiod_edge_event_get_timestamp_ns(event);

            uint64_t  edge_time_diff = stamp_ns - rec->prev_stamp_ns;

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
                        if( in_range(_500US , edge_time_diff) )
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
                        if( in_range(_2MS , edge_time_diff))
                        {
                            rec->state = WAIT_BIT_RISING;
                            rec->prev_stamp_ns = stamp_ns;
                        }
                        else
                        {
                            rec->state = IDLE_STATE;
                            rec->prev_stamp_ns = 0;
                        }
                    }
                    break;
                case WAIT_BIT_RISING:
                    
                    if( type == GPIOD_EDGE_EVENT_RISING_EDGE)
                    {
                        uint8_t bit = 0;

                        if( in_range( _400US , edge_time_diff))
                        {
                            bit = 1;
                        }
                        else if( in_range( _800US , edge_time_diff))
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

                        if( rec->bits_received == ID_LENGTH)
                        {
                            if (rec->playerID == PLAYER1_CODE) {
                                printf("PLAYER 1 HIT\n");
                                ++hits;
                            }

                            rec->state = IDLE_STATE;
                            rec->playerID = 0;
                            rec->bits_received = 0;
                            rec->prev_stamp_ns = 0;
                        } 
                        else
                        {
                        rec->prev_stamp_ns = stamp_ns;
                        rec->state = WAIT_BIT_FALLING;
                        }
                    }
                    break;
                case WAIT_BIT_FALLING:
                    if( type == GPIOD_EDGE_EVENT_FALLING_EDGE)
                    {
                        if( in_range( _1MS , edge_time_diff))
                        {
                            rec->state = WAIT_BIT_RISING;
                            rec->prev_stamp_ns = stamp_ns;
                        }
                    }
                    

            }
          
        }
    }

}

