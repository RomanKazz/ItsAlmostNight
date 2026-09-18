#include "TestHarness.hpp"
#include "graphics/GlbNodeAnimation.hpp"

#include <cmath>

void runGlbNodeAnimationTests() {
    ian::GlbNodeAnimation original;
    ian::GlbNodeAnimation alternate;
    require(original.load(
                IAN_SOURCE_DIR "/assets/models/tools/pickaxe.glb",
                "swing pivot"),
            "original harvest swing must load");
    require(alternate.load(
                IAN_SOURCE_DIR "/assets/models/tools/pickaxe_anim2.glb",
                "swing pivot.001"),
            "alternate harvest swing must load");

    const Transform& base = original.baseTransform();
    const Transform& alternateBase = alternate.baseTransform();
    requireNear(base.translation.x, alternateBase.translation.x,
                0.0001, "harvest swing pivots must share x position");
    requireNear(base.translation.y, alternateBase.translation.y,
                0.0001, "harvest swing pivots must share y position");
    requireNear(base.translation.z, alternateBase.translation.z,
                0.0001, "harvest swing pivots must share z position");

    const Transform originalStrike = original.sample(20.0F / 60.0F);
    const Transform alternateStrike = alternate.sample(27.0F / 60.0F);
    require(std::abs(originalStrike.translation.z -
                     alternateStrike.translation.z) > 0.04F,
            "alternate swing must have a distinct strike pose");
    const Transform alternateEnd = alternate.sample(1.0F);
    requireNear(alternateEnd.translation.x, alternateBase.translation.x,
                0.0001, "alternate swing must return to its pivot x");
    requireNear(alternateEnd.translation.y, alternateBase.translation.y,
                0.0001, "alternate swing must return to its pivot y");
    requireNear(alternateEnd.translation.z, alternateBase.translation.z,
                0.0001, "alternate swing must return to its pivot z");
}
