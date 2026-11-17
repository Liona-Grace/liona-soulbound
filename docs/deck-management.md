# Deck management (Milestone 4)

All catalog, deck validation and persistence APIs belong to `soulbound_core` and
have no CLI/UI dependencies. Gameplay rules are unchanged.

## JSON

Each deck lives at `<deck-directory>/<id>.json`; the default application directory
is `data/decks/`. Files use this strict schema:

```json
{
  "id": "my-deck",
  "name": "My Knight Deck",
  "cards": [
    { "cardId": "knight", "count": 20 },
    { "cardId": "guard", "count": 10 }
  ]
}
```

ID must contain only ASCII letters, digits, `-` or `_`; it must match the filename.
The name cannot be blank. Counts are positive signed 64-bit integers. JSON
strings, fractions, booleans, out-of-range integers, unknown/duplicate JSON fields
and mismatched types are rejected. Card IDs must exist in the supplied database.
Repeated entries for the same Card ID are allowed and contribute to the total;
there is no per-Card copy limit.

## API

```cpp
#include "soulbound/DeckManager.hpp"
#include "soulbound/Game.hpp"
#include "soulbound/JsonLoader.hpp"

const auto cards = soulbound::CardDatabase::fromFile("data/cards.json");
const auto config = soulbound::JsonLoader::loadGameConfig("data/game_config.json");
soulbound::DeckManager manager(cards, config, "data/decks");

// Partial or empty decks can be saved as drafts.
soulbound::DeckDefinition draft{"my-deck", "My Deck", {{"knight", 10}}};
manager.create(draft);
auto result = manager.validate(draft, soulbound::DeckValidationMode::Play);
// result.error == IncompleteDeck, result.totalCards == 10.

draft.cards = {{"knight", 20}, {"guard", 10}}; // For default deckSize == 30.
manager.update(draft);
const auto loaded = manager.get("my-deck"); // optional<DeckDefinition>
const auto all = manager.getAll();           // sorted by deck ID

soulbound::Game game(cards, config, *loaded, *loaded, 42);
const auto error = game.start();             // validates Play mode, then deals
// Check error == ActionError::None before playing.
const bool removed = manager.remove("my-deck"); // Game is unaffected.
```

`CardDatabase::find(id)` looks up an immutable definition; `getAll()` returns
independent definitions sorted by ID. `fromFile()` delegates to the existing
strict JSON loader; `JsonLoader::loadCards()` remains supported.

`DeckValidator::validate()` and `DeckManager::validate()` return a
`DeckValidationResult` containing `error`, `message`, `totalCards` and `isValid()`.
Storage mode (the default) accepts totals from zero through `config.deckSize`.
Play mode requires exactly `config.deckSize`. Unknown cards, non-positive counts
and excessive totals fail both modes. Summation checks the remaining capacity
before adding, including when a count is INT64_MAX. The configuration must itself
be valid; an invalid GameConfig throws `std::invalid_argument`.

`DeckManager` owns a database snapshot and config. It validates before creating,
updating, or returning stored definitions, without exposing mutable internal state.

| Operation | Result/error contract |
| --- | --- |
| `getAll()` | Sorted definitions; missing directory is an empty list. Any corrupt `.json` file fails the call; nothing is silently skipped. Other extensions are ignored. |
| `get(id)` | Independent optional value; absent file returns nullopt. Invalid ID, malformed JSON or invalid deck throws. |
| `create(deck)` | Validate draft, then save; existing ID throws invalid_argument. |
| `update(deck)` | Validate draft, then replace; missing ID throws out_of_range. ID is stable; change `name` to rename a deck for display. |
| `remove(id)` | True when deleted, false when absent. Can delete a corrupt JSON file by ID. |
| `validate(deck, mode)` | Structured validation result; no disk access or mutation. |

Malformed JSON/schema and I/O errors throw exceptions (runtime_error or derived
filesystem_error); semantic invalidity uses invalid_argument through Manager.
Failed validation does not replace an existing file. `getAll()` does not create
the directory as a read side effect; the first save creates it.

`DeckStorage` supplies the underlying `getAll`, `get`, `save`, `remove` file API.
It enforces filename/schema safety, while semantic card/config validation belongs
to DeckValidator/DeckManager. Callers using storage directly must validate before
saving. Saves serialize fully first, write into a temporary directory under the
deck root, close/check the file and rename it over the target. Exceptions clean up
the staging directory and failed writes do not truncate the old JSON. Deck-file
symlinks are rejected. The configured root is trusted application configuration.
The API assumes one writer; it does not provide concurrent-writer locking or
power-loss durability guarantees.

## Game compatibility

The new constructor `Game(cards, config, deck1, deck2, seed)` owns copies of both
DeckDefinitions and the CardDatabase. `start()` validates both before changing
state or allocating card instances, expands counts, and uses the existing start,
shuffle and dealing path. It returns UnknownCard for an unknown definition and
InvalidDeck for other deck validation failures. Reset keeps the deck snapshots;
restarting produces fresh InstanceIds. Files and DeckManager can be changed or
destroyed after construction without affecting the match.

The old constructor, `Game::fromFiles(config, cards, seed)` and
`start(vector<string>, vector<string>)` remain supported. The old constructor and
fromFiles do not set deck snapshots, so their no-argument start returns InvalidDeck.
Calling the legacy start with explicit vectors does not replace snapshots held
by a Game made with the new constructor.

## CLI and tests

```sh
./build/cli/soulbound_cli --list-decks
./build/cli/soulbound_cli data starter-knight starter-guard
./build/cli/soulbound_cli /absolute/path/to/data my-deck another-deck
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'decks\.|cli.deck_selection' --output-on-failure
```

Without IDs the CLI selects `starter-knight` and `starter-guard`. Both supplied
sample JSON decks have 15 Knights and 15 Guards. Listing includes drafts and
readiness; trying to play a draft fails. The same deck may be selected for both
players, producing separate card instances. CLI only loads/selects through Core
APIs; no CRUD commands or deck-building rules are implemented in the CLI.

New tests cover catalog loading/listing, draft and playable validation, count
boundaries, no copy limit, custom deck sizes, CRUD/reopen persistence, rejected
updates preserving prior data, corrupt/schema-invalid JSON, filesystem errors,
path/filename checks, independent Game snapshots, deterministic dealing, legacy
compatibility and CLI deck selection/listing/failure handling. Existing full-match
CLI smoke tests now load their one-card decks from fixture JSON.
