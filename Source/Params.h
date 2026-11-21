// GRANULAR_SYSTEM — parameter definitions shared by the engine, the plugin and the offline tests.
#pragma once
#include <cmath>
#include <cstring>
#include <initializer_list>

namespace gs
{
enum PitchMode { PFree, PSemi, POct, PFifth, PScale };
enum FilterType { FLP, FBP, FHP };
enum FxType { FxOff, FxChorus, FxFlanger, FxPhaser, FxEnsemble };
enum DelayMode { DStereo, DPingPong };
enum LfoShape { LSine, LTri, LSquare, LSaw, LRandom };
enum LfoTarget { TNone, TPos, TSpray, TSize, TDens, TPitch, TPSpray, TCut, TRes, TDrive, TWidth, TFxMix, TFxDepth, TDMix, TRMix, NumTargets };
enum KeyMode { KPlay, KTranspose };

struct Params
{
    float pos = 0, spray = 0, size = 0.1f, sizeJ = 0, dens = 20, chaos = 0, gatk = 0.5f, grel = 0.5f, gcurve = 0.5f;
    float scan = 1, rev = 0, shuffle = 0, glitch = 0, width = 0.4f;
    float pitch = 0, fine = 0, pspray = 0, tpitch = 0, dpitch = 0;
    float cut = 18000, res = 0.1f, drive = 0, crush = 0, fenv = 0, fatk = 0.002f, fdec = 0.25f, fsus = 0.5f, frel = 0.2f;
    float fxRate = 0.4f, fxDepth = 0.5f, fxFb = 0.3f, fxMix = 0.5f;
    float dtime = 0.3f, dfb = 0.4f, dtone = 0.7f, dmix = 0;
    float rsize = 0.6f, rdecay = 0.45f, rdamp = 0.4f, rhp = 20, rmix = 0.15f;
    float l1r = 0.2f, l1d = 0, l2r = 3, l2d = 0;
    float att = 0.003f, rel = 0.06f, vol = 0.75f;
    float bpm = 100, gate = 1, swing = 0, slen = 16;
    int pmode = PFree, ftype = FLP, fxType = FxOff, dmode = DStereo, dsync = 0, l1s = LSine, l1t = TNone, l2s = LTri, l2t = TNone;
    int div = 8, assignA = 1, assignB = 4, kmode = KPlay, pitchOn = 1, pseq = 1, dfollow = 0;
};

// ---- value mappings ----
inline float scanFrom01 (float u)
{
    const float x = u * 2.0f - 1.0f, ax = std::abs (x);
    if (ax < 0.03f) return 0.0f;
    float v = 0.01f * std::pow (800.0f, (ax - 0.03f) / 0.97f);
    for (float s : { 0.25f, 0.5f, 1.0f, 2.0f }) if (std::abs (v - s) / s < 0.04f) v = s;
    return x < 0 ? -v : v;
}
inline float scanTo01 (float v)
{
    if (v == 0.0f) return 0.5f;
    const float ax = 0.03f + 0.97f * std::log (std::abs (v) / 0.01f) / std::log (800.0f);
    return ((v < 0 ? -1.0f : 1.0f) * std::fmin (1.0f, ax) + 1.0f) * 0.5f;
}

enum class Fmt { Pct, Sec, PerSec, Semi, Cents, Scan, Hz, Bpm, Oct, Ratio, Slices, Curve, Decay, HzOff, CutOpen, Plain };
enum class Map { Lin, Log, Scan };

struct FloatDef { const char* id; const char* name; float min, max, def, step; Map map; Fmt fmt; float Params::* field; };

inline const FloatDef floatDefs[] = {
    { "pos",     "Position",      0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::pos },
    { "spray",   "Spray",         0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::spray },
    { "size",    "Size",          0.0015f, 1.5f, 0.1f, 0, Map::Log, Fmt::Sec, &Params::size },
    { "sizeJ",   "Size Jitter",   0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::sizeJ },
    { "dens",    "Density",       0.5f, 4000, 20, 0, Map::Log, Fmt::PerSec, &Params::dens },
    { "chaos",   "Chaos",         0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::chaos },
    { "gatk",    "Window Attack", 0, 1, 0.5f, 0, Map::Lin, Fmt::Pct, &Params::gatk },
    { "grel",    "Window Release",0, 1, 0.5f, 0, Map::Lin, Fmt::Pct, &Params::grel },
    { "gcurve",  "Window Curve",  0, 1, 0.5f, 0, Map::Lin, Fmt::Curve, &Params::gcurve },
    { "scan",    "Scan",         -8, 8, 1, 0, Map::Scan, Fmt::Scan, &Params::scan },
    { "rev",     "Reverse",       0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::rev },
    { "shuffle", "Slices",        0, 32, 0, 1, Map::Lin, Fmt::Slices, &Params::shuffle },
    { "glitch",  "Glitch",        0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::glitch },
    { "width",   "Width",         0, 1, 0.4f, 0, Map::Lin, Fmt::Pct, &Params::width },
    { "pitch",   "Transpose",   -24, 24, 0, 1, Map::Lin, Fmt::Semi, &Params::pitch },
    { "fine",    "Fine",       -100, 100, 0, 1, Map::Lin, Fmt::Cents, &Params::fine },
    { "pspray",  "Random Pitch",  0, 24, 0, 0, Map::Lin, Fmt::Semi, &Params::pspray },
    { "tpitch",  "Test Pitch",  -24, 24, 0, 1, Map::Lin, Fmt::Semi, &Params::tpitch },
    { "dpitch",  "Drone Pitch", -24, 24, 0, 1, Map::Lin, Fmt::Semi, &Params::dpitch },
    { "cut",     "Cutoff",       30, 18000, 18000, 0, Map::Log, Fmt::CutOpen, &Params::cut },
    { "res",     "Resonance",     0, 1, 0.1f, 0, Map::Lin, Fmt::Pct, &Params::res },
    { "drive",   "Drive",         0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::drive },
    { "crush",   "Crush",         0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::crush },
    { "fenv",    "Env Amount",   -1, 1, 0, 0, Map::Lin, Fmt::Oct, &Params::fenv },
    { "fatk",    "Env Attack",    0.0005f, 4, 0.002f, 0, Map::Log, Fmt::Sec, &Params::fatk },
    { "fdec",    "Env Decay",     0.005f, 6, 0.25f, 0, Map::Log, Fmt::Sec, &Params::fdec },
    { "fsus",    "Env Sustain",   0, 1, 0.5f, 0, Map::Lin, Fmt::Pct, &Params::fsus },
    { "frel",    "Env Release",   0.005f, 8, 0.2f, 0, Map::Log, Fmt::Sec, &Params::frel },
    { "fxRate",  "FX Rate",       0.02f, 12, 0.4f, 0, Map::Log, Fmt::Hz, &Params::fxRate },
    { "fxDepth", "FX Depth",      0, 1, 0.5f, 0, Map::Lin, Fmt::Pct, &Params::fxDepth },
    { "fxFb",    "FX Feedback",   0, 1, 0.3f, 0, Map::Lin, Fmt::Pct, &Params::fxFb },
    { "fxMix",   "FX Mix",        0, 1, 0.5f, 0, Map::Lin, Fmt::Pct, &Params::fxMix },
    { "dtime",   "Delay Time",    0.01f, 2, 0.3f, 0, Map::Log, Fmt::Sec, &Params::dtime },
    { "dfb",     "Delay Feedback",0, 0.95f, 0.4f, 0, Map::Lin, Fmt::Pct, &Params::dfb },
    { "dtone",   "Delay Tone",    0, 1, 0.7f, 0, Map::Lin, Fmt::Ratio, &Params::dtone },
    { "dmix",    "Delay Mix",     0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::dmix },
    { "rsize",   "Reverb Size",   0, 1, 0.6f, 0, Map::Lin, Fmt::Pct, &Params::rsize },
    { "rdecay",  "Reverb Decay",  0, 1, 0.45f, 0, Map::Lin, Fmt::Decay, &Params::rdecay },
    { "rdamp",   "Reverb Damping",0, 1, 0.4f, 0, Map::Lin, Fmt::Pct, &Params::rdamp },
    { "rhp",     "Reverb High-pass", 20, 2000, 20, 0, Map::Log, Fmt::HzOff, &Params::rhp },
    { "rmix",    "Reverb Mix",    0, 1, 0.15f, 0, Map::Lin, Fmt::Pct, &Params::rmix },
    { "l1r",     "LFO1 Rate",     0.02f, 30, 0.2f, 0, Map::Log, Fmt::Hz, &Params::l1r },
    { "l1d",     "LFO1 Depth",    0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::l1d },
    { "l2r",     "LFO2 Rate",     0.02f, 30, 3, 0, Map::Log, Fmt::Hz, &Params::l2r },
    { "l2d",     "LFO2 Depth",    0, 1, 0, 0, Map::Lin, Fmt::Pct, &Params::l2d },
    { "att",     "Attack",        0.001f, 4, 0.003f, 0, Map::Log, Fmt::Sec, &Params::att },
    { "rel",     "Release",       0.01f, 8, 0.06f, 0, Map::Log, Fmt::Sec, &Params::rel },
    { "vol",     "Volume",        0, 1, 0.75f, 0, Map::Lin, Fmt::Pct, &Params::vol },
    { "bpm",     "Tempo (no host)", 40, 240, 100, 1, Map::Lin, Fmt::Bpm, &Params::bpm },
    { "gate",    "Gate",          0.1f, 4, 1, 0, Map::Log, Fmt::Pct, &Params::gate },
    { "swing",   "Swing",         0, 0.4f, 0, 0, Map::Lin, Fmt::Plain, &Params::swing },
    { "slen",    "Steps",         1, 16, 16, 1, Map::Lin, Fmt::Plain, &Params::slen },
};
constexpr int numFloatDefs = (int) (sizeof (floatDefs) / sizeof (floatDefs[0]));

inline const char* const kPModes[]  = { "FREE", "SEMI", "OCT", "5TH", "SCALE" };
inline const char* const kFTypes[]  = { "LP", "BP", "HP" };
inline const char* const kFx[]      = { "OFF", "CHORUS", "FLANGER", "PHASER", "ENSEMBLE" };
inline const char* const kDModes[]  = { "STEREO", "PING-PONG" };
inline const char* const kDivs[]    = { "1/1", "1/2", "1/4", "1/4T", "3/8", "1/8", "1/8T", "3/16", "1/16", "1/16T", "1/32", "1/64" };
inline const double      kDivVals[] = { 1.0, 0.5, 0.25, 1.0 / 6, 0.375, 0.125, 1.0 / 12, 0.1875, 0.0625, 1.0 / 24, 1.0 / 32, 1.0 / 64 };
inline const char* const kDSync[]   = { "FREE", "1/1", "1/2", "1/4", "1/4T", "3/8", "1/8", "1/8T", "3/16", "1/16", "1/16T", "1/32", "1/64" };
inline const char* const kShapes[]  = { "SINE", "TRIANGLE", "SQUARE", "SAW", "RANDOM S&H" };
inline const char* const kTargets[] = { "NONE", "Position", "Spray", "Size", "Density", "Pitch", "Random pitch", "Cutoff", "Resonance", "Drive", "Width", "Mod FX mix", "Mod FX depth", "Delay mix", "Reverb mix" };
inline const char* const kOnOff[]   = { "OFF", "ON" };
inline const char* const kKModes[]  = { "PLAY", "TRANSPOSE SEQ" };

// Parameters the two assign lanes can drive (index 0 = none).
inline const char* const kAssignIds[] = { "none", "rmix", "rsize", "rdecay", "rhp", "rdamp", "dmix", "dfb", "dtime", "dtone", "fxMix", "fxDepth", "fxRate", "fxFb",
                                          "cut", "res", "drive", "fenv", "crush", "spray", "size", "dens", "chaos", "glitch", "rev", "scan", "pspray", "pitch",
                                          "gatk", "grel", "gcurve", "sizeJ", "shuffle", "width", "vol" };
inline const char* const kAssignNames[] = { "NONE", "REVERB MIX", "REVERB SIZE", "REVERB DECAY", "REVERB HIGH-PASS", "REVERB DAMPING", "DELAY MIX", "DELAY FEEDBACK", "DELAY TIME", "DELAY TONE",
                                            "MOD FX MIX", "MOD FX DEPTH", "MOD FX RATE", "MOD FX FEEDBACK", "FILTER CUTOFF", "FILTER RESONANCE", "DRIVE", "FILTER ENV AMOUNT", "CRUSH",
                                            "SPRAY", "GRAIN SIZE", "DENSITY", "CHAOS", "GLITCH", "REVERSE", "SCAN", "RANDOM PITCH", "TRANSPOSE",
                                            "WINDOW ATTACK", "WINDOW RELEASE", "WINDOW CURVE", "SIZE JITTER", "SLICES", "WIDTH", "VOLUME" };
constexpr int numAssign = (int) (sizeof (kAssignIds) / sizeof (kAssignIds[0]));

struct ChoiceDef { const char* id; const char* name; const char* const* items; int count; int def; int Params::* field; };
inline const ChoiceDef choiceDefs[] = {
    { "pmode",   "Random Mode",  kPModes, 5, 0, &Params::pmode },
    { "ftype",   "Filter Mode",  kFTypes, 3, 0, &Params::ftype },
    { "fxType",  "Mod FX Type",  kFx, 5, 0, &Params::fxType },
    { "dmode",   "Delay Mode",   kDModes, 2, 0, &Params::dmode },
    { "dsync",   "Delay Sync",   kDSync, 13, 0, &Params::dsync },
    { "l1s",     "LFO1 Shape",   kShapes, 5, 0, &Params::l1s },
    { "l1t",     "LFO1 Target",  kTargets, NumTargets, 0, &Params::l1t },
    { "l2s",     "LFO2 Shape",   kShapes, 5, 1, &Params::l2s },
    { "l2t",     "LFO2 Target",  kTargets, NumTargets, 0, &Params::l2t },
    { "div",     "Interval",     kDivs, 12, 8, &Params::div },
    { "assignA", "Assign A",     kAssignNames, numAssign, 1, &Params::assignA },
    { "assignB", "Assign B",     kAssignNames, numAssign, 6, &Params::assignB },
    { "kmode",   "Keys Mode",    kKModes, 2, 0, &Params::kmode },
    { "pitchOn", "Pitch",        kOnOff, 2, 1, &Params::pitchOn },
    { "pseq",    "Seq Notes",    kOnOff, 2, 1, &Params::pseq },
    { "dfollow", "Dens->Pitch",  kOnOff, 2, 0, &Params::dfollow },
};
constexpr int numChoiceDefs = (int) (sizeof (choiceDefs) / sizeof (choiceDefs[0]));

inline const FloatDef* findFloat (const char* id)
{
    for (const auto& d : floatDefs) if (std::strcmp (d.id, id) == 0) return &d;
    return nullptr;
}

// 0..1 lane value -> parameter value, matching each parameter's own scale.
inline float fromNorm (const FloatDef& d, float u)
{
    float v;
    if (d.map == Map::Scan) v = scanFrom01 (u);
    else if (d.map == Map::Log) v = d.min * std::pow (d.max / d.min, u);
    else v = d.min + u * (d.max - d.min);
    if (d.step > 0) v = std::round (v / d.step) * d.step;
    return v;
}
inline float toNorm (const FloatDef& d, float v)
{
    float u;
    if (d.map == Map::Scan) u = scanTo01 (v);
    else if (d.map == Map::Log) u = std::log (v / d.min) / std::log (d.max / d.min);
    else u = (v - d.min) / (d.max - d.min);
    return std::fmin (1.0f, std::fmax (0.0f, u));
}

inline float reverbFeedback (float rdecay) { return 0.7f + 0.2995f * (1.0f - std::pow (1.0f - rdecay, 3.0f)); }

inline void setDefaults (Params& p)
{
    for (const auto& d : floatDefs) p.*(d.field) = d.def;
    for (const auto& d : choiceDefs) p.*(d.field) = d.def;
}
} // namespace gs
