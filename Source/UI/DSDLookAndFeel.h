#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dsd
{
    class DSDLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        DSDLookAndFeel();
        ~DSDLookAndFeel() override = default;

        void drawLinearSlider(juce::Graphics& g,
                              int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style,
                              juce::Slider& slider) override;

        void drawRotarySlider(juce::Graphics& g,
                              int x, int y, int width, int height,
                              float sliderPosProportional,
                              float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider& slider) override;

        void drawButtonBackground(juce::Graphics& g,
                                  juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;

        // Custom Console Palette
        static juce::Colour getConsoleDarkBg()   noexcept { return juce::Colour(0xff1f2125); }
        static juce::Colour getConsolePanelBg()  noexcept { return juce::Colour(0xff2a2d33); }
        static juce::Colour getConsoleStripBg()  noexcept { return juce::Colour(0xff32363d); }
        static juce::Colour getConsoleBevel()    noexcept { return juce::Colour(0xff434852); }
        static juce::Colour getOledBlack()       noexcept { return juce::Colour(0xff0d0e11); }
        static juce::Colour getAccentRed()       noexcept { return juce::Colour(0xffe03131); }
        static juce::Colour getAccentGreen()     noexcept { return juce::Colour(0xff10b981); }
        static juce::Colour getAccentBlue()      noexcept { return juce::Colour(0xff2563eb); }
        static juce::Colour getMeterGreen()      noexcept { return juce::Colour(0xff22c55e); }
        static juce::Colour getMeterYellow()     noexcept { return juce::Colour(0xffeab308); }
        static juce::Colour getMeterRed()        noexcept { return juce::Colour(0xffef4444); }
        static juce::Colour getTextPrimary()     noexcept { return juce::Colour(0xfff3f4f6); }
        static juce::Colour getTextSecondary()   noexcept { return juce::Colour(0xff9ca3af); }
    };
} // namespace dsd
