#!/bin/bash

dirs=(
    ./source/hal/
    ./source/utl/
    ./test/utl/dbg/
)

for dir in "${dirs[@]}"; do
    echo "Formatting files in $dir"
    find "$dir" -maxdepth 1 -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format --verbose -i {} +
done

