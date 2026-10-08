#include "ui/HudBuildingContextCard.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "ui/BuildingCardLayout.hpp"
#include "ui/UiChamferedPlate.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"
#include "ui/UiViewport.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace ian::hud_detail {

std::string actionKeyLabel(
    const ControlSettings& controls, ControlAction action) {
    const int key = controlKey(controls, action);
    if (action == ControlAction::Attack) {
        if (key == KEY_NULL) {
            return "LMB";
        }
        return "LMB / " + keyboardKeyName(key);
    }
    if (action == ControlAction::Dash && key == KEY_NULL) {
        return "RMB";
    }
    return keyboardKeyName(key);
}


namespace {
struct StatRow {
    std::string_view label;
    double value;
    std::optional<double> next;
    int decimals{};
    const char* suffix = "";
};

std::string statValue(double value, int decimals, const char* suffix) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.*f%s", decimals, value, suffix);
    return buffer;
}

float smooth01(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

float easeOutBack(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F) - 1.0F;
    constexpr float overshoot = 1.35F;
    return 1.0F + (overshoot + 1.0F) * t * t * t + overshoot * t * t;
}

void drawStatsCard(std::string_view title, int level, Vector3 base,
                   double maxHealth, std::optional<double> nextMaxHealth,
                   std::span<const StatRow> rows,
                   const Camera3D& camera,
                   float entranceSeconds, float buildingHalfWidth, bool isCore) {
    const Vector2 viewport{static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())};
    const auto projection = projectBuildingCard(
        Vector3Add(base, {0, isCore ? .95F : 1.15F, 0}), camera, viewport);
    if (!projection) return;
    const float progress = smooth01(entranceSeconds / .12F);
    // Match the health bar's hover fade while preserving the old card pop.
    const float opacity = 1.0F - std::exp(-entranceSeconds / .035F);
    constexpr float CardSizeScale = 1.4F;
    const float panelWidth = 460 * CardSizeScale;
    const float panelHeight =
        (98 + static_cast<float>(rows.size()) * 29) * CardSizeScale;
    const auto transform = buildingCardTransform(*projection, viewport,
        easeOutBack(progress), (1.0F-progress)*18.0F,
        panelWidth, panelHeight,
        buildingHalfWidth, isCore, 0.55F);
    const float scale = transform.scale * CardSizeScale;
    if (scale <= .001F || opacity <= .001F) return;
    const float width = 460 * scale;
    const float height = panelHeight * transform.scale;
    const float x = transform.offsetX + 407 * transform.scale;
    const float y = transform.offsetY + 284 * transform.scale;
    const auto faded = [opacity](Color color) {
        color.a = static_cast<unsigned char>(
            std::lround(static_cast<float>(color.a) * opacity));
        return color;
    };
    // Fill once: stacking translucent plate layers makes the panel opaque.
    const float cut = 12 * scale;
    drawChamferedFill({x,y,width,height}, cut, faded({32,44,38,198}));
    const std::array<Vector2, 9> outline{{
        {x+cut,y}, {x,y+cut}, {x,y+height-cut}, {x+cut,y+height},
        {x+width-cut,y+height}, {x+width,y+height-cut},
        {x+width,y+cut}, {x+width-cut,y}, {x+cut,y},
    }};
    for (std::size_t i = 1; i < outline.size(); ++i)
        DrawLineEx(outline[i-1], outline[i], 3 * scale, faded({8,15,11,220}));
    DrawLineEx({x+12*scale,y+48*scale}, {x+width-12*scale,y+48*scale},
        scale, faded({171,190,169,110}));
    const auto point = [=](float px, float py) { return Vector2{x+px*scale,y+py*scale}; };
    const float titleFont = std::min(19.0F, 19.0F * 330.0F /
        std::max(330.0F, measureUiText(title, 19).x)) * scale;
    drawSdfUiText(title, point(16,9), titleFont, faded({255,244,216,255}));
    if (level > 0) {
        const std::string label = "LVL " + std::to_string(level);
        drawSdfUiText(label, point(444-measureUiText(label,13).x,15),13*scale,
            faded({255,204,25,255}));
    }
    drawSdfUiText("MAX HEALTH", point(16,62), 13*scale, faded({202,211,202,255}));
    const std::string hp = statValue(maxHealth,0,"");
    const bool healthChanges = nextMaxHealth && std::abs(*nextMaxHealth-maxHealth)>.0001;
    const std::string nextHp = healthChanges ? statValue(*nextMaxHealth,0,"") : std::string{};
    const float nextHpWidth = healthChanges ? measureUiText(nextHp,13).x : 0;
    const float hpRight = healthChanges ? 444-nextHpWidth-26 : 444;
    drawSdfUiText(hp, point(hpRight-measureUiText(hp,13).x,62), 13*scale, faded(UiWhite));
    if (healthChanges) {
        drawSdfUiText(">",point(444-nextHpWidth-20,62),13*scale,
            faded({235,218,159,255}));
        drawSdfUiText(nextHp,point(444-nextHpWidth,62),13*scale,
            faded({71,236,123,255}));
    }
    for (std::size_t i=0; i<rows.size(); ++i) {
        const auto& row=rows[i];
        const float rowY=91+static_cast<float>(i)*29;
        drawSdfUiText(row.label, point(16,rowY), 12*scale,faded({202,211,202,255}));
        const auto current=statValue(row.value,row.decimals,row.suffix);
        const bool changes=row.next && std::abs(*row.next-row.value)>.0001;
        const auto next=changes ? statValue(*row.next,row.decimals,row.suffix) : std::string{};
        const float nextWidth=changes ? measureUiText(next,13).x : 0;
        const float currentRight=changes ? 444-nextWidth-26 : 444;
        drawSdfUiText(current,point(currentRight-measureUiText(current,13).x,rowY),13*scale,faded(UiWhite));
        if (changes) {
            drawSdfUiText(">",point(444-nextWidth-20,rowY),13*scale,
                faded({235,218,159,255}));
            drawSdfUiText(next,point(444-nextWidth,rowY),13*scale,
                faded({71,236,123,255}));
        }
    }
}
} // namespace

