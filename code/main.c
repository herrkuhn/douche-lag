/*
 * main.c - Entry point. Launches dual-core operation and wires FIFO transport to measure_run.
 *
 * Measurement logic lives in measure.c; display/serial logic lives in output.c.
 * Dual-core wiring (callback, core1 entry, handshake) lives here to keep both
 * modules free of transport concerns.
 *
 * Init order:
 *   core 0: stdio_init_all -> multicore_launch_core1(core1_entry)
 *   core 1: output_init -> push sentinel 0 on core1->core0 FIFO
 *   core 0: pop sentinel -> measure_init_gpio -> measure_run(fifo_push_callback)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "config.h"
#include "measure.h"
#include "output.h"

/* Forwards lag_us from core 0 measurement loop to core 1 via hardware FIFO.
 * Uses push_timeout_us (100 ms) rather than push_blocking so a stalled core 1
 * cannot corrupt timing-critical lag_us values in the polling loop. Silent drop
 * on timeout is acceptable at 2 Hz with an 8-deep hardware FIFO. */
static void fifo_push_callback(uint32_t lag_us) {
    multicore_fifo_push_timeout_us(lag_us, 100000);
}

/* Core 1 entry point: initializes I2C/OLED, signals readiness, then consumes
 * measurements from core 0 indefinitely.
 *
 * Handshake: pushes sentinel 0 on the core1->core0 FIFO after output_init so
 * core 0 cannot enter measure_run before the display is ready. The core1->core0
 * direction is separate from the core0->core1 measurement FIFO, eliminating
 * sentinel value confusion.
 *
 * Loop: multicore_fifo_pop_blocking puts this core into WFE between measurements,
 * consuming zero cycles at the ~2 Hz rate.
 *
 * Known failure mode: if output_init hangs (I2C bus error, SSD1306 absent), the
 * sentinel is never pushed and core 0 blocks on pop_blocking indefinitely.
 * Watchdog recovery is not implemented. */
static void core1_entry(void) {
    output_init();
    multicore_fifo_push_blocking(0);
    while (1) {
        uint32_t lag_us = multicore_fifo_pop_blocking();
        output_record_measurement(lag_us);
    }
}

int main(void) {
    /* stdio_init_all on core 0 only; printf is confined to core 1 via output.c.
     * The SDK USB CDC polling is interrupt-driven and printf serializes through a
     * single buffer, so exclusive-core access avoids the lock that a cross-core
     * shared call would require. */
    stdio_init_all();
    multicore_launch_core1(core1_entry);
    multicore_fifo_pop_blocking();   /* block until core 1 signals output_init done */
    measure_init_gpio();
    measure_run(fifo_push_callback);
    return 0;
}
