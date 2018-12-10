#!/usr/local/bin/bash
if [[ $1 == *.* ]]; then
	echo 'The file must be the target executable name'
	exit 1	
fi

function ceiling() {
	c=$(echo "($1 + $2 - 1) / $2" | bc)
	echo $c
}

SUPER_BLOCK_MAGIC_NUMBER=0xfa19283e
BLOCK_SIZE=512
TOTAL_SIZE=$(echo "2^30" | bc)
TOTAL_BLOCKS=$(ceiling "$TOTAL_SIZE" "$BLOCK_SIZE")
ENTRIES_PER_FAT_BLOCK=$(echo "$BLOCK_SIZE / 4" | bc) # 4 is the number of bytes per entry (int32_t)
NUM_POINTERS_IN_FAT=$TOTAL_BLOCKS
NUM_FAT_BLOCKS=$(ceiling "$NUM_POINTERS_IN_FAT" "$ENTRIES_PER_FAT_BLOCK")

set -exu
make clean CSOURCE=$1
make CSOURCE=$1 SUPER_BLOCK_MAGIC_NUMBER=$SUPER_BLOCK_MAGIC_NUMBER BLOCK_SIZE=$BLOCK_SIZE TOTAL_SIZE=$TOTAL_SIZE TOTAL_BLOCKS=$TOTAL_BLOCKS ENTRIES_PER_FAT_BLOCK=$ENTRIES_PER_FAT_BLOCK NUM_POINTERS_IN_FAT=$NUM_POINTERS_IN_FAT NUM_FAT_BLOCKS=$NUM_FAT_BLOCKS
set +exu

