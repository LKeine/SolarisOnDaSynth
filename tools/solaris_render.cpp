// SolarisOnDaSynth — rendu hors-ligne d'un fichier WAV avec le moteur du plugin.
//
//   solaris_render entree.wav sortie.wav [--bpm 120] [--auto | --preset N] [--source groupe]
//                  [--set id=valeur ...]
//
// --auto : analyse le son (comme le bouton "Analyser le synthe") puis applique les reglages.

#include "../Source/dsp/SolarisEngine.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace
{
struct Audio
{
    int sampleRate = 48000;
    std::vector<float> L, R;
};

template <typename T> T readLE (std::istream& in) { T v {}; in.read (reinterpret_cast<char*> (&v), sizeof (T)); return v; }

bool readWav (const std::string& path, Audio& a)
{
    std::ifstream in (path, std::ios::binary);
    if (! in) return false;
    char id[4];
    in.read (id, 4); readLE<uint32_t> (in); in.read (id, 4);
    if (std::string (id, 4) != "WAVE") return false;

    uint16_t format = 1, channels = 2, bits = 16;
    while (in.read (id, 4))
    {
        const uint32_t size = readLE<uint32_t> (in);
        const std::string chunk (id, 4);
        if (chunk == "fmt ")
        {
            format = readLE<uint16_t> (in); channels = readLE<uint16_t> (in);
            a.sampleRate = (int) readLE<uint32_t> (in); readLE<uint32_t> (in); readLE<uint16_t> (in);
            bits = readLE<uint16_t> (in);
            if (size > 16) in.seekg (size - 16, std::ios::cur);
            if (format == 0xFFFE) format = bits == 32 ? 3 : 1;   // WAVE_FORMAT_EXTENSIBLE (cas simple)
        }
        else if (chunk == "data")
        {
            const size_t frames = size / (channels * (bits / 8));
            a.L.resize (frames); a.R.resize (frames);
            for (size_t f = 0; f < frames; ++f)
            {
                float v[2] = { 0, 0 };
                for (int c = 0; c < channels; ++c)
                {
                    float x = 0.0f;
                    if (format == 3 && bits == 32) x = readLE<float> (in);
                    else if (bits == 16) x = (float) readLE<int16_t> (in) / 32768.0f;
                    else if (bits == 24)
                    {
                        uint8_t b[3]; in.read (reinterpret_cast<char*> (b), 3);
                        int32_t s = (b[0] | (b[1] << 8) | (b[2] << 16)); if (s & 0x800000) s |= ~0xFFFFFF;
                        x = (float) s / 8388608.0f;
                    }
                    if (c < 2) v[c] = x;
                }
                a.L[f] = v[0];
                a.R[f] = channels > 1 ? v[1] : v[0];
            }
            return true;
        }
        else in.seekg (size + (size & 1), std::ios::cur);
    }
    return false;
}

bool writeWav (const std::string& path, const Audio& a)
{
    std::ofstream out (path, std::ios::binary);
    if (! out) return false;
    const uint32_t frames = (uint32_t) a.L.size(), dataSize = frames * 2 * 4;
    auto w32 = [&] (uint32_t v) { out.write (reinterpret_cast<const char*> (&v), 4); };
    auto w16 = [&] (uint16_t v) { out.write (reinterpret_cast<const char*> (&v), 2); };
    out.write ("RIFF", 4); w32 (36 + dataSize); out.write ("WAVE", 4);
    out.write ("fmt ", 4); w32 (16); w16 (3); w16 (2); w32 ((uint32_t) a.sampleRate);
    w32 ((uint32_t) a.sampleRate * 8); w16 (8); w16 (32);
    out.write ("data", 4); w32 (dataSize);
    for (uint32_t f = 0; f < frames; ++f) { out.write (reinterpret_cast<const char*> (&a.L[f]), 4); out.write (reinterpret_cast<const char*> (&a.R[f]), 4); }
    return true;
}
} // namespace

