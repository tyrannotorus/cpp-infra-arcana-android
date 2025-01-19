#!/usr/bin/env sh

set -xue

root_dir=${PWD}

rm -rf build
mkdir build
cd build
cmake -DIA_BUILD_STATIC_SDL=ON -DWIN32=TRUE -DMSVC=FALSE -DARCH=64bit -DCMAKE_TOOLCHAIN_FILE=../Toolchain-cross-mingw32.txt ..
VERBOSE=1 cmake --build . --target install -- -j$(nproc)

cd ${root_dir}
