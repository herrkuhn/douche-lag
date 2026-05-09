/*
 * config.h - Shared compile-time constants for GPIO assignments and timing.
 *
 * All three modules (measure, output, main) include this header so constants
 * are defined in exactly one place.
 */
#ifndef _CONFIG_H
#define _CONFIG_H

#define I2C_SDA 4
#define I2C_CLK 5

/* Sensor and controller pad GPIO assignments */
#define LIGHTSENSE 2
#define PAD1       10
#define PAD2       11
#define PAD3       12
#define BTN1       21
#define BTN2       20

/* SCREENUS: interval between button presses (~2 Hz measurement rate).
 * TOGGLEUS: duration PAD1 is driven OUT to simulate a press.
 * MAXSCREENDELAYUS: half-screen-interval guard against PWM false edges.
 */
#define SCREENUS         500000
#define TOGGLEUS          60000
#define MAXSCREENDELAYUS  ((SCREENUS) / 2)  /* parenthesised for operator-precedence safety */

/* Rolling window size for min/avg/max statistics */
#define BATCHSIZE 20

#endif /* _CONFIG_H */
