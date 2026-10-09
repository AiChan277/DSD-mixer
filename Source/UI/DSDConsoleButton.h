#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace dsd
{
    enum class ButtonLabelPlacement
    {
        None,
        Above,
        Below,
        Inside
    };

    enum class ConsoleButtonStyle
    {
        SquareAsset,     // uses OnButton.png / OffButton.png
        RectangularVst,  // uses BtnVst.png
        RectangularDm    // uses BtnDm.png
    };

    class DSDConsoleButton : public juce::Component,
                             public juce::SettableTooltipClient
    {
    public:
        DSDConsoleButton(const juce::String& text = {},
                         ConsoleButtonStyle style = ConsoleButtonStyle::SquareAsset,
                         ButtonLabelPlacement labelPlacement = ButtonLabelPlacement::Below);

        void setToggled(bool state, bool sendCallback = true);
        bool isToggled() const noexcept { return toggled; }

        void setButtonEnabled(bool enabled);
        bool isButtonEnabled() const noexcept { return buttonEnabled; }

        void setToggleMode(bool isToggle) noexcept { toggleMode = isToggle; }
        bool isToggleMode() const noexcept { return toggleMode; }

        void setLabelPlacement(ButtonLabelPlacement placement) noexcept { labelPlacement = placement; repaint(); }

        juce::String getText() const { return label; }
        void setText(const juce::String& text) { label = text; repaint(); }

        std::function<void(bool)> onToggled;
        std::function<void()> onClick;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

    private:
        juce::String label;
        ConsoleButtonStyle buttonStyle{ConsoleButtonStyle::SquareAsset};
        ButtonLabelPlacement labelPlacement{ButtonLabelPlacement::Below};
        bool toggled{false};
        bool buttonEnabled{true};
        bool toggleMode{true};
        bool mouseIsDown{false};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DSDConsoleButton)
    };
}
