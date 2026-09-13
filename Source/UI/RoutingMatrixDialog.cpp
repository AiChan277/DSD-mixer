#include "UI/RoutingMatrixDialog.h"
#include "UI/DSDLookAndFeel.h"
#include "DSP/GainProcessor.h"
#include <cmath>

namespace dsd
{
    // ==============================================================================
    // DHD Broadcast Console Grayscale Layering (Disciplined 5 Levels)
    // ==============================================================================
    namespace
    {
        const juce::Colour colAppBg     (0xffF5F6F7); // 1. Main window background
        const juce::Colour colHeaderBg  (0xffECEEF0); // 2. Header & Column header background
        const juce::Colour colRowEven   (0xffFFFFFF); // 3a. Row even background
        const juce::Colour colRowOdd    (0xffF8F8F8); // 3b. Row odd background
        const juce::Colour colRowSelect (0xffEAECEE); // 4. Selected row background
        const juce::Colour colSeparator (0xffD9DDE1); // 5. Separator line (crisp 1px)
        const juce::Colour colAccent    (0xffFF6D00); // Authentic Studio Amber/Orange accent
        const juce::Colour colTextPri   (0xff20242A); // Primary dark text
        const juce::Colour colTextSec   (0xff7A828C); // Secondary muted text
    }

