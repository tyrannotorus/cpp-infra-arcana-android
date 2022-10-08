#!/usr/bin/env sh

set -xue

root_dir=$PWD

./build-debug.sh

cd build

./ia-debug --stress-test

cd $root_dir
