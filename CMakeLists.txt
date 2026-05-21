cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_edge_telemetry_nrf52840)

target_sources(app PRIVATE
  src/main.c
  src/app/app_config.c
  src/drivers/sensor_bus.c
  src/drivers/irq_button.c
  src/pipeline/sampler.c
  src/pipeline/pipeline_math.c
  src/pipeline/processor.c
  src/connectivity/net_client.c
  src/connectivity/ws_client.c
  src/connectivity/rest_gateway.c
  src/control/control_plane.c
)

target_include_directories(app PRIVATE include)
