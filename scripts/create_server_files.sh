#!/bin/sh
INPUT_DIR="./static"
OUTPUT_FILE="./spiffs_filesystem_partition.bin"

# NOTE: For the correct arguments refer to CONFIG_SPIFFS_* variables in ./sdkconfig
# CONFIG_SPIFFS_USE_MAGIC=y
# CONFIG_SPIFFS_USE_MAGIC_LENGTH=y
CONFIG_SPIFFS_PAGE_SIZE=256
CONFIG_SPIFFS_OBJ_NAME_LEN=32
CONFIG_SPIFFS_META_LENGTH=4
# TODO: Determine what the block size argument corresponds to?
CONFIG_SPIFFS_BLOCK_SIZE=4096
# NOTE: This must be identical to the size specified in ./partitions.csv
# 1M = 1024K = 1048576
CONFIG_SPIFFS_PARTITION_SIZE=1048576

set -x

rm -f $OUTPUT_FILE
python ./scripts/create_server_index.py

python ./scripts/spiffsgen.py\
 --use-magic --use-magic-len\
 --block-size $CONFIG_SPIFFS_BLOCK_SIZE\
 --page-size $CONFIG_SPIFFS_PAGE_SIZE\
 --obj-name-len $CONFIG_SPIFFS_OBJ_NAME_LEN\
 --meta-len $CONFIG_SPIFFS_META_LENGTH\
 --aligned-obj-ix-tables\
 $CONFIG_SPIFFS_PARTITION_SIZE $INPUT_DIR $OUTPUT_FILE
