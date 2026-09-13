#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dsd
{
    class FaderComponent : public juce::Component
    {
    public:
        FaderComponent(const juce::String& name = "Fader");
        ~FaderComponent() override = default;

        void setValue(float dbValue, juce::NotificationType notification = juce::sendNotification);
        float getValue() const noexcept;

        void setOnValueChanged(std::function<void(float)> callback) { onValueChanged = callback; }

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        juce::Slider slider;
        std::function<void(float)> onValueChanged;

        static float skewDb(float db) noexcept;
    };
} // namespace dsd
