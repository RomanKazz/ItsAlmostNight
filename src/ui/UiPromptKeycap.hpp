#pragma once

#include "ui/UiHintStyle.hpp"
#include "ui/UiRoundedRect.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace ian {

// Shared raised keycap for world interaction prompts and tutorial hints.
inline void drawPromptKeycap(Rectangle key, std::string_view label,
                             bool pressed, float opacity) {
    const ScopedUiFontRole font(UiFontRole::Display);
    const float height=key.height;
    const float stroke=height*.055F,depth=height*.13F;
    constexpr Color keyFrame{61,37,25,255},keyFace{250,239,216,255},keySide{119,75,41,255};
    constexpr Color keyInk{65,37,23,255};
    const Rectangle base{key.x,key.y+depth,key.width,key.height-depth};
    drawSolidRoundedRect(base,.50F,hintStyle::fade(keyFrame,opacity));
    Rectangle face{base.x+stroke,base.y+stroke,base.width-stroke*2,base.height-stroke*2};
    drawSolidRoundedRect(face,.48F,hintStyle::fade(keySide,opacity));
    if(pressed) key.y+=depth*.75F;
    key.height-=depth;
    drawSolidRoundedRect(key,.50F,hintStyle::fade(keyFrame,opacity));
    face={key.x+stroke,key.y+stroke,key.width-stroke*2,key.height-stroke*2};
    drawSolidRoundedRect(face,.48F,hintStyle::fade(keyFace,opacity));
    const float keyFont=fitUiTextSize(label,height*.40F,height*.20F,
        face.width-stroke*2,face.height);
    const auto letterSize=measureUiText(label,keyFont);
    // Center the visible glyphs, matching the interaction prompt's key.
    const Font keyTypeface=uiFont();
    const float glyphScale=letterSize.y/static_cast<float>(keyTypeface.baseSize);
    float inkLeft=letterSize.x,inkTop=letterSize.y,inkRight=0,inkBottom=0,advance=0;
    const std::string displayedLabel=localizeText(label);
    for(std::size_t offset=0;offset<displayedLabel.size();) {
        int byteCount=0;
        const int codepoint=GetCodepointNext(displayedLabel.c_str()+offset,&byteCount);
        offset+=static_cast<std::size_t>(std::max(byteCount,1));
        const auto glyph=GetGlyphInfo(keyTypeface,codepoint);
        const auto rect=GetGlyphAtlasRec(keyTypeface,codepoint);
        if(codepoint!=' ') {
            const float x=advance+static_cast<float>(glyph.offsetX)*glyphScale;
            const float y=static_cast<float>(glyph.offsetY)*glyphScale;
            inkLeft=std::min(inkLeft,x);inkTop=std::min(inkTop,y);
            inkRight=std::max(inkRight,x+rect.width*glyphScale);
            inkBottom=std::max(inkBottom,y+rect.height*glyphScale);
        }
        advance+=(glyph.advanceX>0?static_cast<float>(glyph.advanceX):rect.width)*glyphScale+letterSize.y*.02F;
    }
    SdfUiTextStyle letterStyle{};letterStyle.fillColor=hintStyle::fade(keyInk,opacity);
    letterStyle.outlinePixels=0;letterStyle.shadowColor=BLANK;
    drawStyledSdfUiText(label,{face.x+face.width*.5F-(inkLeft+inkRight)*.5F,
        face.y+face.height*.5F-(inkTop+inkBottom)*.5F},keyFont,letterStyle);
}

} // namespace ian
