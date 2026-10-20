# OBS connection

1. Start VoiceCraft Studio on the same desktop audio server as OBS.
2. Select your physical microphone, click START, and wait for READY FOR OBS.
3. In OBS Sources, add Audio Input Capture (PulseAudio).
4. Choose **VoiceCraft Studio Microphone** and confirm.
5. Speak and make a short test recording, then play it back with headphones.

Disable or mute the original microphone source in OBS so it does not mix dry and
processed versions together. Avoid applying several noise suppressors or gain
controllers at once. Keep OBS monitoring off unless you understand the feedback
route. VoiceCraft does not change OBS settings automatically.

VoiceCraft reports OBS capture connected when an OBS PulseAudio source-output
is attached to its current virtual source. That is connectivity detection, not a
verification of your OBS scene/mute/recording settings. After an audio-server
restart, OBS may require source reselection because the virtual source has a new
instance identity. Sandboxed OBS must be allowed to access the host audio server.

The supplied automated acceptance test uses OBS Studio 25.0.3 on Ubuntu 20.04,
a real PulseAudio capture source, software-rendered black video, and an actual
MKV recording of a synthetic signal processed by VoiceCraft. It is not a test of
your USB microphone, room, speech quality, desktop permissions, or OBS filters.
See `evidence/obs-recording.json` and `evidence/obs-console.log`.
