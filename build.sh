#!/bin/sh

pushd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
popd
