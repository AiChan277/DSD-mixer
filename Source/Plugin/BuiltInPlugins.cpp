#include "Plugin/BuiltInPlugins.h"
#include <cmath>

namespace dsd
{
    namespace
    {
        using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

        std::unique_ptr<juce::AudioParameterFloat> floatParam(const juce::String& id, const juce::String& name,
                                                              float min, float max, float def, float step,
                                                              const juce::String& unit, float skewCentre = 0.0f)
        {
            juce::NormalisableRange<float> range(min, max, step);
            if (skewCentre > min && skewCentre < max)
                range.setSkewForCentre(skewCentre);
            return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id, 1 }, name, range, def,
                                                               juce::AudioParameterFloatAttributes().withLabel(unit));
        }

        inline float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
        inline float gainToDb(float g) noexcept { return g > 1.0e-9f ? 20.0f * std::log10(g) : -180.0f; }

        // One-pole smoothing coefficient for a time constant in milliseconds.
        inline float timeCoeff(double sampleRate, float ms) noexcept
        {
            const double t = std::max(0.01, static_cast<double>(ms)) * 0.001;
            return static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * t)));
        }

        Layout gateLayout()
        {
            Layout l;
            l.add(floatParam("threshold", "Threshold", -80.0f, 0.0f, -50.0f, 0.1f, "dB"));
            l.add(floatParam("range", "Range", -80.0f, 0.0f, -80.0f, 0.1f, "dB"));
            l.add(floatParam("attack", "Attack", 0.1f, 50.0f, 1.0f, 0.1f, "ms", 5.0f));
            l.add(floatParam("hold", "Hold", 0.0f, 500.0f, 50.0f, 1.0f, "ms", 100.0f));
            l.add(floatParam("release", "Release", 5.0f, 2000.0f, 150.0f, 1.0f, "ms", 200.0f));
            return l;
        }

        Layout eqLayout()
        {
            Layout l;
            l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "hpfOn", 1 }, "HPF On", false));
            l.add(floatParam("hpfFreq", "HPF Freq", 20.0f, 400.0f, 80.0f, 1.0f, "Hz", 100.0f));
            l.add(floatParam("lowFreq", "Low Shelf Freq", 30.0f, 500.0f, 120.0f, 1.0f, "Hz", 150.0f));
            l.add(floatParam("lowGain", "Low Shelf Gain", -18.0f, 18.0f, 0.0f, 0.1f, "dB"));
            l.add(floatParam("mid1Freq", "Mid 1 Freq", 100.0f, 2000.0f, 400.0f, 1.0f, "Hz", 500.0f));
            l.add(floatParam("mid1Gain", "Mid 1 Gain", -18.0f, 18.0f, 0.0f, 0.1f, "dB"));
            l.add(floatParam("mid1Q", "Mid 1 Q", 0.3f, 8.0f, 1.0f, 0.01f, "", 1.0f));
            l.add(floatParam("mid2Freq", "Mid 2 Freq", 800.0f, 12000.0f, 3000.0f, 1.0f, "Hz", 3000.0f));
            l.add(floatParam("mid2Gain", "Mid 2 Gain", -18.0f, 18.0f, 0.0f, 0.1f, "dB"));
            l.add(floatParam("mid2Q", "Mid 2 Q", 0.3f, 8.0f, 1.0f, 0.01f, "", 1.0f));
            l.add(floatParam("highFreq", "High Shelf Freq", 2000.0f, 16000.0f, 8000.0f, 1.0f, "Hz", 6000.0f));
            l.add(floatParam("highGain", "High Shelf Gain", -18.0f, 18.0f, 0.0f, 0.1f, "dB"));
            l.add(floatParam("output", "Output", -18.0f, 18.0f, 0.0f, 0.1f, "dB"));
            return l;
        }

        Layout compressorLayout()
        {
            Layout l;
            l.add(floatParam("threshold", "Threshold", -60.0f, 0.0f, -18.0f, 0.1f, "dB"));
            l.add(floatParam("ratio", "Ratio", 1.0f, 20.0f, 3.0f, 0.1f, ":1", 4.0f));
            l.add(floatParam("attack", "Attack", 0.1f, 100.0f, 10.0f, 0.1f, "ms", 10.0f));
            l.add(floatParam("release", "Release", 10.0f, 2000.0f, 150.0f, 1.0f, "ms", 200.0f));
            l.add(floatParam("knee", "Knee", 0.0f, 12.0f, 6.0f, 0.1f, "dB"));
            l.add(floatParam("makeup", "Makeup", 0.0f, 24.0f, 0.0f, 0.1f, "dB"));
            return l;
        }

        // RBJ cookbook coefficient helpers (normalised by a0).
        void setCoefficients(BuiltInEq::Biquad& bq, double b0, double b1, double b2, double a0, double a1, double a2)
        {
            bq.b0 = b0 / a0; bq.b1 = b1 / a0; bq.b2 = b2 / a0;
            bq.a1 = a1 / a0; bq.a2 = a2 / a0;
        }

        void makeHighPass(BuiltInEq::Biquad& bq, double sr, double f)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos(w), alpha = std::sin(w) / (2.0 * 0.70710678);
            setCoefficients(bq, (1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
        }

        void makePeak(BuiltInEq::Biquad& bq, double sr, double f, double gainDb, double q)
        {
            const double A = std::pow(10.0, gainDb / 40.0);
            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
            setCoefficients(bq, 1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
        }

        void makeShelf(BuiltInEq::Biquad& bq, double sr, double f, double gainDb, bool high)
        {
            const double A = std::pow(10.0, gainDb / 40.0);
            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos(w), sw = std::sin(w);
            const double alpha = sw / 2.0 * std::sqrt(2.0); // shelf slope S = 1
            const double sq = 2.0 * std::sqrt(A) * alpha;
            if (high)
                setCoefficients(bq,
                                A * ((A + 1) + (A - 1) * cw + sq),
                                -2 * A * ((A - 1) + (A + 1) * cw),
                                A * ((A + 1) + (A - 1) * cw - sq),
                                (A + 1) - (A - 1) * cw + sq,
                                2 * ((A - 1) - (A + 1) * cw),
                                (A + 1) - (A - 1) * cw - sq);
            else
                setCoefficients(bq,
                                A * ((A + 1) - (A - 1) * cw + sq),
                                2 * A * ((A - 1) - (A + 1) * cw),
                                A * ((A + 1) - (A - 1) * cw - sq),
                                (A + 1) + (A - 1) * cw + sq,
                                -2 * ((A - 1) + (A + 1) * cw),
                                (A + 1) + (A - 1) * cw - sq);
        }
    } // namespace

    // =========================================================================
    // Factory
    // =========================================================================
    namespace BuiltInPlugins
    {
        juce::Array<juce::PluginDescription> getDescriptions()
        {
            juce::Array<juce::PluginDescription> out;
            for (auto id : { kGateId, kEqId, kCompressorId })
            {
                if (auto p = create(id))
                {
                    juce::PluginDescription d;
                    p->fillInPluginDescription(d);
                    out.add(d);
                }
            }
            return out;
        }

        bool isBuiltIn(const juce::PluginDescription& desc)
        {
            return desc.pluginFormatName == kFormatName || desc.fileOrIdentifier.startsWith("dsd.builtin.");
        }

        std::unique_ptr<juce::AudioPluginInstance> create(const juce::String& identifier)
        {
            if (identifier == kGateId)       return std::make_unique<BuiltInGate>();
            if (identifier == kEqId)         return std::make_unique<BuiltInEq>();
            if (identifier == kCompressorId) return std::make_unique<BuiltInCompressor>();
            return nullptr;
        }
    } // namespace BuiltInPlugins

    // =========================================================================
    // Base
    // =========================================================================
    BuiltInProcessorBase::BuiltInProcessorBase(const juce::String& name, const juce::String& identifier, Layout layout)
        : AudioPluginInstance(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          parameters(*this, nullptr, juce::Identifier("DSDBuiltIn"), std::move(layout)),
          processorName(name),
          processorIdentifier(identifier)
    {
    }

    void BuiltInProcessorBase::fillInPluginDescription(juce::PluginDescription& desc) const
    {
        desc.name = processorName;
        desc.descriptiveName = "DSD Mixer built-in " + processorName;
        desc.pluginFormatName = BuiltInPlugins::kFormatName;
        desc.category = "Dynamics/EQ";
        desc.manufacturerName = "DSD Mixer";
        desc.version = "1.0";
        desc.fileOrIdentifier = processorIdentifier;
        desc.uniqueId = desc.deprecatedUid = static_cast<int>(processorIdentifier.hashCode());
        desc.isInstrument = false;
        desc.numInputChannels = 2;
        desc.numOutputChannels = 2;
    }

    juce::AudioProcessorEditor* BuiltInProcessorBase::createEditor()
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }

    bool BuiltInProcessorBase::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void BuiltInProcessorBase::getStateInformation(juce::MemoryBlock& destData)
    {
        if (auto xml = parameters.copyState().createXml())
            copyXmlToBinary(*xml, destData);
    }

    void BuiltInProcessorBase::setStateInformation(const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary(data, sizeInBytes))
            if (xml->hasTagName(parameters.state.getType()))
                parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }

    void BuiltInProcessorBase::setParameterValue(const juce::String& paramId, float value)
    {
        if (auto* p = parameters.getParameter(paramId))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    float BuiltInProcessorBase::getParameterValue(const juce::String& paramId) const
    {
        return param(paramId);
    }

    float BuiltInProcessorBase::param(const juce::String& id) const noexcept
    {
        if (auto* v = parameters.getRawParameterValue(id))
            return v->load(std::memory_order_relaxed);
        return 0.0f;
    }

    // =========================================================================
    // Gate
    // =========================================================================
    BuiltInGate::BuiltInGate() : BuiltInProcessorBase("DSD Gate", BuiltInPlugins::kGateId, gateLayout()) {}

    void BuiltInGate::prepareToPlay(double sampleRate, int)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        envelope = 0.0f;
        gain = dbToGain(param("range"));
        open = false;
        holdRemaining = 0;
    }

    void BuiltInGate::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const int n = buffer.getNumSamples();
        const int chans = std::min(2, buffer.getNumChannels());
        if (n <= 0 || chans <= 0)
            return;

        const float openThreshold = dbToGain(param("threshold"));
        const float closeThreshold = dbToGain(param("threshold") - 4.0f); // 4 dB hysteresis
        const float closedGain = dbToGain(param("range"));
        const float attackCoeff = timeCoeff(sr, param("attack"));
        const float releaseCoeff = timeCoeff(sr, param("release"));
        const int holdSamples = static_cast<int>(param("hold") * 0.001 * sr);
        const float envRelease = timeCoeff(sr, 20.0f); // detector decay

        float* l = buffer.getWritePointer(0);
        float* r = chans > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max(std::abs(l[i]), r != nullptr ? std::abs(r[i]) : 0.0f);
            envelope = peak > envelope ? peak : envelope + envRelease * (peak - envelope);

            if (envelope >= openThreshold)
            {
                open = true;
                holdRemaining = holdSamples;
            }
            else if (open && envelope < closeThreshold)
            {
                if (holdRemaining > 0)
                    --holdRemaining;
                else
                    open = false;
            }

            const float target = open ? 1.0f : closedGain;
            gain += (target > gain ? attackCoeff : releaseCoeff) * (target - gain);

            l[i] *= gain;
            if (r != nullptr)
                r[i] *= gain;
        }

        currentGainDb.store(gainToDb(gain), std::memory_order_relaxed);
    }

    // =========================================================================
    // EQ
    // =========================================================================
    BuiltInEq::BuiltInEq() : BuiltInProcessorBase("DSD EQ", BuiltInPlugins::kEqId, eqLayout()) {}

    void BuiltInEq::prepareToPlay(double sampleRate, int)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        for (auto& b : bands)
            b.z1[0] = b.z1[1] = b.z2[0] = b.z2[1] = 0.0;
        coefficientsValid = false;
        updateCoefficients();
    }

    void BuiltInEq::updateCoefficients()
    {
        static const char* ids[] = { "hpfOn", "hpfFreq", "lowFreq", "lowGain", "mid1Freq", "mid1Gain", "mid1Q",
                                     "mid2Freq", "mid2Gain", "mid2Q", "highFreq", "highGain" };
        float current[12];
        bool changed = !coefficientsValid;
        for (int i = 0; i < 12; ++i)
        {
            current[i] = param(ids[i]);
            changed = changed || current[i] != lastParams[i];
        }
        if (!changed)
            return;

        for (int i = 0; i < 12; ++i)
            lastParams[i] = current[i];

        const double nyq = sr * 0.45;
        bands[0].active = current[0] > 0.5f;
        makeHighPass(bands[0], sr, std::min<double>(current[1], nyq));

        bands[1].active = std::abs(current[3]) > 0.01f;
        makeShelf(bands[1], sr, std::min<double>(current[2], nyq), current[3], false);

        bands[2].active = std::abs(current[5]) > 0.01f;
        makePeak(bands[2], sr, std::min<double>(current[4], nyq), current[5], current[6]);

        bands[3].active = std::abs(current[8]) > 0.01f;
        makePeak(bands[3], sr, std::min<double>(current[7], nyq), current[8], current[9]);

        bands[4].active = std::abs(current[11]) > 0.01f;
        makeShelf(bands[4], sr, std::min<double>(current[10], nyq), current[11], true);

        coefficientsValid = true;
    }

    void BuiltInEq::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        updateCoefficients();

        const int n = buffer.getNumSamples();
        const int chans = std::min(2, buffer.getNumChannels());
        const float outGain = dbToGain(param("output"));

        for (int ch = 0; ch < chans; ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            for (auto& band : bands)
            {
                if (!band.active)
                    continue;
                for (int i = 0; i < n; ++i)
                    d[i] = band.process(d[i], ch);
            }
            if (std::abs(outGain - 1.0f) > 1.0e-6f)
                for (int i = 0; i < n; ++i)
                    d[i] *= outGain;
        }
    }

    // =========================================================================
    // Compressor
    // =========================================================================
    BuiltInCompressor::BuiltInCompressor()
        : BuiltInProcessorBase("DSD Compressor", BuiltInPlugins::kCompressorId, compressorLayout()) {}

    void BuiltInCompressor::prepareToPlay(double sampleRate, int)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        smoothedGrDb = 0.0f;
    }

    float BuiltInCompressor::computeGainDb(float x, float t, float ratio, float knee) noexcept
    {
        ratio = std::max(1.0f, ratio);
        const float slope = 1.0f / ratio - 1.0f; // <= 0
        if (knee > 0.0f && 2.0f * std::abs(x - t) <= knee)
        {
            const float d = x - t + knee * 0.5f;
            return slope * d * d / (2.0f * knee);
        }
        return x > t ? slope * (x - t) : 0.0f;
    }

    void BuiltInCompressor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const int n = buffer.getNumSamples();
        const int chans = std::min(2, buffer.getNumChannels());
        if (n <= 0 || chans <= 0)
            return;

        const float threshold = param("threshold");
        const float ratio = param("ratio");
        const float knee = param("knee");
        const float makeup = param("makeup");
        const float attackCoeff = timeCoeff(sr, param("attack"));
        const float releaseCoeff = timeCoeff(sr, param("release"));

        float* l = buffer.getWritePointer(0);
        float* r = chans > 1 ? buffer.getWritePointer(1) : nullptr;
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max(std::abs(l[i]), r != nullptr ? std::abs(r[i]) : 0.0f);
            const float targetGr = computeGainDb(gainToDb(peak), threshold, ratio, knee); // <= 0

            // More reduction -> attack, less reduction -> release.
            smoothedGrDb += (targetGr < smoothedGrDb ? attackCoeff : releaseCoeff) * (targetGr - smoothedGrDb);
            maxGr = std::min(maxGr, smoothedGrDb);

            const float g = dbToGain(smoothedGrDb + makeup);
            l[i] *= g;
            if (r != nullptr)
                r[i] *= g;
        }

        gainReductionDb.store(maxGr, std::memory_order_relaxed);
    }
} // namespace dsd
