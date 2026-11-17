#include "soulbound/JsonLoader.hpp"
#include "soulbound/CombatSystem.hpp"
#include "soulbound/Game.hpp"

#include <algorithm>
#include <functional>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace soulbound;

static_assert(std::is_same_v<decltype(std::declval<Game&>().getState()), const GameState&>);
static_assert(std::is_same_v<decltype(std::declval<Board&>().zones()),
                             const std::vector<std::optional<CardInstance>>&>);
static_assert(std::is_same_v<decltype(std::declval<CardDatabase&>().find("knight")),
                             const CardDefinition*>);

#define CHECK(expression) do { if (!(expression)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #expression); } while (false)

void ok(ActionError error) { CHECK(error == ActionError::None); }

// Exercise JSON validation through the public file-loading API.
template<typename Loader>
auto loadTestJson(std::string_view text, Loader loader) {
    struct TemporaryDirectory {
        std::filesystem::path path;
        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
    auto path = std::filesystem::temp_directory_path();
    do {
        path = std::filesystem::temp_directory_path() /
            ("soulbound-json-test-" + std::to_string(std::random_device{}()));
    } while (!std::filesystem::create_directory(path));
    const TemporaryDirectory cleanup{path};
    const auto file = path / "input.json";
    {
        std::ofstream output(file, std::ios::binary);
        output << text;
        output.close();
        CHECK(output.good());
    }
    return loader(file);
}

CardDatabase loadCardsJson(std::string_view text) {
    return loadTestJson(text, JsonLoader::loadCards);
}

GameConfig loadGameConfigJson(std::string_view text) {
    return loadTestJson(text, JsonLoader::loadGameConfig);
}

// Historical combat fixtures intentionally permit repeated vanilla units.
GameConfig gameplayConfig() { GameConfig config; config.maxCopiesPerCard = 30; return config; }

CardDatabase database() { return JsonLoader::loadCards(CARD_DATA_PATH); }

Game::Deck deck(const std::string& id = "knight") { return Game::Deck(30, id); }

Game::Deck mixedDeck() {
    auto result = deck();
    std::fill(result.begin() + 15, result.end(), "guard");
    return result;
}

Game started(const std::string& first = "knight", const std::string& second = "guard") {
    auto game = Game::fromFiles(CONFIG_DATA_PATH, CARD_DATA_PATH, 42);
    ok(game.start(deck(first), deck(second)));
    return game;
}

InstanceId summon(Game& game, PlayerId player, std::size_t zone = 0) {
    const auto id = game.getState().players[player].hand.front().id;
    ok(game.summonUnit(player, id, zone));
    return id;
}

const CardInstance& unit(const Game& game, PlayerId player, std::size_t zone = 0) {
    return game.getState().players[player].board.zones().at(zone).value();
}

// Include every piece of state so rejected actions are checked for partial mutation.
std::string snapshot(const Game& game) {
    std::ostringstream out;
    const auto& state = game.getState();
    out << static_cast<int>(state.status) << ':' << state.activePlayer << ':'
        << state.hasDrawnThisTurn << ':' << state.turnNumber << ':' << static_cast<int>(state.phase);
    auto card = [&](const CardInstance& value) {
        out << '(' << value.id << ',' << value.definitionId << ',' << value.owner
            << ',' << value.currentHP << ',' << value.hasAttacked << ')';
    };
    for (const auto& player : state.players) {
        out << "HP" << player.hp << "Deck";
        for (const auto& value : player.deck) card(value);
        out << "Hand";
        for (const auto& value : player.hand) card(value);
        out << "Tomb";
        for (const auto& value : player.tomb) card(value);
        out << "Board";
        for (const auto& zone : player.board.zones()) {
            if (zone) card(*zone); else out << "empty";
        }
    }
    return out.str();
}

void rejected(Game& game, ActionError expected, const std::function<ActionError()>& action) {
    const auto before = snapshot(game);
    CHECK(action() == expected);
    CHECK(snapshot(game) == before);
}

void invariant(const Game& game, std::size_t deckSize = 30, std::size_t boardSize = 6) {
    std::set<InstanceId> ids;
    const auto definitions = database();
    const auto& state = game.getState();
    CHECK(state.activePlayer < 2);
    CHECK(state.turnNumber >= 1);
    CHECK(state.activePlayer == (state.turnNumber - 1) % 2);
    CHECK(state.phase == Phase::Main || state.phase == Phase::Battle);
    for (PlayerId id = 0; id < 2; ++id) {
        const auto& player = state.players[id];
        CHECK(player.hp >= 0);
        std::size_t count = 0;
        auto inspect = [&](const CardInstance& card) {
            CHECK(card.id != 0);
            CHECK(ids.insert(card.id).second);
            CHECK(card.owner == id);
            CHECK(definitions.find(card.definitionId));
            CHECK(card.currentHP <= definitions.find(card.definitionId)->hp);
            ++count;
        };
        for (const auto& card : player.deck) {
            inspect(card);
            CHECK(card.currentHP == definitions.find(card.definitionId)->hp);
            CHECK(!card.hasAttacked);
        }
        for (const auto& card : player.hand) {
            inspect(card);
            CHECK(card.currentHP == definitions.find(card.definitionId)->hp);
            CHECK(!card.hasAttacked);
        }
        for (const auto& card : player.tomb) inspect(card);
        CHECK(player.board.zones().size() == boardSize);
        for (const auto& zone : player.board.zones()) {
            if (zone) { inspect(*zone); CHECK(zone->currentHP > 0); }
        }
        CHECK(count == deckSize);
    }
    CHECK(ids.size() == deckSize * 2);
    CHECK(Game::evaluateOutcome(state) == state.status);
}

void testCards() {
    auto db = database();
    CHECK(db.size() == 2);
    CHECK(db.find("knight")->attack == 5);
    CHECK(db.find("knight")->hp == 8);
    CHECK(db.find("guard")->attack == 3);
    CHECK(db.find("guard")->hp == 6);
    CHECK(!db.find("missing"));
    CHECK(!db.add({"knight", "Duplicate", 1, 1}));
    CHECK(!db.add({"bad", "Bad", -1, 1}));
    CHECK(!db.add({"bad", "Bad", 1, 0}));
    CHECK(!db.add({"", "Bad", 1, 1}));
    CHECK(!db.add({"bad", "", 1, 1}));
    CHECK(db.size() == 2);
    bool threw = false;
    try { (void)JsonLoader::loadCards("nonexistent-soulbound-cards.json"); }
    catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

void testJsonValidation() {
    const std::vector<std::string> bad = {
        "", "null", "{}", "[]", "{\"cards\":null}", "{\"cards\":[{}]}",
        R"({"cards":[{"id":"a","name":"A","atk":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":"5","hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":1.5,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":1e2,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":01,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":2147483648,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":-1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":1,"hp":0}]})",
        R"({"cards":[{"id":"","name":"A","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","id":"b","name":"A","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":1,"hp":1,"effect":null}]})",
        R"({"cards":[{"id":"a","name":"A","atk":1,"hp":1},]})",
        R"({"cards":[],"cards":[]})", R"({"cards":[]} garbage)",
        R"({"cards":[{"id":"a","name":"\q","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"\uD800","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"\uDC00","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"\uD800\u0041","atk":1,"hp":1}]})",
        R"({"cards":[{"id":"a","name":"A","atk":1,"hp":1},{"id":"a","name":"B","atk":2,"hp":2}]})",
        std::string("{\"cards\":[{\"id\":\"a\",\"name\":\"") + char(0xC0) + char(0xAF) + "\",\"atk\":1,\"hp\":1}]}"
    };
    for (const auto& json : bad) {
        bool threw = false;
        try { (void)loadCardsJson(json); }
        catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
    CHECK(loadCardsJson(R"({"cards":[]})").size() == 0);
    const auto unicode = loadCardsJson(
        R"({"cards":[{"hp":8,"name":"Hiệp sĩ \uD83D\uDEE1\n\"","id":"knight","atk":5}]})");
    CHECK(unicode.find("knight")->name == "Hiệp sĩ 🛡\n\"");
}

