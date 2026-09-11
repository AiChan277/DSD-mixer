#include "UI/StageInspectorDialog.h"
#include "UI/DSDLookAndFeel.h"
#include "DSP/GainProcessor.h"

namespace dsd
{
    StageInspectorDialog::StageInspectorDialog(ChannelManager& chanMgr, OutputManager& outMgr, DSPScheduler& scheduler)
        : channelManagerRef(chanMgr), outputManagerRef(outMgr), schedulerRef(scheduler)
    {
        // Channel selection dropdown
        channelSelector.setTextWhenNothingSelected("Select Channel...");
        const int numCh = channelManagerRef.getNumChannels();
        for (int i = 0; i < numCh; ++i)
        {
            if (auto* ch = channelManagerRef.getChannel(i))
            {
                juce::String name = juce::String::formatted("CH %02d: %s", i + 1, ch->getName().c_str());
                channelSelector.addItem(name, i + 1);
            }
        }
        channelSelector.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(channelSelector);

        // DSP Mode Button (Toggle between Single-Thread and Multicore)
        dspModeBtn.setClickingTogglesState(false);
        dspModeBtn.onClick = [this]()
        {
            bool current = schedulerRef.isParallelEnabled();
            schedulerRef.setParallelEnabled(!current);
            dspModeBtn.setButtonText(!current ? "DSP: MULTICORE PARALLEL" : "DSP: SINGLE-THREAD (Safe)");
        };
        dspModeBtn.setButtonText(schedulerRef.isParallelEnabled() ? "DSP: MULTICORE PARALLEL" : "DSP: SINGLE-THREAD (Safe)");
        addAndMakeVisible(dspModeBtn);

        // Reset Clips button: resets clip on all channels and outputs
        resetClipsBtn.onClick = [this]()
        {
            for (int i = 0; i < channelManagerRef.getNumChannels(); ++i)
            {
                if (auto* ch = channelManagerRef.getChannel(i))
                {
                    ch->getMeterInput().resetClip();
                    ch->getMeterPostGain().resetClip();
                    ch->getMeterPostVST().resetClip();
                    ch->getMeterValues().resetClip();
                }
            }
            for (int i = 0; i < outputManagerRef.getNumOutputs(); ++i)
            {
                if (auto* out = outputManagerRef.getOutput(i))
                    out->getMeterValues().resetClip();
            }
        };
        addAndMakeVisible(resetClipsBtn);

        // Setup 5 stage diagnostic cards
        setupCard(stageA, "STAGE A: RAW INPUT");
        setupCard(stageB, "STAGE B: POST-GAIN (Pre-Fader)");
        setupCard(stageC, "STAGE C: POST-VST (Plugin Rack)");
        setupCard(stageD, "STAGE D: POST-FADER & PAN");
        setupCard(stageE, "STAGE E: OUTPUT BUS (Sum)");

        startTimerHz(30);
        setSize(780, 480);
    }

    StageInspectorDialog::~StageInspectorDialog()
    {
        stopTimer();
    }

    void StageInspectorDialog::setupCard(StageCard& card, const juce::String& title)
    {
        card.titleLabel.setText(title, juce::dontSendNotification);
        card.titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        card.titleLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        card.titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        card.titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(card.titleLabel);

        card.peakLabel.setText("Peak: -oo dBFS", juce::dontSendNotification);
        card.peakLabel.setFont(juce::FontOptions(11.5f));
        card.peakLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(card.peakLabel);

        card.rmsLabel.setText("RMS: -oo dBFS", juce::dontSendNotification);
        card.rmsLabel.setFont(juce::FontOptions(11.5f));
        card.rmsLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(card.rmsLabel);

        card.statusLabel.setText("STATUS: OK", juce::dontSendNotification);
        card.statusLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        card.statusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        card.statusLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(card.statusLabel);
    }

