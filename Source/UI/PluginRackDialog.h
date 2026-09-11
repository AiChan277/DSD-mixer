#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Channel/AudioChannel.h"

namespace dsd
{
    class PluginRackDialog : public juce::Component
    {
    public:
        PluginRackDialog(AudioChannel& channel);
        ~PluginRackDialog() override = default;

        void refreshList();
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioChannel& channelRef;

        juce::Label titleLabel;
        juce::TextButton addPluginBtn{"+ ADD VST3 PLUGIN"};
        juce::TextButton scanFolderBtn{"SCAN VST3 FOLDER"};
        juce::Label totalLatencyLabel;

        struct SlotRow
        {
            std::unique_ptr<juce::Label> nameLabel;
            std::unique_ptr<juce::TextButton> bypassBtn;
            std::unique_ptr<juce::TextButton> openGuiBtn;
            std::unique_ptr<juce::TextButton> removeBtn;
            std::unique_ptr<juce::Label> latencyLabel;
        };

        std::vector<SlotRow> slotRows;
        juce::Viewport viewport;
        juce::Component listContainer;

        void onAddPluginClicked();
        void onScanFolderClicked();
    };
} // namespace dsd
