#include "graphics/Renderer.hpp"
#include "graphics/ExperienceCrystalStyle.hpp"
#include "graphics/WorldTransforms.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

void drawLootItemGeometry(
    LootUpgradeEffect effect, Color color) {
    if (effect == LootUpgradeEffect::Damage) {
        rlRotatef(45.0F, 0.0F, 0.0F, 1.0F);
        DrawCube({}, 0.32F, 0.32F, 0.32F, color);
    } else if (effect == LootUpgradeEffect::MoveSpeed) {
        DrawCube({-0.10F, 0.0F, 0.0F}, 0.12F, 0.48F, 0.18F, color);
        DrawCube({0.10F, 0.08F, 0.0F}, 0.12F, 0.38F, 0.18F, color);
    } else if (effect == LootUpgradeEffect::IronBar) {
        DrawCube({}, 0.48F, 0.18F, 0.20F, color);
    } else if (effect == LootUpgradeEffect::FuelJerrycan) {
        DrawCube({}, 0.30F, 0.46F, 0.18F, color);
        DrawCube({0.0F, 0.28F, 0.0F}, 0.14F, 0.10F, 0.14F, color);
    } else if (effect == LootUpgradeEffect::Compass) {
        DrawCylinder({}, 0.22F, 0.22F, 0.10F, 12, color);
        DrawCylinder({0.0F, 0.08F, 0.0F}, 0.04F, 0.04F, 0.22F, 6,
                     WHITE);
    } else if (effect == LootUpgradeEffect::Nail) {
        rlRotatef(45.0F, 0.0F, 0.0F, 1.0F);
        DrawCylinder({}, 0.045F, 0.045F, 0.40F, 8, color);
        DrawCylinder({0.0F, 0.22F, 0.0F}, 0.10F, 0.10F, 0.06F, 8,
                     color);
    } else if (effect == LootUpgradeEffect::Key) {
        DrawCylinder({}, 0.14F, 0.14F, 0.06F, 12, color);
        DrawCylinder({0.0F, 0.0F, 0.0F}, 0.08F, 0.08F, 0.08F, 12,
                     {24, 28, 31, 255});
        DrawCube({0.18F, 0.0F, 0.0F}, 0.28F, 0.06F, 0.06F, color);
    } else if (effect == LootUpgradeEffect::Map) {
        DrawCube({}, 0.46F, 0.28F, 0.08F, color);
        DrawLine3D({-0.08F, -0.14F, -0.05F},
                   {-0.08F, 0.14F, -0.05F}, WHITE);
    } else if (effect == LootUpgradeEffect::Anvil) {
        DrawCube({0.0F, 0.08F, 0.0F}, 0.48F, 0.16F, 0.20F, color);
        DrawCube({0.0F, -0.12F, 0.0F}, 0.20F, 0.28F, 0.16F, color);
    } else if (effect == LootUpgradeEffect::Saw) {
        DrawCube({}, 0.50F, 0.08F, 0.10F, color);
        for (int tooth = 0; tooth < 4; ++tooth) {
            DrawCube({-0.15F + tooth * 0.10F, -0.08F, 0.0F},
                     0.06F, 0.10F, 0.10F, color);
        }
    } else if (effect == LootUpgradeEffect::Potion) {
        DrawCylinder({0.0F, -0.08F, 0.0F}, 0.14F, 0.18F, 0.30F, 10,
                     color);
        DrawCylinder({0.0F, 0.14F, 0.0F}, 0.08F, 0.08F, 0.10F, 8,
                     color);
    } else {
        DrawSphereEx({-0.11F, 0.08F, 0.0F}, 0.18F, 6, 6, color);
        DrawSphereEx({0.11F, 0.08F, 0.0F}, 0.18F, 6, 6, color);
        rlRotatef(45.0F, 0.0F, 0.0F, 1.0F);
        DrawCube({0.0F, -0.10F, 0.0F}, 0.25F, 0.25F, 0.25F, color);
    }
}

