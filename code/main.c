/* main.c - Dual-core entry point and FIFO transport wiring. */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "config.h"
#include "debug.h"
#include "measure.h"
#include "output.h"

/* 100 ms timeout: drops rather than stalling the measurement loop. */
static void fifo_push_callback(uint32_t lag_us) {
    multicore_fifo_push_timeout_us(lag_us, 100000);
}

/* 0 us timeout: fire-and-forget, debug words are expendable. */
static void debug_fifo_push(uint32_t encoded) {
    multicore_fifo_push_timeout_us(encoded, 0);
}

/* Pushes sentinel after output_init so core 0 waits for display readiness. */
static void core1_entry(void) {
    output_init();
    multicore_fifo_push_blocking(0);
    while (1) {
        uint32_t word = multicore_fifo_pop_blocking();
        if (DBG_IS_DEBUG(word)) {
            output_debug_message(word);
        } else {
            output_record_measurement(word);
        }
    }
}

int main(void) {
    stdio_init_all();
    multicore_launch_core1(core1_entry);
    multicore_fifo_pop_blocking();
    measure_init_gpio();
    measure_run(fifo_push_callback, debug_fifo_push);
    return 0;
}
