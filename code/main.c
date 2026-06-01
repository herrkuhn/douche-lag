/* main.c - Dual-core entry point and FIFO transport wiring. */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "config.h"
#include "debug.h"
#include "measure.h"
#include "output.h"

/* Pushes timestamp word then lag word; drops both on timeout rather than stalling core 0.
 * Lag push is skipped when timestamp push fails to prevent orphan lag words
 * that core 1 cannot pair. */
static void fifo_push_callback(uint32_t ts_ms, uint32_t lag_us) {
    if (!multicore_fifo_push_timeout_us(MEAS_ENCODE_TS(ts_ms), 100000)) {
        return;
    }
    multicore_fifo_push_timeout_us(MEAS_ENCODE_LAG(lag_us), 100000);
}

/* 0 us timeout: fire-and-forget, debug words are expendable. */
static void debug_fifo_push(uint32_t encoded) {
    multicore_fifo_push_timeout_us(encoded, 0);
}

/* Pushes sentinel after output_init so core 0 waits for display readiness. */
static void core1_entry(void) {
    output_init();
    multicore_fifo_push_blocking(0);
    /* One pending slot: timestamp word sets it, lag word consumes it.
     * Dropped lag -> overwritten by next timestamp; dropped timestamp -> orphan lag discarded;
     * debug word between the pair retains pending_ts. */
    uint32_t pending_ts = 0;
    unsigned have_ts = 0;
    while (1) {
        uint32_t word = multicore_fifo_pop_blocking();
        if (DBG_IS_DEBUG(word)) {
            output_debug_message(word);
        } else if (MEAS_IS_TS(word)) {
            /* Overwrite any stale pending_ts; tolerate dropped lag from prior cycle. */
            pending_ts = MEAS_PAYLOAD(word);
            have_ts = 1;
        } else {
            if (have_ts) {
                output_record_measurement(pending_ts, MEAS_PAYLOAD(word));
                have_ts = 0;
            }
            /* have_ts==0: orphan lag word; discard silently. */
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
