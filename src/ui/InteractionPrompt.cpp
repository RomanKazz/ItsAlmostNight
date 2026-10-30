#include "ui/UiViewport.hpp"
#include "ui/InteractionPrompt.hpp"

#include "ui/GameUi.hpp"
#include "ui/InputKeycap.hpp"
#include "ui/UiText.hpp"
#include "ui/UiHintStyle.hpp"
#include "ui/UiPromptKeycap.hpp"
#include "ui/UiItemTooltip.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/TargetHealthBarAnchor.hpp"

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
    out.bounds.x=std::round(out.bounds.x);out.bounds.y=std::round(out.bounds.y);
    return out;
}
struct WorldPromptProjection { Vector2 anchor;float size; };
std::optional<WorldPromptProjection> worldPromptProjection(Vector3 anchor,const Camera3D& camera) {
    const Vector2 projected=uiWorldToScreen(anchor,camera);
    const Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up=Vector3Normalize(Vector3CrossProduct(right,forward));
    const Vector2 projectedUp=uiWorldToScreen(Vector3Add(anchor,up),camera);
    const float pixelsPerUnit=Vector2Distance(projected,projectedUp);
    if(!std::isfinite(pixelsPerUnit) || pixelsPerUnit<=0) return std::nullopt;
    return WorldPromptProjection{projected,pixelsPerUnit*BuildingUpgradePromptWorldHeight};
}

