#pragma once

// SolarisOnDaSynth — blocs DSP de base (sans dependance a JUCE)

#include "SolarisParams.h"
#include <array>

namespace solaris
{
//==============================================================================
struct Biquad
{
    enum Type { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // Formules RBJ (Audio EQ Cookbook)
    void design (Type t, double fs, double f, double q, double gainDb = 0.0) noexcept
    {
        f = clampv (f, 10.0, fs * 0.49);
        q = std::max (0.05, q);
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w  = kTwoPi * f / fs;
        const double cw = std::cos (w), sw = std::sin (w), al = sw / (2.0 * q);
        double B0, B1, B2, A0, A1, A2;

        switch (t)
        {
            case LowPass:  B0 = (1 - cw) / 2; B1 = 1 - cw;    B2 = (1 - cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case HighPass: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case BandPass: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case Peak:     B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
            case LowShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * al;
                B0 = A * ((A + 1) - (A - 1) * cw + sq);
                B1 = 2 * A * ((A - 1) - (A + 1) * cw);
                B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq;
                A1 = -2 * ((A - 1) + (A + 1) * cw);
                A2 = (A + 1) + (A - 1) * cw - sq;
                break;
            }
            case HighShelf:
            default:
            {
                const double sq = 2.0 * std::sqrt (A) * al;
                B0 = A * ((A + 1) + (A - 1) * cw + sq);
                B1 = -2 * A * ((A - 1) + (A + 1) * cw);
                B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq;
                A1 = 2 * ((A - 1) - (A + 1) * cw);
                A2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
        }

        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

struct OnePoleLowpass
{
    float z = 0.0f, a = 0.0f;
    void setCutoff (double fs, double hz) noexcept { a = (float) std::exp (-kTwoPi * clampv (hz, 1.0, fs * 0.49) / fs); }
    void reset() noexcept { z = 0.0f; }
    float process (float x) noexcept { z = (1.0f - a) * x + a * z; return z; }
};

struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.999f;
    void setup (double fs, double hz = 10.0) noexcept { r = (float) (1.0 - kTwoPi * hz / fs); x1 = y1 = 0.0f; }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// Lissage exponentiel des parametres (evite les clics)
struct Smooth
{
    float cur = 0.0f, target = 0.0f, coef = 1.0f;
    void setup (double fs, double seconds) noexcept { coef = (float) (1.0 - std::exp (-1.0 / (std::max (1.0e-4, seconds) * fs))); }
    void snap (float v) noexcept { cur = target = v; }
    float next() noexcept { cur += (target - cur) * coef; return cur; }
};

// Ligne a retard circulaire avec lecture interpolee. Toujours lire AVANT d'ecrire.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    int maxDelay() const noexcept { return mask - 2; }

    float read (float delay) const noexcept
    {
        delay = clampv (delay, 1.0f, (float) (mask - 2));
        const float pos = (float) w - delay;
        const float fl  = std::floor (pos);
        const int   i   = (int) fl;
        const float fr  = pos - fl;
        const float s0  = buf[(size_t) (i & mask)];
        const float s1  = buf[(size_t) ((i + 1) & mask)];
        return s0 + fr * (s1 - s0);
    }
    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
};

//==============================================================================
// Suréchantillonnage x2 a phase lineaire (FIR 127 coefficients, fenetre de Kaiser).
// Latence totale (montee + descente) = 63 echantillons, constante.
class Oversampler2x
{
public:
    static constexpr int numTaps = 127;
    static constexpr int phaseLen = (numTaps + 1) / 2;     // 64
    static constexpr int latency = (numTaps - 1) / 2;      // 63 echantillons a la frequence d'origine

    void prepare (int maxBlock)
    {
        design();
        for (int ch = 0; ch < 2; ++ch)
        {
            upHist[ch].assign ((size_t) (phaseLen - 1 + maxBlock), 0.0f);
            downHist[ch].assign ((size_t) (numTaps - 1 + 2 * maxBlock), 0.0f);
        }
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (upHist[ch].begin(), upHist[ch].end(), 0.0f);
            std::fill (downHist[ch].begin(), downHist[ch].end(), 0.0f);
        }
    }

    void up (int ch, const float* in, float* out, int n) noexcept
    {
        auto& h = upHist[ch];
        std::copy (in, in + n, h.begin() + (phaseLen - 1));
        for (int i = 0; i < n; ++i)
        {
            const float* x = h.data() + i + (phaseLen - 1);
            float e = 0.0f, o = 0.0f;
            for (int k = 0; k < phaseLen; ++k)
            {
                e += evenTaps[(size_t) k] * x[-k];
                o += oddTaps[(size_t) k]  * x[-k];
            }
            out[2 * i]     = e;
            out[2 * i + 1] = o;
        }
        std::copy (h.begin() + n, h.begin() + n + (phaseLen - 1), h.begin());
    }

    void down (int ch, const float* in2x, float* out, int n) noexcept
    {
        auto& h = downHist[ch];
        std::copy (in2x, in2x + 2 * n, h.begin() + (numTaps - 1));
        for (int i = 0; i < n; ++i)
        {
            const float* u = h.data() + (numTaps - 1) + 2 * i;
            float acc = 0.0f;
            for (int k = 0; k < numTaps; ++k)
                acc += taps[(size_t) k] * u[-k];
            out[i] = acc;
        }
        std::copy (h.begin() + 2 * n, h.begin() + 2 * n + (numTaps - 1), h.begin());
    }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 50; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    void design()
    {
        const double fc = 0.23, beta = 8.0, centre = (numTaps - 1) / 2.0;
        double sum = 0.0;
        for (int n = 0; n < numTaps; ++n)
        {
            const double t = n - centre;
            const double sinc = t == 0.0 ? 2.0 * fc : std::sin (kTwoPi * fc * t) / (3.141592653589793 * t);
            const double r = 2.0 * n / (numTaps - 1) - 1.0;
            const double win = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            taps[(size_t) n] = (float) (sinc * win);
            sum += sinc * win;
        }
        for (auto& t : taps)
            t = (float) (t / sum);

        evenTaps.fill (0.0f);
        oddTaps.fill (0.0f);
        for (int k = 0; k < phaseLen; ++k)
        {
            if (2 * k < numTaps)     evenTaps[(size_t) k] = 2.0f * taps[(size_t) (2 * k)];
            if (2 * k + 1 < numTaps) oddTaps[(size_t) k]  = 2.0f * taps[(size_t) (2 * k + 1)];
        }
    }

    std::array<float, numTaps> taps {};
    std::array<float, phaseLen> evenTaps {}, oddTaps {};
    std::vector<float> upHist[2], downHist[2];
};

//==============================================================================
// Reverb algorithmique : diffusion + FDN 8 lignes (matrice de Hadamard),
// modulation des lignes, amorti des aigus dans la boucle, reflexions precoces.
// Modes inspires des reverbs "vintage" : Vintage Hall, Plate, Room, Chamber, Cosmos.
class SpaceReverb
{
public:
    static constexpr int numLines = 8;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& p : pre)
            p.prepare ((int) (0.6 * fs));
        for (auto& l : lines)
            l.prepare ((int) (0.21 * fs));
        for (auto& ap : allpassL) ap.line.prepare ((int) (0.03 * fs));
        for (auto& ap : allpassR) ap.line.prepare ((int) (0.03 * fs));
        er.prepare ((int) (0.1 * fs));
        preSmooth.setup (fs, 0.08);
        preSmooth.snap (0.02f * (float) fs);
        reset();
    }

