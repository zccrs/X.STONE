#!/bin/sh

rm -r build
mkdir build
cd build
qmake ..
make -j8
