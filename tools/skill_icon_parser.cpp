#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int SheetSize = 1254;
constexpr int GridSize = 3;
constexpr int CellSize = SheetSize / GridSize;
constexpr int OutputSize = 256;
constexpr float ContentFraction = 0.78F;
constexpr int TransparentLuminance = 12;
constexpr int OpaqueLuminance = 210;
constexpr Color IconColor{244, 240, 223, 255};

constexpr std::array<std::string_view, 9> DefaultNames{
    "bare_hands", "axe", "pickaxe",
    "efficient_strikes", "power_swing", "lumber_mill",
    "quarry", "crystal_mine", "night_shift",
};

int luminance(Color color) {
    return (54 * static_cast<int>(color.r) +
            183 * static_cast<int>(color.g) +
            19 * static_cast<int>(color.b)) /
           256;
}

unsigned char maskAlpha(Color color) {
    const float linear = std::clamp(
        static_cast<float>(luminance(color) - TransparentLuminance) /
            static_cast<float>(OpaqueLuminance - TransparentLuminance),
        0.0F, 1.0F);
    const float smooth = linear * linear * (3.0F - 2.0F * linear);
    return static_cast<unsigned char>(std::lround(smooth * 255.0F));
}

bool validName(std::string_view name) {
    if (name.empty()) return false;
    return std::ranges::all_of(name, [](char value) {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9') ||
               value == '_' || value == '-';
    });
}

struct Bounds {
    int minX{CellSize};
    int minY{CellSize};
    int maxX{-1};
    int maxY{-1};
    double weightedX{};
    double weightedY{};
    double weight{};
};

using CellMask = std::vector<unsigned char>;

CellMask makeCellMask(const Color* pixels, int sheetWidth,
                      int cellX, int cellY) {
    CellMask mask(static_cast<std::size_t>(CellSize * CellSize));
    for (int y = 0; y < CellSize; ++y) {
        for (int x = 0; x < CellSize; ++x) {
            mask[static_cast<std::size_t>(y * CellSize + x)] =
                maskAlpha(pixels[
                    static_cast<std::size_t>(cellY + y) *
                        static_cast<std::size_t>(sheetWidth) +
                    static_cast<std::size_t>(cellX + x)]);
        }
    }

    std::vector<unsigned char> visited(mask.size());
    struct Component {
        std::vector<int> pixels;
        bool touchesEdge{};
    };
    std::vector<Component> components;
    constexpr std::array<int, 8> Dx{-1, 0, 1, -1, 1, -1, 0, 1};
    constexpr std::array<int, 8> Dy{-1, -1, -1, 0, 0, 1, 1, 1};
    for (int start = 0; start < CellSize * CellSize; ++start) {
        if (visited[static_cast<std::size_t>(start)] != 0 ||
            mask[static_cast<std::size_t>(start)] < 12) {
            continue;
        }
        Component component;
        std::vector<int> pending{start};
        visited[static_cast<std::size_t>(start)] = 1;
        while (!pending.empty()) {
            const int current = pending.back();
            pending.pop_back();
            component.pixels.push_back(current);
            const int x = current % CellSize;
            const int y = current / CellSize;
            component.touchesEdge = component.touchesEdge ||
                x == 0 || y == 0 || x == CellSize - 1 || y == CellSize - 1;
            for (std::size_t direction = 0; direction < Dx.size(); ++direction) {
                const int nextX = x + Dx[direction];
                const int nextY = y + Dy[direction];
                if (nextX < 0 || nextY < 0 ||
                    nextX >= CellSize || nextY >= CellSize) continue;
                const int next = nextY * CellSize + nextX;
                if (visited[static_cast<std::size_t>(next)] != 0 ||
                    mask[static_cast<std::size_t>(next)] < 12) continue;
                visited[static_cast<std::size_t>(next)] = 1;
                pending.push_back(next);
            }
        }
        components.push_back(std::move(component));
    }

    std::size_t largestArea = 0;
    for (const Component& component : components) {
        largestArea = std::max(largestArea, component.pixels.size());
    }
    for (const Component& component : components) {
        const bool jpegSpeck = component.pixels.size() <
            std::max<std::size_t>(8, largestArea / 1000);
        const bool neighboringCellLeak = component.touchesEdge &&
            component.pixels.size() < largestArea / 8;
        if (!jpegSpeck && !neighboringCellLeak) continue;
        for (const int pixel : component.pixels) {
            mask[static_cast<std::size_t>(pixel)] = 0;
        }
    }
    return mask;
}

