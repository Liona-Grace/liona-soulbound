#include "CliApp.hpp"
#include "soulbound/JsonLoader.hpp"
#include "soulbound/DeckManager.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    try {
        const bool listDecks = (argc == 2 && std::string(argv[1]) == "--list-decks")
            || (argc == 3 && std::string(argv[2]) == "--list-decks");
        if (!listDecks && (argc > 4 || argc == 3)) {
            std::cerr << "Usage: soulbound_cli [data-directory [player1-deck-id player2-deck-id]]\n       soulbound_cli [data-directory] --list-decks\n";
            return 1;
        }
        const std::filesystem::path dataDir = argc >= 2 && std::string(argv[1]) != "--list-decks"
            ? std::filesystem::absolute(argv[1]) : std::filesystem::path{SOULBOUND_DATA_DIR};
        std::cout << "SOULBOUND - CLI Simulator\n";
        const auto config = soulbound::JsonLoader::loadGameConfig(dataDir / "game_config.json");
        const auto cards = soulbound::CardDatabase::fromFile(dataDir / "cards.json");
        const soulbound::DeckManager decks(cards, config, dataDir / "decks");
        if (listDecks) {
            for (const auto& deck : decks.getAll()) {
                const auto result = decks.validate(deck, soulbound::DeckValidationMode::Play);
                std::cout << deck.id << " | " << deck.name << " | " << result.totalCards
                          << '/' << config.deckSize << (result.isValid() ? " | ready" : " | draft") << '\n';
            }
            return 0;
        }
        const std::string firstId = argc == 4 ? argv[2] : "starter-knight";
        const std::string secondId = argc == 4 ? argv[3] : "starter-guard";
        const auto first = decks.get(firstId);
        const auto second = decks.get(secondId);
        if (!first || !second) {
            std::cerr << "Deck not found: " << (!first ? firstId : secondId) << '\n';
            return 1;
        }
        soulbound::Game game(cards, config, *first, *second);
        const auto error = game.start();
        if (error != soulbound::ActionError::None) {
            std::cerr << "Failed to start game: " << soulbound::cli::errorMessage(error) << '\n';
            return 1;
        }
        std::cout << "Game started!\n";
        soulbound::cli::CliApp app(game, cards, std::cout);
        app.run(std::cin);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
