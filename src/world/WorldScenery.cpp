#include "world/WorldScenery.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr double SiteRadius = 10.0;
struct Piece { WorldSceneryModel model; double x, z, yaw; };
using Model = WorldSceneryModel;
// Each composition has two open sides and gaps of at least three meters.
// Keep closed gates out: scenery must not seal off a combat or loot area.
constexpr std::array<Piece, 6> Ruins{{
    {Model::RuinedHouse, -3,-2,0}, {Model::StoneFence,6,-2,0},
    {Model::StoneFence,6,3,0}, {Model::StoneFence,-1,6,Pi/2},
    {Model::StoneFence,4,6,Pi/2},
    {Model::Wheelbarrow,-4,3,Pi/2},
}};
constexpr std::array<Piece, 6> Camp{{
    {Model::Tent,-3,-2,0}, {Model::WoodFence,6,-2,0},
    {Model::WoodFence,6,2,0}, {Model::WoodFence,0,6,Pi/2},
    {Model::LumberStack,2,0,0}, {Model::Wheelbarrow,-4,3,Pi/2},
}};
constexpr std::array<Piece, 5> Outpost{{
    {Model::StoneWall,-3.2,-5,0}, {Model::StoneWall,3.2,-5,0},
    {Model::StoneWall,6,-.5,Pi/2}, {Model::RuinedFrame,-3,2,0},
    {Model::Well,2,3,0},
}};
constexpr std::array<Piece, 5> Courtyard{{
    {Model::RuinedFrame,-3,-3,0}, {Model::Well,3,-1,0},
    {Model::StoneFence,6,3,0}, {Model::StoneFence,0,6,Pi/2},
    {Model::LumberStack,-3,2,Pi/2},
}};
double distanceSquared(double x, double z, const CollisionBox& box) {
    const double dx = std::max({box.minX-x, 0.0, x-box.maxX});
    const double dz = std::max({box.minZ-z, 0.0, z-box.maxZ});
    return dx*dx + dz*dz;
}
bool suitableSurface(const TerrainHeightfield& terrain, Vec3 center,
                     double halfX, double halfZ, double tolerance) {
    double low = std::numeric_limits<double>::infinity();
    double high = -low;
    for (int x=-1;x<=1;++x) for (int z=-1;z<=1;++z) {
        const double px=center.x+x*halfX, pz=center.z+z*halfZ;
        if (!terrain.isInside(px,pz) || terrain.waterSignedDistance(px,pz)<2.0 ||
            terrain.getNormal(px,pz).y<.94) return false;
        const double y=terrain.getHeight(px,pz);
        low=std::min(low,y); high=std::max(high,y);
    }
    return high-low<=tolerance;
}
} // namespace

CollisionBox worldSceneryCollisionBox(const WorldSceneryInstance& instance) {
    const auto& definition=WorldSceneryDefinitions[static_cast<std::size_t>(instance.model)];
    const double c=std::abs(std::cos(instance.yaw)), s=std::abs(std::sin(instance.yaw));
    const double halfX=(c*definition.width+s*definition.depth)*.5;
    const double halfZ=(s*definition.width+c*definition.depth)*.5;
    return {instance.position.x-halfX,instance.position.x+halfX,
            instance.position.z-halfZ,instance.position.z+halfZ,
            instance.position.y+definition.height+1.65,instance.position.y-.2};
}

std::vector<CollisionBox> worldScenerySolidBoxes(
    const WorldSceneryInstance& instance) {
    if (instance.model != WorldSceneryModel::Tent)
        return {worldSceneryCollisionBox(instance)};
    // This asset is an open canopy. Its footprint reserves placement, but
    // only the four posts obstruct movement beneath the roof.
    std::vector<CollisionBox> posts;
    const double c = std::cos(instance.yaw), s = std::sin(instance.yaw);
    for (double x : {-1.01, 1.01}) {
        for (double z : {-1.01, 1.01}) {
            const double px = instance.position.x + c*x + s*z;
            const double pz = instance.position.z - s*x + c*z;
            posts.push_back({px-.14, px+.14, pz-.14, pz+.14,
                            instance.position.y+3.95, instance.position.y-.2});
        }
    }
    return posts;
}

