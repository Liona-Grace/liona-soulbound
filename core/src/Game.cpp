#include "soulbound/Game.hpp"

#include "soulbound/CombatSystem.hpp"
#include "soulbound/JsonLoader.hpp"
#include "soulbound/DeckValidator.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace soulbound {
namespace {

bool canDamage(HitPoints hp, HitPoints amount) {
    return amount >= 0 && hp >= std::numeric_limits<HitPoints>::min() + amount;
}

HitPoints remainingPlayerHP(HitPoints hp, HitPoints damage) {
    return damage >= hp ? 0 : hp - damage;
}

bool canHeal(HitPoints hp, HitPoints amount) {
    return amount >= 0 && hp <= std::numeric_limits<HitPoints>::max() - amount;
}

} // namespace

Game::Game(CardDatabase database, GameConfig config, std::uint32_t seed)
    : database_(std::move(database)), config_(config), random_(seed) {
    if (!config_.isValid()) {
        throw std::invalid_argument("Invalid GameConfig");
    }
    reset();
}

Game::Game(CardDatabase database, GameConfig config, DeckDefinition player1Deck,
           DeckDefinition player2Deck, std::uint32_t seed)
    : Game(std::move(database), config, seed) {
    deckDefinitions_ = std::array<DeckDefinition, 2>{std::move(player1Deck), std::move(player2Deck)};
}

ActionError Game::start() {
    if (!deckDefinitions_) return ActionError::InvalidDeck;
    for (const auto& deck : *deckDefinitions_) {
        const auto result = DeckValidator::validate(deck, database_, config_, DeckValidationMode::Play);
        if (!result.isValid()) return result.error == DeckValidationError::UnknownCard
            ? ActionError::UnknownCard : ActionError::InvalidDeck;
    }
    std::array<Deck, 2> expanded;
    for (std::size_t i = 0; i < expanded.size(); ++i) {
        expanded[i].reserve(config_.deckSize);
        for (const auto& entry : (*deckDefinitions_)[i].cards)
            expanded[i].insert(expanded[i].end(), static_cast<std::size_t>(entry.count), entry.cardId);
    }
    return start(expanded[0], expanded[1]);
}

Game Game::fromFiles(const std::filesystem::path& configPath,
                     const std::filesystem::path& cardsPath, std::uint32_t seed) {
    auto config = JsonLoader::loadGameConfig(configPath);
    auto database = JsonLoader::loadCards(cardsPath);
    return Game(std::move(database), config, seed);
}

const GameConfig& Game::getConfig() const noexcept { return config_; }
const GameState& Game::getState() const noexcept { return state_; }
const GameState& Game::state() const noexcept { return getState(); }

void Game::reset() {
    state_ = GameState{};
    // Keep nextInstanceId_ increasing across restarts of this Game object.
}

ActionError Game::start(const Deck& player1Deck, const Deck& player2Deck) {
    const std::array<const Deck*, 2> decks{&player1Deck, &player2Deck};
    for (const auto* deck : decks) {
        if (deck->size() != config_.deckSize) return ActionError::InvalidDeck;
        for (const auto& id : *deck) {
            if (!database_.find(id)) return ActionError::UnknownCard;
        }
    }
    if (config_.deckSize > (std::numeric_limits<InstanceId>::max() - nextInstanceId_) / 2)
        return ActionError::InvalidDeck;

    GameState next;
    auto nextId = nextInstanceId_;
    auto nextRandom = random_;
    for (PlayerId id = 0; id < next.players.size(); ++id) {
        auto& player = next.players[id];
        player.hp = config_.startingHP;
        player.board = Board(config_.boardSize);
        player.deck.reserve(config_.deckSize);
        player.hand.reserve(config_.deckSize);
        player.tomb.reserve(config_.deckSize);
        for (const auto& definitionId : *decks[id]) {
            const auto& definition = *database_.find(definitionId);
            player.deck.push_back({nextId++, definitionId, id, definition.hp, false});
        }
        std::shuffle(player.deck.begin(), player.deck.end(), nextRandom);
        for (std::size_t draw = 0; draw < config_.startingHandSize; ++draw) {
            player.hand.push_back(std::move(player.deck.back()));
            player.deck.pop_back();
        }
    }
    next.status = GameStatus::Playing;
    state_ = std::move(next);
    nextInstanceId_ = nextId;
    random_ = nextRandom;
    return ActionError::None;
}