void testStart() {
    auto game = started();
    const auto& state = game.getState();
    CHECK(state.status == GameStatus::Playing);
    CHECK(state.activePlayer == 0);
    CHECK(state.turnNumber == 1);
    CHECK(state.phase == Phase::Main);
    for (const auto& player : state.players) {
        CHECK(player.hp == 20);
        CHECK(player.hand.size() == 4);
        CHECK(player.deck.size() == 26);
        CHECK(player.tomb.empty());
        CHECK(player.board.empty());
    }
    invariant(game);
    const auto oldId = state.players[0].hand.front().id;
    ok(game.start(deck(), deck("guard")));
    for (const auto& card : game.getState().players[0].hand) CHECK(card.id != oldId);
    game.reset();
    CHECK(game.getState().status == GameStatus::NotStarted);
    rejected(game, ActionError::NotPlaying, [&] { return game.drawCard(0); });
    ok(game.start(deck(), deck("guard")));
    invariant(game);
}

void testShuffleAndSnapshot() {
    auto db = database();
    Game first(db, gameplayConfig(), 1234), second(db, gameplayConfig(), 1234), third(db, gameplayConfig(), 5678);
    CHECK(db.add({"extra", "Extra", 1, 1}));
    const auto mixed = mixedDeck();
    ok(first.start(mixed, mixed));
    ok(second.start(mixed, mixed));
    ok(third.start(mixed, mixed));
    CHECK(snapshot(first) == snapshot(second));
    CHECK(snapshot(first) != snapshot(third));
    auto unknown = mixed;
    unknown.front() = "extra";
    rejected(first, ActionError::UnknownCard, [&] { return first.start(unknown, mixed); });
    auto invalid = mixed;
    invalid.pop_back();
    rejected(first, ActionError::InvalidDeck, [&] { return first.start(invalid, mixed); });
    invariant(first);
}

