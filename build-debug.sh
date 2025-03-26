#!/usr/bin/env sh

set -xue

# NOTE: "$*" allows adding extra arguments.

cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DIA_DEBUG_SANITIZE=0 $*

cmake --build build --target ia-debug -- -j$(nproc)
