#pragma once

namespace ian {

// A screen transition consumes both the press and its later release.
class MenuInputBarrier {
public:
    void blockUntilRelease() { blocked_ = true; }

    [[nodiscard]] bool allowsInput(bool activationHeld) {
        if (!blocked_) return true;
        if (!activationHeld) blocked_ = false;
        // The release frame still belongs to the screen we just left.
        return false;
    }

private:
    bool blocked_{};
};

} // namespace ian
