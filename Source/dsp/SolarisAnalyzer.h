#pragma once

// SolarisOnDaSynth — analyse du son et reglage automatique des modules

#include "SolarisDSP.h"

namespace solaris
{
//==============================================================================
// Ce que l'analyse retient du son
struct Report
{
    bool  valid = false;
    float seconds = 0.0f;
    float l10 = -100.0f, l50 = -100.0f, l95 = -100.0f, dynamicRange = 0.0f, peakDb = -100.0f;
    float onsetRate = 0.0f, riseMs = 20.0f, intervalMs = -1.0f;
    bool  percussive = false;
    float lowRatio = 0.0f;
    float bandRel[7] {};        // energie par octave (dB) par rapport a la bande 600-1200 Hz
    float sideToMidDb = -100.0f;
};

// Bandes d'octave mesurees : 150-300, 300-600, 600-1200, 1.2-2.5k, 2.5-5k, 5-10k, 10-20k
constexpr double kBandLo[7] = { 150, 300, 600, 1200, 2500, 5000, 10000 };
constexpr double kBandHi[7] = { 300, 600, 1200, 2500, 5000, 10000, 20000 };
// Equilibre vise pour une melodie "propre" (reference : mix modernes US, pente douce vers l'aigu)
constexpr float kBandTarget[7] = { 3.0f, 2.0f, 0.0f, -2.5f, -5.0f, -8.5f, -13.0f };

//==============================================================================
// Collecte (thread audio, sans allocation apres prepare)
class Analyzer
{
public:
    // maxTargetSeconds : la plus longue analyse possible (allocation faite ici, une fois)
    void prepare (double sampleRate, float maxTargetSeconds = 24.0f)
    {
        fs = sampleRate;
        winLen = std::max (1, (int) std::lround (fs * 0.005));
        maxTarget = maxTargetSeconds;
        maxWindows = (int) (3.0f * maxTargetSeconds / 0.005f);   // marge pour les silences
        targetActive = (int) (6.0f / 0.005f);
        rmsDb.assign ((size_t) maxWindows, -100.0f);
        lowDb.assign ((size_t) maxWindows, -100.0f);
        low1.setCutoff (fs, 150.0);
        low2.setCutoff (fs, 150.0);
        for (int b = 0; b < 7; ++b)
        {
            const double hi = std::min (kBandHi[b], fs * 0.45);
            const double c  = std::sqrt (kBandLo[b] * hi);
            // 2 passe-bandes en cascade = filtre d'octave plus selectif
            bands[b].design  (Biquad::BandPass, fs, c, 1.2 * c / (hi - kBandLo[b]));
            bands2[b].design (Biquad::BandPass, fs, c, 1.2 * c / (hi - kBandLo[b]));
        }
        start (6.0f);
    }

    // targetSeconds : duree de son utile a ecouter (6, 12 ou 24 s)
    void start (float targetSeconds) noexcept
    {
        targetActive = (int) (std::min (targetSeconds, maxTarget) / 0.005f);
        pos = numWindows = numActive = 0;
        accSq = accLow = accMid = accSide = 0.0;
        for (auto& a : accBand) a = 0.0;
        for (auto& t : totBand) t = 0.0;
        totMid = totSide = totFull = 0.0;
        peak = winPeak = 0.0f;
        low1.reset(); low2.reset();
        for (auto& b : bands) b.reset();
        for (auto& b : bands2) b.reset();
        done = false;
    }

    bool isDone() const noexcept { return done; }
    float getProgress() const noexcept { return std::min (1.0f, (float) numActive / (float) std::max (1, targetActive)); }

    // Renvoie true quand l'analyse est terminee
    bool feed (const float* L, const float* R, int n) noexcept
    {
        if (done)
            return true;

        for (int i = 0; i < n; ++i)
        {
            const float mid  = 0.5f * (L[i] + R[i]);
            const float side = 0.5f * (L[i] - R[i]);
            const float low  = low2.process (low1.process (mid));

            accSq   += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]);
            accLow  += (double) low * low;
            accMid  += (double) mid * mid;
            accSide += (double) side * side;
            for (int b = 0; b < 7; ++b)
            {
                const float y = bands2[b].process (bands[b].process (mid));
                accBand[b] += (double) y * y;
            }
            winPeak = std::max (winPeak, std::max (std::abs (L[i]), std::abs (R[i])));

