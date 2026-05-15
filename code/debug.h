/*
 * debug.h - Header-only leveled logging with tag-bit FIFO multiplexing for RP2040 dual-core transport.
 *
 * Exists as a separate module rather than extending config.h: debug.h includes the
 * message lookup table and macro infrastructure that would triple config.h size and
 * mix GPIO/timing constants with debug concerns.
 *
 * Macros expand to do{}while(0) for levels above LOG_LEVEL threshold.
 * Core 0 runs a timing-critical polling loop; disabled macros must generate zero
 * instructions so the debug infrastructure has no cost when LOG_LEVEL is low.
 *
 * Each TU that emits logs defines a file-scope static dbg_emit() function.
 * Core 0 encodes and pushes to FIFO; core 1 printfs directly.
 */
#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdint.h>

/* Log level constants */
enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3
};

/* Default to ERROR-only if not set by build system */
#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

/* Bit 31 is always 0 for measurements (max value is below 2^19); safe to use as debug tag. */
#define DBG_FLAG_BIT (1u << 31)

/* Pack level, msg_id, and param into one uint32_t matching the FIFO word size.
 * Layout: [31]=debug_flag [30:29]=level [28:21]=msg_id [20:0]=param */
#define DBG_ENCODE(level, id, param) \
    (DBG_FLAG_BIT | (((uint32_t)(level) & 0x3u) << 29) | (((uint32_t)(id) & 0xFFu) << 21) | ((uint32_t)(param) & 0x1FFFFFu))

/* Decode fields from an encoded debug word */
#define DBG_IS_DEBUG(word) (((uint32_t)(word)) & DBG_FLAG_BIT)
#define DBG_LEVEL(word)    ((((uint32_t)(word)) >> 29) & 0x3u)
#define DBG_MSG_ID(word)   ((((uint32_t)(word)) >> 21) & 0xFFu)
#define DBG_PARAM(word)    (((uint32_t)(word)) & 0x1FFFFFu)

/* Logging macros. Each translation unit that calls these must provide a file-scope
 * static void dbg_emit(uint32_t level, uint32_t id, uint32_t param).
 * Core 0 (measure.c) encodes and pushes to FIFO; core 1 (output.c) printfs directly.
 * Disabled levels expand to do{}while(0) for zero overhead. */
#define DBG_ERROR(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_ERROR) { dbg_emit(LOG_LEVEL_ERROR, (id), (param)); } } while (0)
#define DBG_WARN(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_WARN)  { dbg_emit(LOG_LEVEL_WARN,  (id), (param)); } } while (0)
#define DBG_INFO(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_INFO)  { dbg_emit(LOG_LEVEL_INFO,  (id), (param)); } } while (0)
#define DBG_DEBUG(id, param) \
    do { if (LOG_LEVEL >= LOG_LEVEL_DEBUG) { dbg_emit(LOG_LEVEL_DEBUG, (id), (param)); } } while (0)

/* Message IDs for all debug instrumentation points */
typedef enum {
    MSG_MEASURE_START = 0,
    MSG_MEASURE_LAG_REPORTED,
    MSG_MEASURE_PRESS_SCHEDULED,
    MSG_MEASURE_TOGGLE_RELEASED,
    MSG_OUTPUT_INIT_DONE,
    MSG_COUNT
} debug_msg_id;

/* Maps message IDs to format strings for core 1 output.
 * static const: each TU gets its own copy in rodata; no multiple-definition error. */
static const char *log_msg_table[MSG_COUNT] = {
        [MSG_MEASURE_START]          = "measure_run started",
    [MSG_MEASURE_LAG_REPORTED]   = "lag reported",
    [MSG_MEASURE_PRESS_SCHEDULED]= "press scheduled",
    [MSG_MEASURE_TOGGLE_RELEASED]= "toggle released",
    [MSG_OUTPUT_INIT_DONE]       = "output_init done"
};

#endif /* _DEBUG_H */