void testConfig() {
    GameConfig config{40, 2, 3, 6, 6};
    Game game(database(), config, 7);
    ok(game.start(Game::Deck(6, "knight"), Game::Deck(6, "guard")));
    CHECK(game.getState().players[0].hp == 40);
    CHECK(game.getState().players[0].hand.size() == 2);
    CHECK(game.getState().players[0].deck.size() == 4);
    invariant(game, 6, 3);
    for (const auto& bad : std::vector<GameConfig>{{0,4,6,30}, {20,31,6,30},
                                                  {20,4,0,30}, {20,0,6,0}}) {
        bool threw = false;
        try { Game invalid(database(), bad, 1); }
        catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
}

void testConfigJson() {
    const auto config = JsonLoader::loadGameConfig(CONFIG_DATA_PATH);
    CHECK(config.startingHP == 20);
    CHECK(config.startingHandSize == 4);
    CHECK(config.boardSize == 6);
    CHECK(config.deckSize == 30);
    const auto reordered = loadGameConfigJson(
        R"({"deckSize":6,"boardSize":3,"startingHandSize":2,"startingHP":40})");
    CHECK(reordered.maxCopiesPerCard == 3); // Absent field stays backward compatible.
    const auto explicitLimit = loadGameConfigJson(
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30,"maxCopiesPerCard":4})");
    CHECK(explicitLimit.maxCopiesPerCard == 4);
    for (const auto* value : {"0", "-1", "1.5", "true", "null", "\"3\"", "18446744073709551616"}) {
        bool threw = false;
        try {
            (void)loadGameConfigJson(std::string(R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30,"maxCopiesPerCard":)") + value + "}");
        } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
    bool duplicateThrew = false;
    try { (void)loadGameConfigJson(R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30,"maxCopiesPerCard":3,"maxCopiesPerCard":4})"); }
    catch (const std::runtime_error&) { duplicateThrew = true; }
    CHECK(duplicateThrew);
    CHECK(reordered.startingHP == 40);
    CHECK(reordered.startingHandSize == 2);
    CHECK(reordered.boardSize == 3);
    CHECK(reordered.deckSize == 6);
    CHECK(loadGameConfigJson(
        R"({"startingHP":1,"startingHandSize":0,"boardSize":1,"deckSize":1})").startingHandSize == 0);

    const std::vector<std::string> bad = {
        "", "null", "[]", "{}",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6})",
        R"({"startingHP":0,"startingHandSize":4,"boardSize":6,"deckSize":30})",
        R"({"startingHP":-1,"startingHandSize":4,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":31,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":-1,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":0,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":-1,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":0,"boardSize":6,"deckSize":0})",
        R"({"startingHP":20,"startingHandSize":0,"boardSize":6,"deckSize":-1})",
        R"({"startingHP":"20","startingHandSize":4,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4.5,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":true,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":3e1})",
        R"({"startingHP":2147483648,"startingHandSize":4,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":18446744073709551616})",
        R"({"startingHP":20,"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30,"extra":1})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30,})",
        R"({"startingHP":20,"startingHandSize":4,"boardSize":6,"deckSize":30} trailing)"
    };
    for (const auto& json : bad) {
        bool threw = false;
        try { (void)loadGameConfigJson(json); }
        catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
}

void testLoadOrder() {
    // Invalid config must fail before opening even a missing card database.
    bool configFailed = false;
    try { (void)Game::fromFiles(CARD_DATA_PATH, "missing-cards.json", 42); }
    catch (const std::runtime_error& error) {
        configFailed = std::string(error.what()).find("unknown game config field") != std::string::npos;
    }
    CHECK(configFailed);
    bool missingConfigFailed = false;
    try { (void)Game::fromFiles("missing-game-config.json", "missing-cards.json", 42); }
    catch (const std::runtime_error& error) {
        missingConfigFailed = std::string(error.what()).find("Cannot open game config file") != std::string::npos;
    }
    CHECK(missingConfigFailed);
    bool cardsFailed = false;
    try { (void)Game::fromFiles(CONFIG_DATA_PATH, "missing-cards.json", 42); }
    catch (const std::runtime_error& error) {
        cardsFailed = std::string(error.what()).find("Cannot open card file") != std::string::npos;
    }
    CHECK(cardsFailed);

    auto game = Game::fromFiles(CONFIG_FIXTURE_PATH, CARD_DATA_PATH, 42);
    CHECK(game.getConfig().startingHP == 40);
    ok(game.start(Game::Deck(6, "knight"), Game::Deck(6, "guard")));
    CHECK(game.getState().players[0].hp == 40);
    CHECK(game.getState().players[0].hand.size() == 2);
    CHECK(game.getState().players[0].deck.size() == 4);
    invariant(game, 6, 3);
}

void testDrawAndEmptyDeck() {
    auto game = started();
    rejected(game, ActionError::WrongPlayer, [&] { return game.drawCard(1); });
    rejected(game, ActionError::InvalidPlayer, [&] { return game.drawCard(2); });
    while (!game.getState().players[0].deck.empty()) {
        const auto expected = game.getState().players[0].deck.back().id;
        ok(game.drawCard(0));
        CHECK(game.getState().players[0].hand.back().id == expected);
        CHECK(game.getState().hasDrawnThisTurn);
        rejected(game, ActionError::AlreadyDrawn, [&] { return game.drawCard(0); });
        ok(game.endTurn());
        CHECK(!game.getState().hasDrawnThisTurn);
        ok(game.drawCard(1));
        rejected(game, ActionError::AlreadyDrawn, [&] { return game.drawCard(1); });
        ok(game.endTurn());
        invariant(game);
    }
    CHECK(game.getState().players[0].hand.size() == 30);
    rejected(game, ActionError::EmptyDeck, [&] { return game.drawCard(0); });
    CHECK(game.getState().status == GameStatus::Playing);
    ok(game.beginBattle());
    rejected(game, ActionError::WrongPhase, [&] { return game.drawCard(0); });
}

