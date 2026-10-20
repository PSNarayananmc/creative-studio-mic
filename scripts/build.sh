#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
# RNNoise v0.1 needs no generated config when HAVE_CONFIG_H is unset.
${CC:-cc} -O2 -std=gnu99 -Wall -Wextra -Ivendor/rnnoise/include -Ivendor/rnnoise/src \
 src/filter.c vendor/rnnoise/src/denoise.c vendor/rnnoise/src/rnn.c \
 vendor/rnnoise/src/rnn_data.c vendor/rnnoise/src/pitch.c \
 vendor/rnnoise/src/kiss_fft.c vendor/rnnoise/src/celt_lpc.c -lm -o build/creative-mic-filter
