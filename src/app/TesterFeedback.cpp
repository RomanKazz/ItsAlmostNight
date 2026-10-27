#include "app/TesterFeedback.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiViewport.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ian {
namespace {
float smooth(float t) { t=std::clamp(t,0.0F,1.0F);return t*t*(3-2*t); }
Color alpha(Color color,float opacity) {
    color.a=static_cast<unsigned char>(static_cast<float>(color.a)*std::clamp(opacity,0.0F,1.0F));return color;
}
Rectangle mix(Rectangle a,Rectangle b,float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.width+(b.width-a.width)*t,a.height+(b.height-a.height)*t};
}
std::size_t previousCharacter(std::string_view text,std::size_t cursor) {
    if(!cursor)return 0;
    --cursor;while(cursor && (static_cast<unsigned char>(text[cursor])&0xc0)==0x80)--cursor;return cursor;
}
std::size_t nextCharacter(std::string_view text,std::size_t cursor) {
    if(cursor>=text.size())return text.size();
    ++cursor;while(cursor<text.size() && (static_cast<unsigned char>(text[cursor])&0xc0)==0x80)++cursor;return cursor;
}
std::size_t characters(std::string_view text) {
    std::size_t count=0;for(char value:text)count+=(static_cast<unsigned char>(value)&0xc0)!=0x80;return count;
}
// User text stays literal and uses the Cyrillic-capable face in either UI language.
Vector2 measureComment(std::string_view text,float size) {
    const ScopedUiFontRole font(UiFontRole::BodyBold);
    return MeasureTextEx(uiFont(),std::string(text).c_str(),size,size*.02F);
}
void writeFile(const std::filesystem::path& file,std::string_view value) {
    std::ofstream stream(file,std::ios::binary);stream.write(value.data(),static_cast<std::streamsize>(value.size()));
    stream.close();if(!stream)throw std::runtime_error("Cannot write feedback file");
}
std::string timestamp() {
    const auto now=std::chrono::system_clock::now();
    const auto time=std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc,&time);
#else
    gmtime_r(&time,&utc);
#endif
    const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000;
    std::ostringstream out;out<<std::put_time(&utc,"%Y-%m-%d_%H-%M-%S")<<'-'<<std::setfill('0')<<std::setw(3)<<millis<<"_UTC";
    return out.str();
}
} // namespace

