#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build.sh
stage=build/deb
rm -rf "$stage"
mkdir -p "$stage/DEBIAN" "$stage/usr/lib/creative-mic" "$stage/usr/bin" \
 "$stage/usr/share/applications" "$stage/usr/share/icons/hicolor/scalable/apps" \
 "$stage/usr/share/doc/creative-mic"
install -m755 build/creative-mic-filter "$stage/usr/lib/creative-mic/"
install -m644 src/creative_mic.py "$stage/usr/lib/creative-mic/"
printf '#!/bin/sh\nexec /usr/bin/python3 /usr/lib/creative-mic/creative_mic.py "$@"\n' > "$stage/usr/bin/creative-mic"
chmod 755 "$stage/usr/bin/creative-mic"
install -m644 assets/creative-mic.desktop "$stage/usr/share/applications/"
install -m644 assets/creative-mic.svg "$stage/usr/share/icons/hicolor/scalable/apps/"
install -m644 LICENSE "$stage/usr/share/doc/creative-mic/copyright"
install -m644 vendor/rnnoise/COPYING "$stage/usr/share/doc/creative-mic/RNNoise-COPYING"
install -m644 README.md VERIFICATION.md "$stage/usr/share/doc/creative-mic/"
cat > "$stage/DEBIAN/control" <<'CONTROL'
Package: creative-mic
Version: 1.0.2
Section: sound
Priority: optional
Architecture: amd64
Maintainer: Creative Mic contributors <creative-mic@users.noreply.github.com>
Depends: python3 (>= 3.8), python3-tk, pulseaudio-utils (>= 1:13.0), libc6 (>= 2.31)
Recommends: pulseaudio | pipewire-pulse
Description: Offline RNNoise microphone noise suppression
 Routes a physical microphone through bundled RNNoise into a virtual microphone.
 Uses an existing PulseAudio server or PipeWire through pipewire-pulse.
CONTROL
mkdir -p dist
# Gzip control and data archives are supported by dpkg on Ubuntu 20.04.
dpkg-deb --root-owner-group -Zgzip --build "$stage" dist/creative-mic_1.0.2_amd64.deb