    void reset()
    {
        for (auto& p : pre) p.reset();
        for (auto& l : lines) l.reset();
        for (auto& ap : allpassL) ap.line.reset();
        for (auto& ap : allpassR) ap.line.reset();
        er.reset();
        for (auto& d : damp) d.reset();
        for (auto& f : inLow) f.reset();
        for (auto& f : inHigh) f.reset();
        for (int i = 0; i < numLines; ++i)
            phase[(size_t) i] = (double) i * kTwoPi / numLines;
    }

    // Ecrit le signal "wet" seul dans outL / outR
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  int modeIndex, float decaySec, float predelayMs, float lowCut, float highCut, float modAmount) noexcept
    {
        const ModeDef& m = modeDef (modeIndex);
        decaySec = clampv (decaySec, 0.1f, 30.0f);

        // Filtres d'entree : on nettoie les basses (reverb propre, style US) et les aigus
        for (int ch = 0; ch < 2; ++ch)
        {
            inLow[ch].design  (Biquad::HighPass, fs, lowCut, 0.707);
            inHigh[ch].design (Biquad::LowPass,  fs, std::min ((double) highCut * 1.15, fs * 0.45), 0.707);
        }

        // Lignes du FDN : longueur, gain de decroissance, amorti
        const double sqrtSize = std::sqrt (m.size);
        static const double baseMs[numLines] = { 31.7, 37.9, 41.3, 45.1, 53.3, 59.9, 67.1, 73.7 };
        const float modDepth = (float) (m.modDepthMs * 0.001 * fs) * (modAmount / 50.0f);
        for (int i = 0; i < numLines; ++i)
        {
            lineDelay[(size_t) i] = (float) (baseMs[i] * m.size * 0.001 * fs);
            lineGain[(size_t) i]  = (float) std::pow (10.0, -3.0 * lineDelay[(size_t) i] / (decaySec * fs));
            damp[(size_t) i].setCutoff (fs, std::min ((double) highCut * m.brightness, fs * 0.45));
        }

        static const double apL[4] = { 4.31, 3.37, 12.1, 8.93 };
        static const double apR[4] = { 4.73, 3.11, 11.3, 9.67 };
        for (int k = 0; k < 4; ++k)
        {
            allpassL[(size_t) k].delay = (float) (apL[k] * sqrtSize * 0.001 * fs);
            allpassR[(size_t) k].delay = (float) (apR[k] * sqrtSize * 0.001 * fs);
            allpassL[(size_t) k].g = allpassR[(size_t) k].g = (float) m.diffusion;
        }

        static const double erMs[6]   = { 7.1, 11.3, 16.9, 23.3, 29.9, 37.1 };
        static const float  erGain[6] = { 0.8f, 0.65f, 0.5f, 0.4f, 0.3f, 0.22f };
        float erDelay[6];
        for (int k = 0; k < 6; ++k)
            erDelay[k] = (float) (erMs[k] * m.size * 0.001 * fs);

        preSmooth.target = std::max (1.0f, predelayMs * 0.001f * (float) fs);
        const double lfoInc = kTwoPi * m.modRateHz / fs;

        // Niveau de sortie : normalise selon la longueur pour garder un niveau stable
        const float outScale = 0.55f / std::sqrt (std::max (0.3f, decaySec) / 2.0f) / std::sqrt ((float) m.size);

        for (int i = 0; i < n; ++i)
        {
            // Pre-delay
            const float pd = preSmooth.next();
            float l = pre[0].read (pd); pre[0].push (inL[i]);
            float r = pre[1].read (pd); pre[1].push (inR[i]);

            l = inHigh[0].process (inLow[0].process (l));
            r = inHigh[1].process (inLow[1].process (r));

            // Reflexions precoces (Room / Chamber)
            float erL = 0.0f, erR = 0.0f;
            if (m.erLevel > 0.0f)
            {
                for (int k = 0; k < 6; ++k)
                {
                    const float tap = er.read (erDelay[k]) * erGain[k];
                    if (k & 1) erR += tap; else erL += tap;
                }
            }
            er.push (0.5f * (l + r));

            // Diffusion d'entree
            for (auto& ap : allpassL) l = ap.process (l);
            for (auto& ap : allpassR) r = ap.process (r);

            // FDN
            float o[numLines];
            for (int k = 0; k < numLines; ++k)
            {
                const float mod = modDepth * (float) std::sin (phase[(size_t) k]);
                phase[(size_t) k] += lfoInc * (1.0 + 0.13 * k);
                if (phase[(size_t) k] > kTwoPi) phase[(size_t) k] -= kTwoPi;
                o[k] = lines[(size_t) k].read (lineDelay[(size_t) k] + mod + modDepth + 2.0f);
            }

            const float wetL = (o[0] - o[1] + o[2] - o[3] + o[4] - o[5] + o[6] - o[7]);
            const float wetR = (o[0] + o[1] - o[2] - o[3] + o[4] + o[5] - o[6] - o[7]);

            float v[numLines];
            for (int k = 0; k < numLines; ++k)
                v[k] = damp[(size_t) k].process (o[k]) * lineGain[(size_t) k];

            hadamard (v);

            for (int k = 0; k < numLines; ++k)
            {
                const float in = (k < 4 ? l : r) * ((k & 1) ? -0.5f : 0.5f);
                lines[(size_t) k].push (v[k] + in);
            }

            outL[i] = wetL * outScale + erL * m.erLevel * 0.5f;
            outR[i] = wetR * outScale + erR * m.erLevel * 0.5f;
        }
    }

private:
    struct ModeDef { double size, diffusion, modDepthMs, modRateHz; float erLevel; double brightness; };

