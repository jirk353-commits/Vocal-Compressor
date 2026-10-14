// Offline test runner for Vocal Compressor: DSP checks + UI screenshot.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }
static void setP (VocalCompProcessor& p, const String& id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
static std::vector<float> run (VocalCompProcessor& p, std::function<float (int)> in, int total, int block = 512)
{
    std::vector<float> out; out.reserve ((size_t) total);
    AudioBuffer<float> buf (2, block); MidiBuffer m;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, 0, n);
        for (int i = 0; i < n; ++i) { const float x = in (pos + i); b.setSample (0, i, x); b.setSample (1, i, x); }
        p.processBlock (b, m);
        for (int i = 0; i < n; ++i) out.push_back (b.getSample (0, i));
    }
    return out;
}
static float rms (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return (float) std::sqrt (s / (double) (b - a)); }
static float peakOf (const std::vector<float>& x, size_t a, size_t b) { float m = 0; for (size_t i = a; i < b; ++i) m = jmax (m, std::abs (x[i])); return m; }
static std::function<float (int)> sine (float f, float amp) { return [=] (int i) { return amp * (float) std::sin (MathConstants<double>::twoPi * f * i / 48000.0); }; }
static float gainDb (VocalCompProcessor& p, float f, float amp, int len = 96000)
{
    auto y = run (p, sine (f, amp), len);
    return Decibels::gainToDecibels (rms (y, (size_t) len / 2, y.size()) / (amp / std::sqrt (2.0f)));
}
static float harmonicDb (const std::vector<float>& x, double f)
{
    double re = 0, im = 0; const size_t n = x.size();
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * (double) i / (double) n);
        re += x[i] * w * std::cos (MathConstants<double>::twoPi * f * (double) i / 48000.0);
        im += x[i] * w * std::sin (MathConstants<double>::twoPi * f * (double) i / 48000.0);
    }
    return (float) Decibels::gainToDecibels (std::sqrt (re * re + im * im) / (double) n * 4.0, -200.0);
}
static float voiceSample (int i)
{
    static Random rnd (5);
    const double t = i / 48000.0;
    double v = 0;
    for (int h = 1; h <= 20; ++h) v += (std::exp (-std::pow ((180.0 * h - 700) / 500, 2)) + 0.2) / h * std::sin (MathConstants<double>::twoPi * 180.0 * h * t + h);
    const double syl = std::pow (std::max (0.0, std::sin (MathConstants<double>::pi * 3.0 * t)), 0.6) * (0.4 + 0.6 * std::abs (std::sin (0.7 * t)));
    return (float) (0.3 * v * syl) + 0.0015f * (rnd.nextFloat() * 2 - 1);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double fs = 48000;

    // ---- transparent defaults ---------------------------------------------
    {
        VocalCompProcessor p; p.prepareToPlay (fs, 512);
        bool flat = true;
        for (float f : { 60.f, 1000.f, 10000.f }) { const float g = gainDb (p, f, 0.3f, 48000); std::printf ("  defaults @%5.0f Hz: %+.3f dB\n", f, g); flat &= std::abs (g) < 0.05f; }
        check (flat, "defaults (ratio 1.00, VOX comp 0, gate off) are transparent");
    }

    // ---- COMP ratio, threshold, upward expansion ---------------------------
    {
        const float amp = 0.5f, peakDb = Decibels::gainToDecibels (amp);   // about -6 dBFS peaks
        for (float ratio : { 2.0f, 4.0f, 10.0f })
        {
            VocalCompProcessor p; p.prepareToPlay (fs, 512);
            setP (p, "c_thresh", -26); setP (p, "c_ratio", ratio); setP (p, "c_char", 1);
            const float g = gainDb (p, 1000, amp);
            const float expect = -(peakDb + 26.0f) * (1.0f - 1.0f / ratio);
            std::printf ("  ratio %5.2f: %+.1f dB (expected %+.1f), meter %.1f\n", ratio, g, expect, p.meters.cChange.load());
            check (std::abs (g - expect) < 1.5f, "COMP ratio " + String (ratio, 1) + ":1 at -26 dB threshold");
        }
        VocalCompProcessor p; p.prepareToPlay (fs, 512);
        setP (p, "c_thresh", -26); setP (p, "c_ratio", 0.7f); setP (p, "c_char", 1);
        const float amp2 = 0.25f;
        const float g = gainDb (p, 1000, amp2);
        const float expect = (Decibels::gainToDecibels (amp2) + 26.0f) * (1.0f / 0.7f - 1.0f);
        std::printf ("  ratio 0.70 (upward): %+.1f dB (expected about %+.1f), meter %+.1f\n", g, expect, p.meters.cChange.load());
        check (g > 3.0f && p.meters.cChange.load() > 3.0f, "ratio below 1 gives upward gain (blue meter)");
    }

    // ---- release behaviours -------------------------------------------------
    {
        auto recovery = [&] (int behaviour, bool arc, float thresh, int afterMs)
        {
            VocalCompProcessor p; p.prepareToPlay (fs, 512);
            setP (p, "c_thresh", thresh); setP (p, "c_ratio", 4); setP (p, "c_release", 160); setP (p, "c_arc", arc ? 1.0f : 0.0f); setP (p, "c_behav", (float) behaviour);
            run (p, [] (int i) { return (i < 48000 ? 0.5f : 0.02f) * (float) std::sin (i * 0.13); }, 48000 + afterMs * 48, 48);
            return p.meters.cChange.load();
        };
        const float elLight = recovery (0, false, -9, 60), opLight = recovery (1, false, -9, 60);
        const float elHeavy = recovery (0, false, -30, 100), opHeavy = recovery (1, false, -30, 100);
        const float arc = recovery (1, true, -30, 100);
        std::printf ("  light GR (<3 dB), 60 ms after: Electro %.2f dB, Opto %.2f dB\n", elLight, opLight);
        std::printf ("  heavy GR, 100 ms after: Electro %.2f dB, Opto %.2f dB, Opto+ARC %.2f dB\n", elHeavy, opHeavy, arc);
        check (elLight > opLight, "Electro: release speeds up near 0 dB (Opto puts on the brakes)");
        check (elHeavy < opHeavy, "Electro: slower, leveler-like release above 3 dB GR (Opto faster)");
        check (std::abs (arc - opHeavy) > 0.05f, "ARC changes the release");
    }

    // ---- character -----------------------------------------------------------
    {
        float h[2];
        for (int c = 0; c < 2; ++c)
        {
            VocalCompProcessor p; p.prepareToPlay (fs, 512);
            setP (p, "c_thresh", -30); setP (p, "c_ratio", 6); setP (p, "c_char", (float) c);
            auto y = run (p, sine (100, 0.6f), 96000);
            std::vector<float> tail (y.end() - 32768, y.end());
            h[c] = jmax (harmonicDb (tail, 200), harmonicDb (tail, 300)) - harmonicDb (tail, 100);
        }
        std::printf ("  low harmonics under heavy GR: Warm %.1f dBc, Smooth %.1f dBc\n", h[0], h[1]);
        check (h[0] > h[1] + 10.0f && h[0] > -50.0f, "Warm adds low-frequency harmonics, Smooth stays clean");
    }

    // ---- limiter, mix, trim ----------------------------------------------------
    {
        VocalCompProcessor p; p.prepareToPlay (fs, 512);
        setP (p, "c_gain", 18);
        auto y = run (p, sine (300, 0.5f), 48000);
        const float pk = peakOf (y, 0, y.size());
        std::printf ("  +18 dB into the limiter: peak %.4f, limiter %.1f dB\n", pk, p.meters.cLimit.load());
        check (pk <= 1.0001f && p.meters.cLimit.load() > 6.0f, "COMP limiter holds 0 dBFS");
        setP (p, "c_gain", 0); setP (p, "c_trim", -6);
        check (std::abs (gainDb (p, 1000, 0.2f) + 6.0f) < 0.1f, "Trim is post-limiter output level");
        setP (p, "c_trim", 0); setP (p, "c_thresh", -40); setP (p, "c_ratio", 20); setP (p, "c_char", 1);
        const float wet = gainDb (p, 1000, 0.3f);
        setP (p, "c_mix", 50);
        const float half = gainDb (p, 1000, 0.3f);
        std::printf ("  mix 100%%: %+.1f dB, mix 50%%: %+.1f dB\n", wet, half);
        check (half > wet + 3.0f, "Mix blends the dry signal back");
    }

    // ---- VOX: upward gain, ceiling, gate -------------------------------------------
    {
        VocalCompProcessor p; p.prepareToPlay (fs, 512);
        setP (p, "c_on", 0);
        auto dry = run (p, voiceSample, 96000);
        const float dryRms = rms (dry, 24000, dry.size());
        setP (p, "v_comp", -18);
        auto y = run (p, voiceSample, 96000);
        const float lift = Decibels::gainToDecibels (rms (y, 24000, y.size()) / dryRms);
        const float pk = peakOf (y, 0, y.size());
        std::printf ("  VOX comp -18: level %+.1f dB, peak %.3f, total GR %.1f dB\n", lift, pk, p.meters.vGr.load());
        check (lift > 3.0f && pk <= 1.0001f, "lowering VOX COMP raises the level and never clips");
        setP (p, "v_gain", -6);
        auto y2 = run (p, voiceSample, 96000);
        const float pk2 = peakOf (y2, 0, y2.size());
        std::printf ("  VOX gain -6: peak %.3f (ceiling %.3f)\n", pk2, Decibels::decibelsToGain (-6.0f));
        check (pk2 <= Decibels::decibelsToGain (-6.0f) * 1.0001f, "VOX GAIN is the output ceiling");
    }
    {
        VocalCompProcessor p; p.prepareToPlay (fs, 512);
        setP (p, "v_gate", -40);
        const float noise = gainDb (p, 3000, 0.003f);     // -50 dBFS "noise"
        const float voice = gainDb (p, 300, 0.3f);
        std::printf ("  gate -40: quiet noise %+.1f dB, loud vocal %+.2f dB\n", noise, voice);
        check (noise < -15.0f && std::abs (voice) < 0.3f, "gate pulls the noise down, leaves the vocal");
    }

    // ---- order, presets, state ------------------------------------------------------
    {
        VocalCompProcessor p; p.prepareToPlay (fs, 333);
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto y = run (p, voiceSample, 48000, 333);
            float pk = 0; bool fin = true; for (auto v : y) { fin &= std::isfinite (v); pk = jmax (pk, std::abs (v)); }
            std::printf ("  preset %-22s comp %+5.1f dB  vox GR %4.1f  peak %.3f\n", p.getProgramName (i).toRawUTF8(), p.meters.cChange.load(), p.meters.vGr.load(), pk);
            ok &= fin && pk <= 1.0001f;
        }
        check (ok, "all presets stable, nothing above 0 dBFS");
        VocalCompProcessor a, b; a.setCurrentProgram (5);
        MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.getCurrentProgram() == 5 && std::abs (b.value ("c_ratio") - 0.75f) < 0.01f, "state save/restore");
    }

    if (argc > 1)
    {
        VocalCompProcessor p; p.prepareToPlay (fs, 512);
        setP (p, "c_thresh", -22); setP (p, "c_ratio", 3.5f); setP (p, "c_gain", 3); setP (p, "v_gate", -52); setP (p, "v_comp", -9.5f); setP (p, "v_gain", -1.5f);
        if (argc > 3) { setP (p, "c_ratio", 0.7f); setP (p, "c_thresh", -30); setP (p, "c_gain", -2); }
        int pos = 0;
        run (p, [&] (int) { return voiceSample (pos++); }, 48000);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (vcui::MainPanel::W, vcui::MainPanel::H);
        for (int k = 0; k < 20; ++k) { run (p, [&] (int) { const int i = pos++; return voiceSample (i) + 0.12f * (float) std::sin (i * 0.05); }, 1600); MessageManager::getInstance()->runDispatchLoopUntil (35); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, argc > 2 ? (float) std::atof (argv[2]) : 1.0f);
        File out (argv[1]); out.deleteFile(); FileOutputStream os (out);
        check (PNGImageFormat().writeImageToStream (img, os), "wrote screenshot");
    }
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
