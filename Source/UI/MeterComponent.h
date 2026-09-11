#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioTypes.h"

namespace dsd
{
    class MeterComponent : public juce::Component
    {
    public:
        MeterComponent(bool isStereoMode = false);
        ~MeterComponent() override = default;

        void setMeterValues(float peakL, float peakR, float peakHoldL, float peakHoldR, bool clipped);
        void setClipResetCallback(std::function<void()> callback) { onClipReset = callback; }

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;

    private:
        bool isStereo{false};
        float currentPeakL{0.0f};
        float currentPeakR{0.0f};
        float currentHoldL{0.0f};
        float currentHoldR{0.0f};
        bool isClipped{false};

        std::function<void()> onClipReset;

        static float linearToMeterProportion(float linear) noexcept;
        void drawSingleMeterBar(juce::Graphics& g, const juce::Rectangle<float>& bounds, float peak, float hold);
    };
} // namespace dsd
