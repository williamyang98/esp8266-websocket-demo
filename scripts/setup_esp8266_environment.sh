#!/bin/sh
# Usage: bash // This spawns a new shell so we can exit back to original shell
#        source setup_esp8266_environment.sh
echo "Setting up ESP8266-RTOS-SDK"

BASE_PATH="/c/tools/msys64/home/acidi/Coding/Sandbox"

export IDF_PATH="$BASE_PATH/esp8266-rtos-sdk"
export XTENSA_LX106_PATH="$BASE_PATH/xtensa-lx106-elf/bin"
export PATH="$XTENSA_LX106_PATH:$PATH"
echo "Added path variables"

source "$IDF_PATH/venv/bin/activate"
echo "Activated python environment"

export IDF_TARGET="esp8266"
echo "Setting up build variables"
