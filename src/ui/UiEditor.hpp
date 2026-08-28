#pragma once

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace ian {

class GameUi;

class UiEditor {
  public:
    UiEditor() = default;
    ~UiEditor();

    UiEditor(const UiEditor&) = delete;
    UiEditor& operator=(const UiEditor&) = delete;

    void initialize();
    void shutdown();
    void toggle();
    void close();
    [[nodiscard]] bool visible() const;
    void draw(GameUi& ui);

  private:
    enum class Mode { Layout, NineSlice };
    enum class ElementKind { Panel, Button, Label };
    enum class SliceHandle { None, Left, Top, Right, Bottom };

    struct Asset {
        std::string name;
        std::string path;
        Texture2D texture{};
        int left{};
        int top{};
        int right{};
        int bottom{};
        int rasterScale{1};
    };

    struct Element {
        std::string id;
        ElementKind kind{ElementKind::Button};
        Rectangle bounds{};
        std::size_t asset{};
        std::string text;
    };

    struct AssetState {
        std::string name;
        std::string path;
        int left{};
        int top{};
        int right{};
        int bottom{};
        int rasterScale{1};
    };

    struct EditorState {
        std::vector<AssetState> assets;
        std::vector<Element> elements;
        std::size_t selectedAsset{};
        int selectedElement{-1};
        float previewWidth{};
        float previewHeight{};
    };

    void addDefaultAssets();
    void addDefaultLayout();
    void loadProject();
    void saveProject();
    void reloadAsset(Asset& asset);
    void importDroppedFiles();
    void drawChrome(GameUi& ui);
    void drawLayout(GameUi& ui);
    void drawNineSlice(GameUi& ui);
    void drawAssetList(GameUi& ui, Rectangle bounds);
    void updateTextEditing();
    void addElement(ElementKind kind);
    void deleteSelectedElement();
    [[nodiscard]] EditorState captureState() const;
    void restoreState(const EditorState& state);
    void pushUndo();
    void undo();
    [[nodiscard]] Rectangle canvasBounds() const;
    [[nodiscard]] Rectangle designToScreen(Rectangle bounds) const;
    [[nodiscard]] Vector2 screenToDesign(Vector2 point) const;
    [[nodiscard]] NPatchInfo patchFor(const Asset& asset) const;
    void drawAsset(const Asset& asset, Rectangle bounds,
                   Color tint = WHITE) const;

    std::vector<Asset> assets_;
    std::vector<Element> elements_;
    Mode mode_{Mode::Layout};
    bool visible_{};
    bool initialized_{};
    bool dirty_{};
    bool textEditing_{};
    std::size_t selectedAsset_{};
    int selectedElement_{-1};
    int draggingElement_{-1};
    int resizingElement_{-1};
    Vector2 dragOffset_{};
    SliceHandle sliceHandle_{SliceHandle::None};
    bool resizingPreview_{};
    float previewWidth_{360.0F};
    float previewHeight_{150.0F};
    std::vector<EditorState> undoStack_;
    std::string status_;
    double statusRemaining_{};
};

} // namespace ian
