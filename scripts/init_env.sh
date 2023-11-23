#!/bin/bash
# Usage: bash // This spawns a new shell so we can exit back to original shell
#        source setup_esp8266_environment.sh
echo "Setting up ESP8266-RTOS-SDK"

BASE_PATH="$(realpath -s ./vendor)"

export IDF_PATH="$BASE_PATH/esp8266-rtos-sdk"
export XTENSA_LX106_PATH="$BASE_PATH/xtensa-lx106-elf/bin"
export PATH="$XTENSA_LX106_PATH:$PATH"
echo "Added path variables"

# OSTYPE source: https://stackoverflow.com/a/33828925
case "$OSTYPE" in
    linux*|darwin*|msys*|cygwin*)
        source "./venv/bin/activate" ;;
    win*)
        source "./venv/Scripts/activate" ;;
    *)
        echo "ERROR: Unknown OSTYPE: $OSTYPE"
        exit 1
        ;;
esac
echo "Activated python environment"

export IDF_TARGET="esp8266"
echo "Exporting toolchain environment variables"
