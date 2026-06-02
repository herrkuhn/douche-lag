/*
 * measure.c - GPIO interrupt-driven lag measurement and button-press simulation.
 *
 * Captures time from PAD1 toggle (OUT=press) to the LIGHTSENSE falling edge via
 * a GPIO ISR. The ISR stores the timestamp delta; the main loop delivers the
 * measurement via the on_measurement callback (FIFO push is not ISR-safe).
 * measure_run gates arming on proven pre-press darkness (baseline causal gate);
 * lightsense_isr is unchanged.
 */
#include "measure.h"
#include "debug.h"
#include "hardware/gpio.h"
#include "pico/time.h"

static void (*debug_push_fn)(uint32_t);
static volatile uint64_t button_press_time;
static volatile uint32_t captured_lag;
/* Frozen in the same ISR invocation as captured_lag; pair is self-consistent. */
static volatile uint32_t captured_ts_ms;
static volatile uint32_t measurement_done;
/* Out-of-window edge count. Written only in lightsense_isr (single writer);
 * read and snapshotted in measure_run main loop. Main loop emits the FIFO
 * push -- multicore_fifo_push is not ISR-safe. */
static volatile uint32_t rejected_edges;
/* Counts accepted measurements delivered via on_meas_cb since boot.
 * Written only in the main loop; used for SWD positive-count verification. */
static uint32_t measurements_emitted;
static measure_callback_t on_meas_cb;
/* Counts cycles where the sensor-derived curScreen differs from the dead-reckon prediction (!curScreen).
 * Written only in the main loop; inspect via SWD/GDB watch. */
static uint32_t phase_corrections;

static void dbg_emit(uint32_t level, uint32_t id, uint32_t param) {
    debug_push_fn(DBG_ENCODE(level, id, param));
}

/* Falling-edge ISR: validates the edge, captures lag, self-disarms on accept.
 * The SDK acknowledges the serviced edge before calling this callback, so the
 * reject path simply returns with the IRQ still enabled -- leaving the ISR armed
 * for the next falling edge. Both window bounds are comparisons (time constant 0).
 *
 * Validity window: [MINLAG_US, MAXSCREENDELAYUS]. An edge inside the window
 * freezes captured_lag and captured_ts_ms atomically (both written before
 * measurement_done=1), self-disarms the IRQ, and signals the main loop.
 * An edge outside the window increments rejected_edges and returns; the IRQ
 * remains enabled so the real first in-window pulse is still caught. */
static void lightsense_isr(uint gpio, uint32_t events) {
    (void)gpio; (void)events;
    uint64_t edgeTime = time_us_64();
    uint32_t lag = (uint32_t)(edgeTime - button_press_time);

    if (lag < MINLAG_US || lag > MAXSCREENDELAYUS) {
        rejected_edges++;
        return;
    }

    captured_lag = lag;
    /* button_press_time is overwritten by the next scheduled press; capture here or skew. */
    captured_ts_ms = (uint32_t)(button_press_time / 1000);
    gpio_set_irq_enabled(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, false);
    measurement_done = 1;
}

/*
 * Initialises GPIOs for the sensor, buttons, and pads.
 * I2C is initialised in output_init, not here, so core 1 can own I2C0
 * exclusively in dual-core mode.
 */
void measure_init_gpio(void) {
    gpio_init(LIGHTSENSE);
    gpio_set_dir(LIGHTSENSE, GPIO_IN);
    gpio_disable_pulls(LIGHTSENSE);

    gpio_init(BTN1);
    gpio_set_dir(BTN1, GPIO_IN);
    gpio_init(BTN2);
    gpio_set_dir(BTN2, GPIO_IN);

    gpio_init(PAD1);
    gpio_set_dir(PAD1, GPIO_IN);
    gpio_init(PAD2);
    gpio_set_dir(PAD2, GPIO_IN);
    gpio_init(PAD3);
    gpio_set_dir(PAD3, GPIO_IN);

    // For GBA, set the output register value to low.
    //gpio_put( PAD1, 0 );
    gpio_pull_up( PAD1 );
}

