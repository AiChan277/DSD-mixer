#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Routing/RoutingEngine.h"
#include "Channel/ChannelManager.h"
#include "Output/OutputManager.h"
#include "UI/DSDLookAndFeel.h"
#include <memory>
#include <vector>

namespace dsd
{
    // ==============================================================================
    // MatrixLookAndFeel - Sharp Box Controls & Horizontal Console Fader
    // ==============================================================================
    class MatrixLookAndFeel : public DSDLookAndFeel
    {
    public:
        MatrixLookAndFeel();
        ~MatrixLookAndFeel() override = default;

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;

        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox& box) override;

        void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    };

    class RoutingMatrixDialog : public juce::Component
    {
    public:
        RoutingMatrixDialog(RoutingEngine& router, ChannelManager& chMgr, OutputManager& outMgr);
        ~RoutingMatrixDialog() override;

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        MatrixLookAndFeel matrixLookAndFeel;

        RoutingEngine& routingEngineRef;
        ChannelManager& channelManagerRef;
        OutputManager& outputManagerRef;

        // Top Header & Preset Controls
        juce::Label titleLabel;
        juce::ComboBox presetBox;
        juce::TextButton savePresetBtn{"Save"};
        juce::TextButton newPresetBtn{"+ New"};
        juce::TextButton renamePresetBtn{"Rename"};
        juce::TextButton deletePresetBtn{"Delete"};
        juce::Label quickLabel;
        juce::TextButton defaultBtn{"1:1 Default"};
        juce::TextButton allToMainBtn{"All to Main"};
        juce::TextButton clearAllBtn{"Clear All"};

        // High-Performance DHD Broadcast Matrix Canvas
        class MatrixCanvas : public juce::Component
        {
        public:
            MatrixCanvas(RoutingEngine& router, ChannelManager& chMgr, OutputManager& outMgr,
                         std::function<void(int ch, int out)> onCellSelected);
            ~MatrixCanvas() override = default;

            void paint(juce::Graphics& g) override;
            void mouseMove(const juce::MouseEvent& e) override;
            void mouseExit(const juce::MouseEvent& e) override;
            void mouseDown(const juce::MouseEvent& e) override;
            void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

            void getSelectedCell(int& ch, int& out) const noexcept { ch = selectedRow; out = selectedCol; }
            void setSelectedCell(int ch, int out);

            int getColWidth() const noexcept;

            static constexpr int ROW_HEIGHT = 28;
            static constexpr int MIN_COL_WIDTH = 88;
            static constexpr int LEFT_HEADER_W = 155;
            static constexpr int TOP_HEADER_H = 46;

        private:
            RoutingEngine& routerRef;
            ChannelManager& chMgrRef;
            OutputManager& outMgrRef;
            std::function<void(int ch, int out)> cellSelectedCallback;

            int hoveredRow{-1};
            int hoveredCol{-1};
            int selectedRow{0};
            int selectedCol{0};

            void drawCheckmark(juce::Graphics& g, const juce::Rectangle<float>& box, const juce::Colour& colour);
            void drawColumnHeader(juce::Graphics& g, const juce::String& busNum, const juce::String& busName,
                                  float x, float y, float width, float height,
                                  bool isHovered, bool isSelected);
        };

        juce::Viewport viewport;
        std::unique_ptr<MatrixCanvas> canvas;

        // Bottom Broadcast Inspector Bar (Topology in Grid, Parameters in Inspector)
        juce::Label inspectorRouteLabel;
        juce::TextButton inspectorToggleBtn{"Disconnect"};
        juce::Label inspectorGainPrefixLabel;
        juce::Slider inspectorGainSlider;
        juce::Label inspectorGainReadout;
        juce::TextButton inspectorResetGainBtn{"0 dB"};

        void updateInspector(int ch, int out);
        void applyDefaultRouting();
        void applyAllToMain();
        void applyClearAll();

        // Preset Management Functions
        static juce::File getPresetsDirectory();
        void ensureDefaultPresetsExist();
        void refreshPresetList();
        void loadSelectedPreset();
        void saveCurrentPreset();
        void createNewPreset();
        void renameCurrentPreset();
        void deleteCurrentPreset();
        bool serializePresetToFile(const juce::File& file, const juce::String& name);
        bool deserializePresetFromFile(const juce::File& file);
    };
} // namespace dsd
