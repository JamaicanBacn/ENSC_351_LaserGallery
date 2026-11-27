
#include "LED_RINGS.h"

static int SPI_FD = -1;

pthread_mutex_t led_lock = PTHREAD_MUTEX_INITIALIZER;

RGB LED_RINGS[RINGS] = {0};
uint8_t frame[2 * RESET_BYTES + RINGS * PIXELS_PER_RING * 9] = {0};

void encode_channel(uint8_t value, uint8_t out[3])
{
    uint32_t encoded = 0;

    for (int bit = 7; bit >= 0; --bit) {
        uint32_t symbol = (value & (1u << bit)) ? 0x6u : 0x4u;
        encoded = (encoded << 3) | symbol;
    }

    out[0] = (uint8_t)(encoded >> 16);
    out[1] = (uint8_t)(encoded >> 8);
    out[2] = (uint8_t)encoded;
}

void led_init()
{
    SPI_FD = open("/dev/spidev0.0", O_WRONLY);

    if (SPI_FD < 0) {
        perror("open /dev/spidev0.0");
        exit(1);
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits_per_word = 8;
    uint32_t speed_hz = 2400000;

    if (ioctl(SPI_FD, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(SPI_FD, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 ||
        ioctl(SPI_FD, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {

        perror("configure SPI");
        close(SPI_FD);
        exit(1);
    }
}

void set_led( int ring , RGB rgb )
{
    // before changing the color make thread safe
    pthread_mutex_lock(&led_lock);
    LED_RINGS[ring] = rgb;
    pthread_mutex_unlock(&led_lock);

}

void update_leds()
{

    uint8_t *pixel = frame + RESET_BYTES;
    
    pthread_mutex_lock(&led_lock);

    for (int ring = 0; ring < RINGS; ++ring) {
        for (int led = 0; led < PIXELS_PER_RING; ++led) {
            encode_channel(LED_RINGS[ring].G, pixel);
            encode_channel(LED_RINGS[ring].R, pixel + RGB_SIZE);
            encode_channel(LED_RINGS[ring].B, pixel + RGB_SIZE * 2);
            pixel += 9;
        }
    }

    ssize_t written = write(SPI_FD, frame, sizeof(frame));

    pthread_mutex_unlock(&led_lock);

    if (written < 0) {
        perror("write SPI frame");
        exit(1);
    }

    if (written != (ssize_t)sizeof frame) {
        fprintf(stderr, "Incomplete LED frame: %zd bytes\n", written);
        exit(1);
    }

}

void led_close()
{
    close(SPI_FD);
}