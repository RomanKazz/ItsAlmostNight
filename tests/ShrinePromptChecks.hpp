#pragma once
#include "ui/InteractionPrompt.hpp"
#include "ui/GameUi.hpp"
#include "ui/InputKeycap.hpp"
#include "localization/Localization.hpp"
#include <filesystem>

inline void checkShrineResourcePrompt(ian::GameUi& ui,const char* captureDirectory) {
    using namespace ian;
    SetWindowSize(1280,720);SetTargetFPS(60);
    const auto target=LoadRenderTexture(1280,720);
    const Camera3D camera{{0,0,8},{0,0,0},{0,1,0},4,CAMERA_ORTHOGRAPHIC};
    ControlSettings controls{};controls.keys[static_cast<std::size_t>(ControlAction::Interact)]=KEY_F;
    InteractionPrompt prompt{
        .targetKind=InteractionPromptTargetKind::Building,.targetId={1,1},
        .worldAnchor={0,0,0},.actionText="Player bonus",
        .state=InteractionState::Warning,.cost=ResourceCost{20,12,4},.showResourceCost=true,
        .availableResources={0,99,99},
    };
    const auto render=[&](const InteractionPrompt& current,const char* name) {
        InteractionPromptRenderer painter;
        for(int frame=0;frame<16;++frame) {
            BeginDrawing();BeginTextureMode(target);ClearBackground({24,32,40,255});
            painter.draw(current,camera,ui,controls);EndTextureMode();EndDrawing();
        }
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        if(captureDirectory) {
            std::filesystem::create_directories(captureDirectory);
            require(ExportImage(image,(std::filesystem::path(captureDirectory)/name).string().c_str()),"unified prompt capture exports");
        }
        return image;
    };
    Image reference=render(prompt,"prompt-upgrade.png");
    prompt.targetKind=InteractionPromptTargetKind::Landmark;
    Image shrine=render(prompt,"shrine-resource-price.png");
    const Color* a=LoadImageColors(reference);const Color* b=LoadImageColors(shrine);
    const auto bounds=interactionPromptPanelBounds(prompt,{640,360},{1280,720},controls);
    int changed=0,red=0,cream=0,dark=0;
    for(int y=static_cast<int>(bounds.y);y<static_cast<int>(bounds.y+bounds.height);++y)
        for(int x=static_cast<int>(bounds.x);x<static_cast<int>(bounds.x+bounds.width);++x) {
            const auto i=y*reference.width+x;
            changed+=std::abs(static_cast<int>(a[i].r)-b[i].r)>6 ||
                std::abs(static_cast<int>(a[i].g)-b[i].g)>6 || std::abs(static_cast<int>(a[i].b)-b[i].b)>6;
            red+=b[i].r>220 && b[i].g>65 && b[i].g<140 && b[i].b<130;
            cream+=b[i].r>225 && b[i].g>225 && b[i].b>200;
            dark+=b[i].r<60 && b[i].g<65 && b[i].b<60;
        }
    require(changed<250 && red>30 && cream>30 && dark>1000,
        "shrine and upgrade share a dark panel with red missing and light available resources");
    UnloadImageColors(const_cast<Color*>(a));UnloadImageColors(const_cast<Color*>(b));
    UnloadImage(reference);UnloadImage(shrine);
    // Building upgrades use the earlier one-line arrow/price style. Shrines
    // and other interactions retain their general prompt cards.
    auto upgrade=prompt;
    upgrade.targetKind=InteractionPromptTargetKind::Building;
    upgrade.input=ControlAction::Upgrade;upgrade.actionText="Upgrade";
    Image compact=render(upgrade,"upgrade-arrow-pill.png");
    upgrade.availableResources={99,99,99};
    Image affordable=render(upgrade,"upgrade-arrow-pill-affordable.png");
    auto* compactPixels=LoadImageColors(compact);auto* affordablePixels=LoadImageColors(affordable);
    int greens=0,reds=0,affordableReds=0,whites=0,pillPixels=0;
    int minX=1280,maxX=0,minY=720,maxY=0;
    for(int y=0;y<compact.height;++y) for(int x=0;x<compact.width;++x) {
        const auto c=compactPixels[y*compact.width+x];
        const auto d=affordablePixels[y*compact.width+x];
        greens+=c.g>150 && c.g>c.r*1.6 && c.r<110;
        reds+=c.r>220 && c.g>65 && c.g<140 && c.b<130;
        affordableReds+=d.r>220 && d.g>65 && d.g<140 && d.b<130;
        whites+=c.r>225 && c.g>225 && c.b>200;
        // Black at 40% opacity over RGB(24,32,40), allowing integer blending.
        pillPixels+=c.r>=14 && c.r<=16 && c.g>=18 && c.g<=21 && c.b>=23 && c.b<=26;
        if(c.r!=24 || c.g!=32 || c.b!=40) {
            minX=std::min(minX,x);maxX=std::max(maxX,x);
            minY=std::min(minY,y);maxY=std::max(maxY,y);
        }
    }
    require(greens>80 && whites>30 && reds>affordableReds+20 && pillPixels>300,
        "upgrade row restores green arrow, per-resource colours and forty-percent black pill");
    require(maxY-minY<70 && minY<360 && maxY>360,
        "upgrade prompt is one compact row anchored over the building");
    require(std::abs((minX+maxX)*.5F-640)<=2,
        "keycap, arrow and resource price are centred together over the building");
    int keyPeach=0,keyTan=0;
    for(int y=330;y<390;++y) for(int x=480;x<550;++x) {
        const auto c=affordablePixels[y*affordable.width+x];
        keyPeach+=c.r>220 && c.r<235 && c.g>180 && c.g<195 && c.b>140 && c.b<155;
        keyTan+=c.r>140 && c.r<155 && c.g>108 && c.g<123 && c.b>65 && c.b<80;
    }
    require(keyPeach>300 && keyTan>40,
        "upgrade keycap has the hotbar peach face and visible brown lower edge");
    require(InputKeycap::label(controls,ControlAction::Upgrade)=="U",
        "upgrade keycap defaults to U");
    controls.keys[static_cast<std::size_t>(ControlAction::Upgrade)]=KEY_I;
    Image rebound=render(upgrade,"upgrade-arrow-pill-rebound.png");
    auto* reboundPixels=LoadImageColors(rebound);
    int keyChanges=0,priceChanges=0;
    for(int y=0;y<rebound.height;++y) for(int x=0;x<rebound.width;++x) {
        const auto c=affordablePixels[y*rebound.width+x];
        const auto d=reboundPixels[y*rebound.width+x];
        if(c.r==d.r && c.g==d.g && c.b==d.b) continue;
        if(x<640) ++keyChanges;else ++priceChanges;
    }
    require(keyChanges>20 && priceChanges==0,
        "rebound upgrade letter changes on the left without moving the resource price");
    controls.keys[static_cast<std::size_t>(ControlAction::Upgrade)]=KEY_U;
    UnloadImageColors(reboundPixels);UnloadImage(rebound);
    UnloadImageColors(compactPixels);UnloadImageColors(affordablePixels);
    UnloadImage(compact);UnloadImage(affordable);
    // Prime the appearance at a real perspective depth, then turn away in a
    // single frame while the old prompt is still held by MissingGrace/fade.
    const Camera3D worldCamera{{0,0,8},{0,0,0},{0,1,0},60,CAMERA_PERSPECTIVE};
    const auto drawWorldPrompt=[&](InteractionPromptRenderer& painter,
        const std::optional<InteractionPrompt>& current,const Camera3D& view) {
        BeginDrawing();BeginTextureMode(target);ClearBackground({24,32,40,255});
        painter.draw(current,view,ui,controls);EndTextureMode();EndDrawing();
    };
    const auto visiblePixels=[](Image image) {
        const auto* pixels=LoadImageColors(image);int changed=0;
        for(int i=0;i<image.width*image.height;++i)
            changed+=pixels[i].r!=24 || pixels[i].g!=32 || pixels[i].b!=40;
        UnloadImageColors(const_cast<Color*>(pixels));return changed;
    };
    for(float degrees : {70.0F,89.9F,90.0F,90.1F,110.0F,180.0F}) {
        InteractionPromptRenderer painter;
        for(int frame=0;frame<12;++frame) drawWorldPrompt(painter,upgrade,worldCamera);
        auto turned=worldCamera;
        const float angle=degrees*DEG2RAD;
        turned.target={std::sin(angle),0,8-std::cos(angle)};
        drawWorldPrompt(painter,std::nullopt,turned);
        Image image=LoadImageFromTexture(target.texture);
        require(visiblePixels(image)==0,
            "cached upgrade price disappears off camera, including either side of the projection plane");
        UnloadImage(image);
    }
    // At the right edge the price is genuinely clipped, not shifted inward.
    auto edgeCamera=worldCamera;
    const float edgeAngle=std::atan((1280.0F-8.0F-640.0F)/(360.0F/std::tan(30*DEG2RAD)));
    edgeCamera.target={-std::sin(edgeAngle),0,8-std::cos(edgeAngle)};
    InteractionPromptRenderer edgePainter;
    for(int frame=0;frame<12;++frame) drawWorldPrompt(edgePainter,upgrade,edgeCamera);
    Image edge=LoadImageFromTexture(target.texture);ImageFlipVertical(&edge);
    int edgeInk=0;
    for(int y=0;y<edge.height;++y) {
        const auto c=GetImageColor(edge,edge.width-1,y);
        edgeInk+=c.r!=24 || c.g!=32 || c.b!=40;
    }
    require(edgeInk>10,"world upgrade price leaves the screen instead of sticking to its edge");
    if(captureDirectory) ExportImage(edge,(std::filesystem::path(captureDirectory)/"upgrade-world-edge.png").string().c_str());
    UnloadImage(edge);
    require(InputKeycap::label(controls,ControlAction::Interact)=="F","unified prompt preserves remapped action key");
    for(const auto screen:{Vector2{640,360},Vector2{1024,768},Vector2{1920,1080},Vector2{800,1200}})
        for(const auto anchor:{Vector2{0,0},screen,Vector2{screen.x*.5F,screen.y*.5F}}) {
            const auto panel=interactionPromptPanelBounds(prompt,anchor,screen,controls);
            require(panel.x>=0 && panel.y>=0 && panel.x+panel.width<=screen.x && panel.y+panel.height<=screen.y,
                "localized prompt stays inside small, tall and wide viewports");
            require(panel.width<=screen.x*.7F && panel.height<screen.y*.6F,"world prompt remains compact near its target");
        }
    auto barrel=prompt;barrel.targetKind=InteractionPromptTargetKind::Resource;
    barrel.actionText="Break open";barrel.cost.reset();
    const auto lowHint=interactionPromptPanelBounds(barrel,{640,20},{1280,720},controls);
    require(lowHint.y>=360 && lowHint.y<420,"crate/barrel hint stays close below the crosshair even when its world anchor is high");
    // An appearance fade changes alpha only. Compare actual rendered geometry
    // during entrance and after it settles, catching font/plate scale jitter.
    InteractionPromptRenderer stable;
    const auto inkBounds=[](Image image) {
        Rectangle ink{static_cast<float>(image.width),static_cast<float>(image.height),0,0};
        int maxX=0,maxY=0;
        const Color* pixels=LoadImageColors(image);
        for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
            const auto c=pixels[y*image.width+x];
            if(std::abs(static_cast<int>(c.r)-24)>12 || std::abs(static_cast<int>(c.g)-32)>12 ||
                std::abs(static_cast<int>(c.b)-40)>12) {
                ink.x=std::min(ink.x,static_cast<float>(x));ink.y=std::min(ink.y,static_cast<float>(y));
                maxX=std::max(maxX,x);maxY=std::max(maxY,y);
            }
        }
        ink.width=static_cast<float>(maxX)-ink.x;ink.height=static_cast<float>(maxY)-ink.y;
        UnloadImageColors(const_cast<Color*>(pixels));return ink;
    };
    Rectangle early{};
    for(int frame=0;frame<32;++frame) {
        BeginDrawing();BeginTextureMode(target);ClearBackground({24,32,40,255});
        stable.draw(barrel,camera,ui,controls);EndTextureMode();EndDrawing();
        if(frame==5 || frame==31) {
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            const auto ink=inkBounds(image);
            if(frame==5) early=ink;
            else require(std::abs(ink.x-early.x)<=2 && std::abs(ink.y-early.y)<=2 &&
                std::abs(ink.width-early.width)<=2 && std::abs(ink.height-early.height)<=2,
                "prompt geometry and text remain stationary while entrance opacity changes");
            UnloadImage(image);
        }
    }
    const auto occlusionImage=[&](InteractionPrompt current) {
        current.occluded=true;return render(current,"prompt-occluded.png");
    };
    Image hidden=occlusionImage(prompt);
    require(GetImageColor(hidden,640,360).r==24,"occluded interactions stay hidden");UnloadImage(hidden);
    for(const auto language:{Language::English,Language::Russian}) {
        setLanguage(language);
        const std::string suffix=language==Language::Russian ? "-ru.png" : ".png";
        for(int kind=0;kind<6;++kind) {
            auto sample=prompt;sample.cost.reset();sample.objectName.clear();sample.showResourceCost=false;
            std::string name;
            switch(kind) {
            case 0: sample.targetKind=InteractionPromptTargetKind::Resource;sample.actionText="Break open";name="prompt-barrel";break;
            case 1: sample.targetKind=InteractionPromptTargetKind::Challenge;sample.actionText="BEGIN TRIAL";name="prompt-trial";break;
            case 2: sample.targetKind=InteractionPromptTargetKind::Landmark;sample.actionText="Restore";
                sample.cost=ResourceCost{0,0,120};sample.availableCurrency=0;name="prompt-mine";break;
            case 3: sample.targetKind=InteractionPromptTargetKind::Chest;sample.actionText="Open";
                sample.cost=ResourceCost{0,0,30};sample.availableCurrency=99;name="prompt-chest";break;
            case 4: sample.targetKind=InteractionPromptTargetKind::ModularBuilding;sample.actionText="Repair Wall";
                sample.showProgress=true;sample.progress=.65F;name="prompt-repair";break;
            default: sample=prompt;name="prompt-shrine";break;
            }
            Image image=render(sample,(name+suffix).c_str());UnloadImage(image);
        }
    }
    setLanguage(Language::English);UnloadRenderTexture(target);SetTargetFPS(0);
}
