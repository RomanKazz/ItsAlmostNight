#pragma once

#include "ui/GameUi.hpp"
#include "ui/UiSolidGeometry.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ian::reward_backdrop {
enum class Kind { Coins, Experience };
inline float unit(int index,int channel) {
    std::uint32_t hash=static_cast<std::uint32_t>(index+7)*0x9e3779b9U ^ static_cast<std::uint32_t>(channel+3)*0x85ebca6bU;
    hash^=hash>>16U;hash*=0x7feb352dU;hash^=hash>>15U;hash*=0x846ca68bU;hash^=hash>>16U;
    return static_cast<float>(hash%65536U)/65536.0F;
}
inline void crystal(Vector2 center,float size,float tilt,Color tint) {
    const float c=std::cos(tilt),s=std::sin(tilt);
    const auto point=[&](float x,float y){return Vector2{center.x+(x*c-y*s)*size,center.y+(x*s+y*c)*size};};
    const auto triangle=[&](Vector2 a,Vector2 b,Vector2 d,Color color) {
        const std::array<Vector2,3> points{a,b,d};color.a=tint.a;drawSolidTriangleFan(points.data(),3,color);
    };
    const auto top=point(0,-.65F),right=point(.42F,-.27F),bottomRight=point(.42F,.3F),
        bottom=point(0,.65F),left=point(-.42F,-.27F),bottomLeft=point(-.42F,.3F),middle=point(.03F,0);
    triangle(top,left,middle,{162,255,255,255});triangle(top,middle,right,{98,247,255,255});
    triangle(left,bottomLeft,middle,{18,194,226,255});triangle(right,middle,bottomRight,{23,219,240,255});
    triangle(bottomLeft,bottom,middle,{0,155,203,255});triangle(middle,bottom,bottomRight,{9,198,227,255});
    triangle(point(-.17F,-.28F),point(-.13F,.04F),point(-.07F,-.27F),{238,255,255,255});
}
inline void draw(const GameUi& ui,Kind kind,double seconds,Vector2 viewport,float opacity,bool reduced=false) {
    const float scale=std::min(viewport.x/1920.0F,viewport.y/1080.0F);
    const double time=reduced?0:seconds;
    const auto texture=ui.resourceTexture(UiResourceIcon::Coin);
    for(int i=0;i<36;++i) {
        const float depth=.45F+.55F*unit(i,0);
        const float size=(24+28*depth)*scale;
        const double phase=time*(.50+depth*.55)+unit(i,3)*6.28;
        const float x=viewport.x*unit(i,1)+
            static_cast<float>(std::sin(phase*.62))*4*scale;
        const float travel=static_cast<float>(std::fmod(unit(i,2)+time*(.16+depth*.12),1.0));
        const float y=viewport.y*travel;
        const float edge=std::min(1.0F,std::min(travel,1-travel)*12);
        const auto alpha=static_cast<unsigned char>(std::clamp(opacity*edge*(.25F+.35F*depth),0.0F,1.0F)*255);
        if(kind==Kind::Coins && IsTextureValid(texture)) {
            const float face=std::max(.10F,std::abs(static_cast<float>(std::cos(phase))));
            DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
                {x,y,size*face,size},{size*face*.5F,size*.5F},static_cast<float>(std::sin(phase*.35))*18,{255,255,255,alpha});
        } else if(kind==Kind::Experience) {
            crystal({x,y},size,static_cast<float>(std::sin(phase))*.22F,{255,255,255,alpha});
        }
    }
}
} // namespace ian::reward_backdrop
