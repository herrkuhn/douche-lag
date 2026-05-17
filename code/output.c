/* output.c - OLED display and serial output. Runs on core 1; owns I2C0 and printf. */
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

static ssd1306_t disp;
static unsigned lagVals[BATCHSIZE];
static unsigned curMeasurement = 0;
static unsigned totalMeasurements = 0;

/* I2C init lives here so core 1 owns the bus from first use. */
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

/* Stats kept here (not measure.c) so only a 32-bit lag_us crosses the FIFO. */
void output_record_measurement(uint32_t lag_us) {
    DBG_DEBUG(MSG_MEASURE_LAG_REPORTED, lag_us);
    lagVals[curMeasurement] = lag_us;
    ++curMeasurement;
    ++totalMeasurements;

    if (curMeasurement >= BATCHSIZE) {
        curMeasurement = 0;
    }

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

/* Serial only — refreshing OLED here would disrupt the lag display. */
void output_debug_message(uint32_t word) {
    dbg_emit(DBG_LEVEL(word), DBG_MSG_ID(word), DBG_PARAM(word));
}
