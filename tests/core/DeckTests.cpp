#include "soulbound/DeckManager.hpp"
#include "soulbound/Game.hpp"
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace soulbound;
#define CHECK(expression) do { if (!(expression)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expression); } while (false)

template<class Exception = std::exception, class Action>
void throws(Action action) {
    bool caught = false;
    try { action(); } catch (const Exception&) { caught = true; }
    CHECK(caught);
}
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        do {
            path = std::filesystem::temp_directory_path() /
                ("soulbound-decks-test-" + std::to_string(std::random_device{}()));
        } while (!std::filesystem::create_directory(path));
    }
    ~TemporaryDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path); output << text; output.close(); CHECK(output.good());
}
CardDatabase cards() { return CardDatabase::fromFile(CARD_DATA_PATH); }
DeckDefinition deck(std::string id = "sample", std::int64_t count = 6) {
    if (count == 6) return {std::move(id), "Sample Deck", {{"knight", 3}, {"guard", 3}}};
    return {std::move(id), "Sample Deck", {{"knight", count}}};
}

void testCatalog() {
    auto database = cards();
    auto all = database.getAll();
    CHECK(all.size() == 2);
    CHECK(all[0].id == "guard" && all[1].id == "knight");
    CHECK(all[1].attack == 5 && all[1].hp == 8);
    all[1].name = "changed";
    CHECK(database.find("knight")->name == "Knight");
    throws<std::runtime_error>([] { (void)CardDatabase::fromFile("missing-deck-test-cards.json"); });
}

void testValidation() {
    const auto database = cards();
    GameConfig config; config.deckSize = 6;
    auto validate = [&](const DeckDefinition& value, DeckValidationMode mode = DeckValidationMode::Storage) {
        return DeckValidator::validate(value, database, config, mode);
    };
    CHECK(validate(deck()).isValid());
    CHECK(validate(deck(), DeckValidationMode::Play).isValid()); // Three copies of each ID.
    CHECK(validate(deck("draft", 1)).isValid());
    CHECK(validate(deck("draft", 1), DeckValidationMode::Play).error == DeckValidationError::IncompleteDeck);
    CHECK(validate({"empty", "Empty", {}}).isValid());
    CHECK(validate({"empty", "Empty", {}}, DeckValidationMode::Play).error == DeckValidationError::IncompleteDeck);
    for (const auto count : {std::int64_t{0}, std::int64_t{-1}, std::numeric_limits<std::int64_t>::min()})
        CHECK(validate(deck("sample", count)).error == DeckValidationError::InvalidCount);
    CHECK(validate(deck("sample", 31)).error == DeckValidationError::TooManyCards);
    CHECK(validate(deck("sample", std::numeric_limits<std::int64_t>::max())).error == DeckValidationError::TooManyCards);
    auto value = deck(); value.cards.push_back({"guard", 1});
    CHECK(validate(value).error == DeckValidationError::TooManyCards);
    value = deck(); value.cards[0].cardId = "missing";
    CHECK(validate(value).error == DeckValidationError::UnknownCard);
    value = deck(); value.name = " \t\r\n";
    CHECK(validate(value).error == DeckValidationError::InvalidName);
    for (const auto* id : {"", "../escape", "/absolute", "a/b", "a\\b", ".", "a.json"})
        CHECK(validate(deck(id)).error == DeckValidationError::InvalidId);
    value = {"duplicate-entries", "Repeated entries", {{"knight", 1}, {"knight", 2}, {"guard", 3}}};
    CHECK(validate(value, DeckValidationMode::Play).isValid());
    value.cards.push_back({"knight", 1});
    CHECK(validate(value).error == DeckValidationError::TooManyCards);
    GameConfig small{20, 1, 2, 3};
    CHECK(DeckValidator::validate(deck("small", 3), database, small, DeckValidationMode::Play).isValid());
    CHECK(DeckValidator::validate(deck("small", 4), database, small).error == DeckValidationError::TooManyCards);
    throws<std::invalid_argument>([&] { (void)DeckValidator::validate(deck(), database, {0, 4, 6, 30}); });
}

