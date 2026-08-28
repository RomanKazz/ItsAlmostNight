#include "ui/UiEditor.hpp"

#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <lunasvg.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

namespace ian {
namespace {

using Json = nlohmann::json;

constexpr float DesignWidth = 1280.0F;
constexpr float DesignHeight = 720.0F;
constexpr float LeftWidth = 226.0F;
constexpr float RightWidth = 292.0F;
constexpr float TopHeight = 72.0F;
constexpr std::string_view ProjectPath =
    "assets/ui/ui_editor.json";
constexpr std::size_t UndoHistoryLimit = 64U;

std::string lowercaseExtension(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos) return {};
    std::string extension{path.substr(dot)};
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char value) {
                               return static_cast<char>(std::tolower(value));
                           });
    return extension;
}

bool hasSupportedImageExtension(std::string_view path) {
    const std::string extension = lowercaseExtension(path);
    return extension == ".png" || extension == ".svg";
}

bool isSvgPath(std::string_view path) {
    return lowercaseExtension(path) == ".svg";
}

Texture2D loadSvgTexture(const std::string& path, int rasterScale) {
    std::unique_ptr<lunasvg::Document> document =
        lunasvg::Document::loadFromFile(path);
    if (!document) return {};
    const int scale = std::clamp(rasterScale, 1, 8);
    const int width = std::clamp(
        static_cast<int>(std::lround(std::max(document->width(), 1.0F))) * scale,
        1, 4096);
    const int height = std::clamp(
        static_cast<int>(std::lround(std::max(document->height(), 1.0F))) * scale,
        1, 4096);
    lunasvg::Bitmap bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull()) return {};
    bitmap.convertToRGBA();
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    for (int row = 0; row < height; ++row) {
        std::memcpy(pixels.data() + static_cast<std::size_t>(row) *
                                      static_cast<std::size_t>(width) * 4U,
                    bitmap.data() + static_cast<std::size_t>(row) *
                                      static_cast<std::size_t>(bitmap.stride()),
                    static_cast<std::size_t>(width) * 4U);
    }
    Image image{.data = pixels.data(), .width = width, .height = height,
                .mipmaps = 1,
                .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    return LoadTextureFromImage(image);
}

Rectangle fitRectangle(float width, float height, Rectangle area) {
    if (width <= 0.0F || height <= 0.0F) return area;
    const float scale = std::min(area.width / width, area.height / height);
    const float fittedWidth = width * scale;
    const float fittedHeight = height * scale;
    return {
        area.x + (area.width - fittedWidth) * 0.5F,
        area.y + (area.height - fittedHeight) * 0.5F,
        fittedWidth, fittedHeight};
}

void drawEditorPanel(Rectangle bounds, Color color) {
    DrawRectangleRounded(bounds, 0.035F, 8, color);
    DrawRectangleRoundedLinesEx(
        bounds, 0.035F, 8, 2.0F, {91, 116, 136, 255});
}

void drawCheckerboard(Rectangle bounds) {
    constexpr float Cell = 24.0F;
    const int columns = static_cast<int>(std::ceil(bounds.width / Cell));
    const int rows = static_cast<int>(std::ceil(bounds.height / Cell));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const Color color = (row + column) % 2 == 0
                ? Color{42, 49, 60, 255}
                : Color{49, 57, 69, 255};
            DrawRectangleRec(
                {bounds.x + static_cast<float>(column) * Cell,
                 bounds.y + static_cast<float>(row) * Cell,
                 std::min(Cell, bounds.x + bounds.width -
                                   (bounds.x + static_cast<float>(column) * Cell)),
                 std::min(Cell, bounds.y + bounds.height -
                                   (bounds.y + static_cast<float>(row) * Cell))},
                color);
        }
    }
}

} // namespace

UiEditor::~UiEditor() {
    shutdown();
}

void UiEditor::initialize() {
    shutdown();
    addDefaultAssets();
    loadProject();
    if (elements_.empty()) addDefaultLayout();
    undoStack_.clear();
    initialized_ = true;
}

void UiEditor::shutdown() {
    for (Asset& asset : assets_) {
        if (IsTextureValid(asset.texture)) {
            UnloadTexture(asset.texture);
        }
        asset.texture = {};
    }
    assets_.clear();
    elements_.clear();
    undoStack_.clear();
    initialized_ = false;
    visible_ = false;
}

void UiEditor::toggle() {
    visible_ = !visible_;
    draggingElement_ = -1;
    resizingElement_ = -1;
    sliceHandle_ = SliceHandle::None;
    resizingPreview_ = false;
    textEditing_ = false;
}

void UiEditor::close() {
    visible_ = false;
    draggingElement_ = -1;
    resizingElement_ = -1;
    sliceHandle_ = SliceHandle::None;
    resizingPreview_ = false;
    textEditing_ = false;
}

bool UiEditor::visible() const {
    return visible_;
}