ModelResource* lootItemModelFor(
    GraphicsResources& resources, LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Apple:
        return &resources.appleLootModel();
    case LootUpgradeEffect::Bread:
        return &resources.breadLootModel();
    case LootUpgradeEffect::IronBar:
        return &resources.ironBarLootModel();
    case LootUpgradeEffect::FuelJerrycan:
        return &resources.fuelJerrycanLootModel();
    case LootUpgradeEffect::Compass:
        return &resources.compassLootModel();
    case LootUpgradeEffect::Nail:
        return &resources.nailLootModel();
    case LootUpgradeEffect::Key:
        return &resources.keyLootModel();
    case LootUpgradeEffect::Map:
        return &resources.mapLootModel();
    case LootUpgradeEffect::Anvil:
        return &resources.anvilLootModel();
    case LootUpgradeEffect::Saw:
        return &resources.sawLootModel();
    case LootUpgradeEffect::Potion:
        return &resources.potionLootModel();
    case LootUpgradeEffect::Blueprint:
        return &resources.blueprintLootModel();
    case LootUpgradeEffect::Hourglass:
        return &resources.hourglassLootModel();
    case LootUpgradeEffect::Rope:
        return &resources.ropeLootModel();
    case LootUpgradeEffect::Magnet:
        return &resources.magnetLootModel();
    case LootUpgradeEffect::HealthAid:
        return &resources.healthAidLootModel();
    case LootUpgradeEffect::Damage:
    case LootUpgradeEffect::MoveSpeed:
    case LootUpgradeEffect::MaximumHealth:
        return nullptr;
    }
    return nullptr;
}

constexpr float CommonLootModelScale = 0.36F;

void drawAuthoredLootModel(ModelResource& resource, Color tint) {
    Model& model = resource.get();
    rlPushMatrix();
    rlScalef(
        CommonLootModelScale,
        CommonLootModelScale,
        CommonLootModelScale);
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(
            original, tint);
        DrawMesh(model.meshes[meshIndex], material, model.transform);
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    rlPopMatrix();
}

struct LootModelFit {
    Vector3 center{};
    float scale{1.0F};
};

LootModelFit lootModelFit(const ModelResource& resource) {
    const BoundingBox bounds = resource.visualBounds();
    const Vector3 extent{
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z,
    };
    const float maximumExtent = std::max({extent.x, extent.y, extent.z});
    constexpr float TargetMaximumExtent = 0.46F;
    return {
        .center = {
            (bounds.min.x + bounds.max.x) * 0.5F,
            (bounds.min.y + bounds.max.y) * 0.5F,
            (bounds.min.z + bounds.max.z) * 0.5F,
        },
        .scale = maximumExtent > 0.0001F
            ? TargetMaximumExtent / maximumExtent
            : 1.0F,
    };
}

void drawFittedLootModel(ModelResource& resource, Color tint) {
    Model& model = resource.get();
    const LootModelFit fit = lootModelFit(resource);
    rlPushMatrix();
    rlScalef(fit.scale, fit.scale, fit.scale);
    rlTranslatef(-fit.center.x, -fit.center.y, -fit.center.z);
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(
            original, tint);
        DrawMesh(model.meshes[meshIndex], material, model.transform);
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    rlPopMatrix();
}


} // namespace

