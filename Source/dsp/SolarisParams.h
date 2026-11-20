#pragma once

// SolarisOnDaSynth — parametres, presets et utilitaires (sans dependance a JUCE)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace solaris
{
constexpr double kTwoPi = 6.283185307179586;

template <typename T>
inline T clampv (T v, T lo, T hi) noexcept { return std::min (hi, std::max (lo, v)); }

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g, float floorDb = -100.0f) noexcept
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

//==============================================================================
// Tous les reglages du plugin (les booleens et choix sont stockes en float)
struct Settings
{
    float inGain = 0, outGain = 0, autoGain = 0, bypass = 0, anaLength = 0;
    float hpfOn = 0, hpfFreq = 0;
    float eqOn = 0, eqMode = 0, lowFreq = 0, lowGain = 0, midFreq = 0, midGain = 0, midQ = 0,
          highFreq = 0, highGain = 0, eqColour = 0;
    float satOn = 0, satType = 0, satDrive = 0, satMix = 0, satBright = 0, satBrightFreq = 0;
    float compOn = 0, compSource = 0, compThresh = 0, compRatio = 0, compAttack = 0,
          compRelease = 0, compMakeup = 0, compScHpf = 0, compMix = 0;
    float widthOn = 0, width = 0, monoFreq = 0;
    float delayOn = 0, delaySync = 0, delayFb = 0, delayMix = 0, delayPing = 0;
    float revOn = 0, revMode = 0, revDecaySync = 0, revDecay = 0, revDecayNote = 0,
          revPreSync = 0, revPredelay = 0, revPreNote = 0, revLowCut = 0, revHighCut = 0,
          revMod = 0, revMix = 0;
};

enum class Kind { Float, Bool, Choice };
enum class Unit { None, Db, Hz, Pct, Ms, Sec, Ratio };

struct ParamDef
{
    const char* id;
    const char* name;
    Kind kind;
    float minV, maxV, defV, centre;   // centre > 0 : echelle "logarithmique" centree
    Unit unit;
    std::vector<std::string> choices;
    float Settings::* field;
};

