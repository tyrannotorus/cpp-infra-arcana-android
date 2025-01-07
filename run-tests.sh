#!/usr/bin/env sh

set -xue

root_dir=${PWD}

./build-tests.sh

cd build

# ctest --verbose
./ia-test --use-colour=no -D 3 --abort $*

cd ${root_dir}
