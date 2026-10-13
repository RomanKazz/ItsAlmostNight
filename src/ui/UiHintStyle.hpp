#pragma once

#include "ui/UiMenuStyle.hpp"
#include "ui/UiViewport.hpp"
#include "localization/Localization.hpp"

namespace ian::hintStyle {
inline constexpr Color Warning{255,104,90,255};
inline Color fade(Color color,float opacity) {
    color.a=static_cast<unsigned char>(color.a*std::clamp(opacity,0.0F,1.0F));return color;
}
inline void panel(Rectangle bounds,float scale,float opacity=1) {
    menuStyle::panel({bounds.x+2*scale,bounds.y+4*scale,bounds.width,bounds.height},scale,
        {0,0,0,140},opacity);
    menuStyle::panel(bounds,scale,menuStyle::Inset,opacity);
}
inline void text(std::string_view value,Rectangle bounds,float size,Color color=menuStyle::Cream,
    UiFontRole role=UiFontRole::MenuBodyBold,float opacity=1,int align=0) {
    // Shared outlined faces and bevelled plates, no separate world-prompt font.
    if(role==UiFontRole::BodyBold) role=UiFontRole::MenuBodyBold;
    const ScopedUiFontRole font(role);
    const float fitted=fitUiTextSize(value,size,size*.6F,bounds.width,bounds.height);
    const auto measured=measureUiText(value,fitted);
    const float x=align==1 ? bounds.x+(bounds.width-measured.x)*.5F :
        align==2 ? bounds.x+bounds.width-measured.x : bounds.x;
    SdfUiTextStyle style{};style.fillColor=fade(color,opacity);
    style.outlineColor=fade({18,18,15,255},opacity);
    style.outlinePixels=fitted*(role==UiFontRole::Display ? .12F : role==UiFontRole::MenuBodyBold ? .09F : .04F);
    style.shadowColor=fade(BLACK,opacity);style.shadowOffsetPixels={0,fitted*.1F};style.softness=.6F;
    drawStyledSdfUiText(value,{x,bounds.y+(bounds.height-measured.y)*.5F},fitted,style);
}
inline void key(Rectangle bounds,std::string_view label,bool pressed=false,float opacity=1) {
    const float scale=bounds.height/44;
    if(pressed) bounds.y+=2*scale;
    menuStyle::panel(bounds,scale,pressed ? menuStyle::Selected : menuStyle::Card,opacity);
    text(label,{bounds.x+5*scale,bounds.y,bounds.width-10*scale,bounds.height},22*scale,
        menuStyle::Cream,UiFontRole::Display,opacity,1);
}
inline void notice(std::string_view value,float y,float fontSize,Color color=menuStyle::Cream,float offset=0) {
    const float scale=fontSize/20;
    const ScopedUiFontRole role(UiFontRole::MenuBodyBold);
    const float width=std::min(static_cast<float>(uiScreenWidth())-24,
        measureUiText(value,fontSize).x+32*scale);
    const Rectangle bounds{(uiScreenWidth()-width)*.5F+offset,y,width,42*scale};
    panel(bounds,scale);
    text(value,{bounds.x+12*scale,bounds.y,bounds.width-24*scale,bounds.height},fontSize,color);
}
} // namespace ian::hintStyle