bool Renderer::drawLootChest(
    LootChestType type, Vector3 position, float yawRadians,
    float openingProgress, Color tint, float scale) {
    ModelResource& resource = type == LootChestType::Wooden
        ? resources_.woodenChestModel()
        : resources_.stoneChestModel();
    if (!resource.valid()) return false;
    Model& model = resource.get();
    prepareModelShader(model);

    const LootChestWorldTransform worldTransform =
        lootChestWorldTransform(
            type, position, yawRadians, openingProgress, scale);
    if (!worldTransform.valid) return false;
    const auto drawMesh = [&resource, &model, tint](
                              int meshIndex, Matrix transform) {
        if (meshIndex < 0 || meshIndex >= model.meshCount ||
            !resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            return;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            return;
        }
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    if (worldTransform.hasLid) {
        drawMesh(1, worldTransform.baseTransform);
        drawMesh(0, worldTransform.lidTransform);
        for (int index = 2; index < model.meshCount; ++index) {
            drawMesh(index, worldTransform.baseTransform);
        }
    } else {
        for (int index = 0; index < model.meshCount; ++index) {
            drawMesh(index, worldTransform.baseTransform);
        }
    }
    return true;
}

LootChestWorldTransform Renderer::lootChestWorldTransform(
    LootChestType type, Vector3 position, float yawRadians,
    float openingProgress, float visualScale) {
    LootChestWorldTransform result{};
    ModelResource& resource = type == LootChestType::Wooden
        ? resources_.woodenChestModel()
        : resources_.stoneChestModel();
    if (!resource.valid()) return result;
    Model& model = resource.get();
    if (model.meshCount <= 0 || model.meshes == nullptr ||
        model.meshMaterial == nullptr || model.materials == nullptr) {
        return result;
    }

    constexpr float ModelScale = 2.45F;
    const float safeScale = std::clamp(visualScale, 0.0F, 4.0F);
    const float progress = std::clamp(openingProgress, 0.0F, 1.0F);
    const float delayed = std::clamp(
        (progress - 0.08F) / 0.76F, 0.0F, 1.0F);
    constexpr float Back = 1.70158F;
    const float shifted = delayed - 1.0F;
    const float eased = 1.0F + (Back + 1.0F) * shifted * shifted * shifted +
        Back * shifted * shifted;
    const float lidAngle = std::clamp(eased, 0.0F, 1.08F) *
        -108.0F * DEG2RAD;
    const Matrix scale = MatrixScale(
        ModelScale*safeScale,
        ModelScale*safeScale,
        ModelScale*safeScale);
    const Matrix terrainRotation = terrainAlignedRotation(
        position.x, position.z, yawRadians);
    const Matrix translation = MatrixTranslate(
        position.x, position.y, position.z);
    result.baseTransform = MatrixMultiply(
        MatrixMultiply(scale, terrainRotation), translation);
    result.lidTransform = result.baseTransform;
    result.hasLid = model.meshCount >= 2 &&
        resource.meshValid(0U) && resource.meshValid(1U);
    if (result.hasLid) {
        // raylib bakes glTF node translation into lid vertices. Rotate those
        // baked vertices around authored lid-node origin. A bounding-box
        // edge is not the hinge and shifts the lid while it opens.
        const Vector3 pivot = type == LootChestType::Wooden
            ? Vector3{0.0F, 0.22719747F, -0.21279876F}
            : Vector3{0.0F, 0.2F, -0.2F};
        const Matrix lidRotation = MatrixRotateX(lidAngle);
        const Matrix toPivot = MatrixTranslate(
            -pivot.x, -pivot.y, -pivot.z);
        const Matrix fromPivot = MatrixTranslate(
            pivot.x, pivot.y, pivot.z);
        result.lidTransform = MatrixMultiply(
            MatrixMultiply(
                MatrixMultiply(toPivot, lidRotation), fromPivot),
            result.baseTransform);
    }

    bool initialized = false;
    const auto addMeshBounds = [&](int meshIndex, Matrix transform) {
        if (meshIndex < 0 || meshIndex >= model.meshCount ||
            !resource.meshValid(static_cast<std::size_t>(meshIndex))) {
            return;
        }
        const auto bounds = resource.meshBounds();
        if (static_cast<std::size_t>(meshIndex) >= bounds.size()) return;
        world_transforms::expandBounds(
            result.worldBounds,
            world_transforms::transformBounds(
                bounds[static_cast<std::size_t>(meshIndex)],
                MatrixMultiply(model.transform, transform)),
            initialized);
    };
    if (result.hasLid) {
        addMeshBounds(0, result.lidTransform);
        addMeshBounds(1, result.baseTransform);
        for (int index = 2; index < model.meshCount; ++index) {
            addMeshBounds(index, result.baseTransform);
        }
    } else {
        for (int index = 0; index < model.meshCount; ++index) {
            addMeshBounds(index, result.baseTransform);
        }
    }
    result.valid = initialized &&
        world_transforms::finite(result.worldBounds);
    return result;
}

void Renderer::drawLootItem(
    Vector3 position, LootUpgradeEffect effect,
    LootRarity rarity, float rotationRadians, Color tint,
    float scale, Vector3 surfaceNormal) {
    // Static chest rewards are solid props. Drawing them through raylib's
    // default alpha blend can mix the authored material with the chest and
    // scenery behind it, making the whole item look translucent. Keep alpha
    // only for the reroll transition and the hourglass's intentional glass.
    const bool opaquePass = tint.a == 255 &&
        effect != LootUpgradeEffect::Hourglass &&
        !selectionMaskPassOpen_ && !shadowPassOpen_;
    if (opaquePass) {
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        rlDisableColorBlend();
    }
    const Color color = ColorTint(
        ColorBrightness(lootRarityColor(rarity), 0.22F), tint);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlMultMatrixf(MatrixToFloat(world_transforms::surfaceRotation(
        surfaceNormal, rotationRadians)));
    rlScalef(scale, scale, scale);
    ModelResource* resource = lootItemModelFor(resources_, effect);
    if (resource != nullptr && resource->valid()) {
        Model& model = resource->get();
        Shader shader{
            .id = rlGetShaderIdDefault(),
            .locs = rlGetShaderLocsDefault(),
        };
        if (selectionMaskPassOpen_ &&
            resources_.selectionMaskShader().valid()) {
            shader = resources_.selectionMaskShader().get();
        } else if (shadowPassOpen_ && resources_.shadowShader().valid()) {
            shader = resources_.shadowShader().get();
        } else if (worldShaderActive_ && resources_.worldShader().valid()) {
            shader = resources_.worldShader().get();
        }
        if (model.materials == nullptr || model.meshMaterial == nullptr) {
            rlPopMatrix();
            if (opaquePass) {
                rlDrawRenderBatchActive();
                rlEnableColorBlend();
            }
            return;
        }
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = shader;
        }
        // Keep the authored food colors; rarity belongs to the silhouette,
        // not to a cyan tint over the item itself.
        drawAuthoredLootModel(*resource, ColorTint(WHITE, tint));
    } else {
        drawLootItemGeometry(effect, color);
    }
    rlPopMatrix();
    if (opaquePass) {
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
    }
}

