#!/bin/bash
cd vendor/

rm -rf xtensa-lx106-elf/
echo "Removing old toolchain"

# OSTYPE source: https://stackoverflow.com/a/33828925
# TOOLCHAIN_URL source: https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html#setup-toolchain
case "$OSTYPE" in
    linux*)
        echo "Installing Linux toolchain for '$OSTYPE'"
        TOOLCHAIN_URL="https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz"
        curl -s $TOOLCHAIN_URL | tar xz
        ;;
    msys*|win*|cygwin*)
        # DEPENDS: Requires unzip to be installed
        echo "Installing Windows toolchain for '$OSTYPE'"
        TOOLCHAIN_URL="https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-win32.zip"
        curl -s $TOOLCHAIN_URL -o temp.zip 
        unzip temp.zip
        rm temp.zip
        ;;
    darwin*)
        echo "Installing MacOS toolchain for '$OSTYPE'"
        TOOLCHAIN_URL="https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-macos.tar.gz"
        curl -s $TOOLCHAIN_URL | tar xz
        ;;
    *) 
        echo "ERROR: Unknown OSTYPE: $OSTYPE"
        exit 1
        ;;
esac
