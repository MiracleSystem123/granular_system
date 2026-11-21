// GRANULAR_SYSTEM engine: framework-free C++17, allocation-free in render().
// A direct port of the web version's AudioWorklet (v5).
#pragma once
#include "Params.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace gs
{
constexpr float kTau = 6.28318530718f, kPi = 3.14159265359f;
inline float sclip (float x) { if (x > 3.f) return 1.f; if (x < -3.f) return -1.f; const float x2 = x * x; return x * (27.f + x2) / (27.f + 9.f * x2); }

struct Sample { std::vector<float> L, R; double sampleRate = 44100; size_t length() const { return L.size(); } };

struct Step { bool on = false; float v = 0.9f; int m = -1; int n = 0; float len = 1, sz = 1; float a = -1, b = -1; }; // a/b < 0 = empty

class Engine
{
public:
    static constexpr int kGrains = 256;

    void prepare (double newSr)
    {
        sr = (float) newSr;
        mbL.assign ((size_t) (sr * 0.08f) + 4, 0); mbR.assign (mbL.size(), 0);
        dL.assign ((size_t) (sr * 3.0f) + 4, 0); dR.assign (dL.size(), 0);
        static const int cl[4] = { 1116, 1188, 1277, 1356 }, al[2] = { 556, 441 };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 4; ++i) { auto& c = combs[ch][i]; c.n = (int) std::ceil ((cl[i] + ch * 23) * sr / 44100.f); c.b.assign ((size_t) (c.n * 1.6f) + 2, 0); c.i = 0; c.s = 0; }
            for (int i = 0; i < 2; ++i) { auto& a = aps[ch][i]; a.b.assign ((size_t) std::ceil ((al[i] + ch * 23) * sr / 44100.f), 0); a.i = 0; }
        }
        reset();
    }
    void reset()
    {
        for (auto& g : G) g.on = false;
        gE = 0; scan = 0; stepScan = 0; acc = 0; next = 1; stut = 0; sg = 0; fe = 0; fst = 0;
        for (auto& a : sacc) a = { 0, 1 };
        for (auto& k : held) k.acc = { 0, 1 };
        std::fill (std::begin (svf), std::end (svf), 0.f);
        std::fill (dL.begin(), dL.end(), 0.f); std::fill (dR.begin(), dR.end(), 0.f);
        std::fill (mbL.begin(), mbL.end(), 0.f); std::fill (mbR.begin(), mbR.end(), 0.f);
        for (auto& ch : combs) for (auto& c : ch) { std::fill (c.b.begin(), c.b.end(), 0.f); c.s = 0; }
        for (auto& ch : aps) for (auto& a : ch) std::fill (a.b.begin(), a.b.end(), 0.f);
        std::fill (std::begin (apL), std::end (apL), 0.0); std::fill (std::begin (apR), std::end (apR), 0.0);
        lim = 0; rh1 = rh2 = rx1 = rx2 = 0; dlp[0] = dlp[1] = 0; afL = afR = 0; holdL = holdR = 0;
    }

    // ---------- control ----------
    void setSample (const Sample* s) { if (s != sample) { sample = s; for (auto& g : G) g.on = false; scan = 0; } }
    void setParams (const Params& p)
    {
        base = p; ovA.field = nullptr; ovB.field = nullptr;
        if (curStep >= 0) applyOverrides (lastStepData);
        else P = base;
        if (P.gatk != wA || P.grel != wR || P.gcurve != wC) buildWindow();
    }
    void setTest (bool on)  { if (on && ! test) { acc = 1; next = 0; sacc[1] = { 1, 0 }; trigEnv (1); } test = on; }
    void setDrone (bool on) { if (on && ! drone) { acc = 1; next = 0; sacc[2] = { 1, 0 }; trigEnv (1); } drone = on; }
    void setTranspose (float semis) { ktr = semis; }
    void setBend (float semis) { bend = semis; }
    void resetScan() { scan = 0; }

    void noteOn (int note, float vel)
    {
        noteOff (note);
        if (numHeld < (int) held.size()) { held[(size_t) numHeld++] = { note, vel, { 1, 0 } }; }
        acc = 1; next = 0; trigEnv (vel);
    }
    void noteOff (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i].note == note) { for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --numHeld; --i; }
    }
    void allNotesOff() { numHeld = 0; }
    int  heldCount() const { return numHeld; }

    // Called by the host wrapper at each sequencer step boundary.
    // pos = resolved marker position for the step (<0 = follow the position line).
    void doStep (int index, const Step& st, float pos, double stepSamples)
    {
        curStep = index; lastStepData = st; lastStepData.a = st.a; lastStepData.b = st.b;
        applyOverrides (st);
        if (st.on)
        {
            sg = st.len * P.gate * stepSamples; sn = (float) st.n; sv = st.v; spos = pos; ssz = st.sz;
            stepScan = 0; acc = 1; next = 0; sacc[0] = { 1, 0 }; trigEnv (st.v);
        }
    }
    void stopSequence() { sg = 0; curStep = -1; P = base; }
    int  currentStep() const { return curStep; }
    const Params& effective() const { return P; }

    // ---------- visualisation ----------
    float basePosition() const { return (float) wrapD (P.pos + scan); }
    float envLevel() const { return fe; }
    int getViz (float* posOut, float* ampOut, int maxN) const
    {
        if (! sample || sample->length() < 2) return 0;
        const double len = (double) sample->length(); int c = 0;
        for (const auto& g : G)
        {
            if (! g.on || c >= maxN) continue;
            double idx = std::fmod (g.pos + g.age * g.rate, len); if (idx < 0) idx += len;
            posOut[c] = (float) (idx / len);
            ampOut[c] = g.amp * gE * win[(size_t) std::min (1024, (int) (g.age / g.dur * 1024.0))];
            ++c;
        }
        return c;
    }

    // ---------- audio ----------
    void render (float* OL, float* OR, int N)
    {
        if (! sample || sample->length() < 2 || dL.empty()) { std::fill (OL, OL + N, 0.f); std::fill (OR, OR + N, 0.f); return; }
        const float* SL = sample->L.data(); const float* SR = sample->R.data();
        const double len = (double) sample->length(); const int ilen = (int) sample->length();
        const int dlen = (int) dL.size();
        for (int n = 0; n < N; ++n)
        {
            const Params& p = P;
            if ((cc & 63) == 0) control();
            if ((cc & 7) == 0) filterCoefs();
            ++cc;
            if (sg > 0) sg -= 1.0;
            const bool active = sg > 0 || test || drone || numHeld > 0;
            if (active && gE < 1) gE = std::min (1.f, gE + 1.f / (p.att * sr));
            else if (! active && gE > 0) gE = std::max (0.f, gE - 1.f / (p.rel * sr));
            if (fst == 1) { fe += 1.f / (p.fatk * sr); if (fe >= 1) { fe = 1; fst = 2; } if (! active) fst = 3; }
            else if (fst == 2) { if (active) fe = p.fsus + (fe - p.fsus) * kd; else fst = 3; }
            else if (fst == 3) { fe *= kr; if (fe < 1e-5f) { fe = 0; fst = 0; } }

            const double sc = p.scan * sample->sampleRate / len / sr;
            scan += sc; scan -= std::floor (scan); stepScan += sc;
            dens = p.dens * std::exp2 (m[TDens] * 3.f);

            if (p.dfollow)
            {
                const float pb = p.pitchOn ? p.pitch + p.fine / 100.f : 0.f;
                if (sg > 0) runSource (sacc[0], 0, pb + ktr + ((p.pitchOn && p.pseq) ? sn : 0.f));
                if (test && ! numHeld) runSource (sacc[1], 1, pb + p.tpitch);
                if (drone && ! numHeld && ! test) runSource (sacc[2], 2, pb + p.dpitch);
                for (int i = 0; i < numHeld; ++i) runSource (held[(size_t) i].acc, 3 + i, pb + (float) (held[(size_t) i].note - 60) + bend);
                stut = 0;
            }
            else if (active || gE > 0.0005f)
            {
                const float inc = dens / sr; acc += inc;
                if (stut > 0 && acc >= 0.25f) { acc = 0; --stut; spawn (-1, 0, 0); }
                else if (acc >= next) { acc -= next; if (acc > 1) acc = 0; const float off = acc / inc; next = 1 + rnd11() * p.chaos * 0.95f; spawn (-1, off, 0); }
            }

            float l = 0, r = 0;
            for (auto& g : G)
            {
                if (! g.on) continue;
                if (g.age >= g.dur) { g.on = false; continue; }
                const float w = win[(size_t) (g.age / g.dur * 1024.0)];
                double idx = std::fmod (g.pos + g.age * g.rate, len); if (idx < 0) idx += len;
                const int i0 = (int) idx, i1 = i0 + 1 < ilen ? i0 + 1 : 0; const float fr = (float) (idx - i0);
                const float a = w * g.amp;
                l += (SL[i0] + (SL[i1] - SL[i0]) * fr) * a * g.pl;
                r += (SR[i0] + (SR[i1] - SR[i0]) * fr) * a * g.pr;
                g.age += 1.0;
            }
            l *= gE; r *= gE;

            // state-variable filter + drive
            if (fOn)
            {
                float v3 = l - svf[1], v1 = fa1 * svf[0] + fa2 * v3, v2 = svf[1] + fa2 * svf[0] + fa3 * v3; svf[0] = 2 * v1 - svf[0]; svf[1] = 2 * v2 - svf[1];
                const float yl = p.ftype == FLP ? v2 : p.ftype == FBP ? v1 * (1 + fk) * 0.8f : l - fk * v1 - v2;
                v3 = r - svf[3]; v1 = fa1 * svf[2] + fa2 * v3; v2 = svf[3] + fa2 * svf[2] + fa3 * v3; svf[2] = 2 * v1 - svf[2]; svf[3] = 2 * v2 - svf[3];
                const float yr = p.ftype == FLP ? v2 : p.ftype == FBP ? v1 * (1 + fk) * 0.8f : r - fk * v1 - v2;
                if (! (std::abs (svf[0]) + std::abs (svf[2]) < 1e4f)) std::fill (std::begin (svf), std::end (svf), 0.f);
                l = yl; r = yr;
            }
            { const float drv = clamp01 (p.drive + m[TDrive]); if (drv > 0) { const float d = 1 + drv * 10, mk = 1.5f / (1 + drv * 2); l = sclip (l * d) * mk; r = sclip (r * d) * mk; } }
            if (p.crush > 0)
            {
                if (--holdC <= 0) { holdC = 1 + (int) (p.crush * p.crush * 40); const float q = std::exp2 (16 - p.crush * 13); holdL = std::round (l * q) / q; holdR = std::round (r * q) / q; }
                l = holdL; r = holdR;
            }
            // modulation fx
            if (p.fxType != FxOff)
            {
                const float mix = clamp01 (p.fxMix + m[TFxMix]), dep = clamp01 (p.fxDepth + m[TFxDepth]);
                fxph += p.fxRate / sr; if (fxph >= 1) fxph -= 1;
                const float s = std::sin (kTau * fxph), c = std::cos (kTau * fxph);
                float wl = 0, wr = 0;
                if (p.fxType == FxChorus) { wl = rd (mbL, (0.012f + dep * 0.009f * (1 + s)) * sr); wr = rd (mbR, (0.012f + dep * 0.009f * (1 + c)) * sr); mbL[(size_t) mw] = l; mbR[(size_t) mw] = r; }
                else if (p.fxType == FxFlanger)
                {
                    const float fb = p.fxFb * 0.92f;
                    wl = rd (mbL, (0.0004f + dep * 0.0045f * (1 + s) / 2) * sr); wr = rd (mbR, (0.0004f + dep * 0.0045f * (1 + c) / 2) * sr);
                    mbL[(size_t) mw] = sclip (l + wl * fb); mbR[(size_t) mw] = sclip (r + wr * fb);
                }
                else if (p.fxType == FxEnsemble)
                {
                    ph2 += 6.3f / sr; if (ph2 >= 1) ph2 -= 1; const float v = std::sin (kTau * ph2) * 0.00035f;
                    const float t0 = rd (mbL, (0.008f + dep * 0.005f * (1 + std::sin (kTau * fxph)) + v) * sr);
                    const float t1 = rd (mbL, (0.008f + dep * 0.005f * (1 + std::sin (kTau * (fxph + 1.f / 3))) - v) * sr);
                    const float t2 = rd (mbL, (0.008f + dep * 0.005f * (1 + std::sin (kTau * (fxph + 2.f / 3))) + v * 0.7f) * sr);
                    wl = t0 * 0.8f + t1 * 0.45f; wr = t2 * 0.8f + t1 * 0.45f; mbL[(size_t) mw] = (l + r) * 0.5f;
                }
                else // phaser
                {
                    const double fb = p.fxFb * 0.85; double xl = l + afL * fb, xr = r + afR * fb;
                    for (int k = 0; k < 6; ++k)
                    {
                        const double yl = aL * xl + apL[k * 2] - aL * apL[k * 2 + 1]; apL[k * 2] = xl; apL[k * 2 + 1] = yl; xl = yl;
                        const double yr = aR * xr + apR[k * 2] - aR * apR[k * 2 + 1]; apR[k * 2] = xr; apR[k * 2 + 1] = yr; xr = yr;
                    }
                    afL = sclip ((float) xl); afR = sclip ((float) xr); wl = (float) xl; wr = (float) xr;
                }
                mw = (mw + 1) % (int) mbL.size();
                if (p.fxType == FxPhaser) { l = l * (1 - mix * 0.5f) + wl * mix * 0.5f; r = r * (1 - mix * 0.5f) + wr * mix * 0.5f; }
                else { l = l * (1 - mix * 0.45f) + wl * mix * 0.75f; r = r * (1 - mix * 0.45f) + wr * mix * 0.75f; }
            }
            // delay
            const float dmix = clamp01 (p.dmix + m[TDMix]);
            if (dmix > 0)
            {
                const float dt = p.dsync > 0 ? (float) (60.0 / hostBpm * 4.0 * kDivVals[p.dsync - 1]) : p.dtime;
                const float ds = std::min ((float) dlen - 2, std::max (1.f, dt * sr));
                float rp = (float) dw - ds; if (rp < 0) rp += (float) dlen; if (rp >= (float) dlen) rp -= (float) dlen;
                const int j0 = ((int) rp) % dlen, j1 = (j0 + 1) % dlen; const float fr = rp - (float) (int) rp;
                const float dl = dL[(size_t) j0] + (dL[(size_t) j1] - dL[(size_t) j0]) * fr, dr = dR[(size_t) j0] + (dR[(size_t) j1] - dR[(size_t) j0]) * fr;
                const float tc = 1 - std::exp (-kTau * 300 * std::pow (60.f, p.dtone) / sr);
                dlp[0] += tc * (dl - dlp[0]); dlp[1] += tc * (dr - dlp[1]);
                if (p.dmode == DPingPong) { dL[(size_t) dw] = sclip ((l + r) * 0.5f + dlp[1] * p.dfb); dR[(size_t) dw] = sclip (dlp[0] * p.dfb); }
                else { dL[(size_t) dw] = sclip (l + dlp[0] * p.dfb); dR[(size_t) dw] = sclip (r + dlp[1] * p.dfb); }
                dw = (dw + 1) % dlen;
                l += dlp[0] * dmix; r += dlp[1] * dmix;
            }
            // reverb
            const float rmix = clamp01 (p.rmix + m[TRMix]);
            if (rmix > 0)
            {
                const float rsz = reverbFeedback (p.rdecay), damp = 0.05f + p.rdamp * 0.6f, scl = 0.6f + p.rsize * 0.9f;
                const float ig = 0.12f * std::min (1.f, std::sqrt ((1 - rsz) / 0.02f));
                const float hc = std::exp (-kTau * p.rhp / sr);
                float x = (l + r) * 0.5f;
                rh1 = hc * (rh1 + x - rx1); rx1 = x; rh2 = hc * (rh2 + rh1 - rx2); rx2 = rh1; x = rh2;
                float o[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    float s = 0;
                    for (auto& cb : combs[ch])
                    {
                        int n2 = (int) ((float) cb.n * scl); n2 = std::max (8, std::min (n2, (int) cb.b.size()));
                        if (cb.i >= n2) cb.i = 0;
                        const float y = cb.b[(size_t) cb.i]; cb.s = y * (1 - damp) + cb.s * damp;
                        cb.b[(size_t) cb.i] = sclip (x * ig + cb.s * rsz); if (++cb.i >= n2) cb.i = 0; s += y;
                    }
                    for (auto& a : aps[ch]) { const float y = a.b[(size_t) a.i], out = y - s * 0.5f; a.b[(size_t) a.i] = s + out * 0.5f; s = out; if (++a.i >= (int) a.b.size()) a.i = 0; }
                    o[ch] = s;
                }
                l = l * (1 - rmix * 0.5f) + o[0] * rmix * 1.4f; r = r * (1 - rmix * 0.5f) + o[1] * rmix * 1.4f;
            }
            // output + gentle limiter
            l *= p.vol; r *= p.vol;
            const float lv = std::max (std::abs (l), std::abs (r));
            if (lv > lim) lim += (lv - lim) * 0.03f; else lim *= 0.99993f;
            const float lg = lim > 0.8f ? 0.8f / lim : 1.f;
            OL[n] = sclip (l * lg); OR[n] = sclip (r * lg);
        }
    }

    double hostBpm = 120.0;