void UiEditor::addDefaultAssets() {
    const auto add = [this](std::string name, std::string path,
                            int left, int top, int right, int bottom) {
        Asset asset{
            .name = std::move(name), .path = std::move(path),
            .left = left, .top = top, .right = right, .bottom = bottom,
            .rasterScale = 1};
        reloadAsset(asset);
        assets_.push_back(std::move(asset));
    };
    add("Panel", "assets/ui/cartoony/panel.png", 16, 16, 16, 18);
    add("Inset", "assets/ui/cartoony/inset.png", 9, 9, 9, 9);
    add("Button", "assets/ui/cartoony/button.png", 12, 10, 12, 14);
    add("Button hover", "assets/ui/cartoony/button-hover.png", 12, 10, 12, 14);
    add("Button pressed", "assets/ui/cartoony/button-pressed.png", 12, 10, 12, 14);
    add("Button active", "assets/ui/cartoony/button-active.png", 12, 10, 12, 14);
    add("Bar", "assets/ui/cartoony/bar-back.png", 12, 4, 12, 4);
    add("Bar fill", "assets/ui/cartoony/bar-blue.png", 12, 4, 12, 4);
}

void UiEditor::addDefaultLayout() {
    elements_.push_back({"menu_panel", ElementKind::Panel,
                         {360.0F, 90.0F, 560.0F, 540.0F}, 0U, {}});
    elements_.push_back({"menu_title", ElementKind::Label,
                         {480.0F, 135.0F, 320.0F, 54.0F}, 0U,
                         "MAIN MENU"});
    elements_.push_back({"play", ElementKind::Button,
                         {485.0F, 245.0F, 310.0F, 58.0F}, 2U, "PLAY"});
    elements_.push_back({"settings", ElementKind::Button,
                         {485.0F, 325.0F, 310.0F, 58.0F}, 2U,
                         "SETTINGS"});
    elements_.push_back({"exit", ElementKind::Button,
                         {485.0F, 405.0F, 310.0F, 58.0F}, 2U, "EXIT"});
}

void UiEditor::reloadAsset(Asset& asset) {
    if (IsTextureValid(asset.texture)) UnloadTexture(asset.texture);
    asset.rasterScale = std::clamp(asset.rasterScale, 1, 8);
    asset.texture = isSvgPath(asset.path)
        ? loadSvgTexture(asset.path, asset.rasterScale)
        : LoadTexture(asset.path.c_str());
    if (IsTextureValid(asset.texture)) {
        SetTextureFilter(asset.texture, TEXTURE_FILTER_BILINEAR);
        asset.left = std::clamp(asset.left, 0, asset.texture.width - 1);
        asset.right = std::clamp(
            asset.right, 0, asset.texture.width - asset.left - 1);
        asset.top = std::clamp(asset.top, 0, asset.texture.height - 1);
        asset.bottom = std::clamp(
            asset.bottom, 0, asset.texture.height - asset.top - 1);
    }
}

void UiEditor::loadProject() {
    std::ifstream input{std::string{ProjectPath}};
    if (!input) return;
    try {
        const Json root = Json::parse(input);
        if (const auto found = root.find("assets");
            found != root.end() && found->is_array()) {
            for (const auto& item : *found) {
                const std::string path = item.value("path", "");
                if (path.empty()) continue;
                auto asset = std::ranges::find_if(
                    assets_, [&path](const Asset& current) {
                        return current.path == path;
                    });
                if (asset == assets_.end()) {
                    Asset imported{
                        .name = item.value("name", "Imported"),
                        .path = path,
                        .left = item.value("left", 8),
                        .top = item.value("top", 8),
                        .right = item.value("right", 8),
                        .bottom = item.value("bottom", 8),
                        .rasterScale = item.value("rasterScale", 1)};
                    reloadAsset(imported);
                    assets_.push_back(std::move(imported));
                } else {
                    asset->left = item.value("left", asset->left);
                    asset->top = item.value("top", asset->top);
                    asset->right = item.value("right", asset->right);
                    asset->bottom = item.value("bottom", asset->bottom);
                    asset->rasterScale =
                        item.value("rasterScale", asset->rasterScale);
                    reloadAsset(*asset);
                }
            }
        }
        if (const auto found = root.find("elements");
            found != root.end() && found->is_array()) {
            elements_.clear();
            for (const auto& item : *found) {
                const std::string kind = item.value("kind", "button");
                Element element;
                element.id = item.value("id", "element");
                element.kind = kind == "panel" ? ElementKind::Panel
                    : kind == "label" ? ElementKind::Label
                                      : ElementKind::Button;
                element.bounds = {
                    item.value("x", 100.0F), item.value("y", 100.0F),
                    item.value("width", 240.0F),
                    item.value("height", 56.0F)};
                element.asset = std::min<std::size_t>(
                    item.value("asset", 0U),
                    assets_.empty() ? 0U : assets_.size() - 1U);
                element.text = item.value("text", "BUTTON");
                elements_.push_back(std::move(element));
            }
        }
    } catch (const Json::exception&) {
        status_ = "LOAD ERROR - USING DEFAULTS";
        statusRemaining_ = 4.0;
    }
}

void UiEditor::saveProject() {
    Json root;
    root["designWidth"] = DesignWidth;
    root["designHeight"] = DesignHeight;
    root["assets"] = Json::array();
    for (const Asset& asset : assets_) {
        root["assets"].push_back({
            {"name", asset.name}, {"path", asset.path},
            {"left", asset.left}, {"top", asset.top},
            {"right", asset.right}, {"bottom", asset.bottom},
            {"rasterScale", asset.rasterScale}});
    }
    root["elements"] = Json::array();
    for (const Element& element : elements_) {
        const char* kind = element.kind == ElementKind::Panel ? "panel"
            : element.kind == ElementKind::Label ? "label" : "button";
        root["elements"].push_back({
            {"id", element.id}, {"kind", kind},
            {"x", element.bounds.x}, {"y", element.bounds.y},
            {"width", element.bounds.width},
            {"height", element.bounds.height},
            {"asset", element.asset}, {"text", element.text}});
    }
    std::filesystem::create_directories("assets/ui");
    std::ofstream output{std::string{ProjectPath}};
    if (!output) {
        status_ = "SAVE FAILED";
        statusRemaining_ = 3.0;
        return;
    }
    output << root.dump(2) << '\n';
    dirty_ = false;
    status_ = "SAVED  assets/ui/ui_editor.json";
    statusRemaining_ = 3.0;
}

