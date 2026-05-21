#ifndef EDGE_APP_CONFIG_H_
#define EDGE_APP_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

struct edge_runtime_config {
    char node_id[48];
    char telemetry_host[64];
    uint16_t telemetry_port;
    uint16_t rest_port;
    char ws_url[96];
    char rest_url[96];
    uint32_t sample_period_ms;
    uint32_t rest_poll_ms;
    bool websocket_enabled;
};

void app_config_init(void);
void app_config_get(struct edge_runtime_config *out);
uint32_t app_config_get_sample_period_ms(void);
void app_config_set_sample_period_ms(uint32_t period_ms);
void app_config_set_websocket_enabled(bool enabled);

#endif
