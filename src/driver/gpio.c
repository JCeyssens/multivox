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

/* RP1 io_bank0 FUNCSEL value that routes the pad to the SYS_RIO
   signal (function 5 in the RP1 GPIO function table; 6 is PROC_RIO,
   which is driven by the RP1's own processor and not visible from
   the host's sys_rio0 registers). */
#define GPIO_FUNC_RIO 5

static void gpio_set_func_rio(int pin) {
    /* io_bank0: GPIO<n>_STATUS at n*8, GPIO<n>_CTRL at n*8+4.
       OUTOVER [13:12] and OEOVER [15:14] select override mode; 0 means
       drive from the function selected by FUNCSEL. */
    volatile uint32_t *ctrl = rp1_reg(RP1_IO_BANK0 + (uint32_t)pin * 8 + 4);
    uint32_t val = *ctrl;
    val &= ~0x1fu;
    val |= GPIO_FUNC_RIO;
    val &= ~(0x3u << 12); /* OUTOVER: drive from the selected function */
    val &= ~(0x3u << 14); /* OEOVER: output-enable from the selected function */
    *ctrl = val;
}

void gpio_init_pull(int pin, int pud) {
    /* pud: 0 off, 1 up, 2 down. Pulls live in the pad control register:
       PUE is bit 3, PDE is bit 2. */
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
    bits |= (1u << 6);  /* IE: input enable */
    bits |= (1u << 7);  /* OD: output disable at the pad (overrides the function) */
    *pad = bits;
}

void gpio_init_out(int pin) {
    gpio_set_func_rio(pin);
    *rp1_reg(RP1_SYS_RIO0 + RIO_OE + RIO_SET) = 1u << pin;

    gpio_init_pull(pin, GPIO_PUD_OFF);

    volatile uint32_t *pad = rp1_reg(RP1_PADS0 + 0x04 + (uint32_t)pin * 4);
    uint32_t bits = *pad;
    bits |= (1u << 6);  /* IE: input enable (keep the pad readable) */
    bits &= ~(1u << 7); /* OD: clear output disable so the pad can drive */
    *pad = bits;
}

static bool gpio_mapmem(void) {
    /*
     * Pi 5: the header GPIOs are on the RP1, reached over PCIe at
     * 0x1f00000000. The kernel exposes that window as /dev/gpiomem0
     * (rootless) covering io_bank0, sys_rio0 and pads; /dev/mem is the
     * fallback. The 1 MHz system timer lives in the BCM2712 SoC and is
     * only reachable through /dev/mem.
     */
    int rp1fd = open("/dev/gpiomem0", O_RDWR | O_SYNC);
    size_t rp1_len = RP1_WINDOW_SIZE; /* the gpiomem driver only accepts a
                                         mapping of the full window (else EINVAL) */
    off_t rp1_off = 0; /* the gpiomem0 window base is io_bank0 */

    if (rp1fd < 0) {
        rp1_len = RP1_MAP_SIZE;
        rp1_off = (off_t)RP1_IO_PHYS;
        if ((rp1fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
            perror("Can't open /dev/gpiomem0 or /dev/mem (must be root)");
            return false;
        }
    }

    int memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (memfd < 0) {
        perror("Can't open /dev/mem for the timer (must be root)");
        close(rp1fd);
        return false;
    }

    void *rp1 = mmap(NULL, rp1_len, PROT_READ | PROT_WRITE, MAP_SHARED, rp1fd, rp1_off);
    if (rp1 == MAP_FAILED) {
        perror("mmap RP1 GPIO window (/dev/gpiomem0 or /dev/mem)");
        close(rp1fd);
        close(memfd);
        return false;
    }

    void *timer_base = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, (off_t)TIMER_PHYS);
    close(rp1fd);
    close(memfd);

    if (timer_base == MAP_FAILED) {
        perror("mmap BCM2712 1 MHz timer (/dev/mem)");
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