void testCrud() {
    TemporaryDirectory temporary;
    const auto root = temporary.path / "decks";
    GameConfig config; config.deckSize = 6;
    DeckManager manager(cards(), config, root);
    CHECK(manager.getAll().empty());
    CHECK(!manager.get("missing"));
    CHECK(!manager.remove("missing"));
    auto draft = deck("draft", 2);
    draft.name = "Bộ bài thử \"Knight\"\n";
    manager.create(draft);
    CHECK(std::filesystem::is_regular_file(root / "draft.json"));
    CHECK(manager.get("draft") == draft);
    CHECK(!manager.validate(draft, DeckValidationMode::Play).isValid());
    const DeckManager reopened(cards(), config, root);
    CHECK(reopened.get("draft") == draft);
    throws<std::invalid_argument>([&] { manager.create(draft); });
    auto invalid = draft; invalid.cards[0].count = 31;
    throws<std::invalid_argument>([&] { manager.update(invalid); });
    CHECK(manager.get("draft") == draft);
    invalid.cards[0] = {"unknown", 1};
    throws<std::invalid_argument>([&] { manager.create(invalid); });
    CHECK(manager.get("draft") == draft);
    draft.cards = {{"knight", 3}, {"guard", 3}}; draft.name = "Completed";
    manager.update(draft);
    CHECK(reopened.get("draft") == draft);
    CHECK(manager.validate(draft, DeckValidationMode::Play).isValid());
    throws<std::out_of_range>([&] { manager.update(deck("absent")); });
    manager.create({"a-empty", "Empty draft", {}});
    CHECK(manager.getAll().size() == 2);
    CHECK(manager.getAll()[0].id == "a-empty");
    CHECK(manager.remove("draft")); CHECK(!manager.get("draft"));
    CHECK(!manager.remove("draft"));
    throws<std::invalid_argument>([&] { (void)manager.get("../escape"); });
    throws<std::invalid_argument>([&] { (void)manager.remove("../escape"); });
    throws<std::invalid_argument>([&] { manager.create(deck("../escape")); });
    CHECK(!std::filesystem::exists(temporary.path / "escape.json"));
    for (const auto& entry : std::filesystem::directory_iterator(root))
        CHECK(entry.path().extension() == ".json"); // Staging files cleaned up.
}

void testCorruptStorage() {
    TemporaryDirectory temporary;
    DeckManager manager(cards(), {}, temporary.path);
    const std::vector<std::string> corrupt = {
        "", "{", "null", "[]", "{}",
        R"({"id":"x","name":"X","cards":{}})",
        R"({"id":"x","id":"x","name":"X","cards":[]})",
        R"({"id":"wrong","name":"X","cards":[]})",
        R"({"id":"x","name":5,"cards":[]})",
        R"({"id":"x","name":"X","cards":[],"extra":1})",
        R"({"id":"x","name":"X","cards":[{}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":5,"count":1}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":1.5}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":true}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":"1"}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":18446744073709551615}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":-1}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":0}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"knight","count":31}]})",
        R"({"id":"x","name":"X","cards":[{"cardId":"unknown","count":1}]})"
    };
    for (const auto& json : corrupt) {
        write(temporary.path / "x.json", json);
        throws([&] { (void)manager.get("x"); });
        throws([&] { (void)manager.getAll(); });
    }
    CHECK(manager.remove("x")); // Corrupt files can still be deleted through API.
    write(temporary.path / "ignore.txt", "not json");
    CHECK(manager.getAll().empty());
    std::filesystem::create_directory(temporary.path / "directory.json");
    DeckStorage storage(temporary.path);
    throws([&] { (void)storage.get("directory"); });
    throws([&] { storage.save(deck("directory")); });
    CHECK(std::filesystem::is_directory(temporary.path / "directory.json"));
    throws([&] { (void)storage.remove("directory"); });
    // Deterministic I/O failure, even when tests are run as a privileged user.
    DeckStorage blocked(temporary.path / "ignore.txt" / "decks");
    throws([&] { blocked.save(deck()); });
    DeckManager invalid(cards(), {}, temporary.path / "ignore.txt" / "decks");
    throws([&] { invalid.create(deck()); });
    const auto external = temporary.path / "external.txt";
    write(external, "unchanged");
    std::filesystem::create_symlink(external, temporary.path / "link.json");
    throws([&] { (void)storage.get("link"); });
    throws([&] { storage.save(deck("link")); });
    throws([&] { (void)storage.remove("link"); });
    std::ifstream input(external); std::string text; input >> text;
    CHECK(text == "unchanged");
}

