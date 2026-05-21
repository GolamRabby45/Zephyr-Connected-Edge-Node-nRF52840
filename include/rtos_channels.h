#ifndef EDGE_RTOS_CHANNELS_H_
#define EDGE_RTOS_CHANNELS_H_

#include <zephyr/kernel.h>

#include "app_types.h"

extern struct k_msgq edge_raw_sample_msgq;
extern struct k_msgq edge_processed_sample_msgq;
extern struct k_msgq edge_control_cmd_msgq;
extern struct k_msgq edge_irq_event_msgq;

extern struct k_sem edge_sample_trigger_sem;
extern struct k_sem edge_work_done_sem;

extern struct k_mutex edge_network_mutex;

#endif
