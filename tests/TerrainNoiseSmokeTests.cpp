#include "TestHarness.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <array>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <string>

namespace {
struct Probe {float x,y;unsigned int expected;};
// Frozen 24-bit results, including negative coordinates and distant cells.
constexpr std::array<Probe,18> Probes{{
    {0,0,0x8754e2U},{1,0,0xd8631bU},{0,1,0x81d22aU},{-1,0,0xb4479dU},
    {0,-1,0x51cfd1U},{-1,-1,0x780621U},{16,5,0xb7903dU},{17,6,0x514faeU},
    {31,-19,0xf01661U},{-15,27,0x15b69bU},{123,-77,0x9ce3a5U},
    {-512,511,0x38be54U},{1024,2048,0xd56359U},{-10000,9000,0x49f4f1U},
    {32767,-32768,0x5828d2U},{100000,-100000,0x02aab7U},
    {16.7F,5.2F,0xb7903dU},{-.1F,-.1F,0x780621U}}};

std::string hashFunction(const char* path,const std::string& name) {
    std::ifstream file(path);std::string source(std::istreambuf_iterator<char>(file),{});
    const auto start=source.find("float "+name+"(vec2 position)");
    require(start!=std::string::npos,"terrain shader exposes its noise hash");
    const auto end=source.find('}',start);
    require(end!=std::string::npos,"terrain noise function is complete");
    return source.substr(start,end-start+1);
}

void check(const char* path,const char* name,bool vertexStage) {
    const auto hash=hashFunction(path,name);
    const std::string encode=R"(
void main() {
    uint value=uint(PROBE*16777216.0);
    finalColor=vec4(float((value>>16u)&255u),float((value>>8u)&255u),float(value&255u),255.0)/255.0;
})";
    std::string fragment="#version 330\nout vec4 finalColor;\nuniform vec2 probeCell;\n";
    std::string vertex;
    if(vertexStage) {
        vertex="#version 330\nin vec3 vertexPosition;uniform mat4 mvp;uniform vec2 probeCell;flat out float probeValue;\n"+
            hash+"\nvoid main(){probeValue="+name+"(probeCell);gl_Position=mvp*vec4(vertexPosition,1.0);}";
        fragment+="flat in float probeValue;\n#define PROBE probeValue\n";
    } else fragment+=hash+"\n#define PROBE "+name+"(probeCell)\n";
    fragment+=encode;
    const Shader shader=LoadShaderFromMemory(vertexStage?vertex.c_str():nullptr,fragment.c_str());
    require(IsShaderValid(shader) && shader.id!=rlGetShaderIdDefault(),"actual terrain hash compiles in GLSL 330");
    const int location=GetShaderLocation(shader,"probeCell");
    const auto target=LoadRenderTexture(1,1);
    for(const auto& probe:Probes) {
        const Vector2 cell{probe.x,probe.y};SetShaderValue(shader,location,&cell,SHADER_UNIFORM_VEC2);
        BeginTextureMode(target);ClearBackground(BLANK);BeginShaderMode(shader);
        DrawRectangle(0,0,1,1,WHITE);EndShaderMode();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);const auto pixel=GetImageColor(image,0,0);UnloadImage(image);
        const unsigned int actual=(static_cast<unsigned int>(pixel.r)<<16U)|(static_cast<unsigned int>(pixel.g)<<8U)|pixel.b;
        if(actual!=probe.expected) std::cerr<<path<<" stage="<<vertexStage<<" cell="<<probe.x<<','<<probe.y<<
            " expected="<<probe.expected<<" actual="<<actual<<'\n';
        require(actual==probe.expected,"GPU terrain hash exactly matches the stable cross-vendor reference");
    }
    UnloadRenderTexture(target);UnloadShader(shader);
}
}

int main(int argc,char** argv) {
    SetTraceLogLevel(LOG_ERROR);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(64,64,"Terrain noise parity");
    if(!IsWindowReady()) return 77;
    const std::filesystem::path root=argc>1?argv[1]:"assets/shaders";
    const auto worldVertex=(root/"world.vs").string(),worldFragment=(root/"world.fs").string();
    const Shader world=LoadShader(worldVertex.c_str(),worldFragment.c_str());
    require(IsShaderValid(world) && world.id!=rlGetShaderIdDefault(),"complete world shader links with stable noise");
    UnloadShader(world);
    for(bool vertex:{false,true}) {
        check(worldVertex.c_str(),"terrainHash21",vertex);
        check(worldFragment.c_str(),"hash21",vertex);
        check((root/"grass_instanced.fs").string().c_str(),"hash21",vertex);
    }
    CloseWindow();std::cout<<"All 108 terrain-noise probes match exactly in both shader stages\n";
}