void UiEditor::importDroppedFiles() {
    if (!IsFileDropped()) return;
    const FilePathList dropped = LoadDroppedFiles();
    for (unsigned int index = 0; index < dropped.count; ++index) {
        const std::string sourcePath = dropped.paths[index];
        if (!hasSupportedImageExtension(sourcePath)) continue;
        std::filesystem::create_directories("assets/ui/editor_imports");
        const std::filesystem::path destination =
            std::filesystem::path{"assets/ui/editor_imports"} /
            std::filesystem::path{sourcePath}.filename();
        std::error_code copyError;
        if (std::filesystem::path{sourcePath}.lexically_normal() !=
            destination.lexically_normal()) {
            std::filesystem::copy_file(
                sourcePath, destination,
                std::filesystem::copy_options::overwrite_existing,
                copyError);
        }
        const std::string path = copyError
            ? sourcePath : destination.generic_string();
        Asset asset{
            .name = std::filesystem::path(path).stem().string(),
            .path = path, .left = isSvgPath(path) ? 32 : 8,
            .top = isSvgPath(path) ? 32 : 8,
            .right = isSvgPath(path) ? 32 : 8,
            .bottom = isSvgPath(path) ? 32 : 8,
            .rasterScale = isSvgPath(path) ? 4 : 1};
        reloadAsset(asset);
        if (IsTextureValid(asset.texture)) {
            pushUndo();
            assets_.push_back(std::move(asset));
            selectedAsset_ = assets_.size() - 1U;
            dirty_ = true;
            status_ = isSvgPath(path) ? "SVG IMPORTED" : "PNG IMPORTED";
            statusRemaining_ = 2.0;
        }
    }
    UnloadDroppedFiles(dropped);
}

NPatchInfo UiEditor::patchFor(const Asset& asset) const {
    return {
        .source = {0.0F, 0.0F,
                   static_cast<float>(asset.texture.width),
                   static_cast<float>(asset.texture.height)},
        .left = asset.left, .top = asset.top,
        .right = asset.right, .bottom = asset.bottom,
        .layout = NPATCH_NINE_PATCH};
}

void UiEditor::drawAsset(const Asset& asset, Rectangle bounds,
                         Color tint) const {
    if (!IsTextureValid(asset.texture)) {
        DrawRectangleLinesEx(bounds, 2.0F, RED);
        return;
    }
    DrawTextureNPatch(asset.texture, patchFor(asset), bounds,
                      {0.0F, 0.0F}, 0.0F, tint);
}

Rectangle UiEditor::canvasBounds() const {
    const Rectangle available{
        LeftWidth + 16.0F, TopHeight + 16.0F,
        static_cast<float>(GetScreenWidth()) - LeftWidth - RightWidth - 48.0F,
        static_cast<float>(GetScreenHeight()) - TopHeight - 32.0F};
    return fitRectangle(DesignWidth, DesignHeight, available);
}

Rectangle UiEditor::designToScreen(Rectangle bounds) const {
    const Rectangle canvas = canvasBounds();
    const float scale = canvas.width / DesignWidth;
    return {canvas.x + bounds.x * scale, canvas.y + bounds.y * scale,
            bounds.width * scale, bounds.height * scale};
}

Vector2 UiEditor::screenToDesign(Vector2 point) const {
    const Rectangle canvas = canvasBounds();
    const float scale = canvas.width / DesignWidth;
    return {(point.x - canvas.x) / scale, (point.y - canvas.y) / scale};
}

void UiEditor::addElement(ElementKind kind) {
    pushUndo();
    const char* kindLabel = kind == ElementKind::Panel ? "PANEL"
        : kind == ElementKind::Label ? "LABEL" : "BUTTON";
    const std::size_t asset = kind == ElementKind::Panel ? 0U : 2U;
    const std::size_t sequence = elements_.size() + 1U;
    Element element{
        .id = std::string{kindLabel} + "_" + std::to_string(sequence),
        .kind = kind,
        .bounds = kind == ElementKind::Panel
            ? Rectangle{390.0F, 150.0F, 500.0F, 400.0F}
            : kind == ElementKind::Label
                ? Rectangle{490.0F, 180.0F, 300.0F, 48.0F}
                : Rectangle{490.0F, 300.0F, 300.0F, 58.0F},
        .asset = std::min(asset, assets_.empty() ? 0U : assets_.size() - 1U),
        .text = kind == ElementKind::Label ? "LABEL"
                                           : kind == ElementKind::Button ? "BUTTON" : ""};
    elements_.push_back(std::move(element));
    selectedElement_ = static_cast<int>(elements_.size() - 1U);
    dirty_ = true;
}

