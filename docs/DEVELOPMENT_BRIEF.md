# MASTER DEVELOPMENT PROMPT — VOICECRAFT STUDIO

## ROLE

Act as a senior Linux audio engineer, digital signal processing specialist, C++ developer, UI/UX designer, and professional software architect.

Build a complete, production-ready, lightweight application called **VoiceCraft Studio**.

**Mission:** Transform an ordinary microphone into a professional podcast-style voice source in real time, compatible with OBS Studio.

The user must be able to launch VoiceCraft Studio, select their microphone, click **START**, and select **VoiceCraft Studio Microphone** as an audio input inside OBS.

The software must handle audio routing, audio processing, virtual audio device creation, and device recovery automatically.

Do not create a UI mockup, fake audio processor, or unfinished prototype. Build functional software with actual audio processing.

---

## 1. TARGET SYSTEM

Primary operating system:
- Ubuntu 20.04 LTS, x86_64
- Intel J1800-class processor, approximately 2 GHz
- 4 GB RAM
- Intel integrated graphics
- USB microphones and analog 3.5 mm microphones
- PulseAudio-first support for Ubuntu 20.04
- Optional PipeWire support for compatible systems

Application requirements:
- No GPU required.
- No internet required during normal use.
- Prefer native C++17, Qt 5 and efficient audio/DSP libraries.
- Optimize for low CPU usage and minimal memory consumption.
- Target less than 100 MB RAM during typical operation, if feasible.
- Aim for under 30 ms processing/routing latency when the hardware permits; measure and report actual round-trip latency.
- Avoid Electron and heavyweight Python runtimes for the final application.
- Distribute a `.deb` package and, if compatibility tests succeed, an AppImage.

Do not force a system-wide migration from PulseAudio to PipeWire.

## 2. ONE-CLICK OPERATION

The application must have one large START button.

When clicked:

1. Detect the active audio server.
2. Detect all available microphones.
3. Use the selected microphone or a valid default.
4. Initialize audio processing.
5. Create a virtual microphone named **VoiceCraft Studio Microphone**.
6. Route processed audio into that device.
7. Make the virtual microphone selectable in OBS Studio.
8. Enable recommended podcast processing.
9. Display the live microphone waveform and levels.
10. Show the status: "READY FOR OBS".
11. Maintain processing until STOP is clicked.

When STOP is clicked, release microphone resources and any audio modules created by the application without disrupting other audio devices.

Create a **Repair Audio Connection** button for recovery.

The software must never silently change the system's default microphone.

## 3. OBS INTEGRATION — CRITICAL

The main feature is seamless OBS compatibility.

The virtual microphone must appear in OBS as:

**VoiceCraft Studio Microphone**

A practical implementation may create a named PulseAudio null sink and expose its monitor source as a virtual microphone, using appropriate routing and source identification.

Implement the actual Linux audio routing. Renaming an ordinary output stream is not sufficient.

Requirements:
- Support PulseAudio on Ubuntu 20.04.
- Support PipeWire through an appropriately tested backend.
- Detect whether the OBS audio source can access the virtual microphone.
- Prevent duplicate audio routing and feedback.
- Handle USB microphone reconnection.
- Restore audio connections after device changes.
- Avoid creating duplicate virtual devices across repeated START/STOP cycles.
- Test compatibility with OBS Studio's Linux ALSA/PulseAudio capture source.
- Provide automatic OBS source configuration only when technically possible and explicitly authorized.
- Otherwise show one clear instruction: "In OBS, add Audio Input Capture (PulseAudio) and choose VoiceCraft Studio Microphone."

**Acceptance test:** Record audio in OBS while VoiceCraft Studio processes the microphone in real time. Playback must contain the processed audio, not the original dry microphone.

## 4. PROFESSIONAL AUDIO PROCESSING

Implement this signal chain:

Microphone Input
→ High-Pass Filter
→ Noise Suppression
→ Expander/Gate
→ Corrective Equalizer
→ De-Esser
→ Compressor
→ Optional Warmth/Saturation
→ Loudness Control
→ True-Peak Limiter
→ OBS Virtual Microphone

All modules must process actual audio samples.

### Noise suppression
Integrate an efficient CPU-compatible noise suppression system, preferably RNNoise if build and performance tests are successful.

Provide adjustable suppression intensity.

Avoid excessive suppression that produces metallic, robotic, or underwater-sounding speech.

### Equalizer
Implement a parametric EQ with adjustable:
- High-pass cutoff
- Low-frequency warmth
- Mud reduction
- Presence
- Clarity
- High-frequency brightness

Do not blindly boost bass for every microphone.

### Compressor
Implement a professional vocal compressor with adjustable threshold, ratio, attack, release, knee, and makeup gain.

Start with sensible podcast-oriented settings and provide automatic gain control with safeguards.

### De-Esser
Control harsh sibilance without making speech dull.

### Limiter
Prevent digital clipping with appropriate lookahead and peak management.

### Loudness
Provide an OBS-friendly streaming preset and optional loudness targeting for recorded podcast exports.

Avoid claiming a fixed LUFS target is always suitable for every situation. Measure loudness and true peaks where possible.

## 5. AUTOMATIC VOICE OPTIMIZATION

Implement an **AUTO STUDIO VOICE** mode.

When enabled, analyze a short microphone sample with user permission.

Estimate:
- Input noise floor
- Vocal loudness
- Excessive low-frequency noise
- Presence and intelligibility
- Sibilance
- Plosives and rumble
- Clipping risk

