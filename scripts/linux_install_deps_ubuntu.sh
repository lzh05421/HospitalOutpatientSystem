#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y build-essential ninja-build mysql-server

if apt-cache show cmake3 >/dev/null 2>&1; then
    sudo apt install -y cmake3
else
    sudo apt install -y cmake
fi

if apt-cache show qt6-base-dev >/dev/null 2>&1; then
    sudo apt install -y qt6-base-dev qt6-base-dev-tools libqt6sql6-mysql
else
    sudo apt install -y qtbase5-dev qtbase5-dev-tools libqt5sql5-mysql
fi
