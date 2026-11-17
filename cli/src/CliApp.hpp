#pragma once
#include "soulbound/Game.hpp"
#include <iosfwd>
#include <string>

namespace soulbound::cli {
const char* errorMessage(ActionError error);
class CliApp {
public:
    CliApp(Game& game, const CardDatabase& cards, std::ostream& output)
        : game_(game), cards_(cards), out_(output) {}
    void run(std::istream& input);
    // Returns false for quit. Input errors leave the match untouched.
    bool execute(const std::string& line);
private:
    Game& game_;
    const CardDatabase& cards_;
    std::ostream& out_;
    void printCard(const CardInstance& card);
    void printState();
    void printTurnState();
    void printHand();
    void announceResult();
};
}