            if (++pos >= winLen)
            {
                const double nn = (double) winLen;
                const float r = 10.0f * (float) std::log10 (accSq / nn + 1.0e-12);
                rmsDb[(size_t) numWindows] = r;
                lowDb[(size_t) numWindows] = 10.0f * (float) std::log10 (accLow / nn + 1.0e-12);
                ++numWindows;

                if (r > -55.0f)
                {
                    ++numActive;
                    for (int b = 0; b < 7; ++b) totBand[b] += accBand[b];
                    totMid += accMid;
                    totSide += accSide;
                    totFull += accSq;
                    peak = std::max (peak, winPeak);
                }

                pos = 0;
                accSq = accLow = accMid = accSide = 0.0;
                for (auto& a : accBand) a = 0.0;
                winPeak = 0.0f;

                if (numActive >= targetActive || numWindows >= maxWindows)
                {
                    done = true;
                    return true;
                }
            }
        }
        return false;
    }

    void finishNow() noexcept { done = true; }

    // Calcul des mesures (thread de l'interface, une fois l'analyse terminee)
    Report makeReport() const
    {
        Report rep;
        const int n = numWindows;

        std::vector<float> active;
        double fullPow = 0.0, lowPow = 0.0;
        for (int i = 0; i < n; ++i)
        {
            if (rmsDb[(size_t) i] > -55.0f)
            {
                active.push_back (rmsDb[(size_t) i]);
                fullPow += std::pow (10.0, rmsDb[(size_t) i] / 10.0);
                lowPow  += std::pow (10.0, lowDb[(size_t) i] / 10.0);
            }
        }

        if (active.size() < 100)
            return rep;

        rep.valid = true;
        rep.seconds = (float) n * 0.005f;
        rep.l10 = percentile (active, 0.10f);
        rep.l50 = percentile (active, 0.50f);
        rep.l95 = percentile (active, 0.95f);
        rep.dynamicRange = rep.l95 - rep.l10;
        rep.peakDb = gainToDb (peak);
        rep.lowRatio = (float) (lowPow / std::max (1.0e-12, fullPow));

        // Attaques des notes, detectees sans les basses
        std::vector<double> highPow ((size_t) n);
        for (int i = 0; i < n; ++i)
            highPow[(size_t) i] = std::max (1.0e-12, std::pow (10.0, rmsDb[(size_t) i] / 10.0)
                                                    - std::pow (10.0, lowDb[(size_t) i] / 10.0));

        std::vector<float> rise, interval;
        int lastOnset = -1000;
        for (int i = 10; i < n - 20; ++i)
        {
            double prevPow = 0.0;
            for (int j = i - 10; j < i; ++j)
                prevPow += highPow[(size_t) j];
            const double prevDb = 10.0 * std::log10 (prevPow / 10.0 + 1.0e-12);
            const double curDb  = 10.0 * std::log10 (std::max (highPow[(size_t) i], highPow[(size_t) i + 1]));

            if (curDb - prevDb > 5.0 && rmsDb[(size_t) i] > rep.l10 + 3.0f && i - lastOnset >= 10)
            {
                int peakIdx = i;
                for (int j = i; j < i + 20; ++j)
                    if (rmsDb[(size_t) j] > rmsDb[(size_t) peakIdx])
                        peakIdx = j;
                rise.push_back ((float) (peakIdx - i + 1) * 5.0f);
                if (lastOnset >= 0)
                    interval.push_back ((float) (i - lastOnset) * 5.0f);
                lastOnset = i;
            }
        }

        rep.onsetRate  = (float) rise.size() / std::max (0.1f, rep.seconds);
        rep.riseMs     = rise.empty() ? 20.0f : percentile (rise, 0.5f);
        rep.intervalMs = interval.size() >= 3 ? percentile (interval, 0.5f) : -1.0f;
        rep.percussive = rep.onsetRate > 1.0f && rep.dynamicRange > 8.0f;

        // Equilibre tonal par octave (normalise par la largeur de bande)
        double ref = 0.0;
        for (int b = 0; b < 7; ++b)
            if (b == 2) ref = totBand[b];
        for (int b = 0; b < 7; ++b)
            rep.bandRel[b] = 10.0f * (float) std::log10 ((totBand[b] + 1.0e-20) / (ref + 1.0e-20));

        rep.sideToMidDb = 10.0f * (float) std::log10 ((totSide + 1.0e-20) / (totMid + 1.0e-20));
        return rep;
    }

    static float percentile (std::vector<float> v, float pct)
    {
        if (v.empty())
            return -100.0f;
        const auto idx = (size_t) clampv ((int) std::lround (pct * (float) (v.size() - 1)), 0, (int) v.size() - 1);
        std::nth_element (v.begin(), v.begin() + (std::ptrdiff_t) idx, v.end());
        return v[idx];
    }

