#!/bin/sh
INPUT_DIR="./static"
OUTPUT_FILE="./spiffs_filesystem_partition.bin"

# NOTE: This must be identical to the size specified in ./partitions.csv
# 1M = 128K = 131072
PARTITION_SIZE=131072


# NOTE: For the correct arguments refer to CONFIG_SPIFFS_* variables in ./sdkconfig
# CONFIG_SPIFFS_USE_MAGIC=y
# CONFIG_SPIFFS_USE_MAGIC_LENGTH=y
CONFIG_SPIFFS_PAGE_SIZE=256
CONFIG_SPIFFS_OBJ_NAME_LEN=32
CONFIG_SPIFFS_META_LENGTH=4
# TODO: Determine what the block size argument corresponds to?

rm -f $OUTPUT_FILE
python ./scripts/spiffsgen.py\
 $PARTITION_SIZE $INPUT_DIR $OUTPUT_FILE\
 --use-magic --use-magic-len\
 --block-size 4096\
 --page-size $CONFIG_SPIFFS_PAGE_SIZE\
 --obj-name-len $CONFIG_SPIFFS_OBJ_NAME_LEN\
 --meta-len $CONFIG_SPIFFS_META_LENGTH\
 --aligned-obj-ix-tables
