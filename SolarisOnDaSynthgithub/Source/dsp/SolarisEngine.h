#pragma once

// SolarisOnDaSynth — moteur audio complet (utilise par le plugin ET par l'outil de rendu)
//
// Entree -> Coupe-bas -> [x2 : EQ analogique -> Saturation + Brillance]
//        -> Compresseur -> Stereo -> Delay -> Reverb -> Sortie

#include "SolarisAnalyzer.h"

namespace solaris
{
class Engine
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        fs = sampleRate;
        fs2 = sampleRate * 2.0;
        maxBlock = maxBlockSize;

        os.prepare (maxBlock);
        for (auto& b : up) b.assign ((size_t) (2 * maxBlock), 0.0f);
        wetL.assign ((size_t) maxBlock, 0.0f);
        wetR.assign ((size_t) maxBlock, 0.0f);
        dryL.assign ((size_t) maxBlock, 0.0f);
        dryR.assign ((size_t) maxBlock, 0.0f);
        dryDelay[0].prepare (getLatency() + 8);
        dryDelay[1].prepare (getLatency() + 8);

        // Ponderation K (ITU-R BS.1770) pour mesurer un volume proche de l'oreille
        for (auto* k : { &kInShelf[0], &kInShelf[1], &kOutShelf[0], &kOutShelf[1] })
            k->design (Biquad::HighShelf, fs, 1681.97, 0.7072, 4.0);
        for (auto* k : { &kInHp[0], &kInHp[1], &kOutHp[0], &kOutHp[1] })
            k->design (Biquad::HighPass, fs, 38.13, 0.5003);

        for (int ch = 0; ch < 2; ++ch)
        {
            eqDc[ch].setup (fs2);
            satDc[ch].setup (fs2);
            satSoftener[ch].setCutoff (fs2, 11000.0);
            exciterSilk[ch].setCutoff (fs2, 16000.0);
            delayDarken[ch].setCutoff (fs, 6000.0);
        }

        for (auto* sm : { &inGain, &outGain, &makeup, &compMix, &width })
            sm->setup (fs, 0.03);
        autoGainSm.setup (fs, 0.4);
        bypassSm.setup (fs, 0.02);
        for (auto* sm : { &drive, &satMix, &bright })
            sm->setup (fs2, 0.03);
        delayTime.setup (fs, 0.08);

        delayLine[0].prepare ((int) (4.0 * fs));
        delayLine[1].prepare ((int) (4.0 * fs));
        reverb.prepare (fs);

