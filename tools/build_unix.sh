#!/usr/bin/env bash

cd "$(dirname "$0")";
cd ..;

if [ -e build ]; then
    rm -rf build;
fi

mkdir build;
cd build;
cmake ..;
make;
cd ..;