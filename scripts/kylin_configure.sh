#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build-kylin" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
