#include "UI/RoutingMatrixDialog.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    RoutingMatrixDialog::RoutingMatrixDialog(RoutingEngine& router, ChannelManager& chMgr, OutputManager& outMgr)
        : routingEngineRef(router), channelManagerRef(chMgr), outputManagerRef(outMgr)
    {
        titleLabel.setText("16x4 ROUTING MATRIX (SEND GAIN & CROSSPOINTS)", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(titleLabel);

        viewport.setViewedComponent(&gridContainer, false);
        viewport.setScrollBarsShown(true, true);
        addAndMakeVisible(viewport);

        const int numCh = NUM_CHANNELS_LEVEL1;
        const int numOut = NUM_OUTPUT_BUSES_LEVEL1;

        // Column Labels (Outputs)
        for (int out = 0; out < numOut; ++out)
        {
            auto lbl = std::make_unique<juce::Label>();
            std::string outName = (outputManagerRef.getOutput(out) != nullptr)
                ? outputManagerRef.getOutput(out)->getName() : ("OUT " + std::to_string(out + 1));
            lbl->setText(outName, juce::dontSendNotification);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::FontOptions(11.0f, juce::Font::bold));
            lbl->setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentRed());
            gridContainer.addAndMakeVisible(lbl.get());
            outputColLabels.push_back(std::move(lbl));
        }

        // Rows (Channels) & Cells
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto lbl = std::make_unique<juce::Label>();
            std::string chName = (channelManagerRef.getChannel(ch) != nullptr)
                ? channelManagerRef.getChannel(ch)->getName() : ("CH " + std::to_string(ch + 1));
            lbl->setText(chName, juce::dontSendNotification);
            lbl->setFont(juce::FontOptions(11.0f, juce::Font::bold));
            lbl->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
            gridContainer.addAndMakeVisible(lbl.get());
            channelRowLabels.push_back(std::move(lbl));

            for (int out = 0; out < numOut; ++out)
            {
                auto cell = std::make_unique<MatrixCellComponent>();
                cell->chIdx = ch;
                cell->outIdx = out;

                cell->enableBtn.setButtonText("ON");
                cell->enableBtn.setToggleState(routingEngineRef.isRouteEnabled(ch, out), juce::dontSendNotification);
                cell->enableBtn.onClick = [this, cellPtr = cell.get()]()
                {
                    routingEngineRef.setRouteEnabled(cellPtr->chIdx, cellPtr->outIdx, cellPtr->enableBtn.getToggleState());
                };
                cell->addAndMakeVisible(cell->enableBtn);

                cell->sendGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                cell->sendGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
                cell->sendGainSlider.setRange(-60.0, 12.0, 0.1);
                cell->sendGainSlider.setValue(routingEngineRef.getRouteGainDb(ch, out), juce::dontSendNotification);
                cell->sendGainSlider.setDoubleClickReturnValue(true, 0.0);
                cell->sendGainSlider.onValueChange = [this, cellPtr = cell.get()]()
                {
                    routingEngineRef.setRouteGainDb(cellPtr->chIdx, cellPtr->outIdx, static_cast<float>(cellPtr->sendGainSlider.getValue()));
                };
                cell->addAndMakeVisible(cell->sendGainSlider);

                gridContainer.addAndMakeVisible(cell.get());
                cells.push_back(std::move(cell));
            }
        }

        setSize(680, 560);
    }

    void RoutingMatrixDialog::resized()
    {
        auto bounds = getLocalBounds().reduced(12, 10);
        titleLabel.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(8);

        viewport.setBounds(bounds);

        const int rowH = 50;
        const int headerH = 26;
        const int chLabelW = 110;
        const int colW = 120;
        const int numCh = NUM_CHANNELS_LEVEL1;
        const int numOut = NUM_OUTPUT_BUSES_LEVEL1;

        const int totalW = chLabelW + numOut * colW + 20;
        const int totalH = headerH + numCh * rowH + 20;
        gridContainer.setBounds(0, 0, totalW, totalH);

        // Position Column Headers
        for (int out = 0; out < numOut; ++out)
        {
            outputColLabels[out]->setBounds(chLabelW + out * colW, 0, colW, headerH);
        }

        // Position Rows & Cells
        int cellIndex = 0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int y = headerH + ch * rowH;
            channelRowLabels[ch]->setBounds(0, y, chLabelW, rowH);

            for (int out = 0; out < numOut; ++out)
            {
                auto* cell = cells[cellIndex++].get();
                cell->setBounds(chLabelW + out * colW, y, colW, rowH);

                // Internal cell layout
                auto cellArea = cell->getLocalBounds().reduced(2, 2);
                cell->enableBtn.setBounds(cellArea.removeFromLeft(42).reduced(0, 8));
                cell->sendGainSlider.setBounds(cellArea);
            }
        }
    }

    void RoutingMatrixDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
    }
} // namespace dsd
