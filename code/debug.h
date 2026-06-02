/* debug.h - Leveled logging via tag-bit FIFO multiplexing.
 * Each TU defines a static dbg_emit(); core 0 pushes to FIFO, core 1 printfs. */
#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdint.h>

enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3
};

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

/* Bit 31 distinguishes debug words from measurements in the shared FIFO.
 * Layout: [31]=debug [30:29]=level [28:21]=msg_id [20:0]=param */
#define DBG_FLAG_BIT (1u << 31)

/* Measurement word sub-tags (bit 31 clear):
 *   bit30=1  timestamp word  [29:0]=ms since boot (~12.4-day wrap; safe for sessions)
 *   bit30=0  lag word        [29:0]=microseconds  (<= ~500ms measurement window)
 * Core 1 classifies each word independently, enabling self-healing on
 * dropped or interleaved words. */
#define MEAS_TS_BIT (1u << 30)
#define MEAS_ENCODE_TS(ms_val)  (MEAS_TS_BIT | ((uint32_t)(ms_val) & 0x3FFFFFFFu))
#define MEAS_ENCODE_LAG(us_val) ((uint32_t)(us_val) & 0x3FFFFFFFu)
#define MEAS_IS_TS(word)        (((uint32_t)(word) & (DBG_FLAG_BIT | MEAS_TS_BIT)) == MEAS_TS_BIT)
#define MEAS_PAYLOAD(word)      ((uint32_t)(word) & 0x3FFFFFFFu)

#define DBG_ENCODE(level, id, param) \
    (DBG_FLAG_BIT | (((uint32_t)(level) & 0x3u) << 29) | (((uint32_t)(id) & 0xFFu) << 21) | ((uint32_t)(param) & 0x1FFFFFu))

#define DBG_IS_DEBUG(word) (((uint32_t)(word)) & DBG_FLAG_BIT)
#define DBG_LEVEL(word)    ((((uint32_t)(word)) >> 29) & 0x3u)
#define DBG_MSG_ID(word)   ((((uint32_t)(word)) >> 21) & 0xFFu)
#define DBG_PARAM(word)    (((uint32_t)(word)) & 0x1FFFFFu)

/* Disabled levels expand to zero instructions. */
#define DBG_ERROR(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_ERROR) { dbg_emit(LOG_LEVEL_ERROR, (id), (param)); } } while (0)
#define DBG_WARN(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_WARN)  { dbg_emit(LOG_LEVEL_WARN,  (id), (param)); } } while (0)
#define DBG_INFO(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_INFO)  { dbg_emit(LOG_LEVEL_INFO,  (id), (param)); } } while (0)
#define DBG_DEBUG(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_DEBUG) { dbg_emit(LOG_LEVEL_DEBUG, (id), (param)); } } while (0)

typedef enum {
    MSG_MEASURE_START = 0,
    MSG_MEASURE_LAG_REPORTED,
    MSG_MEASURE_PRESS_SCHEDULED,
    MSG_MEASURE_TOGGLE_RELEASED,
    MSG_MEASURE_IRQ_ARMED,
    MSG_MEASURE_IRQ_FIRED,
    MSG_OUTPUT_INIT_DONE,
    /* CRT-path diagnostics.
     * SENSOR_STUCK_LOW fires at WARN when LIGHTSENSE is low at arm time
     * (ambient saturation or residual CRT glow). EDGE_REJECTED fires at WARN
     * when the ISR discards an out-of-window edge. Both are silent in the
     * default LOG_LEVEL=0 release build; build with -DLOG_LEVEL=1 to observe. */
    MSG_MEASURE_SENSOR_STUCK_LOW,
    MSG_MEASURE_EDGE_REJECTED,
    /* Emitted at WARN when the baseline observation window found LIGHTSENSE high
     * (lit) during the black half; arming is skipped for this cycle to avoid
     * measuring a CRT refresh pulse instead of the true press response. */
    MSG_MEASURE_BASELINE_NOT_DARK,
    /* Emitted at DEBUG when the derived curScreen differs from the dead-reckon prediction;
     * indicates closed-loop phase resync engaged. Observe with -DLOG_LEVEL=3. */
    MSG_MEASURE_PHASE_RESYNC,
    MSG_COUNT
} debug_msg_id;

/* static const: each TU gets its own copy; no multiple-definition error. */
static const char *log_msg_table[MSG_COUNT] = {
        [MSG_MEASURE_START]          = "measure_run started",
    [MSG_MEASURE_LAG_REPORTED]   = "lag reported",
    [MSG_MEASURE_PRESS_SCHEDULED]= "press scheduled",
    [MSG_MEASURE_TOGGLE_RELEASED]= "toggle released",
    [MSG_MEASURE_IRQ_ARMED]      = "irq armed",
    [MSG_MEASURE_IRQ_FIRED]      = "irq fired",
    [MSG_OUTPUT_INIT_DONE]       = "output_init done",
    [MSG_MEASURE_SENSOR_STUCK_LOW] = "sensor low at arm (ambient/residual)",
    [MSG_MEASURE_EDGE_REJECTED]    = "edge rejected (out of window)",
    [MSG_MEASURE_BASELINE_NOT_DARK] = "baseline lit: abstaining (CRT refresh guard)",
    [MSG_MEASURE_PHASE_RESYNC]     = "phase resync (curScreen corrected from sensor)"
};

#endif /* _DEBUG_H */