// The compact world-space upgrade row predates the general interaction cards.
// Keep its existing arrow, chunky numbers and per-resource affordability colours.
void drawCompactUpgrade(const InteractionPrompt& prompt,const Camera3D& camera,
    const GameUi& ui,const ControlSettings& controls,float opacity) {
    const ScopedUiFontRole font(UiFontRole::Display);
    const auto projection=worldPromptProjection(prompt.worldAnchor,camera);
    if(!projection) return;
    Vector2 projected=projection->anchor;
    const float size=projection->size;
    projected.y-=size*.4F;
    const float iconSize=size*.76F,fontSize=iconSize/1.68F,gap=size*.10F;
    struct Price { UiResourceIcon icon;int amount;int available; };
    const std::array prices{
        Price{UiResourceIcon::Wood,prompt.cost->wood,prompt.availableResources.wood},
        Price{UiResourceIcon::Stone,prompt.cost->stone,prompt.availableResources.stone},
        Price{UiResourceIcon::Crystal,prompt.cost->crystals,prompt.availableResources.crystals}};
    float width=size;
    for(const auto& price:prices) if(price.amount>0)
        width+=gap+iconSize+gap*.5F+measureUiText(std::to_string(price.amount),fontSize).x;
    const float paddingX=size*.28F,paddingY=size*.15F;
    const float height=size+paddingY*2;
    // Centre the complete key + gap + price pill on the building's anchor.
    // Single letters stay square; longer rebound key names can widen the cap.
    const auto upgradeKey=InputKeycap::label(controls,ControlAction::Upgrade);
    const float keyHeight=size*1.36F;
    const Vector2 keySize{std::max(keyHeight,
        measureUiText(upgradeKey,keyHeight*.40F).x+keyHeight*.22F),keyHeight};
    const float keyGap=size*.12F;
    Rectangle pill{projected.x-width*.5F-paddingX+(keySize.x+keyGap)*.5F,
        projected.y-size*.5F-paddingY,width+paddingX*2,height};
    // This is a world-anchored billboard. Let it leave the viewport naturally;
    // clamping its rectangle would pin a departing building's price to the HUD.
    DrawRectangleRounded(pill,1,16,hintStyle::fade({0,0,0,102},opacity));
    const float left=pill.x+paddingX,centerY=pill.y+paddingY+size*.5F;
    drawPromptKeycap({pill.x-keyGap-keySize.x,centerY-keySize.y*.5F,keySize.x,keySize.y},
        upgradeKey,InputKeycap::held(controls,ControlAction::Upgrade),opacity);
    const Texture2D arrow=ui.coreMenuTexture(CoreMenuTexture::UpgradeGreen);
    if(IsTextureValid(arrow))DrawTexturePro(arrow,
        {0,0,static_cast<float>(arrow.width),static_cast<float>(arrow.height)},
        {left,centerY-size*.5F,size,size},{},0,hintStyle::fade(WHITE,opacity));
    SdfUiTextStyle style{};
    style.outlineColor=hintStyle::fade({20,24,20,255},opacity);
    style.outlinePixels=size*.065F;
    style.shadowColor=hintStyle::fade({0,0,0,140},opacity);
    style.shadowOffsetPixels={0,size*.055F};
    float cursor=left+size;
    for(const auto& price:prices) {
        if(price.amount<=0) continue;
        cursor+=gap;
        ui.drawResourceIcon({cursor,centerY-iconSize*.5F,iconSize,iconSize},price.icon,
            hintStyle::fade(WHITE,opacity));
        cursor+=iconSize+gap*.5F;
        const auto amount=std::to_string(price.amount);
        const Vector2 measured=measureUiText(amount,fontSize);
        style.fillColor=hintStyle::fade(price.available>=price.amount?UiWhite:hintStyle::Warning,opacity);
        drawStyledSdfUiText(amount,{cursor,centerY-measured.y*.5F},fontSize,style);
        cursor+=measured.x;
    }
}
void drawWorldAction(const InteractionPrompt& prompt,const Camera3D& camera,
    const GameUi& ui,const ControlSettings& controls,float opacity) {
    const ScopedUiFontRole font(UiFontRole::Display);
    const auto projection=worldPromptProjection(prompt.worldAnchor,camera);
    if(!projection) return;
    const auto [anchor,size]=*projection;
    const float fontSize=size*.48F,padding=size*.28F,keyHeight=size*1.36F,keyGap=size*.12F;
    const bool wallDoor = prompt.targetKind == InteractionPromptTargetKind::Building &&
        prompt.actionText == "DOOR";
    const bool gateAction = prompt.targetKind == InteractionPromptTargetKind::Building &&
        (prompt.actionText == "OPEN" || prompt.actionText == "CLOSE");
    const bool shrine = prompt.targetKind == InteractionPromptTargetKind::Landmark &&
        prompt.showResourceCost && prompt.cost.has_value();
    const bool producer=prompt.targetKind==InteractionPromptTargetKind::Landmark && !shrine;
    const auto label=wallDoor ? std::string{"F"} :
        InputKeycap::label(controls,ControlAction::Interact);
    const float keyWidth=std::max(keyHeight,measureUiText(label,keyHeight*.40F).x+keyHeight*.22F);
    const std::string_view action=shrine ? std::string_view{} : wallDoor || gateAction || producer ? std::string_view{prompt.actionText} :
        prompt.targetKind==InteractionPromptTargetKind::Challenge?"CHALLENGE":"Open";
    const auto textSize=measureUiText(action,fontSize);
    const bool priced=(prompt.targetKind==InteractionPromptTargetKind::Chest || producer) && prompt.cost.has_value();
    const std::string price=priced?std::to_string(prompt.cost->crystals):std::string{};
    const float coinSize=size*.76F,priceGap=size*.16F;
    const auto priceSize=measureUiText(price,fontSize);
    struct ShrinePrice { UiResourceIcon icon; int amount; int available; };
    const std::array shrinePrices{
        ShrinePrice{UiResourceIcon::Wood,shrine?prompt.cost->wood:0,prompt.availableResources.wood},
        ShrinePrice{UiResourceIcon::Stone,shrine?prompt.cost->stone:0,prompt.availableResources.stone},
        ShrinePrice{UiResourceIcon::Crystal,shrine?prompt.cost->crystals:0,prompt.availableResources.crystals}};
    float priceWidth=priced?priceGap+coinSize+priceGap*.5F+priceSize.x:0;
    if(shrine) {
        bool first=true;
        for(const auto& resource:shrinePrices) if(resource.amount>0) {
            if(!first) priceWidth+=priceGap;
            priceWidth+=coinSize+priceGap*.5F+
                measureUiText(std::to_string(resource.amount),fontSize).x;
            first=false;
        }
    }
    const bool showKey=!(producer && prompt.state==InteractionState::Unavailable);
    const float keySpace=showKey ? keyWidth+keyGap : 0;
    const float pillWidth=textSize.x+priceWidth+padding*2,pillHeight=size*1.3F;
    const float left=anchor.x-(keySpace+pillWidth)*.5F;
    const Rectangle pill{left+keySpace,anchor.y-pillHeight*.5F,pillWidth,pillHeight};
    DrawRectangleRounded(pill,1,16,hintStyle::fade({0,0,0,102},opacity));
    if(showKey) drawPromptKeycap({left,anchor.y-keyHeight*.5F,keyWidth,keyHeight},label,
        wallDoor ? IsKeyDown(KEY_F) : InputKeycap::held(controls,ControlAction::Interact),opacity);
    SdfUiTextStyle style{};style.fillColor=hintStyle::fade(UiWhite,opacity);
    style.outlineColor=hintStyle::fade({20,24,20,255},opacity);style.outlinePixels=size*.065F;
    style.shadowColor=hintStyle::fade({0,0,0,140},opacity);style.shadowOffsetPixels={0,size*.055F};
    if(!shrine) drawStyledSdfUiText(action,{pill.x+padding,anchor.y-textSize.y*.5F},fontSize,style);
    if((shrine || producer) && !prompt.objectName.empty()) {
        const float titleFont=fontSize*1.12F;
        const std::string title=localizeText(prompt.objectName);
        const auto titleSize=measureUiText(title,titleFont);
        SdfUiTextStyle titleStyle=style;
        titleStyle.fillColor=hintStyle::fade(prompt.accentColor,opacity);
        drawStyledSdfUiText(title,{anchor.x-titleSize.x*.5F,
            pill.y-titleSize.y-size*.16F},titleFont,titleStyle);
    }
    if (producer && prompt.productionPerMinute && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        std::string income=TextFormat("%.1f / MIN",*prompt.productionPerMinute);
        if(prompt.nextProductionPerMinute) income+="  >  "+std::string(TextFormat("%.1f / MIN",*prompt.nextProductionPerMinute));
        const float detailSize=fontSize*.85F;
        const auto measured=measureUiText(income,detailSize);
        const Rectangle detail{anchor.x-measured.x*.5F-padding,pill.y+pill.height+size*.20F,
            measured.x+padding*2,size*.95F};
        DrawRectangleRounded(detail,1,16,hintStyle::fade({0,0,0,132},opacity));
        drawStyledSdfUiText(income,{anchor.x-measured.x*.5F,detail.y+(detail.height-measured.y)*.5F},detailSize,style);
    }
    if(priced) {
        float cursor=pill.x+padding+textSize.x+priceGap;
        ui.drawResourceIcon({cursor,anchor.y-coinSize*.5F-size*.05F,coinSize,coinSize},UiResourceIcon::Coin,
            hintStyle::fade(WHITE,opacity));
        cursor+=coinSize+priceGap*.5F;
        const bool enough=prompt.availableCurrency
            ? *prompt.availableCurrency>=prompt.cost->crystals : prompt.state!=InteractionState::Warning;
        style.fillColor=hintStyle::fade(enough?UiWhite:hintStyle::Warning,opacity);
        drawStyledSdfUiText(price,{cursor,anchor.y-priceSize.y*.5F},fontSize,style);
    } else if(shrine) {
        float cursor=pill.x+padding;
        bool first=true;
        for(const auto& resource:shrinePrices) {
            if(resource.amount<=0) continue;
            if(!first) cursor+=priceGap;
            ui.drawResourceIcon({cursor,anchor.y-coinSize*.5F,coinSize,coinSize},resource.icon,
                hintStyle::fade(WHITE,opacity));
            cursor+=coinSize+priceGap*.5F;
            const auto amount=std::to_string(resource.amount);
            const auto measured=measureUiText(amount,fontSize);
            style.fillColor=hintStyle::fade(resource.available>=resource.amount?UiWhite:hintStyle::Warning,opacity);
            drawStyledSdfUiText(amount,{cursor,anchor.y-measured.y*.5F},fontSize,style);
            cursor+=measured.x;
            first=false;
        }
    }
}

