#!/bin/sh
BUILD_DIR=./build
BUILD_OUT=./build-artifact
APP_NAME=websocket-demo
SPIFFS_PARTITION="./spiffs_filesystem_partition.bin"

rm -rf $BUILD_OUT
mkdir $BUILD_OUT
cp --parents $BUILD_DIR/$APP_NAME.bin $BUILD_OUT
cp --parents $BUILD_DIR/$APP_NAME.elf $BUILD_OUT
cp --parents $BUILD_DIR/$APP_NAME.map $BUILD_OUT
cp --parents $BUILD_DIR/bootloader/bootloader.bin $BUILD_OUT
cp --parents $BUILD_DIR/bootloader/bootloader.elf $BUILD_OUT
cp --parents $BUILD_DIR/bootloader/bootloader.map $BUILD_OUT
cp --parents $BUILD_DIR/partition_table/partition-table.bin $BUILD_OUT
echo "Copying compiled binaries"

cp $SPIFFS_PARTITION $BUILD_OUT/$SPIFFS_PARTITION
cp ./partitions.csv $BUILD_OUT/partitions.csv
echo "Copying SPIFFS filesystem image"

cp -rf ./scripts $BUILD_OUT/scripts
echo "Copying scripts"
