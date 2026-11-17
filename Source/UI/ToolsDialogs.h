#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioEngine.h"
#include <array>
#include <memory>
#include <vector>

namespace dsd
{
    // ======================================================================
    // Scenes: store / recall / rename / clear 8 mix snapshots
    // ======================================================================
    class ScenesDialog : public juce::Component, private juce::Timer
    {
    public:
        explicit ScenesDialog(AudioEngine& engine);
        ~ScenesDialog() override;
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        void timerCallback() override;
        void refresh();

        struct Row
        {
            std::unique_ptr<juce::Label> number, name, info;
            std::unique_ptr<juce::TextButton> store, recall, clear;
        };

        AudioEngine& engineRef;
        juce::Label title, help;
        std::vector<Row> rows;
    };

    // ======================================================================
    // Auto-ducking: 4 rules (trigger channel -> ducked channels)
    // ======================================================================
    class AutoDuckDialog : public juce::Component, private juce::Timer
    {
    public:
        explicit AutoDuckDialog(AudioEngine& engine);
        ~AutoDuckDialog() override;
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        void timerCallback() override;
        void loadFromEngine();

        struct RulePanel
        {
            std::unique_ptr<juce::ToggleButton> enable;
            std::unique_ptr<juce::ComboBox> trigger;
            std::array<std::unique_ptr<juce::TextButton>, 16> targets;
            std::unique_ptr<juce::Slider> threshold, depth, attack, hold, release;
            std::unique_ptr<juce::Label> gr;
            juce::Rectangle<int> bounds;
        };

        AudioEngine& engineRef;
        // Declared before the sliders so it outlives them. The console
        // LookAndFeel only draws vertical fader art, not horizontal sliders.
        juce::LookAndFeel_V4 sliderLook{ juce::LookAndFeel_V4::getLightColourScheme() };
        juce::Label title, help;
        std::array<RulePanel, AutoDucker::kMaxRules> panels;
    };

    // ======================================================================
    // Output loudness (BS.1770) and true-peak limiter per output bus
    // ======================================================================
    class LoudnessDialog : public juce::Component, private juce::Timer
    {
    public:
        explicit LoudnessDialog(AudioEngine& engine);
        ~LoudnessDialog() override;
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        void timerCallback() override;

        struct Column
        {
            std::unique_ptr<juce::Label> name, momentary, shortTerm, integrated, truePeak, gr;
            std::unique_ptr<juce::TextButton> reset;
            std::unique_ptr<juce::ToggleButton> limiterOn;
            std::unique_ptr<juce::Slider> ceiling;
            juce::Rectangle<int> bounds;
        };

        AudioEngine& engineRef;
        juce::LookAndFeel_V4 sliderLook{ juce::LookAndFeel_V4::getLightColourScheme() };
        juce::Label title, targetLabel;
        juce::ComboBox targetBox;
        std::vector<Column> columns;
        float targetLufs{-23.0f};
    };
} // namespace dsd
