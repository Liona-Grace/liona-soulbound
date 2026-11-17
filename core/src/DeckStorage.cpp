#include "soulbound/DeckStorage.hpp"
#include "soulbound/DeckValidator.hpp"
#include "detail/JsonSupport.hpp"
#include <algorithm>
#include <fstream>
#include <random>
#include <stdexcept>
#include <utility>

namespace soulbound {
DeckStorage::DeckStorage(std::filesystem::path directory)
    : directory_(std::filesystem::absolute(std::move(directory))) {}

std::filesystem::path DeckStorage::pathFor(const std::string& id) const {
    if (!DeckValidator::isValidId(id)) throw std::invalid_argument("Invalid deck ID: " + id);
    const auto path = directory_ / (id + ".json");
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path)))
        throw std::runtime_error("Deck files must not be symbolic links: " + path.string());
    return path;
}

std::optional<DeckDefinition> DeckStorage::get(const std::string& id) const {
    const auto path = pathFor(id);
    if (!std::filesystem::exists(path)) return std::nullopt;
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Expected deck file: " + path.string());
    const auto json = detail::parseJson(detail::readJsonFile(path, "deck"));
    detail::requireFields(json, {"id", "name", "cards"}, "deck");
    if (!json.at("id").is_string() || !json.at("name").is_string() || !json.at("cards").is_array())
        throw std::runtime_error("Invalid deck JSON types: " + path.string());
    DeckDefinition deck{json.at("id").get<std::string>(), json.at("name").get<std::string>(), {}};
    if (deck.id != id) throw std::runtime_error("Deck ID does not match filename: " + path.string());
    for (const auto& entry : json.at("cards")) {
        detail::requireFields(entry, {"cardId", "count"}, "deck entry");
        if (!entry.at("cardId").is_string()) throw std::runtime_error("cardId must be a string");
        deck.cards.push_back({entry.at("cardId").get<std::string>(), detail::jsonInteger<std::int64_t>(entry.at("count"))});
    }
    return deck;
}

std::vector<DeckDefinition> DeckStorage::getAll() const {
    std::vector<DeckDefinition> decks;
    if (!std::filesystem::exists(directory_)) return decks;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (entry.path().extension() != ".json") continue;
        const auto deck = get(entry.path().stem().string());
        if (!deck) throw std::runtime_error("Deck disappeared while reading storage");
        decks.push_back(*deck);
    }
    std::sort(decks.begin(), decks.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return decks;
}

void DeckStorage::save(const DeckDefinition& deck) const {
    const auto target = pathFor(deck.id);
    nlohmann::json json{{"id", deck.id}, {"name", deck.name}, {"cards", nlohmann::json::array()}};
    for (const auto& entry : deck.cards)
        json["cards"].push_back({{"cardId", entry.cardId}, {"count", entry.count}});
    const auto text = json.dump(2) + '\n';
    std::filesystem::create_directories(directory_);
    struct TemporaryDirectory {
        std::filesystem::path path;
        ~TemporaryDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    };
    std::filesystem::path staging;
    do {
        staging = directory_ / (".deck-write-" + std::to_string(std::random_device{}()));
    } while (!std::filesystem::create_directory(staging));
    const TemporaryDirectory cleanup{staging};
    const auto temporary = staging / "deck.json";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Cannot write deck: " + deck.id);
    output << text;
    output.close();
    if (!output) throw std::runtime_error("Cannot finish writing deck: " + deck.id);
    std::filesystem::rename(temporary, target);
}

bool DeckStorage::remove(const std::string& id) const {
    const auto path = pathFor(id);
    if (!std::filesystem::exists(path)) return false;
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Expected deck file: " + path.string());
    return std::filesystem::remove(path);
}
}
