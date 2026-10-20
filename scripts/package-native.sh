#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(dpkg --print-architecture)" = amd64 ] || { echo 'amd64 build required' >&2; exit 1; }
cmake --build build-native -j "${BUILD_JOBS:-2}"
stage="$PWD/build-native/package"
rm -rf "$stage"
DESTDIR="$stage" cmake --install build-native --prefix /usr
mkdir -p "$stage/DEBIAN" dist
# dpkg-shlibdeps derives minimum versions from the actual target build.
deps=$(dpkg-shlibdeps -O -e build-native/voicecraft-studio 2>/dev/null | sed 's/^shlibs:Depends=//')
[ -n "$deps" ]
cat > "$stage/DEBIAN/control" <<CONTROL
Package: voicecraft-studio
Version: 0.1.0
Section: sound
Priority: optional
Architecture: amd64
Maintainer: PSNarayananmc <PSNarayananmc@users.noreply.github.com>
Depends: $deps, pulseaudio-utils (>= 1:13.0), ffmpeg
Recommends: pulseaudio | pipewire-pulse
Description: Native offline podcast microphone studio for OBS
 Qt 5 and C++17 audio processing with bundled RNNoise, virtual microphone
 routing, voice shaping, local recording and edited audio export.
CONTROL
find "$stage" -type d -exec chmod 755 {} +
find "$stage" -type f -exec chmod 644 {} +
chmod 755 "$stage/usr/bin/voicecraft-studio"
dpkg-deb --root-owner-group -Zgzip --build "$stage" dist/voicecraft-studio_0.1.0_amd64.deb
