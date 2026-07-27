#pragma once

#include <raylib.h>

#include <string_view>

namespace ian {

enum class UiBarColor {
    Red,
    Green,
    Yellow,
};

class GameUi {
  public:
    GameUi() = default;
    ~GameUi();

    GameUi(const GameUi&) = delete;
    GameUi& operator=(const GameUi&) = delete;

    void initialize();
    void shutdown();

    void drawPanel(Rectangle bounds, unsigned char alpha = 245) const;
    void drawInsetPanel(Rectangle bounds,
                        unsigned char alpha = 235) const;
    void drawLabel(Rectangle bounds, std::string_view text,
                   int alignment = 0) const;
    void drawProgressBar(Rectangle bounds, float fraction,
                         UiBarColor color) const;
    bool drawButton(Rectangle bounds, std::string_view text) const;

  private:
    static Texture2D loadTexture(const char* path);
    static void unloadTexture(Texture2D& texture);

    Texture2D panel_{};
    Texture2D insetPanel_{};
    Texture2D button_{};
    Texture2D buttonPressed_{};
    Texture2D barBack_{};
    Texture2D barRed_{};
    Texture2D barGreen_{};
    Texture2D barYellow_{};
    bool initialized_{};
};

} // namespace ian