void UiEditor::deleteSelectedElement() {
    if (selectedElement_ < 0 ||
        selectedElement_ >= static_cast<int>(elements_.size())) return;
    pushUndo();
    elements_.erase(elements_.begin() + selectedElement_);
    selectedElement_ = -1;
    dirty_ = true;
}

void UiEditor::updateTextEditing() {
    if (!textEditing_ || selectedElement_ < 0 ||
        selectedElement_ >= static_cast<int>(elements_.size())) return;
    Element& element = elements_[static_cast<std::size_t>(selectedElement_)];
    bool capturedUndo = false;
    const auto captureUndoOnce = [this, &capturedUndo]() {
        if (capturedUndo) return;
        pushUndo();
        capturedUndo = true;
    };
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        if (codepoint >= 32 && codepoint <= 126 && element.text.size() < 64U) {
            captureUndoOnce();
            element.text.push_back(static_cast<char>(codepoint));
            dirty_ = true;
        }
        codepoint = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !element.text.empty()) {
        captureUndoOnce();
        element.text.pop_back();
        dirty_ = true;
    }
    if (IsKeyPressed(KEY_ENTER)) textEditing_ = false;
}

UiEditor::EditorState UiEditor::captureState() const {
    EditorState state;
    state.assets.reserve(assets_.size());
    for (const Asset& asset : assets_) {
        state.assets.push_back({asset.name, asset.path, asset.left, asset.top,
                                asset.right, asset.bottom, asset.rasterScale});
    }
    state.elements = elements_;
    state.selectedAsset = selectedAsset_;
    state.selectedElement = selectedElement_;
    state.previewWidth = previewWidth_;
    state.previewHeight = previewHeight_;
    return state;
}

void UiEditor::restoreState(const EditorState& state) {
    for (Asset& asset : assets_) {
        if (IsTextureValid(asset.texture)) UnloadTexture(asset.texture);
    }
    assets_.clear();
    assets_.reserve(state.assets.size());
    for (const AssetState& saved : state.assets) {
        Asset asset{.name = saved.name, .path = saved.path,
                    .left = saved.left, .top = saved.top,
                    .right = saved.right, .bottom = saved.bottom,
                    .rasterScale = saved.rasterScale};
        reloadAsset(asset);
        assets_.push_back(std::move(asset));
    }
    elements_ = state.elements;
    selectedAsset_ = assets_.empty()
        ? 0U : std::min(state.selectedAsset, assets_.size() - 1U);
    selectedElement_ = state.selectedElement >= 0 &&
            state.selectedElement < static_cast<int>(elements_.size())
        ? state.selectedElement : -1;
    previewWidth_ = state.previewWidth;
    previewHeight_ = state.previewHeight;
    draggingElement_ = -1;
    resizingElement_ = -1;
    sliceHandle_ = SliceHandle::None;
    resizingPreview_ = false;
    textEditing_ = false;
    dirty_ = true;
}

void UiEditor::pushUndo() {
    if (undoStack_.size() >= UndoHistoryLimit) {
        undoStack_.erase(undoStack_.begin());
    }
    undoStack_.push_back(captureState());
}

void UiEditor::undo() {
    if (undoStack_.empty()) {
        status_ = "NOTHING TO UNDO";
        statusRemaining_ = 1.5;
        return;
    }
    EditorState state = std::move(undoStack_.back());
    undoStack_.pop_back();
    restoreState(state);
    status_ = "UNDO";
    statusRemaining_ = 1.5;
}

void UiEditor::drawAssetList(GameUi& ui, Rectangle bounds) {
    drawEditorPanel(bounds, {224, 232, 238, 252});
    drawUiText("ASSETS", {bounds.x + 14.0F, bounds.y + 12.0F},
               18.0F, {40, 61, 78, 255});
    float y = bounds.y + 46.0F;
    for (std::size_t index = 0; index < assets_.size(); ++index) {
        if (y + 34.0F > bounds.y + bounds.height - 8.0F) break;
        if (ui.drawToggleButton(
                {bounds.x + 10.0F, y, bounds.width - 20.0F, 34.0F},
                assets_[index].name, selectedAsset_ == index)) {
            selectedAsset_ = index;
            if (selectedElement_ >= 0 &&
                selectedElement_ < static_cast<int>(elements_.size()) &&
                elements_[static_cast<std::size_t>(selectedElement_)].kind !=
                    ElementKind::Label) {
                pushUndo();
                elements_[static_cast<std::size_t>(selectedElement_)].asset = index;
                dirty_ = true;
            }
        }
        y += 40.0F;
    }
}

void UiEditor::drawChrome(GameUi& ui) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {19, 25, 34, 255});
    DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(TopHeight),
                  {232, 239, 244, 255});
    drawUiText("UI EDITOR", {18.0F, 20.0F}, 25.0F,
               {36, 55, 70, 255});
    if (ui.drawToggleButton({170.0F, 12.0F, 152.0F, 48.0F},
                            "LAYOUT", mode_ == Mode::Layout)) {
        mode_ = Mode::Layout;
    }
    if (ui.drawToggleButton({332.0F, 12.0F, 180.0F, 48.0F},
                            "9-SLICE", mode_ == Mode::NineSlice)) {
        mode_ = Mode::NineSlice;
    }
    if (ui.drawButton({static_cast<float>(GetScreenWidth()) - 290.0F,
                       12.0F, 130.0F, 48.0F},
                      dirty_ ? "SAVE *" : "SAVE")) {
        saveProject();
        ui.reloadThemeSlices();
    }
    if (ui.drawButton({static_cast<float>(GetScreenWidth()) - 150.0F,
                       12.0F, 130.0F, 48.0F}, "CLOSE")) {
        close();
    }
    if (statusRemaining_ > 0.0) {
        drawCenteredUiText(status_, static_cast<float>(GetScreenHeight()) - 28.0F,
                           13.0F, {255, 215, 92, 255});
        statusRemaining_ = std::max(
            0.0, statusRemaining_ - static_cast<double>(GetFrameTime()));
    }
}

