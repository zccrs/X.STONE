#!/bin/sh

rm -r build
mkdir build
cd build
qmake6 ..
make -j8
