#ifndef JUSTRT_SIMPLE_H
#define JUSTRT_SIMPLE_H

#include <stdint.h>

typedef struct
{
	volatile uint32_t state;
	volatile uint32_t runs;
	volatile uint32_t pass;
	volatile uint32_t fail;
	volatile uint32_t done;
} simple_result_t;

enum
{
	SIMPLE_STATE_IDLE = 0U,
	SIMPLE_STATE_RUNNING = 1U,
	SIMPLE_STATE_COMPLETE = 2U
};

void simple_example_start(void);

extern simple_result_t g_simple_result;
extern volatile uint32_t g_simple_worker_runs;
extern volatile uint32_t g_simple_observer_runs;

#endif
