/*
 * measure.c - Light-sensor polling loop and button-press simulation.
 *
 * Measures time from PAD1 toggle (OUT=press) to the first black-to-white
 * transition on LIGHTSENSE. Contains no blocking I/O; all output is via
 * the on_measurement callback.
 */
#include "measure.h"
#include "debug.h"
#include "hardware/gpio.h"
#include "pico/time.h"

static void (*debug_push_fn)(uint32_t);

static void dbg_emit(uint32_t level, uint32_t id, uint32_t param) {
    debug_push_fn(DBG_ENCODE(level, id, param));
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

/*
 * Blocking polling loop. Toggles PAD1 every SCREENUS microseconds to
 * simulate a controller button press, then measures elapsed time until
 * LIGHTSENSE transitions from black (1) to white (0).
 *
 * State machine:
 *   curScreen tracks expected screen color. registeredChange suppresses
 *   duplicate edge reports within a single toggle cycle.
 *   toggling tracks whether PAD1 is currently driven OUT.
 *
 * PWM guard: during the white-screen window, LCD PWM can generate spurious
 * black readings. The MAXSCREENDELAYUS check filters these by requiring the
 * edge to arrive after half the screen interval has elapsed.
 *
 * on_measurement is called only on black-to-white transitions while
 * registeredChange==0, so one lag value is produced per button press. No
 * I/O calls inside this function; the callback owns all output.
 */
/* debug_push is stored in a file-scope static for use by dbg_emit.
 * Caller supplies the wrapper; measure.c has no direct FIFO dependency. */
void measure_run(measure_callback_t on_measurement, void (*debug_push)(uint32_t)) {
    debug_push_fn = debug_push;
    unsigned lastSensorVal = 0;
    unsigned registeredChange = 1;
    uint64_t buttonPressTime = time_us_64();
    unsigned toggling = 0;
    unsigned curScreen = 0;

    DBG_INFO(MSG_MEASURE_START, 0);

    while (1) {
        unsigned lightVal = gpio_get(LIGHTSENSE);
        uint64_t curTime = time_us_64();

        if (lightVal != lastSensorVal) {
            if (!registeredChange) {
                if (lastSensorVal == 1 && lightVal == 0) {
                    uint32_t lag = (uint32_t)(curTime - buttonPressTime);
                    on_measurement(lag);
                }
            } else {
                /* PWM false-edge guard: only accept after half interval */
                if (curTime - buttonPressTime > MAXSCREENDELAYUS) {
                    curScreen = 1;
                }
            }

            registeredChange = 1;
        }

        if (curTime - buttonPressTime > SCREENUS) {
            /* Direction OUT drives line to GND via common-ground switch; no gpio_put needed */
            gpio_set_dir(PAD1, GPIO_OUT);
            buttonPressTime = time_us_64();
            toggling = 1;
            curScreen = !curScreen;
            DBG_DEBUG(MSG_MEASURE_PRESS_SCHEDULED, 0);

            if (curScreen) {
                /* White screen expected: arm edge detection for the black-to-white transition */
                registeredChange = 0;
            }
        } else if (curTime - buttonPressTime > TOGGLEUS && toggling) {
            /* Revert to IN releases the line; no open-drain transistor required */
            gpio_set_dir(PAD1, GPIO_IN);
            toggling = 0;
            DBG_DEBUG(MSG_MEASURE_TOGGLE_RELEASED, 0);
        }

        lastSensorVal = lightVal;
    }
}