inline const std::vector<ParamDef>& paramDefs()
{
    using S = Settings;
    auto F = [] (const char* id, const char* name, float lo, float hi, float def, float centre, Unit u, float S::* f)
    { return ParamDef { id, name, Kind::Float, lo, hi, def, centre, u, {}, f }; };
    auto B = [] (const char* id, const char* name, bool def, float S::* f)
    { return ParamDef { id, name, Kind::Bool, 0.0f, 1.0f, def ? 1.0f : 0.0f, 0.0f, Unit::None, {}, f }; };
    auto C = [] (const char* id, const char* name, std::vector<std::string> ch, int def, float S::* f)
    { const float mx = (float) ch.size() - 1.0f; return ParamDef { id, name, Kind::Choice, 0.0f, mx, (float) def, 0.0f, Unit::None, std::move (ch), f }; };

    static const std::vector<ParamDef> defs = {
        F ("inGain",  "Gain entree", -24, 24, 0, 0, Unit::Db, &S::inGain),
        F ("outGain", "Gain sortie", -24, 24, 0, 0, Unit::Db, &S::outGain),
        B ("autoGain", "Auto gain", true, &S::autoGain),
        B ("bypass",   "Bypass", false, &S::bypass),
        C ("anaLength", "Duree analyse", { "6 s", "12 s", "24 s" }, 0, &S::anaLength),

        B ("hpfOn",   "Coupe-bas ON", true, &S::hpfOn),
        F ("hpfFreq", "Coupe-bas", 20, 1000, 20, 150, Unit::Hz, &S::hpfFreq),

        B ("eqOn",     "EQ ON", true, &S::eqOn),
        C ("eqMode",   "EQ mode", { "Stereo", "Mid", "Side" }, 0, &S::eqMode),
        F ("lowFreq",  "Grave freq", 30, 500, 100, 120, Unit::Hz, &S::lowFreq),
        F ("lowGain",  "Grave gain", -12, 12, 0, 0, Unit::Db, &S::lowGain),
        F ("midFreq",  "Medium freq", 150, 8000, 800, 1000, Unit::Hz, &S::midFreq),
        F ("midGain",  "Medium gain", -12, 12, 0, 0, Unit::Db, &S::midGain),
        F ("midQ",     "Medium Q", 0.3f, 6, 1, 1.2f, Unit::None, &S::midQ),
        F ("highFreq", "Aigu freq", 3000, 16000, 10000, 8000, Unit::Hz, &S::highFreq),
        F ("highGain", "Aigu gain", -12, 12, 0, 0, Unit::Db, &S::highGain),
        F ("eqColour", "EQ couleur", 0, 100, 50, 0, Unit::Pct, &S::eqColour),

        B ("satOn",         "Satu ON", false, &S::satOn),
        C ("satType",       "Satu type", { "Tape", "Tube" }, 1, &S::satType),
        F ("satDrive",      "Drive", 0, 24, 6, 0, Unit::Db, &S::satDrive),
        F ("satMix",        "Chaleur", 0, 100, 40, 0, Unit::Pct, &S::satMix),
        F ("satBright",     "Brillance", 0, 100, 30, 0, Unit::Pct, &S::satBright),
        F ("satBrightFreq", "Brillance freq", 2000, 10000, 5000, 5000, Unit::Hz, &S::satBrightFreq),

        B ("compOn",      "Comp ON", false, &S::compOn),
        C ("compSource",  "Comp source", { "Synthe / Melo", "Groupe" }, 0, &S::compSource),
        F ("compThresh",  "Seuil", -48, 0, -18, 0, Unit::Db, &S::compThresh),
        F ("compRatio",   "Ratio", 1, 20, 2.5f, 4, Unit::Ratio, &S::compRatio),
        F ("compAttack",  "Attaque", 0.1f, 100, 20, 10, Unit::Ms, &S::compAttack),
        F ("compRelease", "Release", 10, 1000, 120, 150, Unit::Ms, &S::compRelease),
        F ("compMakeup",  "Makeup", 0, 24, 0, 0, Unit::Db, &S::compMakeup),
        F ("compScHpf",   "Clarte (SC HPF)", 20, 500, 100, 120, Unit::Hz, &S::compScHpf),
        F ("compMix",     "Comp mix", 0, 100, 100, 0, Unit::Pct, &S::compMix),

        B ("widthOn",  "Stereo ON", false, &S::widthOn),
        F ("width",    "Largeur", 0, 200, 100, 0, Unit::Pct, &S::width),
        F ("monoFreq", "Basses mono", 20, 500, 150, 150, Unit::Hz, &S::monoFreq),

        B ("delayOn",   "Delay ON", false, &S::delayOn),
        C ("delaySync", "Delay note", { "1/2", "1/4", "1/8 D", "1/8", "1/8 T", "1/16" }, 2, &S::delaySync),
        F ("delayFb",   "Feedback", 0, 90, 30, 0, Unit::Pct, &S::delayFb),
        F ("delayMix",  "Delay niveau", 0, 100, 20, 0, Unit::Pct, &S::delayMix),
        B ("delayPing", "Ping-pong", true, &S::delayPing),

        B ("revOn",        "Reverb ON", false, &S::revOn),
        C ("revMode",      "Reverb mode", { "Vintage Hall", "Plate", "Room", "Chamber", "Cosmos" }, 0, &S::revMode),
        B ("revDecaySync", "Decay sync BPM", true, &S::revDecaySync),
        F ("revDecay",     "Decay", 0.2f, 20, 2.5f, 3, Unit::Sec, &S::revDecay),
        C ("revDecayNote", "Decay note", { "Tight 1/4", "Small Room 1/2", "Big Room 1 mes", "Hall 2 mes", "Cosmos 4 mes" }, 2, &S::revDecayNote),
        B ("revPreSync",   "Predelay sync BPM", true, &S::revPreSync),
        F ("revPredelay",  "Predelay", 0, 250, 20, 40, Unit::Ms, &S::revPredelay),
        C ("revPreNote",   "Predelay note", { "Auto", "1/512", "1/128", "1/64", "1/32", "1/16" }, 0, &S::revPreNote),
        F ("revLowCut",    "Reverb low cut", 20, 1000, 250, 200, Unit::Hz, &S::revLowCut),
        F ("revHighCut",   "Reverb high cut", 1000, 20000, 8000, 6000, Unit::Hz, &S::revHighCut),
        F ("revMod",       "Modulation", 0, 100, 50, 0, Unit::Pct, &S::revMod),
        F ("revMix",       "Reverb niveau", 0, 100, 15, 0, Unit::Pct, &S::revMix),
    };
    return defs;
}

