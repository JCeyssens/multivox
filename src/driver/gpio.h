#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdbool.h>
#include <stdint.h>

/*
 * Raspberry Pi 5 (BCM2712 + RP1). The 40-pin header is on the RP1 chip,
 * not the BCM2712 GPIO block, so there is no Pi 4 style peripheral base
 * (0xFE000000) to map - the RP1 sits behind a PCIe link at 0x1f00000000.
 * CPU physical addresses:
 *   RP1 io_bank0  0x1f000d0000   (GPIO_FUNC/STATUS registers)
 *   RP1 sys_rio0  0x1f000e0000   (host driven output/input registers)
 *   RP1 pads      0x1f000f0000   (pad control: pulls, enable, drive)
 *   BCM2712 1 MHz system timer  0x107c003000
 *
 * The RP1 window is exposed by the kernel as /dev/gpiomem0 (rootless);
 * see gpio_mapmem() in gpio.c.
 */
#define RP1_IO_PHYS   0x1F000D0000ULL
#define RP1_MAP_SIZE  0x21000  /* what the driver actually touches (io_bank0 .. pads) */
/* The kernel's /dev/gpiomem0 driver (raspberrypi-gpiomem.c) ignores the mmap
   offset and rejects any mapping shorter than the full concatenated window
   (io_bank0 + sys_rio0 + pads = 0x30000) with -EINVAL. Always map the whole
   window when going through gpiomem0. */
#define RP1_WINDOW_SIZE 0x30000

#define RP1_IO_BANK0  0x00000
#define RP1_SYS_RIO0  0x10000
#define RP1_PADS0     0x20000

/* sys_rio0: base registers at +0x00..0x0c, atomic SET/CLR aliases */
#define RIO_OUT       0x00
#define RIO_OE        0x04
#define RIO_IN        0x08
#define RIO_SET       0x2000
#define RIO_CLR       0x3000

#define TIMER_PHYS    0x107C003000ULL

/* pud: 0 off, 1 up, 2 down - same meaning as the old BCM2711 code */
#define GPIO_PUD_OFF  0
#define GPIO_PUD_UP   1
#define GPIO_PUD_DOWN 2

extern volatile uint8_t *rp1_base;
extern volatile uint32_t *timer_uS;

static inline volatile uint32_t *rp1_reg(uint32_t offset) {
    return (volatile uint32_t *)(rp1_base + offset);
}

static inline void gpio_busy_wait(uint32_t uS) {
    uint32_t start = *timer_uS;
    while (*timer_uS - start <= uS);
}

static inline void gpio_set_bits(uint32_t bits) {
    *rp1_reg(RP1_SYS_RIO0 + RIO_OUT + RIO_SET) = bits;
}
static inline void gpio_clear_bits(uint32_t bits) {
    *rp1_reg(RP1_SYS_RIO0 + RIO_OUT + RIO_CLR) = bits;
}
static inline void gpio_set_pin(int pin) {
    gpio_set_bits(1ul << pin);
}
static inline void gpio_clear_pin(int pin) {
    gpio_clear_bits(1ul << pin);
}

static inline uint32_t gpio_get_bits(uint32_t bits) {
    return *rp1_reg(RP1_SYS_RIO0 + RIO_IN) & bits;
}
static inline int gpio_get_pin(int pin) {
    return gpio_get_bits(1ul << pin) != 0;
}

void gpio_init_pull(int pin, int pud);
void gpio_init_in(int pin);
void gpio_init_out(int pin);

bool gpio_init(void);

#endif
