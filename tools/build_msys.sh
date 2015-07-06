#!/usr/bin/env sh

cd "$(dirname "$0")";
cd ..;

if [ -e build ]; then
    rm -rf build;
fi

mkdir build;
cd build;
cmake .. -G "MSYS Makefiles";
make;
cd ..;