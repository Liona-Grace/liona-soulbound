#pragma once

#include "soulbound/CardDatabase.hpp"
#include "soulbound/GameConfig.hpp"
#include "soulbound/GameState.hpp"

#include <random>
#include <filesystem>
#include <string>
#include <vector>

namespace soulbound {

enum class ActionError {
    None, NotPlaying, InvalidPlayer, WrongPlayer, WrongPhase, FirstTurn,
    InvalidDeck, UnknownCard, InvalidInstance, InvalidZone, OccupiedZone,
    InsufficientHP, EmptyDeck, AlreadyAttacked, InvalidTarget, InvalidAmount, AlreadyDrawn
};

class Game {
public:
    using Deck = std::vector<std::string>;

    // Database is copied/moved into the Game and cannot change during a match.
    // An explicit seed makes simulations and tests reproducible.
    explicit Game(CardDatabase database, GameConfig config = {},
                  std::uint32_t seed = std::random_device{}());

    // Config is loaded and validated before attempting to read card definitions.
    [[nodiscard]] static Game fromFiles(const std::filesystem::path& configPath,
                                       const std::filesystem::path& cardsPath,
                                       std::uint32_t seed = std::random_device{}());
    [[nodiscard]] const GameConfig& getConfig() const noexcept;

    [[nodiscard]] ActionError start(const Deck& player1Deck, const Deck& player2Deck);
    [[nodiscard]] const GameState& getState() const noexcept;
    [[nodiscard]] const GameState& state() const noexcept; // Compatibility alias.
    [[nodiscard]] static GameStatus evaluateOutcome(const GameState& state) noexcept;
    void reset();

    [[nodiscard]] ActionError drawCard(PlayerId player);
    [[nodiscard]] ActionError summonUnit(PlayerId player, InstanceId card, std::size_t zone);
    [[nodiscard]] ActionError beginBattle();
    [[nodiscard]] ActionError endTurn();
    [[nodiscard]] ActionError attackUnit(InstanceId attacker, InstanceId defender);
    [[nodiscard]] ActionError attackPlayer(InstanceId attacker, PlayerId defender);

    // Engine operations, not player commands. Adapters should expose only actions
    // allowed by their command interface; these still validate state and amounts.
    [[nodiscard]] ActionError moveToTomb(InstanceId instance);
    [[nodiscard]] ActionError damagePlayer(PlayerId player, HitPoints amount);
    [[nodiscard]] ActionError damageUnit(InstanceId instance, HitPoints amount);
    [[nodiscard]] ActionError healPlayer(PlayerId player, HitPoints amount);
    [[nodiscard]] ActionError healUnit(InstanceId instance, HitPoints amount);

private:
    struct Location { PlayerId player; std::size_t zone; };
    CardDatabase database_;
    GameConfig config_;
    std::mt19937 random_;
    GameState state_;
    InstanceId nextInstanceId_ = 1;

    [[nodiscard]] ActionError checkPlayerAction(PlayerId player, Phase phase) const;
    [[nodiscard]] ActionError checkAttacker(InstanceId attacker) const;
    [[nodiscard]] std::optional<Location> locateUnit(InstanceId instance) const;
    CardInstance& unitAt(Location location);
    void bury(Location location);
    void checkGameOver();
};

} // namespace soulbound
