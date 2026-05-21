#ifndef EDGE_REST_GATEWAY_H_
#define EDGE_REST_GATEWAY_H_

#include "app_types.h"

int rest_gateway_start(void);
int rest_gateway_poll_command(struct edge_control_cmd *cmd_out);

#endif
