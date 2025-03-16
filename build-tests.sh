#!/usr/bin/env sh

set -xue

cmake -B build

cmake --build build --target ia-test -- -j$(nproc)