void drawBuildingActions(const InteractionPrompt& prompt, const Camera3D& camera,
    const ControlSettings& controls, float opacity) {
    const auto projection = worldPromptProjection(prompt.worldAnchor, camera);
    if (!projection) return;
    const ScopedUiFontRole font(UiFontRole::Display);
    const float size = projection->size * .75F;
    const float fontSize = size * .48F;
    const float keyHeight = size * 1.36F;
    const float keyGap = size * .12F, actionGap = size * .55F, padding = size * .28F;
    struct Action { std::string text, key; bool held; float keyWidth, textWidth; };
    std::array<Action, 4> actions{};
    std::size_t count = 0;
    float width = 0;
    const auto add = [&](std::string text, std::string key, bool held) {
        const float keyWidth = std::max(keyHeight,
            measureUiText(key, keyHeight * .40F).x + keyHeight * .22F);
        const float textWidth = measureUiText(text, fontSize).x;
        if (count > 0) width += actionGap;
        width += keyWidth + keyGap + textWidth + padding * 2;
        actions[count++] = {std::move(text), std::move(key), held, keyWidth, textWidth};
    };
    if (!prompt.actionText.empty()) {
        const bool door = prompt.actionText == "DOOR";
        add(prompt.actionText, door ? "F" : InputKeycap::label(controls, prompt.input),
            door ? IsKeyDown(KEY_F) : InputKeycap::held(controls, prompt.input));
    }
    if (prompt.showRotate) add("ROTATE", "R", IsKeyDown(KEY_R));
    if (prompt.showRepair) add("REPAIR", InputKeycap::label(controls, ControlAction::Repair),
        InputKeycap::held(controls, ControlAction::Repair));
    if (prompt.showRemove) add("REMOVE", InputKeycap::label(controls, ControlAction::Sell),
        InputKeycap::held(controls, ControlAction::Sell));
    // Keep the row attached to the world anchor while it leaves the viewport.
    float cursor = projection->anchor.x - width * .5F;
    const float y = projection->anchor.y;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& action = actions[i];
        drawPromptKeycap({cursor, y - keyHeight * .5F, action.keyWidth, keyHeight},
            action.key, action.held, opacity);
        cursor += action.keyWidth + keyGap;
        const Rectangle pill{cursor, y - size * .65F, action.textWidth + padding * 2, size * 1.3F};
        DrawRectangleRounded(pill, 1, 16, hintStyle::fade({0, 0, 0, 102}, opacity));
        SdfUiTextStyle style{};
        style.fillColor = hintStyle::fade(UiWhite, opacity);
        style.outlineColor = hintStyle::fade({20, 24, 20, 255}, opacity);
        style.outlinePixels = size * .065F;
        style.shadowColor = hintStyle::fade({0, 0, 0, 140}, opacity);
        style.shadowOffsetPixels = {0, size * .055F};
        const auto measured = measureUiText(action.text, fontSize);
        drawStyledSdfUiText(action.text, {pill.x + padding, y - measured.y * .5F}, fontSize, style);
        cursor += pill.width + actionGap;
    }
}