void testSummon() {
    auto game = started();
    auto id = game.getState().players[0].hand[0].id;
    rejected(game, ActionError::WrongPlayer, [&] { return game.summonUnit(1, id, 0); });
    rejected(game, ActionError::InvalidZone, [&] { return game.summonUnit(0, id, 6); });
    rejected(game, ActionError::InvalidInstance, [&] { return game.summonUnit(0, 9999, 0); });
    ok(game.summonUnit(0, id, 5));
    CHECK(unit(game, 0, 5).id == id);
    CHECK(unit(game, 0, 5).currentHP == 8);
    CHECK(game.getState().players[0].hp == 12);
    CHECK(game.getState().players[0].hand.size() == 3);
    rejected(game, ActionError::OccupiedZone, [&] { return game.summonUnit(0, id, 5); });
    rejected(game, ActionError::InvalidInstance, [&] { return game.summonUnit(0, id, 0); });
    (void)summon(game, 0);
    CHECK(game.getState().players[0].hp == 4);
    id = game.getState().players[0].hand.front().id;
    rejected(game, ActionError::InsufficientHP, [&] { return game.summonUnit(0, id, 1); });
    ok(game.beginBattle());
    rejected(game, ActionError::WrongPhase, [&] { return game.summonUnit(0, id, 1); });
    invariant(game);
}

void testZeroHPAndTomb() {
    auto game = started();
    ok(game.damagePlayer(0, 12));
    const auto id = summon(game, 0);
    CHECK(game.getState().players[0].hp == 0);
    CHECK(game.getState().status == GameStatus::Playing);
    ok(game.damagePlayer(0, 100));
    CHECK(game.getState().players[0].hp == 0);
    CHECK(game.getState().status == GameStatus::Playing);
    ok(game.moveToTomb(id));
    CHECK(game.getState().players[0].board.empty());
    CHECK(game.getState().players[0].tomb.back().id == id);
    CHECK(game.getState().status == GameStatus::Player2Won);
    rejected(game, ActionError::NotPlaying, [&] { return game.healPlayer(0, 1000); });
    rejected(game, ActionError::NotPlaying, [&] { return game.endTurn(); });
    invariant(game);
}

void testTurnAndFirstTurn() {
    auto game = started();
    const auto first = summon(game, 0);
    rejected(game, ActionError::WrongPhase, [&] { return game.attackPlayer(first, 1); });
    ok(game.beginBattle());
    rejected(game, ActionError::FirstTurn, [&] { return game.attackPlayer(first, 1); });
    ok(game.endTurn());
    CHECK(game.getState().turnNumber == 2);
    CHECK(game.getState().activePlayer == 1);
    CHECK(game.getState().phase == Phase::Main);
    const auto second = summon(game, 1);
    ok(game.beginBattle());
    // A newly summoned unit can fight on the second turn of the match.
    ok(game.attackUnit(second, first));
    CHECK(unit(game, 0).currentHP == 5);
    CHECK(unit(game, 1).currentHP == 1);
    CHECK(!unit(game, 0).hasAttacked);
    CHECK(unit(game, 1).hasAttacked);
    rejected(game, ActionError::AlreadyAttacked, [&] { return game.attackUnit(second, first); });
    ok(game.endTurn());
    CHECK(!unit(game, 0).hasAttacked);
    CHECK(!unit(game, 1).hasAttacked);
    ok(game.beginBattle());
    ok(game.attackUnit(first, second));
    CHECK(game.getState().players[1].board.empty());
    invariant(game);
}

void testCombatSurvival() {
    auto game = started();
    const auto knight = summon(game, 0);
    ok(game.endTurn());
    const auto guard = summon(game, 1);
    ok(game.endTurn());
    ok(game.beginBattle());
    ok(game.attackUnit(knight, guard));
    CHECK(unit(game, 0).currentHP == 5);
    CHECK(unit(game, 1).currentHP == 1);
    CHECK(game.getState().players[0].hp == 12);
    CHECK(game.getState().players[1].hp == 14);
    CHECK(game.getState().players[0].tomb.empty());
    CHECK(game.getState().players[1].tomb.empty());
    invariant(game);
}

void testCombatSpillover() {
    for (const HitPoints defenderHP : {3, 5}) {
        auto game = started();
        const auto knight = summon(game, 0);
        ok(game.endTurn());
        const auto guard = summon(game, 1);
        ok(game.damageUnit(guard, 6 - defenderHP));
        ok(game.endTurn());
        ok(game.beginBattle());
        ok(game.attackUnit(knight, guard));
        CHECK(unit(game, 0).currentHP == 8); // Dead defender never counterattacks.
        CHECK(game.getState().players[1].board.empty());
        CHECK(game.getState().players[1].tomb.back().id == guard);
        CHECK(game.getState().players[1].hp == 14 - (5 - defenderHP));
        invariant(game);
    }
}

void testCounterattackSpillover() {
    auto game = started("guard", "knight");
    const auto guard = summon(game, 0);
    ok(game.damageUnit(guard, 4)); // Two HP remaining.
    ok(game.endTurn());
    const auto knight = summon(game, 1);
    ok(game.endTurn());
    ok(game.beginBattle());
    ok(game.damagePlayer(0, 13)); // One HP remains before counterattack spillover.
    ok(game.attackUnit(guard, knight));
    CHECK(unit(game, 1).currentHP == 5);
    CHECK(game.getState().players[0].hp == 0); // Three spillover damage is capped at remaining HP.
    CHECK(game.getState().status == GameStatus::Player2Won);
    CHECK(game.getState().players[0].board.empty());
    CHECK(game.getState().players[0].tomb.back().id == guard);
    invariant(game);
}

