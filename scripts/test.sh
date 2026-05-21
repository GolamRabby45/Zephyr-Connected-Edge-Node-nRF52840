#!/usr/bin/env bash
set -euo pipefail
west twister -T tests/unit -p native_sim