private:
    struct Grain { bool on = false; double pos = 0, rate = 1, age = 0, dur = 1; float amp = 0, pl = 1, pr = 1; };
    struct Acc { float acc = 0, next = 1; };
    struct Held { int note = 60; float vel = 1; Acc acc; };
    struct Comb { std::vector<float> b; int i = 0, n = 1; float s = 0; };
    struct AP { std::vector<float> b; int i = 0; };
    struct Override { float Params::* field = nullptr; float value = 0; };

    static float clamp01 (float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
    static float wrap01 (float x) { x = std::fmod (x, 1.f); return x < 0 ? x + 1 : x; }
    static double wrapD (double x) { x = std::fmod (x, 1.0); return x < 0 ? x + 1 : x; }
    float rnd01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng >> 8) * (1.f / 16777216.f); }
    float rnd11() { return rnd01() * 2 - 1; }
    void trigEnv (float v) { fst = 1; fvel = v; }

    float rd (const std::vector<float>& b, float d) const
    {
        const int n = (int) b.size(); float rp = (float) mw - d; if (rp < 0) rp += (float) n;
        const int i0 = ((int) rp) % n, i1 = (i0 + 1) % n; const float fr = rp - (float) (int) rp;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
    }

    void applyOverrides (const Step& st)
    {
        P = base;
        auto apply = [this] (int assign, float u)
        {
            if (assign <= 0 || assign >= numAssign || u < 0) return;
            if (const FloatDef* d = findFloat (kAssignIds[assign])) P.*(d->field) = fromNorm (*d, u);
        };
        apply (base.assignA, st.a);
        apply (base.assignB, st.b);
        if (P.gatk != wA || P.grel != wR || P.gcurve != wC) buildWindow();
    }

    void buildWindow()
    {
        wA = P.gatk; wR = P.grel; wC = P.gcurve;
        float A = std::max (0.002f, wA), R = std::max (0.002f, wR);
        if (A + R > 1) { const float s = A + R; A /= s; R /= s; }
        const float seg = std::max (0.f, std::min (0.9999f, wC)) * 4; const int si = (int) seg; const float fr = seg - (float) si;
        auto f = [] (int i, float x)
        {
            switch (i)
            {
                case 0:  return x * x * x * x;
                case 1:  return x;
                case 2:  return 0.5f - 0.5f * std::cos (kPi * x);
                case 3:  return std::sin (kPi * x / 2);
                default: return std::sqrt (std::sin (kPi * x / 2));
            }
        };
        double s1 = 0, s2 = 0;
        for (int i = 0; i <= 1025; ++i)
        {
            const float t = std::min (1.f, (float) i / 1024.f);
            float x = t < A ? t / A : t > 1 - R ? (1 - t) / R : 1; x = clamp01 (x);
            const float w = f (si, x) * (1 - fr) + f (si + 1, x) * fr;
            win[(size_t) i] = w;
            if (i < 1024) { s1 += w; s2 += (double) w * w; }
        }
        mW = std::max (0.05f, (float) (s1 / 1024)); mW2 = std::max (0.02f, (float) (s2 / 1024));
    }

    float lfoValue (float ph, int shape, float hold) const
    {
        switch (shape)
        {
            case LSine:   return std::sin (kTau * ph);
            case LTri:    return 1 - 4 * std::abs (ph - 0.5f);
            case LSquare: return ph < 0.5f ? 1.f : -1.f;
            case LSaw:    return ph * 2 - 1;
            default:      return hold;
        }
    }

    void control()
    {
        const Params& p = P; const float n = 64.f / sr;
        const float a = l1 + p.l1r * n, b = l2 + p.l2r * n;
        if (a >= 1) h1 = rnd11();
        if (b >= 1) h2 = rnd11();
        l1 = std::fmod (a, 1.f); l2 = std::fmod (b, 1.f);
        m.fill (0.f);
        if (p.l1t > TNone && p.l1t < NumTargets) m[(size_t) p.l1t] += lfoValue (l1, p.l1s, h1) * p.l1d;
        if (p.l2t > TNone && p.l2t < NumTargets) m[(size_t) p.l2t] += lfoValue (l2, p.l2s, h2) * p.l2d;
        kd = std::exp (-3.f / (p.fdec * sr)); kr = std::exp (-3.f / (p.frel * sr));
        if (p.fxType == FxPhaser)
        {
            const float dep = clamp01 (p.fxDepth + m[TFxDepth]), s = std::sin (kTau * fxph), c = std::cos (kTau * fxph);
            auto co = [this] (float f) { const double t = std::tan (kPi * std::min (sr * 0.45f, f) / sr); return (t - 1) / (t + 1); };
            aL = co (160 * std::exp2 (dep * 5.5f * (1 + s) / 2)); aR = co (160 * std::exp2 (dep * 5.5f * (1 + c) / 2));
        }
    }

    void filterCoefs()
    {
        const Params& p = P; const float env = fe * fvel;
        fOn = ! (p.cut >= 17900 && p.fenv == 0 && m[TCut] == 0 && p.ftype == FLP);
        float fc = p.cut * std::exp2 (p.fenv * 6 * env + m[TCut] * 4);
        fc = std::min (sr * 0.45f, std::max (30.f, fc));
        const float g = std::tan (kPi * fc / sr); fk = 2 - 1.94f * clamp01 (p.res + m[TRes]);
        fa1 = 1 / (1 + g * (g + fk)); fa2 = g * fa1; fa3 = g * fa2;
    }

    void runSource (Acc& a, int src, float semi)
    {
        const float rate = dens * std::exp2 (semi / 12.f), inc = rate / sr;
        a.acc += inc;
        if (a.acc >= a.next)
        {
            a.acc -= a.next; if (a.acc > 1) a.acc = 0;
            const float off = a.acc / inc; a.next = 1 + rnd11() * P.chaos * 0.95f;
            spawn (src, off, rate);
        }
    }

    void spawn (int src, float off, float rateHint)
    {
        const Params& p = P; const bool follow = p.dfollow != 0;
        const bool hs = sg > 0;
        float explicitPitch = 0, vel = 1, szm = 1, seqNote = 0; double pos;
        const double basePos = p.pos + scan;
        if (src < 0)
        {
            const bool tt = test && ! numHeld, dd = drone && ! numHeld && ! test;
            const int cnt = (hs ? 1 : 0) + (tt ? 1 : 0) + (dd ? 1 : 0) + numHeld;
            if (! cnt) return;
            int r = std::min (cnt - 1, (int) (rnd01() * (float) cnt));
            if (hs && r == 0) src = 0;
            else { if (hs) --r; if (tt && r == 0) src = 1; else { if (tt) --r; if (dd && r == 0) src = 2; else { if (dd) --r; src = 3 + r; } } }
        }
        if (src == 0) { pos = spos >= 0 ? spos + stepScan : basePos; vel = sv; szm = ssz; seqNote = sn; explicitPitch = ktr; }
        else
        {
            pos = basePos;
            if (src == 1) explicitPitch = p.tpitch;
            else if (src == 2) explicitPitch = p.dpitch;
            else { const int k = src - 3; if (k >= numHeld) return; explicitPitch = (float) (held[(size_t) k].note - 60) + bend; vel = held[(size_t) k].vel; }
        }
        if (follow) { explicitPitch = 0; seqNote = 0; }

        Grain* g = nullptr; Grain* oldest = nullptr; double oa = -1;
        for (auto& x : G) { if (! x.on) { g = &x; break; } const double a = x.age / x.dur; if (a > oa) { oa = a; oldest = &x; } }
        if (! g) g = oldest;

        pos += m[TPos] * 0.5;
        const float sp = clamp01 (p.spray + m[TSpray]);
        pos += rnd11() * sp * 0.5;
        float size = p.size * std::exp2 (m[TSize] * 3) * szm * (1 + rnd11() * p.sizeJ * 0.9f);
        float semis = explicitPitch;
        const float ps = std::max (0.f, p.pspray + m[TPSpray] * 12);
        if (p.pitchOn)
        {
            semis += p.pitch + p.fine / 100 + m[TPitch] * 12;
            if (p.pseq) semis += seqNote;
            if (ps > 0)
            {
                float q = rnd11() * ps;
                if (p.pmode == POct) q = std::round (q / 12) * 12;
                else if (p.pmode == PFifth)
                {
                    static const int F[] = { -24, -12, -5, 0, 7, 12, 19, 24 }; int c[8], nc = 0;
                    for (int f : F) if (std::abs ((float) f) <= ps + 0.01f) c[nc++] = f;
                    q = nc ? (float) c[std::min (nc - 1, (int) (rnd01() * (float) nc))] : 0;
                }
                else if (p.pmode == PScale)
                {
                    static const int S[] = { 0, 3, 5, 7, 10, 12 };
                    const float o = std::floor (q / 12), w = q - o * 12; int best = 0; float bd = 99;
                    for (int x : S) { const float d = std::abs (w - (float) x); if (d < bd) { bd = d; best = x; } }
                    q = o * 12 + (float) best;
                }
                else if (p.pmode == PSemi) q = std::round (q);
                semis += q;
            }
        }
        bool reverse = rnd01() < p.rev;
        if (p.glitch > 0 && rnd01() < p.glitch * 0.6f)
        {
            switch ((int) (rnd01() * 4))
            {
                case 0: pos = rnd01(); break;
                case 1: size *= 0.12f + rnd01() * 0.2f; if (p.pitchOn) semis += 12; stut = 2 + (int) (rnd01() * 6); break;
                case 2: reverse = ! reverse; if (p.pitchOn) semis -= 12; break;
                default: pos = lastPos; size *= 0.5f; stut = 3 + (int) (rnd01() * 8); break;
            }
        }
        if (p.shuffle >= 2) { const double ns = std::round (p.shuffle); pos = std::floor (wrapD (pos) * ns) / ns; }
        pos = wrapD (pos); lastPos = (float) pos;
        size = std::max (0.0015f, std::min ({ 3.f, size, 240.f / std::max (1.f, rateHint > 0 ? rateHint : dens) }));

        g->rate = std::exp2 (semis / 12.0) * sample->sampleRate / sr * (reverse ? -1 : 1);
        g->pos = pos * (double) sample->length();
        g->dur = std::max (12.0, (double) size * sr);
        g->age = std::min (0.999, std::max (0.0, (double) off));
        if (reverse) g->pos += g->dur * std::abs (g->rate);
        const float w = clamp01 (p.width + m[TWidth]), pan = rnd11() * w;
        g->pl = pan > 0 ? 1 - pan : 1; g->pr = pan < 0 ? 1 + pan : 1;
        const float ov = std::min (256.f, (rateHint > 0 ? rateHint : dens) * size);
        const float coh = (1 - p.chaos) * (1 - std::min (1.f, sp * 10)) * ((ps > 0.05f && p.pitchOn) ? 0.4f : 1.f);
        g->amp = vel / (coh * std::max (1.f, ov * mW) + (1 - coh) * std::max (1.f, std::sqrt (ov * mW2)));
        g->on = true;
    }

    float sr = 48000;
    const Sample* sample = nullptr;
    Params base, P;
    std::array<Grain, kGrains> G {};
    std::array<float, 1026> win {};
    float wA = -1, wR = -1, wC = -1, mW = 0.5f, mW2 = 0.375f;
    std::array<Held, 32> held {}; int numHeld = 0;
    std::array<Acc, 3> sacc {};
    bool test = false, drone = false;
    float ktr = 0, bend = 0;
    double scan = 0, stepScan = 0; float gE = 0, acc = 0, next = 1, dens = 20, lastPos = 0; int stut = 0;
    double sg = 0; float sn = 0, sv = 1, spos = -1, ssz = 1; int curStep = -1; Step lastStepData;
    Override ovA, ovB;
    float l1 = 0, l2 = 0, h1 = 0, h2 = 0; std::array<float, NumTargets> m {}; unsigned cc = 0;
    float fe = 0, fvel = 1, kd = 0.999f, kr = 0.999f; int fst = 0;
    bool fOn = false; float svf[4] = { 0, 0, 0, 0 }, fk = 2, fa1 = 1, fa2 = 0, fa3 = 0;
    float holdL = 0, holdR = 0; int holdC = 0;
    std::vector<float> mbL, mbR; int mw = 0; float fxph = 0, ph2 = 0; double apL[12] {}, apR[12] {}; float afL = 0, afR = 0; double aL = 0, aR = 0;
    std::vector<float> dL, dR; int dw = 0; float dlp[2] = { 0, 0 };
    std::array<std::array<Comb, 4>, 2> combs; std::array<std::array<AP, 2>, 2> aps;
    float rh1 = 0, rh2 = 0, rx1 = 0, rx2 = 0, lim = 0;
    uint32_t rng = 0x9E3779B9u;
};
} // namespace gs