void Renderer::drawCoin(
    CoinType type, Vector3 position, float rotationRadians, float scale) {
    if (scale <= 0.0F) return;
    // A bevelled, minted coin instead of tinting the authored red texture.
    // Both faces carry the same relief so rotation never reveals a blank back.
    constexpr std::array<Vector2, 16> Ring{{
        {1, 0}, {.92388F, .38268F}, {.70711F, .70711F}, {.38268F, .92388F},
        {0, 1}, {-.38268F, .92388F}, {-.70711F, .70711F}, {-.92388F, .38268F},
        {-1, 0}, {-.92388F, -.38268F}, {-.70711F, -.70711F}, {-.38268F, -.92388F},
        {0, -1}, {.38268F, -.92388F}, {.70711F, -.70711F}, {.92388F, -.38268F},
    }};
    const bool silver = type == CoinType::Silver;
    const bool valuable = type == CoinType::HighValue;
    scale *= valuable ? 1.20F : silver ? 1.10F : 1.0F;
    const Color face = silver ? Color{175, 210, 233, 255} :
        valuable ? Color{255, 211, 49, 255} : Color{255, 194, 65, 255};
    const Color rim = silver ? Color{227, 247, 255, 255} :
        valuable ? Color{255, 241, 125, 255} : Color{255, 225, 103, 255};
    const Color edge = silver ? Color{84, 117, 147, 255} : Color{181, 94, 22, 255};
    const Color shine = silver ? Color{250, 255, 255, 255} : Color{255, 250, 207, 255};
    const auto tint = [](Color a, Color b, float amount) {
        const auto mix = [amount](unsigned char first, unsigned char second) {
            return static_cast<unsigned char>(std::lround(
                float(first) + (float(second) - float(first)) * amount));
        };
        return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 255};
    };
    const auto vertex = [](Vector3 point, Color color, Vector3 normal) {
        rlNormal3f(normal.x, normal.y, normal.z);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(point.x, point.y, point.z);
    };
    const auto triangle = [&vertex](Vector3 a, Vector3 b, Vector3 c,
                                   Color ca, Color cb, Color cc) {
        const Vector3 normal = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(b, a), Vector3Subtract(c, a)));
        vertex(a, ca, normal); vertex(b, cb, normal); vertex(c, cc, normal);
    };
    const auto point = [](Vector2 p, float radius, float z) {
        return Vector3{p.x * radius, p.y * radius, z};
    };
    const auto painted = [&tint, edge, shine](Color color, Vector2 p) {
        return p.y > 0 ? tint(color, shine, p.y * .20F)
                       : tint(color, edge, -p.y * .25F);
    };
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(rotationRadians * RAD2DEG, 0, 1, 0);
    rlRotatef(8.0F, 0, 0, 1);
    rlScalef(scale, scale, scale);
    rlBegin(RL_TRIANGLES);
    for (std::size_t i = 0; i < Ring.size(); ++i) {
        const Vector2 a = Ring[i], b = Ring[(i + 1U) % Ring.size()];
        const Vector3 frontA = point(a, .215F, .025F), frontB = point(b, .215F, .025F);
        const Vector3 backA = point(a, .215F, -.025F), backB = point(b, .215F, -.025F);
        const Vector3 na{a.x, a.y, 0}, nb{b.x, b.y, 0};
        const Color ea = painted(edge, a), eb = painted(edge, b);
        // Shared radial normals keep the barrel smooth while the face is flat.
        vertex(frontA, ea, na); vertex(backA, ea, na); vertex(frontB, eb, nb);
        vertex(frontB, eb, nb); vertex(backA, ea, na); vertex(backB, eb, nb);
        for (const float side : {1.0F, -1.0F}) {
            const Vector3 outerA = point(a, .215F, side * .025F);
            const Vector3 outerB = point(b, .215F, side * .025F);
            const Vector3 bevelA = point(a, .19F, side * .044F);
            const Vector3 bevelB = point(b, .19F, side * .044F);
            const Vector3 innerA = point(a, .161F, side * .044F);
            const Vector3 innerB = point(b, .161F, side * .044F);
            const Color ra = painted(rim, a), rb = painted(rim, b);
            const Color fa = painted(face, a), fb = painted(face, b);
            const auto faceTriangle = [&](Vector3 p, Vector3 q, Vector3 r,
                                          Color cp, Color cq, Color cr) {
                if (side > 0) triangle(p, q, r, cp, cq, cr);
                else triangle(p, r, q, cp, cr, cq);
            };
            faceTriangle(outerA, outerB, bevelA, ea, eb, ra);
            faceTriangle(outerB, bevelB, bevelA, eb, rb, ra);
            faceTriangle(bevelA, bevelB, innerA, ra, rb, ra);
            faceTriangle(bevelB, innerB, innerA, rb, rb, ra);
            faceTriangle({0, 0, side * .044F}, innerA, innerB, face, fa, fb);
        }
    }
    // Raised four-point mint mark and a small off-centre glossy stroke.
    constexpr std::array<Vector2, 8> Stamp{{
        {0, .080F}, {-.023F, .023F}, {-.062F, 0}, {-.023F, -.023F},
        {0, -.080F}, {.023F, -.023F}, {.062F, 0}, {.023F, .023F},
    }};
    for (const float side : {1.0F, -1.0F}) {
        const auto faceTriangle = [&](Vector3 a, Vector3 b, Vector3 c,
                                      Color ca, Color cb, Color cc) {
            if (side > 0) triangle(a, b, c, ca, cb, cc);
            else triangle(a, c, b, ca, cc, cb);
        };
        for (std::size_t i = 0; i < Stamp.size(); ++i) {
            const Vector2 a = Stamp[i], b = Stamp[(i + 1U) % Stamp.size()];
            const Vector3 raisedA = point(a, 1, side * .050F);
            const Vector3 raisedB = point(b, 1, side * .050F);
            const Vector3 baseA = point(a, 1.15F, side * .045F);
            const Vector3 baseB = point(b, 1.15F, side * .045F);
            faceTriangle(baseA, baseB, raisedA, edge, edge, rim);
            faceTriangle(baseB, raisedB, raisedA, edge, rim, rim);
            faceTriangle({0, 0, side * .056F}, raisedA, raisedB,
                         shine, painted(rim, a), painted(rim, b));
        }
        faceTriangle({-.132F, .048F, side * .047F}, {-.120F, .056F, side * .047F},
                     {-.069F, .127F, side * .047F}, shine, shine, shine);
        faceTriangle({-.132F, .048F, side * .047F}, {-.069F, .127F, side * .047F},
                     {-.081F, .131F, side * .047F}, shine, shine, shine);
    }
    rlEnd();
    rlPopMatrix();
}


