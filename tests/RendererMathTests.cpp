#include "TestHarness.hpp"
#include "graphics/UniformValueCache.hpp"
#include "graphics/SurfaceEffectPlane.hpp"
#include "graphics/BuildingFoundationVisual.hpp"
#include "graphics/EnemyAnimationSampling.hpp"
#include "ui/BuildingCardLayout.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <limits>

void runRendererMathTests() {
    for(const Vector3 normal : {Vector3{0,1,0},Vector3{-.6F,.8F,0},Vector3{0,.8F,.6F},Vector3{0,0,0}}) {
        const ian::SurfaceEffectPlane plane{normal};
        requireNear(Vector3Length(plane.normal),1,1e-6,"surface normal is safe and normalized");
        requireNear(Vector3DotProduct(plane.right,plane.normal),0,1e-6,"ground-ring right axis lies in the surface");
        requireNear(Vector3DotProduct(plane.forward,plane.normal),0,1e-6,"ground-ring forward axis lies in the surface");
        const Vector3 center{3,4,5};
        for(int angle=0;angle<360;angle+=10) {
            const float radians=static_cast<float>(angle)*DEG2RAD;
            const auto p=plane.point(center,3*std::cos(radians),3*std::sin(radians));
            requireNear(Vector3DotProduct(Vector3Subtract(p,center),plane.normal),.045,1e-5,
                "explosion and boss rings follow the ground normal at every angle");
        }
    }
    {
        ian::TerrainHeightfield terrain;
        const auto normal=terrain.getNormal(15,15);
        const ian::SurfaceEffectPlane plane{{static_cast<float>(normal.x),static_cast<float>(normal.y),static_cast<float>(normal.z)},&terrain};
        const Vector3 center{15,static_cast<float>(terrain.getHeight(15,15)),15};
        for(int angle=0;angle<360;angle+=5) {
            const float a=static_cast<float>(angle)*DEG2RAD;
            const auto p=plane.point(center,8*std::cos(a),8*std::sin(a));
            require(p.y>=terrain.getHeight(p.x,p.z)+.044,"large rings stay above uneven terrain at their perimeter");
        }
    }
    const Camera3D cardCamera{{0, 1.7F, 0}, {0, 1.7F, -1}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
    const Vector2 cardViewport{1920, 1080};
    require(ian::projectBuildingCard({0, 1.15F, -3}, cardCamera, cardViewport).has_value(),
        "visible nearby building projects a context card");
    require(!ian::projectBuildingCard({0, 1.15F, 3}, cardCamera, cardViewport),
        "building behind the camera cannot leave a mirrored card pinned to the screen");
    require(!ian::projectBuildingCard({0, 1.7F, -0.02F}, cardCamera, cardViewport),
        "card crossing the camera plane cannot explode in size");
    require(!ian::projectBuildingCard({20, 1.15F, -3}, cardCamera, cardViewport),
        "offscreen building is hidden rather than clamped back into the viewport");
    auto turnedCamera = cardCamera;
    turnedCamera.target = {0.866F, 1.7F, -0.5F};
    require(!ian::projectBuildingCard({0, 1.15F, -3}, turnedCamera, cardViewport),
        "turning away invalidates an already open card while the target remains in range");
    require(!ian::projectBuildingCard({std::numeric_limits<float>::quiet_NaN(), 0, -3}, cardCamera, cardViewport),
        "invalid projection cannot reach card layout");
    require(ian::buildingCardInRange({}, {0, 0, -4}) &&
            !ian::buildingCardInRange({}, {0, 0, -5}) &&
            !ian::buildingCardInRange({}, {0, 5, 0}),
        "open cards respect interaction distance in every axis even while the cursor is active");
    for (const bool core : {false, true}) {
        const float width = core ? 920.0F : 560.0F;
        for (const bool left : {false, true}) {
            const ian::BuildingCardProjection projection{{left ? 1600.0F : 320.0F, 540}, 300};
            const auto open = ian::buildingCardTransform(projection, cardViewport, 1, 0, width, 630, 1, core)
                .rect(407, 284, width, 630);
            const auto closing = ian::buildingCardTransform(projection, cardViewport, .4F, 0, width, 630, 1, core)
                .rect(407, 284, width, 630);
            require(closing.width < open.width, "building card shrinks during closing");
            requireNear(left ? closing.x + closing.width : closing.x,
                left ? open.x + open.width : open.x, .001,
                "closing card keeps the edge nearest the building fixed on either side");
            require(left ? closing.x > open.x : closing.x + closing.width < open.x + open.width,
                "left card closes to the right and right card closes to the left");
        }
    }
    for (const Vector2 viewport : {Vector2{1920, 1080}, Vector2{1600, 1200},
            Vector2{2560, 1080}, Vector2{900, 1600}}) {
        for (const bool core : {false, true}) {
            for (const float pixelsPerUnit : {150.0F, 700.0F, 5000.0F}) {
                for (const float anchorX : {10.0F, viewport.x * 0.5F, viewport.x - 10.0F}) {
                    const float width = core ? 920.0F : 560.0F;
                    const float height = core ? 630.0F : 736.0F;
                    const auto transform = ian::buildingCardTransform(
                        {{anchorX, viewport.y * 0.5F}, pixelsPerUnit}, viewport,
                        1.13F, 0, width, height, 1, core);
                    const auto bounds = transform.rect(407, 284, width, height);
                    require(transform.scale <= 1.0F && bounds.width <= viewport.x * (core ? .60F : .42F) + .01F &&
                            bounds.height <= (viewport.y - 36) * .78F + .01F,
                        "core and defense cards stay bounded at close range including entrance overshoot");
                    require(bounds.x >= 17.99F && bounds.y >= 17.99F &&
                            bounds.x + bounds.width <= viewport.x - 17.99F &&
                            bounds.y + bounds.height <= viewport.y - 17.99F,
                        "building cards retain margins across narrow wide and tall viewports");
                }
            }
        }
    }
    const auto earlyPose = ian::sampleEnemyAnimation(10.2F / ian::EnemyAnimationFps, 61, true);
    const auto laterPose = ian::sampleEnemyAnimation(10.8F / ian::EnemyAnimationFps, 61, true);
    require(earlyPose.frame == laterPose.frame && earlyPose.nextFrame == 11,
        "nearby enemies share endpoint poses within the same source frame");
    require(laterPose.blend > earlyPose.blend,
        "each rendered frame advances the blend instead of holding a discrete crowd pose");
    const auto endPose = ian::sampleEnemyAnimation(60.5F / ian::EnemyAnimationFps, 61, true);
    require(endPose.frame == 60 && endPose.nextFrame == 0,
        "looping animation interpolates across its final frame");
    const auto stoppedPose = ian::sampleEnemyAnimation(100, 61, false);
    require(stoppedPose.frame == 60 && stoppedPose.nextFrame == 60 && stoppedPose.blend == 0,
        "one-shot animations stay at their final pose");
    require(ian::sampleEnemyAnimation(0, 61, true).blend == 0,
        "frozen enemies retain the initial pose");
    const auto coarsePose = ian::sampleEnemyAnimation(10.8F / ian::EnemyAnimationFps, 61, true, true);
    require(coarsePose.blend == 0 && coarsePose.frame == coarsePose.nextFrame,
        "distant crowds preserve their inexpensive discrete poses");
    const Matrix blended = ian::blendEnemyBoneMatrices(MatrixTranslate(0, 0, 0), MatrixTranslate(4, 2, 0), .25F);
    requireNear(blended.m12, 1, 1e-6, "selection pose uses the same linear skinning interpolation as the GPU");
    requireNear(blended.m13, .5, 1e-6, "skin blend preserves translation on all axes");
    // Compact buildings still require the complete 2x2 supporting platform.
    for (const auto position : {ian::GridPosition{0, 0}, ian::GridPosition{5, -3}, ian::GridPosition{-7, -4}}) {
        for (const auto type : {ian::BuildingType::SpikeTrap, ian::BuildingType::SlowTrap,
                ian::BuildingType::CrystalMine, ian::BuildingType::LumberMill,
                ian::BuildingType::Quarry, ian::BuildingType::Wall, ian::BuildingType::Gate}) {
            const auto frame = ian::makeBuildingFoundationVisual(type, position, 2.0, .5, 1.0);
            require(frame.has_value(), "raised compact buildings have a visible pallet");
            requireNear(frame->supports[3].top.x - frame->supports[0].top.x, 2.0, 1e-9,
                "a 1x1 trap retains its 2x2 supporting platform width");
            requireNear(frame->supports[3].top.z - frame->supports[0].top.z, 2.0, 1e-9,
                "a 1x1 trap retains its 2x2 supporting platform depth");
            const auto center = ian::platformFrameVisualCenter(*frame);
            requireNear(center.x, static_cast<double>(position.x) + 1.0, 1e-9,
                "the platform animation pivot uses the center of its 2x2 support X");
            requireNear(center.z, static_cast<double>(position.z) + 1.0, 1e-9,
                "the platform animation pivot uses the center of its 2x2 support Z");
            requireNear(frame->supports[0].bottom.y, .5, 1e-9, "support bottoms preserve the construction plane");
            requireNear(frame->supports[0].length, 1.5, 1e-9, "compact support height is preserved");
        }
        for (const auto type : {ian::BuildingType::Core, ian::BuildingType::GunTurret,
                ian::BuildingType::Turret, ian::BuildingType::Cannon,
                ian::BuildingType::Catapult, ian::BuildingType::WoodStorage,
                ian::BuildingType::StoneStorage, ian::BuildingType::CrystalStorage}) {
            const auto frame = ian::makeBuildingFoundationVisual(type, position, 2.0, .5, 1.0);
            require(frame.has_value(), "large buildings retain their visual foundation");
            requireNear(frame->supports[3].top.x - frame->supports[0].top.x, 2.0, 1e-9,
                "2x2 foundation width remains unchanged");
            requireNear(frame->supports[3].top.z - frame->supports[0].top.z, 2.0, 1e-9,
                "2x2 foundation depth remains unchanged");
            const auto center = ian::platformFrameVisualCenter(*frame);
            requireNear(center.x, static_cast<double>(position.x), 1e-9, "large foundations remain centered X");
            requireNear(center.z, static_cast<double>(position.z), 1e-9, "large foundations remain centered Z");
        }
    }
    require(!ian::makeBuildingFoundationVisual(ian::BuildingType::SpikeTrap, {}, 0, 0, 1),
        "no support geometry is drawn for a zero-height foundation");
    ian::BuildingPreview trapPreview{
        .type = ian::BuildingType::SpikeTrap, .gridPosition = {0, 0}, .rotation = 0,
        .placement = {}, .baseHeight = 2, .foundationBottomHeight = 2,
        .foundationPreview = ian::PlatformFramePlacement{
            .anchor = {-1, 2, -1}, .floorHeight = 2, .storey = 0,
            .supports = {{{{-1, 2, -1}, {-1, 2, -1}, 0},
                {{1, 2, -1}, {1, 2, -1}, 0}, {{-1, 2, 1}, {-1, 2, 1}, 0},
                {{1, 2, 1}, {1, 2, 1}, 0}}},
        },
    };
    const auto exactPlatform = ian::makeBuildingFoundationPreviewVisual(trapPreview, 1);
    require(exactPlatform && exactPlatform->anchor == ian::GridCoord{-1, 2, -1},
        "trap ghost uses the chosen alternative platform anchor rather than a guessed one");
    requireNear(exactPlatform->supports[3].top.x - exactPlatform->supports[0].top.x, 2, 1e-9,
        "platform deck remains visible at full 2x2 size even without extended legs");
    for (const auto error : {ian::ModularPlacementError::OutsideCoreArea,
             ian::ModularPlacementError::InsufficientResources, ian::ModularPlacementError::TooFar}) {
        trapPreview.foundationPreview->error = error;
        const auto invalidPlatform = ian::makeBuildingFoundationPreviewVisual(trapPreview, 1);
        require(invalidPlatform && invalidPlatform->floorHeight == trapPreview.baseHeight,
            "invalid compact-building ghosts retain the exact support height and geometry");
    }
    require(!ian::PlatformFramePlacement{}.hasGeometry(), "uncomputed supports never create a platform at origin");
    trapPreview.foundationPreview->existingFrame = ian::EntityId{12, 1};
    require(!ian::makeBuildingFoundationPreviewVisual(trapPreview, 1), "existing supports are not duplicated in the ghost pass");
    trapPreview.foundationPreview.reset(); trapPreview.platformStorey = 0;
    require(!ian::makeBuildingFoundationPreviewVisual(trapPreview, 1),
        "an existing real platform is not duplicated with an incorrectly offset ghost");
    ian::UniformValueCache cache;
    std::array<float, 1> zero{0.0F};
    require(cache.update(0, zero), "first zero uniform must upload");
    require(!cache.update(0, zero), "unchanged zero must skip");
    require(!cache.update(-1, zero), "inactive uniform must skip");
    std::array<float, 3> color{1.0F, 0.5F, 0.2F};
    require(cache.update(3, color), "first color must upload");
    require(!cache.update(3, color), "unchanged color must skip");
    auto variant = color;
    variant[1] = 0.8F;
    require(cache.update(3, variant), "temporary material must upload");
    require(cache.update(3, color), "restoring material must upload");
    require(!cache.update(3, color), "restored material must skip duplicates");
    require(cache.update(4, color), "locations must be independent");
    require(cache.update(4, zero), "uniform size changes must upload");
    cache.reset();
    require(cache.update(3, color), "relinked shader must upload again");
    cache.reset();
    int uploads = 0;
    for (int location = 0; location < 64; ++location) {
        uploads += cache.update(location, zero) ? 1 : 0;
    }
    require(uploads == 64, "first material must initialize every uniform");
    uploads = 0;
    const std::array<float, 1> flash{1.0F};
    for (int location = 0; location < 64; ++location) {
        uploads += cache.update(location, location == 17 ? flash : zero) ? 1 : 0;
    }
    require(uploads == 1, "one material change must upload only one uniform");

    // Compare the sparse shader equations with full raymath multiplication.
    // This also guards the symmetric-perspective assumption and matrix indexing.
    for (double fov : {25.0, 45.0, 90.0, 120.0}) {
        for (double aspect : {0.6, 1.0, 1.6, 2.4}) {
            for (double nearPlane : {0.01, 0.1, 1.0}) {
                for (double farPlane : {100.0, 1000.0, 10000.0}) {
                    const Matrix p = MatrixPerspective(fov*DEG2RAD, aspect, nearPlane, farPlane);
                    const Matrix inv = MatrixInvert(p);
                    for (float x : {-1.0F, -0.3F, 0.0F, 0.7F, 1.0F}) {
                        for (float y : {-1.0F, 0.0F, 1.0F}) {
                            for (float z : {-1.0F, -0.5F, 0.0F, 0.9F, 1.0F}) {
                                // Use double intermediates to test the algebra independently
                                // of compiler float FMA contraction near the far plane.
                                const double dx = x, dy = y, dz = z;
                                const double fullX = inv.m0*dx + inv.m4*dy + inv.m8*dz + inv.m12;
                                const double fullY = inv.m1*dx + inv.m5*dy + inv.m9*dz + inv.m13;
                                const double fullZ = inv.m2*dx + inv.m6*dy + inv.m10*dz + inv.m14;
                                const double fullW = inv.m3*dx + inv.m7*dy + inv.m11*dz + inv.m15;
                                const double w = inv.m11*dz + inv.m15;
                                const double denominator = std::max(w, 0.000001);
                                const std::array<double, 3> sparse{inv.m0*dx/denominator,
                                    inv.m5*dy/denominator, (inv.m10*dz + inv.m14)/denominator};
                                requireNear(sparse[0], fullX/std::max(fullW, 0.000001), 0.00001,
                                    "sparse inverse projection x must match");
                                requireNear(sparse[1], fullY/std::max(fullW, 0.000001), 0.00001,
                                    "sparse inverse projection y must match");
                                requireNear(sparse[2], fullZ/std::max(fullW, 0.000001), 0.00001,
                                    "sparse inverse projection z must match");
                                const double px = sparse[0], py = sparse[1], pz = sparse[2];
                                requireNear(p.m0*px, p.m0*px + p.m4*py + p.m8*pz + p.m12,
                                    0.00001, "sparse projection x must match");
                                requireNear(p.m5*py, p.m1*px + p.m5*py + p.m9*pz + p.m13,
                                    0.00001, "sparse projection y must match");
                                requireNear(p.m11*pz + p.m15,
                                    p.m3*px + p.m7*py + p.m11*pz + p.m15,
                                    0.00001, "sparse projection w must match");
                            }
                        }
                    }
                }
            }
        }
    }
}