void testTargets() {
    auto game = started();
    const auto knight = summon(game, 0);
    const auto other = summon(game, 0, 1);
    ok(game.endTurn());
    const auto guard = summon(game, 1);
    ok(game.endTurn());
    ok(game.beginBattle());
    rejected(game, ActionError::InvalidInstance, [&] { return game.attackUnit(9999, guard); });
    rejected(game, ActionError::InvalidInstance, [&] { return game.attackUnit(knight, 9999); });
    rejected(game, ActionError::InvalidTarget, [&] { return game.attackUnit(knight, other); });
    rejected(game, ActionError::WrongPlayer, [&] { return game.attackUnit(guard, knight); });
    rejected(game, ActionError::InvalidTarget, [&] { return game.attackPlayer(knight, 0); });
    rejected(game, ActionError::InvalidPlayer, [&] { return game.attackPlayer(knight, 2); });
    rejected(game, ActionError::InvalidTarget, [&] { return game.attackPlayer(knight, 1); });
    ok(game.attackUnit(knight, guard));
    CHECK(!unit(game, 1).hasAttacked);
    ok(game.attackUnit(other, guard));
    CHECK(game.getState().players[1].board.empty());
    invariant(game);

    auto repeated = started("guard", "knight");
    const auto firstGuard = summon(repeated, 0);
    const auto secondGuard = summon(repeated, 0, 1);
    ok(repeated.endTurn());
    const auto defender = summon(repeated, 1);
    ok(repeated.endTurn());
    ok(repeated.beginBattle());
    ok(repeated.attackUnit(firstGuard, defender));
    rejected(repeated, ActionError::AlreadyAttacked, [&] {
        return repeated.attackUnit(firstGuard, defender);
    });
    ok(repeated.attackUnit(secondGuard, defender));
    CHECK(unit(repeated, 1).currentHP == 2);
    CHECK(!unit(repeated, 1).hasAttacked);
    CHECK(unit(repeated, 0, 0).currentHP == 1);
    CHECK(unit(repeated, 0, 1).currentHP == 1);
    invariant(repeated);
}

void testDirectAttackAndGameOver() {
    auto game = started();
    const auto knight = summon(game, 0);
    ok(game.endTurn());
    ok(game.endTurn());
    ok(game.beginBattle());
    ok(game.attackPlayer(knight, 1));
    CHECK(game.getState().players[1].hp == 15);
    rejected(game, ActionError::AlreadyAttacked, [&] { return game.attackPlayer(knight, 1); });
    ok(game.endTurn());
    ok(game.endTurn());
    ok(game.beginBattle());
    ok(game.damagePlayer(1, 14));
    ok(game.attackPlayer(knight, 1));
    CHECK(game.getState().players[1].hp == 0);
    CHECK(game.getState().status == GameStatus::Player1Won);
    rejected(game, ActionError::NotPlaying, [&] { return game.attackPlayer(knight, 1); });
    invariant(game);

    GameState simultaneous;
    simultaneous.status = GameStatus::Playing;
    simultaneous.players[0].hp = 0;
    simultaneous.players[1].hp = -1;
    CHECK(Game::evaluateOutcome(simultaneous) == GameStatus::Draw);
}

void testHPAndEngineOperations() {
    auto game = started();
    const auto id = summon(game, 0);
    ok(game.healPlayer(0, 100));
    CHECK(game.getState().players[0].hp == 112);
    ok(game.healUnit(id, 100));
    CHECK(unit(game, 0).currentHP == 8);
    ok(game.damageUnit(id, 3));
    ok(game.healUnit(id, 2));
    CHECK(unit(game, 0).currentHP == 7);
    ok(game.healUnit(id, 10));
    CHECK(unit(game, 0).currentHP == 8);
    ok(game.damageUnit(id, 7));
    CHECK(unit(game, 0).currentHP == 1);
    rejected(game, ActionError::InvalidAmount, [&] { return game.damagePlayer(0, -1); });
    rejected(game, ActionError::InvalidAmount, [&] { return game.damageUnit(id, -1); });
    rejected(game, ActionError::InvalidAmount, [&] { return game.healPlayer(0, -1); });
    rejected(game, ActionError::InvalidAmount, [&] { return game.healUnit(id, -1); });
    rejected(game, ActionError::InvalidPlayer, [&] { return game.healPlayer(10, 1); });
    rejected(game, ActionError::InvalidInstance, [&] { return game.moveToTomb(9999); });
    rejected(game, ActionError::InvalidAmount, [&] {
        return game.healPlayer(0, std::numeric_limits<HitPoints>::max());
    });
    ok(game.damageUnit(id, 2));
    CHECK(game.getState().players[0].hp == 112); // Generic damage has no combat spillover.
    CHECK(game.getState().players[0].tomb.back().currentHP == -1);
    rejected(game, ActionError::InvalidInstance, [&] { return game.healUnit(id, 10); });
    rejected(game, ActionError::InvalidInstance, [&] { return game.moveToTomb(id); });
    invariant(game);
}