std::vector<WorldSceneryInstance> generateWorldScenery(
    const TerrainHeightfield& terrain, double worldLimit, Vec3 spawn,
    std::span<const ResourceNode> resources, std::span<const CollisionBox> obstacles,
    std::span<const WorldSceneryExclusion> exclusions) {
    std::vector<WorldSceneryInstance> result;
    if (worldLimit<58.0) return result;
    constexpr std::size_t SiteCount=12, Attempts=2048;
    result.reserve(SiteCount*7);
    std::vector<Vec3> centers;
    std::vector<WorldSceneryInstance> candidate;
    candidate.reserve(7);
    const double maximumRadius=std::min(worldLimit-16.0,
        terrain.config().terrainWorldSize*.5-20.0);
    if (maximumRadius<=42.0) return result;
    for (std::size_t site=0;site<SiteCount;++site) {
        const std::span<const Piece> pieces=site%4==0 ? std::span<const Piece>{Ruins}
            : site%4==1 ? std::span<const Piece>{Camp}
            : site%4==2 ? std::span<const Piece>{Outpost} : std::span<const Piece>{Courtyard};
        for (std::size_t attempt=0;attempt<Attempts;++attempt) {
            const auto seed=mixBits64(static_cast<std::uint64_t>(terrain.seed())^
                0x6a09e667f3bcc909ULL^(site+1)*0xd1b54a32d192ed03ULL^
                (attempt+1)*0x9e3779b97f4a7c15ULL);
            const double angle=unitRandom(seed)*2*Pi;
            const double radius=std::sqrt(42.0*42.0+unitRandom(seed^0xa4093822299f31d0ULL)*
                (maximumRadius*maximumRadius-42.0*42.0));
            Vec3 center{spawn.x+std::cos(angle)*radius,0,spawn.z+std::sin(angle)*radius};
            if (std::abs(center.x)+SiteRadius>worldLimit-5 ||
                std::abs(center.z)+SiteRadius>worldLimit-5 ||
                !suitableSurface(terrain,center,8,8,.72)) continue;
            if (std::ranges::any_of(centers,[&](Vec3 other){
                    return std::hypot(center.x-other.x,center.z-other.z)<29.0;}) ||
                std::ranges::any_of(exclusions,[&](const auto& other){
                    return std::hypot(center.x-other.position.x,center.z-other.position.z)<
                        SiteRadius+other.radius;}) ||
                std::ranges::any_of(obstacles,[&](const auto& box){
                    return distanceSquared(center.x,center.z,box)<SiteRadius*SiteRadius;})) continue;
            const double yaw=static_cast<double>((seed>>8U)%4U)*Pi*.5;
            const double c=std::cos(yaw), s=std::sin(yaw);
            candidate.clear();
            bool valid=true;
            for (const Piece& piece:pieces) {
                WorldSceneryInstance instance{
                    .id={static_cast<std::uint32_t>(930000+result.size()+candidate.size()),1},
                    .model=piece.model,
                    .position={center.x+c*piece.x+s*piece.z,0,center.z-s*piece.x+c*piece.z},
                    .yaw=yaw+piece.yaw,.site=site};
                instance.position.y=terrain.getHeight(instance.position.x,instance.position.z)-.06;
                const CollisionBox box=worldSceneryCollisionBox(instance);
                if (!suitableSurface(terrain,instance.position,
                        (box.maxX-box.minX)*.5,(box.maxZ-box.minZ)*.5,.28) ||
                    std::ranges::any_of(resources,[&](const ResourceNode& node){
                        const double clearance=std::max(.5,node.radius*node.visualScale)+.65;
                        return distanceSquared(node.position.x,node.position.z,box)<clearance*clearance;
                    })) { valid=false; break; }
                candidate.push_back(instance);
            }
            if (!valid) continue;
            result.insert(result.end(),candidate.begin(),candidate.end());
            centers.push_back(center);
            break;
        }
    }
    return result;
}
} // namespace ian
