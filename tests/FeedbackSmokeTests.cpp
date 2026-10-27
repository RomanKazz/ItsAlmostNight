#include "TestHarness.hpp"
#include "app/TesterFeedback.hpp"
#include "ui/UiText.hpp"
#include "ui/UiViewport.hpp"
#include "localization/Localization.hpp"

#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
Image screenshot() {
    auto image=GenImageColor(960,540,{28,115,155,255});
    ImageDrawRectangle(&image,0,270,960,270,{54,129,42,255});
    ImageDrawCircle(&image,480,300,80,{139,196,22,255});
    return image;
}
}
int main(int argc,char** argv) {
    using namespace ian;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(960,540,"Tester feedback smoke");
    if(!IsWindowReady()) return 77;
    SetExitKey(KEY_NULL);
    initializeLocalization();initializeUiText();
    const auto root=std::filesystem::temp_directory_path()/
        ("ian-feedback-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const nlohmann::json context{{"format_version",1},{"world_seed",8123123},{"game_version","test"}};
    constexpr Vector2 screen{1920,1080};
    {
        TesterFeedback feedback;
        feedback.begin(context.dump());
        require(feedback.active() && feedback.needsCapture(),"F3 draft waits for the completed frame");
        require(feedback.capture(screenshot()),"a captured image becomes a valid preview");
        feedback.advance(.6);
        auto input=[&](TesterFeedback::Input value){return feedback.input(value,screen);};
        static_cast<void>(input({.text="Дерево исчезло.\nEnemy stuck near the core."}));
        require(feedback.comment()=="Дерево исчезло.\nEnemy stuck near the core.","multiline Russian and English comments survive intact");
        static_cast<void>(input({.selectAll=true,.text="Тест 😀"}));
        static_cast<void>(input({.backspace=true}));
        require(feedback.comment()=="Тест ","backspace removes a complete Unicode character");
        static_cast<void>(input({.cursorStep=-1}));
        static_cast<void>(input({.text="!"}));
        require(feedback.comment()=="Тест! ","insertion and cursor movement respect UTF-8 boundaries");
        static_cast<void>(input({.selectAll=true,.text=std::string(1400,'a')}));
        require(feedback.comment().size()==TesterFeedback::MaximumCharacters,"pasting cannot exceed the comment limit");
        static_cast<void>(input({.selectAll=true,.text="Обводка пропала после удара.\nHappens near a tree."}));
        require(input({.save=true,.text="!"})==TesterFeedback::Action::Save && feedback.comment().ends_with("!"),
            "saving includes the final characters from the same input frame");
        const auto comment=feedback.comment();
        std::filesystem::create_directories(root);
        {std::ofstream file(root/"blocked");file<<"cannot be a directory";}
        require(!feedback.save(root/"blocked") && feedback.ready() && feedback.comment()==comment,
            "a failed write preserves the screenshot and draft for retry");
        require(feedback.save(root/"feedback"),"the full report can be retried and saved");
        const auto directory=feedback.savedDirectory();
        require(read(directory/"comment.txt")==comment,"comment.txt is exact UTF-8");
        const auto metadata=nlohmann::json::parse(read(directory/"context.json"));
        require(metadata.at("world_seed")==8123123 && metadata.contains("created_utc"),"the report identifies the captured session");
        auto image=LoadImage((directory/"screenshot.png").string().c_str());
        require(IsImageValid(image) && image.width==960 && image.height==540,"saved PNG retains full capture resolution");
        const auto top=GetImageColor(image,20,20),bottom=GetImageColor(image,20,500);
        require(top.r==28 && top.g==115 && bottom.g==129 && bottom.r==54,
            "screenshot is unmodified, upright, and contains no dimmer, caption or frame");
        UnloadImage(image);
        require(feedback.save(root/"feedback") && feedback.savedDirectory()==directory,"a double click cannot duplicate a report");
        require(input({.save=true})==TesterFeedback::Action::Close,"the saved screen returns to the game with Done");
        const auto layout=TesterFeedback::layout(screen,16.0F/9.0F);
        require(input({.pointer={layout.folder.x+5,layout.folder.y+5},.click=true})==TesterFeedback::Action::OpenFolder,
            "the saved report offers a folder shortcut");
        static_cast<void>(input({.text="immutable"}));require(feedback.comment()==comment,"saved reports cannot be edited accidentally");
        if(argc>1) {
            std::filesystem::create_directories(argv[1]);
            for(const bool russian:{false,true}) {
                setLanguage(russian?Language::Russian:Language::English);
                TesterFeedback previewForm;
                previewForm.begin(context.dump());
                require(previewForm.capture(argc>2?LoadImage(argv[2]):screenshot()),"preview screenshot loads");
                previewForm.advance(.6);
                static_cast<void>(previewForm.input({.text="Обводка пропала после удара.\nHappens near a tree."},screen));
                const auto target=LoadRenderTexture(1920,1080);
                BeginTextureMode(target);ClearBackground(BLACK);previewForm.draw(screen,russian,false);EndTextureMode();
                auto preview=LoadImageFromTexture(target.texture);ImageFlipVertical(&preview);
                require(ExportImage(preview,(std::filesystem::path(argv[1])/(russian?"feedback-ru.png":"feedback-en.png")).string().c_str()),"UI preview exports");
                UnloadImage(preview);UnloadRenderTexture(target);
            }
        }
        feedback.begin(context.dump());require(feedback.capture(screenshot()),"a subsequent capture replaces the owned image safely");
        feedback.advance(.6);require(feedback.save(root/"feedback") && feedback.savedDirectory()!=directory,"successive reports never overwrite one another");
        feedback.clear();require(!feedback.active() && !feedback.ready() && feedback.comment().empty(),"closing releases the screenshot and clears the draft");
        feedback.begin(context.dump());require(feedback.capture(screenshot()),"cancelled draft can own a screenshot");
        static_cast<void>(input({.text="discard"}));
        require(input({.close=true})==TesterFeedback::Action::Close,"Escape discards a draft without saving");
        feedback.clear();
        std::size_t reports=0;
        for(const auto& entry:std::filesystem::directory_iterator(root/"feedback")) {
            require(!entry.path().filename().string().starts_with(".pending-"),"no incomplete report is left behind");
            if(entry.is_directory()) ++reports;
        }
        require(reports==2,"only explicitly saved reports appear in feedback");
        require(TesterFeedback::folderUrl(root/std::filesystem::path(u8"Папка с пробелом")).find("%20")!=std::string::npos,"folder links escape Unicode and spaces");
        for(const Vector2 resolution:{Vector2{1920,1080},Vector2{1280,800},Vector2{800,1200},Vector2{640,360}}) {
            const auto viewport=UiViewport::fromScreen(resolution);
            const auto l=TesterFeedback::layout(viewport.size,16.0F/9.0F);
            require(l.panel.x>=0 && l.panel.y>=0 && l.panel.x+l.panel.width<=viewport.size.x && l.panel.y+l.panel.height<=viewport.size.y,
                "the feedback form stays on-screen at supported aspect ratios");
            require(l.text.y>l.photo.y+l.photo.height && l.save.y>l.text.y+l.text.height,"caption and buttons do not overlap the screenshot");
        }
    }
    const auto shutter=LoadWave("assets/audio/feedback_shutter.wav");
    require(IsWaveValid(shutter) && static_cast<double>(shutter.frameCount)/shutter.sampleRate<.25,"the original shutter cue is short and loadable");
    UnloadWave(shutter);
    std::filesystem::remove_all(root);
    shutdownUiText();CloseWindow();
    return 0;
}
