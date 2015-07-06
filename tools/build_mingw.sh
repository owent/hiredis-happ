#!/usr/bin/env bash

cd "$(dirname "$0")";
cd ..;

if [ -e build ];
    rm -rf build;
fi

mkdir build;
cd build;
cmake .. -G "MinGW Makefiles";
make;
cd ..;