// Built-in demo sounds, synthesised on load so the plugin makes noise before you load a file.
#pragma once
#include "Engine.h"
#include <memory>

namespace gs
{
inline const char* const demoNames[] = { "DRUM LOOP", "GLASS CHORD", "CHOIR-ISH", "MUSIC BOX", "NOISE SWEEP" };
constexpr int numDemos = 5;

inline std::shared_ptr<Sample> makeDemo (int which)
{
    static const int remap[] = { 1, 0, 2, 3, 4 };
    which = remap[which < 0 ? 0 : which > 4 ? 4 : which];
    constexpr double SR = 44100.0;
    const int n = (int) (SR * 4.0);
    auto s = std::make_shared<Sample>();
    s->sampleRate = SR;
    s->L.assign ((size_t) n, 0.0f);
    s->R.assign ((size_t) n, 0.0f);
    auto& L = s->L; auto& R = s->R;
    uint32_t seed = 12345u;
    auto rnd = [&seed]() { seed = seed * 1103515245u + 12345u; return (float) (seed / 4294967296.0) * 2.0f - 1.0f; };
    auto add = [&] (int i, float l, float r) { if (i >= 0 && i < n) { L[(size_t) i] += l; R[(size_t) i] += r; } };
    auto mtof = [] (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); };
    const double T = 6.283185307179586;

    if (which == 0) // glass chord
    {
        const int notes[] = { 72, 76, 79, 83, 84, 88 };
        for (int k = 0; k < 6; ++k)
        {
            const int t0 = (int) (k * 0.35 * SR); const double f = mtof (notes[k]); const float pan = (float) k / 5.0f;
            for (int i = t0; i < n; ++i)
            {
                const double t = (i - t0) / SR, e = std::exp (-t * 1.1);
                if (e < 1e-4) break;
                const float v = (float) ((std::sin (T * f * t) + 0.5 * std::sin (T * f * 2.756 * t) * std::exp (-t * 3)
                                         + 0.25 * std::sin (T * f * 5.404 * t) * std::exp (-t * 6)) * e * 0.18);
                add (i, v * (1 - pan * 0.6f), v * (0.4f + pan * 0.6f));
            }
        }
    }
    else if (which == 1) // drum loop
    {
        const double b = SR * 0.5;
        for (int beat = 0; beat < 8; ++beat)
        {
            const int t0 = (int) (beat * b);
            if (beat % 2 == 0 || beat == 5)
                for (int i = 0; i < (int) (SR * 0.35); ++i)
                {
                    const double t = i / SR;
                    const float v = (float) (std::sin (T * (45 * t + 90 * (1 - std::exp (-t * 30)) / 30)) * std::exp (-t * 7) * 0.8);
                    add (t0 + i, v, v);
                }
            if (beat % 2 == 1)
                for (int i = 0; i < (int) (SR * 0.25); ++i)
                {
                    const double t = i / SR;
                    const float v = (float) ((rnd() * 0.6 + std::sin (T * 190 * t) * 0.4) * std::exp (-t * 16) * 0.55);
                    add (t0 + i, v, v);
                }
            for (int h = 0; h < 2; ++h)
            {
                const int th = (int) (t0 + h * b / 2); float lp = 0;
                for (int i = 0; i < (int) (SR * 0.08); ++i)
                {
                    const float x = rnd(), hp = x - lp; lp = x;
                    const float v = hp * (float) std::exp (-i / SR * 60) * 0.25f;
                    add (th + i, v * 0.7f, v);
                }
            }
        }
    }
    else if (which == 2) // choir-ish: saw chord through morphing formants
    {
        const int chord[] = { 48, 55, 63, 70 };
        const double vow[4][3] = { { 730, 1090, 2440 }, { 570, 840, 2410 }, { 300, 870, 2240 }, { 270, 2290, 3010 } };
        double ph[4] = { 0, 0, 0, 0 }; double st[3][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
        for (int i = 0; i < n; ++i)
        {
            const double t = i / SR; double x = 0;
            for (int k = 0; k < 4; ++k)
            {
                const double f = mtof (chord[k]) * (1 + 0.006 * std::sin (T * 5.2 * t + k));
                ph[k] += f / SR; ph[k] -= std::floor (ph[k]); x += (ph[k] * 2 - 1) * 0.25;
            }
            const double vp = std::min (2.999, t / 4 * 3); const int vi = (int) vp; const double vf = vp - vi;
            double y = 0; const double gains[3] = { 1, 0.6, 0.3 };
            for (int j = 0; j < 3; ++j)
            {
                const double fc = vow[vi][j] * (1 - vf) + vow[vi + 1][j] * vf;
                const double g = std::tan (3.141592653589793 * fc / SR), kk = 0.12, a1 = 1 / (1 + g * (g + kk)), a2 = g * a1, a3 = g * a2;
                const double v3 = x - st[j][1], v1 = a1 * st[j][0] + a2 * v3, v2 = st[j][1] + a2 * st[j][0] + a3 * v3;
                st[j][0] = 2 * v1 - st[j][0]; st[j][1] = 2 * v2 - st[j][1]; y += v1 * gains[j];
            }
            const double e = std::min (1.0, t * 3) * std::min (1.0, (4 - t) * 3) * 0.22;
            add (i, (float) (y * e), (float) (y * e * 0.92));
        }
    }
    else if (which == 3) // music box
    {
        const int mel[] = { 76, 79, 83, 84, 83, 79, 76, 72, 74, 76, 79, 74, 72, 71, 67, 72 };
        for (int k = 0; k < 16; ++k)
        {
            const int t0 = (int) (k * 0.25 * SR); const double f = mtof (mel[k]);
            for (int i = t0; i < n; ++i)
            {
                const double t = (i - t0) / SR, e = std::exp (-t * 3.5);
                if (e < 1e-4) break;
                const float v = (float) ((std::sin (T * f * t) + 0.3 * std::sin (T * f * 3.01 * t) * std::exp (-t * 8)) * e * 0.3);
                add (i, v, v * (0.7f + 0.3f * (float) (k % 2)));
            }
        }
    }
    else // noise sweep
    {
        double lp = 0, bp = 0;
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / n, fc = 200 * std::pow (40.0, std::sin (3.141592653589793 * t));
            const double f = 2 * std::sin (3.141592653589793 * fc / SR), x = rnd();
            lp += f * bp; const double hp = x - lp - 0.15 * bp; bp += f * hp;
            const float v = (float) (bp * 0.35 + std::sin (T * (55 + t * 110) * i / SR) * 0.15);
            add (i, v, v * 0.9f);
        }
    }
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
    const float g = 0.9f / (pk > 0 ? pk : 1.0f);
    for (int i = 0; i < n; ++i) { L[(size_t) i] *= g; R[(size_t) i] *= g; }
    return s;
}
} // namespace gs

