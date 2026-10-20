#include "dsp.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main() {
    try {
        auto start = std::chrono::steady_clock::now();
        float in[480]{}, out[480];
        vc::Settings s;
        vc::Processor p;
        for (int f = 0; f < 100; f++) {
            p.process(in, out, s);
            for (float x : out)
                check(x == 0, "silence");
        }
        puts("PASS silence");
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> random(-1, 1);
        double inputPower = 0, outputPower = 0;
        for (int f = 0; f < 500; f++) {
            for (float &x : in)
                x = random(rng) * .06;
            p.process(in, out, s);
            if (f > 100)
                for (int i = 0; i < 480; i++) {
                    inputPower += in[i] * in[i];
                    outputPower += out[i] * out[i];
                }
        }
        printf("Noise power ratio %.6f\n", outputPower / inputPower);
        check(outputPower / inputPower < .6, "noise suppression");
        s.input = 4;
        s.output = 4;
        s.makeup = 12;
        for (int f = 0; f < 300; f++) {
            for (float &x : in)
                x = random(rng);
            p.process(in, out, s);
            for (float x : out)
                check(std::isfinite(x) && std::abs(x) <= .708f, "limiter");
        }
        puts("PASS randomized overload, finite output, -3 dBFS sample ceiling");
        s = vc::Settings{};
        s.lowCpu = true;
        s.mix = 0;
        s.gate = -90;
        s.deess = 0;
        s.highpass = 20;
        auto rms = [&](float amplitude) {
            vc::Processor unit;
            double power = 0;
            for (int f = 0; f < 300; f++) {
                for (int i = 0; i < 480; i++)
                    in[i] = amplitude * std::sin((f * 480 + i) * 2 * M_PI * 220 / 48000);
                unit.process(in, out, s);
                if (f > 200)
                    for (float x : out)
                        power += x * x;
            }
            return std::sqrt(power / (99 * 480));
        };
        double a = rms(.03f), b = rms(.5f);
        printf("Input contrast 24.44 dB; output %.2f dB\n", 20 * std::log10(b / a));
        check(b / a < 10, "compression");
        puts("PASS compressor reduces dynamics");
        for (int preset = 0; preset < 5; preset++) {
            s = vc::preset(preset);
            for (int f = 0; f < 40; f++) {
                for (float &x : in)
                    x = random(rng) * .1;
                p.process(in, out, s);
                for (float x : out)
                    check(std::isfinite(x), "preset transition");
            }
        }
        puts("PASS all presets and smoothed transitions");
        vc::Processor delay;
        s = vc::Settings{};
        s.bypass = true;
        for (int warm = 0; warm < 100; warm++) {
            for (float &x : in)
                x = 0;
            delay.process(in, out, s);
        }
        for (int f = 0; f < 4; f++) {
            for (int i = 0; i < 480; i++)
                in[i] = (f == 0 && i == 0) ? .1f : 0;
            delay.process(in, out, s);
            if (f == 0)
                for (float x : out)
                    check(std::abs(x) < 1e-7, "initial delay");
            if (f == 1)
                check(std::abs(out[240]) > .01, "720 sample delay");
        }
        puts("PASS A/B aligned 720-sample delay");
        printf("DSP tests completed in %.3f seconds\n",
               std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "FAIL %s\n", e.what());
        return 1;
    }
}
