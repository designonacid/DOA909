#pragma once
// Thump kick engine. A kick is fully determined by its settings, so the
// engine renders the whole hit into a buffer whenever a setting changes.
// Playback is then just sample playback: zero CPU spikes on note-on, and the
// waveform shown in the editor is exactly what you hear.

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace thump
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Breakpoint curve. x and y are 0..1. Each node's bend shapes the segment
// that leaves it (0 = straight, +/- = exponential either way).
struct CurveNode { float x = 0, y = 0, bend = 0; };

struct Curve
{
    std::vector<CurveNode> nodes;

    static float shape (float t, float bend)
    {
        if (std::abs (bend) < 1.0e-3f) return t;
        const float c = bend * 8.0f;
        return (std::exp (c * t) - 1.0f) / (std::exp (c) - 1.0f);
    }

    float eval (float x) const
    {
        if (nodes.empty()) return 0.0f;
        if (x <= nodes.front().x) return nodes.front().y;
        for (size_t i = 0; i + 1 < nodes.size(); ++i)
        {
            const auto& a = nodes[i];
            const auto& b = nodes[i + 1];
            if (x <= b.x)
            {
                const float w = b.x - a.x;
                const float t = w > 1.0e-6f ? (x - a.x) / w : 1.0f;
                return a.y + (b.y - a.y) * shape (t, a.bend);
            }
        }
        return nodes.back().y;
    }

    void sortAndClamp()
    {
        std::sort (nodes.begin(), nodes.end(), [] (auto& a, auto& b) { return a.x < b.x; });
        for (auto& n : nodes)
        {
            n.x = std::clamp (n.x, 0.0f, 1.0f);
            n.y = std::clamp (n.y, 0.0f, 1.0f);
            n.bend = std::clamp (n.bend, -1.0f, 1.0f);
        }
        if (! nodes.empty()) { nodes.front().x = 0.0f; nodes.back().x = 1.0f; }
    }
};

// Pitch curve y (0..1) <-> Hz on a log scale 20 Hz .. 20 kHz
inline float pitchYToHz (float y)  { return 20.0f * std::pow (1000.0f, y); }
inline float hzToPitchY (float hz) { return std::log (std::max (20.0f, hz) / 20.0f) / std::log (1000.0f); }

//==============================================================================
struct KickParams
{
    float lengthMs   = 600.0f;
    float tuneSemis  = 0.0f;
    float shape      = 0.0f;   // 0 = pure sine .. 1 = heavily driven, square-ish
    float phaseDeg   = 90.0f;  // start phase; 90 gives a hard initial "knock"

    float clickLevel = 0.5f;
    float clickTone  = 0.6f;   // 0..1 -> 1 kHz .. 12 kHz
    float clickDecay = 0.2f;   // 0..1 -> 1 ms .. 30 ms
    int   clickType  = 0;      // 0 noise, 1 snap (fast sine chirp)

    int   distType   = 0;      // 0 soft, 1 hard, 2 fold, 3 bit crush
    float distDrive  = 0.2f;
    float distMix    = 1.0f;

    float lowBoostDb = 0.0f;   // low shelf at 60 Hz
    float highCutHz  = 20000.0f;
    float outGainDb  = 0.0f;
};

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }

    void lowShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
        const double cs = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0);
        const double sA = 2 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * cs + sA;
        set (A * ((A + 1) - (A - 1) * cs + sA), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sA),
             a0, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sA);
    }
    void lowPass (double fs, double f, double q)
    {
        const double w = 2 * kPi * std::min (f, fs * 0.45) / fs, cs = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void highPass (double fs, double f, double q)
    {
        const double w = 2 * kPi * std::min (f, fs * 0.45) / fs, cs = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
private:
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
        a1 = (float) (na1 / a0); a2 = (float) (na2 / a0);
    }
};

//==============================================================================
inline float distort (float x, int type, float drive)
{
    const float g = 1.0f + drive * drive * 24.0f;   // squared: gentle at low settings
    switch (type)
    {
        case 1: return std::clamp (x * g, -1.0f, 1.0f);                       // hard clip
        case 2: { float v = x * (1.0f + drive * 6.0f);                         // wavefolder
                  for (int i = 0; i < 6 && (v > 1.0f || v < -1.0f); ++i) v = v > 1.0f ? 2.0f - v : -2.0f - v;
                  return v; }
        case 3: { const float steps = std::pow (2.0f, 16.0f - drive * 13.0f);  // bit crush
                  return std::round (std::tanh (x * (1.0f + drive * 3.0f)) * steps) / steps; }
        default: return std::tanh (x * g) / std::tanh (g);                       // soft, unity peak
    }
}