void UiEditor::drawLayout(GameUi& ui) {
    const Rectangle tools{12.0F, TopHeight + 12.0F,
                          LeftWidth - 24.0F,
                          static_cast<float>(GetScreenHeight()) - TopHeight - 24.0F};
    drawEditorPanel(tools, {224, 232, 238, 252});
    drawUiText("ADD", {tools.x + 14.0F, tools.y + 12.0F}, 17.0F,
               {40, 61, 78, 255});
    float toolY = tools.y + 44.0F;
    if (ui.drawButton({tools.x + 10.0F, toolY, tools.width - 20.0F, 38.0F},
                      "+ PANEL")) addElement(ElementKind::Panel);
    toolY += 44.0F;
    if (ui.drawButton({tools.x + 10.0F, toolY, tools.width - 20.0F, 38.0F},
                      "+ BUTTON")) addElement(ElementKind::Button);
    toolY += 44.0F;
    if (ui.drawButton({tools.x + 10.0F, toolY, tools.width - 20.0F, 38.0F},
                      "+ LABEL")) addElement(ElementKind::Label);
    toolY += 56.0F;
    drawUiText("ELEMENTS", {tools.x + 14.0F, toolY}, 17.0F,
               {40, 61, 78, 255});
    toolY += 30.0F;
    for (std::size_t index = 0; index < elements_.size(); ++index) {
        if (toolY + 31.0F > tools.y + tools.height - 54.0F) break;
        if (ui.drawToggleButton(
                {tools.x + 10.0F, toolY, tools.width - 20.0F, 31.0F},
                elements_[index].id,
                selectedElement_ == static_cast<int>(index))) {
            selectedElement_ = static_cast<int>(index);
            selectedAsset_ = elements_[index].asset;
            textEditing_ = false;
        }
        toolY += 35.0F;
    }

    const Rectangle canvas = canvasBounds();
    DrawRectangleRec(canvas, {12, 17, 24, 255});
    DrawRectangleLinesEx(canvas, 2.0F, {105, 132, 151, 255});
    BeginScissorMode(static_cast<int>(canvas.x), static_cast<int>(canvas.y),
                     static_cast<int>(canvas.width), static_cast<int>(canvas.height));
    for (std::size_t index = 0; index < elements_.size(); ++index) {
        const Element& element = elements_[index];
        const Rectangle bounds = designToScreen(element.bounds);
        if (element.kind == ElementKind::Label) {
            const float fontSize = std::max(9.0F, bounds.height * 0.48F);
            const Vector2 size = measureUiText(element.text, fontSize);
            drawUiText(element.text,
                       {bounds.x + (bounds.width - size.x) * 0.5F,
                        bounds.y + (bounds.height - size.y) * 0.5F},
                       fontSize, {235, 241, 245, 255});
        } else if (element.asset < assets_.size()) {
            drawAsset(assets_[element.asset], bounds);
            if (element.kind == ElementKind::Button) {
                const float fontSize = std::max(8.0F, bounds.height * 0.36F);
                const Vector2 size = measureUiText(element.text, fontSize);
                drawUiText(element.text,
                           {bounds.x + (bounds.width - size.x) * 0.5F,
                            bounds.y + (bounds.height - size.y) * 0.46F},
                           fontSize, WHITE);
            }
        }
        if (selectedElement_ == static_cast<int>(index)) {
            DrawRectangleLinesEx(bounds, 2.0F, {75, 235, 174, 255});
            DrawRectangleRec(
                {bounds.x + bounds.width - 10.0F,
                 bounds.y + bounds.height - 10.0F, 12.0F, 12.0F},
                {75, 235, 174, 255});
        }
    }
    EndScissorMode();

    const Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, canvas)) {
        selectedElement_ = -1;
        for (std::size_t reverse = elements_.size(); reverse > 0U; --reverse) {
            const std::size_t index = reverse - 1U;
            const Rectangle bounds = designToScreen(elements_[index].bounds);
            const Rectangle resizeHandle{
                bounds.x + bounds.width - 14.0F,
                bounds.y + bounds.height - 14.0F, 18.0F, 18.0F};
            if (CheckCollisionPointRec(mouse, resizeHandle)) {
                selectedElement_ = static_cast<int>(index);
                pushUndo();
                resizingElement_ = static_cast<int>(index);
                break;
            }
            if (CheckCollisionPointRec(mouse, bounds)) {
                selectedElement_ = static_cast<int>(index);
                pushUndo();
                draggingElement_ = static_cast<int>(index);
                const Vector2 designMouse = screenToDesign(mouse);
                dragOffset_ = {
                    designMouse.x - elements_[index].bounds.x,
                    designMouse.y - elements_[index].bounds.y};
                break;
            }
        }
        if (selectedElement_ >= 0) {
            selectedAsset_ = elements_[static_cast<std::size_t>(selectedElement_)].asset;
        }
        textEditing_ = false;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 designMouse = screenToDesign(mouse);
        if (draggingElement_ >= 0) {
            Element& element = elements_[static_cast<std::size_t>(draggingElement_)];
            element.bounds.x = std::clamp(
                designMouse.x - dragOffset_.x, 0.0F,
                DesignWidth - element.bounds.width);
            element.bounds.y = std::clamp(
                designMouse.y - dragOffset_.y, 0.0F,
                DesignHeight - element.bounds.height);
            dirty_ = true;
        } else if (resizingElement_ >= 0) {
            Element& element = elements_[static_cast<std::size_t>(resizingElement_)];
            element.bounds.width = std::clamp(
                designMouse.x - element.bounds.x, 24.0F,
                DesignWidth - element.bounds.x);
            element.bounds.height = std::clamp(
                designMouse.y - element.bounds.y, 24.0F,
                DesignHeight - element.bounds.y);
            dirty_ = true;
        }
    } else {
        draggingElement_ = -1;
        resizingElement_ = -1;
    }

    const Rectangle inspector{
        static_cast<float>(GetScreenWidth()) - RightWidth - 12.0F,
        TopHeight + 12.0F, RightWidth,
        static_cast<float>(GetScreenHeight()) - TopHeight - 24.0F};
    drawEditorPanel(inspector, {224, 232, 238, 252});
    drawUiText("INSPECTOR", {inspector.x + 14.0F, inspector.y + 12.0F},
               17.0F, {40, 61, 78, 255});
    if (selectedElement_ >= 0 &&
        selectedElement_ < static_cast<int>(elements_.size())) {
        Element& element = elements_[static_cast<std::size_t>(selectedElement_)];
        float y = inspector.y + 48.0F;
        drawUiText(element.id, {inspector.x + 14.0F, y}, 15.0F,
                   {40, 61, 78, 255});
        y += 28.0F;
        drawUiText(TextFormat("X %.0f   Y %.0f", element.bounds.x,
                              element.bounds.y),
                   {inspector.x + 14.0F, y}, 13.0F,
                   {76, 99, 116, 255});
        y += 24.0F;
        drawUiText(TextFormat("W %.0f   H %.0f", element.bounds.width,
                              element.bounds.height),
                   {inspector.x + 14.0F, y}, 13.0F,
                   {76, 99, 116, 255});
        y += 36.0F;
        if (element.kind != ElementKind::Panel &&
            ui.drawToggleButton(
                {inspector.x + 12.0F, y, inspector.width - 24.0F, 40.0F},
                textEditing_ ? element.text + "_" : "EDIT: " + element.text,
                textEditing_)) {
            textEditing_ = !textEditing_;
        }
        if (element.kind != ElementKind::Panel) y += 50.0F;
        if (element.kind != ElementKind::Label && !assets_.empty()) {
            drawUiText(
                "ASSET: " + assets_[element.asset].name,
                {inspector.x + 14.0F, y}, 12.0F,
                {40, 61, 78, 255});
            y += 25.0F;
            if (ui.drawButton(
                    {inspector.x + 12.0F, y, 58.0F, 34.0F}, "<")) {
                pushUndo();
                element.asset = element.asset == 0U
                    ? assets_.size() - 1U : element.asset - 1U;
                selectedAsset_ = element.asset;
                dirty_ = true;
            }
            if (ui.drawButton(
                    {inspector.x + inspector.width - 70.0F, y,
                     58.0F, 34.0F}, ">")) {
                pushUndo();
                element.asset = (element.asset + 1U) % assets_.size();
                selectedAsset_ = element.asset;
                dirty_ = true;
            }
            y += 44.0F;
        }
        if (ui.drawButton(
                {inspector.x + 12.0F, y, inspector.width - 24.0F, 40.0F},
                "DELETE")) deleteSelectedElement();
        y += 58.0F;
        drawUiText("ARROWS MOVE", {inspector.x + 14.0F, y}, 12.0F,
                   {76, 99, 116, 255});
        y += 19.0F;
        drawUiText("ALT + ARROWS RESIZE", {inspector.x + 14.0F, y}, 12.0F,
                   {76, 99, 116, 255});
        y += 19.0F;
        drawUiText("SHIFT = 10 PX", {inspector.x + 14.0F, y}, 12.0F,
                   {76, 99, 116, 255});
    }
    updateTextEditing();
    if (!textEditing_ && selectedElement_ >= 0 &&
        selectedElement_ < static_cast<int>(elements_.size())) {
        Element& element = elements_[static_cast<std::size_t>(selectedElement_)];
        const float step = (IsKeyDown(KEY_LEFT_SHIFT) ||
                            IsKeyDown(KEY_RIGHT_SHIFT)) ? 10.0F : 1.0F;
        const bool resize = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        float dx = 0.0F;
        float dy = 0.0F;
        if (IsKeyPressed(KEY_LEFT)) dx -= step;
        if (IsKeyPressed(KEY_RIGHT)) dx += step;
        if (IsKeyPressed(KEY_UP)) dy -= step;
        if (IsKeyPressed(KEY_DOWN)) dy += step;
        if (dx != 0.0F || dy != 0.0F) {
            pushUndo();
            if (resize) {
                element.bounds.width = std::max(24.0F, element.bounds.width + dx);
                element.bounds.height = std::max(24.0F, element.bounds.height + dy);
            } else {
                element.bounds.x = std::clamp(
                    element.bounds.x + dx, 0.0F,
                    DesignWidth - element.bounds.width);
                element.bounds.y = std::clamp(
                    element.bounds.y + dy, 0.0F,
                    DesignHeight - element.bounds.height);
            }
            dirty_ = true;
        }
        if (IsKeyPressed(KEY_DELETE)) deleteSelectedElement();
    }
}

