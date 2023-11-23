#!/bin/sh
APP_NAME=websocket-demo
python $IDF_PATH/tools/idf_monitor.py --port $ESPPORT --baud 115200 ./build/$APP_NAME.elf
