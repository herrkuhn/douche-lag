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
 * MAXSCREENDELAYUS: ISR validity upper bound (enforced) and the maximum
 *   measurable lag. Raise toward SCREENUS if measuring very slow games.
 * MINLAG_US: ISR validity lower bound; rejects PAD1-drive crosstalk
 *   (physically impossible sub-1 ms display response). Lower if a console
 *   responds faster than 1 ms (none currently known).
 * SETTLE_US: post-PAD1-drive settle before arming the sensor IRQ.
 *   Invariant: SETTLE_US < MINLAG_US so the settle delay can never blank
 *   a real edge; lag is referenced to button_press_time captured before the
 *   drive, so the delay adds no measurement bias.
 *
 * Tuning note: MINLAG_US=1000 and SETTLE_US=500
 *   satisfy the invariant SETTLE_US < MINLAG_US strictly. If MINLAG_US is
 *   lowered, SETTLE_US must be reduced to maintain the invariant.
 *   MAXSCREENDELAYUS=250 ms is retained as the max measurable lag ceiling.
 *   BASELINE_US (50 ms) detects lit by level over the black-half tail
 *   (elapsed > SCREENUS - BASELINE_US): any LIGHTSENSE low in that window
 *   indicates a lit source. Must remain
 *   <= SCREENUS - MAXSCREENDELAYUS; raise for refresh rates below 50 Hz.
 */
#define SCREENUS         500000
/* BASELINE_US: observation window (in microseconds) at the tail of the black
 * half used to verify the sensor is dark before arming the white-press ISR.
 * Must be >= one full refresh period (PAL 50 Hz = 20 ms) so at least one CRT
 * pulse, if present, falls inside the window. Must be <= SCREENUS -
 * MAXSCREENDELAYUS (250 ms) so the window ends before the white press is due.
 * Chosen 50 ms. */
#define BASELINE_US       50000
#define TOGGLEUS          60000
#define MAXSCREENDELAYUS  ((SCREENUS) / 2)  /* parenthesised for operator-precedence safety */

#define MINLAG_US         1000
#define SETTLE_US          500

/* Rolling window size for min/avg/max statistics */
#define BATCHSIZE 20

#endif /* _CONFIG_H */
