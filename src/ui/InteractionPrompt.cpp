#include "ui/UiViewport.hpp"
#include "ui/InteractionPrompt.hpp"

#include "ui/GameUi.hpp"
#include "ui/InputKeycap.hpp"
#include "ui/UiText.hpp"
#include "ui/UiHintStyle.hpp"
#include "ui/UiItemTooltip.hpp"
#include "ui/HudFormatting.hpp"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ian {
namespace {
constexpr float SwitchDelay=0.06F;
constexpr float MissingGrace=0.08F;
constexpr float PromptMaxDistance=16;
float approach(float current,float target,float seconds,float deltaSeconds) {
    return current+(target-current)*(1-std::exp(-deltaSeconds/std::max(seconds,.001F)));
}
float textWidth(std::string_view value,float size,UiFontRole role) {
    const ScopedUiFontRole font(role);return measureUiText(value,size).x;
}
struct PromptLayout {
    Rectangle bounds;
    float scale;
    float actionY;
    float costY;
    float stockY;
    float hintY;
    float progressY;
    std::string action;
    std::vector<std::string> hintLines;
};
PromptLayout layout(const InteractionPrompt& prompt,Vector2 anchor,Vector2 screen,
    const ControlSettings& controls) {
    const float s=std::clamp(screen.y/1080,.72F,1.05F);
    const std::string action=prompt.actionText.empty()
        ? prompt.cost ? "Upgrade" : "Collect" : prompt.actionText;
    const auto key=InputKeycap::label(controls,prompt.input);
    const float keyWidth=InputKeycap::size(key,38*s).x;
    const float maximum=std::min(420*s,screen.x-24);
    float width=std::max(260*s,32*s+keyWidth+12*s+textWidth(action,22*s,UiFontRole::Display));
    if(!prompt.objectName.empty()) width=std::max(width,32*s+textWidth(prompt.objectName,18*s,UiFontRole::MenuBodyBold));
    if(prompt.cost && prompt.showResourceCost) {
        const ScopedUiFontRole role(UiFontRole::MenuBodyBold);
        width=std::max(width,32*s+hud_detail::resourceCostRowWidth(*prompt.cost,19*s,25*s));
    }
    width=std::min(width,maximum);
    float y=12*s;
    if(!prompt.objectName.empty()) y+=24*s;
    PromptLayout out{};out.scale=s;out.actionY=y;out.action=action;y+=42*s;
    out.costY=y;
    if(prompt.cost) y+=34*s;
    out.stockY=y;
    if(prompt.producerStock) y+=34*s;
    out.hintY=y;
    if(prompt.hint && !prompt.hint->empty()) {
        out.hintLines=itemTooltipLines(localizeText(*prompt.hint),width-32*s,15*s);
        if(out.hintLines.size()>2) out.hintLines.resize(2);
        y+=20*s*static_cast<float>(out.hintLines.size())+4*s;
    }
    out.progressY=y;
    if(prompt.showProgress) y+=12*s;
    const float height=y+12*s;
    const float top=std::min(96*s,std::max(12.0F,screen.y-height-12));
    const float bottom=std::max(top,screen.y-140*s-height);
    out.bounds={std::clamp(anchor.x-width*.5F,12.0F,std::max(12.0F,screen.x-width-12)),
        std::clamp(anchor.y-height-18*s,top,bottom),width,height};
    const Rectangle crosshair{screen.x*.5F-52*s,screen.y*.5F-36*s,104*s,72*s};
    if(CheckCollisionRecs(out.bounds,crosshair))
        out.bounds.y=std::clamp(crosshair.y-height-12*s,top,bottom);
    // Small breakable props are aimed at the crosshair. Their former metre-
    // high world anchors pushed the hint to the sky at close range.
    if(prompt.targetKind==InteractionPromptTargetKind::Resource)
        out.bounds.y=std::clamp(screen.y*.5F+44*s,top,bottom);
    out.bounds.x=std::round(out.bounds.x);out.bounds.y=std::round(out.bounds.y);
    return out;
}
} // namespace

Rectangle interactionPromptPanelBounds(const InteractionPrompt& prompt,Vector2 anchor,Vector2 screen,
    const ControlSettings& controls) {return layout(prompt,anchor,screen,controls).bounds;}

InteractionPromptRenderer::TargetKey InteractionPromptRenderer::keyFor(
    const InteractionPrompt& prompt) {
    return {prompt.targetKind, prompt.targetId};
}

bool InteractionPromptRenderer::projectable(
    const InteractionPrompt& prompt, const Camera3D& camera) {
    if (prompt.occluded) {
        return false;
    }
    const Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 offset = Vector3Subtract(
        prompt.worldAnchor, camera.position);
    if (Vector3DotProduct(offset, forward) <= 0.05F ||
        Vector3Length(offset) > PromptMaxDistance) {
        return false;
    }
    const Vector2 screen = uiWorldToScreen(
        prompt.worldAnchor, camera);
    return std::isfinite(screen.x) && std::isfinite(screen.y) &&
           screen.x > -96.0F &&
           screen.x < static_cast<float>(uiScreenWidth()) + 96.0F &&
           screen.y > -96.0F &&
           screen.y < static_cast<float>(uiScreenHeight()) + 96.0F;
}