void testCombatGameOverAndLimits() {
    auto game = started();
    const auto knight = summon(game, 0);
    ok(game.endTurn());
    const auto guard = summon(game, 1);
    ok(game.damagePlayer(1, 14)); // Zero HP with a unit is still alive.
    ok(game.damageUnit(guard, 3));
    ok(game.endTurn());
    ok(game.beginBattle());
    ok(game.attackUnit(knight, guard));
    CHECK(game.getState().players[1].hp == 0);
    CHECK(game.getState().status == GameStatus::Player1Won);
    invariant(game);

    auto limits = started();
    const auto protectedUnit = summon(limits, 0);
    ok(limits.damagePlayer(0, std::numeric_limits<HitPoints>::max()));
    CHECK(limits.getState().players[0].hp == 0);
    ok(limits.damagePlayer(0, 13)); // Further damage stays at zero while protected by a unit.
    CHECK(limits.getState().players[0].hp == 0);
    ok(limits.damagePlayer(0, 1));
    CHECK(limits.getState().players[0].hp == 0);
    ok(limits.healPlayer(0, std::numeric_limits<HitPoints>::max()));
    CHECK(limits.getState().players[0].hp == std::numeric_limits<HitPoints>::max());
    rejected(limits, ActionError::InvalidAmount, [&] { return limits.healPlayer(0, 1); });
    ok(limits.healPlayer(0, 0)); // Zero healing is valid even at the numeric limit.
    rejected(limits, ActionError::InvalidAmount, [&] {
        return limits.healPlayer(0, std::numeric_limits<HitPoints>::max());
    });
    rejected(limits, ActionError::InvalidAmount, [&] {
        return limits.healPlayer(0, std::numeric_limits<HitPoints>::min());
    });
    ok(limits.healUnit(protectedUnit, std::numeric_limits<HitPoints>::max() - 8));
    CHECK(unit(limits, 0).currentHP == 8);
    ok(limits.healUnit(protectedUnit, std::numeric_limits<HitPoints>::max()));
    CHECK(unit(limits, 0).currentHP == 8);
    invariant(limits);
}

void testSimulations() {
    // Repeated legal and illegal operations must preserve conservation and ownership.
    for (std::uint32_t seed = 0; seed < 30; ++seed) {
        Game game(database(), gameplayConfig(), seed);
        ok(game.start(mixedDeck(), mixedDeck()));
        invariant(game);
        for (int turn = 0; turn < 30 && game.getState().status == GameStatus::Playing; ++turn) {
            const auto player = game.getState().activePlayer;
            const auto opponent = 1 - player;
            (void)game.drawCard(player);
            invariant(game);
            auto hand = game.getState().players[player].hand;
            for (std::size_t zone = 0; zone < 6 && !hand.empty(); ++zone) {
                (void)game.summonUnit(player, hand.back().id, zone);
                hand.pop_back();
                invariant(game);
            }
            ok(game.beginBattle());
            std::vector<InstanceId> attackers;
            for (const auto& zone : game.getState().players[player].board.zones())
                if (zone) attackers.push_back(zone->id);
            for (const auto attacker : attackers) {
                if (game.getState().status != GameStatus::Playing) break;
                InstanceId defender = 0;
                for (const auto& zone : game.getState().players[opponent].board.zones())
                    if (zone) { defender = zone->id; break; }
                if (defender) (void)game.attackUnit(attacker, defender);
                else (void)game.attackPlayer(attacker, opponent);
                invariant(game);
            }
            if (game.getState().status == GameStatus::Playing) ok(game.endTurn());
        }
    }
}


// Stable identity includes definition and owner, not merely a conserved card count.
using IdentityLedger = std::map<InstanceId, std::pair<std::string, PlayerId>>;
IdentityLedger identities(const Game& game) {
    IdentityLedger result;
    auto record = [&](const CardInstance& card) {
        CHECK(result.emplace(card.id, std::make_pair(card.definitionId, card.owner)).second);
    };
    for (const auto& player : game.getState().players) {
        for (const auto& card : player.deck) record(card);
        for (const auto& card : player.hand) record(card);
        for (const auto& card : player.tomb) record(card);
        for (const auto& zone : player.board.zones()) if (zone) record(*zone);
    }
    return result;
}

void testManualPhases() {
    auto game = started();
    const auto ledger = identities(game);
    auto check = [&] { invariant(game); CHECK(identities(game) == ledger); };
    const auto first = game.getState().players[0].hand.front().id;
    const auto second = game.getState().players[1].hand.front().id;
    const auto sizes = [&] {
        return std::array<std::size_t, 4>{game.getState().players[0].hand.size(),
            game.getState().players[0].deck.size(), game.getState().players[1].hand.size(),
            game.getState().players[1].deck.size()};
    };
    const auto initial = sizes();
    ok(game.endTurn()); check(); // Main -> opponent Main is currently legal.
    CHECK(sizes() == initial);
    CHECK(!game.getState().hasDrawnThisTurn);
    ok(game.beginBattle()); check();
    rejected(game, ActionError::WrongPhase, [&] { return game.beginBattle(); }); check();
    rejected(game, ActionError::WrongPhase, [&] { return game.drawCard(1); }); check();
    rejected(game, ActionError::WrongPhase, [&] { return game.summonUnit(1, second, 0); }); check();
    ok(game.endTurn()); check();
    CHECK(sizes() == initial);
    const auto top = game.getState().players[0].deck.back().id;
    ok(game.drawCard(0)); check();
    CHECK(game.getState().players[0].hand.size() == initial[0] + 1);
    CHECK(game.getState().players[0].deck.size() == initial[1] - 1);
    CHECK(game.getState().players[0].hand.back().id == top);
    CHECK(game.getState().players[1].hand.size() == initial[2]);
    rejected(game, ActionError::AlreadyDrawn, [&] { return game.drawCard(0); }); check();
    ok(game.summonUnit(0, first, 0)); check();
    CHECK(game.getState().players[0].hp == 12);
    CHECK(!unit(game, 0).hasAttacked);
    ok(game.beginBattle()); check();
    ok(game.attackPlayer(first, 1)); check(); // Newly summoned on turn 3.
    CHECK(game.getState().players[1].hp == 15);
    ok(game.endTurn()); check();
    CHECK(!game.getState().hasDrawnThisTurn);
    CHECK(!unit(game, 0).hasAttacked);
}

