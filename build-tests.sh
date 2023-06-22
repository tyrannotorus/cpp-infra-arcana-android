#!/usr/bin/env sh

set -xue

root_dir=${PWD}

mkdir -p build && cd build

# NOTE: All script arguments are appended to the cmake call below. The intention
# is to support something like "./build-tests.sh -DDEBUG_SANITIZE".
cmake $* ..

cmake --build . --target ia-test -- -j$(nproc)

cd ${root_dir}
