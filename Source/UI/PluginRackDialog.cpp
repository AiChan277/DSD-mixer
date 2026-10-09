#include "UI/PluginRackDialog.h"
#include "UI/DSDLookAndFeel.h"
#include "Plugin/PluginManager.h"
#include <map>

namespace dsd
{
    // =========================================================================
    // PluginRowView Implementation
    // =========================================================================
    PluginRowView::PluginRowView(PluginRackDialog& parentDialog, AudioChannel& channel, PluginInstanceID instanceId, int slotNumber)
        : ownerDialog(parentDialog), channelRef(channel), pluginId(instanceId), displaySlotNumber(slotNumber)
    {
        // 1. Drag Handle (::)
        dragHandleLabel.setText("::", juce::dontSendNotification);
        dragHandleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        dragHandleLabel.setJustificationType(juce::Justification::centred);
        dragHandleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        dragHandleLabel.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        dragHandleLabel.setTooltip("Drag to reorder in plugin chain");
        addAndMakeVisible(dragHandleLabel);

        // 2. Slot Number Badge (01, 02, etc.)
        slotNumberBadge.setText(juce::String::formatted("%02d", displaySlotNumber), juce::dontSendNotification);
        slotNumberBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        slotNumberBadge.setJustificationType(juce::Justification::centred);
        slotNumberBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1A2433));
        slotNumberBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentBlue());
        slotNumberBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(slotNumberBadge);

        // 3. Plugin Name Label
        nameLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        nameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(nameLabel);

        // 4. Latency Readout Badge
        latencyBadge.setFont(juce::FontOptions(10.5f));
        latencyBadge.setJustificationType(juce::Justification::centredRight);
        latencyBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(latencyBadge);

        // 5. Bypass Button (Independent per plugin instance)
        bypassBtn.setClickingTogglesState(true);
        bypassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2D35));
        bypassBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        bypassBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffB0B4BC));
        bypassBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        bypassBtn.setTooltip("Bypass this plugin instance");
        bypassBtn.onClick = [this]()
        {
            const bool isBypassed = bypassBtn.getToggleState();
            channelRef.getPluginRack().setBypassById(pluginId, isBypassed);
            ownerDialog.updateLatencyDisplay();
            updateDisplay();
        };
        addAndMakeVisible(bypassBtn);

        // 6. GUI Button (Opens plugin native editor)
        openGuiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1E3A2F));
        openGuiBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getMeterGreen().brighter(0.2f));
        openGuiBtn.setTooltip("Open plugin graphic editor window");
        openGuiBtn.onClick = [this]()
        {
            channelRef.getPluginRack().openPluginEditorById(pluginId);
        };
        addAndMakeVisible(openGuiBtn);

        // 7. Remove Button (✕)
        removeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4A1D24));
        removeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffFFA0A0));
        removeBtn.setTooltip("Remove plugin from rack");
        removeBtn.onClick = [this]()
        {
            channelRef.getPluginRack().removePluginById(pluginId);
            ownerDialog.refreshList();
        };
        addAndMakeVisible(removeBtn);

        updateDisplay();
    }

    void PluginRowView::updateDisplay()
    {
        const auto* slot = channelRef.getPluginRack().getSlotById(pluginId);
        if (slot == nullptr)
            return;

        nameLabel.setText(slot->name, juce::dontSendNotification);

        const bool isBypassed = slot->bypassed.load(std::memory_order_relaxed);
        bypassBtn.setToggleState(isBypassed, juce::dontSendNotification);

        const int latSamples = slot->latencySamples.load(std::memory_order_relaxed);
        const double latMs = (static_cast<double>(latSamples) / 48000.0) * 1000.0;

        if (isBypassed)
        {
            latencyBadge.setText("BYPASSED", juce::dontSendNotification);
            latencyBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
            nameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        }
        else
        {
            if (latSamples > 0)
                latencyBadge.setText(juce::String::formatted("%d smp (%.2f ms)", latSamples, latMs), juce::dontSendNotification);
            else
                latencyBadge.setText("0 smp", juce::dontSendNotification);

            latencyBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
            nameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        }

        repaint();
    }

    void PluginRowView::mouseDown(const juce::MouseEvent& e)
    {
        if (e.x < 36) // Drag handle region
        {
            isDraggingThis = true;
        }
    }

    void PluginRowView::mouseDrag(const juce::MouseEvent& e)
    {
        if (isDraggingThis && e.getDistanceFromDragStart() > 4)
        {
            isDraggingThis = false;
            auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
            if (container != nullptr)
            {
                juce::var desc("DSD_PLUGIN:" + juce::String(pluginId));
                container->startDragging(desc, this);
            }
        }
    }

    void PluginRowView::resized()
    {
        auto bounds = getLocalBounds().reduced(4, 2);

        // 1. Drag handle (width: 24px)
        dragHandleLabel.setBounds(bounds.removeFromLeft(24));
        bounds.removeFromLeft(4);

        // 2. Slot number badge (width: 26px)
        slotNumberBadge.setBounds(bounds.removeFromLeft(26).reduced(0, 4));
        bounds.removeFromLeft(8);

        // 3. Trailing control buttons: [REMOVE] [GUI] [BYPASS]
        removeBtn.setBounds(bounds.removeFromRight(28).reduced(0, 4));
        bounds.removeFromRight(6);

        openGuiBtn.setBounds(bounds.removeFromRight(44).reduced(0, 4));
        bounds.removeFromRight(6);

        bypassBtn.setBounds(bounds.removeFromRight(64).reduced(0, 4));
        bounds.removeFromRight(10);

        // 4. Latency readout (width: 105px)
        latencyBadge.setBounds(bounds.removeFromRight(105));
        bounds.removeFromRight(8);

        // 5. Plugin name fills remaining width
        nameLabel.setBounds(bounds);
    }

    void PluginRowView::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f, 1.0f);
        const auto* slot = channelRef.getPluginRack().getSlotById(pluginId);
        const bool isBypassed = (slot != nullptr && slot->bypassed.load(std::memory_order_relaxed));

        // Dark chassis cassette slot background
        if (isBypassed)
            g.setColour(juce::Colour(0xff16171B));
        else
            g.setColour(juce::Colour(0xff1E222A));

        g.fillRoundedRectangle(bounds, 3.5f);

        // Bevel border
        if (isBypassed)
            g.setColour(juce::Colour(0xff2E3138));
        else
            g.setColour(juce::Colour(0xff3B414E));

        g.drawRoundedRectangle(bounds, 3.5f, 1.0f);
    }

    // =========================================================================
    // PluginListContainer Implementation (Drag & Drop Target)
    // =========================================================================
    PluginListContainer::PluginListContainer(PluginRackDialog& owner)
        : ownerDialog(owner)
    {
    }

    bool PluginListContainer::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
    {
        return dragSourceDetails.description.toString().startsWith("DSD_PLUGIN:");
    }

    void PluginListContainer::itemDragEnter(const SourceDetails& dragSourceDetails)
    {
        itemDragMove(dragSourceDetails);
    }

    void PluginListContainer::itemDragMove(const SourceDetails& dragSourceDetails)
    {
        const int rowHeight = 38;
        const int y = dragSourceDetails.localPosition.y;
        int idx = std::clamp(y / rowHeight, 0, ownerDialog.getChannel().getPluginRack().getNumPlugins());
        setDropInsertIndex(idx);
    }

    void PluginListContainer::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
    {
        setDropInsertIndex(-1);
    }

    void PluginListContainer::itemDropped(const SourceDetails& dragSourceDetails)
    {
        const juce::String desc = dragSourceDetails.description.toString();
        if (desc.startsWith("DSD_PLUGIN:"))
        {
            const PluginInstanceID draggedId = static_cast<PluginInstanceID>(desc.substring(11).getLargeIntValue());
            const int targetIdx = dropInsertIndex >= 0 ? dropInsertIndex : 0;
            ownerDialog.handleRowDrop(draggedId, targetIdx);
        }
        setDropInsertIndex(-1);
    }

    void PluginListContainer::setDropInsertIndex(int idx)
    {
        if (dropInsertIndex != idx)
        {
            dropInsertIndex = idx;
            repaint();
        }
    }

    void PluginListContainer::paintOverChildren(juce::Graphics& g)
    {
        if (dropInsertIndex >= 0)
        {
            const int rowHeight = 38;
            const float lineY = static_cast<float>(dropInsertIndex * rowHeight);

            // Glowing cyan/amber insertion indicator line
            g.setColour(DSDLookAndFeel::getAccentAmber());
            g.fillRect(8.0f, lineY - 1.5f, static_cast<float>(getWidth() - 16), 3.0f);

            // Left triangle pointer
            juce::Path p;
            p.addTriangle(2.0f, lineY - 5.0f, 2.0f, lineY + 5.0f, 9.0f, lineY);
            g.fillPath(p);
        }
    }

    // =========================================================================
    // PluginRackDialog Implementation
    // =========================================================================
    PluginRackDialog::PluginRackDialog(AudioChannel& channel)
        : channelRef(channel), listContainer(*this)
    {
        PluginManager::getInstance().addListener(this);

        // 1. Channel Badge & Title
        channelBadge.setText(juce::String::formatted("CH %02d", channelRef.getChannelID()), juce::dontSendNotification);
        channelBadge.setJustificationType(juce::Justification::centred);
        channelBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        channelBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff15263F));
        channelBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentBlue());
        channelBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(channelBadge);

        titleLabel.setText(juce::String(channelRef.getName()) + " - VST3 Plugin Rack", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(titleLabel);

        // 2. Buttons: Add & Rescan
        addPluginBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getAccentBlue());
        addPluginBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addPluginBtn.onClick = [this]() { onAddPluginClicked(); };
        addAndMakeVisible(addPluginBtn);

        scanFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333742));
        scanFolderBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextOnOled());
        scanFolderBtn.onClick = [this]() { onScanFolderClicked(); };
        addAndMakeVisible(scanFolderBtn);

        scanStatusLabel.setFont(juce::FontOptions(11.0f));
        scanStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        if (PluginManager::getInstance().isScanning())
            scanStatusLabel.setText("Scanning VST3 plugins in background...", juce::dontSendNotification);
        addAndMakeVisible(scanStatusLabel);

        // 3. OLED Latency Banner Card
        latencyCard.setJustificationType(juce::Justification::centredLeft);
        latencyCard.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        latencyCard.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0D0F12));
        latencyCard.setColour(juce::Label::outlineColourId, juce::Colour(0xff2A2D35));
        latencyCard.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(latencyCard);

        // 4. Viewport and List
        viewport.setViewedComponent(&listContainer, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        // 5. Empty State Message
        emptyStateLabel.setText("No VST3 plugins loaded in this channel rack.\nClick '+ ADD VST3 PLUGIN' to insert audio processors.", juce::dontSendNotification);
        emptyStateLabel.setJustificationType(juce::Justification::centred);
        emptyStateLabel.setFont(juce::FontOptions(12.0f));
        emptyStateLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        listContainer.addChildComponent(emptyStateLabel);

        refreshList();
        setSize(560, 460);
    }

    PluginRackDialog::~PluginRackDialog()
    {
        PluginManager::getInstance().removeListener(this);
    }

    void PluginRackDialog::pluginListChanged()
    {
        scanFolderBtn.setEnabled(true);
        scanStatusLabel.setText("Plugins updated (" + juce::String(PluginManager::getInstance().getNumKnownPlugins()) + " available)",
                                juce::dontSendNotification);
    }

    void PluginRackDialog::scanProgressUpdated(const juce::String& pluginName, float /*progress*/)
    {
        scanStatusLabel.setText("Scanning: " + juce::File(pluginName).getFileNameWithoutExtension(),
                                juce::dontSendNotification);
    }

    void PluginRackDialog::updateLatencyDisplay()
    {
        const int totalLat = channelRef.getPluginRack().getTotalLatencySamples();
        const double latMs = (static_cast<double>(totalLat) / 48000.0) * 1000.0;
        latencyCard.setText(juce::String::formatted("  Total Rack Latency: %d samples | %.2f ms", totalLat, latMs),
                            juce::dontSendNotification);
    }

    void PluginRackDialog::refreshList()
    {
        rowViews.clear();
        listContainer.removeAllChildren();

        auto& rack = channelRef.getPluginRack();
        const int numPlugins = rack.getNumPlugins();
        const int rowHeight = 38;

        if (numPlugins == 0)
        {
            emptyStateLabel.setVisible(true);
            emptyStateLabel.setBounds(10, 30, 500, 80);
            listContainer.addAndMakeVisible(emptyStateLabel);
            listContainer.setBounds(0, 0, 520, 160);
        }
        else
        {
            emptyStateLabel.setVisible(false);
            const int totalHeight = numPlugins * rowHeight + 10;
            listContainer.setBounds(0, 0, 520, std::max(200, totalHeight));

            for (int i = 0; i < numPlugins; ++i)
            {
                const auto* slot = rack.getSlot(i);
                if (slot == nullptr) continue;

                auto row = std::make_unique<PluginRowView>(*this, channelRef, slot->instanceId, i + 1);
                row->setBounds(4, i * rowHeight + 4, 512, rowHeight - 2);
                listContainer.addAndMakeVisible(row.get());
                rowViews.push_back(std::move(row));
            }
        }

        updateLatencyDisplay();
    }

    void PluginRackDialog::handleRowDrop(PluginInstanceID draggedId, int targetIndex)
    {
        auto& rack = channelRef.getPluginRack();
        const int fromIndex = rack.findSlotIndex(draggedId);
        if (fromIndex >= 0)
        {
            // Adjust index if moving downwards
            int adjustedTarget = targetIndex;
            if (fromIndex < targetIndex)
                adjustedTarget = std::max(0, targetIndex - 1);

            rack.movePlugin(fromIndex, adjustedTarget);
            refreshList();
        }
    }

    void PluginRackDialog::onAddPluginClicked()
    {
        auto& pm = PluginManager::getInstance();
        const auto& list = pm.getKnownPluginList();

        juce::PopupMenu menu;
        auto types = list.getTypes();

        if (pm.isScanning())
        {
            menu.addItem(-1, "[ VST3 background scan running... ]", false);
            menu.addSeparator();
        }

        if (types.isEmpty())
        {
            menu.addItem(1, "Scan Standard VST3 Plugins...");
            menu.addSeparator();
            menu.addItem(2, "Browse .vst3 File on Disk...");
        }
        else
        {
            std::map<juce::String, std::vector<int>> mfgMap;
            for (int i = 0; i < types.size(); ++i)
            {
                auto mfg = types[i].manufacturerName.trim();
                if (mfg.isEmpty()) mfg = "Standard VST3";
                mfgMap[mfg].push_back(i);
            }

            for (auto& [mfg, indices] : mfgMap)
            {
                if (indices.size() == 1)
                {
                    int idx = indices[0];
                    menu.addItem(100 + idx, types[idx].name + " (" + mfg + ")");
                }
                else
                {
                    juce::PopupMenu sub;
                    for (int idx : indices)
                    {
                        sub.addItem(100 + idx, types[idx].name);
                    }
                    menu.addSubMenu(mfg, sub);
                }
            }

            menu.addSeparator();
            menu.addItem(1, "Rescan VST3 Plugins Now...");
            menu.addItem(2, "Browse .vst3 File on Disk...");
        }

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addPluginBtn),
            [this, types](int result)
            {
                if (result == 1)
                {
                    onScanFolderClicked();
                }
                else if (result == 2)
                {
                    auto chooser = std::make_shared<juce::FileChooser>("Select VST3 Plugin",
                                                                        juce::File("C:\\Program Files\\Common Files\\VST3"),
                                                                        "*.vst3");
                    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this, chooser](const juce::FileChooser& fc)
                        {
                            auto resultFile = fc.getResult();
                            if (resultFile.exists())
                            {
                                juce::String err;
                                auto plugin = PluginManager::getInstance().loadPluginFromFile(resultFile, err);
                                if (plugin != nullptr)
                                {
                                    channelRef.getPluginRack().addPlugin(std::move(plugin), resultFile.getFileNameWithoutExtension());
                                    refreshList();
                                }
                                else
                                {
                                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed", err, "OK");
                                }
                            }
                        });
                }
                else if (result >= 100)
                {
                    int idx = result - 100;
                    if (idx >= 0 && idx < types.size())
                    {
                        juce::String err;
                        auto plugin = PluginManager::getInstance().loadPlugin(types[idx], err);
                        if (plugin != nullptr)
                        {
                            channelRef.getPluginRack().addPlugin(std::move(plugin), types[idx].name);
                            refreshList();
                        }
                        else
                        {
                            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed", err, "OK");
                        }
                    }
                }
            });
    }

    void PluginRackDialog::onScanFolderClicked()
    {
        scanFolderBtn.setEnabled(false);
        scanStatusLabel.setText("Scanning VST3 plugins...", juce::dontSendNotification);
        PluginManager::getInstance().rescanAllAsync([this](int totalFound)
        {
            scanFolderBtn.setEnabled(true);
            scanStatusLabel.setText("Scan complete. Found " + juce::String(totalFound) + " plugins.", juce::dontSendNotification);
        });
    }

    void PluginRackDialog::resized()
    {
        auto bounds = getLocalBounds().reduced(14, 12);

        // 1. Header: Channel Badge & Title
        auto header = bounds.removeFromTop(26);
        channelBadge.setBounds(header.removeFromLeft(48));
        header.removeFromLeft(8);
        titleLabel.setBounds(header);

        bounds.removeFromTop(8);

        // 2. Button Toolbar
        auto btnRow = bounds.removeFromTop(28);
        addPluginBtn.setBounds(btnRow.removeFromLeft(160));
        btnRow.removeFromLeft(8);
        scanFolderBtn.setBounds(btnRow.removeFromLeft(140));
        btnRow.removeFromLeft(8);
        scanStatusLabel.setBounds(btnRow);

        bounds.removeFromTop(8);

        // 3. OLED Latency Banner Card
        latencyCard.setBounds(bounds.removeFromTop(26));

        bounds.removeFromTop(8);

        // 4. Viewport containing list of plugin rack slots
        viewport.setBounds(bounds);
    }

    void PluginRackDialog::paint(juce::Graphics& g)
    {
        // Dark metallic console chassis
        g.fillAll(juce::Colour(0xff121418));
        g.setColour(juce::Colour(0xff2A2D35));
        g.drawRect(getLocalBounds(), 1);
    }

    // =========================================================================
    // PluginRackWindow Implementation
    // =========================================================================
    PluginRackWindow::PluginRackWindow(const juce::String& title, AudioChannel& channel)
        : DocumentWindow(title, juce::Colour(0xff121418), DocumentWindow::closeButton, true)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new PluginRackDialog(channel), true);
        setResizable(true, false);
        centreWithSize(580, 480);
        toFront(true);
        setVisible(true);
    }

    void PluginRackWindow::closeButtonPressed()
    {
        setVisible(false);
        delete this;
    }
} // namespace dsd
