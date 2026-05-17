/* measure.h - Core 0 measurement interface. No I2C or printf; output via callback. */
#ifndef _MEASURE_H
#define _MEASURE_H

#include "config.h"
#include "pico/stdlib.h"

typedef void (*measure_callback_t)(uint32_t lag_us);

void measure_init_gpio(void);
void measure_run(measure_callback_t on_measurement, void (*debug_push)(uint32_t));

#endif /* _MEASURE_H */
