#pragma once

#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiItemTooltip.hpp"
#include "ui/UiViewport.hpp"

#include <charconv>
#include <limits>
#include <optional>

namespace ian {

using DebugUpgradeSelection = std::array<bool, RunUpgradeEffectCount>;
enum class DebugUpgradeCategory { All, Player, Defense, Economy, Unlocks, Utility };
enum class DebugUpgradeField { None, Search, Count };
struct DebugUpgradeGrant {
    DebugUpgradeSelection selected{};
    int count{1};
    RunUpgradeRarity rarity{RunUpgradeRarity::Common};
};
struct DebugUpgradeMenuState {
    DebugUpgradeSelection selected{};
    DebugUpgradeCategory category{};
    DebugUpgradeField field{};
    RunUpgradeRarity rarity{RunUpgradeRarity::Common};
    RunUpgradeEffect focused{RunUpgradeEffect::DefenseDamage};
    std::string search;
    std::string countText{"1"};
    std::string feedback;
    float scroll{};
};
struct DebugUpgradeMenuInput {
    Vector2 pointer{-1, -1};
    bool click{};
    float wheel{};
    std::string text;
    bool backspace{};
    bool enter{};
};
struct DebugUpgradeMenuResult {
    std::optional<DebugUpgradeGrant> grant;
    bool close{};
};

inline bool debugUpgradeUnique(RunUpgradeEffect effect) {
    return runUpgradeBuilding(effect).has_value() || runUpgradeWeapon(effect).has_value()
        || runUpgradeCannonStyle(effect).has_value();
}
inline DebugUpgradeCategory debugUpgradeCategory(RunUpgradeEffect effect) {
    if (debugUpgradeUnique(effect)) return DebugUpgradeCategory::Unlocks;
    switch (effect) {
    case RunUpgradeEffect::BuildingHealth: case RunUpgradeEffect::BuildRadius:
    case RunUpgradeEffect::DefenseDamage: case RunUpgradeEffect::DefenseFireRate:
    case RunUpgradeEffect::Overkill: case RunUpgradeEffect::Bounces:
    case RunUpgradeEffect::ProjectileCount: case RunUpgradeEffect::ClusterPayload:
    case RunUpgradeEffect::ReactiveTraps: case RunUpgradeEffect::Pierce:
    case RunUpgradeEffect::GunShrapnel: case RunUpgradeEffect::ProjectileSpeed:
    case RunUpgradeEffect::AttackDuration: case RunUpgradeEffect::AttackSize:
    case RunUpgradeEffect::CriticalChance: case RunUpgradeEffect::CriticalDamage:
    case RunUpgradeEffect::AttackKnockback: return DebugUpgradeCategory::Defense;
    case RunUpgradeEffect::ProductionSpeed: case RunUpgradeEffect::GatheringSpeed:
    case RunUpgradeEffect::RichResources: case RunUpgradeEffect::Salvager:
    case RunUpgradeEffect::LumberMillProduction: case RunUpgradeEffect::QuarryProduction:
    case RunUpgradeEffect::CrystalMineProduction: return DebugUpgradeCategory::Economy;
    case RunUpgradeEffect::Damage: case RunUpgradeEffect::AttackSpeed:
    case RunUpgradeEffect::MoveSpeed: case RunUpgradeEffect::MaximumHealth:
    case RunUpgradeEffect::RecoverableArmor: case RunUpgradeEffect::BloodHarvest:
    case RunUpgradeEffect::Regeneration: return DebugUpgradeCategory::Player;
    default: return DebugUpgradeCategory::Utility;
    }
}
inline std::string debugUpgradeFold(std::string_view value) {
    std::string result;
    for (std::size_t i = 0; i < value.size();) {
        int bytes = 0;
        int code = GetCodepointNext(value.data() + i, &bytes);
        if (bytes <= 0) break;
        i += static_cast<std::size_t>(bytes);
        if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
        if (code >= 0x410 && code <= 0x42F) code += 0x20;
        if (code == 0x401) code = 0x451;
        int length = 0;
        const char* encoded = CodepointToUTF8(code, &length);
        result.append(encoded, static_cast<std::size_t>(length));
    }
    return result;
}
inline bool debugUpgradeMatches(const RunUpgradeDefinition& definition,
                                const DebugUpgradeMenuState& state) {
    if (retiredRunUpgrade(definition.effect)) return false;
    if (state.category != DebugUpgradeCategory::All
        && state.category != debugUpgradeCategory(definition.effect)) return false;
    if (state.search.empty()) return true;
    const std::string haystack = std::string(definition.name) + " "
        + std::string(definition.description) + " " + localizeText(definition.name)
        + " " + localizeText(definition.description);
    return debugUpgradeFold(haystack).find(debugUpgradeFold(state.search)) != std::string::npos;
}
inline int debugUpgradeCount(std::string_view text) {
    int count = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && count > 0 ? count : 0;
}
inline void debugUpgradeBackspace(std::string& value) {
    if (value.empty()) return;
    std::size_t index = value.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(value[index]) & 0xC0U) == 0x80U) --index;
    value.erase(index);
}
inline bool debugUpgradeGrantable(RunUpgradeEffect effect, const SimulationSnapshot& snapshot) {
    if (const auto building = runUpgradeBuilding(effect)) {
        const auto* character = playerClassDefinition(snapshot.playerClass);
        const auto starting = character ? character->startingBuilding : BuildingType::GunTurret;
        return *building != starting && snapshot.runUpgradeStacks[runUpgradeIndex(effect)] == 0;
    }
    if (const auto weapon = runUpgradeWeapon(effect)) {
        const auto* character = playerClassDefinition(snapshot.playerClass);
        return !snapshot.unlockedWeapons[static_cast<std::size_t>(*weapon)]
            && (!character || *weapon != character->startingWeapon)
            && snapshot.runUpgradeStacks[runUpgradeIndex(effect)] == 0;
    }
    return !debugUpgradeUnique(effect) || snapshot.runUpgradeStacks[runUpgradeIndex(effect)] == 0;
}
inline void debugUpgradeSelect(DebugUpgradeMenuState& state, RunUpgradeEffect effect, bool selected) {
    if (selected && runUpgradeCannonStyle(effect)) {
        for (const auto& definition : RunUpgradeDefinitions)
            if (runUpgradeCannonStyle(definition.effect)) state.selected[runUpgradeIndex(definition.effect)] = false;
    }
    state.selected[runUpgradeIndex(effect)] = selected;
}

