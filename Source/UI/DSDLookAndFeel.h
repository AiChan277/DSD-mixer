#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsd
{
    class DSDLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        DSDLookAndFeel();
        ~DSDLookAndFeel() override = default;

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style, juce::Slider& slider) override;

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider& slider) override;

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox& box) override;

        // dhd.audio Broadcast Console Palette (Light Grey Chassis)
        static juce::Colour getConsoleDarkBg()   noexcept { return juce::Colour(0xffC5C8CE); }
        static juce::Colour getConsolePanelBg()  noexcept { return juce::Colour(0xffB8BBC2); }
        static juce::Colour getConsoleStripBg()  noexcept { return juce::Colour(0xffD0D2D6); }
        static juce::Colour getConsoleBevel()    noexcept { return juce::Colour(0xff9DA0A8); }
        static juce::Colour getOledBlack()       noexcept { return juce::Colour(0xff0A0B0D); }
        static juce::Colour getTextPrimary()     noexcept { return juce::Colour(0xff2A2D33); }
        static juce::Colour getTextSecondary()   noexcept { return juce::Colour(0xff6B6F78); }
        static juce::Colour getTextOnOled()      noexcept { return juce::Colour(0xffE8EAED); }
        static juce::Colour getButtonOffBg()     noexcept { return juce::Colour(0xffDCDEE3); }
        static juce::Colour getAccentRed()       noexcept { return juce::Colour(0xffD32F2F); }
        static juce::Colour getAccentGreen()     noexcept { return juce::Colour(0xff2E7D32); }
        static juce::Colour getAccentBlue()      noexcept { return juce::Colour(0xff1565C0); }
        static juce::Colour getAccentAmber()     noexcept { return juce::Colour(0xffD97706); }
        static juce::Colour getMeterGreen()      noexcept { return juce::Colour(0xff22C55E); }
        static juce::Colour getMeterYellow()     noexcept { return juce::Colour(0xffEAB308); }
        static juce::Colour getMeterRed()        noexcept { return juce::Colour(0xffEF4444); }
    };
} // namespace dsd
