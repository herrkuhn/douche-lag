/*
 * output.h - OLED display and USB CDC serial output.
 *
 * Owns I2C0 and all blocking I/O. Intended for core 1 in dual-core mode.
 * Stats (min/avg/max) are computed here, not in the measurement loop,
 * keeping core 0 free of display state.
 */
#ifndef _OUTPUT_H
#define _OUTPUT_H

#include "config.h"
#include <stdint.h>

/* Initialises I2C0, SSD1306 display, and shows "Waiting..." splash.
 * Must be called before measure_init_gpio so I2C is ready before GPIOs. */
void output_init(void);
/* Records one lag measurement, updates rolling min/avg/max window, and
 * redraws the OLED. Also prints to USB CDC. Safe to call only from the
 * output owner (core 1 in dual-core mode). */
void output_record_measurement(uint32_t lag_us);

#endif /* _OUTPUT_H */