void InteractionPromptRenderer::activate(
    const InteractionPrompt& prompt) {
    activeTarget_ = keyFor(prompt);
    activePrompt_ = prompt;
    opacity_ = 0.0F;
    progress_ = 0.0F;
    progressOpacity_ = 0.0F;
    missingSeconds_ = 0.0;
}

void InteractionPromptRenderer::updateAnimation(
    float deltaSeconds, bool wantsVisible,
    const InteractionPrompt* prompt) {
    if (prompt && prompt->recentFailure) {
        if (!failureSignalActive_) {
            failureRemaining_ = 0.16F;
        }
        failureSignalActive_ = true;
    } else {
        failureSignalActive_ = false;
    }
    if (prompt && prompt->recentSuccess) {
        successRemaining_ = 0.18F;
    }
    failureRemaining_ = std::max(
        0.0F, failureRemaining_ - deltaSeconds);
    successRemaining_ = std::max(
        0.0F, successRemaining_ - deltaSeconds);

    opacity_ = approach(
        opacity_, wantsVisible ? 1.0F : 0.0F,
        wantsVisible ? 0.035F : 0.045F, deltaSeconds);

    const bool showProgress = prompt && prompt->showProgress &&
                              prompt->targetKind !=
                                  InteractionPromptTargetKind::Resource &&
                              wantsVisible;
    if (showProgress) {
        progress_ = approach(
            progress_, std::clamp(prompt->progress, 0.0F, 1.0F),
            0.08F, deltaSeconds);
        progressOpacity_ = approach(
            progressOpacity_, 1.0F, 0.08F, deltaSeconds);
    } else {
        progressOpacity_ = approach(
            progressOpacity_, 0.0F, 0.52F, deltaSeconds);
    }
}

void InteractionPromptRenderer::draw(
    const std::optional<InteractionPrompt>& prompt,
    const Camera3D& camera, const GameUi& ui,
    const ControlSettings& controls) {
    const float deltaSeconds = std::clamp(
        GetFrameTime(), 0.0F, 0.10F);
    const bool visibleCandidate =
        prompt && projectable(*prompt, camera);
    const TargetKey candidateKey = visibleCandidate
        ? keyFor(*prompt)
        : TargetKey{InteractionPromptTargetKind::Resource, {}};

    if (visibleCandidate) {
        if (!activeTarget_ || *activeTarget_ == candidateKey) {
            if (!activeTarget_) {
                activate(*prompt);
            } else if (activePrompt_) {
                *activePrompt_ = *prompt;
            }
            candidateTarget_.reset();
            candidateSeconds_ = 0.0;
            missingSeconds_ = 0.0;
        } else {
            missingSeconds_ += deltaSeconds;
            if (!candidateTarget_ ||
                *candidateTarget_ != candidateKey) {
                candidateTarget_ = candidateKey;
                candidateSeconds_ = 0.0;
            } else {
                candidateSeconds_ += deltaSeconds;
            }
            if (candidateSeconds_ >= SwitchDelay) {
                activate(*prompt);
                candidateTarget_.reset();
                candidateSeconds_ = 0.0;
            }
        }
    } else {
        candidateTarget_.reset();
        candidateSeconds_ = 0.0;
        if (activeTarget_) {
            missingSeconds_ += deltaSeconds;
        }
        if (missingSeconds_ >= MissingGrace) {
            activeTarget_.reset();
        }
    }

    const bool wantsVisible = activeTarget_.has_value() &&
                              activePrompt_.has_value() &&
                              (visibleCandidate ||
                               missingSeconds_ < MissingGrace);
    updateAnimation(
        deltaSeconds, wantsVisible,
        wantsVisible && visibleCandidate ? &*prompt : nullptr);
    if (opacity_ <= 0.005F || !activePrompt_) {
        return;
    }
    drawPrompt(*activePrompt_, camera, ui, controls,opacity_);
}

