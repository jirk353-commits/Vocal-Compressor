#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace vc
{
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-7f)); }
inline float coefMs (float ms, double fs) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, (double) ms) * 0.001 * fs)); }

/** Zero-latency peak limiter: instant attack, smooth release. Output never passes the ceiling. */
struct Limiter
{
    float g = 1.0f, rel = 0.999f;
    void prepare (double fs) { g = 1.0f; rel = coefMs (60.0f, fs); }
    void process (float& l, float& r, float ceiling) noexcept
    {
        const float pk = std::max (std::abs (l), std::abs (r));
        const float need = pk > ceiling ? ceiling / pk : 1.0f;
        g = std::min (rel * g + (1.0f - rel), need);
        l *= g; r *= g;
    }
    float grDb() const noexcept { return -gainToDb (g); }
};

//==============================================================================
/** COMP section: classic warm compressor / upward expander with
    ARC auto-release, Electro/Opto release behaviour and Warm/Smooth character. */
class CompSection
{
public:
    struct Params
    {
        float thresh = 0, ratio = 1, gainDb = 0, attackMs = 16, releaseMs = 160;
        bool arc = true; int behaviour = 0; int character = 0;   // behaviour 0 Electro 1 Opto, character 0 Warm 1 Smooth
        float mix = 1, trimDb = 0;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate; env = cur = mem = 0;
        detRel = coefMs (12.0f, fs);
        memUp = 1.0f - coefMs (400.0f, fs); memDn = 1.0f - coefMs (1500.0f, fs);
        lowLp.prepare ({ fs, 512, 2 }); lowLp.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        lowLp.setCutoffFrequency (220.0f); lowLp.reset();
        limiter.prepare (fs);
    }
    void setParams (const Params& p) { prm = p; atk = coefMs (p.attackMs, fs); }

    /** gain computer: soft knee starting ~6 dB below threshold; negative = compression, positive = upward expansion */
    static float curve (float levelDb, float thresh, float ratio) noexcept
    {
        const float slope = 1.0f / ratio - 1.0f, x = levelDb - thresh;
        float g;
        if (x <= -6.0f)     g = 0.0f;
        else if (x < 6.0f)  g = slope * (x + 6.0f) * (x + 6.0f) / 24.0f;
        else                g = slope * x;
        return std::clamp (g, -60.0f, 18.0f);
    }

    void process (float& l, float& r) noexcept
    {
        const float dl = l, dr = r;
        const float pk = std::max (std::abs (l), std::abs (r));
        env = pk > env ? pk : detRel * env + (1.0f - detRel) * pk;
        const float lvl = gainToDb (env) + (prm.character == 0 ? 1.5f : 0.0f);
        const float target = std::abs (prm.ratio - 1.0f) < 0.005f ? 0.0f : curve (lvl, prm.thresh, prm.ratio);

        const float a = std::abs (cur);
        float coef;
        if (std::abs (target) > a) coef = atk;
        else
        {
            float ms = prm.releaseMs;
            if (prm.arc)
            {
                mem += a > 1.0f ? memUp * (1.0f - mem) : memDn * (0.0f - mem);
                ms *= 0.3f + 1.7f * mem;                                    // short peaks recover fast, sustained GR slowly
            }
            if (prm.behaviour == 0) ms *= a < 3.0f ? std::max (0.25f, a / 3.0f) : 1.0f + (a - 3.0f) / 6.0f;   // Electro
            else                    ms *= a < 3.0f ? 1.0f + 2.0f * (1.0f - a / 3.0f) : std::max (0.4f, 1.0f - (a - 3.0f) / 12.0f); // Opto
            coef = coefMs (ms, fs);
        }
        cur = coef * cur + (1.0f - coef) * target;
        const float g = dbToGain (cur);
        float wl = l * g, wr = r * g;

        {   // Warm: low-frequency harmonics that grow with gain reduction (only the added harmonics, no phase shift)
            float ll, hl, lr, hr;
            lowLp.processSample (0, wl, ll, hl);
            lowLp.processSample (1, wr, lr, hr);
            const float k = prm.character == 0 ? std::min (0.6f, std::max (0.0f, -cur - 0.5f) * 0.05f) : 0.0f;
            if (k > 0.0f) { wl += k * (warm (ll) - ll); wr += k * (warm (lr) - lr); }
        }

        const float og = dbToGain (prm.gainDb);
        l = (dl + prm.mix * (wl - dl)) * og;
        r = (dr + prm.mix * (wr - dr)) * og;
        limiter.process (l, r, 1.0f);
        const float t = dbToGain (prm.trimDb);
        l *= t; r *= t;
    }