void testCombatLifetime() {
    // Two different attackers repeatedly hit a surviving defender; each receives
    // a counterattack. The same defender can initiate an attack on its own turn.
    auto game = started("guard", "knight");
    const auto a = summon(game, 0, 0);
    const auto b = summon(game, 0, 1);
    ok(game.endTurn());
    const auto d = summon(game, 1);
    ok(game.endTurn());
    ok(game.beginBattle());
    const auto ledger = identities(game);
    ok(game.attackUnit(a, d)); invariant(game);
    CHECK(unit(game, 0, 0).currentHP == 1);
    CHECK(unit(game, 1).currentHP == 5);
    CHECK(!unit(game, 1).hasAttacked);
    ok(game.attackUnit(b, d)); invariant(game);
    CHECK(unit(game, 0, 1).currentHP == 1);
    CHECK(unit(game, 1).currentHP == 2);
    CHECK(!unit(game, 1).hasAttacked);
    ok(game.endTurn());
    ok(game.beginBattle());
    ok(game.attackUnit(d, a)); invariant(game);
    CHECK(unit(game, 1).currentHP == 2); // Dead guard cannot counterattack.
    CHECK(unit(game, 1).hasAttacked);
    CHECK(game.getState().players[0].tomb.back().id == a);
    CHECK(game.getState().players[0].tomb.back().currentHP == -4);
    CHECK(game.getState().players[0].hp == 4); // 20 - 6 - 6 - 4 overflow.
    CHECK(identities(game) == ledger);
    rejected(game, ActionError::AlreadyAttacked, [&] { return game.attackUnit(d, b); });

    // Counterattack kills the attacker, leaving negative HP in Tomb and
    // clamping overflow to zero without ending a player protected by another unit.
    auto counter = started("guard", "knight");
    const auto attacker = summon(counter, 0);
    (void)summon(counter, 0, 1);
    ok(counter.damageUnit(attacker, 5));
    ok(counter.damagePlayer(0, 7)); // HP 1 with two guards.
    ok(counter.endTurn());
    const auto defender = summon(counter, 1);
    ok(counter.endTurn()); ok(counter.beginBattle());
    ok(counter.attackUnit(attacker, defender)); invariant(counter);
    CHECK(counter.getState().players[0].hp == 0);
    CHECK(counter.getState().status == GameStatus::Playing);
    CHECK(counter.getState().players[0].tomb.back().currentHP == -4);
    CHECK(counter.getState().players[0].tomb.back().hasAttacked);
    CHECK(unit(counter, 1).currentHP == 5);
    CHECK(counter.getState().players[1].hp == 12);
}

void checkInactiveActions(Game& game) {
    const std::vector<std::function<ActionError()>> actions = {
        [&] { return game.drawCard(0); }, [&] { return game.summonUnit(0, 1, 0); },
        [&] { return game.beginBattle(); }, [&] { return game.endTurn(); },
        [&] { return game.attackUnit(1, 2); }, [&] { return game.attackPlayer(1, 1); },
        [&] { return game.damagePlayer(0, 1); }, [&] { return game.healPlayer(0, 1); },
        [&] { return game.damageUnit(1, 1); }, [&] { return game.healUnit(1, 1); },
        [&] { return game.moveToTomb(1); }
    };
    for (const auto& action : actions) rejected(game, ActionError::NotPlaying, action);
}

void testLifecycleRejections() {
    Game fresh(database(), {}, 11);
    checkInactiveActions(fresh);
    for (PlayerId loser : {PlayerId{0}, PlayerId{1}}) {
        auto game = started();
        const auto old = identities(game);
        ok(game.damagePlayer(loser, 20));
        CHECK(game.getState().status == (loser == 0 ? GameStatus::Player2Won : GameStatus::Player1Won));
        invariant(game);
        checkInactiveActions(game);
        CHECK(identities(game) == old);
        ok(game.start(mixedDeck(), mixedDeck()));
        invariant(game);
        const auto next = identities(game);
        for (const auto& [id, identity] : next) CHECK(!old.contains(id));
        game.reset();
        CHECK(game.getState().status == GameStatus::NotStarted);
        CHECK(identities(game).empty());
        checkInactiveActions(game);
        ok(game.start(mixedDeck(), mixedDeck()));
        invariant(game);
        for (const auto& [id, identity] : identities(game)) {
            CHECK(!next.contains(id)); CHECK(!old.contains(id));
        }
    }
}

void testHealingBoundaries() {
    auto game = started();
    const auto id = summon(game, 0);
    const auto max = std::numeric_limits<HitPoints>::max();
    const auto min = std::numeric_limits<HitPoints>::min();
    ok(game.damageUnit(id, 7));
    ok(game.healUnit(id, max));
    CHECK(unit(game, 0).currentHP == 8);
    ok(game.healPlayer(0, max - 12));
    CHECK(game.getState().players[0].hp == max);
    for (const auto amount : {HitPoints{1}, max, min}) {
        rejected(game, ActionError::InvalidAmount, [&] { return game.healPlayer(0, amount); });
        invariant(game);
    }
    rejected(game, ActionError::InvalidAmount, [&] { return game.healUnit(id, min); });
    rejected(game, ActionError::InvalidAmount, [&] { return game.damageUnit(id, min); });
    rejected(game, ActionError::InvalidAmount, [&] { return game.damagePlayer(0, min); });
    ok(game.healPlayer(0, 0));
    ok(game.damagePlayer(0, max));
    CHECK(game.getState().players[0].hp == 0);
    CHECK(game.getState().status == GameStatus::Playing);
    ok(game.healPlayer(0, max));
    ok(game.damageUnit(id, max));
    CHECK(game.getState().players[0].tomb.back().currentHP == 8 - max);
    CHECK(game.getState().players[0].hp == max); // Engine damage has no overflow.
    rejected(game, ActionError::InvalidInstance, [&] { return game.healUnit(id, max); });
    invariant(game);
}

