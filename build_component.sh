#!/bin/bash

TARGET=$1

BUILDROOT_TARGET=${TARGET}-dirclean make -j$(nproc)

# Check if the first command succeeded
if [ $? -eq 0 ]; then
    echo "First build succeeded. Starting second build..."
    make -j$(nproc)
else
    echo "First build failed. Aborting."
    exit 1
fi