inline std::string formatValue (Unit u, float v)
{
    char buf[48];
    switch (u)
    {
        case Unit::Db:    std::snprintf (buf, sizeof (buf), "%.1f dB", v); break;
        case Unit::Pct:   std::snprintf (buf, sizeof (buf), "%d %%", (int) std::lround (v)); break;
        case Unit::Ratio: std::snprintf (buf, sizeof (buf), "%.1f:1", v); break;
        case Unit::Sec:   std::snprintf (buf, sizeof (buf), "%.2f s", v); break;
        case Unit::Ms:
            if (v < 10.0f) std::snprintf (buf, sizeof (buf), "%.1f ms", v);
            else           std::snprintf (buf, sizeof (buf), "%d ms", (int) std::lround (v));
            break;
        case Unit::Hz:
            if (v < 1000.0f) std::snprintf (buf, sizeof (buf), "%d Hz", (int) std::lround (v));
            else             std::snprintf (buf, sizeof (buf), "%.1f kHz", v / 1000.0f);
            break;
        case Unit::None:
        default:          std::snprintf (buf, sizeof (buf), "%.2f", v); break;
    }
    return buf;
}

inline const ParamDef* findParam (const char* id)
{
    for (auto& d : paramDefs())
        if (std::strcmp (d.id, id) == 0)
            return &d;
    return nullptr;
}

// Reglages d'usage qui ne font pas partie d'un son (ignores par presets et A/B)
inline bool isGlobalParam (const char* id)
{
    return std::strcmp (id, "bypass") == 0 || std::strcmp (id, "anaLength") == 0;
}

inline float analysisSeconds (const Settings& s)
{
    static const float secs[] = { 6.0f, 12.0f, 24.0f };
    return secs[clampv ((int) std::lround (s.anaLength), 0, 2)];
}

inline Settings defaultSettings()
{
    Settings s;
    for (auto& d : paramDefs())
        s.*(d.field) = d.defV;
    return s;
}

inline bool setById (Settings& s, const char* id, float v)
{
    if (auto* d = findParam (id))
    {
        s.*(d->field) = clampv (v, d->minV, d->maxV);
        return true;
    }
    return false;
}

//==============================================================================
// Reverb : logique du "reverb calculator"
//   noire = 60000 / BPM ms. Decay synchro = duree de la note - predelay.
//   Predelay "Auto" = duree totale / 64 (Tight : / 128), comme le calculateur.
struct ReverbTimes { float decaySec = 2.0f, predelayMs = 20.0f, totalSec = 2.0f; };