struct DebugUpgradeMenuLayout {
    Rectangle panel{}, catalog{}, details{}, search{}, clearSearch{}, close{};
    std::array<Rectangle, 6> tabs{};
    Rectangle selectVisible{}, clearSelection{}, count{}, minus{}, plus{}, grant{};
    std::array<Rectangle, 5> rarities{};
    std::array<Rectangle, 4> presets{};
    float scale{1};
    int columns{3};
    bool portrait{};
    [[nodiscard]] Rectangle card(int index, float scroll) const {
        const float gap = 12 * scale;
        const float width = (catalog.width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        return {catalog.x + static_cast<float>(index % columns) * (width + gap),
            catalog.y + static_cast<float>(index / columns) * 112 * scale - scroll, width, 100 * scale};
    }
};
inline DebugUpgradeMenuLayout debugUpgradeMenuLayout(Vector2 screen) {
    DebugUpgradeMenuLayout l;
    l.portrait = screen.x / screen.y < 1.2F;
    l.columns = l.portrait ? 2 : 3;
    const float width = l.portrait ? 860.0F : 1480.0F;
    const float height = l.portrait ? 1480.0F : 880.0F;
    l.scale = std::min({1.0F, (screen.x - 48) / width, (screen.y - 48) / height});
    const float s = l.scale;
    l.panel = {(screen.x - width*s)*.5F, (screen.y-height*s)*.5F, width*s, height*s};
    const auto r = [&](float x, float y, float w, float h) {
        return Rectangle{l.panel.x+x*s,l.panel.y+y*s,w*s,h*s};
    };
    const float catalogWidth = l.portrait ? width-56 : 1000;
    l.search = r(28,112,catalogWidth-64,54);
    l.clearSearch = r(28+catalogWidth-54,112,54,54);
    l.close = r(width-76,28,48,48);
    for (std::size_t i=0;i<l.tabs.size();++i)
        l.tabs[i]=r(28+static_cast<float>(i)*(catalogWidth+8)/6,180,(catalogWidth-40)/6,40);
    l.catalog = r(28,234,catalogWidth,l.portrait ? 522 : 538);
    l.selectVisible = r(28,l.portrait?772:790,238,54);
    l.clearSelection = r(280,l.portrait?772:790,238,54);
    l.details = l.portrait ? r(28,850,width-56,602) : r(1052,112,400,740);
    const auto d = [&](float x,float y,float w,float h) {
        return Rectangle{l.details.x+x*s,l.details.y+y*s,w*s,h*s};
    };
    const float dw = l.details.width/s;
    const float controlsY = l.portrait ? 218 : 280;
    for (std::size_t i=0;i<5;++i) {
        const float rowWidth = i < 3 ? (dw-56)/3 : (dw-48)/2;
        const float column = static_cast<float>(i < 3 ? i : i-3);
        l.rarities[i]=d(20+column*(rowWidth+8),controlsY+(i<3?0:48),rowWidth,40);
    }
    l.minus=d(20,controlsY+136,50,52);
    l.count=d(82,controlsY+136,dw-164,52);
    l.plus=d(dw-70,controlsY+136,50,52);
    for (std::size_t i=0;i<4;++i) l.presets[i]=d(20+static_cast<float>(i)*(dw-32)/4,controlsY+202,(dw-64)/4,40);
    l.grant=d(20,l.details.height/s-82,dw-40,60);
    return l;
}

inline DebugUpgradeMenuResult drawDebugUpgradeMenu(GameUi& ui, const SimulationSnapshot& snapshot,
    DebugUpgradeMenuState& state, Vector2 screen, const DebugUpgradeMenuInput& input = {}) {
    namespace ms = menuStyle;
    const auto l=debugUpgradeMenuLayout(screen);
    const float s=l.scale;
    DebugUpgradeMenuResult result;
    const auto label=[&](std::string_view value,Rectangle bounds,float size,Color color=ms::Cream,
                        UiFontRole role=UiFontRole::Display,int align=0) {
        ms::text(localizeText(value),bounds,size*s,color,role,align);
    };
    const auto button=[&](Rectangle bounds,std::string_view value,float size=18,bool enabled=true) {
        return ms::button(bounds,s,localizeText(value),size,input.pointer,input.click,enabled);
    };
    DrawRectangle(0,0,static_cast<int>(screen.x),static_cast<int>(screen.y),{6,12,9,185});
    ms::panel(l.panel,s);
    label("GRANT UPGRADES",{l.panel.x+28*s,l.panel.y+20*s,l.panel.width-140*s,44*s},32,ms::Gold);
    label(state.feedback.empty() ? localizeText("DEBUG CATALOG · SHIFT + F5") : state.feedback,{l.panel.x+28*s,l.panel.y+68*s,l.panel.width-140*s,24*s},16,ms::Muted,UiFontRole::Body);
    result.close=ms::closeButton(l.close,s,input.pointer,input.click);

    if(input.click) state.field=CheckCollisionPointRec(input.pointer,l.search)?DebugUpgradeField::Search
        :CheckCollisionPointRec(input.pointer,l.count)?DebugUpgradeField::Count:DebugUpgradeField::None;
    if(state.field!=DebugUpgradeField::None) {
        auto& value=state.field==DebugUpgradeField::Search?state.search:state.countText;
        const std::string before=value;
        if(input.backspace) debugUpgradeBackspace(value);
        if (state.field == DebugUpgradeField::Search) {
            for (std::size_t i = 0; i < input.text.size();) {
                int bytes = 0;
                GetCodepointNext(input.text.data() + i, &bytes);
                if (bytes <= 0 || value.size() + static_cast<std::size_t>(bytes) > 128U) break;
                value.append(input.text, i, static_cast<std::size_t>(bytes));
                i += static_cast<std::size_t>(bytes);
            }
        } else {
            for (char ch : input.text)
                if (ch >= '0' && ch <= '9' && value.size() < 10U) value += ch;
        }
        if(state.search!=before && state.field==DebugUpgradeField::Search) state.scroll=0;
    }
    ms::panel(l.search,s,state.field==DebugUpgradeField::Search?ms::Hover:ms::Inset);
    label(state.search.empty()?"SEARCH UPGRADES...":state.search,
        {l.search.x+16*s,l.search.y,l.search.width-32*s,l.search.height},20,
        state.search.empty()?ms::Muted:ms::Cream,UiFontRole::Body);
    if(button(l.clearSearch,"X")){state.search.clear();state.scroll=0;}
    constexpr std::array<std::string_view,6> Categories{"ALL","PLAYER","DEFENSE","ECONOMY","UNLOCKS","UTILITY"};
    for(std::size_t i=0;i<6;++i) {
        const bool selected=static_cast<std::size_t>(state.category)==i;
        ms::panel(l.tabs[i],s,selected?ms::Selected:ms::Card);
        label(Categories[i],l.tabs[i],17,ms::Cream,UiFontRole::Display,1);
        if(input.click&&CheckCollisionPointRec(input.pointer,l.tabs[i])) {
            state.category=static_cast<DebugUpgradeCategory>(i);state.scroll=0;
        }
    }
    std::array<RunUpgradeEffect,RunUpgradeEffectCount> visible{};
    int visibleCount=0;
    for(const auto& definition:RunUpgradeDefinitions) {
        if(!debugUpgradeGrantable(definition.effect,snapshot)) state.selected[runUpgradeIndex(definition.effect)]=false;
        if(debugUpgradeMatches(definition,state)) visible[static_cast<std::size_t>(visibleCount++)]=definition.effect;
    }
    const float contentHeight=static_cast<float>((visibleCount+l.columns-1)/l.columns)*112*s-12*s;
    const float maxScroll=std::max(0.0F,contentHeight-l.catalog.height);
    const bool inCatalog=CheckCollisionPointRec(input.pointer,l.catalog);
    if(inCatalog) state.scroll-=input.wheel*90*s;
    state.scroll=std::clamp(state.scroll,0.0F,maxScroll);
    beginUiScissor(static_cast<int>(l.catalog.x),static_cast<int>(l.catalog.y),
        static_cast<int>(l.catalog.width),static_cast<int>(l.catalog.height));
    {
    const ScopedSdfUiTextCache directText(false);
    for(int i=0;i<visibleCount;++i) {
        const auto effect=visible[static_cast<std::size_t>(i)];
        const auto& definition=RunUpgradeDefinitions[runUpgradeIndex(effect)];
        const auto bounds=l.card(i,state.scroll);
        if(bounds.y+bounds.height<l.catalog.y||bounds.y>l.catalog.y+l.catalog.height) continue;
        const bool hover=inCatalog&&CheckCollisionPointRec(input.pointer,bounds);
        const bool enabled=debugUpgradeGrantable(effect,snapshot);
        if(hover) {
            state.focused=effect;
            if(input.click&&enabled) debugUpgradeSelect(state,effect,!state.selected[runUpgradeIndex(effect)]);
        }
        const bool selected=state.selected[runUpgradeIndex(effect)];
        ms::panel(bounds,s,selected?ms::Selected:hover?ms::Hover:enabled?ms::Card:ms::Disabled);
        const auto icon=ui.runUpgradeTexture(effect);
        if(IsTextureValid(icon)) DrawTexturePro(icon,{0,0,static_cast<float>(icon.width),static_cast<float>(icon.height)},
            {bounds.x+12*s,bounds.y+20*s,60*s,60*s},{},0,enabled?WHITE:Fade(WHITE,.45F));
        label(definition.name,{bounds.x+82*s,bounds.y+12*s,bounds.width-98*s,44*s},19,
            enabled?ms::Cream:ms::Muted);
        const int level=snapshot.runUpgradeStacks[runUpgradeIndex(effect)];
        const std::string status=enabled?localizeText("OWNED")+": "+std::to_string(level):localizeText("ACTIVE");
        label(status,{bounds.x+82*s,bounds.y+61*s,bounds.width-100*s,26*s},15,
            enabled?ms::Cream:ms::Green,UiFontRole::Body);
        if(selected) label("+",{bounds.x+bounds.width-29*s,bounds.y+68*s,20*s,24*s},22,ms::Cream,UiFontRole::Display,1);
    }
    }
    EndScissorMode();
    if(visibleCount==0) label("NO MATCHING UPGRADES",l.catalog,24,ms::Muted,UiFontRole::Display,1);
    if(maxScroll>0) {
        const float thumb=std::max(24*s,l.catalog.height*l.catalog.height/contentHeight);
        DrawRectangleRec({l.catalog.x+l.catalog.width+5*s,l.catalog.y,4*s,l.catalog.height},ms::Inset);
        DrawRectangleRec({l.catalog.x+l.catalog.width+5*s,l.catalog.y+(l.catalog.height-thumb)*state.scroll/maxScroll,4*s,thumb},ms::Gold);
    }
    if(button(l.selectVisible,"SELECT VISIBLE"))
        for(int i=0;i<visibleCount;++i) if(debugUpgradeGrantable(visible[static_cast<std::size_t>(i)],snapshot))
            debugUpgradeSelect(state,visible[static_cast<std::size_t>(i)],true);
    if(button(l.clearSelection,"CLEAR SELECTION")) state.selected.fill(false);
    const int selectedCount=static_cast<int>(std::count(state.selected.begin(),state.selected.end(),true));
    label(localizeText("SELECTED")+": "+std::to_string(selectedCount),
        {l.clearSelection.x+l.clearSelection.width+16*s,l.clearSelection.y,
         l.catalog.width-506*s,54*s},20,ms::Gold,UiFontRole::Display,2);

    ms::panel(l.details,s,ms::Inset);
    const auto d=[&](float x,float y,float w,float h){return Rectangle{l.details.x+x*s,l.details.y+y*s,w*s,h*s};};
    const float dw=l.details.width/s;
    const auto& focused=RunUpgradeDefinitions[runUpgradeIndex(state.focused)];
    const auto icon=ui.runUpgradeTexture(state.focused);
    if(IsTextureValid(icon)) DrawTexturePro(icon,{0,0,static_cast<float>(icon.width),static_cast<float>(icon.height)},d(20,20,84,84),{},0,WHITE);
    label(focused.name,d(120,22,dw-140,54),25,ms::Gold);
    label(localizeText("OWNED")+": "+std::to_string(snapshot.runUpgradeStacks[runUpgradeIndex(state.focused)]),
        d(120,80,dw-140,24),17,ms::Muted,UiFontRole::Body);
    const auto lines=itemTooltipLines(localizeText(focused.description),(dw-40)*s,19*s);
    for(std::size_t i=0;i<std::min<std::size_t>(lines.size(),4);++i)
        label(lines[i],d(20,124+26*static_cast<float>(i),dw-40,26),19,ms::Cream,UiFontRole::Body);
    const float controlsY=l.portrait?218.0F:280.0F;
    label("RARITY",d(20,controlsY-34,dw-40,28),17,ms::Muted);
    constexpr std::array<Color,5> Colors{{{220,225,214,255},{95,224,103,255},{86,181,255,255},{207,121,255,255},{255,196,59,255}}};
    for(std::size_t i=0;i<5;++i) {
        ms::panel(l.rarities[i],s,static_cast<std::size_t>(state.rarity)==i?ms::Selected:ms::Card);
        label(runUpgradeRarityName(static_cast<RunUpgradeRarity>(i)),l.rarities[i],16,Colors[i],UiFontRole::Display,1);
        if(input.click&&CheckCollisionPointRec(input.pointer,l.rarities[i])) state.rarity=static_cast<RunUpgradeRarity>(i);
    }
    label("COPIES PER UPGRADE",d(20,controlsY+100,dw-40,28),17,ms::Muted);
    int count=debugUpgradeCount(state.countText);
    if(button(l.minus,"-")) state.countText=std::to_string(std::max(1,count-1));
    if(button(l.plus,"+")) state.countText=std::to_string(count<std::numeric_limits<int>::max()?count+1:count);
    ms::panel(l.count,s,state.field==DebugUpgradeField::Count?ms::Hover:ms::Card);
    label(state.countText.empty()?"0":state.countText,l.count,26,ms::Cream,UiFontRole::Display,1);
    constexpr std::array<int,4> Presets{1,10,100,1000};
    for(std::size_t i=0;i<4;++i) if(button(l.presets[i],std::to_string(Presets[i]))) state.countText=std::to_string(Presets[i]);
    count=debugUpgradeCount(state.countText);
    if(!l.portrait) {
        const auto notes=itemTooltipLines(localizeText("No level or blueprint limits. Unlocks remain unique."),(dw-40)*s,17*s);
        for(std::size_t i=0;i<notes.size();++i) label(notes[i],d(20,controlsY+268+23*static_cast<float>(i),dw-40,23),17,ms::Muted,UiFontRole::Body);

    }
    if(button(l.grant,"GRANT SELECTED",23,count>0&&selectedCount>0)
        ||(input.enter&&state.field!=DebugUpgradeField::Search&&count>0&&selectedCount>0))
        result.grant=DebugUpgradeGrant{state.selected,count,state.rarity};
    return result;
}
} // namespace ian
