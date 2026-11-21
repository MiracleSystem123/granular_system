// Offline checks for the GRANULAR_SYSTEM C++ engine (mirrors the web version's tests).
// Build: c++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test
#include "Engine.h"
#include "Samples.h"
#include <cstdio>
#include <chrono>
#include <random>
#include <functional>
using namespace gs;

static const double SR = 48000;
struct Out { std::vector<float> L, R; long bad = 0; float pk = 0; };

static Out run (Engine& e, double secs, std::function<void (Engine&, int block)> perBlock = nullptr)
{
    Out o; float l[128], r[128];
    const int blocks = (int) (secs * SR / 128);
    for (int b = 0; b < blocks; ++b)
    {
        if (perBlock) perBlock (e, b);
        e.render (l, r, 128);
        for (int i = 0; i < 128; ++i) { o.L.push_back (l[i]); o.R.push_back (r[i]); if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) ++o.bad; o.pk = std::max (o.pk, std::max (std::abs (l[i]), std::abs (r[i]))); }
    }
    return o;
}
static float rms (const std::vector<float>& a, double t0, double t1) { double s = 0; long n = 0; for (long i = (long) (t0 * SR); i < (long) (t1 * SR) && i < (long) a.size(); ++i) { s += a[(size_t) i] * a[(size_t) i]; ++n; } return (float) std::sqrt (s / std::max (1L, n)); }
static float peakFreq (const std::vector<float>& x, float lo, float hi, float stepHz = 0.5f)
{
    const int N = (int) SR / 2, off = (int) (SR * 0.4); float best = 0, bf = 0;
    for (float f = lo; f <= hi; f += stepHz)
    {
        double re = 0, im = 0; const double w = 2 * M_PI * f / SR;
        for (int i = 0; i < N; i += 2) { re += x[(size_t) (off + i)] * std::cos (w * i); im += x[(size_t) (off + i)] * std::sin (w * i); }
        const float m = (float) (re * re + im * im); if (m > best) { best = m; bf = f; }
    }
    return bf;
}
static Params natural() { Params p; setDefaults (p); p.width = 0; p.rmix = 0; p.vol = 1; return p; }

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++fails; };

    // 1. Clean playback: NATURAL settings should reproduce the sample.
    {
        Sample s; s.sampleRate = SR; const int n = (int) SR * 2; s.L.resize ((size_t) n);
        std::mt19937 g (1); std::uniform_real_distribution<float> U (-1, 1);
        for (int i = 0; i < n; ++i) { const double t = i / SR; s.L[(size_t) i] = (float) (0.5 * std::sin (2 * M_PI * 220 * t) + 0.3 * std::sin (2 * M_PI * 331 * t) * std::exp (-std::fmod (t, 0.5) * 4) + 0.1 * U (g) * std::exp (-std::fmod (t, 0.25) * 30)); }
        s.R = s.L;
        Engine e; e.prepare (SR); e.setSample (&s); e.setParams (natural()); e.setDrone (true);
        auto o = run (e, 1.3);
        double best = 1e9;
        for (int lag = -50; lag <= 50; ++lag)
        {
            double num = 0, den = 0; std::vector<float> a, b;
            for (long t = (long) (SR * 0.3); t < (long) (SR * 1.2); t += 7) { a.push_back (o.L[(size_t) t]); b.push_back (s.L[(size_t) ((t + lag) % n)]); }
            for (size_t i = 0; i < a.size(); ++i) { num += a[i] * b[i]; den += b[i] * b[i]; }
            const double gg = num / den; double err = 0, pe = 0;
            for (size_t i = 0; i < a.size(); ++i) { err += (a[i] - gg * b[i]) * (a[i] - gg * b[i]); pe += a[i] * a[i]; }
            best = std::min (best, err / pe);
        }
        char msg[128]; std::snprintf (msg, sizeof msg, "natural playback matches the sample: residual %.1f dB", 10 * std::log10 (best));
        check (10 * std::log10 (best) < -25, msg);
    }
    // 2. Audio-rate density pitch accuracy, and density following MIDI notes.
    {
        Sample s; s.sampleRate = SR; s.L.resize ((size_t) SR * 2); for (size_t i = 0; i < s.L.size(); ++i) s.L[i] = 0.8f * (float) std::sin (2 * M_PI * 1500 * (double) i / SR); s.R = s.L;
        for (float d : { 110.f, 440.f, 2950.f })
        {
            Engine e; e.prepare (SR); e.setSample (&s); Params p = natural(); p.dens = d; p.size = 0.004f; p.scan = 0; e.setParams (p); e.setDrone (true);
            auto o = run (e, 1.6); const float f = peakFreq (o.L, d * 0.97f, d * 1.03f);
            char msg[128]; std::snprintf (msg, sizeof msg, "density %.0f/s heard at %.1f Hz", d, f); check (std::abs (f - d) < 1.5f, msg);
        }
        Engine e; e.prepare (SR); e.setSample (&s); Params p = natural(); p.dfollow = 1; p.dens = 261.6f; p.size = 0.006f; p.scan = 0; e.setParams (p); e.noteOn (72, 1);
        auto o = run (e, 1.6); const float f = peakFreq (o.L, 500, 550); char msg[128]; std::snprintf (msg, sizeof msg, "DENS->PITCH: MIDI C5 gives %.1f Hz (expect 523.3)", f); check (std::abs (f - 523.3f) < 1.5f, msg);
    }
    // 3. MIDI note pitch shifting (play mode) and pitch bend.
    {
        Sample s; s.sampleRate = SR; s.L.resize ((size_t) SR * 2); for (size_t i = 0; i < s.L.size(); ++i) s.L[i] = 0.6f * (float) std::sin (2 * M_PI * 300 * (double) i / SR); s.R = s.L;
        auto freqFor = [&] (int note, float bend, int pitchOn)
        {
            Engine e; e.prepare (SR); e.setSample (&s); Params p = natural(); p.pitchOn = pitchOn; e.setParams (p); e.setBend (bend); e.noteOn (note, 1);
            auto o = run (e, 1.4); return peakFreq (o.L, 100, 1300, 1.f);
        };
        check (std::abs (freqFor (60, 0, 1) - 300) < 2, "MIDI C4 plays the sample at its own pitch");
        check (std::abs (freqFor (72, 0, 1) - 600) < 2, "MIDI C5 plays an octave up");
        check (std::abs (freqFor (72, 0, 0) - 600) < 2, "MIDI notes still shift pitch with PITCH off");
        check (std::abs (freqFor (60, 12, 1) - 600) < 2, "pitch bend +12 st shifts an octave");
    }
    // 4. Ping-pong delay alternates sides; stereo stays centred.
    {
        Sample s; s.sampleRate = SR; s.L.assign ((size_t) SR * 2, 0.f); for (int i = 0; i < 2000; ++i) s.L[(size_t) i] = 0.8f * (float) std::sin (i * 0.07); s.R = s.L;
        Engine e; e.prepare (SR); e.setSample (&s); Params p = natural(); p.size = 0.04f; p.scan = 0; p.att = 0.001f; p.rel = 0.01f; p.dmix = 1; p.dfb = 0.6f; p.dtime = 0.25f; p.dtone = 0.9f; p.dmode = DPingPong; e.setParams (p);
        auto o = run (e, 1.3, [] (Engine& en, int b) { if (b == 0) en.setTest (true); if (b == (int) (0.04 * SR / 128)) en.setTest (false); });
        const bool alt = rms (o.L, 0.25, 0.31) > 10 * rms (o.R, 0.25, 0.31) && rms (o.R, 0.5, 0.56) > 10 * rms (o.L, 0.5, 0.56) && rms (o.L, 0.75, 0.81) > 10 * rms (o.R, 0.75, 0.81);
        check (alt, "ping-pong delay bounces L, R, L");
    }
    // 5. Reverb: near-infinite decay holds, stays stable under constant input.
    {
        Sample s; s.sampleRate = SR; s.L.resize ((size_t) SR * 2); std::mt19937 g (3); std::uniform_real_distribution<float> U (-1, 1); for (auto& x : s.L) x = U (g); s.R = s.L;
        Engine e; e.prepare (SR); e.setSample (&s); Params p = natural(); p.rmix = 1; p.rdecay = 1; p.rdamp = 0; p.dens = 60; e.setParams (p); e.setDrone (true);
        auto o = run (e, 20); char msg[128]; std::snprintf (msg, sizeof msg, "20 s of noise into infinite reverb: peak %.3f, non-finite %ld", o.pk, o.bad);
        check (o.bad == 0 && o.pk <= 1.0f, msg);
        Engine e2; e2.prepare (SR); e2.setSample (&s); p.dens = 20; e2.setParams (p);
        auto o2 = run (e2, 12, [] (Engine& en, int b) { if (b == 0) en.setTest (true); if (b == (int) (0.1 * SR / 128)) en.setTest (false); });
        std::snprintf (msg, sizeof msg, "infinite decay tail after 11 s: rms %.4f (vs %.4f at 1 s)", rms (o2.L, 11, 11.5), rms (o2.L, 1, 1.5));
        check (rms (o2.L, 11, 11.5) > 0.3f * rms (o2.L, 1, 1.5), msg);
    }
    // 6. Sequencer: marker positions, assign-lane overrides and transpose.
    {
        auto smp = makeDemo (0);
        Engine e; e.prepare (SR); e.setSample (smp.get()); Params p; setDefaults (p); p.assignA = 1; e.setParams (p);
        Step st; st.on = true; st.a = 1.0f;
        e.doStep (0, st, 0.25f, SR * 0.125);
        check (std::abs (e.effective().rmix - 1.0f) < 1e-4f, "assign lane A writes REVERB MIX for the step");
        e.setParams (p);
        check (std::abs (e.effective().rmix - 1.0f) < 1e-4f, "assign override survives a parameter update mid-step");
        Step st2; st2.on = true;
        e.doStep (1, st2, 0.5f, SR * 0.125);
        check (std::abs (e.effective().rmix - p.rmix) < 1e-4f, "empty lane step falls back to the knob value");
        auto o = run (e, 0.2); check (o.pk > 0.01f && o.bad == 0, "sequenced step makes sound");
    }
    // 7. Fuzz: random extreme settings with drone, notes and sequencer.
    {
        std::mt19937 g (7); std::uniform_real_distribution<float> U (0, 1); long bad = 0; float pk = 0;
        auto smp = makeDemo (2);
        for (int t = 0; t < 120; ++t)
        {
            Params p; setDefaults (p);
            for (const auto& d : floatDefs) { float u = U (g); if (U (g) < 0.2f) u = U (g) < 0.5f ? 0.f : 1.f; p.*(d.field) = fromNorm (d, u); }
            for (const auto& d : choiceDefs) p.*(d.field) = std::min (d.count - 1, (int) (U (g) * (float) d.count));
            p.att = std::min (p.att, 0.3f);
            Engine e; e.prepare (t % 2 ? 44100 : 96000); e.setSample (smp.get()); e.setParams (p); e.setDrone (true); e.noteOn (40 + t % 40, 0.8f); e.noteOn (67, 1);
            Step st; st.on = true; st.a = U (g); st.b = U (g); st.n = (int) (U (g) * 48) - 24; st.sz = 2; e.doStep (0, st, U (g), 6000);
            auto o = run (e, 0.4); bad += o.bad; pk = std::max (pk, o.pk);
        }
        char msg[128]; std::snprintf (msg, sizeof msg, "fuzz 120 random patches: non-finite %ld, peak %.3f", bad, pk); check (bad == 0 && pk <= 1.0f, msg);
    }
    // 8. CPU.
    {
        auto smp = makeDemo (2);
        Engine e; e.prepare (SR); e.setSample (smp.get()); Params p; setDefaults (p); p.dens = 4000; p.size = 0.06f; p.fxType = FxEnsemble; p.dmix = 0.5f; p.rmix = 0.5f; p.cut = 800; p.res = 0.8f; p.drive = 0.5f; e.setParams (p); e.setDrone (true);
        auto t0 = std::chrono::steady_clock::now(); run (e, 10); const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("info  CPU worst case (4000 grains/s + all effects): %.1f%% of one core\n", ms / 100.0);
        Engine e2; e2.prepare (SR); e2.setSample (smp.get()); Params q; setDefaults (q); q.dens = 40; q.size = 0.15f; q.rmix = 0.3f; q.dmix = 0.2f; e2.setParams (q); e2.setDrone (true);
        t0 = std::chrono::steady_clock::now(); run (e2, 10); const double ms2 = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("info  CPU typical: %.1f%% of one core\n", ms2 / 100.0);
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails ? 1 : 0;
}
