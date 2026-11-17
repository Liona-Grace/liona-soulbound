#include "soulbound/JsonLoader.hpp"
#include "detail/JsonSupport.hpp"

#include <stdexcept>
#include <utility>

namespace soulbound {
namespace {

constexpr std::string_view IdKey = "id";
constexpr std::string_view NameKey = "name";
constexpr std::string_view AttackKey = "atk";
constexpr std::string_view HPKey = "hp";
constexpr std::string_view CardLabel = "card";
constexpr std::string_view StartingHPKey = "startingHP";
constexpr std::string_view StartingHandSizeKey = "startingHandSize";
constexpr std::string_view BoardSizeKey = "boardSize";
constexpr std::string_view DeckSizeKey = "deckSize";

GameConfig parseGameConfig(const nlohmann::json& json) {
    // Existing four-field files retain the default copy limit of three.
    auto fields = json;
    if (fields.is_object() && !fields.contains("maxCopiesPerCard")) fields["maxCopiesPerCard"] = GameConfig{}.maxCopiesPerCard;
    detail::requireFields(fields, {StartingHPKey, StartingHandSizeKey, BoardSizeKey, DeckSizeKey, "maxCopiesPerCard"}, "game config");
    GameConfig config{
        detail::jsonInteger<int>(fields.at(StartingHPKey)),
        detail::jsonInteger<std::size_t>(fields.at(StartingHandSizeKey)),
        detail::jsonInteger<std::size_t>(fields.at(BoardSizeKey)),
        detail::jsonInteger<std::size_t>(fields.at(DeckSizeKey)),
        detail::jsonInteger<std::size_t>(fields.at("maxCopiesPerCard"))
    };
    if (!config.isValid()) throw std::runtime_error("Invalid game config values");
    return config;
}

CardDatabase parseCards(const nlohmann::json& json) {
    detail::requireFields(json, {"cards"}, "card database");
    const auto& cards = json.at("cards");
    if (!cards.is_array()) throw std::runtime_error("cards must be an array");

    CardDatabase database;
    for (const auto& card : cards) {
        detail::requireFields(card, {IdKey, NameKey, AttackKey, HPKey}, CardLabel);
        if (!card.at(IdKey).is_string() || !card.at(NameKey).is_string())
            throw std::runtime_error("Card id and name must be strings");
        CardDefinition definition{
            card.at(IdKey).get<std::string>(),
            card.at(NameKey).get<std::string>(),
            detail::jsonInteger<int>(card.at(AttackKey)),
            detail::jsonInteger<int>(card.at(HPKey))
        };
        if (!database.add(std::move(definition)))
            throw std::runtime_error("Invalid or duplicate card definition");
    }
    return database;
}

} // namespace

GameConfig JsonLoader::loadGameConfig(const std::filesystem::path& path) {
    return parseGameConfig(detail::parseJson(detail::readJsonFile(path, "game config")));
}

CardDatabase JsonLoader::loadCards(const std::filesystem::path& path) {
    return parseCards(detail::parseJson(detail::readJsonFile(path, CardLabel)));
}

} // namespace soulbound
