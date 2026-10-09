#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace dsd
{
    enum class KnobStyle
    {
        GainKnob,
        PanKnob
    };

    class DSDKnob : public juce::Component,
                    public juce::SettableTooltipClient
    {
    public:
        DSDKnob(const juce::String& labelText = {},
                KnobStyle style = KnobStyle::GainKnob,
                double minVal = 0.0, double maxVal = 1.0, double defaultVal = 0.0);

        void setValue(double newValue, bool sendCallback = true);
        double getValue() const noexcept { return value; }

        void setRange(double minVal, double maxVal, double defaultVal);
        void setBipolar(bool isBi) noexcept { bipolar = isBi; repaint(); }
        bool isBipolar() const noexcept { return bipolar; }

        std::function<void(double)> onValueChanged;
        std::function<juce::String(double)> formatValue;

        void paint(juce::Graphics& g) override;
        void resized() override {}

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseDoubleClick(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    private:
        juce::String label;
        KnobStyle knobStyle{KnobStyle::GainKnob};
        double value{0.0};
        double minValue{0.0};
        double maxValue{1.0};
        double defaultValue{0.0};
        bool bipolar{false};

        double dragStartValue{0.0};
        juce::Point<float> dragStartPos;

        double getNormalisedValue() const;
        void setNormalisedValue(double norm, bool sendCallback);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DSDKnob)
    };
}
