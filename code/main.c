/*
 * main.c - Entry point. Wires output_init, measure_init_gpio, and measure_run.
 *
 * All logic lives in measure.c and output.c. Adding dual-core support
 * requires only multicore_launch_core1(core1_entry) and a FIFO-pushing
 * callback here; neither measure.c nor output.c changes.
 *
 * Init order: output_init before measure_init_gpio so I2C0 is ready before
 * GPIO configuration.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "measure.h"
#include "output.h"

int main(void) {
    /* Single stdio_init_all call. */
    stdio_init_all();
    output_init();
    measure_init_gpio();
    /* measure_run never returns; output_record_measurement is the direct-call
     * callback. In dual-core mode swap for a FIFO-pushing wrapper. */
    measure_run(output_record_measurement);
    return 0;
}
