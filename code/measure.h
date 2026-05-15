/*
 * measure.h - Timing-critical measurement interface.
 *
 * Contains no blocking I2C or printf calls; safe to run on core 0 in
 * dual-core mode. Output is delivered via callback so the transport
 * (direct call or FIFO push) is determined by the caller.
 */
#ifndef _MEASURE_H
#define _MEASURE_H

#include "config.h"
#include "pico/stdlib.h"

/* Callback invoked once per detected black-to-white screen transition.
 * lag_us is measured from PAD1 press to edge detection. In dual-core mode,
 * a FIFO-pushing wrapper can replace this callback without modifying
 * measure.c.
 */
typedef void (*measure_callback_t)(uint32_t lag_us);

/* Initialises sensor and pad GPIOs. Call before measure_run. */
void measure_init_gpio(void);
/* Blocking measurement loop. Never returns.
 * Invokes on_measurement for each valid black-to-white transition.
 * debug_push receives encoded words for fire-and-forget FIFO dispatch; never blocks. */
void measure_run(measure_callback_t on_measurement, void (*debug_push)(uint32_t));

#endif /* _MEASURE_H */