    static const ModeDef& modeDef (int index)
    {
        static const ModeDef modes[5] = {
            { 1.50, 0.70, 0.55, 0.45, 0.00f, 0.85 },   // Vintage Hall : grand, sombre, chorus lent
            { 0.70, 0.78, 0.20, 0.90, 0.00f, 1.25 },   // Plate : dense, brillant
            { 0.42, 0.62, 0.12, 0.70, 0.55f, 1.00 },   // Room : petit, reflexions precoces
            { 0.85, 0.72, 0.28, 0.60, 0.30f, 0.95 },   // Chamber : chaleureux
            { 2.20, 0.75, 1.20, 0.25, 0.00f, 1.10 },   // Cosmos : immense, tres module
        };
        return modes[clampv (index, 0, 4)];
    }

    static void hadamard (float* v) noexcept
    {
        for (int h = 1; h < numLines; h *= 2)
            for (int i = 0; i < numLines; i += 2 * h)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b;
                    v[j + h] = a - b;
                }
        const float norm = 0.35355339f;   // 1 / sqrt(8)
        for (int k = 0; k < numLines; ++k)
            v[k] *= norm;
    }

    struct Allpass
    {
        DelayLine line;
        float delay = 100.0f, g = 0.7f;
        float process (float x) noexcept
        {
            const float d = line.read (delay);
            const float w = x + g * d;
            line.push (w);
            return d - g * w;
        }
    };

    double fs = 48000.0;
    DelayLine pre[2], er;
    std::array<DelayLine, numLines> lines;
    std::array<Allpass, 4> allpassL, allpassR;
    std::array<OnePoleLowpass, numLines> damp;
    std::array<float, numLines> lineDelay {}, lineGain {};
    std::array<double, numLines> phase {};
    Biquad inLow[2], inHigh[2];
    Smooth preSmooth;
};

} // namespace solaris
