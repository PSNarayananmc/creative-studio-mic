# Troubleshooting

**No devices:** Check `pactl info` and `pactl list short sources` as your normal
user. Start the normal desktop PulseAudio server or pipewire-pulse. The app does
not replace your server. Refresh after plugging in the microphone.

**Disconnected/recovering:** Wait for a retry, then use Repair Audio Connection.
If the exact source name changed, Refresh and select the new source. The app never
silently switches to a different physical input. If OBS loses its capture after
recovery, reselect the new VoiceCraft source in OBS.

**Input clipping:** Lower hardware gain. The limiter cannot repair distortion
that happened in the microphone. A zero clipping counter does not rule out
analog distortion. Keep loud speech below full scale.

**Metallic speech:** Lower suppression mix or use Clean Interview. Avoid stacking
OBS/Discord noise suppression or AGC. Low CPU mode disables RNNoise explicitly.

**Echo/delayed voice:** Use headphones, turn off OBS/software monitoring, and avoid
looping output into input. VoiceCraft does not implement echo cancellation.

**High CPU/dropouts:** Choose Gaming Streamer or Low CPU mode, close competing
applications, and inspect underruns. DSP percent is processing time per 10 ms
frame on the current CPU, not total system CPU. Buffer timing is a server estimate,
not a hardware round-trip measurement. The target J1800 needs local measurement.

**Duplicate instance:** Stop the other instance for this Unix user. Locks use the
PID and release normally; do not manually remove a live instance's lock.

**Audio modules after a crash/SIGKILL:** No process can clean up after SIGKILL or
power loss. Inspect `pactl list short modules` and remove only clearly identified
`voicecraft_` modules if needed; never unload unrelated module IDs. Restarting
the desktop audio server removes stale instance modules but interrupts other apps.

**Recording/export:** Check free disk space. A capture overrun stops recording
and reports the issue. ffmpeg/ffplay must be installed for codecs and preview.
Only export preserves a temporary take beyond normal application exit.

**AppImage:** No AppImage is offered until its bundled libraries and desktop
compatibility have actually been tested. Use the verified `.deb`.
