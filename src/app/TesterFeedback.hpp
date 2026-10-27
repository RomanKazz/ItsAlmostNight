#pragma once

#include <raylib.h>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

// A draft owns its screenshot until it is saved or explicitly discarded.
class TesterFeedback {
public:
    static constexpr std::size_t MaximumCharacters = 1200;
    enum class Action { None, Save, Close, OpenFolder };
    struct Input {
        Vector2 pointer{};
        bool click{}, close{}, save{}, backspace{}, erase{}, selectAll{}, shift{}, home{}, end{};
        int cursorStep{};
        float wheel{};
        std::string text;
    };
    struct Layout {
        Rectangle panel{}, photo{}, text{}, save{}, cancel{}, folder{};
        float scale{1};
    };

    TesterFeedback() = default;
    ~TesterFeedback();
    TesterFeedback(const TesterFeedback&) = delete;
    TesterFeedback& operator=(const TesterFeedback&) = delete;
    void begin(std::string context);
    [[nodiscard]] bool capture(Image image); // Takes ownership even on failure.
    void clear();
    void advance(double seconds);
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool needsCapture() const { return active_ && !IsTextureValid(texture_); }
    [[nodiscard]] bool ready() const { return IsTextureValid(texture_); }
    [[nodiscard]] bool saved() const { return !savedDirectory_.empty(); }
    [[nodiscard]] const std::string& comment() const { return comment_; }
    [[nodiscard]] const std::filesystem::path& savedDirectory() const { return savedDirectory_; }
    [[nodiscard]] static Layout layout(Vector2 screen, float aspect);
    [[nodiscard]] Action input(const Input& input, Vector2 screen);
    void draw(Vector2 screen, bool russian, bool reduceFlashes) const;
    [[nodiscard]] bool save(const std::filesystem::path& root);
    [[nodiscard]] static std::string folderUrl(const std::filesystem::path& folder);

private:
    struct Line { std::size_t begin{}, end{}; };
    [[nodiscard]] std::vector<Line> textLines(float width, float size) const;
    void replaceSelection(std::string_view text);
    void eraseSelection();
    void keepCursorVisible(float width, float font, float lineHeight, float height);
    std::string context_, comment_, error_;
    std::filesystem::path savedDirectory_;
    Image image_{};
    Texture2D texture_{};
    bool active_{};
    double age_{};
    std::size_t cursor_{}, anchor_{};
    int firstLine_{};
};

} // namespace ian
