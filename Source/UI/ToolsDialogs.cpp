#include "UI/ToolsDialogs.h"
#include "UI/DSDLookAndFeel.h"
#include "Session/SceneManager.h"

namespace dsd
{
    namespace
    {
        void styleLabel(juce::Label& l, float size, bool bold, juce::Colour c)
        {
            l.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
            l.setColour(juce::Label::textColourId, c);
        }

        void styleButton(juce::TextButton& b)
        {
            b.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getButtonOffBg());
            b.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentAmber());
            b.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
            b.setColour(juce::TextButton::textColourOnId, DSDLookAndFeel::getTextPrimary());
        }

        std::unique_ptr<juce::Slider> makeSlider(double min, double max, double step, double value, const juce::String& suffix)
        {
            auto s = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            s->setRange(min, max, step);
            s->setValue(value, juce::dontSendNotification);
            s->setTextValueSuffix(suffix);
            s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 18);
            s->setColour(juce::Slider::textBoxTextColourId, DSDLookAndFeel::getTextPrimary());
            s->setColour(juce::Slider::textBoxOutlineColourId, DSDLookAndFeel::getConsoleBevel());
            s->setColour(juce::Slider::trackColourId, DSDLookAndFeel::getAccentBlue());
            return s;
        }

        juce::String formatLufs(float v)
        {
            return v <= -199.0f ? juce::String("--.-") : juce::String(v, 1);
        }
    } // namespace

    // =========================================================================
    // ScenesDialog
    // =========================================================================
    ScenesDialog::ScenesDialog(AudioEngine& engine) : engineRef(engine)
    {
        title.setText("SCENES - MIX SNAPSHOTS", juce::dontSendNotification);
        styleLabel(title, 13.0f, true, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(title);

        help.setText("A scene stores levels, mutes, channel switches, plugin bypass, routing (incl. PRE/POST), output levels, "
                     "limiters, ducking and PDC. Sources, devices and loaded plugins are not changed by a recall. "
                     "Double-click a name to rename.",
                     juce::dontSendNotification);
        styleLabel(help, 10.5f, false, DSDLookAndFeel::getTextSecondary());
        help.setJustificationType(juce::Justification::topLeft);
        help.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(help);

        for (int i = 0; i < SceneManager::kNumScenes; ++i)
        {
            Row r;
            r.number = std::make_unique<juce::Label>("", juce::String(i + 1));
            styleLabel(*r.number, 12.0f, true, DSDLookAndFeel::getTextSecondary());
            r.number->setJustificationType(juce::Justification::centred);

            r.name = std::make_unique<juce::Label>();
            styleLabel(*r.name, 12.0f, true, DSDLookAndFeel::getTextPrimary());
            r.name->setEditable(false, true, false);
            r.name->setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getConsoleStripBg());
            r.name->onTextChange = [this, i, lbl = r.name.get()]() { SceneManager::getInstance().setName(i, lbl->getText()); refresh(); };

            r.info = std::make_unique<juce::Label>();
            styleLabel(*r.info, 10.5f, false, DSDLookAndFeel::getTextSecondary());

            r.store = std::make_unique<juce::TextButton>("STORE");
            r.recall = std::make_unique<juce::TextButton>("RECALL");
            r.clear = std::make_unique<juce::TextButton>("CLEAR");
            styleButton(*r.store);
            styleButton(*r.recall);
            styleButton(*r.clear);

            r.store->onClick = [this, i]()
            {
                auto& sm = SceneManager::getInstance();
                auto doStore = [this, i]() { SceneManager::getInstance().store(i, engineRef); refresh(); };
                if (!sm.isUsed(i))
                {
                    doStore();
                    return;
                }
                juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Overwrite scene",
                    "Replace \"" + sm.getName(i) + "\" with the current mix?", "Overwrite", "Cancel", this,
                    juce::ModalCallbackFunction::create([doStore](int result) { if (result == 1) doStore(); }));
            };
            r.recall->onClick = [this, i]() { SceneManager::getInstance().recall(i, engineRef); refresh(); };
            r.clear->onClick = [this, i]()
            {
                juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Clear scene",
                    "Clear \"" + SceneManager::getInstance().getName(i) + "\"?", "Clear", "Cancel", this,
                    juce::ModalCallbackFunction::create([this, i](int result)
                    {
                        if (result == 1) { SceneManager::getInstance().clear(i); refresh(); }
                    }));
            };

            for (juce::Component* c : { static_cast<juce::Component*>(r.number.get()), static_cast<juce::Component*>(r.name.get()),
                                        static_cast<juce::Component*>(r.info.get()), static_cast<juce::Component*>(r.store.get()),
                                        static_cast<juce::Component*>(r.recall.get()), static_cast<juce::Component*>(r.clear.get()) })
                addAndMakeVisible(c);

            rows.push_back(std::move(r));
        }

        refresh();
        startTimerHz(4);
        setSize(620, 120 + SceneManager::kNumScenes * 34);
    }

    ScenesDialog::~ScenesDialog() { stopTimer(); }

    void ScenesDialog::timerCallback() { refresh(); }

    void ScenesDialog::refresh()
    {
        auto& sm = SceneManager::getInstance();
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        {
            auto& r = rows[static_cast<size_t>(i)];
            if (!r.name->isBeingEdited())
                r.name->setText(sm.getName(i), juce::dontSendNotification);
            const bool used = sm.isUsed(i);
            r.info->setText(used ? "stored " + sm.getStoredTime(i).formatted("%d %b %H:%M") : juce::String("empty"),
                            juce::dontSendNotification);
            r.recall->setEnabled(used);
            r.clear->setEnabled(used);
            const bool live = sm.getLastRecalled() == i;
            r.recall->setToggleState(live, juce::dontSendNotification);
            r.number->setColour(juce::Label::textColourId, live ? DSDLookAndFeel::getAccentAmber() : DSDLookAndFeel::getTextSecondary());
        }
    }

    void ScenesDialog::resized()
    {
        auto b = getLocalBounds().reduced(16, 12);
        title.setBounds(b.removeFromTop(22));
        help.setBounds(b.removeFromTop(48));
        b.removeFromTop(8);
        for (auto& r : rows)
        {
            auto row = b.removeFromTop(28);
            b.removeFromTop(6);
            r.number->setBounds(row.removeFromLeft(28));
            r.clear->setBounds(row.removeFromRight(64));
            row.removeFromRight(6);
            r.recall->setBounds(row.removeFromRight(72));
            row.removeFromRight(6);
            r.store->setBounds(row.removeFromRight(64));
            row.removeFromRight(10);
            r.info->setBounds(row.removeFromRight(130));
            row.removeFromRight(6);
            r.name->setBounds(row);
        }
    }

    void ScenesDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
    }

    // =========================================================================
    // AutoDuckDialog
    // =========================================================================
    AutoDuckDialog::AutoDuckDialog(AudioEngine& engine) : engineRef(engine)
    {
        title.setText("AUTO-DUCKING", juce::dontSendNotification);
        styleLabel(title, 13.0f, true, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(title);

        help.setText("When the trigger channel's post-fader level passes the threshold, the selected channels are lowered by the depth. "
                     "A muted or switched-off trigger never ducks.",
                     juce::dontSendNotification);
        styleLabel(help, 10.5f, false, DSDLookAndFeel::getTextSecondary());
        help.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(help);

        auto& chMgr = engineRef.getChannelManager();
        for (int ri = 0; ri < AutoDucker::kMaxRules; ++ri)
        {
            auto& p = panels[static_cast<size_t>(ri)];
            auto& rule = engineRef.getAutoDucker().getRule(ri);

            p.enable = std::make_unique<juce::ToggleButton>("Rule " + juce::String(ri + 1));
            p.enable->setColour(juce::ToggleButton::textColourId, DSDLookAndFeel::getTextPrimary());
            p.enable->setColour(juce::ToggleButton::tickColourId, DSDLookAndFeel::getAccentGreen());
            p.enable->onClick = [&rule, btn = p.enable.get()]() { rule.enabled.store(btn->getToggleState()); };
            addAndMakeVisible(*p.enable);

            p.trigger = std::make_unique<juce::ComboBox>();
            p.trigger->addItem("(no trigger)", 1);
            for (int c = 0; c < chMgr.getNumChannels() && c < 16; ++c)
            {
                const auto* ch = chMgr.getChannel(c);
                p.trigger->addItem(juce::String::formatted("CH%02d  ", c + 1) + (ch ? juce::String(ch->getName()) : juce::String()), c + 2);
            }
            p.trigger->onChange = [&rule, box = p.trigger.get()]() { rule.triggerChannel.store(box->getSelectedId() - 2); };
            addAndMakeVisible(*p.trigger);

            for (int c = 0; c < 16; ++c)
            {
                auto t = std::make_unique<juce::TextButton>(juce::String(c + 1));
                styleButton(*t);
                t->setClickingTogglesState(true);
                t->setTooltip("Duck channel " + juce::String(c + 1));
                t->onClick = [&rule, c, btn = t.get()]()
                {
                    uint32_t m = rule.targetMask.load();
                    m = btn->getToggleState() ? (m | (1u << c)) : (m & ~(1u << c));
                    rule.targetMask.store(m);
                };
                addAndMakeVisible(*t);
                p.targets[static_cast<size_t>(c)] = std::move(t);
            }

            p.threshold = makeSlider(-60.0, 0.0, 0.5, rule.thresholdDb.load(), " dB");
            p.threshold->onValueChange = [&rule, s = p.threshold.get()]() { rule.thresholdDb.store(static_cast<float>(s->getValue())); };
            p.depth = makeSlider(-40.0, 0.0, 0.5, rule.depthDb.load(), " dB");
            p.depth->onValueChange = [&rule, s = p.depth.get()]() { rule.depthDb.store(static_cast<float>(s->getValue())); };
            p.attack = makeSlider(1.0, 200.0, 1.0, rule.attackMs.load(), " ms");
            p.attack->onValueChange = [&rule, s = p.attack.get()]() { rule.attackMs.store(static_cast<float>(s->getValue())); };
            p.hold = makeSlider(0.0, 2000.0, 10.0, rule.holdMs.load(), " ms");
            p.hold->onValueChange = [&rule, s = p.hold.get()]() { rule.holdMs.store(static_cast<float>(s->getValue())); };
            p.release = makeSlider(20.0, 3000.0, 10.0, rule.releaseMs.load(), " ms");
            p.release->onValueChange = [&rule, s = p.release.get()]() { rule.releaseMs.store(static_cast<float>(s->getValue())); };
            for (auto* s : { p.threshold.get(), p.depth.get(), p.attack.get(), p.hold.get(), p.release.get() })
            {
                s->setLookAndFeel(&sliderLook);
                addAndMakeVisible(*s);
            }

            p.gr = std::make_unique<juce::Label>();
            styleLabel(*p.gr, 11.0f, true, DSDLookAndFeel::getTextSecondary());
            p.gr->setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(*p.gr);
        }

        loadFromEngine();
        startTimerHz(15);
        setSize(860, 90 + AutoDucker::kMaxRules * 132);
    }

    AutoDuckDialog::~AutoDuckDialog()
    {
        stopTimer();
        for (auto& p : panels)
            for (auto* s : { p.threshold.get(), p.depth.get(), p.attack.get(), p.hold.get(), p.release.get() })
                if (s != nullptr)
                    s->setLookAndFeel(nullptr);
    }

    void AutoDuckDialog::loadFromEngine()
    {
        for (int ri = 0; ri < AutoDucker::kMaxRules; ++ri)
        {
            auto& p = panels[static_cast<size_t>(ri)];
            const auto& rule = engineRef.getAutoDucker().getRule(ri);
            p.enable->setToggleState(rule.enabled.load(), juce::dontSendNotification);
            p.trigger->setSelectedId(rule.triggerChannel.load() + 2, juce::dontSendNotification);
            const uint32_t mask = rule.targetMask.load();
            for (int c = 0; c < 16; ++c)
                p.targets[static_cast<size_t>(c)]->setToggleState(((mask >> c) & 1u) != 0, juce::dontSendNotification);
            p.threshold->setValue(rule.thresholdDb.load(), juce::dontSendNotification);
            p.depth->setValue(rule.depthDb.load(), juce::dontSendNotification);
            p.attack->setValue(rule.attackMs.load(), juce::dontSendNotification);
            p.hold->setValue(rule.holdMs.load(), juce::dontSendNotification);
            p.release->setValue(rule.releaseMs.load(), juce::dontSendNotification);
        }
    }

    void AutoDuckDialog::timerCallback()
    {
        for (int ri = 0; ri < AutoDucker::kMaxRules; ++ri)
        {
            auto& p = panels[static_cast<size_t>(ri)];
            const auto& rule = engineRef.getAutoDucker().getRule(ri);
            const float db = rule.currentGainDb.load();
            const bool trig = rule.triggered.load();
            p.gr->setText(rule.enabled.load() ? ((trig ? "DUCKING " : "idle ") + juce::String(db, 1) + " dB") : juce::String("off"),
                          juce::dontSendNotification);
            p.gr->setColour(juce::Label::textColourId, trig ? DSDLookAndFeel::getAccentAmber() : DSDLookAndFeel::getTextSecondary());
        }
        repaint();
    }

    void AutoDuckDialog::resized()
    {
        auto b = getLocalBounds().reduced(16, 12);
        title.setBounds(b.removeFromTop(22));
        help.setBounds(b.removeFromTop(32));
        b.removeFromTop(6);

        for (auto& p : panels)
        {
            auto area = b.removeFromTop(124);
            b.removeFromTop(8);
            p.bounds = area;
            area = area.reduced(10, 8);

            auto top = area.removeFromTop(24);
            p.enable->setBounds(top.removeFromLeft(90));
            top.removeFromLeft(6);
            juce::Label* dummy = nullptr;
            (void)dummy;
            p.trigger->setBounds(top.removeFromLeft(210));
            p.gr->setBounds(top.removeFromRight(170));

            area.removeFromTop(6);
            auto targetsRow = area.removeFromTop(22);
            const int tw = std::max(24, targetsRow.getWidth() / 16 - 2);
            for (auto& t : p.targets)
            {
                t->setBounds(targetsRow.removeFromLeft(tw));
                targetsRow.removeFromLeft(2);
            }

            area.removeFromTop(8);
            auto sliders = area.removeFromTop(48);
            const int sw = sliders.getWidth() / 5;
            for (auto* s : { p.threshold.get(), p.depth.get(), p.attack.get(), p.hold.get(), p.release.get() })
            {
                auto cell = sliders.removeFromLeft(sw).reduced(4, 0);
                cell.removeFromTop(16); // caption painted above
                s->setBounds(cell.removeFromTop(26));
            }
        }
    }

    void AutoDuckDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
        static const char* captions[] = { "THRESHOLD", "DEPTH", "ATTACK", "HOLD", "RELEASE" };
        for (const auto& p : panels)
        {
            g.setColour(DSDLookAndFeel::getConsoleStripBg());
            g.fillRoundedRectangle(p.bounds.toFloat(), 4.0f);
            g.setColour(DSDLookAndFeel::getConsoleBevel());
            g.drawRoundedRectangle(p.bounds.toFloat(), 4.0f, 1.0f);

            auto area = p.bounds.reduced(10, 8);
            area.removeFromTop(24 + 6 + 22 + 8);
            auto sliders = area.removeFromTop(48);
            const int sw = sliders.getWidth() / 5;
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.setColour(DSDLookAndFeel::getTextSecondary());
            for (int k = 0; k < 5; ++k)
                g.drawText(captions[k], sliders.removeFromLeft(sw).reduced(4, 0).removeFromTop(16), juce::Justification::centredLeft);
        }
    }

    // =========================================================================
    // LoudnessDialog
    // =========================================================================
    LoudnessDialog::LoudnessDialog(AudioEngine& engine) : engineRef(engine)
    {
        title.setText("OUTPUT LOUDNESS (BS.1770 / EBU R128) & TRUE-PEAK LIMITERS", juce::dontSendNotification);
        styleLabel(title, 13.0f, true, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(title);

        targetLabel.setText("Target:", juce::dontSendNotification);
        styleLabel(targetLabel, 11.0f, true, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(targetLabel);

        targetBox.addItem("-23 LUFS (EBU R128 broadcast)", 1);
        targetBox.addItem("-24 LUFS (ATSC A/85)", 2);
        targetBox.addItem("-16 LUFS (podcast)", 3);
        targetBox.addItem("-14 LUFS (streaming platforms)", 4);
        targetBox.setSelectedId(1, juce::dontSendNotification);
        targetBox.onChange = [this]()
        {
            static const float values[] = { -23.0f, -24.0f, -16.0f, -14.0f };
            targetLufs = values[std::clamp(targetBox.getSelectedId() - 1, 0, 3)];
        };
        addAndMakeVisible(targetBox);

        auto& outMgr = engineRef.getOutputManager();
        for (int i = 0; i < outMgr.getNumOutputs(); ++i)
        {
            Column c;
            auto* bus = outMgr.getOutput(i);

            c.name = std::make_unique<juce::Label>();
            styleLabel(*c.name, 12.0f, true, DSDLookAndFeel::getTextPrimary());
            c.name->setJustificationType(juce::Justification::centred);

            auto big = [](std::unique_ptr<juce::Label>& l, float size)
            {
                l = std::make_unique<juce::Label>();
                l->setFont(juce::FontOptions(size, juce::Font::bold));
                l->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
                l->setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
                l->setJustificationType(juce::Justification::centred);
            };
            big(c.momentary, 13.0f);
            big(c.shortTerm, 13.0f);
            big(c.integrated, 20.0f);
            big(c.truePeak, 13.0f);

            c.reset = std::make_unique<juce::TextButton>("RESET");
            styleButton(*c.reset);
            c.reset->setTooltip("Restart integrated loudness and max true peak");
            c.reset->onClick = [bus]() { if (bus) bus->getLoudnessMeter().requestReset(); };

            c.limiterOn = std::make_unique<juce::ToggleButton>("True-peak limiter");
            c.limiterOn->setColour(juce::ToggleButton::textColourId, DSDLookAndFeel::getTextPrimary());
            c.limiterOn->setColour(juce::ToggleButton::tickColourId, DSDLookAndFeel::getAccentGreen());
            c.limiterOn->setToggleState(bus && bus->getLimiter().isEnabled(), juce::dontSendNotification);
            c.limiterOn->setTooltip("Adds about 1.6 ms latency to this output while enabled");
            c.limiterOn->onClick = [bus, t = c.limiterOn.get()]() { if (bus) bus->getLimiter().setEnabled(t->getToggleState()); };

            c.ceiling = makeSlider(-6.0, 0.0, 0.1, bus ? bus->getLimiter().getCeilingDb() : -1.0, " dBTP");
            c.ceiling->setLookAndFeel(&sliderLook);
            c.ceiling->onValueChange = [bus, s = c.ceiling.get()]() { if (bus) bus->getLimiter().setCeilingDb(static_cast<float>(s->getValue())); };

            c.gr = std::make_unique<juce::Label>();
            styleLabel(*c.gr, 11.0f, true, DSDLookAndFeel::getTextSecondary());
            c.gr->setJustificationType(juce::Justification::centred);

            for (juce::Component* comp : { static_cast<juce::Component*>(c.name.get()), static_cast<juce::Component*>(c.momentary.get()),
                                           static_cast<juce::Component*>(c.shortTerm.get()), static_cast<juce::Component*>(c.integrated.get()),
                                           static_cast<juce::Component*>(c.truePeak.get()), static_cast<juce::Component*>(c.reset.get()),
                                           static_cast<juce::Component*>(c.limiterOn.get()), static_cast<juce::Component*>(c.ceiling.get()),
                                           static_cast<juce::Component*>(c.gr.get()) })
                addAndMakeVisible(comp);

            columns.push_back(std::move(c));
        }

        timerCallback();
        startTimerHz(10);
        setSize(std::max(640, 40 + static_cast<int>(columns.size()) * 190), 420);
    }

    LoudnessDialog::~LoudnessDialog()
    {
        stopTimer();
        for (auto& c : columns)
            if (c.ceiling != nullptr)
                c.ceiling->setLookAndFeel(nullptr);
    }

    void LoudnessDialog::timerCallback()
    {
        auto& outMgr = engineRef.getOutputManager();
        for (int i = 0; i < static_cast<int>(columns.size()); ++i)
        {
            auto& c = columns[static_cast<size_t>(i)];
            const auto* bus = outMgr.getOutput(i);
            if (bus == nullptr)
                continue;

            c.name->setText(juce::String::formatted("OUT %02d  ", i + 1) + juce::String(bus->getName()), juce::dontSendNotification);
            const auto& m = bus->getLoudnessMeter();
            c.momentary->setText("M  " + formatLufs(m.getMomentaryLufs()), juce::dontSendNotification);
            c.shortTerm->setText("S  " + formatLufs(m.getShortTermLufs()), juce::dontSendNotification);

            const float integrated = m.getIntegratedLufs();
            c.integrated->setText("I  " + formatLufs(integrated) + " LUFS", juce::dontSendNotification);
            juce::Colour ic = DSDLookAndFeel::getTextOnOled();
            if (integrated > -199.0f)
                ic = std::abs(integrated - targetLufs) <= 1.0f ? DSDLookAndFeel::getMeterGreen()
                   : (integrated > targetLufs ? DSDLookAndFeel::getMeterRed() : DSDLookAndFeel::getMeterYellow());
            c.integrated->setColour(juce::Label::textColourId, ic);

            const float tp = m.getTruePeakMaxDb();
            c.truePeak->setText("TP max  " + (tp <= -199.0f ? juce::String("--.-") : juce::String(tp, 1)) + " dBTP", juce::dontSendNotification);
            const float ceiling = bus->getLimiter().getCeilingDb();
            c.truePeak->setColour(juce::Label::textColourId, tp > ceiling + 0.05f ? DSDLookAndFeel::getMeterRed() : DSDLookAndFeel::getTextOnOled());

            if (!c.ceiling->isMouseButtonDown())
                c.ceiling->setValue(ceiling, juce::dontSendNotification);
            c.limiterOn->setToggleState(bus->getLimiter().isEnabled(), juce::dontSendNotification);
            const float gr = bus->getLimiter().getGainReductionDb();
            c.gr->setText(bus->getLimiter().isEnabled() ? ("GR " + juce::String(gr, 1) + " dB") : juce::String("limiter off"),
                          juce::dontSendNotification);
            c.gr->setColour(juce::Label::textColourId, gr < -0.1f ? DSDLookAndFeel::getAccentAmber() : DSDLookAndFeel::getTextSecondary());
        }
    }

    void LoudnessDialog::resized()
    {
        auto b = getLocalBounds().reduced(16, 12);
        auto top = b.removeFromTop(24);
        targetBox.setBounds(top.removeFromRight(230));
        targetLabel.setBounds(top.removeFromRight(56));
        title.setBounds(top);
        b.removeFromTop(12);

        const int n = std::max(1, static_cast<int>(columns.size()));
        const int w = b.getWidth() / n;
        for (auto& c : columns)
        {
            auto col = b.removeFromLeft(w).reduced(6, 0);
            c.bounds = col;
            col = col.reduced(8, 8);
            c.name->setBounds(col.removeFromTop(22));
            col.removeFromTop(6);
            c.momentary->setBounds(col.removeFromTop(28));
            col.removeFromTop(4);
            c.shortTerm->setBounds(col.removeFromTop(28));
            col.removeFromTop(4);
            c.integrated->setBounds(col.removeFromTop(44));
            col.removeFromTop(4);
            c.truePeak->setBounds(col.removeFromTop(28));
            col.removeFromTop(6);
            c.reset->setBounds(col.removeFromTop(24).withSizeKeepingCentre(90, 24));
            col.removeFromTop(16);
            c.limiterOn->setBounds(col.removeFromTop(24));
            col.removeFromTop(4);
            c.ceiling->setBounds(col.removeFromTop(24));
            col.removeFromTop(4);
            c.gr->setBounds(col.removeFromTop(22));
        }
    }

    void LoudnessDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
        for (const auto& c : columns)
        {
            g.setColour(DSDLookAndFeel::getConsoleStripBg());
            g.fillRoundedRectangle(c.bounds.toFloat(), 4.0f);
            g.setColour(DSDLookAndFeel::getConsoleBevel());
            g.drawRoundedRectangle(c.bounds.toFloat(), 4.0f, 1.0f);
        }
    }
} // namespace dsd