int main (int argc, char** argv)
{
    using namespace solaris;
    if (argc < 3)
    {
        std::cerr << "usage: solaris_render in.wav out.wav [--bpm 120] [--auto | --preset N] [--source groupe] [--set id=val]\n";
        return 1;
    }

    Audio in;
    if (! readWav (argv[1], in)) { std::cerr << "Lecture impossible: " << argv[1] << "\n"; return 1; }

    double bpm = 120.0;
    bool autoMode = false, matchLevel = false;
    double analyseFrom = 0.0, analyseSeconds = 12.0;
    Settings s = defaultSettings();
    std::vector<std::pair<std::string, float>> overrides;

    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--bpm" && i + 1 < argc) bpm = std::atof (argv[++i]);
        else if (a == "--auto") autoMode = true;
        else if (a == "--match-level") matchLevel = true;
        else if (a == "--analyse-from" && i + 1 < argc) analyseFrom = std::atof (argv[++i]);
        else if (a == "--analyse-seconds" && i + 1 < argc) analyseSeconds = std::atof (argv[++i]);
        else if (a == "--preset" && i + 1 < argc) s = presetSettings (std::atoi (argv[++i]));
        else if (a == "--source" && i + 1 < argc) { if (std::string (argv[++i]) == "groupe") s.compSource = 1; }
        else if (a == "--set" && i + 1 < argc)
        {
            const std::string kv = argv[++i];
            const auto eq = kv.find ('=');
            if (eq != std::string::npos) overrides.emplace_back (kv.substr (0, eq), (float) std::atof (kv.c_str() + eq + 1));
        }
    }

    const int block = 512;
    const size_t frames = in.L.size();

    if (autoMode)
    {
        // Passe d'analyse : on fait jouer le son dans un moteur "neutre", comme dans le DAW
        Analyzer analyzer;
        analyzer.prepare (in.sampleRate, (float) analyseSeconds);
        analyzer.start ((float) analyseSeconds);
        Engine probe;
        probe.prepare (in.sampleRate, block);
        std::vector<float> l (block), r (block);
        const size_t startFrame = std::min (frames, (size_t) (analyseFrom * in.sampleRate));
        for (size_t pos = startFrame; pos < frames && ! analyzer.isDone(); pos += block)
        {
            const int n = (int) std::min ((size_t) block, frames - pos);
            std::copy_n (in.L.begin() + (long) pos, n, l.begin());
            std::copy_n (in.R.begin() + (long) pos, n, r.begin());
            probe.process (l.data(), r.data(), n, s, bpm, &analyzer);
        }
        analyzer.finishNow();
        const Report rep = analyzer.makeReport();

        std::printf ("ANALYSE: %.1f s | niveaux P10 %.1f P50 %.1f P95 %.1f dB | dynamique %.1f dB | crete %.1f dB\n",
                     rep.seconds, rep.l10, rep.l50, rep.l95, rep.dynamicRange, rep.peakDb);
        std::printf ("ANALYSE: attaques %.2f/s montee %.0f ms intervalle %.0f ms | %s | basses %.2f | side/mid %.1f dB\n",
                     rep.onsetRate, rep.riseMs, rep.intervalMs, rep.percussive ? "percussif" : "tenu",
                     rep.lowRatio, rep.sideToMidDb);
        std::printf ("ANALYSE: octaves (rel. 600-1200 Hz):");
        for (int b = 0; b < 7; ++b) std::printf (" %.1f", rep.bandRel[b]);
        std::printf ("\n");

        std::string summary;
        s = suggestSettings (rep, s, Scope::All, bpm, summary);
        std::printf ("REGLAGES: %s\n", summary.c_str());
    }

    for (auto& kv : overrides)
        if (! setById (s, kv.first.c_str(), kv.second))
            std::cerr << "Parametre inconnu: " << kv.first << "\n";

    for (auto& d : paramDefs())
    {
        const float v = s.*(d.field);
        if (d.kind == Kind::Float)       std::printf ("  %-14s %s\n", d.id, formatValue (d.unit, v).c_str());
        else if (d.kind == Kind::Bool)   std::printf ("  %-14s %s\n", d.id, v > 0.5f ? "on" : "off");
        else                             std::printf ("  %-14s %s\n", d.id, d.choices[(size_t) std::lround (v)].c_str());
    }
    const ReverbTimes t = computeReverbTimes (s, bpm);
    std::printf ("REVERB @%.1f BPM: decay %.2f s, predelay %.2f ms (total %.2f s)\n", bpm, t.decaySec, t.predelayMs, t.totalSec);

    // Rendu (latence compensee, queue de reverb ajoutee)
    Engine engine;
    engine.prepare (in.sampleRate, block);
    const int latency = Engine::getLatency();
    const size_t tail = (size_t) (in.sampleRate * 2);
    const size_t total = frames + tail + (size_t) latency;

    Audio out;
    out.sampleRate = in.sampleRate;
    out.L.assign (total, 0.0f);
    out.R.assign (total, 0.0f);
    for (size_t f = 0; f < frames; ++f) { out.L[f] = in.L[f]; out.R[f] = in.R[f]; }

    for (size_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min ((size_t) block, total - pos);
        engine.process (out.L.data() + pos, out.R.data() + pos, n, s, bpm);
    }
    out.L.erase (out.L.begin(), out.L.begin() + latency);
    out.R.erase (out.R.begin(), out.R.begin() + latency);

    if (matchLevel)
    {
        // Meme niveau RMS que l'original, pour comparer a l'oreille sans etre trompe par le volume
        double ein = 0.0, eout = 0.0;
        for (size_t f = 0; f < frames; ++f)
        {
            ein  += (double) in.L[f] * in.L[f] + (double) in.R[f] * in.R[f];
            eout += (double) out.L[f] * out.L[f] + (double) out.R[f] * out.R[f];
        }
        const float g = (float) std::sqrt (ein / std::max (1.0e-12, eout));
        for (size_t f = 0; f < out.L.size(); ++f) { out.L[f] *= g; out.R[f] *= g; }
        std::printf ("NIVEAU: correction %.2f dB pour egaliser avec l'original\n", gainToDb (g));
    }

    if (! writeWav (argv[2], out)) { std::cerr << "Ecriture impossible\n"; return 1; }
    std::printf ("OK -> %s\n", argv[2]);
    return 0;
}