private:
    double fs = 48000.0;
    float maxTarget = 24.0f;
    int winLen = 240, maxWindows = 4000, targetActive = 1200;
    int pos = 0, numWindows = 0, numActive = 0;
    double accSq = 0, accLow = 0, accMid = 0, accSide = 0;
    double accBand[7] {}, totBand[7] {};
    double totMid = 0, totSide = 0, totFull = 0;
    float peak = 0.0f, winPeak = 0.0f;
    bool done = false;
    std::vector<float> rmsDb, lowDb;
    OnePoleLowpass low1, low2;
    Biquad bands[7], bands2[7];
};

//==============================================================================
enum class Scope { All, Compressor };

// Transforme les mesures en reglages. Philosophie : nettoyer et embellir,
// sans denaturer le son d'origine (corrections limitees, moitie de l'ecart mesure).
inline Settings suggestSettings (const Report& r, Settings s, Scope scope, double bpm, std::string& summary)
{
    char buf[256];
    summary.clear();

    if (! r.valid)
    {
        summary = "Pas assez de son : relance l'analyse pendant la lecture";
        return s;
    }

    const bool group = s.compSource > 0.5f;
    const bool fullMix = r.lowRatio > 0.45f;     // beaucoup de basses : mix complet ou synthe grave

    //--------------------------------------------------------------------------
    // Compresseur : clarte (attaque qui laisse passer les notes) + puissance (parallele)
    {
        const float mix = group ? 70.0f : 80.0f;
        const float targetGr = group ? 3.5f : 5.0f;
        const float ratio = group ? clampv (1.3f + r.dynamicRange * 0.08f, 1.5f, 3.0f)
                                  : clampv (1.5f + r.dynamicRange * 0.15f, 1.8f, 5.0f);
        const float threshold = clampv (r.l95 - targetGr / (1.0f - 1.0f / ratio) - 3.0f, -48.0f, -1.0f);

        float attack = r.percussive ? clampv (r.riseMs + 8.0f, 10.0f, 30.0f)
                                    : clampv (r.riseMs + 3.0f, 3.0f, 15.0f);
        if (group) attack = std::min (50.0f, attack + 10.0f);

        const float quarterMs = 60000.0f / (float) clampv (bpm, 40.0, 300.0);
        // Notes tres rapprochees (< 150 ms : batterie, roulements) -> on se cale sur la croche
        float release = (r.percussive && r.intervalMs >= 150.0f) ? clampv (r.intervalMs * 0.5f, 80.0f, 400.0f)
                      : r.percussive ? clampv (quarterMs * 0.5f, 100.0f, 300.0f)
                                     : clampv (quarterMs * 0.75f, 150.0f, 500.0f);
        if (group) release = std::min (600.0f, release * 1.3f);

        float sc = r.lowRatio > 0.5f ? 160.0f : (r.lowRatio > 0.25f ? 120.0f : 80.0f);
        if (group) sc += 20.0f;

        s.compOn = 1; s.compThresh = threshold; s.compRatio = ratio; s.compAttack = attack;
        s.compRelease = release; s.compScHpf = sc; s.compMix = mix;
        s.compMakeup = clampv (targetGr * (mix / 100.0f) * 0.7f, 0.0f, 12.0f);

        std::snprintf (buf, sizeof (buf), "%s | Comp %.1f:1 att %d ms rel %d ms",
                       r.percussive ? "Son percussif" : "Son tenu", ratio, (int) attack, (int) release);
        summary += buf;
    }

    if (scope == Scope::Compressor)
        return s;

    //--------------------------------------------------------------------------
    // Ecart avec l'equilibre vise
    float dev[7];
    for (int b = 0; b < 7; ++b)
        dev[b] = r.bandRel[b] - kBandTarget[b];

    // Coupe-bas : on laisse la place au 808 sans vider le son
    s.hpfOn = 1;
    s.hpfFreq = fullMix ? 25.0f : (r.lowRatio > 0.2f ? 80.0f : 120.0f);

    // EQ : une seule correction de medium (boue OU durete), corrections douces
    s.eqOn = 1;
    s.eqColour = 50.0f;
    const float mud   = std::max (dev[1], 0.5f * (dev[0] + dev[1]));
    const float harsh = dev[4];
    if (harsh > 2.0f && harsh > mud)
    {
        s.midFreq = 3500.0f; s.midQ = 1.4f;
        s.midGain = -std::min (3.0f, harsh * 0.5f);
    }
    else if (mud > 1.0f)
    {
        s.midFreq = dev[0] > dev[1] ? 250.0f : 350.0f; s.midQ = 1.0f;
        s.midGain = -std::min (3.5f, mud * 0.5f);
    }
    else
    {
        s.midGain = 0.0f;
    }

    // Grave : un peu de corps si le bas-medium manque
    s.lowFreq = 180.0f;
    s.lowGain = dev[0] < -2.0f ? std::min (1.5f, -dev[0] * 0.4f) : 0.0f;

    // Aigu : de l'air colore si le haut manque, on calme s'il y en a trop
    // Au-dessus de 10 kHz, un manque enorme = son filtre ou MP3 : on ne s'y fie pas
    const float airDeficit = dev[6] < -12.0f ? -dev[5] : -(0.4f * dev[5] + 0.6f * dev[6]);
    s.highFreq = dev[5] < dev[6] ? 8000.0f : 11000.0f;
    if (airDeficit > 0.0f)        s.highGain = clampv (airDeficit * 0.5f, 0.5f, 3.0f);
    else if (airDeficit < -2.0f)  s.highGain = std::max (-2.0f, airDeficit * 0.4f);
    else                          s.highGain = 0.5f;

    // Saturation : chaleur discrete, brillance selon le manque d'air
    s.satOn = 1;
    s.satType = r.percussive ? 0.0f : 1.0f;
    s.satDrive = r.dynamicRange > 12.0f ? 4.0f : 6.0f;
    s.satMix = 30.0f;
    s.satBright = airDeficit > 1.0f ? 30.0f : (airDeficit < -2.0f ? 10.0f : 20.0f);
    s.satBrightFreq = harsh > 1.0f ? 7000.0f : 5500.0f;

    // Stereo : on elargit seulement un son trop etroit
    s.monoFreq = 120.0f;
    if (r.sideToMidDb < -20.0f)      { s.widthOn = 1; s.width = 125.0f; }
    else if (r.sideToMidDb < -10.0f) { s.widthOn = 1; s.width = 115.0f; }
    else                             { s.widthOn = 0; s.width = 100.0f; }

    // Delay : discret, rythmique
    s.delayOn = 1;
    s.delayPing = 1;
    if (r.percussive) { s.delaySync = 2; s.delayFb = 22.0f; s.delayMix = 10.0f; }
    else              { s.delaySync = 1; s.delayFb = 18.0f; s.delayMix = 7.0f; }

    // Reverb : Vintage Hall pour les sons tenus, Plate pour les sons percussifs,
    // duree calee sur le tempo (la plus longue qui reste sous 4 s / 2,4 s)
    s.revOn = 1;
    s.revMode = r.percussive ? 1.0f : 0.0f;
    s.revDecaySync = 1;
    s.revPreSync = 1;
    s.revPreNote = 0;
    {
        static const float beats[] = { 1, 2, 4, 8, 16 };
        const float beat = 60.0f / (float) clampv (bpm, 40.0, 300.0);
        const float maxTotal = r.percussive ? 2.4f : 4.0f;
        int idx = 0;
        for (int k = 0; k < 5; ++k)
            if (beats[k] * beat <= maxTotal)
                idx = k;
        s.revDecayNote = (float) idx;
    }
    s.revLowCut  = 250.0f;
    s.revHighCut = r.percussive ? 9000.0f : 7000.0f;
    s.revMod     = 50.0f;
    s.revMix     = fullMix ? 10.0f : (r.percussive ? 14.0f : 18.0f);

    static const char* modeNames[] = { "Vintage Hall", "Plate" };
    const ReverbTimes t = computeReverbTimes (s, bpm);
    std::snprintf (buf, sizeof (buf), " | EQ %+.1f dB @%d Hz, air %+.1f dB | %s %.2f s",
                   s.midGain, (int) s.midFreq, s.highGain, modeNames[(int) s.revMode], t.decaySec);
    summary += buf;
    return s;
}

} // namespace solaris
