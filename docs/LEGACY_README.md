# Creative Mic 1.0.2

Offline, real-time environmental microphone noise suppression for Linux.
Independent implementation using the bundled RNNoise v0.1 source and pretrained
model. It does not use Agora code, models, services, or accounts, and makes no
claim to reproduce Agora's proprietary model or match its quality.

## Use

Install the Ubuntu 20.04 amd64 package:

```sh
sudo apt install ./creative-mic_1.0.2_amd64.deb
creative-mic
```

1. Select your physical microphone; use **Refresh devices** after plugging it in.
2. Click **Start**.
3. Select **Creative Mic** as the input in Discord, Zoom, OBS, or your browser.
4. Keep Creative Mic running. Click **Stop** when finished.

The system default microphone is never changed. Apps may need their device list
refreshed or input permissions granted. Other sandboxed apps must be allowed to
access the host PulseAudio server. If a call app cannot see the source, restart
its device selection. Avoid layering multiple noise suppressors without testing.

## Voice sound and loudness

**Podcast** is the default profile: an 80 Hz high-pass filter reduces rumble;
gentle EQ cuts 250 Hz by 2 dB and boosts 3 kHz by 1.5 dB; a soft-knee 3:1
compressor at -18 dBFS reduces loud/quiet jumps, with 2 dB fixed makeup gain;
a 5 ms lookahead limiter bounds sample peaks to -3 dBFS. These are starting
settings for speech, not a guarantee of studio or podcast recording quality.
No echo or reverb effect is added. This profile does not remove room reflections.

**Clean** retains the original noise-suppression-only behavior.

The suppression slider controls the dry/wet **noise suppression** mix, not
model aggressiveness. At 0%, the denoiser is bypassed with delay compensation;
Podcast tone and dynamics processing still applies. Stop to adjust settings.
In Podcast, gain feeds the compressor/limiter; start at 1×. Clean gain saturates
to signed 16-bit bounds. Clipping detected at the input produces a warning in
the GUI/CLI: lower your microphone hardware gain. Clipped microphone audio
cannot be repaired by the output limiter, and not all analog distortion reaches
digital full scale, so absence of a warning does not prove a clean input.

Use headphones. Close Discord's mic test and disable other software monitoring
to avoid hearing a delayed copy of your own voice. Initially turn off Discord's
noise suppression and automatic gain control to avoid stacking processors; keep
echo cancellation on if needed. Select Creative Mic as **input**, while output
remains your headphones. Never feed monitoring/playback back into the microphone.
If the mic has hardware direct monitoring, don't also monitor a delayed software
return. Keep it roughly 10–15 cm from your mouth, slightly off-axis, with hardware
gain low enough for loud speech to remain undistorted. Exact positioning depends
on the microphone's pickup side and model. Soft furnishings help a roomy sound;
a processor cannot substitute for microphone placement and room treatment.

Clean's processing delay is 480 samples (10 ms); Podcast adds 240 samples (5 ms).
Additional server and calling-app buffering adds latency. Finite streams emit
one output frame per input frame and omit the final delayed tail.

## Scope and privacy

RNNoise targets environmental noise (for example fans and keyboard noise), with
results depending on the microphone, noise, and speech. It can alter speech.
This is **not background-speaker isolation**: other voices may remain audible.
It provides **no acoustic echo cancellation** (no playback reference) and **no
explicit dereverberation**. Use headphones to reduce acoustic echo. These are
distinct capabilities even when commercial audio products group them together.

No cloud inference, accounts, telemetry, recording, or runtime model downloads.
Audio travels through local process pipes and the local audio server. No audio
files are written. Temporary child stderr files contain diagnostics, not audio.

## Compatibility

Target: Ubuntu 20.04 LTS amd64, Python 3.8, PulseAudio 13. The package depends on
Python/Tkinter, pulseaudio-utils, and glibc. It uses your existing PulseAudio
server, or PipeWire's PulseAudio compatibility service (`pipewire-pulse`). It
does not require replacing your audio server. Actual verification status and
limits are in [VERIFICATION.md](VERIFICATION.md).

Only PulseAudio text commands are used, under `LC_ALL=C`; no JSON or newer
pactl features. The pipeline is physical source → parec → native filter → pacat
→ instance-owned null sink → remapped source named **Creative Mic**. Audio is
48 kHz mono signed 16-bit little-endian, in 480-sample (10 ms) filter frames.
RNNoise's overlap-add delay is matched by delaying dry audio 480 samples.
Additional audio-server and application buffering contributes latency.

Stop, window close, SIGINT, SIGTERM, startup failures, child failures, and server
disconnections trigger cleanup. Only module IDs bearing this instance's UUID
are removed, including after server restarts that might reuse IDs. SIGKILL and
power loss cannot execute cleanup; restarting the audio server removes stale
modules. Do not select internal monitors or route call playback into the input.
Only one routing instance per Unix user is allowed.

## Headless use

```sh
creative-mic --list-sources
creative-mic --headless --source alsa_input.YOUR_MIC --profile podcast --mix 1 --gain 1
```

Use an exact source name from the list. Ctrl+C stops routing and cleans up.

## Build and test

On Ubuntu 20.04:

```sh
sudo apt update
sudo apt install build-essential python3 python3-tk pulseaudio pulseaudio-utils xvfb
./scripts/build.sh
python3 -m unittest discover -s tests -v
python3 tests/integration_audio.py
xvfb-run -a python3 src/creative_mic.py --gui-smoke-test
./scripts/package.sh
sudo apt install ./dist/creative-mic_1.0.2_amd64.deb
creative-mic --version
```

Build on Ubuntu 20.04 for its glibc baseline. Do not distribute a host build from
a newer distribution as verified 20.04 compatible. The package script uses gzip
for both Debian archives, root ownership, and explicit permissions. Native code
is linked against glibc/libm, with RNNoise source and model compiled in. No native
architecture optimizations are enabled.

Version 1.0.1 remains available unchanged. Version 1.0.2 adds voice processing.

The GitHub Actions workflow builds and installs inside Ubuntu 20.04, runs unit,
PulseAudio routing, and GUI smoke checks, and only then permits publishing. It
uses the workflow's built-in GITHUB_TOKEN. Existing release tags/assets must not
be replaced. The installer is also retained in `dist/` as a fallback.

## References and licensing

The reference [Agora product page](https://www.agora.io/en/products/ai-noise-suppression/)
and [official technical documentation](https://docs.agora.io/en/realtime-media/rtc/build/enhance-the-audio-experience/ai-noise-suppression)
were inspected on 2026-10-20. They describe a proprietary deep-learning extension
with selectable modes, and the product page also discusses echo and reverberation
processing. Those claims and modes do not describe Creative Mic.

RNNoise: https://github.com/xiph/rnnoise/tree/v0.1 — the complete release source
is bundled under `vendor/rnnoise`, including `src/rnn_data.c` pretrained weights.
Original BSD notices are preserved in source and `COPYING`. Creative Mic's own
code is MIT licensed; see [LICENSE](LICENSE).