void drawWorldLoot(const InteractionPrompt& prompt,const Camera3D& camera,
    const ControlSettings& controls,float opacity) {
    const auto projection=worldPromptProjection(prompt.worldAnchor,camera);
    if(!projection) return;
    const Vector2 anchor=projection->anchor;
    // Loot prompts follow their world anchor, but keep a stable on-screen size
    // while the player approaches or moves away from the item.
    const float size=48.0F*std::clamp(static_cast<float>(uiScreenHeight())/1200.0F,.7F,1.25F);
    const float padding=size*.28F,keyHeight=size*1.36F,keyGap=size*.12F;
    const float actionFont=size*.48F;
    const float titleFont=actionFont*.72F;
    const std::string action="PICK UP";
    const std::string key=InputKeycap::label(controls,ControlAction::Interact);
    const ScopedUiFontRole display(UiFontRole::Display);
    const float keyWidth=std::max(keyHeight,measureUiText(key,keyHeight*.40F).x+keyHeight*.22F);
    const Vector2 actionSize=measureUiText(action,actionFont);
    const float pillWidth=actionSize.x+padding*2;
    const float groupWidth=keyWidth+keyGap+pillWidth;
    const float rowY=anchor.y-size*.45F;
    const float left=anchor.x-groupWidth*.5F;
    const Rectangle pill{left+keyWidth+keyGap,rowY-size*.65F,pillWidth,size*1.3F};
    DrawRectangleRounded(pill,1,16,hintStyle::fade({0,0,0,132},opacity));
    drawPromptKeycap({left,rowY-keyHeight*.5F,keyWidth,keyHeight},key,
        InputKeycap::held(controls,ControlAction::Interact),opacity);
    const auto drawLabel=[&](std::string_view value,Vector2 position,float fontSize,Color color,
        UiFontRole role) {
        const ScopedUiFontRole font(role);
        SdfUiTextStyle style{};
        style.fillColor=hintStyle::fade(color,opacity);
        style.outlineColor=hintStyle::fade({20,24,20,255},opacity);
        style.outlinePixels=fontSize*.12F;
        style.shadowColor=hintStyle::fade({0,0,0,140},opacity);
        style.shadowOffsetPixels={0,fontSize*.11F};
        drawStyledSdfUiText(value,position,fontSize,style);
    };
    drawLabel(action,{pill.x+padding,rowY-actionSize.y*.5F},actionFont,UiWhite,UiFontRole::Display);
    const Vector2 titleSize=measureUiText(prompt.objectName,titleFont);
    drawLabel(prompt.objectName,{anchor.x-titleSize.x*.5F,pill.y-titleSize.y-size*.12F},
        titleFont,prompt.accentColor,UiFontRole::Display);

    if(prompt.hint && !prompt.hint->empty()) {
        const float scale=std::clamp(size/40.0F,.72F,1.15F);
        const float fontSize=16*scale;
        const float panelWidth=360*scale;
        const float panelPadding=12*scale;
        auto lines=itemTooltipLines(localizeText(*prompt.hint),panelWidth-2*panelPadding,fontSize);
        if(lines.size()>2) lines.resize(2);
        const float lineHeight=21*scale;
        const float panelHeight=pill.height;
        const float screenWidth=static_cast<float>(uiScreenWidth());
        float panelX=pill.x+pill.width+size*.20F;
        if(panelX+panelWidth>screenWidth-12*scale)
            panelX=left-size*.20F-panelWidth;
        panelX=std::clamp(panelX,12*scale,std::max(12*scale,screenWidth-panelWidth-12*scale));
        const float panelY=std::clamp(pill.y,12*scale,
            std::max(12*scale,static_cast<float>(uiScreenHeight())-panelHeight-12*scale));
        DrawRectangleRounded({panelX,panelY,panelWidth,panelHeight},1,16,
            hintStyle::fade({0,0,0,132},opacity));
        const float textTop=panelY+(panelHeight-lineHeight*static_cast<float>(lines.size()))*.5F;
        for(std::size_t i=0;i<lines.size();++i)
            drawLabel(lines[i],{panelX+panelPadding,textTop+lineHeight*static_cast<float>(i)},
                fontSize,UiWhite,UiFontRole::Body);
    }
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
    if (prompt.showRepair || prompt.showRemove || prompt.showRotate) {
        if (projectable(prompt, camera)) drawBuildingActions(prompt, camera, controls, opacity);
        return;
    }
    if(prompt.targetKind==InteractionPromptTargetKind::Building &&
        prompt.input==ControlAction::Upgrade && prompt.cost) {
        // MissingGrace/fade can retain the previous target after a fast turn.
        // Revalidate that cached world anchor before perspective sizing: near
        // the camera plane its projection grows without bound and flips behind us.
        if(projectable(prompt,camera)) drawCompactUpgrade(prompt,camera,ui,controls,opacity);
        return;
    }
    if(prompt.input==ControlAction::Interact &&
        ((prompt.targetKind==InteractionPromptTargetKind::Resource && !prompt.cost) ||
            prompt.targetKind==InteractionPromptTargetKind::Chest ||
            prompt.targetKind==InteractionPromptTargetKind::Challenge ||
            prompt.targetKind==InteractionPromptTargetKind::Landmark ||
            (prompt.targetKind==InteractionPromptTargetKind::Building &&
                (prompt.actionText=="DOOR" || prompt.actionText=="OPEN" ||
                    prompt.actionText=="CLOSE")))) {
        if(projectable(prompt,camera)) drawWorldAction(prompt,camera,ui,controls,opacity);
        return;
    }
    if(prompt.targetKind==InteractionPromptTargetKind::Loot &&
        prompt.input==ControlAction::Interact) {
        if(projectable(prompt,camera)) drawWorldLoot(prompt,camera,controls,opacity);
        return;
    }
    const auto anchor=uiWorldToScreen(prompt.worldAnchor,camera);
    auto l=layout(prompt,anchor,{static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())},controls);
    const float s=l.scale;
    if(failureRemaining_>0) l.bounds.x+=std::sin(failureRemaining_*90)*3*s;
    hintStyle::panel(l.bounds,s,opacity);
    const float x=l.bounds.x+16*s;
    const float width=l.bounds.width-32*s;
    if(!prompt.objectName.empty()) hintStyle::text(prompt.objectName,
        {x,l.bounds.y+10*s,width,24*s},18*s,
        prompt.targetKind==InteractionPromptTargetKind::Loot ? prompt.accentColor : menuStyle::Gold,
        UiFontRole::MenuBodyBold,opacity);
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
