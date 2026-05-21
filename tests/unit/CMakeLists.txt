cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(edge_unit_tests)

target_sources(app PRIVATE
  src/test_pipeline_math.c
  ../../src/pipeline/pipeline_math.c
)

target_include_directories(app PRIVATE ../../include)
