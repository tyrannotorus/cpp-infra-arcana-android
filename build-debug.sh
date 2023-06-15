#!/usr/bin/env sh

set -xue

root_dir=${PWD}

mkdir -p build && cd build

# NOTE: All script arguments are appended to the cmake call below. The intention
# is to support something like "./build-debug.sh -DDEBUG_SANITIZE".
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 $* ..

cmake --build . --target ia-debug -- -j$(nproc)

cd ${root_dir}
