# Krisp reference review and speaker-isolation integration gate

Reviewed 2026-10-20. This is a capability assessment, not a claim of a new
implementation or a reverse-engineered Krisp model. Current release: 1.0.2.

## Requested behavior

Reduce fan/keyboard/background noise and grainy artifacts, and exclude other
people's voices from the virtual microphone while preserving the user's speech.
The meaning of the user's additional term "cpy" has not been established.

## Public reference inspection

https://app.krisp.ai/ returned no extractable technical content. No account was
accessed. Official documentation distinguishes environmental Noise Cancellation,
Voice Isolation, and echo processing:

- https://help.krisp.ai/hc/en-us/articles/28299368036124-Krisp-AI-Noise-Cancellation
- https://help.krisp.ai/hc/en-us/articles/15856897420444-Voice-Isolation-with-Krisp-AI-Meeting-Assistant
- https://help.krisp.ai/hc/en-us/articles/4424862471324-Echo-Cancellation-with-Krisp

These describe product behavior, not source code, weights, or a reproducible
model specification. No Krisp source, model, branding, account functionality,
cloud processing, or quality-equivalence claim has been incorporated.

## Current application audit

- RNNoise v0.1 targets environmental noise. Its speech-activity output is not
  a speaker identity and cannot select the user's voice from overlapping people.
- Podcast EQ/compression/limiting controls tone and levels, not voice identity.
- Adding more gain, cascading denoisers, or abruptly gating samples may increase
  artifacts or cut speech. Such changes are not evidence of quality improvement.
- A gate can suppress quieter sounds between utterances; it cannot separate two
  overlapping speakers. It must not be labelled speaker isolation.
- Grainy audio has not been reproduced. Potential causes include input clipping,
  denoiser artifacts, stacked app processing, device noise, or stream faults.
  No particular cause is established by the report alone.
- Echo is still undiagnosed. No echo canceller or dereverberation is present.

## Independent candidate to evaluate, not yet integrated

https://huggingface.co/penta2himajin/tse-conv-tasnet-48k

The public model card describes native 48 kHz causal extraction with 480-sample
chunks, a 192-dimensional ECAPA reference embedding, and 89 streaming state
tensors. It labels weights CC BY 4.0. Its reported evaluation scores are the
publisher's claims, not Creative Mic measurements. The v3 revision is necessary:
its model-card history reports broken EMA weights in the initial release.
Candidate revision from published metadata: 5d8934d48e582dbd00285697bde972c4ec17ba2a.

Matching embedding export:
https://huggingface.co/penta2himajin/ecapa-tdnn-onnx

The publisher labels it Apache-2.0 and describes an 80-mel fbank input and a
192-dimensional output. Feature extraction, normalization, tensor contracts,
license notices, weight hashes, runtime requirements, and performance must all
be checked against actual downloadable assets before use. No publisher accuracy
or real-time claim is accepted as verified compatibility for this application.

OpenSpeakerBeam-SS was also reviewed:
https://github.com/helloooideeeeea/OpenSpeakerBeam-SS
Its public README reports modest performance and lists quality validation as
unfinished. That is insufficient evidence to promise podcast-quality isolation.
DeepFilterNet is an environmental denoising candidate, not a replacement for
speaker-conditioned extraction: https://github.com/Rikorose/DeepFilterNet

## Current blockers

1. The managed environment's HTTP proxy returned HTTP 403 Forbidden to a HEAD
   request for https://huggingface.co/penta2himajin/tse-conv-tasnet-48k/resolve/main/tse_prod_48k.onnx,
   even with approved shell network access. The configured policy currently
   allows GitHub-related hosts, not huggingface.co. No model bytes were obtained.
   Any model-host redirects need to be permitted through the supported network
   configuration too. No proxy bypass or relay workaround was attempted.
2. A voice reference would change the original no-recording requirement. The
   user has been asked whether temporary local enrollment is acceptable. Until
   answered, no microphone recording/enrollment or persistent voice profile is
   authorized or implemented. If approved, raw enrollment should remain in RAM
   and be discarded, and any retained embedding needs an explicit lifecycle.
3. No real two-speaker evaluation, enrolled-voice preservation, target CPU latency,
   or Ubuntu 20.04 inference-runtime verification has occurred for this candidate.

## Implementation acceptance gates

- Keep the bundled RNNoise implementation and all previous releases intact.
- No runtime model downloads, cloud inference, accounts, or telemetry.
- Pin and bundle licensed model files with attribution and integrity hashes.
- Establish correct enrollment preprocessing and model state ownership; reject
  missing, nonfinite, invalid, or mismatched voice embeddings and model tensors.
- Validate silence, frame counts, delay alignment and safe clipping; reset all
  model state on restart. Preserve the current process and module cleanup rules.
- Test both overlapping and alternating real speakers, with independent clean
  target/interferer references. Measure target retention and interference leakage,
  not simply total output-energy reduction (muting everything is not isolation).
- Test speech plus fan/keyboard noise, speech onsets/tails, quiet voices, and
  artifacts. Use clearly licensed test audio; disclose language/dataset limits.
- Measure sustained processing and worst-case chunk time on Ubuntu 20.04 CPU.
  Reject an implementation that cannot keep pace with the stream.
- Test installation, Python 3.8, PulseAudio 13, routing, signal/failure cleanup,
  and GUI startup again before a new release. Keep a usable RNNoise fallback.
- Do not describe this research note, a gate, or an untested model wrapper as
  completed Krisp-like voice isolation. No new installer is warranted yet.