void UiEditor::drawNineSlice(GameUi& ui) {
    drawAssetList(ui, {12.0F, TopHeight + 12.0F,
                       LeftWidth - 24.0F,
                       static_cast<float>(GetScreenHeight()) - TopHeight - 24.0F});
    if (assets_.empty()) return;
    selectedAsset_ = std::min(selectedAsset_, assets_.size() - 1U);
    Asset& asset = assets_[selectedAsset_];
    const Rectangle sourceArea{
        LeftWidth + 28.0F, TopHeight + 48.0F,
        std::max(200.0F, static_cast<float>(GetScreenWidth()) -
                             LeftWidth - RightWidth - 76.0F),
        static_cast<float>(GetScreenHeight()) - TopHeight - 96.0F};
    drawCheckerboard(sourceArea);
    DrawRectangleLinesEx(sourceArea, 2.0F, {105, 132, 151, 255});
    drawUiText("SOURCE - DRAG GREEN LINES",
               {sourceArea.x, sourceArea.y - 30.0F}, 15.0F,
               {214, 225, 233, 255});
    const Rectangle image = fitRectangle(
        static_cast<float>(std::max(asset.texture.width, 1)),
        static_cast<float>(std::max(asset.texture.height, 1)),
        {sourceArea.x + 34.0F, sourceArea.y + 34.0F,
         sourceArea.width - 68.0F, sourceArea.height - 68.0F});
    if (IsTextureValid(asset.texture)) {
        DrawTexturePro(
            asset.texture,
            {0.0F, 0.0F, static_cast<float>(asset.texture.width),
             static_cast<float>(asset.texture.height)},
            image, {0.0F, 0.0F}, 0.0F, WHITE);
    }
    const float leftX = image.x + image.width *
        static_cast<float>(asset.left) /
        static_cast<float>(std::max(asset.texture.width, 1));
    const float rightX = image.x + image.width *
        static_cast<float>(asset.texture.width - asset.right) /
        static_cast<float>(std::max(asset.texture.width, 1));
    const float topY = image.y + image.height *
        static_cast<float>(asset.top) /
        static_cast<float>(std::max(asset.texture.height, 1));
    const float bottomY = image.y + image.height *
        static_cast<float>(asset.texture.height - asset.bottom) /
        static_cast<float>(std::max(asset.texture.height, 1));
    DrawLineEx({leftX, image.y}, {leftX, image.y + image.height},
               3.0F, {62, 239, 163, 255});
    DrawLineEx({rightX, image.y}, {rightX, image.y + image.height},
               3.0F, {62, 239, 163, 255});
    DrawLineEx({image.x, topY}, {image.x + image.width, topY},
               3.0F, {62, 239, 163, 255});
    DrawLineEx({image.x, bottomY}, {image.x + image.width, bottomY},
               3.0F, {62, 239, 163, 255});

    const Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, image)) {
        struct Candidate { SliceHandle handle; float distance; };
        const Candidate candidates[]{
            {SliceHandle::Left, std::abs(mouse.x - leftX)},
            {SliceHandle::Right, std::abs(mouse.x - rightX)},
            {SliceHandle::Top, std::abs(mouse.y - topY)},
            {SliceHandle::Bottom, std::abs(mouse.y - bottomY)}};
        const Candidate* closest = &candidates[0];
        for (const Candidate& candidate : candidates) {
            if (candidate.distance < closest->distance) closest = &candidate;
        }
        if (closest->distance <= 14.0F) {
            pushUndo();
            sliceHandle_ = closest->handle;
        }
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        sliceHandle_ != SliceHandle::None) {
        const int sourceX = static_cast<int>(std::lround(
            (mouse.x - image.x) / std::max(image.width, 1.0F) *
            static_cast<float>(asset.texture.width)));
        const int sourceY = static_cast<int>(std::lround(
            (mouse.y - image.y) / std::max(image.height, 1.0F) *
            static_cast<float>(asset.texture.height)));
        if (sliceHandle_ == SliceHandle::Left) {
            asset.left = std::clamp(sourceX, 0,
                                    asset.texture.width - asset.right - 1);
        } else if (sliceHandle_ == SliceHandle::Right) {
            asset.right = std::clamp(asset.texture.width - sourceX, 0,
                                     asset.texture.width - asset.left - 1);
        } else if (sliceHandle_ == SliceHandle::Top) {
            asset.top = std::clamp(sourceY, 0,
                                   asset.texture.height - asset.bottom - 1);
        } else if (sliceHandle_ == SliceHandle::Bottom) {
            asset.bottom = std::clamp(asset.texture.height - sourceY, 0,
                                      asset.texture.height - asset.top - 1);
        }
        dirty_ = true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        sliceHandle_ = SliceHandle::None;
    }

    const Rectangle inspector{
        static_cast<float>(GetScreenWidth()) - RightWidth - 12.0F,
        TopHeight + 12.0F, RightWidth,
        static_cast<float>(GetScreenHeight()) - TopHeight - 24.0F};
    drawEditorPanel(inspector, {224, 232, 238, 252});
    drawUiText(asset.name, {inspector.x + 14.0F, inspector.y + 12.0F},
               17.0F, {40, 61, 78, 255});
    drawUiText(TextFormat("SOURCE %d x %d", asset.texture.width,
                          asset.texture.height),
               {inspector.x + 14.0F, inspector.y + 40.0F}, 12.0F,
               {76, 99, 116, 255});
    drawUiText(TextFormat("L %d   T %d", asset.left, asset.top),
               {inspector.x + 14.0F, inspector.y + 70.0F}, 14.0F,
               {40, 61, 78, 255});
    drawUiText(TextFormat("R %d   B %d", asset.right, asset.bottom),
               {inspector.x + 14.0F, inspector.y + 94.0F}, 14.0F,
               {40, 61, 78, 255});
    float previewTitleY = inspector.y + 136.0F;
    if (isSvgPath(asset.path)) {
        drawUiText(TextFormat("VECTOR RASTER  %dX", asset.rasterScale),
                   {inspector.x + 14.0F, inspector.y + 126.0F}, 12.0F,
                   {40, 61, 78, 255});
        const auto changeRasterScale = [this, &asset](int newScale) {
            newScale = std::clamp(newScale, 1, 8);
            if (newScale == asset.rasterScale) return;
            pushUndo();
            const float ratio = static_cast<float>(newScale) /
                                static_cast<float>(asset.rasterScale);
            asset.left = static_cast<int>(
                std::lround(static_cast<float>(asset.left) * ratio));
            asset.top = static_cast<int>(
                std::lround(static_cast<float>(asset.top) * ratio));
            asset.right = static_cast<int>(
                std::lround(static_cast<float>(asset.right) * ratio));
            asset.bottom = static_cast<int>(
                std::lround(static_cast<float>(asset.bottom) * ratio));
            asset.rasterScale = newScale;
            reloadAsset(asset);
            dirty_ = true;
        };
        if (ui.drawButton({inspector.x + inspector.width - 92.0F,
                           inspector.y + 116.0F, 34.0F, 32.0F}, "-")) {
            changeRasterScale(asset.rasterScale - 1);
        }
        if (ui.drawButton({inspector.x + inspector.width - 50.0F,
                           inspector.y + 116.0F, 34.0F, 32.0F}, "+")) {
            changeRasterScale(asset.rasterScale + 1);
        }
        previewTitleY = inspector.y + 166.0F;
    }
    drawUiText("LIVE PREVIEW", {inspector.x + 14.0F, previewTitleY},
               15.0F, {40, 61, 78, 255});
    const Rectangle previewArea{
        inspector.x + 14.0F, previewTitleY + 32.0F,
        inspector.width - 28.0F,
        isSvgPath(asset.path) ? 220.0F : 250.0F};
    drawCheckerboard(previewArea);
    const float previewScale = std::min(
        1.0F, std::min(previewArea.width / previewWidth_,
                       previewArea.height / previewHeight_));
    Rectangle preview{
        previewArea.x + 8.0F, previewArea.y + 8.0F,
        previewWidth_ * previewScale, previewHeight_ * previewScale};
    preview.width = std::min(preview.width, previewArea.width - 16.0F);
    preview.height = std::min(preview.height, previewArea.height - 16.0F);
    drawAsset(asset, preview);
    const Rectangle handle{
        preview.x + preview.width - 9.0F,
        preview.y + preview.height - 9.0F, 14.0F, 14.0F};
    DrawRectangleRec(handle, {62, 239, 163, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, handle)) {
        pushUndo();
        resizingPreview_ = true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) resizingPreview_ = false;
    if (resizingPreview_) {
        previewWidth_ = std::clamp(
            (mouse.x - preview.x) / std::max(previewScale, 0.01F),
            static_cast<float>(asset.left + asset.right + 1), 900.0F);
        previewHeight_ = std::clamp(
            (mouse.y - preview.y) / std::max(previewScale, 0.01F),
            static_cast<float>(asset.top + asset.bottom + 1), 700.0F);
    }
    drawUiText(TextFormat("TARGET %.0f x %.0f", previewWidth_, previewHeight_),
               {inspector.x + 14.0F,
                previewArea.y + previewArea.height + 14.0F}, 13.0F,
               {40, 61, 78, 255});
    drawUiText("DROP PNG / SVG INTO WINDOW",
               {inspector.x + 14.0F, inspector.y + inspector.height - 42.0F},
               10.0F, {76, 99, 116, 255});
}

void UiEditor::draw(GameUi& ui) {
    if (!visible_ || !initialized_) return;
    const bool undoModifierDown = IsKeyDown(KEY_LEFT_CONTROL) ||
                                  IsKeyDown(KEY_RIGHT_CONTROL) ||
                                  IsKeyDown(KEY_LEFT_SUPER) ||
                                  IsKeyDown(KEY_RIGHT_SUPER);
    if (undoModifierDown && IsKeyPressed(KEY_Z)) undo();
    importDroppedFiles();
    drawChrome(ui);
    if (!visible_) return;
    if (mode_ == Mode::Layout) drawLayout(ui);
    else drawNineSlice(ui);
}

} // namespace ian
