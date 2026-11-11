#include "PluginProcessor.h"
#include <cmath>

using namespace juce;

namespace
{
    inline float polyBlep (float t, float dt)
    {
        if (t < dt)         { t /= dt;               return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
        return 0.0f;
    }
}

GoaFXProcessor::GoaFXProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

AudioProcessorValueTreeState::ParameterLayout GoaFXProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
        StringArray { "Riser", "Downlifter", "Impact", "Laser Up", "Laser Down", "Alien", "Whoosh", "Drone" }, 0));

    auto addFloat = [&layout] (const char* id, const char* name, float mn, float mx, float def, float skew = 1.0f)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                    NormalisableRange<float> (mn, mx, 0.001f, skew), def));
    };

    addFloat ("length",   "Length (s)",        0.2f,   16.f,    4.f,   0.5f);
    addFloat ("range",    "Pitch Range (st)",  0.f,    48.f,    24.f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "wave", 1 }, "Osc Wave",
        StringArray { "Saw", "Square", "Sine" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "unison", 1 }, "Unison Voices", 1, 7, 5));
    addFloat ("detune",   "Detune (cents)",    0.f,    50.f,    12.f);
    addFloat ("noise",    "Noise Level",       0.f,    1.f,     0.5f);
    addFloat ("osc",      "Osc Level",         0.f,    1.f,     0.5f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ftype", 1 }, "Filter Type",
        StringArray { "Lowpass", "Highpass", "Bandpass" }, 0));
    addFloat ("cutoff",   "Cutoff Max (Hz)",   200.f,  18000.f, 12000.f, 0.3f);
    addFloat ("res",      "Resonance",         0.f,    0.95f,   0.35f);
    addFloat ("drive",    "Drive",             0.f,    1.f,     0.25f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ddiv", 1 }, "Delay Time",
        StringArray { "1/16", "1/8", "1/8 dotted", "1/4", "1/2" }, 2));
    addFloat ("dmix",     "Delay Mix",         0.f,    1.f,     0.25f);
    addFloat ("dfb",      "Delay Feedback",    0.f,    0.95f,   0.5f);
    addFloat ("rmix",     "Reverb Mix",        0.f,    1.f,     0.3f);
    addFloat ("rsize",    "Reverb Size",       0.f,    1.f,     0.75f);
    addFloat ("vol",      "Volume (dB)",       -24.f,  6.f,     -6.f);

    return layout;
}

void GoaFXProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const int size = (int) (sr * 3.0) + 8;
    delayL.assign ((size_t) size, 0.f);
    delayR.assign ((size_t) size, 0.f);
    delayPos = 0;
    reverb.reset();
    reverb.setSampleRate (sr);
    for (auto& v : voices) v = Voice();
}

bool GoaFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void GoaFXProcessor::readParams()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    cur.mode       = (int) get ("mode");
    cur.length     = get ("length");
    cur.range      = get ("range");
    cur.wave       = (int) get ("wave");
    cur.unison     = (int) get ("unison");
    cur.detune     = get ("detune");
    cur.noise      = get ("noise");
    cur.osc        = get ("osc");
    cur.filterType = (int) get ("ftype");
    cur.cutoffMax  = get ("cutoff");
    cur.res        = get ("res");
    cur.drive      = get ("drive");
    cur.delayDiv   = (int) get ("ddiv");
    cur.delayMix   = get ("dmix");
    cur.delayFb    = get ("dfb");
    cur.reverbMix  = get ("rmix");
    cur.reverbSize = get ("rsize");
    cur.volume     = get ("vol");
}

void GoaFXProcessor::noteOn (int note, float vel)
{
    Voice* slot = nullptr;
    for (auto& v : voices) if (! v.active) { slot = &v; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.age > slot->age) slot = &v;
    }
    *slot = Voice();
    slot->active = true;
    slot->note = note;
    slot->vel = vel;
    for (int i = 0; i < 7; ++i) slot->phase[i] = rng.nextDouble();
    slot->shCut = rng.nextFloat();
}

void GoaFXProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && ! v.released) v.released = true;
}

