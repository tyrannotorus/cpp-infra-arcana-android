#/usr/bin/env sh

cppcheck --enable=all --project=./build/compile_commands.json --std=c++17 --suppressions-list=.cppcheck-suppressions
