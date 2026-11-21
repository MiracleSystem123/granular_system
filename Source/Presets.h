// GRANULAR_SYSTEM presets (same as the web version). Choice values are indices into Params.h lists.
#pragma once
#include <vector>
#include <utility>

namespace gs
{
struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        { "NATURAL", { {"width",0} } },
        { "TIME_STRETCH 0.5x", { {"width",0},{"scan",.5f},{"size",.12f},{"dens",16.7f} } },
        { "TIME_STRETCH 0.25x", { {"width",0},{"scan",.25f},{"size",.16f},{"dens",12.5f},{"rmix",.25f} } },
        { "PAD_ATMOSPHERE", { {"rdecay",.8f},{"rhp",120},{"dmode",1},{"vol",1},{"scan",.04f},{"size",.45f},{"sizeJ",.4f},{"dens",28},{"chaos",.8f},{"spray",.18f},{"gcurve",.6f},
                              {"pspray",12},{"pmode",2},{"width",1},{"fxType",4},{"fxMix",.6f},{"fxDepth",.6f},{"fxRate",.3f},{"cut",4500},{"res",.2f},{"rmix",.55f},{"rsize",.92f},
                              {"rdamp",.6f},{"dmix",.2f},{"dsync",5},{"dfb",.5f},{"att",.8f},{"rel",2.5f},{"l1t",1},{"l1d",.08f},{"l1r",.05f} } },
        { "GLASS_SHIMMER", { {"rdecay",.95f},{"rhp",300},{"vol",1},{"scan",0},{"size",.3f},{"dens",50},{"spray",.04f},{"chaos",.6f},{"pspray",12},{"pmode",2},{"fxType",1},{"fxMix",.5f},
                             {"rmix",.6f},{"rsize",.9f},{"att",.3f},{"rel",1.5f},{"width",1} } },
        { "CHIRP_SEQUENCE", { {"ftype",0},{"cut",160},{"res",.86f},{"drive",.55f},{"fenv",.75f},{"fatk",.001f},{"fdec",.12f},{"fsus",0},{"frel",.08f},{"size",.08f},{"dens",25} } },
        { "ACID_DRIVE", { {"ftype",0},{"cut",260},{"res",.93f},{"drive",.8f},{"fenv",.6f},{"fatk",.001f},{"fdec",.18f},{"fsus",.1f},{"frel",.1f},{"l1t",7},{"l1d",.15f},{"l1r",.3f} } },
        { "AUDIO_RATE_BUZZ", { {"dens",110},{"size",.012f},{"chaos",0},{"scan",0},{"gcurve",.25f},{"ftype",0},{"cut",2500},{"res",.4f},{"drive",.3f},{"att",.01f} } },
        { "PULSAR_KEYS", { {"vol",1},{"dfollow",1},{"dens",261.6f},{"size",.012f},{"chaos",0},{"scan",.05f},{"ftype",0},{"cut",5000},{"res",.3f},{"att",.005f},{"rel",.2f} } },
        { "FORMANT_SWEEP", { {"dfollow",1},{"dens",130.8f},{"size",.02f},{"chaos",0},{"scan",.3f},{"ftype",0},{"cut",2500},{"res",.5f},{"fenv",.3f},{"fdec",.3f},{"l1t",3},{"l1d",.3f},{"l1r",.2f} } },
        { "RAUCOUS_NOISE", { {"dens",1600},{"size",.004f},{"sizeJ",.9f},{"chaos",1},{"spray",.6f},{"scan",-8},{"glitch",.5f},{"rev",.4f},{"drive",.85f},{"res",.7f},{"cut",2500},{"ftype",1},
                             {"crush",.3f},{"fxType",2},{"fxMix",.5f},{"fxFb",.8f},{"pspray",24},{"width",1} } },
        { "JET_FLANGE", { {"fxType",2},{"fxRate",.12f},{"fxDepth",.9f},{"fxFb",.85f},{"fxMix",.6f},{"scan",.5f},{"size",.12f},{"dens",16.7f} } },
        { "PHASE_DRIFT", { {"fxType",3},{"fxRate",.18f},{"fxDepth",.8f},{"fxFb",.6f},{"fxMix",.8f},{"size",.2f},{"dens",12},{"chaos",.3f},{"rmix",.3f} } },
        { "STUTTER", { {"size",.06f},{"dens",16},{"gatk",.05f},{"grel",.05f},{"scan",.25f},{"shuffle",16},{"glitch",.15f},{"rmix",.1f},{"dmix",.25f},{"dsync",9},{"dfb",.5f} } },
        { "REVERSE_RAIN", { {"rev",1},{"size",.25f},{"dens",24},{"spray",.6f},{"gatk",.9f},{"grel",.1f},{"chaos",.7f},{"pspray",7},{"pmode",3},{"rmix",.45f},{"width",1},{"rel",.4f} } },
        { "GLITCH_STORM", { {"glitch",.6f},{"size",.04f},{"sizeJ",.9f},{"dens",45},{"chaos",.9f},{"spray",.5f},{"shuffle",24},{"crush",.35f},{"drive",.3f},{"rev",.3f},{"pspray",12},{"pmode",2},{"rmix",.2f} } },
        { "UNDERWATER", { {"size",.4f},{"dens",20},{"spray",.3f},{"chaos",.8f},{"pitch",-12},{"pspray",3},{"cut",900},{"res",.5f},{"l1t",7},{"l1d",.4f},{"l1r",.15f},{"l2t",5},{"l2d",.04f},{"l2r",4},
                          {"rmix",.7f},{"rsize",.95f},{"width",1},{"rel",.5f} } },
    };
    return list;
}

// Window shapes: attack, release, curve
struct WindowShape { const char* name; float a, r, c; };
inline const WindowShape windowShapes[] = { {"HANN",.5f,.5f,.5f}, {"GAUSS",.5f,.5f,.2f}, {"TRI",.5f,.5f,.25f}, {"TUKEY",.2f,.2f,.5f},
                                            {"PERC",.005f,.995f,.1f}, {"SWELL",.995f,.005f,.1f}, {"RECT",.01f,.01f,.25f}, {"FAT",.5f,.5f,1} };
} // namespace gs
