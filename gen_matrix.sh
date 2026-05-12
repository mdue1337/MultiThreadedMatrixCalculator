#!/bin/bash

# Usage: ./gen_matrix.sh <rows> <cols> <filename>
ROWS=$1
COLS=$2
FILENAME=$3

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <rows> <cols> <filename>"
    exit 1
fi

echo "Generating $ROWS x $COLS matrix into $FILENAME..."

# We use Python to handle the binary packing accurately
python3 -c "
import struct
import random

rows = $ROWS
cols = $COLS
filename = '$FILENAME'

with open(filename, 'wb') as f:
    # Pack 'i' (integer, usually 4 bytes) for rows and cols
    f.write(struct.pack('ii', rows, cols))
    
    # Generate random integers between -100 and 100
    for _ in range(rows * cols):
        val = random.randint(-100, 100)
        f.write(struct.pack('i', val))
"

echo "Done."