#pragma once
#include <cstdint>
#include <string>

namespace soulbound::cli {
enum class CommandKind { Empty, Help, State, Quit, Draw, Summon, Battle, Attack, Direct, End, Hand, Board };
struct Command {
    CommandKind kind = CommandKind::Empty;
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    std::string error;
};
Command parseCommand(const std::string& line);
}