    float gainChangeDb() const noexcept { return cur - limiter.grDb(); }
    float limiterDb() const noexcept { return limiter.grDb(); }

private:
    static float warm (float x) noexcept
    {
        constexpr float b = 0.25f; static const float tb = std::tanh (b), n = 1.0f / (1.0f - tb * tb);
        return (std::tanh (2.0f * x + b) - tb) * n * 0.5f;
    }
    double fs = 48000;
    Params prm;
    float env = 0, cur = 0, mem = 0, atk = 0, detRel = 0, memUp = 0, memDn = 0;
    juce::dsp::LinkwitzRileyFilter<float> lowLp;
    Limiter limiter;
};

//==============================================================================
/** VOX section: gentle downward-expander gate, one-fader compressor with automatic
    make-up (lower the fader = more level) and a ceiling limiter. */
class VoxSection
{
public:
    struct Params { float gate = -80, comp = 0, ceilingDb = 0; };

    void prepare (double sampleRate)
    {
        fs = sampleRate; gEnv = cEnv = gCur = cCur = mem = 0; hold = 0;
        gAtt = coefMs (0.2f, fs); gRel = coefMs (30.0f, fs);
        gOpen = coefMs (0.5f, fs); gClose = coefMs (120.0f, fs);
        cDetRel = coefMs (40.0f, fs); cAtt = coefMs (3.0f, fs);
        memUp = 1.0f - coefMs (500.0f, fs); memDn = 1.0f - coefMs (2000.0f, fs);
        holdSamples = (int) (0.025 * fs);
        limiter.prepare (fs);
    }
    void setParams (const Params& p) { prm = p; }

    void process (float& l, float& r) noexcept
    {
        const float pk = std::max (std::abs (l), std::abs (r));
        // ---- gate (downward expander 1:4, soft knee) --------------------------
        gEnv = pk > gEnv ? gAtt * gEnv + (1 - gAtt) * pk : gRel * gEnv + (1 - gRel) * pk;
        float gTarget = 0.0f;
        if (prm.gate > -79.9f)
        {
            constexpr float R = 4.0f, W = 6.0f;
            const float x = gainToDb (gEnv) - prm.gate;
            if (x < -W * 0.5f)      gTarget = (R - 1.0f) * x;
            else if (x < W * 0.5f)  gTarget = -(R - 1.0f) * (x - W * 0.5f) * (x - W * 0.5f) / (2.0f * W);
            gTarget = std::max (gTarget, -60.0f);
        }
        if (gTarget > gCur) { gCur = gOpen * gCur + (1 - gOpen) * gTarget; hold = holdSamples; }
        else if (hold > 0) --hold;
        else gCur = gClose * gCur + (1 - gClose) * gTarget;
        const float gg = dbToGain (gCur);
        l *= gg; r *= gg;

        // ---- compressor with auto make-up -------------------------------------
        float makeup = 0.0f, cTarget = 0.0f;
        if (prm.comp < -0.05f)
        {
            constexpr float R = 4.0f, slope = 1.0f - 1.0f / R;
            const float p2 = pk * gg;
            cEnv = p2 > cEnv ? p2 : cDetRel * cEnv + (1 - cDetRel) * p2;
            const float x = gainToDb (cEnv) - prm.comp;
            if (x > 3.0f)       cTarget = -slope * x;
            else if (x > -3.0f) cTarget = -slope * (x + 3.0f) * (x + 3.0f) / 12.0f;
            makeup = -prm.comp * slope;
        }
        const float a = -cCur;
        if (cTarget < cCur) cCur = cAtt * cCur + (1 - cAtt) * cTarget;
        else
        {
            mem += a > 1.0f ? memUp * (1 - mem) : memDn * (0 - mem);
            const float ms = (60.0f + 440.0f * mem) * (a < 3.0f ? 1.0f + (1.0f - a / 3.0f) : 1.0f);   // opto-style
            const float c = coefMs (ms, fs);
            cCur = c * cCur + (1 - c) * cTarget;
        }
        const float cg = dbToGain (cCur + makeup);
        l *= cg; r *= cg;
        limiter.process (l, r, dbToGain (prm.ceilingDb));
    }

    float totalGrDb() const noexcept { return -(gCur + cCur); }      // gate + compressor (positive)
    float limiterDb() const noexcept { return limiter.grDb(); }

private:
    double fs = 48000;
    Params prm;
    float gEnv = 0, cEnv = 0, gCur = 0, cCur = 0, mem = 0;
    float gAtt = 0, gRel = 0, gOpen = 0, gClose = 0, cDetRel = 0, cAtt = 0, memUp = 0, memDn = 0;
    int hold = 0, holdSamples = 1000;
    Limiter limiter;
};
} // namespace vc
