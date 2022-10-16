#!/usr/bin/env sh

set -xue

root_dir=${PWD}

./build-debug.sh

cd build

gdb --args ./ia-debug $*

cd ${root_dir}
