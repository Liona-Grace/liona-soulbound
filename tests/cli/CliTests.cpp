#include "CliApp.hpp"
#include "CommandParser.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace soulbound;
using namespace soulbound::cli;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        require(parseCommand(" \t\r").kind == CommandKind::Empty, "blank line");
        require(parseCommand(" summon 42 0 \r").first == 42, "whitespace and numeric parsing");
        require(parseCommand("attack 1 18446744073709551615").error.empty(), "uint64 max ID");
        for (const auto* line : {"summon", "summon 1", "summon 1 0 extra", "attack 1",
             "direct", "state extra", "quit extra", "draw extra", "battle extra", "end extra",
             "hand extra", "board extra", "help extra", "unknown", "summon -1 0",
             "summon 1 -1", "direct +1", "direct 1x", "direct 1.5", "direct abc",
             "attack 1 18446744073709551616"}) {
            require(!parseCommand(line).error.empty(), line);
        }
        CardDatabase cards;
        require(cards.add({"knight", "Knight", 5, 8}), "knight definition");
        require(cards.add({"guard", "Guard", 3, 6}), "guard definition");
        GameConfig config;
        config.deckSize = 4;
        config.maxCopiesPerCard = 4; // Legacy homogeneous combat fixture.
        config.startingHandSize = 2;
        Game game(cards, config, 7);
        require(game.start(Game::Deck(4, "knight"), Game::Deck(4, "guard")) == ActionError::None, "start");
        std::ostringstream output;
        CliApp app(game, cards, output);
        app.execute("state");
        require(output.str() == "Active Player: 1\nPhase: Main\nDraw available: Yes\n\n", "compact state");
        output.str("");
        app.execute("help"); app.execute("hand"); app.execute("board");
        require(output.str().find("Knight #") != std::string::npos, "names and IDs");
        require(output.str().find("ATK 5 | HP 8/8") != std::string::npos, "printed stats");
        require(output.str().find("[0: empty]") != std::string::npos, "zero based zones");
        for (const auto* line : {"draw extra", "summon -1 0", "end extra", "quit extra"}) app.execute(line);
        require(game.getState().turnNumber == 1 && game.getState().players[0].hand.size() == 2, "parse errors do not dispatch");
        app.execute("draw"); app.execute("draw");
        require(game.getState().players[0].hand.size() == 3, "manual draw once");
        require(output.str().find("already drawn") != std::string::npos, "draw error message");
        const auto first = game.getState().players[0].hand.front().id;
        const auto second = game.getState().players[0].hand.back().id;
        app.execute("summon " + std::to_string(first) + " 99");
        require(game.getState().players[0].hp == 20, "invalid zone unchanged");
        app.execute("summon " + std::to_string(first) + " 0");
        require(game.getState().players[0].board.zones()[0]->id == first && game.getState().players[0].hp == 12, "summon dispatch");
        app.execute("summon " + std::to_string(second) + " 0");
        require(output.str().find("occupied") != std::string::npos, "occupied zone error");
        app.execute("battle");
        app.execute("direct " + std::to_string(first));
        require(game.getState().players[1].hp == 20, "first match turn cannot attack");
        app.execute("end");
        require(game.getState().activePlayer == 1 && game.getState().phase == Phase::Main, "end dispatch");
        const auto defender = game.getState().players[1].hand.front().id;
        app.execute("summon " + std::to_string(defender) + " 0");
        app.execute("end"); app.execute("battle");
        app.execute("direct " + std::to_string(first));
        require(game.getState().players[1].hp == 14, "direct blocked by board");
        app.execute("attack " + std::to_string(first) + " " + std::to_string(defender));
        require(game.getState().players[0].board.zones()[0]->currentHP == 5, "counterattack");
        require(game.getState().players[1].board.zones()[0]->currentHP == 1, "attack dispatch");
        app.execute("attack " + std::to_string(first) + " " + std::to_string(defender));
        require(output.str().find("already attacked") != std::string::npos, "attack once error");
        require(!app.execute("quit"), "quit dispatch");
        std::istringstream input("help\nstate\nquit\nshould_not_be_processed\n");
        app.run(input);
        require(output.str().find("Goodbye!") != std::string::npos, "stream loop");
        require(output.str().find("Unknown command") == std::string::npos, "stop reading at quit");
        output.str("");
        std::istringstream eofInput("state\n");
        app.run(eofInput);
        require(output.str().find("Goodbye!") != std::string::npos, "EOF exits cleanly");
        std::cout << "CLI parsing and dispatch passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
