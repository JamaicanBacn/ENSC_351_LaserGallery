
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
    int ready = gpiod_line_request_wait_edge_events(
        rec->gpio.request, MS_IN_NS);

    if (ready < 0) {
        perror("wait for IR edge");
        return -1;
    }

    if (ready == 0) {
        return 0;
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

        // Check if the event was a falling edge.
        if (type == GPIOD_EDGE_EVENT_FALLING_EDGE) {
            rec->falling_ns = stamp_ns;  // Record the falling timestamp.
            rec->have_start = true;     // Note that the burst has begun.
        } else if (type == GPIOD_EDGE_EVENT_RISING_EDGE && rec->have_start) {
            /*
                We have a saved falling edge, and the current edge is rising.
            */
            uint64_t pulse_ns = stamp_ns - rec->falling_ns;  // Measure the difference.
            rec->have_start = false;

            printf("LOW pulse: %.1f us\n", pulse_ns / 1000.0);

            if (pulse_ns >= 450000ULL && pulse_ns <= 800000ULL) {
                ++hits;
            }
        }
    }

    return hits;
}

