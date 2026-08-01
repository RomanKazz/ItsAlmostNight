#pragma once

#include "progression/SkillTree.hpp"

#include <raylib.h>

#include <optional>
#include <vector>

namespace ian {

class GameUi;

class SkillTreeScreen {
  public:
    SkillTreeScreen();

    void open();
    void close();
    [[nodiscard]] bool isOpen() const;

    // Returns true once when a node is unlocked.
    [[nodiscard]] bool update(float deltaSeconds);
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

    SkillTree tree_;
    std::vector<float> reveal_;
    std::vector<float> revealDelay_;
    std::vector<float> pulse_;
    bool open_{};
    float opening_{};
    Vector2 camera_{};
    Vector2 targetCamera_{};
    float zoom_{0.88F};
    float targetZoom_{0.88F};
    bool dragging_{};
    Vector2 previousMouse_{};
    std::optional<std::size_t> hovered_;
    std::optional<std::size_t> selected_;
};

} // namespace ian
