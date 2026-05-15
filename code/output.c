/*
 * output.c - SSD1306 display driver wrapper and rolling-window statistics.
 *
 * Owns I2C0 exclusively. In dual-core mode, this module runs on core 1;
 * core 0 (measure.c) passes raw lag_us values via multicore_fifo.
 * printf over USB CDC is not thread-safe on the Pico SDK, so all serial
 * output is confined here.
 */
#include "output.h"
#include "debug.h"
#include "ssd1306.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

static void dbg_emit(uint32_t level, uint32_t id, uint32_t param) {
    static const char *level_names[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    const char *msg = (id < MSG_COUNT) ? log_msg_table[id] : "unknown message";
    printf("[%s] %s (param=%lu)\n", level_names[level], msg, (unsigned long)param);
}

/* Module-level display handle. Allocated here so I2C0 ownership is
 * explicit: no other module touches i2c0 or the SSD1306 instance.
 */
static ssd1306_t disp;

/* Circular buffer for the rolling window. curMeasurement is the next
 * write index (wraps at BATCHSIZE). totalMeasurements counts all samples
 * received; capped implicitly by the BATCHSIZE loop guard in stats.
 */
static unsigned lagVals[BATCHSIZE];
static unsigned curMeasurement = 0;
static unsigned totalMeasurements = 0;

/*
 * Initialises I2C0 at 400 kHz, configures SDA/SCL with pull-ups, and
 * shows a "Waiting..." splash on the 128x64 SSD1306 at address 0x3C.
 *
 * I2C init lives here, separate from GPIO init/measure_init_gpio, so that core 1
 * owns the bus from first use.
 *
 * The "Waiting..." splash is the only startup feedback. The original serial
 * printf("Starting\n") is absent; the OLED message serves the same role
 * without requiring a USB CDC connection.
 */
void output_init(void) {
    i2c_init(i2c0, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_CLK, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_CLK);

    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "Waiting...");
    ssd1306_show(&disp);
}

/*
 * Records lag_us in the circular buffer, computes min/avg/max over up to
 * BATCHSIZE samples, and redraws the OLED with three lines of text.
 * Also prints "Got change!" to USB CDC for each measurement.
 *
 * Stats are computed inline on every call because the measurement rate is
 * ~2 Hz; the O(BATCHSIZE) loop is negligible. Keeping stats here (not in
 * measure.c) avoids state in the timing-critical loop and is compatible with
 * the FIFO path where only 32-bit lag_us crosses core boundary.
 *
 * totalMeasurements is an ever-increasing counter; curMeasurement wraps at
 * BATCHSIZE. The stats loop breaks at totalMeasurements to avoid including
 * uninitialised slots before the buffer fills for the first time.
 *
 * In dual-core mode (Phase 2) this function is called from core1_entry, not
 * directly as measure_run's callback.
 */
void output_record_measurement(uint32_t lag_us) {
    DBG_DEBUG(MSG_MEASURE_LAG_REPORTED, lag_us);
    lagVals[curMeasurement] = lag_us;
    ++curMeasurement;
    ++totalMeasurements;

    if (curMeasurement >= BATCHSIZE) {
        curMeasurement = 0;
    }

    /* Loop guard breaks at totalMeasurements to skip uninitialised slots before buffer fills */
    unsigned minLag = (unsigned)-1;
    unsigned maxLag = 0;
    unsigned avgLag = 0;
    unsigned cnt = 0;

    for (int i = 0; i < BATCHSIZE; ++i) {
        if (i >= totalMeasurements) {
            break;
        }

        unsigned curVal = lagVals[i];
        avgLag += curVal;

        if (curVal > maxLag) {
            maxLag = curVal;
        }

        if (curVal < minLag) {
            minLag = curVal;
        }

        ++cnt;
    }

    avgLag = avgLag / cnt;

    float avgLag_f = (float)avgLag / 1000.0f;
    float minLag_f = (float)minLag / 1000.0f;
    float maxLag_f = (float)maxLag / 1000.0f;

    char textout[100];
    ssd1306_clear(&disp);
    sprintf(textout, "Avg Lag: %f ms", avgLag_f);
    ssd1306_draw_string(&disp, 0, 0, 1, textout);
    sprintf(textout, "Max Lag: %f ms", maxLag_f);
    ssd1306_draw_string(&disp, 0, 16, 1, textout);
    sprintf(textout, "Min Lag: %f ms", minLag_f);
    ssd1306_draw_string(&disp, 0, 32, 1, textout);
    ssd1306_show(&disp);
}

/* Decodes a debug word and prints it to serial.
 * OLED is not updated: adding a refresh here disrupts the running lag display.
 * Call only from core 1 — printf is not safe across cores without locking. */
void output_debug_message(uint32_t word) {
    dbg_emit(DBG_LEVEL(word), DBG_MSG_ID(word), DBG_PARAM(word));
}
