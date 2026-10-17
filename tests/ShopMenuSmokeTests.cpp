#include "TestHarness.hpp"
#include "localization/Localization.hpp"
#include "ui/ShopMenu.hpp"
#include "ui/UiText.hpp"

#include <filesystem>

int main(int argc,char** argv) {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_HIGHDPI);
    InitWindow(64,64,"Shop menu smoke");
    if(!IsWindowReady()) return 77;
    initializeLocalization();initializeUiText();
    GameUi ui;ui.initialize();
    for(auto language:{Language::English,Language::Russian}) {
        setLanguage(language);
        for(Vector2 viewport:{Vector2{1920,1080},Vector2{1280,720},Vector2{1280,800}}) {
            SetWindowSize(static_cast<int>(viewport.x),static_cast<int>(viewport.y));PollInputEvents();
            const auto panel=shopMenuPanelBounds(viewport);
            require(panel.x>0&&panel.y>0&&panel.x+panel.width<viewport.x&&panel.y+panel.height<viewport.y,"shop fits all tested aspect ratios");
            const float scale=std::min(viewport.x/1920,viewport.y/1080);
            const Vector2 origin{(viewport.x-1920*scale)*.5F,(viewport.y-1080*scale)*.5F};
            const auto point=[&](float x,float y){return Vector2{origin.x+x*scale,origin.y+y*scale};};
            auto target=LoadRenderTexture(static_cast<int>(viewport.x),static_cast<int>(viewport.y));
            MetaProgression profile;profile.silver=50;
            ShopMenuState state;
            BeginTextureMode(target);
            ClearBackground({61,82,66,255});
            auto result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1350,868),.click=true});
            require(result.purchase&&profile.silver==48&&metaShopLevel(profile,MetaShopUpgrade::SilverGain)==1,"buy button pays real price and raises permanent level");
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1350,868),.click=true});
            require(result.purchase&&profile.silver==45&&metaShopLevel(profile,MetaShopUpgrade::SilverGain)==2,"second purchase uses escalating price");
            profile.silver=0;const auto before=profile;
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1350,868),.click=true,.confirm=true});
            require(!result.purchase&&profile==before,"insufficient wallet blocks mouse and keyboard purchases");
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(650,340),.click=true});
            require(state.selected==MetaShopUpgrade::Rerolls,"tile selects the actual upgrade");
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1350,868),.click=true});
            require(result.openUnlocks&&!result.purchase,"locked rerolls link to unlock collection");
            profile.quests.ownedUnlocks.insert("meta.refresh");profile.silver=50;
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&metaShopLevel(profile,MetaShopUpgrade::Rerolls)==1&&profile.silver==46,"unlocked upgrade buys using Enter");
            state.selected=MetaShopUpgrade::SilverGain;profile.shopLevels[0]=20;
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1350,868),.click=true,.confirm=true});
            require(!result.purchase&&profile.silver==46,"capped upgrade never charges again");
            result=drawShopMenu(ui,profile,state,viewport,{.selectionStep=5});
            require(state.selected==MetaShopUpgrade::BuildingHealth,"keyboard moves between grid rows");
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(860,620),.click=true});
            require(state.selected==MetaShopUpgrade::TomeSlots,"ninth shop tile selects tome slots");
            profile.silver=50;
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&profile.silver==40&&metaRunBonuses(profile).additionalTomeSlots==1,
                "first tome slot costs ten silver");
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&profile.silver==25&&metaRunBonuses(profile).additionalTomeSlots==2,
                "second tome slot costs fifteen silver");
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(!result.purchase&&profile.silver==25,"tome slot shop purchase stops at six total slots");
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1060,620),.click=true});
            require(state.selected==MetaShopUpgrade::WeaponSlots,"tenth shop tile selects equipment slots");
            profile.silver=50;
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&profile.silver==40&&metaRunBonuses(profile).additionalWeaponSlots==1,
                "first equipment slot costs ten silver");
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&profile.silver==25&&metaRunBonuses(profile).additionalWeaponSlots==2,
                "second equipment slot costs fifteen silver");
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(!result.purchase&&profile.silver==25,"equipment slots stop at six total");
            result=drawShopMenu(ui,profile,state,viewport,{.pointer=point(1260,620),.click=true});
            require(state.selected==MetaShopUpgrade::BuildingSlots,"eleventh tile selects shared structure slots");
            profile.silver=50;
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&profile.silver==40&&metaRunBonuses(profile).additionalBuildingSlots==1,
                "first structure slot costs ten silver");
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(result.purchase&&profile.silver==25&&metaRunBonuses(profile).additionalBuildingSlots==2,
                "second structure slot costs fifteen silver");
            result=drawShopMenu(ui,profile,state,viewport,{.confirm=true});
            require(!result.purchase&&profile.silver==25,"structure slots stop at six total");
            result=drawShopMenu(ui,profile,state,viewport,{.close=true});require(result.close,"close input works");
            profile.shopLevels={4,2,3,1,0,0,0,0};profile.silver=38;state.selected=MetaShopUpgrade::SilverGain;
            ClearBackground({61,82,66,255});
            result=drawShopMenu(ui,profile,state,viewport);
            EndTextureMode();
            if(argc>1) {
                Image capture=LoadImageFromTexture(target.texture);ImageFlipVertical(&capture);
                const auto name="shop_"+std::to_string(static_cast<int>(viewport.x))+"x"+std::to_string(static_cast<int>(viewport.y))+(language==Language::Russian?"_ru.png":"_en.png");
                require(ExportImage(capture,(std::filesystem::path(argv[1])/name).string().c_str()),"shop screenshot exports");UnloadImage(capture);
            }
            UnloadRenderTexture(target);
        }
    }
    ui.shutdown();shutdownUiText();CloseWindow();return 0;
}
