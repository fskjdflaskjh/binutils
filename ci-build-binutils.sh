#! /bin/bash -ex

mkdir build && cd build
../configure --prefix=`pwd`/root --target=${TARGET_TRIPLET} --disable-gdb
make -j
make check
