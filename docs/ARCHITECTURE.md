# Architecture

Qt's GUI thread owns controls, module management, recovery, disk recording,
non-destructive segments and asynchronous ffmpeg export. QProcess passes argument
lists directly; no device names are interpolated into shell commands.

A separate QThread runs libpulse's asynchronous mainloop. Record callbacks
assemble fixed 480-sample mono frames. The DSP performs high-pass, RNNoise
(normalized to its signed-16 scale), delay-aligned mix, expander, EQ, de-esser,
compressor, optional saturation, optional conservative gain assist and limiter.
RNNoise's overlap-add delay is 480 samples; the limiter adds 240. Raw A/B is
crossfaded onto a matching delay and retains the safety limiter. Parameters and
EQ coefficients move toward target settings with retained state.

Playback uses a fixed circular buffer. Capture uses a bounded SPSC ring with
atomic indexes and a separate UI/disk consumer. The audio callbacks perform no
filesystem operations, Qt UI calls or dynamic buffer growth. Settings are copied
under a mutex outside callbacks; the callback sees a stable local setting set.
The limiter uses a monotonic maximum queue, avoiding a full lookahead-window scan
per sample. A four-phase windowed-sinc interpolator estimates intersample peaks.
A safety margin is included; this is not a claim of BS.1770 certification.

A UUID tags each instance's null sink/remapped source. Cleanup rechecks module
arguments before unloading IDs, because a restarted server can reuse IDs.
An exact-source reconnect policy avoids sending a different microphone without
user choice. A per-user lock prevents duplicate routing. The app never sets a
system default source. SIGINT/SIGTERM use a signal flag polled from Qt, avoiding
unsafe work in the signal handler.

The native app depends on Qt 5, libpulse, libc/libm and the C++ runtime.
RNNoise v0.1 source and weights are embedded; no model download is performed.
Python is used only for external tests and the retained legacy implementation.
The Debian package gets its dynamic dependencies from dpkg-shlibdeps on Ubuntu
20.04, and builds portable x86_64 code without host CPU instruction tuning.

## Boundaries

There is no background-speaker isolation, acoustic echo cancellation,
dereverberation, multitrack DAW, live OBS configuration editing, or certified
integrated loudness targeting. The recorded waveform is a sampled overview.
Stereo export duplicates a mono source. Physical USB recovery and PipeWire need
hardware/desktop testing. The automatic sample analysis estimates conservative
initial settings, not intelligibility, studio quality or microphone frequency
response. No fixed sub-30 ms end-to-end latency or J1800 performance is promised.