ActionError Game::checkPlayerAction(PlayerId player, Phase phase) const {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    if (player >= state_.players.size()) return ActionError::InvalidPlayer;
    if (player != state_.activePlayer) return ActionError::WrongPlayer;
    if (state_.phase != phase) return ActionError::WrongPhase;
    return ActionError::None;
}

ActionError Game::drawCard(PlayerId player) {
    const auto error = checkPlayerAction(player, Phase::Main);
    if (error != ActionError::None) return error;
    auto& current = state_.players[player];
    if (state_.hasDrawnThisTurn) return ActionError::AlreadyDrawn;
    if (current.deck.empty()) {
        // TODO: Revisit the consequence of drawing from an empty deck.
        // For now the action fails without damage, defeat or any state change.
        return ActionError::EmptyDeck;
    }
    current.hand.push_back(std::move(current.deck.back()));
    current.deck.pop_back();
    state_.hasDrawnThisTurn = true;
    checkGameOver();
    return ActionError::None;
}

ActionError Game::summonUnit(PlayerId player, InstanceId card, std::size_t zone) {
    const auto error = checkPlayerAction(player, Phase::Main);
    if (error != ActionError::None) return error;
    auto& current = state_.players[player];
    if (zone >= current.board.zones_.size()) return ActionError::InvalidZone;
    if (current.board.zones_[zone]) return ActionError::OccupiedZone;
    const auto it = std::find_if(current.hand.begin(), current.hand.end(),
                                 [card](const auto& instance) { return instance.id == card; });
    if (it == current.hand.end()) return ActionError::InvalidInstance;
    const auto cost = database_.find(it->definitionId)->hp;
    if (current.hp < cost) return ActionError::InsufficientHP;
    current.board.zones_[zone] = std::move(*it);
    current.hand.erase(it);
    current.hp -= cost;
    // Check only after the unit is placed: paying down to zero is legal.
    checkGameOver();
    return ActionError::None;
}

ActionError Game::beginBattle() {
    const auto error = checkPlayerAction(state_.activePlayer, Phase::Main);
    if (error != ActionError::None) return error;
    state_.phase = Phase::Battle;
    return ActionError::None;
}

ActionError Game::endTurn() {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    state_.activePlayer = 1 - state_.activePlayer;
    ++state_.turnNumber;
    state_.hasDrawnThisTurn = false;
    state_.phase = Phase::Main;
    for (auto& player : state_.players) {
        for (auto& zone : player.board.zones_) {
            if (zone) zone->hasAttacked = false;
        }
    }
    return ActionError::None;
}

std::optional<Game::Location> Game::locateUnit(InstanceId instance) const {
    for (PlayerId id = 0; id < state_.players.size(); ++id) {
        const auto& zones = state_.players[id].board.zones_;
        for (std::size_t zone = 0; zone < zones.size(); ++zone) {
            if (zones[zone] && zones[zone]->id == instance) return Location{id, zone};
        }
    }
    return std::nullopt;
}

CardInstance& Game::unitAt(Location location) {
    return *state_.players[location.player].board.zones_[location.zone];
}

ActionError Game::checkAttacker(InstanceId attacker) const {
    const auto error = checkPlayerAction(state_.activePlayer, Phase::Battle);
    if (error != ActionError::None) return error;
    if (state_.turnNumber == 1) return ActionError::FirstTurn;
    const auto location = locateUnit(attacker);
    if (!location) return ActionError::InvalidInstance;
    if (location->player != state_.activePlayer) return ActionError::WrongPlayer;
    if (state_.players[location->player].board.zones_[location->zone]->hasAttacked)
        return ActionError::AlreadyAttacked;
    return ActionError::None;
}

ActionError Game::attackUnit(InstanceId attackerId, InstanceId defenderId) {
    const auto error = checkAttacker(attackerId);
    if (error != ActionError::None) return error;
    const auto attackerLocation = *locateUnit(attackerId);
    const auto defenderLocation = locateUnit(defenderId);
    if (!defenderLocation) return ActionError::InvalidInstance;
    if (defenderLocation->player == attackerLocation.player) return ActionError::InvalidTarget;
    const auto& attacker = unitAt(attackerLocation);
    const auto& defender = unitAt(*defenderLocation);

    // Resolve on copies so numeric validation cannot leave a partially applied combat.
    auto nextAttacker = attacker;
    auto nextDefender = defender;
    const auto result = CombatSystem::resolve(
        *database_.find(attacker.definitionId), nextAttacker,
        *database_.find(defender.definitionId), nextDefender);
    if (!canDamage(state_.players[attackerLocation.player].hp, result.damageToAttackerOwner) ||
        !canDamage(state_.players[defenderLocation->player].hp, result.damageToDefenderOwner))
        return ActionError::InvalidAmount;

    nextAttacker.hasAttacked = true;
    unitAt(attackerLocation) = std::move(nextAttacker);
    unitAt(*defenderLocation) = std::move(nextDefender);
    auto& attackerHP = state_.players[attackerLocation.player].hp;
    attackerHP = remainingPlayerHP(attackerHP, result.damageToAttackerOwner);
    auto& defenderHP = state_.players[defenderLocation->player].hp;
    defenderHP = remainingPlayerHP(defenderHP, result.damageToDefenderOwner);
    if (unitAt(attackerLocation).currentHP <= 0) bury(attackerLocation);
    if (unitAt(*defenderLocation).currentHP <= 0) bury(*defenderLocation);
    checkGameOver();
    return ActionError::None;
}

