# VoiceCraft Studio 0.1.0 — verification

Tests run on an **Ubuntu 20.04 amd64** container, with GCC 9.4, Qt 5.12.8,
Python 3.8.10, PulseAudio 13.99.1 and OBS Studio 25.0.3. Container image is pinned
in `.github/workflows/release.yml`. This file documents local results; it is not
a claim that a GitHub Actions run has completed.

## Executed checks

- Native CMake Release build and DSP CTest: pass. Assertions cover silence,
  synthetic white-noise attenuation, finite overload output, sample ceiling,
  compressor dynamics, preset transitions and aligned raw bypass delay.
- Seeded synthetic white-noise output/input power ratio: **0.205346**. This is
  roughly 6.9 dB reduction in that regression signal, not a speech-quality score.
- A 24.44 dB input-level contrast becomes **17.81 dB** after the tested compressor
  configuration. This checks the dynamics effect, not broadcast certification.
- Independent ffmpeg `ebur128=peak=true` measurements of the safety-limited PCM:
  near-Nyquist tone **−5.0 dBTP**, full-scale random **−3.9 dBTP**, alternating
  full scale **−4.1 dBTP**, transients **−4.9 dBTP**. The conservative margin is
  intentional. No universal true-peak guarantee or meter certification is claimed.
- Isolated real PulseAudio source → native engine → virtual microphone: nonzero
  processed PCM delivered, correct display name and input-source exclusion.
- Per-user duplicate-instance refusal; repeated START/STOP; SIGINT/SIGTERM owned
  cleanup; source disappearance/reappearance; audio-server restart and reused
  module IDs; unrelated modules and default microphone retained: pass.
- OBS's actual PulseAudio capture plugin attaches to VoiceCraft's UUID source
  and records an MKV containing processed synthetic audio: pass. See
  `evidence/obs-console.log` and `evidence/obs-recording.json` for exact frame/sample
  counts and decoded peak. The synthetic MKV is included in the evidence archive.
- Qt GUI test: one functional test plus initialization/cleanup, **3 passed,
  0 failed, 0 skipped**. It starts the native engine, records actual samples,
  pauses/resumes, trims, splits, undoes/redoes, exports WAV/FLAC/MP3 and verifies
  channels/sample rate with ffprobe. Actual screenshot: `evidence/studio.png`.
- The `.deb` is built using `dpkg-shlibdeps`, gzip Debian archives, explicit file
  permissions and root ownership. Package installation, CLI version and GUI
  startup under Xvfb run on Ubuntu 20.04. A separate clean runtime container
  checks installation without development libraries and exercises the installed
  binary's actual audio routing; see `evidence/clean-runtime-test.log`.

Exact native integration results, metrics, GUI memory and test durations are
stored in `evidence/`. In the initial synthetic run the headless engine used about
22 MB RSS and the Qt GUI about 79 MB RSS. DSP time was approximately 1.5–4% of a
single **cloud-host** core, depending on the signal; this does not establish
J1800 performance. The final synthetic soak is 180 seconds, including silence;
it is a short stability check rather than an hours-long reliability claim.

The algorithmic delay is 15 ms: RNNoise 480 samples plus limiter lookahead 240
samples, at 48 kHz. Actual server buffering is measured by libpulse and displayed
separately. Timing may be large during stream startup and varies with the audio
server. The setup does **not** establish sub-30 ms total latency. No hardware
round-trip latency is reported because there is no physical audio hardware.

## Not verified / not implemented

- Physical USB/analog microphone selection, hardware unplug/replug, voice quality,
  real speech intelligibility, actual J1800 CPU usage, and physical latency.
- A PipeWire desktop, sandboxed OBS permissions and long-duration production use.
- AppImage packaging or compatibility: no unverified AppImage is distributed.
- BS.1770-certified true-peak/LUFS metering or automatic export LUFS targeting.
- Echo cancellation, dereverberation and background-speaker isolation.
- Automatic repair of OBS's selected device after a server restart. VoiceCraft
  repairs its own engine; OBS may need source reselection.

The original Creative Mic files and prior verification record are retained in
`src/`, `tests/`, `dist/` and `docs/LEGACY_*`. Their past results do not establish
new features; this document and current evidence describe the native app.
