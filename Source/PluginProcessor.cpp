#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        // c_behav: 0 Electro 1 Opto   c_char: 0 Warm 1 Smooth   order: 0 COMP > VOX  1 VOX > COMP
        static const std::vector<Preset> list =
        {
            { "Init (Transparent)",     {} },
            { "Vocal Gentle Opto",      { { "c_thresh", -18 }, { "c_ratio", 2.5f }, { "c_attack", 12 }, { "c_release", 180 }, { "c_behav", 1 }, { "c_gain", 2 } } },
            { "Vocal Upfront",          { { "c_thresh", -22 }, { "c_ratio", 4 }, { "c_attack", 8 }, { "c_release", 120 }, { "c_gain", 4 }, { "v_comp", -8 }, { "v_gain", -1 } } },
            { "Rap Vocal Loud",         { { "c_thresh", -24 }, { "c_ratio", 6 }, { "c_attack", 3 }, { "c_release", 80 }, { "c_gain", 6 }, { "v_comp", -12 }, { "v_gain", -1 } } },
            { "Podcast Leveler",        { { "v_gate", -55 }, { "v_comp", -14 }, { "v_gain", -1.5f }, { "c_thresh", -20 }, { "c_ratio", 3 }, { "c_behav", 1 }, { "c_char", 1 }, { "c_gain", 3 } } },
            { "Upward Lift (Gain)",     { { "c_thresh", -26 }, { "c_ratio", 0.75f }, { "c_attack", 20 }, { "c_release", 200 }, { "c_char", 1 }, { "c_gain", -3 } } },
            { "Electro Loud",           { { "c_thresh", -28 }, { "c_ratio", 8 }, { "c_attack", 2 }, { "c_release", 60 }, { "c_behav", 0 }, { "c_gain", 8 } } },
            { "Opto Smooth Warm",       { { "c_thresh", -20 }, { "c_ratio", 3 }, { "c_attack", 20 }, { "c_release", 300 }, { "c_behav", 1 }, { "c_char", 0 }, { "c_gain", 3 } } },
            { "Gate + Vox Comp",        { { "order", 1 }, { "v_gate", -48 }, { "v_comp", -10 }, { "v_gain", -1 } } },
            { "Background Vocals",      { { "c_thresh", -26 }, { "c_ratio", 5 }, { "c_attack", 25 }, { "c_release", 250 }, { "c_char", 1 }, { "c_gain", 4 }, { "v_comp", -6 }, { "v_gain", -3 } } },
            { "Parallel Punch",         { { "c_thresh", -34 }, { "c_ratio", 12 }, { "c_attack", 1 }, { "c_release", 90 }, { "c_behav", 0 }, { "c_gain", 6 }, { "c_mix", 45 } } },
            { "Vox Only Leveler",       { { "c_on", 0 }, { "v_comp", -15 }, { "v_gain", -1 } } },
        };
        return list;
    }
}

AudioProcessorValueTreeState::ParameterLayout VocalCompProcessor::createLayout()
{
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto db = A().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return String (x, 1); });
    auto gateDb = A().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return x <= -79.9f ? String ("Off") : String (x, 1); });
    auto ms = A().withLabel ("ms").withStringFromValueFunction ([] (float x, int) { return x < 10 ? String (x, 1) : String (roundToInt (x)); });
    auto pct = A().withLabel ("%").withStringFromValueFunction ([] (float x, int) { return String (x, 1); });
    auto ratio = A().withStringFromValueFunction ([] (float x, int) { return String (x, 2); });
    auto f = [&] (const char* id, const char* n, NormalisableRange<float> r, float d, A a) { v.push_back (std::make_unique<F> (ParameterID { id, 1 }, n, r, d, a)); };
    auto b = [&] (const char* id, const char* n, bool d) { v.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, n, d)); };
    auto c = [&] (const char* id, const char* n, const StringArray& s, int d) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, n, s, d)); };

    // ratio: 0.5 .. 1.0 on the bottom 35 % of the knob travel, 1 .. 50 (log) above
    NormalisableRange<float> ratioRange (0.5f, 50.0f,
        [] (float, float, float n) { return n < 0.3889f ? 0.5f + 0.5f * n / 0.3889f : std::pow (50.0f, (n - 0.3889f) / 0.6111f); },
        [] (float, float, float r) { return r < 1.0f ? (r - 0.5f) / 0.5f * 0.3889f : 0.3889f + 0.6111f * std::log (r) / std::log (50.0f); },
        [] (float, float, float r) { return jlimit (0.5f, 50.0f, std::round (r * 100.0f) / 100.0f); });
    NormalisableRange<float> atkRange (0.5f, 500.0f); atkRange.setSkewForCentre (16.0f);
    NormalisableRange<float> relRange (5.0f, 5000.0f); relRange.setSkewForCentre (160.0f);

    b ("c_on", "Comp On", true);
    f ("c_thresh", "Comp Threshold", { -60, 0, 0.1f }, 0, db);
    f ("c_ratio", "Comp Ratio", ratioRange, 1.0f, ratio);
    f ("c_gain", "Comp Gain", { -30, 30, 0.1f }, 0, db);
    f ("c_attack", "Comp Attack", atkRange, 16, ms);
    f ("c_release", "Comp Release", relRange, 160, ms);
    c ("c_arc", "Release Mode", { "Manual", "ARC" }, 1);
    c ("c_behav", "Behavior", { "Electro", "Opto" }, 0);
    c ("c_char", "Character", { "Warm", "Smooth" }, 0);
    f ("c_mix", "Comp Mix", { 0, 100, 0.1f }, 100, pct);
    f ("c_trim", "Comp Trim", { -18, 18, 0.1f }, 0, db);

    b ("v_on", "Vox On", true);
    f ("v_gate", "Vox Gate", { -80, 0, 0.1f }, -80, gateDb);
    f ("v_comp", "Vox Comp", { -36, 0, 0.1f }, 0, db);
    f ("v_gain", "Vox Gain (Ceiling)", { -36, 0, 0.1f }, 0, db);
    b ("v_link", "Vox Link Comp+Gain", false);
    c ("order", "Order", { "COMP > VOX", "VOX > COMP" }, 0);
    return { v.begin(), v.end() };
}

