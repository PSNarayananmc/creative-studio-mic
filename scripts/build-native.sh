#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j "${BUILD_JOBS:-2}"
(cd build-native && ctest --output-on-failure)
