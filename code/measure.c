/*
 * measure.c - GPIO interrupt-driven lag measurement and button-press simulation.
 *
 * Captures time from PAD1 toggle (OUT=press) to the LIGHTSENSE falling edge via
 * a GPIO ISR. The ISR stores the timestamp delta; the main loop delivers the
 * measurement via the on_measurement callback (FIFO push is not ISR-safe).
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
static measure_callback_t on_meas_cb;

static void dbg_emit(uint32_t level, uint32_t id, uint32_t param) {
    debug_push_fn(DBG_ENCODE(level, id, param));
}

/* Falling-edge ISR: captures lag timestamp and self-disarms. */
static void lightsense_isr(uint gpio, uint32_t events) {
    (void)gpio; (void)events;
    uint64_t edgeTime = time_us_64();
    captured_lag = (uint32_t)(edgeTime - button_press_time);
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

    button_press_time = time_us_64();

    DBG_INFO(MSG_MEASURE_START, 0);

    while (1) {
        uint64_t curTime = time_us_64();
        uint64_t elapsed = curTime - button_press_time;

        if (measurement_done) {
            measurement_done = 0;
            irqArmed = 0;
            on_meas_cb(captured_ts_ms, captured_lag);  /* both values frozen in ISR */
            DBG_DEBUG(MSG_MEASURE_IRQ_FIRED, captured_lag >> 10);
        }

        if (elapsed > SCREENUS) {
            gpio_set_irq_enabled(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, false);
            button_press_time = time_us_64();
            gpio_set_dir(PAD1, GPIO_OUT);
            toggling = 1;
            curScreen = !curScreen;
            irqArmed = 0;
            if (curScreen) {
                gpio_acknowledge_irq(LIGHTSENSE, GPIO_IRQ_EDGE_FALL);
                gpio_set_irq_enabled(LIGHTSENSE, GPIO_IRQ_EDGE_FALL, true);
                irqArmed = 1;
            }
            DBG_DEBUG(MSG_MEASURE_PRESS_SCHEDULED, 0);
        } else if (elapsed > TOGGLEUS && toggling) {
            gpio_set_dir(PAD1, GPIO_IN);
            toggling = 0;
            DBG_DEBUG(MSG_MEASURE_TOGGLE_RELEASED, 0);
        }
    }
}