std::string snapshot(const Game& game) {
    std::ostringstream output;
    const auto& state = game.getState();
    output << static_cast<int>(state.status) << state.activePlayer << state.turnNumber
           << static_cast<int>(state.phase) << state.hasDrawnThisTurn;
    for (const auto& player : state.players) {
        output << "hp" << player.hp;
        auto card = [&](const CardInstance& c) {
            output << c.id << ':' << c.definitionId << ':' << c.owner << ':' << c.currentHP << ':' << c.hasAttacked << ';';
        };
        output << "deck"; for (const auto& c : player.deck) card(c);
        output << "hand"; for (const auto& c : player.hand) card(c);
        output << "tomb"; for (const auto& c : player.tomb) card(c);
        output << "board"; for (const auto& zone : player.board.zones()) { if (zone) card(*zone); else output << "empty"; }
    }
    return output.str();
}
void testGameDecks() {
    const GameConfig config{20, 2, 3, 6};
    auto first = DeckDefinition{"first", "First", {{"knight", 3}, {"guard", 3}}};
    auto second = deck("second", 6);
    auto database = cards();
    Game game(database, config, first, second, 123);
    Game replay(database, config, first, second, 123);
    first.cards.clear(); second.cards[0].cardId = "unknown";
    CHECK(database.add({"extra", "Extra", 1, 1}));
    CHECK(game.start() == ActionError::None);
    CHECK(replay.start() == ActionError::None);
    CHECK(snapshot(game) == snapshot(replay));
    std::set<InstanceId> ids;
    for (PlayerId player = 0; player < 2; ++player) {
        const auto& state = game.getState().players[player];
        CHECK(state.hp == 20 && state.hand.size() == 2 && state.deck.size() == 4);
        CHECK(state.board.zones().size() == 3 && state.board.empty() && state.tomb.empty());
        std::size_t knights = 0, guards = 0;
        auto inspect = [&](const CardInstance& card) {
            CHECK(ids.insert(card.id).second); CHECK(card.id != 0); CHECK(card.owner == player);
            CHECK(!card.hasAttacked);
            CHECK(card.currentHP == cards().find(card.definitionId)->hp);
            if (card.definitionId == "knight") ++knights; else ++guards;
        };
        for (const auto& c : state.hand) inspect(c);
        for (const auto& c : state.deck) inspect(c);
        CHECK(knights == 3); CHECK(guards == 3);
    }
    CHECK(game.start() == ActionError::None);
    for (const auto& player : game.getState().players) {
        for (const auto& c : player.hand) CHECK(!ids.contains(c.id));
        for (const auto& c : player.deck) CHECK(!ids.contains(c.id));
    }
    game.reset(); CHECK(game.start() == ActionError::None);
    for (auto bad : {deck("incomplete", 5), deck("excess", 7), deck("zero", 0),
                     DeckDefinition{"unknown", "Unknown", {{"missing", 6}}}}) {
        const auto expected = bad.id == "unknown" ? ActionError::UnknownCard : ActionError::InvalidDeck;
        Game invalid(cards(), config, deck("good", 6), bad, 1);
        const auto before = snapshot(invalid);
        CHECK(invalid.start() == expected); CHECK(snapshot(invalid) == before);
        // A failed no-argument restart must also preserve an already playing match.
        CHECK(invalid.start(Game::Deck{"knight", "knight", "knight", "guard", "guard", "guard"}, Game::Deck{"guard", "guard", "guard", "knight", "knight", "knight"}) == ActionError::None);
        const auto playing = snapshot(invalid);
        CHECK(invalid.start() == expected); CHECK(snapshot(invalid) == playing);
    }
    Game legacy(cards(), config, 1);
    const auto before = snapshot(legacy);
    CHECK(legacy.start() == ActionError::InvalidDeck); CHECK(snapshot(legacy) == before);
    // No dependence on manager lifetime or files after constructing a Game.
    TemporaryDirectory temporary;
    auto owned = [&] {
        DeckManager manager(cards(), config, temporary.path);
        manager.create(deck("one", 6)); manager.create(deck("two", 6));
        Game result(cards(), config, *manager.get("one"), *manager.get("two"), 9);
        CHECK(manager.remove("one")); CHECK(manager.remove("two"));
        return result;
    }();
    CHECK(owned.start() == ActionError::None);
}