void Renderer::drawSawBladeProjectile(
    Vector3 position, Vector3 direction,
    float spinDegrees, float scale, Color tint) {
    ModelResource& resource = resources_.sawBladeModel();
    const float horizontalLength = std::sqrt(
        direction.x * direction.x + direction.z * direction.z);
    const float headingDegrees = horizontalLength > 0.0001F
        ? std::atan2(direction.x, direction.z) * RAD2DEG
        : 0.0F;

    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    // The authored blade lies in its local XY plane. Keep that plane along
    // the flight path, then roll it around its axle like a thrown saw.
    rlRotatef(headingDegrees + 90.0F, 0.0F, 1.0F, 0.0F);
    rlRotatef(spinDegrees, 0.0F, 0.0F, 1.0F);
    rlScalef(scale * 1.55F, scale * 1.55F, scale * 1.55F);
    if (resource.valid()) {
        Model& model = resource.get();
        Shader shader{
            .id = rlGetShaderIdDefault(),
            .locs = rlGetShaderLocsDefault(),
        };
        if (worldShaderActive_ && resources_.worldShader().valid()) {
            shader = resources_.worldShader().get();
        }
        if (model.materials != nullptr) {
            for (int index = 0; index < model.materialCount; ++index) {
                model.materials[index].shader = shader;
            }
        }
        drawFittedLootModel(resource, tint);
    } else {
        DrawCylinder({}, 0.28F, 0.28F, 0.055F, 16, tint);
    }
    rlPopMatrix();
}

