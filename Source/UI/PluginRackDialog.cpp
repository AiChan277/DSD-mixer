#include "UI/PluginRackDialog.h"
#include "UI/DSDLookAndFeel.h"
#include "Plugin/PluginManager.h"

namespace dsd
{
    PluginRackDialog::PluginRackDialog(AudioChannel& channel)
        : channelRef(channel)
    {
        titleLabel.setText(juce::String(channelRef.getName()) + " - VST3 Plugin Rack", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(titleLabel);

        addPluginBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getAccentBlue());
        addPluginBtn.onClick = [this]() { onAddPluginClicked(); };
        addAndMakeVisible(addPluginBtn);

        scanFolderBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        scanFolderBtn.onClick = [this]() { onScanFolderClicked(); };
        addAndMakeVisible(scanFolderBtn);

        totalLatencyLabel.setText("Total Latency: 0 samples (0.00 ms)", juce::dontSendNotification);
        totalLatencyLabel.setFont(juce::FontOptions(11.0f));
        totalLatencyLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(totalLatencyLabel);

        viewport.setViewedComponent(&listContainer, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        refreshList();
        setSize(520, 420);
    }

    void PluginRackDialog::refreshList()
    {
        slotRows.clear();
        listContainer.removeAllChildren();

        auto& rack = channelRef.getPluginRack();
        const int numPlugins = rack.getNumPlugins();
        const int rowHeight = 36;

        listContainer.setBounds(0, 0, 480, std::max(200, numPlugins * (rowHeight + 6)));

        for (int i = 0; i < numPlugins; ++i)
        {
            auto* slot = rack.getSlot(i);
            if (slot == nullptr) continue;

            SlotRow row;

            // Name
            row.nameLabel = std::make_unique<juce::Label>();
            row.nameLabel->setText(juce::String(i + 1) + ". " + slot->name, juce::dontSendNotification);
            row.nameLabel->setFont(juce::FontOptions(12.0f, juce::Font::bold));
            row.nameLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
            listContainer.addAndMakeVisible(row.nameLabel.get());

            // Bypass
            row.bypassBtn = std::make_unique<juce::TextButton>("BYPASS");
            row.bypassBtn->setClickingTogglesState(true);
            row.bypassBtn->setToggleState(slot->bypassed.load(), juce::dontSendNotification);
            row.bypassBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3139));
            row.bypassBtn->setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
            row.bypassBtn->onClick = [this, i, btn = row.bypassBtn.get()]()
            {
                channelRef.getPluginRack().setBypass(i, btn->getToggleState());
            };
            listContainer.addAndMakeVisible(row.bypassBtn.get());

            // Open Native GUI Editor
            row.openGuiBtn = std::make_unique<juce::TextButton>("GUI");
            row.openGuiBtn->setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getAccentGreen().darker(0.3f));
            row.openGuiBtn->onClick = [this, i]()
            {
                channelRef.getPluginRack().openPluginEditor(i);
            };
            listContainer.addAndMakeVisible(row.openGuiBtn.get());

            // Remove Button
            row.removeBtn = std::make_unique<juce::TextButton>("X");
            row.removeBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a1a1a));
            row.removeBtn->onClick = [this, i]()
            {
                channelRef.getPluginRack().removePlugin(i);
                refreshList();
            };
            listContainer.addAndMakeVisible(row.removeBtn.get());

            // Latency Readout
            const int lat = slot->latencySamples.load();
            row.latencyLabel = std::make_unique<juce::Label>();
            row.latencyLabel->setText(juce::String(lat) + " smp", juce::dontSendNotification);
            row.latencyLabel->setFont(juce::FontOptions(10.0f));
            row.latencyLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
            listContainer.addAndMakeVisible(row.latencyLabel.get());

            // Position row
            const int y = i * (rowHeight + 6) + 4;
            row.nameLabel->setBounds(10, y, 200, rowHeight);
            row.latencyLabel->setBounds(215, y, 65, rowHeight);
            row.bypassBtn->setBounds(285, y + 4, 60, 26);
            row.openGuiBtn->setBounds(355, y + 4, 48, 26);
            row.removeBtn->setBounds(410, y + 4, 32, 26);

            slotRows.push_back(std::move(row));
        }

        const int totalLat = rack.getTotalLatencySamples();
        const double latMs = (static_cast<double>(totalLat) / 48000.0) * 1000.0;
        totalLatencyLabel.setText(juce::String::formatted("Total Rack Latency: %d samples (%.2f ms)", totalLat, latMs),
                                 juce::dontSendNotification);
    }

    void PluginRackDialog::onAddPluginClicked()
    {
        // 1. Check known scanned plugins first
        auto& pm = PluginManager::getInstance();
        const auto& list = pm.getKnownPluginList();

        juce::PopupMenu menu;
        auto types = list.getTypes();

        if (types.isEmpty())
        {
            menu.addItem(1, "Scan Standard VST3 Plugins...");
            menu.addSeparator();
            menu.addItem(2, "Browse .vst3 File on Disk...");
        }
        else
        {
            for (int i = 0; i < types.size(); ++i)
            {
                menu.addItem(100 + i, types[i].name + " (" + types[i].manufacturerName + ")");
            }
            menu.addSeparator();
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
        PluginManager::getInstance().scanDefaultVST3Folders();
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "VST3 Scan Complete",
            "Scanned VST3 directory. Found " + juce::String(PluginManager::getInstance().getKnownPluginList().getNumTypes()) + " plugins.",
            "OK");
    }

    void PluginRackDialog::resized()
    {
        auto bounds = getLocalBounds().reduced(14, 12);
        titleLabel.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(8);

        auto btnRow = bounds.removeFromTop(28);
        addPluginBtn.setBounds(btnRow.removeFromLeft(160));
        btnRow.removeFromLeft(8);
        scanFolderBtn.setBounds(btnRow.removeFromLeft(150));

        bounds.removeFromTop(8);
        totalLatencyLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(8);

        viewport.setBounds(bounds);
    }

    void PluginRackDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRect(getLocalBounds(), 1);
    }
} // namespace dsd