    // ==============================================================================
    // MatrixLookAndFeel Implementation - Pure Box Shapes & Horizontal Console Fader
    // ==============================================================================
    MatrixLookAndFeel::MatrixLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, colHeaderBg);
        setColour(juce::ComboBox::textColourId, colTextPri);
        setColour(juce::ComboBox::outlineColourId, colSeparator);
        setColour(juce::ComboBox::arrowColourId, colTextPri);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xffFFFFFF));
        setColour(juce::PopupMenu::textColourId, colTextPri);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffEAECEE));
        setColour(juce::PopupMenu::highlightedTextColourId, colTextPri);
    }

    void MatrixLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat();

        // 1. Pure sharp rectangular box (0px corner radius)
        juce::Colour bg = button.findColour(juce::TextButton::buttonColourId);
        if (shouldDrawButtonAsDown)
            bg = button.findColour(juce::TextButton::buttonOnColourId);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter(0.04f);

        g.setColour(bg);
        g.fillRect(bounds);

        // 2. Sharp 1px border
        juce::Colour borderCol = colSeparator;
        if (button.getButtonText().equalsIgnoreCase("Clear All") || button.getButtonText().equalsIgnoreCase("Delete"))
        {
            if (shouldDrawButtonAsHighlighted)
                borderCol = juce::Colour(0xffDC2626);
            else
                borderCol = colSeparator;
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            borderCol = juce::Colour(0xff9CA3AF);
        }

        g.setColour(borderCol);
        g.drawRect(bounds, 1.0f);
    }

    void MatrixLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox& /*box*/)
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

        // Pure sharp rectangular box (0px corner radius)
        g.setColour(colHeaderBg);
        g.fillRect(bounds);

        g.setColour(colSeparator);
        g.drawRect(bounds, 1.0f);

        // Dropdown arrow (sharp triangle)
        auto arrowZone = juce::Rectangle<float>(static_cast<float>(buttonX), static_cast<float>(buttonY),
                                               static_cast<float>(buttonW), static_cast<float>(buttonH));
        juce::Path p;
        const float ax = arrowZone.getCentreX();
        const float ay = arrowZone.getCentreY();
        p.startNewSubPath(ax - 3.5f, ay - 1.5f);
        p.lineTo(ax + 3.5f, ay - 1.5f);
        p.lineTo(ax, ay + 2.5f);
        p.closeSubPath();

        g.setColour(colTextPri);
        g.fillPath(p);
    }

    void MatrixLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds(8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, colTextPri); // Ensure crisp, bold dark text!
    }

    void MatrixLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            const juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        {
            DSDLookAndFeel::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float trackCenterY = std::floor(static_cast<float>(y) + static_cast<float>(height) * 0.50f);
        const float trackHeight = 5.0f;
        const float trackY = trackCenterY - trackHeight * 0.5f;

        const float minPos = static_cast<float>(slider.getPositionOfValue(slider.getMinimum()));
        const float maxPos = static_cast<float>(slider.getPositionOfValue(slider.getMaximum()));
        const float trackLeft = std::min(minPos, maxPos);
        const float trackWidth = std::abs(maxPos - minPos);

        // 1. Recessed dark track slot on console chassis
        g.setColour(juce::Colour(0xff14161A));
        g.fillRect(trackLeft - 1.0f, trackY - 0.5f, trackWidth + 2.0f, trackHeight + 1.0f);

        g.setColour(juce::Colour(0xff22252C));
        g.fillRect(trackLeft, trackY, trackWidth, trackHeight);

        // Center silver hairline guide
        g.setColour(juce::Colour(0xff555963));
        g.drawHorizontalLine(static_cast<int>(trackCenterY), trackLeft + 1.0f, trackLeft + trackWidth - 1.0f);

        // Unity Gain (0.0 dB) amber tick mark on track
        const float zeroPos = static_cast<float>(slider.getPositionOfValue(0.0));
        g.setColour(colAccent);
        g.drawVerticalLine(static_cast<int>(zeroPos), trackY - 3.5f, trackY + trackHeight + 3.5f);

        // ========================================================================
        // 2. Horizontal Broadcast Console Fader Cap (DHD RX2/SX2 Chamfered Block)
        // ========================================================================
        const float capWidth = 26.0f;
        const float capHeight = 22.0f;
        const float capX = sliderPos - capWidth * 0.5f;
        const float capY = trackCenterY - capHeight * 0.5f;

        // Drop shadow under cap
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(capX + 1.5f, capY + 2.5f, capWidth, capHeight);

        // 3-stage horizontal chamfer profile
        const float chamferW = 5.0f;

        // Left chamfer (specular slope from left)
        auto leftChamfer = juce::Rectangle<float>(capX, capY, chamferW, capHeight);
        juce::ColourGradient leftGrad(juce::Colour(0xff3C4048), capX, capY,
                                      juce::Colour(0xff22252B), capX + chamferW, capY, false);
        g.setGradientFill(leftGrad);
        g.fillRect(leftChamfer);

        // Right chamfer (shadow slope towards right)
        auto rightChamfer = juce::Rectangle<float>(capX + capWidth - chamferW, capY, chamferW, capHeight);
        juce::ColourGradient rightGrad(juce::Colour(0xff181A1E), capX + capWidth - chamferW, capY,
                                       juce::Colour(0xff0D0E10), capX + capWidth, capY, false);
        g.setGradientFill(rightGrad);
        g.fillRect(rightChamfer);

        // Center main block: matte dark broadcast charcoal
        auto centerBlock = juce::Rectangle<float>(capX + chamferW, capY, capWidth - 2.0f * chamferW, capHeight);
        juce::ColourGradient midGrad(juce::Colour(0xff24272E), centerBlock.getX(), capY,
                                     juce::Colour(0xff181A1E), centerBlock.getRight(), capY, false);
        g.setGradientFill(midGrad);
        g.fillRect(centerBlock);

        // Tactile finger-grip ridges (vertical grooves)
        const float midX = capX + capWidth * 0.5f;
        g.setColour(juce::Colour(0xff121316));
        g.drawVerticalLine(static_cast<int>(midX - 5.0f), capY + 3.0f, capY + capHeight - 3.0f);
        g.drawVerticalLine(static_cast<int>(midX + 5.0f), capY + 3.0f, capY + capHeight - 3.0f);
        g.setColour(juce::Colour(0xff30343D));
        g.drawVerticalLine(static_cast<int>(midX - 4.0f), capY + 3.0f, capY + capHeight - 3.0f);
        g.drawVerticalLine(static_cast<int>(midX + 6.0f), capY + 3.0f, capY + capHeight - 3.0f);

        // Outer border
        g.setColour(juce::Colour(0xff40444D));
        g.drawRect(capX, capY, capWidth, capHeight, 1.0f);

        // Top edge highlight hairline
        g.setColour(juce::Colour(0xff5E6472));
        g.drawHorizontalLine(static_cast<int>(capY), capX + 1.0f, capX + capWidth - 1.0f);

        // 3. Center White Position Indicator Line (Crisp 2.0px Pure White)
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRect(midX, capY + 1.0f, 2.0f, capHeight - 2.0f);
        g.setColour(juce::Colours::white);
        g.fillRect(midX - 0.5f, capY + 1.0f, 2.0f, capHeight - 2.0f);
    }

    // ==============================================================================
    // MatrixCanvas Implementation - DHD Broadcast Console Topology
    // ==============================================================================
    RoutingMatrixDialog::MatrixCanvas::MatrixCanvas(RoutingEngine& router, ChannelManager& chMgr, OutputManager& outMgr,
                                                   std::function<void(int ch, int out)> onCellSelected)
        : routerRef(router), chMgrRef(chMgr), outMgrRef(outMgr), cellSelectedCallback(std::move(onCellSelected))
    {
        setOpaque(true);
    }

    void RoutingMatrixDialog::MatrixCanvas::setSelectedCell(int ch, int out)
    {
        selectedRow = std::clamp(ch, 0, NUM_CHANNELS_LEVEL1 - 1);
        selectedCol = std::clamp(out, 0, NUM_OUTPUT_BUSES_LEVEL1 - 1);
        repaint();
    }

    int RoutingMatrixDialog::MatrixCanvas::getColWidth() const noexcept
    {
        const int availableW = getWidth() - LEFT_HEADER_W;
        return std::max(MIN_COL_WIDTH, availableW / NUM_OUTPUT_BUSES_LEVEL1);
    }

    void RoutingMatrixDialog::MatrixCanvas::drawCheckmark(juce::Graphics& g, const juce::Rectangle<float>& box, const juce::Colour& colour)
    {
        juce::Path p;
        const float bx = box.getX();
        const float by = box.getY();
        const float bw = box.getWidth();
        const float bh = box.getHeight();

        p.startNewSubPath(bx + bw * 0.24f, by + bh * 0.52f);
        p.lineTo(bx + bw * 0.44f, by + bh * 0.72f);
        p.lineTo(bx + bw * 0.76f, by + bh * 0.28f);

        g.setColour(colour);
        g.strokePath(p, juce::PathStrokeType(2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

    void RoutingMatrixDialog::MatrixCanvas::drawColumnHeader(juce::Graphics& g, const juce::String& busNum, const juce::String& busName,
                                                            float x, float y, float width, float height,
                                                            bool isHovered, bool isSelected)
    {
        // 1. Column header surface (sharp box)
        g.setColour(isSelected ? colRowSelect : (isHovered ? colAppBg : colHeaderBg));
        g.fillRect(x, y, width, height);

        // 1px Border (sharp box - 0px radius)
        g.setColour(colSeparator);
        g.drawRect(x, y, width, height, 1.0f);

        // 2. Line 1: OUT 1 (subdued small bold caps)
        auto topRect = juce::Rectangle<float>(x, y + 6.0f, width, 14.0f);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(isSelected ? colAccent : colTextSec);
        g.drawText(busNum.toUpperCase(), topRect.toNearestInt(), juce::Justification::centred, false);

        // 3. Line 2: STUDIO MONITOR (two-line clean horizontal layout)
        auto btmRect = juce::Rectangle<float>(x, y + 21.0f, width, 18.0f);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(colTextPri);
        g.drawText(busName.toUpperCase(), btmRect.reduced(4.0f, 0.0f).toNearestInt(), juce::Justification::centred, true);
    }

    void RoutingMatrixDialog::MatrixCanvas::paint(juce::Graphics& g)
    {
        // 1. Main background
        g.fillAll(colAppBg);

        const int numCh = NUM_CHANNELS_LEVEL1;
        const int numOut = NUM_OUTPUT_BUSES_LEVEL1;
        const int colW = getColWidth();

        // 2. Top-Left Corner Header (Sharp Box - SOURCES / 16 CHANNELS)
        auto cornerRect = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(LEFT_HEADER_W), static_cast<float>(TOP_HEADER_H));
        g.setColour(colHeaderBg);
        g.fillRect(cornerRect);
        g.setColour(colSeparator);
        g.drawRect(cornerRect, 1.0f);

        // Line 1: SOURCES
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(colTextPri);
        g.drawText("SOURCES", 14, 7, LEFT_HEADER_W - 20, 16, juce::Justification::centredLeft, false);

        // Line 2: 16 CHANNELS
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(colTextSec);
        g.drawText("16 CHANNELS", 14, 23, LEFT_HEADER_W - 20, 16, juce::Justification::centredLeft, false);

        // 3. Column Headers (Destinations - Two Horizontal Lines, Sharp Box)
        for (int out = 0; out < numOut; ++out)
        {
            auto* outPtr = outMgrRef.getOutput(out);
            juce::String outName = outPtr ? outPtr->getName() : ("OUT " + juce::String(out + 1));
            juce::String busNum = "OUT " + juce::String(out + 1);

            const float x = static_cast<float>(LEFT_HEADER_W + out * colW);
            const bool isHovered = (out == hoveredCol);
            const bool isSelected = (out == selectedCol);

            drawColumnHeader(g, busNum, outName, x, 0.0f, static_cast<float>(colW), static_cast<float>(TOP_HEADER_H),
                             isHovered, isSelected);
        }

        // 4. Row Headers & DHD Checkbox Grid
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float y = static_cast<float>(TOP_HEADER_H + ch * ROW_HEIGHT);
            const bool isRowHovered = (ch == hoveredRow);
            const bool isRowSelected = (ch == selectedRow);

            // Row background
            if (isRowSelected)
                g.setColour(colRowSelect);
            else if (isRowHovered)
                g.setColour(colAppBg);
            else
                g.setColour((ch % 2 == 0) ? colRowEven : colRowOdd);

            g.fillRect(0.0f, y, static_cast<float>(getWidth()), static_cast<float>(ROW_HEIGHT));

            // 1px Horizontal separator line
            g.setColour(colSeparator);
            g.drawHorizontalLine(static_cast<int>(y + ROW_HEIGHT - 1), 0.0f, static_cast<float>(getWidth()));

            // 1px Vertical separator line for channel header column
            g.drawVerticalLine(LEFT_HEADER_W, y, y + ROW_HEIGHT);

            // Selection indicator: 3px solid Amber accent bar on the left edge
            if (isRowSelected)
            {
                g.setColour(colAccent);
                g.fillRect(0.0f, y, 3.0f, static_cast<float>(ROW_HEIGHT));
            }

            // 01 -> kecil, light/secondary (colTextSec #7A828C, 9.5pt)
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.setColour(isRowSelected ? colAccent : colTextSec);
            g.drawText(juce::String::formatted("%02d", ch + 1), 14, static_cast<int>(y), 22, ROW_HEIGHT, juce::Justification::centredLeft, false);

            // MIC 01 -> lebih besar dan bold (colTextPri #20242A, 11.5pt bold)
            auto* chPtr = chMgrRef.getChannel(ch);
            juce::String chName = chPtr ? chPtr->getName() : ("CH " + juce::String(ch + 1));
            g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.setColour(colTextPri);
            g.drawText(chName, 38, static_cast<int>(y), LEFT_HEADER_W - 44, ROW_HEIGHT, juce::Justification::centredLeft, true);

            // DHD Checkboxes in this row (Pure sharp box without circular edges)
            for (int out = 0; out < numOut; ++out)
            {
                const float cellX = static_cast<float>(LEFT_HEADER_W + out * colW);
                const bool isCellSelected = (ch == selectedRow && out == selectedCol);
                const bool isCellHovered = (ch == hoveredRow && out == hoveredCol);

                // 1px Vertical column separator
                g.setColour(colSeparator);
                g.drawVerticalLine(static_cast<int>(cellX + colW - 1), y, y + ROW_HEIGHT);

                const bool isEnabled = routerRef.isRouteEnabled(ch, out);

                // Center DHD Checkbox (18x18px, pure sharp box, 0px corner radius)
                const float boxSize = 18.0f;
                const float boxX = cellX + (colW - boxSize) * 0.5f;
                const float boxY = y + (ROW_HEIGHT - boxSize) * 0.5f;
                auto boxRect = juce::Rectangle<float>(boxX, boxY, boxSize, boxSize);

                if (isEnabled)
                {
                    if (isCellSelected)
                        g.setColour(colAccent);  // Vibrant amber for selected connection
                    else
                        g.setColour(colTextPri); // Matte broadcast charcoal #20242A

                    g.fillRect(boxRect);
                    drawCheckmark(g, boxRect, juce::Colours::white);
                }
                else
                {
                    if (isCellHovered)
                    {
                        g.setColour(colAccent.withAlpha(0.18f));
                        g.fillRect(boxRect);
                        g.setColour(colAccent.withAlpha(0.70f));
                        g.drawRect(boxRect, 1.0f);
                        drawCheckmark(g, boxRect, colAccent.withAlpha(0.70f));
                    }
                    else
                    {
                        g.setColour(colSeparator);
                        g.drawRect(boxRect, 1.0f);
                    }
                }
            }
        }
    }

    void RoutingMatrixDialog::MatrixCanvas::mouseMove(const juce::MouseEvent& e)
    {
        int newHoveredCol = -1;
        int newHoveredRow = -1;
        const int colW = getColWidth();

        if (e.x >= LEFT_HEADER_W)
            newHoveredCol = (e.x - LEFT_HEADER_W) / colW;

        if (e.y >= TOP_HEADER_H)
            newHoveredRow = (e.y - TOP_HEADER_H) / ROW_HEIGHT;

        if (newHoveredCol >= NUM_OUTPUT_BUSES_LEVEL1)
            newHoveredCol = -1;
        if (newHoveredRow >= NUM_CHANNELS_LEVEL1)
            newHoveredRow = -1;

        if (newHoveredCol != hoveredCol || newHoveredRow != hoveredRow)
        {
            hoveredCol = newHoveredCol;
            hoveredRow = newHoveredRow;
            repaint();
        }
    }

    void RoutingMatrixDialog::MatrixCanvas::mouseExit(const juce::MouseEvent& /*e*/)
    {
        if (hoveredCol != -1 || hoveredRow != -1)
        {
            hoveredCol = -1;
            hoveredRow = -1;
            repaint();
        }
    }

    void RoutingMatrixDialog::MatrixCanvas::mouseDown(const juce::MouseEvent& e)
    {
        const int colW = getColWidth();
        if (e.x >= LEFT_HEADER_W && e.y >= TOP_HEADER_H)
        {
            const int col = (e.x - LEFT_HEADER_W) / colW;
            const int row = (e.y - TOP_HEADER_H) / ROW_HEIGHT;

            if (col >= 0 && col < NUM_OUTPUT_BUSES_LEVEL1 && row >= 0 && row < NUM_CHANNELS_LEVEL1)
            {
                selectedRow = row;
                selectedCol = col;

                // Click cell -> toggle route connection (Topology)
                const bool cur = routerRef.isRouteEnabled(row, col);
                routerRef.setRouteEnabled(row, col, !cur);

                if (cellSelectedCallback)
                    cellSelectedCallback(selectedRow, selectedCol);

                repaint();
            }
        }
        else if (e.y >= TOP_HEADER_H)
        {
            // Click on row header selects the row
            const int row = (e.y - TOP_HEADER_H) / ROW_HEIGHT;
            if (row >= 0 && row < NUM_CHANNELS_LEVEL1)
            {
                selectedRow = row;
                if (cellSelectedCallback)
                    cellSelectedCallback(selectedRow, selectedCol);
                repaint();
            }
        }
    }

    void RoutingMatrixDialog::MatrixCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        const int colW = getColWidth();
        if (e.x >= LEFT_HEADER_W && e.y >= TOP_HEADER_H)
        {
            const int col = (e.x - LEFT_HEADER_W) / colW;
            const int row = (e.y - TOP_HEADER_H) / ROW_HEIGHT;

            if (col >= 0 && col < NUM_OUTPUT_BUSES_LEVEL1 && row >= 0 && row < NUM_CHANNELS_LEVEL1)
            {
                selectedRow = row;
                selectedCol = col;

                const float step = (wheel.deltaY > 0.0f) ? 0.5f : ((wheel.deltaY < 0.0f) ? -0.5f : 0.0f);
                if (step != 0.0f)
                {
                    float curGain = routerRef.getRouteGainDb(row, col);
                    float newGain = std::clamp(curGain + step, -60.0f, 12.0f);
                    newGain = std::round(newGain * 10.0f) / 10.0f;
                    routerRef.setRouteGainDb(row, col, newGain);

                    if (cellSelectedCallback)
                        cellSelectedCallback(selectedRow, selectedCol);

                    repaint();
                }
            }
        }
    }

    // ==============================================================================
    // RoutingMatrixDialog Implementation
    // ==============================================================================
    RoutingMatrixDialog::RoutingMatrixDialog(RoutingEngine& router, ChannelManager& chMgr, OutputManager& outMgr)
        : routingEngineRef(router), channelManagerRef(chMgr), outputManagerRef(outMgr)
    {
        // Apply pure sharp box LookAndFeel (0px radius on all buttons and boxes)
        setLookAndFeel(&matrixLookAndFeel);

        // 1. Top Header Bar & Preset Controls
        titleLabel.setText("ROUTING MATRIX", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, colTextPri);
        addAndMakeVisible(titleLabel);

        presetBox.setTooltip("Select routing matrix preset");
        presetBox.setColour(juce::ComboBox::backgroundColourId, colHeaderBg);
        presetBox.setColour(juce::ComboBox::textColourId, colTextPri);
        presetBox.setColour(juce::ComboBox::outlineColourId, colSeparator);
        presetBox.setColour(juce::ComboBox::arrowColourId, colTextPri);
        presetBox.onChange = [this]() { loadSelectedPreset(); };
        addAndMakeVisible(presetBox);

        auto styleBtn = [](juce::TextButton& btn, bool isDestructive = false)
        {
            btn.setColour(juce::TextButton::buttonColourId, colHeaderBg);
            btn.setColour(juce::TextButton::buttonOnColourId, colSeparator);
            if (isDestructive)
            {
                btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffDC2626));
                btn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffB91C1C));
            }
            else
            {
                btn.setColour(juce::TextButton::textColourOffId, colTextPri);
                btn.setColour(juce::TextButton::textColourOnId, colTextPri);
            }
        };

        savePresetBtn.setTooltip("Save current matrix to selected preset");
        savePresetBtn.onClick = [this]() { saveCurrentPreset(); };
        styleBtn(savePresetBtn, false);
        addAndMakeVisible(savePresetBtn);

        newPresetBtn.setTooltip("Create a new preset from current matrix");
        newPresetBtn.onClick = [this]() { createNewPreset(); };
        styleBtn(newPresetBtn, false);
        addAndMakeVisible(newPresetBtn);

        renamePresetBtn.setTooltip("Rename selected preset");
        renamePresetBtn.onClick = [this]() { renameCurrentPreset(); };
        styleBtn(renamePresetBtn, false);
        addAndMakeVisible(renamePresetBtn);

        deletePresetBtn.setTooltip("Delete selected preset");
        deletePresetBtn.onClick = [this]() { deleteCurrentPreset(); };
        styleBtn(deletePresetBtn, true);
        addAndMakeVisible(deletePresetBtn);

        quickLabel.setText("Presets:", juce::dontSendNotification);
        quickLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        quickLabel.setColour(juce::Label::textColourId, colTextSec);
        addAndMakeVisible(quickLabel);

        defaultBtn.setTooltip("Reset routing to 1:1 default");
        defaultBtn.onClick = [this]() { applyDefaultRouting(); };
        styleBtn(defaultBtn, false);
        addAndMakeVisible(defaultBtn);

        allToMainBtn.setTooltip("Route all 16 channels to Main Output");
        allToMainBtn.onClick = [this]() { applyAllToMain(); };
        styleBtn(allToMainBtn, false);
        addAndMakeVisible(allToMainBtn);

        clearAllBtn.setTooltip("Disconnect all crosspoint routes");
        clearAllBtn.onClick = [this]() { applyClearAll(); };
        styleBtn(clearAllBtn, true);
        addAndMakeVisible(clearAllBtn);

        // Ensure default presets exist and populate dropdown
        ensureDefaultPresetsExist();
        refreshPresetList();

        // 2. High-performance DHD Matrix Canvas & Viewport
        canvas = std::make_unique<MatrixCanvas>(routingEngineRef, channelManagerRef, outputManagerRef,
            [this](int ch, int out)
            {
                updateInspector(ch, out);
            });

        viewport.setViewedComponent(canvas.get(), false);
        viewport.setScrollBarsShown(true, true);
        addAndMakeVisible(viewport);

        // 3. Bottom Broadcast Inspector Bar (Topology in Matrix, Parameters in Inspector)
        inspectorRouteLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        inspectorRouteLabel.setColour(juce::Label::textColourId, colTextPri);
        addAndMakeVisible(inspectorRouteLabel);

        inspectorToggleBtn.onClick = [this]()
        {
            int ch, out;
            canvas->getSelectedCell(ch, out);
            const bool cur = routingEngineRef.isRouteEnabled(ch, out);
            routingEngineRef.setRouteEnabled(ch, out, !cur);
            canvas->repaint();
            updateInspector(ch, out);
        };
        styleBtn(inspectorToggleBtn, false);
        addAndMakeVisible(inspectorToggleBtn);

        inspectorGainPrefixLabel.setText("GAIN", juce::dontSendNotification);
        inspectorGainPrefixLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        inspectorGainPrefixLabel.setColour(juce::Label::textColourId, colTextSec);
        addAndMakeVisible(inspectorGainPrefixLabel);

        inspectorGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        inspectorGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        inspectorGainSlider.setRange(-60.0, 12.0, 0.1);
        inspectorGainSlider.setDoubleClickReturnValue(true, 0.0);
        inspectorGainSlider.onValueChange = [this]()
        {
            int ch, out;
            canvas->getSelectedCell(ch, out);
            const float val = static_cast<float>(inspectorGainSlider.getValue());
            routingEngineRef.setRouteGainDb(ch, out, val);
            if (val <= -59.5f)
                inspectorGainReadout.setText("-inf dB", juce::dontSendNotification);
            else
                inspectorGainReadout.setText(juce::String(val, 1) + " dB", juce::dontSendNotification);
        };
        addAndMakeVisible(inspectorGainSlider);

        // Sharp box gain readout
        inspectorGainReadout.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        inspectorGainReadout.setColour(juce::Label::backgroundColourId, colAppBg);
        inspectorGainReadout.setColour(juce::Label::outlineColourId, colSeparator);
        inspectorGainReadout.setColour(juce::Label::textColourId, colTextPri);
        inspectorGainReadout.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(inspectorGainReadout);

        inspectorResetGainBtn.setTooltip("Reset Send Gain to 0.0 dB");
        inspectorResetGainBtn.onClick = [this]()
        {
            inspectorGainSlider.setValue(0.0, juce::sendNotification);
        };
        styleBtn(inspectorResetGainBtn, false);
        addAndMakeVisible(inspectorResetGainBtn);

        updateInspector(0, 0);
        setSize(760, 620);
    }

    RoutingMatrixDialog::~RoutingMatrixDialog()
    {
        setLookAndFeel(nullptr);
    }

    juce::File RoutingMatrixDialog::getPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("DSDMixer")
                       .getChildFile("RoutingPresets");
        if (!dir.exists())
            dir.createDirectory();
        return dir;
    }

    void RoutingMatrixDialog::ensureDefaultPresetsExist()
    {
        auto dir = getPresetsDirectory();

        // 1. Main Matrix
        auto mainFile = dir.getChildFile("Main Matrix.dsdroute");
        if (!mainFile.existsAsFile())
        {
            auto rootObj = std::make_unique<juce::DynamicObject>();
            rootObj->setProperty("product", "DSD Mixer Routing Preset");
            rootObj->setProperty("version", "1.0");
            rootObj->setProperty("presetName", "Main Matrix");
            juce::Array<juce::var> matrixArray;
            for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
            {
                for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
                {
                    auto cell = std::make_unique<juce::DynamicObject>();
                    cell->setProperty("channelIdx", ch);
                    cell->setProperty("outputIdx", out);
                    cell->setProperty("enabled", (out == 0));
                    cell->setProperty("sendGainDb", 0.0);
                    cell->setProperty("sendPan", 0.0);
                    matrixArray.add(juce::var(cell.release()));
                }
            }
            rootObj->setProperty("routingMatrix", matrixArray);
            mainFile.replaceWithText(juce::JSON::toString(juce::var(rootObj.release()), true));
        }

        // 2. 1-to-1 Direct Split
        auto splitFile = dir.getChildFile("1-to-1 Direct Split.dsdroute");
        if (!splitFile.existsAsFile())
        {
            auto rootObj = std::make_unique<juce::DynamicObject>();
            rootObj->setProperty("product", "DSD Mixer Routing Preset");
            rootObj->setProperty("version", "1.0");
            rootObj->setProperty("presetName", "1-to-1 Direct Split");
            juce::Array<juce::var> matrixArray;
            for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
            {
                for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
                {
                    auto cell = std::make_unique<juce::DynamicObject>();
                    cell->setProperty("channelIdx", ch);
                    cell->setProperty("outputIdx", out);
                    bool en = (out == (ch % NUM_OUTPUT_BUSES_LEVEL1));
                    cell->setProperty("enabled", en);
                    cell->setProperty("sendGainDb", 0.0);
                    cell->setProperty("sendPan", 0.0);
                    matrixArray.add(juce::var(cell.release()));
                }
            }
            rootObj->setProperty("routingMatrix", matrixArray);
            splitFile.replaceWithText(juce::JSON::toString(juce::var(rootObj.release()), true));
        }

        // 3. Broadcast & Stream
        auto broadcastFile = dir.getChildFile("Broadcast & Stream.dsdroute");
        if (!broadcastFile.existsAsFile())
        {
            auto rootObj = std::make_unique<juce::DynamicObject>();
            rootObj->setProperty("product", "DSD Mixer Routing Preset");
            rootObj->setProperty("version", "1.0");
            rootObj->setProperty("presetName", "Broadcast & Stream");
            juce::Array<juce::var> matrixArray;
            for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
            {
                for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
                {
                    auto cell = std::make_unique<juce::DynamicObject>();
                    cell->setProperty("channelIdx", ch);
                    cell->setProperty("outputIdx", out);
                    bool en = (out == 0 || out == 1);
                    float gain = (out == 1) ? -3.0f : 0.0f;
                    cell->setProperty("enabled", en);
                    cell->setProperty("sendGainDb", gain);
                    cell->setProperty("sendPan", 0.0);
                    matrixArray.add(juce::var(cell.release()));
                }
            }
            rootObj->setProperty("routingMatrix", matrixArray);
            broadcastFile.replaceWithText(juce::JSON::toString(juce::var(rootObj.release()), true));
        }

        // 4. Recording All
        auto recFile = dir.getChildFile("Recording All.dsdroute");
        if (!recFile.existsAsFile())
        {
            auto rootObj = std::make_unique<juce::DynamicObject>();
            rootObj->setProperty("product", "DSD Mixer Routing Preset");
            rootObj->setProperty("version", "1.0");
            rootObj->setProperty("presetName", "Recording All");
            juce::Array<juce::var> matrixArray;
            for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
            {
                for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
                {
                    auto cell = std::make_unique<juce::DynamicObject>();
                    cell->setProperty("channelIdx", ch);
                    cell->setProperty("outputIdx", out);
                    bool en = (out == 0 || out == 2);
                    cell->setProperty("enabled", en);
                    cell->setProperty("sendGainDb", 0.0);
                    cell->setProperty("sendPan", 0.0);
                    matrixArray.add(juce::var(cell.release()));
                }
            }
            rootObj->setProperty("routingMatrix", matrixArray);
            recFile.replaceWithText(juce::JSON::toString(juce::var(rootObj.release()), true));
        }
    }

    void RoutingMatrixDialog::refreshPresetList()
    {
        juce::String currentText = presetBox.getText();
        presetBox.clear(juce::dontSendNotification);

        auto dir = getPresetsDirectory();
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.dsdroute");

        int id = 1;
        int selectedId = 1;

        for (const auto& f : files)
        {
            juce::String name = f.getFileNameWithoutExtension();
            presetBox.addItem(name, id);
            if (name.equalsIgnoreCase(currentText))
                selectedId = id;
            id++;
        }

        if (presetBox.getNumItems() > 0)
        {
            presetBox.setSelectedId(selectedId, juce::dontSendNotification);
        }
    }

    void RoutingMatrixDialog::loadSelectedPreset()
    {
        juce::String name = presetBox.getText();
        if (name.isEmpty()) return;

        auto file = getPresetsDirectory().getChildFile(name + ".dsdroute");
        if (file.existsAsFile())
        {
            deserializePresetFromFile(file);
        }
    }

    void RoutingMatrixDialog::saveCurrentPreset()
    {
        juce::String name = presetBox.getText();
        if (name.isEmpty())
            name = "Main Matrix";

        auto file = getPresetsDirectory().getChildFile(name + ".dsdroute");
        serializePresetToFile(file, name);

        savePresetBtn.setButtonText("Saved!");
        juce::Timer::callAfterDelay(1200, [this]()
        {
            savePresetBtn.setButtonText("Save");
        });
    }

    void RoutingMatrixDialog::createNewPreset()
    {
        auto* aw = new juce::AlertWindow("New Routing Preset", "Enter a name for the new matrix preset:", juce::AlertWindow::QuestionIcon);
        aw->addTextEditor("presetName", "My Routing", "Preset Name:");
        aw->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int result)
        {
            if (result == 1)
            {
                juce::String name = aw->getTextEditorContents("presetName").trim();
                if (name.isNotEmpty())
                {
                    auto file = getPresetsDirectory().getChildFile(name + ".dsdroute");
                    serializePresetToFile(file, name);
                    refreshPresetList();
                    for (int i = 0; i < presetBox.getNumItems(); ++i)
                    {
                        if (presetBox.getItemText(i).equalsIgnoreCase(name))
                        {
                            presetBox.setSelectedItemIndex(i, juce::dontSendNotification);
                            break;
                        }
                    }
                }
            }
        }), true);
    }

    void RoutingMatrixDialog::renameCurrentPreset()
    {
        juce::String oldName = presetBox.getText();
        if (oldName.isEmpty()) return;

        auto* aw = new juce::AlertWindow("Rename Preset", "Enter a new name for preset '" + oldName + "':", juce::AlertWindow::QuestionIcon);
        aw->addTextEditor("presetName", oldName, "New Name:");
        aw->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, oldName](int result)
        {
            if (result == 1)
            {
                juce::String newName = aw->getTextEditorContents("presetName").trim();
                if (newName.isNotEmpty() && !newName.equalsIgnoreCase(oldName))
                {
                    auto oldFile = getPresetsDirectory().getChildFile(oldName + ".dsdroute");
                    auto newFile = getPresetsDirectory().getChildFile(newName + ".dsdroute");
                    if (oldFile.existsAsFile())
                    {
                        oldFile.moveFileTo(newFile);
                        refreshPresetList();
                        for (int i = 0; i < presetBox.getNumItems(); ++i)
                        {
                            if (presetBox.getItemText(i).equalsIgnoreCase(newName))
                            {
                                presetBox.setSelectedItemIndex(i, juce::dontSendNotification);
                                break;
                            }
                        }
                    }
                }
            }
        }), true);
    }

    void RoutingMatrixDialog::deleteCurrentPreset()
    {
        juce::String curName = presetBox.getText();
        if (curName.isEmpty()) return;

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Delete Preset",
            "Are you sure you want to delete preset '" + curName + "'?",
            "Delete",
            "Cancel",
            nullptr,
            juce::ModalCallbackFunction::create([this, curName](int result)
            {
                if (result == 1)
                {
                    auto file = getPresetsDirectory().getChildFile(curName + ".dsdroute");
                    if (file.existsAsFile())
                        file.deleteFile();

                    ensureDefaultPresetsExist();
                    refreshPresetList();
                    loadSelectedPreset();
                }
            })
        );
    }

    bool RoutingMatrixDialog::serializePresetToFile(const juce::File& file, const juce::String& name)
    {
        auto rootObj = std::make_unique<juce::DynamicObject>();
        rootObj->setProperty("product", "DSD Mixer Routing Preset");
        rootObj->setProperty("version", "1.0");
        rootObj->setProperty("presetName", name);

        juce::Array<juce::var> matrixArray;
        for (int ch = 0; ch < NUM_CHANNELS_LEVEL1; ++ch)
        {
            for (int out = 0; out < NUM_OUTPUT_BUSES_LEVEL1; ++out)
            {
                auto cellObj = std::make_unique<juce::DynamicObject>();
                cellObj->setProperty("channelIdx", ch);
                cellObj->setProperty("outputIdx", out);
                cellObj->setProperty("enabled", routingEngineRef.isRouteEnabled(ch, out));
                cellObj->setProperty("sendGainDb", routingEngineRef.getRouteGainDb(ch, out));
                cellObj->setProperty("sendPan", routingEngineRef.getRoutePan(ch, out));
                matrixArray.add(juce::var(cellObj.release()));
            }
        }
        rootObj->setProperty("routingMatrix", matrixArray);

        juce::var rootVar(rootObj.release());
        juce::String jsonString = juce::JSON::toString(rootVar, true);
        return file.replaceWithText(jsonString);
    }

    bool RoutingMatrixDialog::deserializePresetFromFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        juce::var rootVar = juce::JSON::parse(file);
        if (!rootVar.isObject())
            return false;

        auto* rootObj = rootVar.getDynamicObject();
        if (rootObj == nullptr)
            return false;

        if (rootObj->hasProperty("routingMatrix"))
        {
            const auto* matrixVar = rootObj->getProperty("routingMatrix").getArray();
            if (matrixVar != nullptr)
            {
                for (const auto& item : *matrixVar)
                {
                    if (!item.isObject()) continue;
                    int ch = static_cast<int>(item["channelIdx"]);
                    int out = static_cast<int>(item["outputIdx"]);
                    if (ch >= 0 && ch < NUM_CHANNELS_LEVEL1 && out >= 0 && out < NUM_OUTPUT_BUSES_LEVEL1)
                    {
                        routingEngineRef.setRouteEnabled(ch, out, static_cast<bool>(item["enabled"]));
                        routingEngineRef.setRouteGainDb(ch, out, static_cast<float>(item["sendGainDb"]));
                        routingEngineRef.setRoutePan(ch, out, static_cast<float>(item["sendPan"]));
                    }
                }
                if (canvas != nullptr)
                {
                    canvas->repaint();
                    int ch, out;
                    canvas->getSelectedCell(ch, out);
                    updateInspector(ch, out);
                }
                return true;
            }
        }
        return false;
    }

    void RoutingMatrixDialog::updateInspector(int ch, int out)
    {
        auto* chPtr = channelManagerRef.getChannel(ch);
        auto* outPtr = outputManagerRef.getOutput(out);

        juce::String chName = chPtr ? chPtr->getName() : ("CH " + juce::String(ch + 1));
        juce::String outName = outPtr ? outPtr->getName() : ("OUT " + juce::String(out + 1));

        inspectorRouteLabel.setText(juce::String::formatted("%02d  ", ch + 1) + chName + "  ➜  " + "OUT " + juce::String(out + 1) + "  " + outName.toUpperCase(),
                                   juce::dontSendNotification);

        const bool isConnected = routingEngineRef.isRouteEnabled(ch, out);
        if (isConnected)
        {
            inspectorToggleBtn.setButtonText("Disconnect");
            inspectorToggleBtn.setColour(juce::TextButton::textColourOffId, colAccent);
        }
        else
        {
            inspectorToggleBtn.setButtonText("Connect");
            inspectorToggleBtn.setColour(juce::TextButton::textColourOffId, colTextPri);
        }

        const float gainDb = routingEngineRef.getRouteGainDb(ch, out);
        inspectorGainSlider.setValue(gainDb, juce::dontSendNotification);
        if (gainDb <= -59.5f)
            inspectorGainReadout.setText("-inf dB", juce::dontSendNotification);
        else
            inspectorGainReadout.setText(juce::String(gainDb, 1) + " dB", juce::dontSendNotification);
    }

    void RoutingMatrixDialog::applyDefaultRouting()
    {
        const int numCh = NUM_CHANNELS_LEVEL1;
        const int numOut = NUM_OUTPUT_BUSES_LEVEL1;

        for (int ch = 0; ch < numCh; ++ch)
        {
            for (int out = 0; out < numOut; ++out)
            {
                routingEngineRef.setRouteEnabled(ch, out, (out == 0));
                routingEngineRef.setRouteGainDb(ch, out, 0.0f);
            }
        }
        canvas->repaint();
        int ch, out;
        canvas->getSelectedCell(ch, out);
        updateInspector(ch, out);
    }

    void RoutingMatrixDialog::applyAllToMain()
    {
        const int numCh = NUM_CHANNELS_LEVEL1;
        for (int ch = 0; ch < numCh; ++ch)
        {
            routingEngineRef.setRouteEnabled(ch, 0, true);
            routingEngineRef.setRouteGainDb(ch, 0, 0.0f);
        }
        canvas->repaint();
        int ch, out;
        canvas->getSelectedCell(ch, out);
        updateInspector(ch, out);
    }

    void RoutingMatrixDialog::applyClearAll()
    {
        const int numCh = NUM_CHANNELS_LEVEL1;
        const int numOut = NUM_OUTPUT_BUSES_LEVEL1;

        for (int ch = 0; ch < numCh; ++ch)
        {
            for (int out = 0; out < numOut; ++out)
            {
                routingEngineRef.setRouteEnabled(ch, out, false);
            }
        }
        canvas->repaint();
        int ch, out;
        canvas->getSelectedCell(ch, out);
        updateInspector(ch, out);
    }

    void RoutingMatrixDialog::resized()
    {
        auto bounds = getLocalBounds();

        // 1. Top Header Bar & Preset Controls (Height: 46px)
        auto headerArea = bounds.removeFromTop(46).reduced(12, 9);
        titleLabel.setBounds(headerArea.removeFromLeft(130));
        headerArea.removeFromLeft(8);

        presetBox.setBounds(headerArea.removeFromLeft(135));
        headerArea.removeFromLeft(4);
        savePresetBtn.setBounds(headerArea.removeFromLeft(46));
        headerArea.removeFromLeft(4);
        newPresetBtn.setBounds(headerArea.removeFromLeft(48));
        headerArea.removeFromLeft(4);
        renamePresetBtn.setBounds(headerArea.removeFromLeft(58));
        headerArea.removeFromLeft(4);
        deletePresetBtn.setBounds(headerArea.removeFromLeft(52));

        auto rightBtnRow = headerArea.removeFromRight(276);
        quickLabel.setBounds(rightBtnRow.removeFromLeft(46));
        rightBtnRow.removeFromLeft(2);
        defaultBtn.setBounds(rightBtnRow.removeFromLeft(74));
        rightBtnRow.removeFromLeft(4);
        allToMainBtn.setBounds(rightBtnRow.removeFromLeft(74));
        rightBtnRow.removeFromLeft(4);
        clearAllBtn.setBounds(rightBtnRow);

        // 2. Bottom Broadcast Inspector Bar (Height: 44px)
        auto inspectArea = bounds.removeFromBottom(44).reduced(12, 7);
        inspectorRouteLabel.setBounds(inspectArea.removeFromLeft(215));
        inspectArea.removeFromLeft(6);
        inspectorToggleBtn.setBounds(inspectArea.removeFromLeft(80));

        inspectArea.removeFromLeft(16);
        inspectorGainPrefixLabel.setBounds(inspectArea.removeFromLeft(36));
        inspectArea.removeFromLeft(4);
        inspectorGainSlider.setBounds(inspectArea.removeFromLeft(160));
        inspectArea.removeFromLeft(8);
        inspectorGainReadout.setBounds(inspectArea.removeFromLeft(56));
        inspectArea.removeFromLeft(6);
        inspectorResetGainBtn.setBounds(inspectArea.removeFromLeft(44));

        // 3. Central Matrix Viewport
        viewport.setBounds(bounds);

        const int availColW = (bounds.getWidth() - MatrixCanvas::LEFT_HEADER_W) / NUM_OUTPUT_BUSES_LEVEL1;
        const int colW = std::max(MatrixCanvas::MIN_COL_WIDTH, availColW);
        const int canvasW = MatrixCanvas::LEFT_HEADER_W + NUM_OUTPUT_BUSES_LEVEL1 * colW;
        const int canvasH = MatrixCanvas::TOP_HEADER_H + NUM_CHANNELS_LEVEL1 * MatrixCanvas::ROW_HEIGHT;
        canvas->setBounds(0, 0, canvasW, std::max(canvasH, bounds.getHeight()));
    }

    void RoutingMatrixDialog::paint(juce::Graphics& g)
    {
        // 1. Clean console surface
        g.fillAll(colHeaderBg);

        // Top studio amber accent line (2px)
        g.setColour(colAccent);
        g.fillRect(0, 0, getWidth(), 2);

        // Header bottom separator line
        g.setColour(colSeparator);
        g.drawHorizontalLine(46, 0.0f, static_cast<float>(getWidth()));

        // Inspector top separator line
        const float inspectY = static_cast<float>(getHeight() - 44);
        g.setColour(colSeparator);
        g.drawHorizontalLine(static_cast<int>(inspectY), 0.0f, static_cast<float>(getWidth()));

        // Inspector background
        g.setColour(colHeaderBg);
        g.fillRect(0.0f, inspectY, static_cast<float>(getWidth()), 44.0f);
    }
} // namespace dsd
