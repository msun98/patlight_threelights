#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BIN="${BUILD_DIR}/patlite_lr6_qt"

if [[ ! -x "${BIN}" ]]; then
    "${SCRIPT_DIR}/build.sh"
fi

exec "${BIN}" "$@"
