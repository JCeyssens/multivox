#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>

#include "gadget.h"
#include "rammel.h"

#include "gpio.h"

volatile uint8_t *rp1_base;
volatile uint32_t *timer_uS;

#define GPIO_FUNC_RIO 5

static void gpio_set_func_rio(int pin) {
    volatile uint32_t *ctrl = rp1_reg(RP1_IO_BANK0 + (uint32_t)pin * 8 + 4);
    uint32_t val = *ctrl;
    val &= ~0x1fu;
    val |= GPIO_FUNC_RIO;
    val &= ~(0x3u << 12); /* OUTOVER: drive from the selected function */
    val &= ~(0x3u << 14); /* OEOVER: output-enable from the selected function */
    *ctrl = val;
}

void gpio_init_pull(int pin, int pud) {
    /* pud: 0 off, 1 up, 2 down. Pulls live in the pad control register. */
    volatile uint32_t *pad = rp1_reg(RP1_PADS0 + 0x04 + (uint32_t)pin * 4);
    uint32_t bits = *pad;
    bits &= ~(0x3u << 2);
    if (pud == GPIO_PUD_UP) {
        bits |= (1u << 3);
    } else if (pud == GPIO_PUD_DOWN) {
        bits |= (1u << 2);
    }
    *pad = bits;
}

void gpio_init_in(int pin) {
    gpio_set_func_rio(pin);
    *rp1_reg(RP1_SYS_RIO0 + RIO_OE + RIO_CLR) = 1u << pin;

    volatile uint32_t *pad = rp1_reg(RP1_PADS0 + 0x04 + (uint32_t)pin * 4);
    uint32_t bits = *pad;
    bits |= (1u << 6);  /* input enable */
    bits |= (1u << 7);  /* output disable at the pad */
    *pad = bits;
}

void gpio_init_out(int pin) {
    gpio_set_func_rio(pin);
    *rp1_reg(RP1_SYS_RIO0 + RIO_OE + RIO_SET) = 1u << pin;

    gpio_init_pull(pin, GPIO_PUD_OFF);

    volatile uint32_t *pad = rp1_reg(RP1_PADS0 + 0x04 + (uint32_t)pin * 4);
    uint32_t bits = *pad;
    bits |= (1u << 6);  /* input enable */
    bits &= ~(1u << 7); /* allow the pad to drive */
    *pad = bits;
}

static bool gpio_mapmem(void) {
    int memfd;

    if ((memfd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("Can't open /dev/mem (must be root)");
        return false;
    }

    void *rp1 = mmap(NULL, RP1_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, (off_t)RP1_IO_PHYS);
    void *timer_base = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, (off_t)TIMER_PHYS);
    close(memfd);

    if (rp1 == MAP_FAILED || timer_base == MAP_FAILED) {
        perror("mmap error");
        return false;
    }

    rp1_base = (volatile uint8_t *)rp1;
    timer_uS = (volatile uint32_t *)((uint8_t *)timer_base + 4); /* CLO, lower 32 bits of the 1 MHz counter */

    return true;
}

bool gpio_init(void) {
    if (!gpio_mapmem()) {
        return false;
    }

    gpio_init_in(SPIN_SYNC);
#ifdef SPIN_SYNC_PULLUP
    gpio_init_pull(SPIN_SYNC, GPIO_PUD_UP);
#else
    gpio_init_pull(SPIN_SYNC, GPIO_PUD_OFF);
#endif

    for (int i = 0; i < count_of(matrix_init_out); ++i) {
        if (matrix_init_out[i] == SPIN_SYNC) {
            continue;
        }
        gpio_init_out(matrix_init_out[i]);
    }

    gpio_set_pin(RGB_BLANK);

    return true;
}