namespace gs
{
// Onset detection for automatic marker placement (same method as the web version).
inline std::vector<float> evenMarkers (int k) { std::vector<float> v; for (int i = 0; i < k; ++i) v.push_back ((float) i / (float) k); return v; }
inline std::vector<float> detectHits (const Sample& s, int maxHits = 16)
{
    const int n = (int) s.length(), hop = 256, frames = n / hop;
    if (frames < 8) return evenMarkers (8);
    std::vector<float> e ((size_t) frames), flux ((size_t) frames, 0.f);
    for (int f = 0; f < frames; ++f)
    {
        double a = 0;
        for (int i = f * hop; i < f * hop + hop; ++i) { const float v = (s.L[(size_t) i] + s.R[(size_t) i]) * 0.5f; a += v * v; }
        e[(size_t) f] = (float) std::log (a / hop + 1e-9);
    }
    for (int f = 4; f < frames; ++f) { const float prev = (e[(size_t) f - 1] + e[(size_t) f - 2] + e[(size_t) f - 3] + e[(size_t) f - 4]) / 4; flux[(size_t) f] = std::max (0.f, e[(size_t) f] - prev); }
    double mean = 0; for (float x : flux) mean += x; mean /= frames;
    double sd = 0; for (float x : flux) sd += (x - mean) * (x - mean); sd = std::sqrt (sd / frames);
    const float thr = (float) (mean + sd * 1.2); const int gap = (int) (0.09 * s.sampleRate / hop);
    std::vector<std::pair<int, float>> c;
    for (int f = 1; f < frames - 1; ++f) if (flux[(size_t) f] > thr && flux[(size_t) f] >= flux[(size_t) f - 1] && flux[(size_t) f] >= flux[(size_t) f + 1]) c.push_back ({ f, flux[(size_t) f] });
    std::sort (c.begin(), c.end(), [] (auto& a, auto& b) { return a.second > b.second; });
    std::vector<int> chosen;
    for (auto& x : c) { bool ok = true; for (int y : chosen) if (std::abs (y - x.first) <= gap) ok = false; if (ok) chosen.push_back (x.first); if ((int) chosen.size() >= maxHits) break; }
    std::vector<float> pos; for (int f : chosen) pos.push_back ((float) std::max (0, (f - 1) * hop) / (float) n);
    std::sort (pos.begin(), pos.end());
    if (pos.empty() || pos[0] > 0.012f) pos.insert (pos.begin(), 0.f);
    if ((int) pos.size() > maxHits) pos.resize ((size_t) maxHits);
    return pos.size() >= 2 ? pos : evenMarkers (8);
}
} // namespace gs