void testCopyLimits() {
    CHECK(GameConfig{}.maxCopiesPerCard == 3);
    const auto database = cards();
    const GameConfig config{20, 2, 3, 6};
    const DeckDefinition valid{"valid", "Three copies each", {{"knight", 3}, {"guard", 3}}};
    const DeckDefinition repeated{"repeated", "Repeated entries", {{"knight", 1}, {"guard", 3}, {"knight", 2}}};
    const DeckDefinition four{"four", "Four copies", {{"knight", 4}, {"guard", 2}}};
    const DeckDefinition splitFour{"split", "Four split copies", {{"knight", 2}, {"guard", 2}, {"knight", 2}}};
    for (const auto mode : {DeckValidationMode::Storage, DeckValidationMode::Play}) {
        CHECK(DeckValidator::validate(valid, database, config, mode).isValid());
        CHECK(DeckValidator::validate(repeated, database, config, mode).isValid());
        for (const auto& bad : {four, splitFour}) {
            const auto result = DeckValidator::validate(bad, database, config, mode);
            CHECK(result.error == DeckValidationError::TooManyCopies);
            CHECK(result.message.find("knight") != std::string::npos);
        }
    }
    // Drafts must obey the same cap even when far below deckSize.
    CHECK(DeckValidator::validate(deck("three", 3), database, {}).isValid());
    CHECK(DeckValidator::validate(deck("four", 4), database, {}).error == DeckValidationError::TooManyCopies);
    TemporaryDirectory temporary;
    DeckManager manager(database, config, temporary.path);
    manager.create(valid);
    for (const auto& bad : {four, splitFour}) {
        throws<std::invalid_argument>([&] { manager.create(bad); });
        auto update = bad; update.id = valid.id;
        throws<std::invalid_argument>([&] { manager.update(update); });
        CHECK(manager.get(valid.id) == valid);
        for (const bool first : {false, true}) {
            Game invalid(database, config, first ? bad : valid, first ? valid : bad, 42);
            const auto before = snapshot(invalid);
            CHECK(invalid.start() == ActionError::InvalidDeck);
            CHECK(snapshot(invalid) == before);
        }
    }
    Game game(database, config, repeated, valid, 42);
    CHECK(game.start() == ActionError::None);
    const auto before = snapshot(game);
    const Game::Deck legal{"knight", "knight", "knight", "guard", "guard", "guard"};
    const Game::Deck illegal{"knight", "guard", "knight", "guard", "knight", "knight"};
    CHECK(game.start(illegal, legal) == ActionError::InvalidDeck);
    CHECK(snapshot(game) == before);
    CHECK(game.start(legal, illegal) == ActionError::InvalidDeck);
    CHECK(snapshot(game) == before);
    CHECK(game.start(legal, legal) == ActionError::None);
    // The limit is configuration-driven, not a hardcoded magic number.
    auto custom = config; custom.maxCopiesPerCard = 4;
    CHECK(DeckValidator::validate(four, database, custom, DeckValidationMode::Play).isValid());
    Game configured(database, custom, four, four, 42);
    CHECK(configured.start() == ActionError::None);
    CHECK(configured.start(illegal, illegal) == ActionError::None);
    custom.maxCopiesPerCard = 0;
    CHECK(!custom.isValid());
    throws<std::invalid_argument>([&] { Game invalid(database, custom, 42); });
}

int main(int argc, char** argv) {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"copy_limits", testCopyLimits}, {"catalog", testCatalog}, {"validation", testValidation}, {"crud", testCrud},
        {"corrupt_storage", testCorruptStorage}, {"game_decks", testGameDecks}
    };
    bool selected = false;
    for (const auto& [name, run] : tests) {
        if (argc > 1 && name != argv[1]) continue;
        selected = true;
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; return 1; }
    }
    return selected ? 0 : 2;
}
