#!/bin/sh
python $IDF_PATH/tools/idf_monitor.py --port $ESPPORT --baud 115200 ./build/hello-world.elf