ActionError Game::attackPlayer(InstanceId attackerId, PlayerId defender) {
    const auto error = checkAttacker(attackerId);
    if (error != ActionError::None) return error;
    if (defender >= state_.players.size()) return ActionError::InvalidPlayer;
    const auto location = *locateUnit(attackerId);
    if (defender == location.player || !state_.players[defender].board.empty())
        return ActionError::InvalidTarget;
    auto& attacker = unitAt(location);
    const auto damage = database_.find(attacker.definitionId)->attack;
    if (!canDamage(state_.players[defender].hp, damage)) return ActionError::InvalidAmount;
    attacker.hasAttacked = true;
    auto& hp = state_.players[defender].hp;
    hp = remainingPlayerHP(hp, damage);
    checkGameOver();
    return ActionError::None;
}

void Game::bury(Location location) {
    auto& player = state_.players[location.player];
    auto& zone = player.board.zones_[location.zone];
    player.tomb.push_back(std::move(*zone));
    zone.reset();
}

ActionError Game::moveToTomb(InstanceId instance) {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    const auto location = locateUnit(instance);
    if (!location) return ActionError::InvalidInstance;
    bury(*location);
    checkGameOver();
    return ActionError::None;
}

ActionError Game::damagePlayer(PlayerId player, HitPoints amount) {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    if (player >= state_.players.size()) return ActionError::InvalidPlayer;
    auto& hp = state_.players[player].hp;
    if (!canDamage(hp, amount)) return ActionError::InvalidAmount;
    hp = remainingPlayerHP(hp, amount);
    checkGameOver();
    return ActionError::None;
}

ActionError Game::healPlayer(PlayerId player, HitPoints amount) {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    if (player >= state_.players.size()) return ActionError::InvalidPlayer;
    auto& hp = state_.players[player].hp;
    if (!canHeal(hp, amount)) return ActionError::InvalidAmount;
    hp += amount;
    checkGameOver();
    return ActionError::None;
}

ActionError Game::damageUnit(InstanceId instance, HitPoints amount) {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    const auto location = locateUnit(instance);
    if (!location) return ActionError::InvalidInstance;
    auto& unit = unitAt(*location);
    if (!canDamage(unit.currentHP, amount)) return ActionError::InvalidAmount;
    unit.currentHP -= amount;
    // Generic engine damage has no spillover: only combat transfers excess damage.
    if (unit.currentHP <= 0) bury(*location);
    checkGameOver();
    return ActionError::None;
}

ActionError Game::healUnit(InstanceId instance, HitPoints amount) {
    if (state_.status != GameStatus::Playing) return ActionError::NotPlaying;
    const auto location = locateUnit(instance);
    if (!location) return ActionError::InvalidInstance;
    auto& hp = unitAt(*location).currentHP;
    if (amount < 0) return ActionError::InvalidAmount;
    const auto baseHP = database_.find(unitAt(*location).definitionId)->hp;
    hp += std::min(amount, baseHP - hp);
    checkGameOver();
    return ActionError::None;
}

GameStatus Game::evaluateOutcome(const GameState& state) noexcept {
    if (state.status == GameStatus::NotStarted) return GameStatus::NotStarted;
    const auto lost = [](const PlayerState& player) {
        return player.hp <= 0 && player.board.empty();
    };
    const bool firstLost = lost(state.players[0]);
    const bool secondLost = lost(state.players[1]);
    if (firstLost && secondLost) return GameStatus::Draw;
    if (firstLost) return GameStatus::Player2Won;
    if (secondLost) return GameStatus::Player1Won;
    return GameStatus::Playing;
}

void Game::checkGameOver() {
    state_.status = evaluateOutcome(state_);
}

} // namespace soulbound
