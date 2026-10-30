#pragma once
#include "ui/UiHintStyle.hpp"
#include "localization/Localization.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace ian {
inline std::vector<std::string> itemTooltipLines(std::string_view value, float width, float fontSize) {
    const ScopedUiFontRole font(UiFontRole::Body);
    std::istringstream words{std::string(value)};
    std::vector<std::string> lines;
    std::string word, line;
    while (words >> word) {
        const std::string next = line.empty() ? word : line+" "+word;
        if (!line.empty() && measureUiText(next,fontSize).x > width) {
            lines.push_back(line); line = word;
        } else line = next;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
inline Rectangle itemTooltipBounds(Vector2 pointer, Vector2 screen, float width, float height) {
    constexpr float margin = 12;
    width = std::min(width,std::max(1.0F,screen.x-2*margin));
    height = std::min(height,std::max(1.0F,screen.y-2*margin));
    float x = pointer.x+20, y = pointer.y+22;
    if (x+width > screen.x-margin) x = pointer.x-width-16;
    if (y+height > screen.y-margin) y = pointer.y-height-16;
    return {std::clamp(x,margin,std::max(margin,screen.x-width-margin)),
        std::clamp(y,margin,std::max(margin,screen.y-height-margin)),width,height};
}
inline void drawItemTooltip(std::string_view name,std::string_view description,
                            Vector2 pointer,Vector2 screen,float scale = 1,float opacity = 1) {
    if (name.empty() || description.empty()) return;
    const float s = std::clamp(scale,0.55F,1.0F);
    const float width = std::min(440*s,screen.x-24);
    const std::string title = localizeText(name), body = localizeText(description);
    const auto lines = itemTooltipLines(body,width-32*s,18*s);
    const Rectangle bounds = itemTooltipBounds(pointer,screen,width,(72+25*static_cast<float>(lines.size()))*s);
    hintStyle::panel(bounds,s,opacity);
    hintStyle::text(title,{bounds.x+16*s,bounds.y+12*s,bounds.width-32*s,32*s},23*s,
        menuStyle::Gold,UiFontRole::Display,opacity);
    for (std::size_t i=0;i<lines.size();++i) hintStyle::text(lines[i],
        {bounds.x+16*s,bounds.y+(54+25*static_cast<float>(i))*s,bounds.width-32*s,25*s},18*s,menuStyle::Cream,UiFontRole::Body,opacity);
}
} // namespace ian