void drawBuildingContextCard(
    GameUi&, const SimulationSnapshot& snapshot,
    const BuildingInstance& building, const HudViewState& view,
    const Camera3D& camera, const ControlSettings&) {
    std::array<StatRow, 20> rows{};
    std::size_t count=0;
    if (snapshot.aimedBuilding == building.id && snapshot.aimedBuildingStats) {
        const auto& comparison=*snapshot.aimedBuildingStats;
        const auto add=[&](std::string_view label, std::optional<double> BuildingStats::* member,
                          int decimals=0, const char* suffix="") {
            if (const auto value=comparison.current.*member)
                rows[count++]={label,*value,comparison.next ? comparison.next.value().*member : std::nullopt,decimals,suffix};
        };
        add("DAMAGE", &BuildingStats::attackDamage);
        add("RANGE", &BuildingStats::attackRange,1,"m");
        add("ARC", &BuildingStats::attackArcDegrees,0," deg");
        add("ATTACK RATE", &BuildingStats::attacksPerSecond,2,"/s");
        add("PIERCING", &BuildingStats::piercingCount);
        add("RADIUS", &BuildingStats::effectRadius,2,"m");
        add("SLOW", &BuildingStats::slowPercent,0,"%");
        add("DURATION", &BuildingStats::effectDuration,1,"s");
        add("COOLDOWN", &BuildingStats::cooldown,2,"s");
        add("PER CYCLE", &BuildingStats::productionPerCycle);
        add("CYCLE TIME", &BuildingStats::productionInterval,1,"s");
        add("CAPACITY", &BuildingStats::producerCapacity);
        add("DEFENSE LIMIT", &BuildingStats::defenseBuildingLimit);
        add("EACH PRODUCER", &BuildingStats::producerPerTypeLimit);
        add("WOOD CAPACITY", &BuildingStats::woodCapacity);
        add("STONE CAPACITY", &BuildingStats::stoneCapacity);
        add("CRYSTAL CAPACITY", &BuildingStats::crystalCapacity);
    }
    const Vec3 center=buildingWorldPosition(building);
    const std::optional<double> nextMaxHealth =
        snapshot.aimedBuilding == building.id && snapshot.aimedBuildingStats &&
        snapshot.aimedBuildingStats->next
            ? std::optional<double>{snapshot.aimedBuildingStats->next->maxHealth}
            : std::nullopt;
    drawStatsCard(buildingDisplayName(building.type),building.level,
        {static_cast<float>(center.x),static_cast<float>(center.y),static_cast<float>(center.z)},
        building.maxHealth,nextMaxHealth,std::span{rows}.first(count),camera,
        view.buildingContextCardEntranceSeconds,
        static_cast<float>(buildingFootprintHalfExtent(building.type)),
        building.type == BuildingType::Core);
}

void drawModularBuildingContextCard(const SimulationSnapshot& snapshot,
                                    const Camera3D& camera, float entranceSeconds) {
    if (!snapshot.aimedModularBuilding) return;
    const EntityId id=*snapshot.aimedModularBuilding;
    const float cell=static_cast<float>(snapshot.worldCellSize);
    for (const auto& frame:snapshot.platformFrames) if (frame.id==id) {
        drawStatsCard("PLATFORM",0,{(frame.anchor.x+1)*cell,static_cast<float>(frame.floorHeight),
            (frame.anchor.z+1)*cell},frame.maxHealth,std::nullopt,{},camera,
            entranceSeconds,cell,false);
        return;
    }
    for (const auto& wall:snapshot.modularWalls) if (wall.id==id) {
        const Vec3 center{(wall.anchor.x+.5)*snapshot.worldCellSize,
            wall.bottomHeight,(wall.anchor.z+.5)*snapshot.worldCellSize};
        drawStatsCard("WALL",0,{static_cast<float>(center.x),static_cast<float>(center.y),
            static_cast<float>(center.z)},wall.maxHealth,std::nullopt,{},camera,
            entranceSeconds,cell*.5F,false);
        return;
    }
    for (const auto& ramp:snapshot.ramps) if (ramp.id==id) {
        const bool alongZ=ramp.rotation==Rotation::Deg0 || ramp.rotation==Rotation::Deg180;
        const float width=alongZ ? ModularRampWidthCells : ModularRampRunCells;
        const float depth=alongZ ? ModularRampRunCells : ModularRampWidthCells;
        drawStatsCard("RAMP",0,{(ramp.anchor.x+width*.5F)*cell,static_cast<float>(ramp.topHeight),
            (ramp.anchor.z+depth*.5F)*cell},ramp.maxHealth,std::nullopt,{},camera,
            entranceSeconds,std::max(width,depth)*cell*.5F,false);
        return;
    }
}

} // namespace ian::hud_detail