Bounds findBounds(const CellMask& mask) {
    Bounds bounds;
    for (int y = 0; y < CellSize; ++y) {
        for (int x = 0; x < CellSize; ++x) {
            const unsigned char alpha =
                mask[static_cast<std::size_t>(y * CellSize + x)];
            if (alpha < 12) continue;
            bounds.minX = std::min(bounds.minX, x);
            bounds.minY = std::min(bounds.minY, y);
            bounds.maxX = std::max(bounds.maxX, x);
            bounds.maxY = std::max(bounds.maxY, y);
            const double weight = static_cast<double>(alpha) / 255.0;
            bounds.weightedX += (static_cast<double>(x) + 0.5) * weight;
            bounds.weightedY += (static_cast<double>(y) + 0.5) * weight;
            bounds.weight += weight;
        }
    }
    if (bounds.maxX >= bounds.minX && bounds.maxY >= bounds.minY) {
        constexpr int SourcePadding = 3;
        bounds.minX = std::max(0, bounds.minX - SourcePadding);
        bounds.minY = std::max(0, bounds.minY - SourcePadding);
        bounds.maxX = std::min(CellSize - 1, bounds.maxX + SourcePadding);
        bounds.maxY = std::min(CellSize - 1, bounds.maxY + SourcePadding);
    }
    return bounds;
}

Image makeIcon(const CellMask& mask, const Bounds& bounds) {
    const int sourceWidth = bounds.maxX - bounds.minX + 1;
    const int sourceHeight = bounds.maxY - bounds.minY + 1;
    auto* sourcePixels = static_cast<Color*>(MemAlloc(
        static_cast<unsigned int>(
            static_cast<std::size_t>(sourceWidth) *
            static_cast<std::size_t>(sourceHeight) * sizeof(Color))));
    for (int y = 0; y < sourceHeight; ++y) {
        for (int x = 0; x < sourceWidth; ++x) {
            const unsigned char alpha = mask[
                static_cast<std::size_t>(bounds.minY + y) *
                    static_cast<std::size_t>(CellSize) +
                static_cast<std::size_t>(bounds.minX + x)];
            sourcePixels[static_cast<std::size_t>(y) *
                             static_cast<std::size_t>(sourceWidth) +
                         static_cast<std::size_t>(x)] =
                {IconColor.r, IconColor.g, IconColor.b, alpha};
        }
    }
    Image source{
        sourcePixels, sourceWidth, sourceHeight, 1,
        PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

    const float maximumContent =
        static_cast<float>(OutputSize) * ContentFraction;
    const float scale = std::min(
        maximumContent / static_cast<float>(sourceWidth),
        maximumContent / static_cast<float>(sourceHeight));
    const int resizedWidth = std::max(
        1, static_cast<int>(std::lround(
               static_cast<float>(sourceWidth) * scale)));
    const int resizedHeight = std::max(
        1, static_cast<int>(std::lround(
               static_cast<float>(sourceHeight) * scale)));
    ImageResize(&source, resizedWidth, resizedHeight);

    const double centroidX = bounds.weightedX / bounds.weight;
    const double centroidY = bounds.weightedY / bounds.weight;
    const float centroidRatioX = static_cast<float>(
        (centroidX - static_cast<double>(bounds.minX)) /
        static_cast<double>(sourceWidth));
    const float centroidRatioY = static_cast<float>(
        (centroidY - static_cast<double>(bounds.minY)) /
        static_cast<double>(sourceHeight));
    constexpr int SafeInset = 12;
    int destinationX = static_cast<int>(std::lround(
        OutputSize * 0.5F -
        centroidRatioX * static_cast<float>(resizedWidth)));
    int destinationY = static_cast<int>(std::lround(
        OutputSize * 0.5F -
        centroidRatioY * static_cast<float>(resizedHeight)));
    destinationX = std::clamp(
        destinationX, SafeInset,
        std::max(SafeInset, OutputSize - SafeInset - resizedWidth));
    destinationY = std::clamp(
        destinationY, SafeInset,
        std::max(SafeInset, OutputSize - SafeInset - resizedHeight));

    Image output = GenImageColor(OutputSize, OutputSize, BLANK);
    ImageDraw(&output, source,
              {0.0F, 0.0F, static_cast<float>(source.width),
               static_cast<float>(source.height)},
              {static_cast<float>(destinationX),
               static_cast<float>(destinationY),
               static_cast<float>(source.width),
               static_cast<float>(source.height)},
              WHITE);
    UnloadImage(source);
    return output;
}

void printUsage(const char* executable) {
    std::cerr
        << "Usage: " << executable
        << " <1254x1254-sheet> [output-directory] [nine icon names]\n"
        << "Default output: assets/ui/skill_icons\n"
        << "Default names: bare_hands axe pickaxe efficient_strikes "
           "power_swing lumber_mill quarry crystal_mine night_shift\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3 && argc != 12) {
        printUsage(argv[0]);
        return 2;
    }

    const std::filesystem::path inputPath = argv[1];
    const std::filesystem::path outputDirectory =
        argc >= 3 ? argv[2] : "assets/ui/skill_icons";
    std::array<std::string, 9> names;
    for (std::size_t index = 0; index < names.size(); ++index) {
        names[index] = argc == 12
            ? argv[static_cast<int>(index) + 3]
            : std::string{DefaultNames[index]};
        if (!validName(names[index])) {
            std::cerr << "Invalid icon name: " << names[index] << '\n';
            return 2;
        }
    }

    Image sheet = LoadImage(inputPath.string().c_str());
    if (!IsImageValid(sheet)) {
        std::cerr << "Could not load image: " << inputPath << '\n';
        return 1;
    }
    if (sheet.width != SheetSize || sheet.height != SheetSize) {
        std::cerr << "Expected exactly 1254x1254 pixels, got "
                  << sheet.width << 'x' << sheet.height << '\n';
        UnloadImage(sheet);
        return 1;
    }

    std::error_code directoryError;
    std::filesystem::create_directories(outputDirectory, directoryError);
    if (directoryError) {
        std::cerr << "Could not create output directory: "
                  << directoryError.message() << '\n';
        UnloadImage(sheet);
        return 1;
    }

    Color* pixels = LoadImageColors(sheet);
    bool success = pixels != nullptr;
    for (int row = 0; success && row < GridSize; ++row) {
        for (int column = 0; success && column < GridSize; ++column) {
            const std::size_t index = static_cast<std::size_t>(
                row * GridSize + column);
            const int cellX = column * CellSize;
            const int cellY = row * CellSize;
            const CellMask mask = makeCellMask(
                pixels, sheet.width, cellX, cellY);
            const Bounds bounds = findBounds(mask);
            if (bounds.maxX < bounds.minX ||
                bounds.maxY < bounds.minY || bounds.weight <= 0.0) {
                std::cerr << "No icon found in cell " << (index + 1) << '\n';
                success = false;
                break;
            }
            Image icon = makeIcon(mask, bounds);
            const std::filesystem::path destination =
                outputDirectory / (names[index] + ".png");
            if (!ExportImage(icon, destination.string().c_str())) {
                std::cerr << "Could not write: " << destination << '\n';
                success = false;
            } else {
                std::cout << destination.string() << '\n';
            }
            UnloadImage(icon);
        }
    }

    if (pixels != nullptr) UnloadImageColors(pixels);
    UnloadImage(sheet);
    return success ? 0 : 1;
}