TesterFeedback::~TesterFeedback(){clear();}
void TesterFeedback::clear() {
    if(IsTextureValid(texture_))UnloadTexture(texture_);
    if(IsImageValid(image_))UnloadImage(image_);
    texture_={};image_={};active_=false;age_=0;comment_.clear();context_.clear();error_.clear();
    savedDirectory_.clear();cursor_=anchor_=0;firstLine_=0;
}
void TesterFeedback::begin(std::string context) {clear();context_=std::move(context);active_=true;}
bool TesterFeedback::capture(Image image) {
    if(!active_) {
        if(IsImageValid(image))UnloadImage(image);
        return false;
    }
    if(IsImageValid(image_))UnloadImage(image_);
    image_=image;
    if(!IsImageValid(image_))return false;
    if(IsTextureValid(texture_))UnloadTexture(texture_);
    texture_=LoadTextureFromImage(image_);
    if(!IsTextureValid(texture_))return false;
    SetTextureFilter(texture_,TEXTURE_FILTER_BILINEAR);age_=0;return true;
}
void TesterFeedback::advance(double seconds){age_+=std::max(0.0,seconds);}
TesterFeedback::Layout TesterFeedback::layout(Vector2 screen,float aspect) {
    const float s=std::min({1.0F,(screen.x-48)/1120,(screen.y-48)/880});
    const Rectangle panel{(screen.x-1120*s)*.5F,(screen.y-880*s)*.5F,1120*s,880*s};
    const Rectangle slot{panel.x+28*s,panel.y+88*s,panel.width-56*s,464*s};
    const float width=std::min(slot.width,slot.height*aspect),height=width/std::max(.1F,aspect);
    return {panel,{slot.x+(slot.width-width)*.5F,slot.y+(slot.height-height)*.5F,width,height},
        {panel.x+28*s,panel.y+600*s,panel.width-56*s,152*s},
        {panel.x+panel.width-268*s,panel.y+788*s,240*s,56*s},
        {panel.x+28*s,panel.y+788*s,168*s,56*s},
        {panel.x+28*s,panel.y+788*s,264*s,56*s},s};
}
std::vector<TesterFeedback::Line> TesterFeedback::textLines(float width,float size) const {
    const ScopedUiFontRole font(UiFontRole::BodyBold);
    std::vector<Line> lines;std::size_t begin=0,end=0;
    while(end<comment_.size()) {
        if(comment_[end]=='\n'){lines.push_back({begin,end});begin=++end;continue;}
        const auto next=nextCharacter(comment_,end);
        if(end>begin && measureComment(std::string_view(comment_).substr(begin,next-begin),size).x>width) {
            lines.push_back({begin,end});begin=end;
        }
        end=next;
    }
    lines.push_back({begin,end});return lines;
}
void TesterFeedback::eraseSelection() {
    const auto begin=std::min(cursor_,anchor_),end=std::max(cursor_,anchor_);
    comment_.erase(begin,end-begin);cursor_=anchor_=begin;
}
void TesterFeedback::replaceSelection(std::string_view text) {
    eraseSelection();std::string valid;
    const std::string padded = std::string(text) + std::string(4, '\0');const auto available=MaximumCharacters-std::min(MaximumCharacters,characters(comment_));
    std::size_t added=0;
    for(std::size_t i=0;i<text.size() && added<available;) {
        int bytes=1;const int code=GetCodepointNext(padded.data()+i,&bytes);
        if(bytes<=0 || i+static_cast<std::size_t>(bytes)>text.size())break;
        if(code=='\r'){i+=static_cast<std::size_t>(bytes);continue;}
        if(code=='\n' || code>=32) {
            if(code=='?' && bytes==1 && text[i]!='?'){++i;continue;}
            valid.append(text.substr(i,static_cast<std::size_t>(bytes)));++added;
        }
        i+=static_cast<std::size_t>(bytes);
    }
    comment_.insert(cursor_,valid);cursor_+=valid.size();anchor_=cursor_;
}
void TesterFeedback::keepCursorVisible(float width,float font,float lineHeight,float height) {
    const auto lines=textLines(width,font);int row=0;
    for(std::size_t i=0;i<lines.size();++i)if(cursor_>=lines[i].begin)row=static_cast<int>(i);
    const int visible=std::max(1,static_cast<int>(height/lineHeight));
    firstLine_=std::clamp(firstLine_,0,std::max(0,static_cast<int>(lines.size())-visible));
    if(row<firstLine_)firstLine_=row;
    if(row>=firstLine_+visible)firstLine_=row-visible+1;
}
TesterFeedback::Action TesterFeedback::input(const Input& in,Vector2 screen) {
    if(in.close)return Action::Close;
    if(!ready())return Action::None;
    const auto l=layout(screen,static_cast<float>(image_.width)/static_cast<float>(image_.height));
    if(!saved() && in.click && CheckCollisionPointRec(in.pointer,l.cancel))return Action::Close;
    const bool saveRequested=age_>.18 && (in.save || (in.click && CheckCollisionPointRec(in.pointer,l.save)));
    if(saved()) {
        if(saveRequested)return Action::Close;
        return in.click && CheckCollisionPointRec(in.pointer,l.folder)?Action::OpenFolder:Action::None;
    }
    const float size=26*l.scale,lineHeight=36*l.scale,width=l.text.width-32*l.scale;
    if(in.click && CheckCollisionPointRec(in.pointer,l.text)) {
        const auto lines=textLines(width,size);
        const int row=std::clamp(firstLine_+static_cast<int>((in.pointer.y-l.text.y-14*l.scale)/lineHeight),0,static_cast<int>(lines.size())-1);
        const auto line=lines[static_cast<std::size_t>(row)];cursor_=line.begin;
        const ScopedUiFontRole font(UiFontRole::BodyBold);
        while(cursor_<line.end) {
            const auto next=nextCharacter(comment_,cursor_);
            if(measureComment(std::string_view(comment_).substr(line.begin,next-line.begin),size).x>in.pointer.x-l.text.x-16*l.scale)break;
            cursor_=next;
        }
        if(!in.shift)anchor_=cursor_;
    }
    if(in.selectAll){anchor_=0;cursor_=comment_.size();}
    if(in.home)cursor_=0;
    if(in.end)cursor_=comment_.size();
    if(in.cursorStep)cursor_=in.cursorStep<0?previousCharacter(comment_,cursor_):nextCharacter(comment_,cursor_);
    if((in.cursorStep||in.home||in.end) && !in.shift)anchor_=cursor_;
    if(in.backspace){if(cursor_==anchor_)anchor_=previousCharacter(comment_,cursor_);eraseSelection();}
    if(in.erase){if(cursor_==anchor_)anchor_=nextCharacter(comment_,cursor_);eraseSelection();}
    if(!in.text.empty())replaceSelection(in.text);
    if(in.backspace||in.erase||!in.text.empty()||in.cursorStep||in.home||in.end)
        keepCursorVisible(width,size,lineHeight,l.text.height-24*l.scale);
    if(in.wheel != 0.0F && CheckCollisionPointRec(in.pointer,l.text)) {
        const auto lines=textLines(width,size);
        firstLine_=std::clamp(firstLine_-static_cast<int>(in.wheel*3),0,std::max(0,static_cast<int>(lines.size())-4));
    }
    return saveRequested?Action::Save:Action::None;
}
void TesterFeedback::draw(Vector2 screen,bool ru,bool reduceFlashes) const {
    if(!active_||!ready())return;
    const auto l=layout(screen,static_cast<float>(image_.width)/static_cast<float>(image_.height));
    const float s=l.scale,t=smooth(static_cast<float>(age_/(reduceFlashes?.14:.34)));
    const float opacity=smooth(static_cast<float>((age_-.06)/.20));
    const auto image=[&](Rectangle box,Color color){DrawTexturePro(texture_,
        {0,0,static_cast<float>(texture_.width),static_cast<float>(texture_.height)},box,{},0,color);};
    image({0,0,screen.x,screen.y},WHITE);
    DrawRectangleRec({0,0,screen.x,screen.y},{0,0,0,static_cast<unsigned char>(175*t)});
    menuStyle::panel({l.panel.x+4*s,l.panel.y+8*s,l.panel.width,l.panel.height},s,{5,9,7,255},opacity*.65F);
    menuStyle::panel(l.panel,s,menuStyle::Inset,opacity);
    menuStyle::text(ru?"ОБРАТНАЯ СВЯЗЬ":"FEEDBACK",{l.panel.x+28*s,l.panel.y+16*s,l.panel.width-56*s,42*s},
        32*s,alpha(menuStyle::Cream,opacity),UiFontRole::Display);
    menuStyle::text(ru?"Игра на паузе. Сохрани момент и расскажи, что случилось.":"Game paused. Save this moment and tell us what happened.",
        {l.panel.x+28*s,l.panel.y+57*s,l.panel.width-56*s,22*s},18*s,alpha(menuStyle::Muted,opacity),UiFontRole::MenuBodyBold);
    const auto start=reduceFlashes?Rectangle{l.photo.x-8*s,l.photo.y-4*s,l.photo.width+16*s,l.photo.height+8*s}:Rectangle{0,0,screen.x,screen.y};
    const auto photo=mix(start,l.photo,t);
    DrawRectangleRec({photo.x-4*s,photo.y-4*s,photo.width+8*s,photo.height+8*s},menuStyle::Cream);
    image(photo,WHITE);
    const float corner=22*s;
    for(const auto point:std::array<Vector2,4>{{{photo.x,photo.y},{photo.x+photo.width,photo.y},
            {photo.x,photo.y+photo.height},{photo.x+photo.width,photo.y+photo.height}}}) {
        const float dx=point.x==photo.x?1.0F:-1.0F,dy=point.y==photo.y?1.0F:-1.0F;
        DrawLineEx(point,{point.x+dx*corner,point.y},3*s,alpha(menuStyle::Gold,1-t));
        DrawLineEx(point,{point.x,point.y+dy*corner},3*s,alpha(menuStyle::Gold,1-t));
    }
    if(!reduceFlashes && age_<.10)DrawRectangleRec({0,0,screen.x,screen.y},alpha(menuStyle::Cream,
        .09F*(1-static_cast<float>(age_/.10))));
    menuStyle::text(saved()?(ru?"СОХРАНЕНО":"SAVED"):(ru?"КОММЕНТАРИЙ":"COMMENT"),
        {l.text.x,l.text.y-36*s,l.text.width-140*s,28*s},23*s,
        alpha(saved()?menuStyle::Green:menuStyle::Cream,opacity),UiFontRole::Display);
    menuStyle::text(std::to_string(characters(comment_))+" / "+std::to_string(MaximumCharacters),
        {l.text.x+l.text.width-140*s,l.text.y-30*s,140*s,22*s},16*s,alpha(menuStyle::Muted,opacity),UiFontRole::MenuBodyBold,2);
    drawSolidRoundedRect(l.text,.13F,alpha({11,18,15,255},opacity));
    DrawRectangleRoundedLinesEx(l.text,.13F,10,1.5F*s,alpha(saved()?menuStyle::Frame:menuStyle::Gold,opacity*.8F));
    const float font=26*s,lineHeight=36*s;
    const auto lines=textLines(l.text.width-32*s,font);
    beginUiScissor(static_cast<int>(l.text.x+12*s),static_cast<int>(l.text.y+8*s),
        static_cast<int>(l.text.width-24*s),static_cast<int>(l.text.height-16*s));
    const ScopedUiFontRole body(UiFontRole::BodyBold);
    const auto begin=std::min(cursor_,anchor_),end=std::max(cursor_,anchor_);
    if(comment_.empty())menuStyle::text(ru?"Что пошло не так? Можно вставить текст из буфера.":"What went wrong? You can paste text from the clipboard.",
        {l.text.x+16*s,l.text.y+14*s,l.text.width-32*s,28*s},22*s,alpha(menuStyle::Muted,opacity),UiFontRole::MenuBodyBold);
    for(std::size_t i=0;i<lines.size();++i) {
        const auto line=lines[i];const float y=l.text.y+14*s+static_cast<float>(static_cast<int>(i)-firstLine_)*lineHeight;
        const auto value=std::string_view(comment_).substr(line.begin,line.end-line.begin);
        const auto selectedBegin=std::max(begin,line.begin),selectedEnd=std::min(end,line.end);
        if(!saved() && selectedBegin<selectedEnd) {
            const float x=measureComment(std::string_view(comment_).substr(line.begin,selectedBegin-line.begin),font).x;
            const float width=measureComment(std::string_view(comment_).substr(selectedBegin,selectedEnd-selectedBegin),font).x;
            DrawRectangleRec({l.text.x+16*s+x,y,width,lineHeight},alpha({98,118,64,255},opacity));
        }
        const std::string literal(value);
        DrawTextEx(uiFont(),literal.c_str(),{l.text.x+16*s,y+1.5F*s},font,font*.02F,alpha(BLACK,opacity*.65F));
        DrawTextEx(uiFont(),literal.c_str(),{l.text.x+16*s,y},font,font*.02F,alpha(menuStyle::Cream,opacity));
        const bool last=i+1==lines.size();
        if(!saved() && cursor_>=line.begin && (cursor_<line.end || (cursor_==line.end && (last||lines[i+1].begin>line.end))) &&
            std::fmod(age_,1.0)<.62) {
            const float x=measureComment(std::string_view(comment_).substr(line.begin,cursor_-line.begin),font).x;
            DrawRectangleRec({l.text.x+16*s+x,y+3*s,2*s,font},alpha(menuStyle::Gold,opacity));
        }
    }
    EndScissorMode();
    const auto button=[&](Rectangle bounds,std::string_view text,bool selected){
        const bool hovered=CheckCollisionPointRec(uiMousePosition(),bounds);
        menuStyle::panel(bounds,s,selected?menuStyle::Selected:hovered?menuStyle::Hover:menuStyle::Card,opacity);
        menuStyle::text(text,{bounds.x+10*s,bounds.y,bounds.width-20*s,bounds.height},22*s,
            alpha(menuStyle::Cream,opacity),UiFontRole::Display,1);
    };
    button(l.save,saved()?(ru?"ГОТОВО":"DONE"):(ru?"СОХРАНИТЬ":"SAVE"),true);
    if(!saved())button(l.cancel,ru?"ОТМЕНА":"CANCEL",false);
    if(saved())button(l.folder,ru?"ОТКРЫТЬ ПАПКУ":"OPEN FOLDER",false);
    const auto detail=!error_.empty()?(ru?"Не удалось сохранить. Проверь доступ к папке игры и повтори.":"Could not save. Check the game folder permissions and try again."):
        saved()?(ru?"Отправь разработчику папку feedback целиком.":"Send the complete feedback folder to the developer."):
        (ru?"Ctrl / Cmd + Enter — сохранить. Esc — отмена.":"Ctrl / Cmd + Enter to save. Esc to cancel.");
    menuStyle::text(detail,{l.text.x,l.panel.y+758*s,l.text.width,22*s},16*s,
        alpha(error_.empty()?menuStyle::Muted:Color{255,119,119,255},opacity),UiFontRole::MenuBodyBold);
}