float GoaFXProcessor::renderVoice (Voice& v)
{
    const double lenSamp = jmax (0.05, (double) cur.length) * sr;
    const float prog = (float) jmin (1.0, v.age / lenSamp);
    const float r = cur.range;
    const float pi = MathConstants<float>::pi;

    float semis = 0.f, amp = 1.f, fcNorm = 1.f;
    bool sustained = false;

    switch (cur.mode)
    {
        case 0: // Riser
            semis = -r * (1.f - prog);
            amp = 0.2f + 0.8f * prog;
            fcNorm = std::pow (prog, 1.5f);
            break;
        case 1: // Downlifter
            semis = r * (1.f - prog);
            amp = 1.f - 0.9f * prog;
            fcNorm = 1.f - std::pow (prog, 0.7f);
            break;
        case 2: // Impact
            semis = -r * (1.f - std::exp (-prog * 10.f));
            amp = std::exp (-prog * 5.f);
            fcNorm = std::exp (-prog * 6.f);
            break;
        case 3: // Laser up
            semis = -r * (1.f - prog) * (1.f - prog);
            amp = std::exp (-prog * 3.5f);
            fcNorm = 0.7f + 0.3f * prog;
            break;
        case 4: // Laser down
            semis = r * (1.f - prog) * (1.f - prog);
            amp = std::exp (-prog * 3.5f);
            fcNorm = 0.7f + 0.3f * prog;
            break;
        case 5: // Alien (random steps, sustained)
        {
            sustained = true;
            v.stepCounter -= 1.0;
            if (v.stepCounter <= 0.0)
            {
                static const int steps[8] = { 0, 1, 5, 7, 8, 12, 13, 17 };
                v.stepCounter += stepLenSamples;
                v.shPitch = (float) steps[rng.nextInt (8)];
                v.shCut = rng.nextFloat();
            }
            const float within = (float) (1.0 - v.stepCounter / stepLenSamples);
            semis = v.shPitch;
            fcNorm = 0.25f + 0.75f * v.shCut;
            amp = 0.4f + 0.6f * std::exp (-within * 3.f);
            amp *= jmin (1.f, (float) (v.age / (0.02 * sr)));
            break;
        }
        case 6: // Whoosh
        {
            const float tri = prog < 0.5f ? prog * 2.f : 2.f - 2.f * prog;
            fcNorm = tri;
            amp = std::sin (pi * prog);
            break;
        }
        default: // Drone (sustained)
        {
            sustained = true;
            v.lfoPhase += 0.15 / sr;
            if (v.lfoPhase >= 1.0) v.lfoPhase -= 1.0;
            amp = jmin (1.f, (float) (v.age / lenSamp));
            fcNorm = 0.5f + 0.4f * std::sin (2.f * pi * (float) v.lfoPhase);
            break;
        }
    }

    // end of voice handling
    if (sustained)
    {
        if (v.released)
        {
            const float relGain = 1.f - (float) (v.relAge / (1.2 * sr));
            v.relAge += 1.0;
            if (relGain <= 0.f) { v.active = false; return 0.f; }
            amp *= relGain;
        }
    }
    else if (v.age > lenSamp)
    {
        const float fade = 1.f - (float) ((v.age - lenSamp) / (0.05 * sr));
        if (fade <= 0.f) { v.active = false; return 0.f; }
        amp *= fade;
    }

    // oscillators
    const double baseFreq = 440.0 * std::pow (2.0, ((double) v.note - 69.0 + (double) semis) / 12.0);
    float oscSum = 0.f;
    if (cur.osc > 0.001f)
    {
        const int nu = jlimit (1, 7, cur.unison);
        for (int i = 0; i < nu; ++i)
        {
            const float pos = nu > 1 ? ((float) i - (float) (nu - 1) * 0.5f) / ((float) (nu - 1) * 0.5f) : 0.f;
            const double f = baseFreq * std::pow (2.0, (double) (pos * cur.detune) / 1200.0);
            const float dt = (float) jmin (0.45, f / sr);
            double& ph = v.phase[i];
            ph += f / sr;
            if (ph >= 1.0) ph -= 1.0;
            const float t = (float) ph;
            float s;
            if (cur.wave == 0)      s = 2.f * t - 1.f - polyBlep (t, dt);
            else if (cur.wave == 1) s = (t < 0.5f ? 1.f : -1.f) + polyBlep (t, dt) - polyBlep (std::fmod (t + 0.5f, 1.f), dt);
            else                    s = std::sin (2.f * pi * t);
            oscSum += s;
        }
        oscSum *= cur.osc / std::sqrt ((float) nu);
    }

    const float noiseSample = (rng.nextFloat() * 2.f - 1.f) * cur.noise;
    float x = oscSum + noiseSample * 0.7f;

    // state variable filter (TPT)
    const float fcMin = 60.f;
    const float fcMax = jmax (fcMin * 2.f, cur.cutoffMax);
    float fc = fcMin * std::pow (fcMax / fcMin, jlimit (0.f, 1.f, fcNorm));
    fc = jlimit (20.f, (float) (sr * 0.45), fc);
    const float g = std::tan (pi * fc / (float) sr);
    const float k = 2.f * (1.f - jlimit (0.f, 0.97f, cur.res));
    const float a1 = 1.f / (1.f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = x - v.ic2;
    const float v1 = a1 * v.ic1 + a2 * v3;
    const float v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
    v.ic1 = 2.f * v1 - v.ic1;
    v.ic2 = 2.f * v2 - v.ic2;
    float y;
    if (cur.filterType == 0)      y = v2;
    else if (cur.filterType == 1) y = x - k * v1 - v2;
    else                          y = v1 * k;

    // drive
    const float dr = 1.f + cur.drive * 10.f;
    y = std::tanh (y * dr) * (0.9f / std::sqrt (dr));

    v.age += 1.0;
    return y * amp * (0.5f + 0.5f * v.vel);
}

void GoaFXProcessor::renderRange (float* out, int start, int end)
{
    for (int i = start; i < end; ++i)
    {
        float s = 0.f;
        for (auto& v : voices)
            if (v.active) s += renderVoice (v);
        out[i] += s;
    }
}

void GoaFXProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    readParams();

    bpm = 145.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = jlimit (40.0, 250.0, *b);
    stepLenSamples = 60.0 / bpm / 4.0 * sr;

    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int ev = jlimit (0, n, meta.samplePosition);
        renderRange (L, pos, ev);
        pos = ev;
        const auto m = meta.getMessage();
        if (m.isNoteOn())       noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) v.active = false;
    }
    renderRange (L, pos, n);

    const float vol = Decibels::decibelsToGain (cur.volume);

    // ping-pong delay
    static const float beatMult[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
    const int size = (int) delayL.size();
    int ds = (int) (60.0 / bpm * beatMult[jlimit (0, 4, cur.delayDiv)] * sr);
    ds = jlimit (1, size - 1, ds);
    for (int i = 0; i < n; ++i)
    {
        const float in = L[i];
        const int rp = (delayPos - ds + size) % size;
        const float dl = delayL[(size_t) rp];
        const float dR = delayR[(size_t) rp];
        delayL[(size_t) delayPos] = in + dR * cur.delayFb;
        delayR[(size_t) delayPos] = dl * cur.delayFb;
        L[i] = in + dl * cur.delayMix;
        R[i] = in + dR * cur.delayMix;
        delayPos = (delayPos + 1) % size;
    }

    // reverb
    Reverb::Parameters rp;
    rp.roomSize = cur.reverbSize;
    rp.damping = 0.4f;
    rp.wetLevel = cur.reverbMix * 0.6f;
    rp.dryLevel = 1.0f - cur.reverbMix * 0.3f;
    rp.width = 1.0f;
    rp.freezeMode = 0.f;
    reverb.setParameters (rp);
    reverb.processStereo (L, R, n);

    buffer.applyGain (vol);
}

AudioProcessorEditor* GoaFXProcessor::createEditor()
{
    return new GenericAudioProcessorEditor (*this);
}

void GoaFXProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GoaFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GoaFXProcessor();
}
