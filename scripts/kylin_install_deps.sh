#!/usr/bin/env bash
set -euo pipefail

if command -v apt >/dev/null 2>&1; then
    sudo apt update
    sudo apt install -y build-essential cmake ninja-build qtbase5-dev qtbase5-dev-tools libqt5sql5-mysql mysql-server
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y gcc-c++ cmake ninja-build qt5-qtbase-devel qt5-qtbase-mysql mysql-server
elif command -v yum >/dev/null 2>&1; then
    sudo yum install -y gcc-c++ cmake ninja-build qt5-qtbase-devel qt5-qtbase-mysql mysql-server
else
    echo "Unsupported package manager. Please install C++ compiler, CMake, Ninja, Qt Widgets, Qt SQL MySQL driver and MySQL manually." >&2
    exit 1
fi