/* Blocking measurement loop. Arms GPIO ISR each cycle, delivers results via callback. */
void measure_run(measure_callback_t on_measurement, void (*debug_push)(uint32_t)) {
    debug_push_fn = debug_push;
    on_meas_cb = on_measurement;

    gpio_set_irq_enabled_with_callback(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, true, lightsense_isr);
    gpio_set_irq_enabled(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, false);
    gpio_acknowledge_irq(LIGHTSENSE, GPIO_IRQ_EDGE_FALL);

    unsigned toggling = 0;
    unsigned curScreen = gpio_get(LIGHTSENSE) ? 0 : 1;
    unsigned irqArmed = 0;
    unsigned observed_lit = 0;
    /* OR-accumulated phase reference: any LOW in the tail sets this; reset at toggle-release,
     * read at the next press to derive curScreen = !observed_lit. */
    /* Pessimistic initial value: assume lit until the first black half proves
     * dark. This causes the first white-press cycle to abstain, ensuring the
     * very first CSV row is causally anchored. */
    unsigned baseline_lit = 1;
    /* Tracks the last rejected_edges value emitted to avoid duplicate WARNs
     * on the same reject count across loop iterations. */
    uint32_t last_reported_rejects = 0;

    button_press_time = time_us_64();

    DBG_INFO(MSG_MEASURE_START, 0);

    while (1) {
        uint64_t curTime = time_us_64();
        uint64_t elapsed = curTime - button_press_time;

        if (measurement_done) {
            measurement_done = 0;
            irqArmed = 0;
            /* Increment before callback so a GDB watchpoint fires with the
             * updated count while both cores are at a stable point. */
            measurements_emitted++;
            on_meas_cb(captured_ts_ms, captured_lag);  /* both values frozen in ISR */
            DBG_DEBUG(MSG_MEASURE_IRQ_FIRED, captured_lag >> 10);
        }

        if (rejected_edges != last_reported_rejects) {
            last_reported_rejects = rejected_edges;
            DBG_WARN(MSG_MEASURE_EDGE_REJECTED, rejected_edges);
        }

        if (elapsed > SCREENUS) {
            gpio_set_irq_enabled(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, false);
            button_press_time = time_us_64();
            /* Causal gate: arm only when darkness was proven in the preceding
             * black half (baseline_lit==0). Only a verified dark->lit
             * transition is attributable to the actual press; arming on an
             * unverified white-press would accept refresh-phase edges as lag.
             * Baseline gate (outer) runs before settle/stuck-low
             * (inner): if baseline_lit, arming is skipped entirely so SETTLE_US
             * is never paid and the stuck-low gpio_get is never reached on
             * abstained cycles. When baseline_lit==0, the stuck-low check
             * covers the distinct case where a CRT pulse arrives during
             * SETTLE_US and pulls the line low at arm time. The two checks are
             * complementary, not redundant, and the ordering is load-bearing. */
            gpio_set_dir(PAD1, GPIO_OUT);
            toggling = 1;
            /* Closed-loop derivation: curScreen reflects the actual displayed half observed
             * over the settled tail window (active-low: LOW==lit, observed_lit==1 means white).
             * Reuses BASELINE_US window; windowed level observation, never a point read. */
            unsigned predicted = !curScreen;
            curScreen = !observed_lit;
            if (curScreen != predicted) {
                phase_corrections++;
                DBG_DEBUG(MSG_MEASURE_PHASE_RESYNC, 0);
            }
            irqArmed = 0;
            if (curScreen) {
                if (baseline_lit) {
                    /* Patch was lit during the black-half tail: abstain. Do not
                     * arm and emit no CSV row; reset so the next black half
                     * re-evaluates from a clean slate. */
                    DBG_WARN(MSG_MEASURE_BASELINE_NOT_DARK, 0);
                    baseline_lit = 0;
                } else {
                    /* SETTLE_US: let PAD1-drive transient decay before sampling
                     * the line and arming the IRQ. Invariant: SETTLE_US < MINLAG_US
                     * ensures this delay cannot blank a real edge. */
                    sleep_us(SETTLE_US);

                    /* A low line at t~press means ambient saturation or residual
                     * CRT glow, not the measurand. Skip arming and log; the next
                     * cycle measures instead. */
                    if (gpio_get(LIGHTSENSE) == 0) {
                        DBG_WARN(MSG_MEASURE_SENSOR_STUCK_LOW, 0);
                    } else {
                        gpio_acknowledge_irq(LIGHTSENSE, GPIO_IRQ_EDGE_FALL);
                        gpio_set_irq_enabled(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, true);
                        irqArmed = 1;
                        /* IRQ_ARMED at DEBUG: visible only at LOG_LEVEL>=3. */
                        DBG_DEBUG(MSG_MEASURE_IRQ_ARMED, 0);
                    }
                }
            }
            DBG_DEBUG(MSG_MEASURE_PRESS_SCHEDULED, 0);
        } else if (elapsed > TOGGLEUS && toggling) {
            gpio_set_dir(PAD1, GPIO_IN);
            toggling = 0;
            /* Black half begins: open a fresh observation window for both observed_lit
             * and baseline_lit. Reset here so the tail sampler accumulates from a clean
             * slate over this period. */
            baseline_lit = 0;
            observed_lit = 0;
            DBG_DEBUG(MSG_MEASURE_TOGGLE_RELEASED, 0);
        } else if (!toggling && elapsed > (SCREENUS - BASELINE_US)) {
            /* Tail sampler: runs in both halves. observed_lit accumulates any LOW
             * seen in this period tail -- the phase reference for curScreen next press
             * (active-low: LOW == lit/white). baseline_lit is set only in the believed-black
             * half (curScreen==0) to preserve the causal gate. */
            if (gpio_get(LIGHTSENSE) == 0) {
                observed_lit = 1;
                if (!curScreen) {
                    baseline_lit = 1;
                }
            }
        }
    }
}