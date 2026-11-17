#include "CommandParser.hpp"
#include <charconv>
#include <limits>
#include <sstream>
#include <string_view>

namespace soulbound::cli {
Command parseCommand(const std::string& line) {
    struct Spec { std::string_view name; CommandKind kind; int arguments; const char* usage; };
    static constexpr Spec specs[] = {
        {"help", CommandKind::Help, 0, "help"}, {"state", CommandKind::State, 0, "state"},
        {"quit", CommandKind::Quit, 0, "quit"}, {"draw", CommandKind::Draw, 0, "draw"},
        {"summon", CommandKind::Summon, 2, "summon <instanceId> <zoneIndex>"},
        {"battle", CommandKind::Battle, 0, "battle"},
        {"attack", CommandKind::Attack, 2, "attack <attackerInstanceId> <defenderInstanceId>"},
        {"direct", CommandKind::Direct, 1, "direct <attackerInstanceId>"},
        {"end", CommandKind::End, 0, "end"}, {"hand", CommandKind::Hand, 0, "hand"},
        {"board", CommandKind::Board, 0, "board"}
    };
    Command result;
    std::istringstream input(line);
    std::string name;
    if (!(input >> name)) return result;
    for (const auto& spec : specs) {
        if (name != spec.name) continue;
        result.kind = spec.kind;
        for (int i = 0; i < spec.arguments; ++i) {
            std::string token;
            if (!(input >> token)) { result.error = std::string("Usage: ") + spec.usage; return result; }
            auto& value = i == 0 ? result.first : result.second;
            const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
            if (error != std::errc{} || end != token.data() + token.size()) {
                result.error = "Invalid number: expected a non-negative decimal integer within range.";
                return result;
            }
        }
        std::string extra;
        if (input >> extra) result.error = std::string("Usage: ") + spec.usage;
        if (result.kind == CommandKind::Summon && result.second > std::numeric_limits<std::size_t>::max())
            result.error = "Invalid number: zone index is out of range.";
        return result;
    }
    result.error = "Unknown command. Type help for available commands.";
    return result;
}
}
