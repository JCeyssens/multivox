#ifndef _GADGET_H_
#define _GADGET_H_

/*
 * Raspberry Pi 5 + Adafruit Triple LED Matrix Bonnet (product 6358),
 * driving port 2 and port 3 - a "dual" configuration. Port 1 is not
 * driven; its G1 line (GPIO 27) is wired to the OPB816Z phototransistor.
 * Open-collector, so the pad pull-up holds the line high while a flag
 * blocks the beam. One rising edge per revolution.
 */
#define SPIN_SYNC 27
#define SPIN_SYNC_PULLUP
#define SPIN_SYNC_INDEX

/* Panel 0 - bonnet port 2 */
#define RGB_0_R1 12
#define RGB_0_G1 5
#define RGB_0_B1 6
#define RGB_0_R2 19
#define RGB_0_G2 13
#define RGB_0_B2 20

/* Panel 1 - bonnet port 3 (note: R1/G1/B1 share the header's
   TxD/SCL1/SDA1 lines, so the serial console and I2C1 must be off) */
#define RGB_1_R1 14
#define RGB_1_G1 2
#define RGB_1_B1 3
#define RGB_1_R2 26
#define RGB_1_G2 16
#define RGB_1_B2 21

/* Direct HUB75E row address. The bonnet has no address shift registers,
   so there is no address enable (ADDR__EN_MASK is 0 and vortex.c takes
   the #ifndef ADDR_CLK direct-addressing path). Set the bonnet's slide
   switch to the E-pad position that matches your 64-row panels. */
#define ROW_A 22
#define ROW_B 23
#define ROW_C 24
#define ROW_D 25
#define ROW_E 15
#define ADDR__EN_MASK (0)

#define RGB_BLANK 18     /* Matrix OE - active low, held high to blank the panels */
#define RGB_CLOCK 17     /* Matrix CLK */
#define RGB_STROBE 4     /* Matrix LAT */
#define RGB_BLANK_MASK (1<<RGB_BLANK)
#define RGB_CLOCK_MASK (1<<RGB_CLOCK)
#define RGB_STROBE_MASK (1<<RGB_STROBE)

#define RGB_0_MASK ((1<<RGB_0_R1)|(1<<RGB_0_G1)|(1<<RGB_0_B1)|(1<<RGB_0_R2)|(1<<RGB_0_G2)|(1<<RGB_0_B2))
#define RGB_1_MASK ((1<<RGB_1_R1)|(1<<RGB_1_G1)|(1<<RGB_1_B1)|(1<<RGB_1_R2)|(1<<RGB_1_G2)|(1<<RGB_1_B2))
#define RGB_BITS_MASK (RGB_0_MASK | RGB_1_MASK)
#define ROW_MASK ((1<<ROW_A)|(1<<ROW_B)|(1<<ROW_C)|(1<<ROW_D)|(1<<ROW_E))

static const int matrix_init_out[] = {RGB_0_R1, RGB_0_G1, RGB_0_B1, RGB_0_R2, RGB_0_G2, RGB_0_B2, RGB_1_R1, RGB_1_G1, RGB_1_B1, RGB_1_R2, RGB_1_G2, RGB_1_B2, ROW_A, ROW_B, ROW_C, ROW_D, ROW_E, RGB_BLANK, RGB_CLOCK, RGB_STROBE};

#define PANEL_WIDTH  128
#define PANEL_HEIGHT 64
#define PANEL_COUNT 2
#define PANEL_MULTIPLEX 2
#define PANEL_FIELD_HEIGHT (PANEL_HEIGHT / PANEL_MULTIPLEX)

#define PANEL_0_ORDER(c) (c)
#define PANEL_1_ORDER(c) (c)

#define PANEL_0_ECCENTRICITY 13.5
#define PANEL_1_ECCENTRICITY 0.375

#define VOXELS_X 128
#define VOXELS_Y 128
#define VOXELS_Z 64

#define VOXEL_Z_STRIDE 1
#define VOXEL_X_STRIDE VOXELS_Z
#define VOXEL_Y_STRIDE (VOXEL_X_STRIDE * VOXELS_X)
#define VOXELS_COUNT (VOXELS_X*VOXELS_Y*VOXELS_Z)

#define ROTATION_ZERO 286

/* Calibrated on the Pi 4 (A72). On the Pi 5 each wait iteration is a
   store to the RP1 across the PCIe link, so the effective panel clock
   differs - tune this if the refresh rate or brightness looks off. */
#define CLOCK_WAITS 5

#endif
