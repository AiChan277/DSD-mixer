#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Channel/AudioChannel.h"
#include "Audio/WindowAudioCapture.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"
#include "UI/DSDKnob.h"
#include "UI/DSDConsoleButton.h"

namespace dsd
{
    class DynamicComboBox : public juce::ComboBox
    {
    public:
        std::function<void()> onBeforePopup;
        void showPopup() override
        {
            if (onBeforePopup)
                onBeforePopup();
            juce::ComboBox::showPopup();
        }
    };

    class ChannelStrip : public juce::Component
    {
    public:
        ChannelStrip(AudioChannel& channel, juce::AudioDeviceManager& deviceManager);
        ~ChannelStrip() override;
        void updateMeterFromAudio();
        void refreshDeviceList();
        void updateUIFromChannel();
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioChannel&             channelRef;
        juce::AudioDeviceManager& devMgrRef;
        DynamicComboBox           inputDeviceSelector;

        // ── OLED display area ──────────────────────────────────
        juce::Rectangle<float>    oledBounds;
        juce::Label               chNumberBadge;
        juce::Label               faderReadout;       // fader dB value (top-right of OLED)
        juce::Label               channelNameLabel;   // editable channel name
        MeterComponent            topMeter;          // PRE-GAIN meter (Stage A) - horizontal segmented
        juce::Label               preGainDbfsReadout; // PRE-GAIN numeric dBFS

        // ── GAIN knob ──────────────────────────────────────────
        DSDKnob                   gainKnob         { "GAIN", KnobStyle::GainKnob, MIN_GAIN_DB, MAX_GAIN_DB, 0.0 };

        // ── PAN knob ───────────────────────────────────────────
        DSDKnob                   panKnob          { "PAN",  KnobStyle::PanKnob,  -1.0, 1.0, 0.0 };

        // ── VST / DM (right column, next to PAN) ──────────────
        DSDConsoleButton          vstRackBtn       { "VST", ConsoleButtonStyle::RectangularVst, ButtonLabelPlacement::None };
        DSDConsoleButton          directMonitorBtn { "DM",  ConsoleButtonStyle::RectangularDm,  ButtonLabelPlacement::None };

        // ── Button matrix row 1 ───────────────────────────────
        DSDConsoleButton          muteBtn          { "MUTE", ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };
        DSDConsoleButton          phaseInvertBtn   { "PH",   ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };
        DSDConsoleButton          monoBtn          { "MONO", ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };

        // ── Button matrix row 2 ───────────────────────────────
        DSDConsoleButton          pflBtn           { "PFL",  ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };
        DSDConsoleButton          hpfBtn           { "HPF",  ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };
        DSDConsoleButton          padBtn           { "PAD",  ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };

        // ── Hardware fader ─────────────────────────────────────
        FaderComponent            fader;

        // ── Bottom ON / OFF ────────────────────────────────────
        DSDConsoleButton          onBtn            { "ON",  ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Above };
        DSDConsoleButton          offBtn           { "OFF", ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Above };

        std::vector<RunningAppInfo> runningApps;

        void setupButtons();
        void onDeviceSelected();
        void openVstRackWindow();

        juce::Component::SafePointer<juce::DocumentWindow> activeRackWindow;
    };
} // namespace dsd
