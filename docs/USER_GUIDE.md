# Using VoiceCraft Studio

Install the Ubuntu 20.04 amd64 `.deb` with `sudo apt install ./filename.deb`.
Run from the desktop launcher or `voicecraft-studio` as your normal user.

Select the physical microphone before pressing START. Monitors and VoiceCraft's
own devices are excluded. The server selects/resamples the hardware format to
48 kHz mono. Start with Podcast Studio, input/output gain 1× and a moderate
suppression mix. Position the microphone close to your mouth, slightly off axis,
with hardware gain low enough to avoid clipping. Use headphones.

READY FOR OBS means the engine's audio streams are connected. It does not mean OBS
has selected the source. Follow the OBS guide, then look for OBS capture connected.
Click STOP when finished. Repair Audio Connection recreates only this instance's
routing. Device/server failures retry every two seconds; select a new name after
Refresh if the device identity changes.

The Mixer exposes every implemented processing parameter. RNNoise suppression
intensity is a delayed dry/wet blend, not a neural-model aggressiveness setting.
Increasing it can damage speech. Presets are starting points, not promises of
studio sound. Low CPU mode disables neural suppression. Raw A/B retains safety
limiting and output gain, with the same 15 ms algorithmic delay.

AUTO STUDIO VOICE asks permission to analyze five seconds locally. Speak normally
and leave a quiet pause. It measures peak/RMS and simple rumble/noise estimates,
then adjusts conservative gate/high-pass/input settings. It cannot diagnose
missing microphone detail, all plosives, acoustic echo, or speech intelligibility.
Audio analyzed for this purpose is not written to disk or uploaded.

## Recording

Start Studio, then click Record in the Recording tab. Recording starts only on
this explicit action. Raw and processed float audio are written to temporary
local files, using roughly 23 MB/minute for both streams together. RAM is bounded
rather than storing the whole take. The take limit is one hour; disk capacity
still applies. Pause omits paused audio. Stop before editing/exporting.

The recorded waveform is a sampled overview of the edited processed take. Click
to move the split cursor; use the wheel to zoom and Shift+wheel to scroll. Trim
keeps the selected range on the edited timeline. Split creates two segments at
the cursor without changing the samples. Undo and Redo restore range edits.

Choose Raw or Processed, WAV/FLAC/MP3, and mono or dual mono before Export.
Preview plays the edited take on the default output through ffplay; wear
headphones. Raw and processed recordings retain the algorithmic delay difference.
MP3 is lossy; WAV and FLAC preserve the edited signal without lossy encoding.
Export does not normalize to an assumed LUFS target.

Export before exiting: temporary takes are removed on normal close. The app asks
before replacing a stopped take. Export uses a file dialog and may overwrite a
file only after the dialog's confirmation. Source controls are saved locally in
Qt's normal user configuration directory; microphone audio is not stored there.
