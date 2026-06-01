/* output.h - Core 1 OLED display and serial output. Owns I2C0. */
#ifndef _OUTPUT_H
#define _OUTPUT_H

#include "config.h"
#include <stdint.h>

void output_init(void);
/* ts_ms and lag_us arrive as a matched pair from the FIFO decoder. */
void output_record_measurement(uint32_t ts_ms, uint32_t lag_us);
void output_debug_message(uint32_t word);

#endif /* _OUTPUT_H */
