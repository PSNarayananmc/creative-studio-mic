#pragma once
#include <array>
#include <cstdint>
extern "C" {
#include <rnnoise.h>
}
namespace vc {
constexpr int Rate = 48000, Frame = 480;
struct Settings {
    float input = 1, output = 1, mix = 0.8f, highpass = 80, warmth = 0, mud = -2, presence = 1.5f,
          brightness = 0;
    float threshold = -18, ratio = 3, attack = 10, release = 150, knee = 6, makeup = 2;
    float gate = -55, deess = 0.35f, saturation = 0, ceiling = -3;
    bool bypass = false, lowCpu = false, agc = false;
};
Settings preset(int index);
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float tick(float x);
    void configure(float hz, float q, float db, bool hp = false);
};
struct Metrics {
    float input = 0, output = 0, reduction = 0, estimatedPeak = 0, noise = -90;
    bool clipped = false;
};
class Processor {
    DenoiseState *rn = nullptr;
    Settings current{};
    Biquad hp, warm, mud, presence, bright, sibilance;
    std::array<float, Frame> previous{}, dry{}, wet{}, filtered{}, rawPrevious{}, rawDry{};
    std::array<float, 241> delay{};
    std::array<float, 16> history{};
    std::array<std::array<float, 16>, 4> sinc{};
    std::array<float, 512> maxValues{};
    std::array<uint64_t, 512> maxPositions{};
    uint64_t position = 0, head = 0, tail = 0;
    float envelope = 0, comp = 1, gateGain = 1, limiter = 1, autoGain = 1, bypassBlend = 0, neuralBlend = 1;
    void configure();
    float peak(float x);

  public:
    Processor();
    ~Processor();
    Processor(const Processor &) = delete;
    Metrics process(const float *input, float *output, const Settings &target);
};
} // namespace vc
