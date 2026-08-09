#pragma once

#include "progression/SkillTree.hpp"

#include <raylib.h>

#include <optional>
#include <vector>

namespace ian {

class GameUi;

class SkillTreeScreen {
  public:
    explicit SkillTreeScreen(const SkillTree& tree);

    void open();
    void close();
    void setUnlimitedPoints(bool unlimited);
    void setInsightProgress(double current, double required);
    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] std::optional<std::size_t> update(float deltaSeconds);
    void draw(const GameUi& ui) const;

    [[nodiscard]] const SkillTree& tree() const;

  private:
    [[nodiscard]] Vector2 worldToScreen(
        SkillTreePoint point) const;
    [[nodiscard]] std::optional<std::size_t> nodeAt(
        Vector2 screenPosition) const;
    void drawConnections() const;
    void drawNodes() const;
    void drawDetails(const GameUi& ui) const;

    const SkillTree* tree_{};
    std::vector<float> reveal_;
    std::vector<float> revealDelay_;
    std::vector<float> connectionReveal_;
    std::vector<float> connectionDelay_;
    std::vector<float> hoverAmount_;
    std::vector<float> pulse_;
    std::vector<float> confirmationPulse_;
    std::vector<float> rejectShake_;
    bool open_{};
    bool unlimitedPoints_{};
    double currentInsight_{};
    double requiredInsight_{100.0};
    float opening_{};
    Vector2 camera_{0.0F, 42.0F};
    Vector2 targetCamera_{0.0F, 42.0F};
    float zoom_{1.15F};
    float targetZoom_{1.15F};
    bool dragging_{};
    Vector2 previousMouse_{};
    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> selected_;
    std::optional<std::size_t> confirmation_;
};

} // namespace ian
