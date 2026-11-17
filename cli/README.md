# Soulbound CLI

Build and run with a C++20 compiler and CMake 3.16+:

```sh
cmake -S . -B build
cmake --build build
./build/cli/soulbound_cli
```

The default data directory is the absolute source `data/` path recorded by CMake.
The executable can be launched from any working directory while that directory
exists. To use another directory (or move the executable), pass its path:

```sh
/path/to/soulbound_cli /absolute/path/to/data
```

A supplied relative path is resolved against the caller's working directory.
The directory must contain `game_config.json` and `cards.json`, and a `decks/` subdirectory. Deck card IDs must exist in `cards.json`.
The default selections are `starter-knight` and `starter-guard`, loaded from
`decks/<id>.json`. Both sample decks contain 15 Knights and 15 Guards.
Core validates their size against the configuration before starting.

List available decks or choose two by ID:

```sh
./build/cli/soulbound_cli --list-decks
./build/cli/soulbound_cli data --list-decks
./build/cli/soulbound_cli data starter-guard starter-knight
```

Drafts can be listed but cannot start a match until their total equals deckSize.
Deck CRUD and validation live in Core. See [Deck API](../docs/deck-management.md).

## Commands

Commands are case-sensitive. Enter one command per line. Numeric arguments must
be unsigned decimal integers; negatives, overflow, trailing characters, missing
arguments and extra arguments produce errors without dispatching an action.

| Command | Behavior |
| --- | --- |
| `help` | List commands and indexing conventions. |
| `state` | Print only Active Player, Phase and Draw available. |
| `hand` | Show the active player's cards, InstanceIds, names, ATK and current/printed HP. |
| `board` | Show both players' HP, deck/hand counts, hands and numbered board zones, plus turn and phase. |
| `draw` | Ask Core to draw one card for the active player. |
| `summon <instanceId> <zoneIndex>` | Summon a card from the active hand into a zone. |
| `battle` | Enter Battle Phase. |
| `attack <attackerInstanceId> <defenderInstanceId>` | Attack an opposing unit. |
| `direct <attackerInstanceId>` | Attack the opposing player. |
| `end` | End the turn and switch active player. |
| `quit` | Exit. EOF also exits cleanly. |

Players are displayed as **1 and 2** (Core uses 0 and 1). Board zones are
**zero-based**, normally 0–5; use the indices printed by `board`. The number after
`#` is an InstanceId, not a hand position or a card definition ID.

For example, if `hand` lists `Knight #17` and zone 0 is empty:

```text
hand
draw
summon 17 0
board
battle
end
```

The second player can inspect their hand, summon, and end their turn. On Player
1's next turn, if Knight #17 survived, choose a target shown by `board`:

```text
battle
attack 17 42
```

Replace `42` with an actual opposing unit InstanceId. If the opposing board is
empty, use `direct 17` instead. Each successful action prints the updated match
and any game result. Core errors are printed as readable messages; the CLI never
repairs or mutates state itself. After a result, inspection commands remain
available and Core rejects further gameplay actions.

Drawing is manual, once per turn; `end` does not draw. Core enforces phase,
ownership, first-turn attack restrictions, HP costs, combat and victory rules.
Both hands are visible in this local simulator. All state displays read a const
reference from `game.getState()`; the CLI stores only references to Game and card
metadata for presentation.

## Tests

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^cli\.' --output-on-failure
```

`cli.parsing_dispatch` exercises malformed input, numeric boundaries, display,
manual drawing, summoning, phase/turn changes, unit combat and Core error output.
`cli.full_match` runs the actual executable with a command script and explicit
fixture data through a Player 1 victory on turn 9, then checks post-game rejection
and clean exit. Its one-card decks make InstanceIds deterministic without relying
on a shuffle seed. Run that script directly with:

```sh
./build/cli/soulbound_cli tests/cli/fixtures < tests/cli/fixtures/full_match.txt
```