bool TesterFeedback::save(const std::filesystem::path& root) {
    if(!active_||!ready())return false;
    if(saved())return true;
    std::filesystem::path temporary;
    try {
        auto context=nlohmann::json::parse(context_);
        std::filesystem::create_directories(root);
        const auto stamp=timestamp();std::filesystem::path destination;
        for(int i=0;i<1000;++i) {
            const auto name=stamp+(i?"-"+std::to_string(i):"");
            destination=root/name;
            const auto candidate=root/(".pending-"+name);
            if(!std::filesystem::exists(destination) && std::filesystem::create_directory(candidate)) {
                temporary=candidate;
                break;
            }
        }
        if(temporary.empty())throw std::runtime_error("Cannot allocate feedback directory");
        if(!ExportImage(image_,(temporary/"screenshot.png").string().c_str()))throw std::runtime_error("Cannot save screenshot");
        writeFile(temporary/"comment.txt",comment_);
        context["created_utc"]=stamp;context["screenshot"]="screenshot.png";context["comment"]="comment.txt";
        writeFile(temporary/"context.json",context.dump(2)+"\n");
        std::filesystem::rename(temporary,destination);temporary.clear();savedDirectory_=destination;error_.clear();
        if(!std::filesystem::exists(root/"README.txt")) {
            try {writeFile(root/"README.txt","FORTBONK tester feedback\nSend this entire feedback folder to the developer.\nEach report contains screenshot.png, comment.txt (UTF-8), and context.json.\n");}
            catch(const std::exception&) {} // The complete report is already safely committed.
        }
        return true;
    } catch(const std::exception&) {
        if(!temporary.empty()){std::error_code error;std::filesystem::remove_all(temporary,error);}
        error_="save_failed";return false;
    }
}
std::string TesterFeedback::folderUrl(const std::filesystem::path& folder) {
    const auto path=std::filesystem::absolute(folder).generic_u8string();std::string url="file://";
#ifdef _WIN32
    url+='/';
#endif
    constexpr char hex[]="0123456789ABCDEF";
    for(char8_t value:path) {
        const auto c=static_cast<unsigned char>(value);
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c==':'||c=='-'||c=='_'||c=='.')url+=static_cast<char>(c);
        else {url+='%';url+=hex[c>>4U];url+=hex[c&15U];}
    }
    return url;
}
} // namespace ian