        firstBlock = true;
        reset();
    }

    void reset()
    {
        os.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto* b : { &hpf1[ch], &hpf2[ch], &lowBand[ch], &midBand[ch], &highBand[ch],
                             &exciterIn[ch], &exciterOut[ch], &scHpf[ch] })
                b->reset();
            eqDc[ch].reset(); satDc[ch].reset();
            satSoftener[ch].reset(); exciterSilk[ch].reset(); delayDarken[ch].reset();
            delayLine[ch].reset();
        }
        sideHp1.reset(); sideHp2.reset();
        dryDelay[0].reset(); dryDelay[1].reset();
        for (auto* k : { &kInShelf[0], &kInShelf[1], &kOutShelf[0], &kOutShelf[1], &kInHp[0], &kInHp[1], &kOutHp[0], &kOutHp[1] })
            k->reset();
        avgIn = avgOut = 0.0;
        compEnvDb = 0.0f;
        reverb.reset();
        firstBlock = true;
    }

    static constexpr int getLatency() noexcept { return Oversampler2x::latency; }
    float getGainReductionDb() const noexcept { return grDb; }
    ReverbTimes getReverbTimes() const noexcept { return times; }

    struct Levels { float inPeak[2] {}, outPeak[2] {}, autoGainDb = 0.0f; };
    const Levels& getLevels() const noexcept { return levels; }

    // n <= maxBlockSize. Si "analyzer" n'est pas nul, il recoit le son d'entree.
    void process (float* L, float* R, int n, const Settings& s, double bpm, Analyzer* analyzer = nullptr) noexcept
    {
        bpm = clampv (bpm, 30.0, 300.0);
        setTargets (s, bpm);
        bypassSm.target = s.bypass > 0.5f ? 1.0f : 0.0f;
        if (firstBlock)
        {
            for (auto* sm : { &inGain, &outGain, &makeup, &compMix, &width, &drive, &satMix, &bright, &delayTime, &bypassSm })
                sm->snap (sm->target);
            autoGainSm.snap (1.0f);
            firstBlock = false;
        }

        // 0. Son d'origine : vumetre d'entree, copie pour le bypass (meme latence), volume de reference
        levels.inPeak[0] = levels.inPeak[1] = 0.0f;
        double powIn = 0.0;
        for (int i = 0; i < n; ++i)
        {
            levels.inPeak[0] = std::max (levels.inPeak[0], std::abs (L[i]));
            levels.inPeak[1] = std::max (levels.inPeak[1], std::abs (R[i]));
            dryL[(size_t) i] = dryDelay[0].read ((float) getLatency()); dryDelay[0].push (L[i]);
            dryR[(size_t) i] = dryDelay[1].read ((float) getLatency()); dryDelay[1].push (R[i]);
            const float kl = kInHp[0].process (kInShelf[0].process (L[i]));
            const float kr = kInHp[1].process (kInShelf[1].process (R[i]));
            powIn += 0.5 * ((double) kl * kl + (double) kr * kr);
        }
        powIn /= std::max (1, n);

        // 1. Gain d'entree
        for (int i = 0; i < n; ++i)
        {
            const float g = inGain.next();
            L[i] *= g;
            R[i] *= g;
        }

        if (analyzer != nullptr)
            analyzer->feed (L, R, n);

        // 2. Coupe-bas 24 dB/oct
        if (s.hpfOn > 0.5f)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float* d = ch == 0 ? L : R;
                for (int i = 0; i < n; ++i)
                    d[i] = hpf2[ch].process (hpf1[ch].process (d[i]));
            }
        }

        // 3. Section x2 : EQ analogique + saturation (toujours traversee : latence constante)
        os.up (0, L, up[0].data(), n);
        os.up (1, R, up[1].data(), n);
        processColour (s, 2 * n);
        os.down (0, up[0].data(), L, n);
        os.down (1, up[1].data(), R, n);

        // 4. Compression
        if (s.compOn > 0.5f) processCompressor (s, L, R, n);
        else grDb = 0.0f;

        // 5. Stereo
        if (s.widthOn > 0.5f) processWidth (L, R, n);

        // 6. Delay
        const bool delayOn = s.delayOn > 0.5f;
        if (delayOn)
        {
            if (! delayWasOn) { delayLine[0].reset(); delayLine[1].reset(); delayDarken[0].reset(); delayDarken[1].reset(); }
            processDelay (s, L, R, n);
        }
        delayWasOn = delayOn;

        // 7. Reverb (en envoi : le son sec reste a 100 %)
        const bool revOn = s.revOn > 0.5f;
        if (revOn)
        {
            if (! reverbWasOn) reverb.reset();
            reverb.process (L, R, wetL.data(), wetR.data(), n, (int) std::lround (s.revMode),
                            times.decaySec, times.predelayMs, s.revLowCut, s.revHighCut, s.revMod);
            const float level = s.revMix / 100.0f;
            for (int i = 0; i < n; ++i)
            {
                L[i] += wetL[(size_t) i] * level;
                R[i] += wetR[(size_t) i] * level;
            }
        }
        reverbWasOn = revOn;

        // 8. Auto gain : meme volume percu que le son d'origine (moyenne glissante ~3 s,
        //    figee pendant les silences pour ne pas remonter les queues de reverb)
        double powOut = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float kl = kOutHp[0].process (kOutShelf[0].process (L[i]));
            const float kr = kOutHp[1].process (kOutShelf[1].process (R[i]));
            powOut += 0.5 * ((double) kl * kl + (double) kr * kr);
        }
        powOut /= std::max (1, n);

        if (powIn > 1.0e-6)   // au-dessus de -60 dB
        {
            const double a = std::exp (-(double) n / (3.0 * fs));
            avgIn  = a * avgIn  + (1.0 - a) * powIn;
            avgOut = a * avgOut + (1.0 - a) * powOut;
        }
        float agTarget = 1.0f;
        if (s.autoGain > 0.5f && avgOut > 1.0e-9 && avgIn > 1.0e-9)
            agTarget = clampv ((float) std::sqrt (avgIn / avgOut), 0.25f, 4.0f);   // +/- 12 dB max
        autoGainSm.target = agTarget;
        levels.autoGainDb = gainToDb (autoGainSm.cur);

        // 9. Gain de sortie, bypass (fondu de 20 ms vers l'original), vumetre de sortie
        levels.outPeak[0] = levels.outPeak[1] = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float g = outGain.next() * autoGainSm.next();
            const float b = bypassSm.next();
            L[i] = (1.0f - b) * L[i] * g + b * dryL[(size_t) i];
            R[i] = (1.0f - b) * R[i] * g + b * dryR[(size_t) i];
            levels.outPeak[0] = std::max (levels.outPeak[0], std::abs (L[i]));
            levels.outPeak[1] = std::max (levels.outPeak[1], std::abs (R[i]));
        }
    }

