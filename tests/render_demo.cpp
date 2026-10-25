// Offline check + demo render of the Thump engine (no JUCE needed)
#include "../Source/DSP/KickEngine.h"
#include <cstdio>
using namespace thump;
static void wav (const char* fn, const std::vector<float>& d, int sr)
{
    FILE* f = fopen (fn, "wb"); int n = (int) d.size(); int32_t v; int16_t s;
    fwrite ("RIFF",1,4,f); v = 36 + n*2; fwrite (&v,4,1,f); fwrite ("WAVEfmt ",1,8,f); v = 16; fwrite (&v,4,1,f);
    s = 1; fwrite (&s,2,1,f); fwrite (&s,2,1,f); v = sr; fwrite (&v,4,1,f); v = sr*2; fwrite (&v,4,1,f);
    s = 2; fwrite (&s,2,1,f); s = 16; fwrite (&s,2,1,f); fwrite ("data",1,4,f); v = n*2; fwrite (&v,4,1,f);
    for (float x : d) { s = (int16_t) (std::clamp (x, -1.0f, 1.0f) * 32767); fwrite (&s,2,1,f); } fclose (f);
}
int main()
{
    const int sr = 48000; std::vector<float> song;
    KickParams variants[4];
    variants[1].lengthMs = 1100; variants[1].clickLevel = 0.2f; variants[1].phaseDeg = 0;               // long sub
    variants[2].distDrive = 0.6f; variants[2].distType = 0; variants[2].lengthMs = 450;                  // hard techno
    variants[3].distType = 2; variants[3].distDrive = 0.5f; variants[3].clickType = 1; variants[3].lowBoostDb = 4;
    bool bad = false;
    for (int v = 0; v < 4; ++v)
    {
        auto k = renderKick (variants[v], defaultPitchCurve(), defaultAmpCurve(), sr);
        float pk = 0; for (float x : k) { if (! std::isfinite (x)) bad = true; pk = std::max (pk, std::abs (x)); }
        // dominant frequency in the tail via zero crossings
        int zc = 0; size_t a = k.size() / 2, b = k.size() * 3 / 4;
        for (size_t i = a + 1; i < b; ++i) if (k[i-1] < 0 && k[i] >= 0) ++zc;
        printf ("variant %d: %zu samples, peak %.2f, tail freq ~%.1f Hz\n", v, k.size(), pk, zc / ((b - a) / (double) sr));
        for (int r = 0; r < 4; ++r) { song.insert (song.end(), k.begin(), k.end()); song.resize (song.size() + (size_t) (sr * 0.5) - k.size() % (sr / 2), 0.0f); }
    }
    wav ("thump_demo.wav", song, sr);
    printf ("nan %d\n", (int) bad);
    return bad;
}
