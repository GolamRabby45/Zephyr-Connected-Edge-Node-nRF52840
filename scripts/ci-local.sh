#!/usr/bin/env bash
set -euo pipefail

bash -n scripts/*.sh

if command -v west >/dev/null 2>&1; then
  west twister -T tests/unit -p native_sim
else
  echo "west not found; skipping Twister execution"
fi
