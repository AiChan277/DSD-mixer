#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Channel/AudioChannel.h"
#include "Plugin/PluginManager.h"
#include <vector>
#include <memory>

namespace dsd
{
    class PluginRackDialog;

    // Component representing an individual VST3 plugin slot in the console rack
    class PluginRowView : public juce::Component
    {
    public:
        PluginRowView(PluginRackDialog& parentDialog, AudioChannel& channel, PluginInstanceID instanceId, int slotNumber);
        ~PluginRowView() override = default;

        void updateDisplay();
        void resized() override;
        void paint(juce::Graphics& g) override;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

        PluginInstanceID getInstanceId() const noexcept { return pluginId; }

    private:
        PluginRackDialog& ownerDialog;
        AudioChannel& channelRef;
        PluginInstanceID pluginId;
        int displaySlotNumber{1};

        juce::Label dragHandleLabel;
        juce::Label slotNumberBadge;
        juce::Label nameLabel;
        juce::Label latencyBadge;

        juce::TextButton bypassBtn{"BYPASS"};
        juce::TextButton openGuiBtn{"GUI"};
        juce::TextButton removeBtn{"X"};

        bool isDraggingThis{false};
    };

    // Component hosting the rows with drag-and-drop target capabilities
    class PluginListContainer : public juce::Component, public juce::DragAndDropTarget
    {
    public:
        PluginListContainer(PluginRackDialog& owner);
        ~PluginListContainer() override = default;

        bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
        void itemDragEnter(const SourceDetails& dragSourceDetails) override;
        void itemDragMove(const SourceDetails& dragSourceDetails) override;
        void itemDragExit(const SourceDetails& dragSourceDetails) override;
        void itemDropped(const SourceDetails& dragSourceDetails) override;

        void setDropInsertIndex(int idx);
        void paintOverChildren(juce::Graphics& g) override;

    private:
        PluginRackDialog& ownerDialog;
        int dropInsertIndex{-1};
    };

    // Main VST3 Rack Dialog
    class PluginRackDialog : public juce::Component,
                             public PluginManager::Listener,
                             public juce::DragAndDropContainer
    {
    public:
        PluginRackDialog(AudioChannel& channel);
        ~PluginRackDialog() override;

        void refreshList();
        void updateLatencyDisplay();

        void resized() override;
        void paint(juce::Graphics& g) override;

        void pluginListChanged() override;
        void scanProgressUpdated(const juce::String& pluginName, float progress) override;

        AudioChannel& getChannel() noexcept { return channelRef; }
        void handleRowDrop(PluginInstanceID draggedId, int targetIndex);

    private:
        AudioChannel& channelRef;

        juce::Label titleLabel;
        juce::Label channelBadge;
        juce::TextButton addPluginBtn{"+ ADD VST3 PLUGIN"};
        juce::TextButton scanFolderBtn{"RESCAN PLUGINS"};
        juce::Label scanStatusLabel;

        // OLED Latency & Info Banner
        juce::Label latencyCard;

        juce::Viewport viewport;
        PluginListContainer listContainer;
        std::vector<std::unique_ptr<PluginRowView>> rowViews;

        juce::Label emptyStateLabel;

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