Use these measurements to select conservative initial processing settings.

Never promise to manufacture missing microphone detail or fully eliminate poor acoustics.

Provide an A/B comparison between raw and processed audio.

Do not require a cloud AI API.

## 6. PRESET SYSTEM

Include these presets:

1. **Podcast Studio** — natural, rich, balanced, broadcast-style spoken voice.
2. **YouTube Narrator** — clear and present narration.
3. **Gaming Streamer** — intelligibility with minimal latency.
4. **Deep Warm Voice** — gentle warmth without artificial pitch changes.
5. **Clean Interview** — natural dynamics and noise control.
6. **Custom** — full manual controls.

Default: **Podcast Studio**.

Preset changes must be smooth and click-free, using parameter smoothing where appropriate.

## 7. AUDACITY-INSPIRED RECORDING FEATURES

Include a separate optional recording workspace with:
- Waveform display
- Record, pause and stop
- Timeline navigation and zoom
- Trim and split
- Undo and redo
- Mono and stereo export
- WAV and FLAC export
- MP3 export when properly supported
- Raw versus processed preview
- Basic recording and export settings

Real-time OBS microphone processing is the highest priority. Do not let advanced editing features delay or destabilize the core functionality.

## 8. PREMIUM MODERN UI

Design a clean, luxurious dark interface inspired by modern professional audio software, without copying another application's branding or proprietary assets.

Main dashboard:

**VOICECRAFT STUDIO**

Top:
- Microphone selection
- Audio system status
- OBS connection status

Center:
- Large animated waveform
- Large START / STOP button
- Input/output audio meters
- Input gain
- Output gain
- Audio latency indicator

Bottom:
- Podcast preset selection
- AUTO STUDIO VOICE toggle
- Noise removal slider
- EQ editor
- Compressor controls
- De-esser
- Limiter

Additional tabs:
- Studio
- Recording
- Mixer
- Presets
- Settings

The UI should remain responsive even when the audio processing thread is heavily loaded.

## 9. RELIABILITY AND SECURITY

- Never run the main application as root.
- Use normal user-level audio services.
- Ask permission before modifying OBS settings.
- Do not upload microphone audio to external servers.
- Do not download arbitrary executable dependencies.
- Pin dependency versions.
- Avoid shell command injection.
- Validate microphone and audio-device names.
- Ensure proper audio-thread scheduling and resource management.
- Handle microphone disconnects and audio server restarts safely.
- Recover without requiring a computer reboot.
- Save settings and presets locally.

## 10. REAL-TIME AUDIO ARCHITECTURE

Build a reliable low-latency processing engine.

Recommended architecture:

Audio Input Backend
→ Preallocated Ring Buffer (only if needed)
→ Real-Time DSP Engine
→ Virtual Microphone Backend
→ OBS

Use separate UI and audio processing responsibilities.

Avoid blocking I/O, heap allocations, filesystem access and UI operations in the real-time audio callback.

Support 48 kHz by default, with safe resampling for devices operating at other rates.

Use measured CPU utilization, buffer underruns and clipping indicators to diagnose problems.

For the target low-end computer, provide a low-CPU compatibility mode.

## 11. BUILD AND PACKAGE

Deliver the following:

- Complete source code
- CMake build configuration
- All required build dependencies
- Dependency license notices
- `.deb` installer for Ubuntu 20.04
- AppImage if successfully verified
- Application icon
- Desktop launcher
- README
- User setup guide
- OBS connection guide
- Troubleshooting guide
- Automated test suite
- Build and test logs

Do not claim builds or compatibility tests passed unless actually executed.

## 12. MANDATORY TESTS

Verify:

1. The software starts on Ubuntu 20.04.
2. An actual microphone can be selected.
3. Audio processing occurs in real time.
4. Clicking START creates or activates the virtual microphone.
5. OBS recognizes the virtual microphone.
6. OBS records the processed audio.
7. Noise suppression changes the microphone signal as expected.
8. Compression controls measurable dynamics.
9. The limiter prevents clipping under test conditions.
10. START/STOP can be repeated without duplicate audio devices.
11. Device disconnect/reconnect is handled.
12. The application runs acceptably on a low-end CPU.
13. No crashes, frozen UI, or unbounded memory growth occur during prolonged use.
14. No unintended changes are made to unrelated audio devices.

Provide actual evidence for the results, including logs and performance measurements.

## 13. DEVELOPMENT PRIORITY

Build in this exact order:

**Phase 1 — Audio foundation**
Get a real microphone working with the virtual microphone and OBS.

**Phase 2 — Podcast audio engine**
Implement actual DSP processing and presets.

**Phase 3 — Premium interface**
Build the complete, responsive GUI.

**Phase 4 — Recording features**
Add waveform editing, recording and exporting.

**Phase 5 — Distribution**
Create and verify Ubuntu-compatible installation packages.

Do not sacrifice verified OBS audio routing to build unnecessary features.

## FINAL SUCCESS CONDITION

A user on Ubuntu 20.04 must be able to:

1. Install VoiceCraft Studio.
2. Open the application.
3. Choose their microphone.
4. Click START.
5. Open OBS Studio.
6. Select **VoiceCraft Studio Microphone** as the audio input.
7. Speak naturally.
8. Hear and record a clean, professionally processed podcast-style voice.

The application must work offline, without subscription, without a cloud API, and without a dedicated GPU.

**Begin implementing the actual application and provide the complete source project and test results. Never substitute a visual demonstration for working software.**