//==============================================================================
// Render one complete kick (mono). Deterministic: same input -> same output.
inline std::vector<float> renderKick (const KickParams& p, const Curve& pitch, const Curve& amp, double fs)
{
    const int n = std::max (16, (int) (p.lengthMs * 0.001 * fs));
    std::vector<float> out ((size_t) n, 0.0f);

    const float tuneRatio = std::pow (2.0f, p.tuneSemis / 12.0f);
    const float shapeDrive = 1.0f + p.shape * 12.0f;
    const float shapeNorm  = std::tanh (shapeDrive);
    double phase = p.phaseDeg / 360.0;

    // --- body: curve-driven sine ---
    for (int i = 0; i < n; ++i)
    {
        const float x = (float) i / (float) (n - 1);
        const double hz = std::min ((double) pitchYToHz (pitch.eval (x)) * tuneRatio, fs * 0.45);
        float s = (float) std::sin (2.0 * kPi * phase);
        if (p.shape > 0.001f) s = std::tanh (s * shapeDrive) / shapeNorm;
        out[(size_t) i] = s * amp.eval (x);
        phase += hz / fs;
        phase -= std::floor (phase);
    }

    // --- click layer ---
    if (p.clickLevel > 0.001f)
    {
        const double toneHz = 1000.0 * std::pow (12.0, (double) p.clickTone);
        const double decaySec = 0.001 * std::pow (30.0, (double) p.clickDecay);
        const int clickLen = std::min (n, (int) (decaySec * 8.0 * fs));
        Biquad hp, lp; hp.highPass (fs, toneHz * 0.5, 0.7); lp.lowPass (fs, toneHz * 2.0, 0.7);
        uint32_t seed = 0x1234567u;
        double cph = 0.0;
        for (int i = 0; i < clickLen; ++i)
        {
            const double t = i / fs;
            const float env = (float) std::exp (-t / decaySec);
            float c;
            if (p.clickType == 0)
            {
                seed = seed * 1664525u + 1013904223u;
                const float noise = ((seed >> 9) * (1.0f / 4194304.0f)) * 2.0f - 1.0f;
                c = lp.process (hp.process (noise)) * 2.5f;
            }
            else
            {
                // fast downward chirp: a short "snap"
                const double f = toneHz * std::exp (-t / (decaySec * 0.7)) + 200.0;
                cph += f / fs;
                c = (float) std::sin (2.0 * kPi * cph);
            }
            out[(size_t) i] += c * env * p.clickLevel;
        }
    }

    // --- distortion, EQ, output ---
    Biquad shelf, cut;
    shelf.lowShelf (fs, 60.0, p.lowBoostDb);
    cut.lowPass (fs, p.highCutHz, 0.707);
    const float outGain = std::pow (10.0f, p.outGainDb / 20.0f);
    const int fadeLen = std::min (n / 4, (int) (0.004 * fs)); // de-click the tail

    for (int i = 0; i < n; ++i)
    {
        float s = out[(size_t) i];
        if (p.distDrive > 0.001f || p.distType == 3)
            s = s * (1.0f - p.distMix) + distort (s, p.distType, p.distDrive) * p.distMix;
        if (std::abs (p.lowBoostDb) > 0.01f) s = shelf.process (s);
        if (p.highCutHz < 19900.0f)          s = cut.process (s);
        s *= outGain;
        // gentle safety ceiling so stacked click + boost never hard-clips the DAW
        const float a = std::abs (s);
        if (a > 0.9f) s = std::copysign (0.9f + 0.08f * std::tanh ((a - 0.9f) / 0.08f), s); // soft knee, max -0.2 dBFS
        if (i >= n - fadeLen) s *= (float) (n - 1 - i) / (float) fadeLen;
        out[(size_t) i] = s;
    }
    return out;
}

//==============================================================================
inline Curve defaultPitchCurve()
{
    Curve c;
    c.nodes = { { 0.0f, hzToPitchY (8000.0f), -0.6f }, { 0.04f, hzToPitchY (180.0f), -0.3f },
                { 0.2f, hzToPitchY (58.0f), 0.0f },    { 1.0f, hzToPitchY (45.0f), 0.0f } };
    return c;
}

inline Curve defaultAmpCurve()
{
    Curve c;
    c.nodes = { { 0.0f, 1.0f, 0.0f }, { 0.55f, 0.85f, -0.35f }, { 1.0f, 0.0f, 0.0f } };
    return c;
}

} // namespace thump