    void StageInspectorDialog::updateCard(StageCard& card, const MeterValues& mv, const juce::String& extra)
    {
        const float pL = mv.peakL.load(std::memory_order_relaxed);
        const float pR = mv.peakR.load(std::memory_order_relaxed);
        const float peakMax = std::max(pL, pR);
        const float rmsMax = std::max(mv.rmsL.load(std::memory_order_relaxed),
                                      mv.rmsR.load(std::memory_order_relaxed));
        const bool isClipped = mv.clipped.load(std::memory_order_relaxed);

        const float peakDb = (peakMax > 0.00001f) ? GainProcessor::linearToDb(peakMax) : -96.0f;
        const float rmsDb  = (rmsMax > 0.00001f)  ? GainProcessor::linearToDb(rmsMax)  : -96.0f;

        juce::String peakStr = (peakDb <= -90.0f) ? "-oo dBFS" : (juce::String(peakDb, 1) + " dBFS");
        juce::String rmsStr  = (rmsDb  <= -90.0f) ? "-oo dBFS" : (juce::String(rmsDb, 1)  + " dBFS");

        if (extra.isNotEmpty())
            card.peakLabel.setText("Peak: " + peakStr + "   (" + extra + ")", juce::dontSendNotification);
        else
            card.peakLabel.setText("Peak: " + peakStr, juce::dontSendNotification);

        card.rmsLabel.setText("RMS:  " + rmsStr, juce::dontSendNotification);

        if (peakDb >= 0.0f)
        {
            card.statusLabel.setText("STATUS: CURRENTLY CLIPPING! (>0 dBFS)", juce::dontSendNotification);
            card.statusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterRed());
        }
        else if (isClipped)
        {
            card.statusLabel.setText("STATUS: CLIP RECORDED (Click Reset)", juce::dontSendNotification);
            card.statusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterYellow());
        }
        else if (peakDb >= -3.0f)
        {
            card.statusLabel.setText("STATUS: HOT (>-3 dBFS)", juce::dontSendNotification);
            card.statusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterYellow());
        }
        else
        {
            card.statusLabel.setText("STATUS: NOMINAL (Clean)", juce::dontSendNotification);
            card.statusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        }
    }

    void StageInspectorDialog::timerCallback()
    {
        const int chIdx = channelSelector.getSelectedId() - 1;
        auto* ch = channelManagerRef.getChannel(chIdx);
        if (ch == nullptr)
            return;

        // 1. Stage A: Input
        updateCard(stageA, ch->getMeterInput(), "Device: " + ch->getInputDeviceName());

        // 2. Stage B: Post-Gain
        updateCard(stageB, ch->getMeterPostGain(), "Gain: " + juce::String(ch->getGainDb(), 1) + " dB");

        // 3. Stage C: Post-VST
        juce::String vstInfo = "VSTs: " + juce::String(ch->getPluginRack().getNumPlugins())
                             + ", Latency: " + juce::String(ch->getPluginRack().getTotalLatencySamples()) + " smp";
        updateCard(stageC, ch->getMeterPostVST(), vstInfo);

        // 4. Stage D: Post-Fader
        updateCard(stageD, ch->getMeterValues(), "Fader: " + juce::String(ch->getFaderDb(), 1) + " dB");

        // 5. Stage E: Output Bus 01
        if (auto* out01 = outputManagerRef.getOutput(0))
        {
            updateCard(stageE, out01->getMeterValues(), "Bus: " + out01->getName());
        }
    }

    void StageInspectorDialog::resized()
    {
        auto bounds = getLocalBounds().reduced(16, 16);

        auto topRow = bounds.removeFromTop(32);
        channelSelector.setBounds(topRow.removeFromLeft(220));
        topRow.removeFromLeft(12);
        dspModeBtn.setBounds(topRow.removeFromLeft(240));
        topRow.removeFromLeft(12);
        resetClipsBtn.setBounds(topRow.removeFromLeft(140));

        bounds.removeFromTop(16);

        // 5 cards stacked vertically
        StageCard* cards[] = { &stageA, &stageB, &stageC, &stageD, &stageE };
        const int cardH = 68;
        const int gap = 10;

        for (auto* c : cards)
        {
            auto cardArea = bounds.removeFromTop(cardH);
            bounds.removeFromTop(gap);

            c->titleLabel.setBounds(cardArea.removeFromTop(24).reduced(2, 0));
            cardArea.removeFromTop(4);

            auto dataRow = cardArea;
            c->peakLabel.setBounds(dataRow.removeFromLeft(dataRow.getWidth() / 3));
            c->rmsLabel.setBounds(dataRow.removeFromLeft(dataRow.getWidth() / 2));
            c->statusLabel.setBounds(dataRow);
        }
    }

    void StageInspectorDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());

        // Draw card containers
        auto bounds = getLocalBounds().reduced(16, 16);
        bounds.removeFromTop(32 + 16);

        const int cardH = 68;
        const int gap = 10;

        for (int i = 0; i < 5; ++i)
        {
            auto cardArea = bounds.removeFromTop(cardH).toFloat();
            bounds.removeFromTop(gap);

            g.setColour(DSDLookAndFeel::getConsoleStripBg());
            g.fillRoundedRectangle(cardArea, 4.0f);
            g.setColour(DSDLookAndFeel::getConsoleBevel());
            g.drawRoundedRectangle(cardArea, 4.0f, 1.0f);
        }
    }
} // namespace dsd
