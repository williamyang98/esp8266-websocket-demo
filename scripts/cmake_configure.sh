#!/bin/sh
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=MinimumSize -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE

