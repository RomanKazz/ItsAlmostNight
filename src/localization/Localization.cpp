#include "localization/Localization.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

namespace ian {
namespace {

using Json = nlohmann::json;

Language language = Language::English;
std::unordered_map<std::string, std::string> russianCatalog;
std::vector<std::pair<std::string_view, std::string_view>>
    russianFragments;

void replaceAll(std::string& text, std::string_view from,
                std::string_view to) {
    if (from.empty()) {
        return;
    }
    std::size_t position = 0;
    while ((position = text.find(from, position)) !=
           std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
}

bool isAsciiWordCharacter(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
           value == '_';
}

void replaceCatalogFragments(std::string& text) {
    for (const auto [from, to] : russianFragments) {
        std::size_t position = 0;
        while ((position = text.find(from, position)) !=
               std::string::npos) {
            const bool leftBoundary =
                position == 0 || !isAsciiWordCharacter(
                    from.front()) ||
                !isAsciiWordCharacter(text[position - 1]);
            const std::size_t end = position + from.size();
            const bool rightBoundary =
                end >= text.size() || !isAsciiWordCharacter(
                    from.back()) ||
                !isAsciiWordCharacter(text[end]);
            if (leftBoundary && rightBoundary) {
                text.replace(position, from.size(), to);
                position += to.size();
            } else {
                position = end;
            }
        }
    }
}

void translateDynamicHudText(std::string& text) {
    // These replacements cover strings assembled with counters, costs and
    // key bindings. Fixed labels are kept in the JSON catalog below.
    constexpr std::pair<std::string_view, std::string_view> Replacements[]{
        {"OBJECTIVE:", "ЦЕЛЬ:"},
        {"Gather by hand", "Собрать вручную"},
        {"Mine trees", "Добыть дерево"},
        {"Mine rocks", "Добыть камень"},
        {"Build Crystal Mine", "Построить кристальную шахту"},
        {"Build defenses", "Построить оборону"},
        {"Survive first night", "Пережить первую ночь"},
        {"Place Core", "Построить Ядро"},
        {"Wood", "Дерево"},
        {"Stone", "Камень"},
        {"CRYSTALS", "КРИСТАЛЛОВ"},
        {"HOSTILES", "ВРАГОВ"},
        {"ACTIVE", "АКТИВНЫХ"},
        {"BOSS RAM INCOMING", "АТАКА БОССА"},
        {"COLLAPSE RISK:", "РИСК ОБРУШЕНИЯ:"},
        {"DEPENDENT PARTS", "ЗАВИСИМЫХ ЧАСТЕЙ"},
        {"RELEASE X TO CONFIRM", "ОТПУСТИТЕ X ДЛЯ ПОДТВЕРЖДЕНИЯ"},
        {"FPS LIMIT: UNLIMITED", "ЛИМИТ FPS: БЕЗ ОГРАНИЧЕНИЙ"},
        {"UNLIMITED", "БЕЗ ОГРАНИЧЕНИЙ"},
        {"CORE L", "ЯДРО Ур. "},
        {"YOU  ", "ЗДОРОВЬЕ  "},
        {"WAVE", "ВОЛНА"},
        {"BEST", "ЛУЧШИЙ"},
        {"TACTICAL MAP", "ТАКТИЧЕСКАЯ КАРТА"},
        {"MAP", "КАРТА"},
        {"Window: ", "Окно: "},
        {"Render: ", "Рендер: "},
        {"PIXEL: ", "ПИКСЕЛИ: "},
        {"MOUSE SENSITIVITY", "ЧУВСТВИТЕЛЬНОСТЬ МЫШИ"},
        {"MASTER VOLUME", "ОБЩАЯ ГРОМКОСТЬ"},
        {"SOUND EFFECTS", "ЗВУКОВЫЕ ЭФФЕКТЫ"},
        {"POSITION ", "ПОЗИЦИЯ "},
        {"ROTATION ", "ВРАЩЕНИЕ "},
        {"PAGE: ", "СТРАНИЦА: "},
        {"MODEL: ", "МОДЕЛЬ: "},
        {"LEVEL ", "УРОВЕНЬ "},
        {"WAVE ", "ВОЛНА "},
        {"BEST ", "ЛУЧШИЙ "},
        {"SUNSET", "ЗАКАТ"},
        {"TWILIGHT", "СУМЕРКИ"},
        {"BUILD / REPAIR", "СТРОЙКА / РЕМОНТ"},
        {"DAWN", "РАССВЕТ"},
        {"DAY IN", "ДЕНЬ ЧЕРЕЗ"},
        {"IN ", "ЧЕРЕЗ "},
        {"ATTACK FROM", "АТАКА С"},
        {"NORTH", "СЕВЕРА"},
        {"EAST", "ВОСТОКА"},
        {"SOUTH", "ЮГА"},
        {"WEST", "ЗАПАДА"},
        {"REPAIR MODE", "РЕЖИМ РЕМОНТА"},
        {" upgraded", " улучшено"},
        {" repaired", " отремонтировано"},
        {" sold", " продано"},
        {" acquired", " получено"},
        {"REMOVE x", "УДАЛИТЬ x"},
        {"  ON", "  ВКЛ"},
        {"  OFF", "  ВЫКЛ"},
        {": ON", ": ВКЛ"},
        {": OFF", ": ВЫКЛ"},
        {"UNLOCKED", "ОТКРЫТО"},
        {"AVAILABLE", "ДОСТУПНО"},
        {"LOCKED", "ЗАБЛОКИРОВАНО"},
        {"COST", "ЦЕНА"},
        {"LEVEL UP", "НОВЫЙ УРОВЕНЬ"},
        {"NEXT POINT", "ДО СЛЕДУЮЩЕГО ОЧКА"},
        {"FREE", "БЕСПЛАТНО"},
    };
    for (const auto [from, to] : Replacements) {
        replaceAll(text, from, to);
    }
}

} // namespace

void initializeLocalization(std::string_view catalogPath) {
    russianCatalog.clear();
    russianFragments.clear();
    try {
        std::ifstream stream{std::string(catalogPath)};
        if (!stream) {
            return;
        }
        const Json document = Json::parse(stream);
        const auto languages = document.find("languages");
        if (languages == document.end() || !languages->is_object()) {
            return;
        }
        const auto russian = languages->find("ru");
        if (russian == languages->end() || !russian->is_object()) {
            return;
        }
        for (const auto& [source, value] : russian->items()) {
            if (value.is_string()) {
                russianCatalog.emplace(source, value.get<std::string>());
            }
        }
        russianFragments.reserve(russianCatalog.size());
        for (const auto& [source, value] : russianCatalog) {
            if (source.size() >= 3) {
                russianFragments.emplace_back(source, value);
            }
        }
        std::sort(
            russianFragments.begin(), russianFragments.end(),
            [](const auto& left, const auto& right) {
                return left.first.size() > right.first.size();
            });
    } catch (...) {
        russianCatalog.clear();
    }
}

void setLanguage(Language selected) {
    language = selected;
}

Language currentLanguage() {
    return language;
}

std::string_view languageCode(Language selected) {
    return selected == Language::Russian ? "ru" : "en";
}

std::string_view languageName(Language selected) {
    return selected == Language::Russian ? "RUSSIAN" : "ENGLISH";
}

std::string localizeText(std::string_view text) {
    if (language == Language::English) {
        return std::string{text};
    }
    const auto translation = russianCatalog.find(std::string{text});
    if (translation != russianCatalog.end()) {
        return translation->second;
    }
    std::string result{text};
    const bool hasRuntimeValue =
        result.find_first_of("0123456789:%\n•") !=
            std::string::npos ||
        result.find(" upgraded") != std::string::npos ||
        result.find(" repaired") != std::string::npos ||
        result.find(" sold") != std::string::npos ||
        result.find(" acquired") != std::string::npos;
    if (hasRuntimeValue) {
        // Standalone section titles use nominative forms, while counters need
        // the genitive form before generic catalog fragments are applied.
        replaceAll(result, " BUILDINGS", " ЗДАНИЙ");
        replaceAll(result, " PIECES", " ДЕТАЛЕЙ");
    }
    // Runtime labels are often composed from independently translated parts
    // (key binding + action, building name + state, and so on). Apply catalog
    // fragments for fixed composed strings too; exact translations above
    // still take priority and preserve natural phrasing.
    replaceCatalogFragments(result);
    if (hasRuntimeValue) {
        translateDynamicHudText(result);
    }
    return result;
}

} // namespace ian