void testCompleteSimulations() {
    struct Result { std::string trace; std::size_t actions; std::uint64_t turns; GameStatus status; };
    auto run = [](std::uint32_t seed) {
        Game game(database(), gameplayConfig(), seed);
        ok(game.start(mixedDeck(), mixedDeck()));
        const auto ledger = identities(game);
        std::ostringstream trace;
        std::size_t actions = 0;
        auto step = [&](const std::function<ActionError()>& action) {
            const auto before = snapshot(game);
            const auto error = action();
            if (error != ActionError::None) CHECK(snapshot(game) == before);
            invariant(game);
            CHECK(identities(game) == ledger);
            trace << static_cast<int>(error) << ':' << snapshot(game) << '\n';
            ++actions;
        };
        invariant(game);
        while (game.getState().status == GameStatus::Playing && game.getState().turnNumber <= 256) {
            const auto player = game.getState().activePlayer;
            const auto opponent = 1 - player;
            // Deliberately rejected calls are audited just like successful calls.
            step([&] { return game.drawCard(opponent); });
            step([&] { return game.drawCard(player); });
            step([&] { return game.drawCard(player); });
            std::vector<InstanceId> hand;
            for (const auto& card : game.getState().players[player].hand) hand.push_back(card.id);
            for (std::size_t zone = 0; zone < 6 && zone < hand.size(); ++zone)
                step([&] { return game.summonUnit(player, hand[zone], zone); });
            step([&] { return game.beginBattle(); });
            step([&] { return game.drawCard(player); });
            std::vector<InstanceId> attackers;
            for (const auto& zone : game.getState().players[player].board.zones())
                if (zone) attackers.push_back(zone->id);
            for (const auto attacker : attackers) {
                if (game.getState().status != GameStatus::Playing) break;
                InstanceId defender = 0;
                for (const auto& zone : game.getState().players[opponent].board.zones())
                    if (zone) { defender = zone->id; break; }
                step([&] { return defender ? game.attackUnit(attacker, defender)
                                          : game.attackPlayer(attacker, opponent); });
            }
            if (game.getState().status == GameStatus::Playing) {
                const auto turn = game.getState().turnNumber;
                const auto hand0 = game.getState().players[0].hand.size();
                const auto hand1 = game.getState().players[1].hand.size();
                step([&] { return game.endTurn(); });
                CHECK(game.getState().turnNumber == turn + 1);
                CHECK(game.getState().activePlayer == opponent);
                CHECK(game.getState().phase == Phase::Main);
                CHECK(!game.getState().hasDrawnThisTurn);
                CHECK(game.getState().players[0].hand.size() == hand0);
                CHECK(game.getState().players[1].hand.size() == hand1);
            }
        }
        CHECK(game.getState().status != GameStatus::Playing);
        checkInactiveActions(game);
        invariant(game);
        CHECK(identities(game) == ledger);
        return Result{trace.str(), actions, game.getState().turnNumber, game.getState().status};
    };
    for (std::uint32_t seed = 0; seed < 32; ++seed) {
        const auto first = run(seed);
        const auto replay = run(seed);
        CHECK(first.trace == replay.trace);
        CHECK(first.actions == replay.actions);
        CHECK(first.turns == replay.turns);
        CHECK(first.status == replay.status);
        std::cout << "seed=" << seed << " turns=" << first.turns << " actions=" << first.actions
                  << " winner=" << (first.status == GameStatus::Player1Won ? 1 :
                                     first.status == GameStatus::Player2Won ? 2 : 0) << '\n';
    }
}

int main(int argc, char** argv) {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"cards", testCards}, {"json", testJsonValidation}, {"start", testStart},
        {"shuffle", testShuffleAndSnapshot}, {"config", testConfig}, {"config_json", testConfigJson}, {"load_order", testLoadOrder},
        {"draw", testDrawAndEmptyDeck}, {"summon", testSummon}, {"zero_hp", testZeroHPAndTomb},
        {"turns", testTurnAndFirstTurn}, {"survival", testCombatSurvival},
        {"spillover", testCombatSpillover}, {"counterattack", testCounterattackSpillover},
        {"targets", testTargets}, {"direct_attack", testDirectAttackAndGameOver},
        {"hp", testHPAndEngineOperations}, {"game_over", testCombatGameOverAndLimits},
        {"simulations", testSimulations},
        {"manual_phases", testManualPhases}, {"combat_lifetime", testCombatLifetime},
        {"lifecycle_rejections", testLifecycleRejections}, {"healing_boundaries", testHealingBoundaries},
        {"complete_simulations", testCompleteSimulations}
    };
    bool selected = false;
    for (const auto& [name, run] : tests) {
        if (argc > 1 && name != argv[1]) continue;
        selected = true;
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return selected ? 0 : 2;
}
