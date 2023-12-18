#!/bin/sh
BUILD_DIR=./build
APP_NAME=websocket-demo
SPIFFS_PARTITION="./spiffs_filesystem_partition.bin"

# NOTE: Refer to the datasheet for your specific version of the ESP8266 for the correct flash size
# flash size for the ESP-01
FLASH_SIZE="1MB" 

# Refer to ./build/flash_project_args for the autogenerated version
python $IDF_PATH/components/esptool_py/esptool/esptool.py\
 --chip esp8266\
 -p $ESPPORT -b 460800\
 write_flash --flash_mode dout --flash_freq 40m --flash_size $FLASH_SIZE\
 0x0 "$BUILD_DIR/bootloader/bootloader.bin"\
 0x8000 "$BUILD_DIR/partition_table/partition-table.bin"\
 0x10000 "$BUILD_DIR/$APP_NAME.bin"
