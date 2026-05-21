#ifndef EDGE_CONTROL_PLANE_H_
#define EDGE_CONTROL_PLANE_H_

#include <stdbool.h>
#include <stdint.h>

int control_plane_start(void);
bool control_plane_consume_irq_latch(void);
int control_plane_watchdog_register(const char *owner, uint32_t reload_ms);
void control_plane_watchdog_feed(int channel_id);

#endif
