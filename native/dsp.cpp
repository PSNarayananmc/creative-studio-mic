#include "dsp.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace vc {
static float db(float x) {
    return std::pow(10.f, x / 20.f);
}
static float level(float x) {
    return 20.f * std::log10(std::max(x, 1e-8f));
}
float Biquad::tick(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}
void Biquad::configure(float hz, float q, float gain, bool highpass) {
    float w = 2 * M_PI * std::clamp(hz, 20.f, 18000.f) / Rate, c = std::cos(w),
          alpha = std::sin(w) / (2 * q), a = std::pow(10.f, gain / 40), a0;
    if (highpass) {
        a0 = 1 + alpha;
        b0 = (1 + c) / 2;
        b1 = -(1 + c);
        b2 = b0;
        a1 = -2 * c;
        a2 = 1 - alpha;
    } else {
        a0 = 1 + alpha / a;
        b0 = 1 + alpha * a;
        b1 = -2 * c;
        b2 = 1 - alpha * a;
        a1 = -2 * c;
        a2 = 1 - alpha / a;
    }
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}
Settings preset(int i) {
    Settings s;
    switch (i) {
    case 1:
        s.presence = 2.5;
        s.brightness = 1;
        s.threshold = -20;
        break;
    case 2:
        s.mix = .6;
        s.ratio = 2.5;
        s.deess = .2;
        s.lowCpu = true;
        break;
    case 3:
        s.warmth = 2;
        s.presence = 1;
        s.saturation = .08;
        break;
    case 4:
        s.mix = .65;
        s.ratio = 2;
        s.threshold = -15;
        s.makeup = 0;
        s.deess = .2;
        break;
    default:
        break;
    }
    return s;
}
Processor::Processor() {
    rn = rnnoise_create();
    if (!rn)
        throw std::runtime_error("Cannot initialize RNNoise");
    for (int p = 0; p < 4; p++) {
        float sum = 0;
        for (int t = 0; t < 16; t++) {
            float x = t - 7 - p / 4.f;
            float v = std::abs(x) < 1e-6 ? 1 : std::sin(M_PI * x) / (M_PI * x);
            v *= .5 - .5 * std::cos(2 * M_PI * (t + .5) / 16);
            sinc[p][t] = v;
            sum += v;
        }
        for (float &v : sinc[p])
            v /= sum;
    }
    configure();
}
Processor::~Processor() {
    rnnoise_destroy(rn);
}
void Processor::configure() {
    hp.configure(current.highpass, .707, 0, true);
    warm.configure(140, .8, current.warmth);
    mud.configure(300, .8, current.mud);
    presence.configure(3000, .8, current.presence);
    bright.configure(9000, .7, current.brightness);
    sibilance.configure(5000, .707, 0, true);
}
float Processor::peak(float x) {
    for (int i = 15; i > 0; i--)
        history[i] = history[i - 1];
    history[0] = x;
    float result = std::abs(x);
    for (auto &phase : sinc) {
        float y = 0;
        for (int i = 0; i < 16; i++)
            y += phase[i] * history[i];
        result = std::max(result, std::abs(y));
    }
    return result;
}
Metrics Processor::process(const float *input, float *output, const Settings &target) {
    // Coefficients change over about 100 ms, with persistent filter state.
    auto smooth = [](float &a, float b) { a += .1f * (b - a); };
    smooth(current.input, target.input);
    smooth(current.output, target.output);
    smooth(current.mix, target.mix);
    smooth(current.highpass, target.highpass);
    smooth(current.warmth, target.warmth);
    smooth(current.mud, target.mud);
    smooth(current.presence, target.presence);
    smooth(current.brightness, target.brightness);
    smooth(current.threshold, target.threshold);
    smooth(current.ratio, target.ratio);
    smooth(current.attack, target.attack);
    smooth(current.release, target.release);
    smooth(current.knee, target.knee);
    smooth(current.makeup, target.makeup);
    smooth(current.gate, target.gate);
    smooth(current.deess, target.deess);
    smooth(current.saturation, target.saturation);
    smooth(current.ceiling, target.ceiling);
    current.lowCpu = target.lowCpu;
    current.agc = target.agc;
    configure();
    Metrics m;
    float power = 0;
    for (int i = 0; i < Frame; i++) {
        float x = std::isfinite(input[i]) ? std::clamp(input[i], -1.f, 1.f) : 0;
        rawDry[i] = rawPrevious[i];
        rawPrevious[i] = x;
        m.input = std::max(m.input, std::abs(x));
        m.clipped |= std::abs(x) > .999f;
        filtered[i] = hp.tick(x * current.input) * 32768;
        dry[i] = previous[i];
        previous[i] = filtered[i];
    }
    // Low CPU mode bypasses the neural network, retaining all tone/dynamics stages.
    neuralBlend += std::clamp((current.lowCpu ? 0.f : 1.f) - neuralBlend, -.05f, .05f);
    if (neuralBlend > 0)
        rnnoise_process_frame(rn, wet.data(), filtered.data());
    else
        wet = dry;
    const float atk = std::exp(-1 / (Rate * std::max(current.attack, 1.f) * .001f));
    const float rel = std::exp(-1 / (Rate * std::max(current.release, 10.f) * .001f));
    const float ceiling = db(std::min(current.ceiling, -1.f));
    for (int i = 0; i < Frame; i++) {
        float x = (dry[i] + current.mix * neuralBlend * (wet[i] - dry[i])) / 32768.f;
        x = bright.tick(presence.tick(mud.tick(warm.tick(x))));
        float sq = x * x;
        envelope = (sq > envelope ? .99584f : .99986f) * envelope +
                   (sq > envelope ? .00416f : .00014f) * sq;
        float rms = std::sqrt(std::max(envelope, 0.f)), l = level(rms);
        power += sq;
        float gateTarget = std::clamp((l - current.gate + 6) / 12, 0.f, 1.f);
        gateGain += (gateTarget < gateGain ? .0004f : .01f) * (gateTarget - gateGain);
        x *= gateGain;
        float ess = std::abs(sibilance.tick(x));
        x /= 1 + current.deess * std::max(0.f, ess - rms * .4f) * 12;
        float over = l - current.threshold, k = std::max(1.f, current.knee), reduction = 0;
        if (over > k / 2)
            reduction = over * (1 - 1 / current.ratio);
        else if (over > -k / 2)
            reduction = (1 - 1 / current.ratio) * (over + k / 2) * (over + k / 2) / (2 * k);
        float targetGain = db(-reduction), coeff = targetGain < comp ? atk : rel;
        comp = coeff * comp + (1 - coeff) * targetGain;
        m.reduction = std::max(m.reduction, -level(comp));
        if (current.agc) {
            float wanted = l > -45 ? std::clamp(db(-23 - l), .5f, 2.f) : 1.f;
            autoGain += .00001f * (wanted - autoGain);
        } else
            autoGain += .0001f * (1 - autoGain);
        x *= comp * db(current.makeup) * autoGain;
        float drive = 1 + current.saturation * 3;
        x = x * (1 - current.saturation) + std::tanh(x * drive) / drive * current.saturation;
        // Raw A/B retains the same 720 sample algorithmic delay and safety limiter.
        bypassBlend += .002f * ((target.bypass ? 1.f : 0.f) - bypassBlend);
        x = x * (1 - bypassBlend) + rawDry[i] * bypassBlend;
        x *= current.output;
        if (!std::isfinite(x))
            x = 0;
        float estimated = peak(x);
        m.estimatedPeak = std::max(m.estimatedPeak, estimated);
        // Monotonic queue: constant amortized work, no allocation in the audio path.
        while (tail > head && maxValues[(tail - 1) % 512] <= estimated)
            --tail;
        maxValues[tail % 512] = estimated;
        maxPositions[tail % 512] = position;
        ++tail;
        while (tail > head && maxPositions[head % 512] + 240 < position)
            ++head;
        float highest = tail > head ? maxValues[head % 512] : 0;
        float limit = highest > ceiling * .8f ? ceiling * .8f / highest : 1.f;
        limiter = std::min(limit, limiter + .0002f * (1 - limiter));
        delay[position % 241] = x;
        float y = position < 240 ? 0 : delay[(position - 240) % 241] * limiter;
        y = std::clamp(y, -ceiling, ceiling);
        ++position;
        output[i] = y;
        m.output = std::max(m.output, std::abs(y));
    }
    m.noise = level(std::sqrt(power / Frame));
    return m;
}
} // namespace vc