void Renderer::drawExperienceOctahedron(
    Vector3 position, float rotationRadians, float scale, int experienceValue) {
    if (scale <= 0.0F) return;
    const auto style = experienceCrystalStyle(experienceValue);
    scale *= style.scale;
    // Keep the pickup API, but give the old eight-triangle diamond a chunky
    // gemstone body: broad faces, narrow bevels and two short pointed caps.
    // Opaque geometry and inset highlights share the regular world depth.
    constexpr std::array<Vector3, 8> Ring{{
        {1.0F, 0.0F, 0.42F}, {0.42F, 0.0F, 1.0F},
        {-0.42F, 0.0F, 1.0F}, {-1.0F, 0.0F, 0.42F},
        {-1.0F, 0.0F, -0.42F}, {-0.42F, 0.0F, -1.0F},
        {0.42F, 0.0F, -1.0F}, {1.0F, 0.0F, -0.42F},
    }};
    constexpr std::array<Color, 8> TurquoiseColors{{
        {32, 199, 206, 255}, {24, 180, 198, 255},
        {12, 147, 176, 255}, {27, 183, 196, 255},
        {44, 211, 208, 255}, {74, 226, 216, 255},
        {40, 198, 209, 255}, {100, 238, 223, 255},
    }};
    constexpr std::array<Color, 8> BlueColors{{
        {37, 143, 247, 255}, {28, 116, 226, 255},
        {24, 86, 196, 255}, {35, 128, 237, 255},
        {55, 165, 255, 255}, {86, 190, 255, 255},
        {41, 142, 246, 255}, {121, 212, 255, 255},
    }};
    constexpr std::array<Color, 8> GoldColors{{
        {255, 181, 35, 255}, {244, 148, 24, 255},
        {206, 105, 20, 255}, {248, 163, 29, 255},
        {255, 202, 54, 255}, {255, 221, 88, 255},
        {251, 179, 33, 255}, {255, 234, 128, 255},
    }};
    const bool gold = style.tier == ExperienceCrystalTier::Gold;
    const auto& faceColors = gold ? GoldColors :
        style.tier == ExperienceCrystalTier::Blue ? BlueColors : TurquoiseColors;
    const Color highlight = gold ? Color{255, 249, 211, 255} :
        style.tier == ExperienceCrystalTier::Blue ? Color{209, 241, 255, 255} :
        Color{205, 255, 239, 255};
    const Color shadow = gold ? Color{139, 60, 16, 255} :
        style.tier == ExperienceCrystalTier::Blue ? Color{14, 40, 127, 255} :
        Color{6, 85, 128, 255};
    const Color tip = gold ? Color{255, 253, 225, 255} :
        style.tier == ExperienceCrystalTier::Blue ? Color{227, 248, 255, 255} :
        Color{224, 255, 243, 255};
    const auto tint = [](Color color, Color target, float amount) {
        const auto mix = [amount](unsigned char a, unsigned char b) {
            return static_cast<unsigned char>(std::lround(
                float(a) + (float(b) - float(a)) * amount));
        };
        return Color{mix(color.r, target.r), mix(color.g, target.g),
                     mix(color.b, target.b), 255};
    };
    const auto vertex = [](Vector3 point, Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(point.x, point.y, point.z);
    };
    const auto triangle = [&vertex](Vector3 a, Vector3 b, Vector3 c,
                                   Color ca, Color cb, Color cc) {
        const Vector3 normal = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(b, a), Vector3Subtract(c, a)));
        rlNormal3f(normal.x, normal.y, normal.z);
        vertex(a, ca); vertex(b, cb); vertex(c, cc);
    };
    const auto ringPoint = [](Vector3 point, float radius, float height) {
        return Vector3{point.x * radius, height, point.z * radius};
    };
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(rotationRadians * RAD2DEG, 0.0F, 1.0F, 0.0F);
    rlRotatef(9.0F, 0.0F, 0.0F, 1.0F);
    rlScalef(scale, scale, scale);
    rlBegin(RL_TRIANGLES);
    for (std::size_t side = 0; side < Ring.size(); ++side) {
        const auto next = (side + 1U) % Ring.size();
        const Vector3 a = ringPoint(Ring[side], 0.18F, 0.10F);
        const Vector3 b = ringPoint(Ring[next], 0.18F, 0.10F);
        const Vector3 c = ringPoint(Ring[next], 0.16F, -0.105F);
        const Vector3 d = ringPoint(Ring[side], 0.16F, -0.105F);
        const Color face = faceColors[side];
        const Color upper = tint(face, highlight, 0.36F);
        const Color lower = tint(face, shadow, 0.34F);
        triangle({0, 0.265F, 0}, b, a,
                 tip, upper, upper);
        triangle(a, b, d, upper, upper, face);
        triangle(b, c, d, upper, face, face);
        triangle({0, -0.25F, 0}, d, c, lower, face, face);

        // Small glossy lozenges sit on alternating broad faces, never a
        // billboard. They remain attached when the crystal turns or flies.
        if (side == 1U || side == 3U || side == 5U || side == 7U) {
            const Vector3 outward = Vector3Scale(Vector3Normalize(
                Vector3{a.x + b.x, 0, a.z + b.z}), 0.0015F);
            const auto surface = [&](float u, float v) {
                return Vector3Add(Vector3Lerp(
                    Vector3Lerp(a, b, u), Vector3Lerp(d, c, u), v),
                    outward);
            };
            const Vector3 top = surface(0.28F, 0.08F);
            const Vector3 right = surface(0.41F, 0.19F);
            const Vector3 bottom = surface(0.33F, 0.48F);
            const Vector3 left = surface(0.19F, 0.36F);
            const Color Shine = tip;
            triangle(top, right, left, Shine, Shine, Shine);
            triangle(right, bottom, left, Shine, Shine, Shine);
        }
    }
    rlEnd();
    rlPopMatrix();
}

