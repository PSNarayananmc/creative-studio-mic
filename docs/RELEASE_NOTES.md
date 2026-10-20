VoiceCraft Studio 0.1.0 is a native Qt 5 / C++17 microphone processor for Ubuntu
20.04 amd64. This is an initial tested release with known hardware-validation gaps.

Install with `sudo apt install ./voicecraft-studio_0.1.0_amd64.deb`, launch
`voicecraft-studio`, select your microphone and click START. In OBS, add Audio
Input Capture (PulseAudio) and select VoiceCraft Studio Microphone.

Includes bundled RNNoise, high-pass/EQ, expander, de-esser, compressor, optional
saturation, conservative level assist, lookahead limiting, presets, meters,
local consented signal analysis, connection recovery and an opt-in recording
workspace with trim/split/undo/redo and WAV/FLAC/MP3 export.

Verified on Ubuntu 20.04 with a real isolated PulseAudio 13 server: PCM delivery,
source/server recovery, cleanup, OBS 25 recording of a processed synthetic source,
GUI recording/edit/export and package installation. Build logs and measurements
are included in the source evidence directory.

Physical microphone quality, USB hardware reconnection, J1800 performance,
PipeWire desktops and hardware round-trip latency remain unverified. The limiter
has independent test-signal peak measurements, not BS.1770 certification. There
is no echo cancellation, dereverberation, speaker isolation or guaranteed LUFS
target. No AppImage is included because it has not been tested.

The existing Creative Mic sources and installers remain available as legacy
reference. Use the new `voicecraft-studio` installer for the native application.
