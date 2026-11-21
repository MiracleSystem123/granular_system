#pragma once
#include <juce_core/juce_core.h>
#include "Params.h"

namespace gs
{
inline juce::String fmtValue (const FloatDef& d, float v)
{
    auto sec = [] (float s) { return s < 1 ? (s < 0.01f ? juce::String (s * 1000, 1) : juce::String (juce::roundToInt (s * 1000))) + "ms" : juce::String (s, 2) + "s"; };
    auto hz  = [] (float f) { return f < 1000 ? juce::String (juce::roundToInt (f)) + "Hz" : juce::String (f / 1000, 1) + "k"; };
    switch (d.fmt)
    {
        case Fmt::Pct:     return juce::String (juce::roundToInt (v * 100)) + "%";
        case Fmt::Sec:     return sec (v);
        case Fmt::PerSec:  return (v < 10 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + "/s";
        case Fmt::Semi:    return (v > 0 ? "+" : "") + (d.step > 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, 1)) + "st";
        case Fmt::Cents:   return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + "ct";
        case Fmt::Scan:    return std::abs (v) < 0.001f ? juce::String ("FREEZE") : (v > 0 ? "+" : "-") + juce::String (std::abs (v), 2) + "x";
        case Fmt::Hz:      return juce::String (v, v < 10 ? 2 : 1) + "Hz";
        case Fmt::Bpm:     return juce::String (juce::roundToInt (v)) + " BPM";
        case Fmt::Oct:     return (v > 0 ? "+" : "") + juce::String (v * 6, 1) + "oct";
        case Fmt::Ratio:   return hz (300 * std::pow (60.f, v));
        case Fmt::Slices:  return v < 2 ? juce::String ("OFF") : juce::String (juce::roundToInt (v));
        case Fmt::Curve:   return v < 0.12f ? "EXP" : v < 0.38f ? "LINEAR" : v < 0.62f ? "HANN" : v < 0.88f ? "SINE" : "FAT";
        case Fmt::Decay:   { const float fb = reverbFeedback (v); if (fb > 0.9993f) return "INF"; const float t = -3.f / std::log10 (fb) * 0.03f * 1.14f; return t < 10 ? juce::String (t, 1) + "s" : juce::String (juce::roundToInt (t)) + "s"; }
        case Fmt::HzOff:   return v < 21 ? juce::String ("OFF") : hz (v);
        case Fmt::CutOpen: return v >= 17900 ? juce::String ("OPEN") : hz (v);
        default:           return d.step > 0 ? juce::String (juce::roundToInt (v)) : juce::String (juce::roundToInt (v / 0.4f * 100)) + "%";
    }
}

} // namespace gs
