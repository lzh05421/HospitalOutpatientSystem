#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_COMMAND="cmake"

if command -v cmake3 >/dev/null 2>&1; then
    CMAKE_COMMAND="cmake3"
fi

CMAKE_VERSION="$("$CMAKE_COMMAND" --version | head -n 1 | awk '{print $3}')"
if ! "$CMAKE_COMMAND" -E compare_files /dev/null /dev/null >/dev/null 2>&1; then
    echo "Selected CMake command is not usable: $CMAKE_COMMAND" >&2
    exit 1
fi

if ! printf '3.16.0\n%s\n' "$CMAKE_VERSION" | sort -V -C; then
    echo "CMake $CMAKE_VERSION is too old. This project requires CMake 3.16 or newer." >&2
    echo "On Ubuntu 16.04, install a newer CMake, for example: sudo snap install cmake --classic" >&2
    exit 1
fi

"$CMAKE_COMMAND" -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build-linux" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