private:
    //==========================================================================
    void setTargets (const Settings& s, double bpm) noexcept
    {
        inGain.target  = dbToGain (s.inGain);
        outGain.target = dbToGain (s.outGain);
        drive.target   = dbToGain (s.satDrive);
        satMix.target  = s.satMix / 100.0f;
        bright.target  = s.satBright / 100.0f;
        makeup.target  = s.compMakeup;
        compMix.target = s.compMix / 100.0f;
        width.target   = s.width / 100.0f;

        const double nyq = fs * 0.45;
        for (int ch = 0; ch < 2; ++ch)
        {
            hpf1[ch].design (Biquad::HighPass, fs, std::min ((double) s.hpfFreq, nyq), 0.5412);
            hpf2[ch].design (Biquad::HighPass, fs, std::min ((double) s.hpfFreq, nyq), 1.3066);

            // EQ analogique : shelfs avec petite bosse en boost, Q proportionnel au gain
            const double lowQ  = s.lowGain  > 0 ? 0.9 : 0.707;
            const double highQ = s.highGain > 0 ? 0.85 : 0.707;
            const double midQ  = s.midQ * (0.7 + 0.7 * std::min (1.0, std::abs (s.midGain) / 12.0));
            lowBand[ch].design  (Biquad::LowShelf,  fs2, std::min ((double) s.lowFreq,  nyq), lowQ,  s.lowGain);
            midBand[ch].design  (Biquad::Peak,      fs2, std::min ((double) s.midFreq,  nyq), midQ,  s.midGain);
            highBand[ch].design (Biquad::HighShelf, fs2, std::min ((double) s.highFreq, nyq), highQ, s.highGain);

            exciterIn[ch].design  (Biquad::HighPass, fs2, std::min ((double) s.satBrightFreq, nyq), 0.707);
            exciterOut[ch].design (Biquad::HighPass, fs2, std::min ((double) s.satBrightFreq, nyq), 0.707);

            scHpf[ch].design (Biquad::HighPass, fs, std::min ((double) s.compScHpf, nyq), 0.707);

        }

        // Basses mono : passe-haut 24 dB sur le side uniquement
        sideHp1.design (Biquad::HighPass, fs, s.monoFreq, 0.5412);
        sideHp2.design (Biquad::HighPass, fs, s.monoFreq, 1.3066);

        static const float delayBeats[] = { 2.0f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
        const int note = clampv ((int) std::lround (s.delaySync), 0, 5);
        delayTime.target = clampv ((float) (delayBeats[note] * 60.0 / bpm * fs), 1.0f, (float) (delayLine[0].maxDelay() - 2));

        times = computeReverbTimes (s, bpm);
    }

    // Partie NON lineaire d'une saturation asymetrique douce (pente 1 a l'origine) :
    // uniquement les harmoniques ajoutees, surtout paires = chaleur.
    static float warmNl (float v, float k, float b, float tb, float norm) noexcept
    {
        return (std::tanh (k * v + b) - tb) * norm - v;
    }

    void processColour (const Settings& s, int n2) noexcept
    {
        const bool eqOn = s.eqOn > 0.5f, satOn = s.satOn > 0.5f;
        if (! eqOn && ! satOn)
        {
            // garder les lissages a jour
            for (int i = 0; i < n2; ++i) { drive.next(); satMix.next(); bright.next(); }
            return;
        }

        // Coloration EQ : seulement en boost, proportionnelle au boost
        const float colour = s.eqColour / 100.0f;
        auto amountFor = [colour] (float g) { return g > 0.0f ? 2.0f * colour * std::min (1.0f, g / 9.0f) : 0.0f; };
        const float aLow = amountFor (s.lowGain), aMid = amountFor (s.midGain), aHigh = amountFor (s.highGain);

        static const float eqK = 3.0f, eqB = 0.35f, eqTb = std::tanh (0.35f), eqNorm = 1.0f / (3.0f * (1.0f - eqTb * eqTb));
        static const float exK = 3.0f, exB = 0.45f, exTb = std::tanh (0.45f), exNorm = 1.0f / (3.0f * (1.0f - exTb * exTb));
        static const float tubeTb = std::tanh (0.3f), tubeSlope = 1.0f - tubeTb * tubeTb;
        const bool tube = s.satType > 0.5f;

        const int eqMode = clampv ((int) std::lround (s.eqMode), 0, 2);   // 0 stereo, 1 mid, 2 side

        auto eqChannel = [&] (int ch, float x) noexcept
        {
            float harm = 0.0f;
            float y = lowBand[ch].process (x);
            if (aLow > 0.0f) harm += aLow * warmNl (y - x, eqK, eqB, eqTb, eqNorm);
            x = y;
            y = midBand[ch].process (x);
            if (aMid > 0.0f) harm += aMid * warmNl (y - x, eqK, eqB, eqTb, eqNorm);
            x = y;
            y = highBand[ch].process (x);
            if (aHigh > 0.0f) harm += aHigh * warmNl (y - x, eqK, eqB, eqTb, eqNorm);
            return y + eqDc[ch].process (harm);
        };

        for (int i = 0; i < n2; ++i)
        {
            const float dr = drive.next(), mx = satMix.next(), br = bright.next();
            const float exDrive = 1.0f + 0.25f * (dr - 1.0f);

            float l = up[0][(size_t) i], r = up[1][(size_t) i];

            if (eqOn)
            {
                if (eqMode == 0)
                {
                    l = eqChannel (0, l);
                    r = eqChannel (1, r);
                }
                else
                {
                    // EQ M/S : on egalise seulement le centre (Mid) ou seulement les cotes (Side)
                    float m = 0.5f * (l + r), sd = 0.5f * (l - r);
                    if (eqMode == 1) m = eqChannel (0, m);
                    else             sd = eqChannel (0, sd);
                    l = m + sd;
                    r = m - sd;
                }
            }

            if (satOn)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    float x = ch == 0 ? l : r;

                    // Corps : harmoniques douces, adoucies au-dessus de 11 kHz
                    const float shaped = tube ? (std::tanh (x * dr + 0.3f) - tubeTb) / (dr * tubeSlope)
                                              : std::tanh (x * dr) / dr;
                    const float body = satDc[ch].process (satSoftener[ch].process (shaped - x));

                    // Brillance : harmoniques generees a partir du haut du spectre uniquement
                    const float highs = exciterIn[ch].process (x);
                    float air = warmNl (highs * exDrive, exK, exB, exTb, exNorm) / exDrive;
                    air = exciterSilk[ch].process (exciterOut[ch].process (air));

                    x = x + body * mx + air * br * 4.0f;
                    (ch == 0 ? l : r) = x;
                }
            }

            up[0][(size_t) i] = l;
            up[1][(size_t) i] = r;
        }
    }

    void processCompressor (const Settings& s, float* L, float* R, int n) noexcept
    {
        const float threshold = s.compThresh;
        const float ratio = std::max (1.0f, s.compRatio);
        const float slope = 1.0f - 1.0f / ratio;
        const float knee = 6.0f;
        const float att = std::exp (-1.0f / (std::max (0.05f, s.compAttack) * 0.001f * (float) fs));
        const float rel = std::exp (-1.0f / (std::max (5.0f, s.compRelease) * 0.001f * (float) fs));
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float scL = scHpf[0].process (L[i]);
            const float scR = scHpf[1].process (R[i]);
            const float level = std::max (std::abs (scL), std::abs (scR));
            const float levelDb = level > 1.0e-6f ? 20.0f * std::log10 (level) : -120.0f;

            const float over = levelDb - threshold;
            float gr = 0.0f;
            if (2.0f * over >= knee)
                gr = over * slope;
            else if (2.0f * over > -knee)
            {
                const float t = over + knee * 0.5f;
                gr = slope * t * t / (2.0f * knee);
            }

            const float c = gr > compEnvDb ? att : rel;
            compEnvDb = c * compEnvDb + (1.0f - c) * gr;
            maxGr = std::max (maxGr, compEnvDb);

            const float m = compMix.next();
            const float g = (1.0f - m) + m * dbToGain (makeup.next() - compEnvDb);
            L[i] *= g;
            R[i] *= g;
        }
        grDb = maxGr;
    }

    // Le centre (mid) n'est jamais modifie : on retire seulement les basses du cote (side)
    // -> basses parfaitement mono, puis on elargit le side. Aucun decalage de phase sur le centre.
    void processWidth (float* L, float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float mid  = 0.5f * (L[i] + R[i]);
            float side = 0.5f * (L[i] - R[i]);
            side = sideHp2.process (sideHp1.process (side)) * width.next();
            L[i] = mid + side;
            R[i] = mid - side;
        }
    }

    void processDelay (const Settings& s, float* L, float* R, int n) noexcept
    {
        const float fb = s.delayFb / 100.0f, level = s.delayMix / 100.0f;
        const bool ping = s.delayPing > 0.5f;

        for (int i = 0; i < n; ++i)
        {
            const float d = delayTime.next();
            const float wl = delayLine[0].read (d);
            const float wr = delayLine[1].read (d);
            const float fl = delayDarken[0].process (wl);
            const float fr = delayDarken[1].process (wr);

            if (ping)
            {
                delayLine[0].push (0.5f * (L[i] + R[i]) + fr * fb);
                delayLine[1].push (fl * fb);
            }
            else
            {
                delayLine[0].push (L[i] + fl * fb);
                delayLine[1].push (R[i] + fr * fb);
            }
            L[i] += wl * level;
            R[i] += wr * level;
        }
    }

    //==========================================================================
    double fs = 48000.0, fs2 = 96000.0;
    int maxBlock = 512;
    bool firstBlock = true;

    Smooth inGain, outGain, drive, satMix, bright, makeup, compMix, width, delayTime, autoGainSm, bypassSm;

    std::vector<float> dryL, dryR;
    DelayLine dryDelay[2];
    Biquad kInShelf[2], kInHp[2], kOutShelf[2], kOutHp[2];
    double avgIn = 0.0, avgOut = 0.0;
    Levels levels;

    Oversampler2x os;
    std::vector<float> up[2], wetL, wetR;

    Biquad hpf1[2], hpf2[2];
    Biquad lowBand[2], midBand[2], highBand[2];
    DcBlocker eqDc[2], satDc[2];
    OnePoleLowpass satSoftener[2], exciterSilk[2];
    Biquad exciterIn[2], exciterOut[2];

    Biquad scHpf[2];
    float compEnvDb = 0.0f, grDb = 0.0f;

    Biquad sideHp1, sideHp2;

    DelayLine delayLine[2];
    OnePoleLowpass delayDarken[2];
    bool delayWasOn = false;

    SpaceReverb reverb;
    bool reverbWasOn = false;
    ReverbTimes times;
};

} // namespace solaris
