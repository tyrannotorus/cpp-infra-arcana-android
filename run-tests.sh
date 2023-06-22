#!/usr/bin/env sh

set -xue

root_dir=${PWD}

./build-tests.sh

cd build

ctest --verbose

cd ${root_dir}
