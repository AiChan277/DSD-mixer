#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Channel/AudioChannel.h"
#include "Plugin/PluginManager.h"

namespace dsd
{
    class PluginRackDialog : public juce::Component,
                             public PluginManager::Listener
    {
    public:
        PluginRackDialog(AudioChannel& channel);
        ~PluginRackDialog() override;

        void refreshList();
        void resized() override;
        void paint(juce::Graphics& g) override;

        void pluginListChanged() override;
        void scanProgressUpdated(const juce::String& pluginName, float progress) override;

    private:
        AudioChannel& channelRef;

        juce::Label titleLabel;
        juce::TextButton addPluginBtn{"+ ADD VST3 PLUGIN"};
        juce::TextButton scanFolderBtn{"RESCAN PLUGINS"};
        juce::Label totalLatencyLabel;
        juce::Label scanStatusLabel;

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

    class PluginRackWindow : public juce::DocumentWindow
    {
    public:
        PluginRackWindow(const juce::String& title, AudioChannel& channel);
        ~PluginRackWindow() override = default;

        void closeButtonPressed() override;
    };
} // namespace dsd