void Renderer::drawHeart(
    Vector3 position, float rotationRadians, float scale) {
    ModelResource& resource = resources_.heartLootModel();
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(rotationRadians * RAD2DEG, 0.0F, 1.0F, 0.0F);
    rlRotatef(8.0F, 0.0F, 0.0F, 1.0F);
    constexpr float PickupScale = 0.98F;
    rlScalef(scale * 0.86F * PickupScale,
             scale * 0.86F * PickupScale,
             scale * 0.86F * PickupScale);
    if (resource.valid()) {
        Model& model = resource.get();
        Shader shader{
            .id = rlGetShaderIdDefault(),
            .locs = rlGetShaderLocsDefault(),
        };
        if (worldShaderActive_ && resources_.worldShader().valid()) {
            shader = resources_.worldShader().get();
        }
        if (model.materials != nullptr) {
            if (resources_.heartOutlineShader().valid()) {
                const Shader outlineShader =
                    resources_.heartOutlineShader().get();
                for (int index = 0; index < model.materialCount; ++index) {
                    model.materials[index].shader = outlineShader;
                }
                rlDrawRenderBatchActive();
                rlEnableBackfaceCulling();
                rlSetCullFace(RL_CULL_FACE_FRONT);
                drawFittedLootModel(resource, WHITE);
                rlDrawRenderBatchActive();
                rlSetCullFace(RL_CULL_FACE_BACK);
            }
            for (int index = 0; index < model.materialCount; ++index) {
                model.materials[index].shader = shader;
            }
        }
        drawFittedLootModel(resource, {255, 105, 112, 255});
    } else {
        DrawSphereEx({-0.08F, 0.05F, 0.0F}, 0.13F, 8, 8,
                     {235, 62, 72, 255});
        DrawSphereEx({0.08F, 0.05F, 0.0F}, 0.13F, 8, 8,
                     {235, 62, 72, 255});
    }
    rlPopMatrix();
}

BoundingBox Renderer::lootItemWorldBounds(
    Vector3 position, LootUpgradeEffect effect,
    float rotationRadians, float scale, Vector3 surfaceNormal) {
    ModelResource* resource = lootItemModelFor(resources_, effect);
    if (resource == nullptr || !resource->valid()) {
        const float radius = std::max(0.05F, scale * 0.32F);
        return {{position.x - radius, position.y - radius,
                 position.z - radius},
                {position.x + radius, position.y + radius,
                 position.z + radius}};
    }
    const Matrix transform = MatrixMultiply(
        resource->get().transform,
        MatrixMultiply(
            MatrixScale(
                CommonLootModelScale * scale,
                CommonLootModelScale * scale,
                CommonLootModelScale * scale),
            MatrixMultiply(
                world_transforms::surfaceRotation(
                    surfaceNormal, rotationRadians),
                MatrixTranslate(
                    position.x, position.y, position.z))));
    return world_transforms::transformBounds(
        resource->visualBounds(), transform);
}


} // namespace ian
