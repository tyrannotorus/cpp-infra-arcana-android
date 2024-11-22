#/usr/bin/env sh

cppcheck --enable=all --project=compile_commands.json --std=c++17 --suppressions-list=.cppcheck-suppressions
