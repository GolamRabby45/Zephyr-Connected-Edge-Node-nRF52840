#!/usr/bin/env bash
set -euo pipefail
west build -b nrf52840dk/nrf52840 -p auto .
