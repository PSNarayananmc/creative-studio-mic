#include "dsp.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
int main(int argc, char **argv) {
    vc::Processor p;
    vc::Settings s;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--dry"))
            s.bypass = true;
        else if (!std::strcmp(argv[i], "--low-cpu"))
            s.lowCpu = true;
        else
            return 2;
    }
    int16_t in[480], out[480];
    float a[480], b[480];
    for (;;) {
        size_t n = fread(in, 1, sizeof(in), stdin);
        if (!n)
            return ferror(stdin) ? 3 : 0;
        if (n != sizeof(in))
            return 2;
        for (int i = 0; i < 480; i++)
            a[i] = in[i] / 32768.f;
        p.process(a, b, s);
        for (int i = 0; i < 480; i++)
            out[i] = std::lrint(b[i] * 32767);
        if (fwrite(out, 1, sizeof(out), stdout) != sizeof(out))
            return 3;
    }
}