void InteractionPromptRenderer::drawPrompt(
    const InteractionPrompt& prompt,const Camera3D& camera,const GameUi& ui,
    const ControlSettings& controls,float opacity) {
    const auto anchor=uiWorldToScreen(prompt.worldAnchor,camera);
    auto l=layout(prompt,anchor,{static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},controls);
    const float s=l.scale;
    if(failureRemaining_>0) l.bounds.x+=std::sin(failureRemaining_*90)*3*s;
    hintStyle::panel(l.bounds,s,opacity);
    const float x=l.bounds.x+16*s;
    const float width=l.bounds.width-32*s;
    if(!prompt.objectName.empty()) hintStyle::text(prompt.objectName,
        {x,l.bounds.y+10*s,width,24*s},18*s,menuStyle::Gold,UiFontRole::MenuBodyBold,opacity);
    const auto label=InputKeycap::label(controls,prompt.input);
    const float keyWidth=InputKeycap::size(label,38*s).x;
    const float actionY=l.bounds.y+l.actionY;
    hintStyle::key({x,actionY,keyWidth,38*s},label,
        InputKeycap::held(controls,prompt.input) || successRemaining_>0,opacity);
    hintStyle::text(l.action,{x+keyWidth+12*s,actionY,width-keyWidth-12*s,38*s},22*s,
        prompt.state==InteractionState::Unavailable ? menuStyle::Muted : menuStyle::Cream,
        UiFontRole::Display,opacity);
    if(prompt.cost) {
        float cursor=x;
        const float y=l.bounds.y+l.costY+3*s;
        const auto entry=[&](UiResourceIcon icon,std::string_view amount,bool sufficient) {
            ui.drawResourceIcon({cursor,y,25*s,25*s},icon,hintStyle::fade(WHITE,opacity));
            cursor+=30*s;
            const float w=textWidth(amount,19*s,UiFontRole::MenuBodyBold);
            hintStyle::text(amount,{cursor,y,w+2*s,25*s},19*s,
                sufficient ? menuStyle::Cream : hintStyle::Warning,UiFontRole::MenuBodyBold,opacity);
            cursor+=w+16*s;
        };
        if(prompt.availableCurrency || (!prompt.showResourceCost &&
            (prompt.targetKind==InteractionPromptTargetKind::Chest || prompt.targetKind==InteractionPromptTargetKind::Landmark))) {
            const int price=prompt.cost->crystals;
            const bool enough=!prompt.availableCurrency || *prompt.availableCurrency>=price;
            const std::string amount=prompt.availableCurrency ? std::to_string(*prompt.availableCurrency)+" / "+std::to_string(price) : std::to_string(price);
            entry(UiResourceIcon::Coin,amount,enough);
        } else {
            const std::array prices{prompt.cost->wood,prompt.cost->stone,prompt.cost->crystals};
            const std::array stocks{prompt.availableResources.wood,prompt.availableResources.stone,prompt.availableResources.crystals};
            const std::array icons{UiResourceIcon::Wood,UiResourceIcon::Stone,UiResourceIcon::Crystal};
            bool any=false;
            for(std::size_t i=0;i<prices.size();++i) if(prices[i]>0) {
                entry(icons[i],std::to_string(prices[i]),stocks[i]>=prices[i]);any=true;
            }
            if(!any) hintStyle::text("FREE",{x,y,width,25*s},19*s,menuStyle::Green,UiFontRole::MenuBodyBold,opacity);
        }
    }
    if(prompt.producerStock) {
        const auto& stock=*prompt.producerStock;
        const float y=l.bounds.y+l.stockY;
        ui.drawResourceIcon({x,y,25*s,25*s},stock.icon,hintStyle::fade(WHITE,opacity));
        const float kw=InputKeycap::size(InputKeycap::label(controls,ControlAction::Interact),28*s).x;
        hintStyle::text(std::to_string(stock.stored)+" / "+std::to_string(stock.capacity),
            {x+31*s,y,width-kw-41*s,28*s},18*s,menuStyle::Cream,UiFontRole::MenuBodyBold,opacity);
        if(prompt.cost) hintStyle::key({x+width-kw,y,kw,28*s},InputKeycap::label(controls,ControlAction::Interact),false,opacity);
    }
    for(std::size_t i=0;i<l.hintLines.size();++i) hintStyle::text(l.hintLines[i],
        {x,l.bounds.y+l.hintY+20*s*static_cast<float>(i),width,20*s},15*s,
        menuStyle::Muted,UiFontRole::Body,opacity);
    if(prompt.showProgress && progressOpacity_>.01F) {
        menuStyle::panel({x,l.bounds.y+l.progressY,width,8*s},s*.3F,menuStyle::Card,opacity*progressOpacity_);
        drawChamferedFill({x+2*s,l.bounds.y+l.progressY+2*s,
            (width-4*s)*std::clamp(progress_,0.0F,1.0F),4*s},s,
            hintStyle::fade(menuStyle::Gold,opacity*progressOpacity_));
    }
}

void InteractionPromptRenderer::reset() {
    activeTarget_.reset();
    candidateTarget_.reset();
    activePrompt_.reset();
    candidateSeconds_ = 0.0;
    missingSeconds_ = 0.0;
    opacity_ = 0.0F;
    progressOpacity_ = 0.0F;
    failureRemaining_ = 0.0F;
    successRemaining_ = 0.0F;
    failureSignalActive_ = false;
}

} // namespace ian