VocalCompProcessor::VocalCompProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            cache[r->getParameterID().toStdString()] = apvts.getRawParameterValue (r->getParameterID());
}

bool VocalCompProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet(), i = l.getMainInputChannelSet();
    return (o == AudioChannelSet::mono() || o == AudioChannelSet::stereo()) && (i == o || (i == AudioChannelSet::mono() && o == AudioChannelSet::stereo()));
}

void VocalCompProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = jmax (1, samplesPerBlock);
    setLatencySamples (0);
    comp.prepare (fs);
    vox.prepare (fs);
}

void VocalCompProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int total = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, total);
    for (int start = 0; start < total; start += maxBlock)
    {
        AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, jmin (maxBlock, total - start));
        processChunk (chunk);
    }
}

void VocalCompProcessor::processChunk (AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int numCh = jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (n == 0 || numCh == 0) return;

    vc::CompSection::Params cp;
    cp.thresh = p ("c_thresh"); cp.ratio = p ("c_ratio"); cp.gainDb = p ("c_gain");
    cp.attackMs = p ("c_attack"); cp.releaseMs = p ("c_release");
    cp.arc = p ("c_arc") > 0.5f; cp.behaviour = (int) p ("c_behav"); cp.character = (int) p ("c_char");
    cp.mix = p ("c_mix") * 0.01f; cp.trimDb = p ("c_trim");
    comp.setParams (cp);
    vc::VoxSection::Params vp; vp.gate = p ("v_gate"); vp.comp = p ("v_comp"); vp.ceilingDb = p ("v_gain");
    vox.setParams (vp);
    const bool cOn = p ("c_on") > 0.5f, vOn = p ("v_on") > 0.5f, voxFirst = p ("order") > 0.5f;

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    float ciL = 0, ciR = 0, coL = 0, coR = 0, viPk = 0, voL = 0, voR = 0, cChangeMin = 0, cChangeMax = 0, cLim = 0, vGr = 0, vLim = 0;

    auto runComp = [&] (float& l, float& r)
    {
        ciL = jmax (ciL, std::abs (l)); ciR = jmax (ciR, std::abs (r));
        if (cOn)
        {
            comp.process (l, r);
            const float ch = comp.gainChangeDb();
            cChangeMin = jmin (cChangeMin, ch); cChangeMax = jmax (cChangeMax, ch);
            cLim = jmax (cLim, comp.limiterDb());
        }
        coL = jmax (coL, std::abs (l)); coR = jmax (coR, std::abs (r));
    };
    auto runVox = [&] (float& l, float& r)
    {
        viPk = jmax (viPk, jmax (std::abs (l), std::abs (r)));
        if (vOn)
        {
            vox.process (l, r);
            vGr = jmax (vGr, vox.totalGrDb());
            vLim = jmax (vLim, vox.limiterDb());
        }
        voL = jmax (voL, std::abs (l)); voR = jmax (voR, std::abs (r));
    };

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = numCh > 1 ? R[i] : L[i];
        if (voxFirst) { runVox (l, r); runComp (l, r); }
        else          { runComp (l, r); runVox (l, r); }
        L[i] = l; if (numCh > 1) R[i] = r;
    }

    auto d = [] (float g) { return Decibels::gainToDecibels (g, -100.0f); };
    meters.cInL.store (d (ciL)); meters.cInR.store (d (ciR));
    meters.cOutL.store (d (coL)); meters.cOutR.store (d (coR));
    meters.cChange.store (cOn ? (-cChangeMin > cChangeMax ? cChangeMin : cChangeMax) : 0.0f);
    meters.cLimit.store (cOn ? cLim : 0.0f);
    meters.vEnergy.store (d (viPk));
    meters.vOutL.store (d (voL)); meters.vOutR.store (d (voR));
    meters.vGr.store (vOn ? vGr : 0.0f);
    meters.vLimit.store (vOn ? vLim : 0.0f);
}

//==============================================================================
int VocalCompProcessor::getNumPrograms() { return (int) presets().size(); }
const String VocalCompProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (presets()[(size_t) i].name) : String(); }

void VocalCompProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    auto set = [this] (const String& id, float v, bool normalised)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (normalised ? v : prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            set (r->getParameterID(), r->getDefaultValue(), true);
    for (auto& [id, val] : presets()[(size_t) index].values)
        set (id, val, false);
}

void VocalCompProcessor::getStateInformation (MemoryBlock& dest)
{
    auto s = apvts.copyState();
    s.setProperty ("program", currentProgram, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}

void VocalCompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = ValueTree::fromXml (*xml);
            currentProgram = (int) t.getProperty ("program", 0);
            apvts.replaceState (t);
        }
}

AudioProcessorEditor* VocalCompProcessor::createEditor() { return new VocalCompEditor (*this); }

#if ! VC_TEST_BUILD
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalCompProcessor(); }
#endif
