#ifndef EDGE_SENSOR_BUS_H_
#define EDGE_SENSOR_BUS_H_

#include "app_types.h"

int sensor_bus_init(void);
int sensor_bus_read(struct edge_raw_sample *sample);

#endif