inline ReverbTimes computeReverbTimes (const Settings& s, double bpm)
{
    static const float decayBeats[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
    static const float preBeats[]   = { 0.0f, 4.0f / 512.0f, 4.0f / 128.0f, 4.0f / 64.0f, 4.0f / 32.0f, 4.0f / 16.0f };

    const double beat = 60.0 / clampv (bpm, 30.0, 300.0);
    const int decayIdx = clampv ((int) std::lround (s.revDecayNote), 0, 4);
    const bool decaySync = s.revDecaySync > 0.5f;

    const double total = decaySync ? decayBeats[decayIdx] * beat : (double) s.revDecay;

    double pre;
    if (s.revPreSync > 0.5f)
    {
        const int preIdx = clampv ((int) std::lround (s.revPreNote), 0, 5);
        if (preIdx == 0)
            pre = total / ((decaySync && decayIdx == 0) ? 128.0 : 64.0);
        else
            pre = preBeats[preIdx] * beat;
    }
    else
    {
        pre = s.revPredelay / 1000.0;
    }
    pre = clampv (pre, 0.0, 0.5);

    ReverbTimes t;
    t.totalSec   = (float) total;
    t.predelayMs = (float) (pre * 1000.0);
    t.decaySec   = (float) (decaySync ? std::max (0.15, total - pre) : total);
    return t;
}

//==============================================================================
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        { "Melo simple", {
            { "hpfOn", 1 }, { "hpfFreq", 150 },
            { "eqOn", 1 }, { "eqColour", 45 }, { "lowFreq", 200 }, { "lowGain", 1.5f },
            { "midFreq", 400 }, { "midGain", -3 }, { "midQ", 1.2f }, { "highFreq", 10000 }, { "highGain", 2.5f },
            { "satOn", 1 }, { "satType", 0 }, { "satDrive", 6 }, { "satMix", 35 }, { "satBright", 25 }, { "satBrightFreq", 5000 },
            { "compOn", 1 }, { "compSource", 0 }, { "compThresh", -18 }, { "compRatio", 3 }, { "compAttack", 20 },
            { "compRelease", 100 }, { "compScHpf", 120 }, { "compMix", 80 }, { "compMakeup", 3 },
            { "revOn", 1 }, { "revMode", 1 }, { "revDecaySync", 1 }, { "revDecayNote", 1 }, { "revPreSync", 1 },
            { "revPreNote", 0 }, { "revLowCut", 250 }, { "revHighCut", 9000 }, { "revMix", 12 } } },

        { "Groupe de melos", {
            { "hpfOn", 1 }, { "hpfFreq", 120 },
            { "eqOn", 1 }, { "eqColour", 55 }, { "lowFreq", 120 }, { "lowGain", 1 },
            { "midFreq", 350 }, { "midGain", -3 }, { "midQ", 0.9f }, { "highFreq", 12000 }, { "highGain", 3 },
            { "satOn", 1 }, { "satType", 1 }, { "satDrive", 6 }, { "satMix", 40 }, { "satBright", 30 }, { "satBrightFreq", 6000 },
            { "compOn", 1 }, { "compSource", 1 }, { "compThresh", -20 }, { "compRatio", 2 }, { "compAttack", 30 },
            { "compRelease", 200 }, { "compScHpf", 140 }, { "compMix", 70 }, { "compMakeup", 2 },
            { "widthOn", 1 }, { "width", 140 }, { "monoFreq", 150 },
            { "delayOn", 1 }, { "delaySync", 2 }, { "delayFb", 25 }, { "delayMix", 15 }, { "delayPing", 1 },
            { "revOn", 1 }, { "revMode", 0 }, { "revDecaySync", 1 }, { "revDecayNote", 2 }, { "revPreSync", 1 },
            { "revPreNote", 0 }, { "revLowCut", 250 }, { "revHighCut", 7500 }, { "revMod", 55 }, { "revMix", 18 } } },

        { "Space Lead (Mike Dean)", {
            { "hpfOn", 1 }, { "hpfFreq", 110 },
            { "eqOn", 1 }, { "eqColour", 60 }, { "lowFreq", 220 }, { "lowGain", 1.5f },
            { "midFreq", 380 }, { "midGain", -2.5f }, { "midQ", 1.0f }, { "highFreq", 11000 }, { "highGain", 2.5f },
            { "satOn", 1 }, { "satType", 1 }, { "satDrive", 8 }, { "satMix", 45 }, { "satBright", 35 }, { "satBrightFreq", 5500 },
            { "compOn", 1 }, { "compSource", 0 }, { "compThresh", -20 }, { "compRatio", 3 }, { "compAttack", 25 },
            { "compRelease", 150 }, { "compScHpf", 130 }, { "compMix", 75 }, { "compMakeup", 3 },
            { "widthOn", 1 }, { "width", 130 }, { "monoFreq", 140 },
            { "delayOn", 1 }, { "delaySync", 2 }, { "delayFb", 35 }, { "delayMix", 14 }, { "delayPing", 1 },
            { "revOn", 1 }, { "revMode", 0 }, { "revDecaySync", 1 }, { "revDecayNote", 3 }, { "revPreSync", 1 },
            { "revPreNote", 0 }, { "revLowCut", 300 }, { "revHighCut", 7000 }, { "revMod", 70 }, { "revMix", 22 } } },

        { "Reset (neutre)", {} },
    };
    return list;
}

inline Settings presetSettings (int index)
{
    Settings s = defaultSettings();
    const auto& list = presets();
    if (index >= 0 && index < (int) list.size())
        for (auto& kv : list[(size_t) index].values)
            setById (s, kv.first, kv.second);
    return s;
}

} // namespace solaris
