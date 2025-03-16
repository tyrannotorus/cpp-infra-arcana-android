#!/usr/bin/env sh

set -xue

# NOTE: "$*" allows adding extra arguments.

cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 $*

cmake --build build --target ia-debug -- -j$(nproc)
