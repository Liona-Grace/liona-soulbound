#include "CliApp.hpp"
#include "CommandParser.hpp"
#include <iostream>
#include <string>

namespace soulbound::cli {

const char* errorMessage(soulbound::ActionError error) {
    using soulbound::ActionError;
    switch (error) {
    case ActionError::None: return "No error.";
    case ActionError::NotPlaying: return "Game is not in progress.";
    case ActionError::InvalidPlayer: return "Invalid player.";
    case ActionError::WrongPlayer: return "It is another player's turn.";
    case ActionError::WrongPhase: return "Action is unavailable in this phase.";
    case ActionError::FirstTurn: return "Attacking is unavailable on the first turn.";
    case ActionError::InvalidDeck: return "Deck violates the configured deck-building rules.";
    case ActionError::UnknownCard: return "Deck contains an unknown card.";
    case ActionError::InvalidInstance: return "Card instance was not found.";
    case ActionError::InvalidZone: return "Invalid board zone.";
    case ActionError::OccupiedZone: return "Board zone is occupied.";
    case ActionError::InsufficientHP: return "Not enough HP to summon this unit.";
    case ActionError::EmptyDeck: return "Deck is empty.";
    case ActionError::AlreadyAttacked: return "Unit has already attacked this turn.";
    case ActionError::InvalidTarget: return "Invalid attack target.";
    case ActionError::InvalidAmount: return "Invalid HP amount.";
    case ActionError::AlreadyDrawn: return "Player has already drawn this turn.";
    }
    return "Unknown action error.";
}

const char* phaseName(soulbound::Phase phase) {
    return phase == soulbound::Phase::Main ? "Main" : "Battle";
}

void CliApp::printCard(const soulbound::CardInstance& card) {
    const auto* definition = cards_.find(card.definitionId);
    out_ << (definition ? definition->name : card.definitionId) << " #" << card.id;
    if (definition) out_ << " (ATK " << definition->attack << " | HP "
                         << card.currentHP << '/' << definition->hp << ')';
    else out_ << " (HP " << card.currentHP << ')';
}

void CliApp::printState() {
    const auto& state = game_.getState();
    out_ << "\nTurn " << state.turnNumber << " | Player "
              << state.activePlayer + 1 << " | " << phaseName(state.phase)
              << " Phase\n\n";
    for (std::size_t id = 0; id < state.players.size(); ++id) {
        const auto& player = state.players[id];
        out_ << "Player " << id + 1 << ": HP " << player.hp
                  << " | Deck " << player.deck.size()
                  << " | Hand " << player.hand.size() << '\n';
        out_ << "  Hand:";
        if (player.hand.empty()) out_ << " empty";
        for (const auto& card : player.hand) {
            out_ << " [";
            printCard(card);
            out_ << ']';
        }
        out_ << "\n  Board:";
        const auto& zones = player.board.zones();
        for (std::size_t zone = 0; zone < zones.size(); ++zone) {
            out_ << " [" << zone << ": ";
            if (zones[zone]) printCard(*zones[zone]);
            else out_ << "empty";
            out_ << ']';
        }
        out_ << '\n';
    }
    out_ << '\n';
}

void CliApp::printTurnState() {
    const auto& state = game_.getState();
    const bool canDraw = state.status == soulbound::GameStatus::Playing
        && state.phase == soulbound::Phase::Main && !state.hasDrawnThisTurn
        && !state.players[state.activePlayer].deck.empty();
    out_ << "Active Player: " << state.activePlayer + 1
              << "\nPhase: " << phaseName(state.phase)
              << "\nDraw available: " << (canDraw ? "Yes" : "No") << "\n\n";
}


void CliApp::printHand() {
    const auto& state = game_.getState();
    out_ << "Player " << state.activePlayer + 1 << " Hand:\n";
    const auto& hand = state.players[state.activePlayer].hand;
    if (hand.empty()) out_ << "  empty\n";
    for (const auto& card : hand) { printCard(card); out_ << '\n'; }
}

void CliApp::announceResult() {
    switch (game_.getState().status) {
    case GameStatus::Player1Won: out_ << "Game over: Player 1 wins!\n"; break;
    case GameStatus::Player2Won: out_ << "Game over: Player 2 wins!\n"; break;
    case GameStatus::Draw: out_ << "Game over: Draw!\n"; break;
    default: break;
    }
}

bool CliApp::execute(const std::string& line) {
    const auto command = parseCommand(line);
    if (!command.error.empty()) { out_ << command.error << '\n'; return true; }
    const auto player = game_.getState().activePlayer;
    ActionError error = ActionError::None;
    switch (command.kind) {
    case CommandKind::Empty: return true;
    case CommandKind::Quit: return false;
    case CommandKind::Help:
        out_ << "Commands: help, state, quit, draw, summon <instanceId> <zoneIndex>, "
                "battle, attack <attackerInstanceId> <defenderInstanceId>, "
                "direct <attackerInstanceId>, end, hand, board\n"
                "Players are displayed as 1 and 2. Zones are zero-based.\n";
        return true;
    case CommandKind::State: printTurnState(); return true;
    case CommandKind::Hand: printHand(); return true;
    case CommandKind::Board: printState(); return true;
    case CommandKind::Draw: error = game_.drawCard(player); break;
    case CommandKind::Summon: error = game_.summonUnit(player, command.first, command.second); break;
    case CommandKind::Battle: error = game_.beginBattle(); break;
    case CommandKind::Attack: error = game_.attackUnit(command.first, command.second); break;
    case CommandKind::Direct: error = game_.attackPlayer(command.first, 1 - player); break;
    case CommandKind::End: error = game_.endTurn(); break;
    }
    if (error != ActionError::None) out_ << "Error: " << errorMessage(error) << '\n';
    else {
        out_ << "Action succeeded.\n";
        printState();
        announceResult();
    }
    return true;
}

void CliApp::run(std::istream& input) {
    printState();
    announceResult();
    std::string line;
    while (out_ << "> " << std::flush, std::getline(input, line)) {
        if (!execute(line)) break;
    }
    out_ << "Goodbye!\n";
}

} // namespace soulbound::cli
