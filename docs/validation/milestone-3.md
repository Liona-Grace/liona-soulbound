# Milestone 3 — Vanilla gameplay validation

Validated on 2026-11-17 with GNU C++ 13.3.0, C++20 and CMake/CTest.
Core and CLI implementation were inspected before changes. No production Core or
CLI code or gameplay mechanics were changed for this milestone.

## Results

- Baseline: build succeeded; 21/21 existing tests passed, with no failures.
- Expanded suite: 27/27 tests passed (24 Core, 3 CLI).
- Debug build with UndefinedBehaviorSanitizer: 27/27 passed with no sanitizer reports.
- 32 mixed Knight/Guard matches, seeds 0–31, each replayed twice: all 64 runs
  finished in 4–7 turns. Player 1 won 14 distinct-seed matches, Player 2 won 18.
- Each pass attempted 1,943 simulated actions, including deliberate rejections;
  3,886 across both passes. Full action/state traces matched between replays.
- The executable full-match fixture completes on turn 9, announces exactly one
  victory, rejects all six gameplay command forms after game over and exits cleanly.
- A separate executable smoke script mixes malformed numbers, missing/extra
  arguments and legal combat, and checks final HP, turn and successful-action count.
- One new smoke-test assertion initially counted nine valid scripted actions; the
  script contains ten. The test expectation was corrected. No implementation
  failure was exposed by these checks.

Per-seed results are in [milestone-3-simulations.txt](milestone-3-simulations.txt).
These counts describe validation, not balance or optimal play. The scripted test
policy draws, attempts summons in numbered zones, and attacks the first opposing
unit (or player when the board is empty); it is confined to tests.

## Confirmed rules and coverage

| Area | Evidence |
| --- | --- |
| State integrity | Stronger invariant checks validate active player/turn parity, phase, non-negative Player HP, board size, ownership, unique nonzero InstanceIds and per-player card counts across Deck, Hand, Board and Tomb. Deck/Hand retain printed HP and unused attack flags; living board units have positive HP no greater than printed HP. |
| Exact conservation | New simulations compare every InstanceId's original definition and owner after every attempted action, not just total counts. Restart/reset tests check IDs are not reused. |
| Rejections and lifecycle | Complete state snapshots stay unchanged on rejected actions. All 11 action/engine-operation methods return NotPlaying before start and after either player's victory. Valid restart/reset remain available. |
| Phases and drawing | Main to Battle, repeated Battle rejection, Main/Battle to next player's Main, wrong-phase draw/summon rejection, exactly one manually drawn card, repeated draw rejection, draw-flag reset and no automatic turn-start draw. |
| Summoning | Printed Unit HP is paid; payment down to zero remains legal while the summoned board unit protects its owner. Fresh units can attack that turn after turn 1; Player 0 cannot attack during the first match turn. |
| Combat order | Attacker damage resolves first; a killed defender does not counterattack. Surviving defenders counterattack each different attacker without consuming their own attack. A previously defending survivor can attack on its next turn. |
| Repeated targets | Two different guards can attack the same knight and both receive counterattacks while it survives. Each attacker is limited to one initiation per turn. |
| Overflow and Tomb | Overflow from attacker damage hits the defender's owner; counterattack overflow hits the attacker's owner. Player HP clamps to zero. Negative Unit HP persists in Tomb, including -4 from combat and 8−INT64_MAX from engine damage. Exact lethal damage produces no overflow. |
| Victory | Zero HP alone does not lose while a unit remains. Losing the last unit at zero HP ends the match. Both victory statuses and post-game immutability are checked; direct lethal attacks and combat lethal overflow are covered. |
| Healing and numeric limits | Unit healing caps at printed HP even for INT64_MAX; Player healing exceeds starting HP and can reach INT64_MAX exactly. Overflowing healing and negative amounts, including INT64_MIN, are rejected without mutation. Dead/Tomb units cannot be healed. |
| CLI | Existing compact state output, metadata display, zero-based zones, parser boundaries, public API dispatch, EOF/quit, full-match completion and invalid input are covered. |

The complete simulations check invariants, exact identities and rejected-action
snapshots after every attempted action, require a terminal result within a
256-turn test bound and compare complete replay traces. The bound is a test
failure condition, not a new game rule.

## Unresolved decisions and limits (behavior preserved)

- **Empty-deck draw:** currently returns EmptyDeck without damage, loss or consuming
  the draw flag. Core contains a TODO about the eventual consequence. No fatigue
  or deck-out rule was introduced.
- **Indefinite matches:** players can keep ending turns, including from Main.
  Empty decks do not force a result. The tested matches terminate, but arbitrary
  play is not guaranteed to terminate; no stalemate/turn-limit rule was added.
- **Draw outcome:** evaluateOutcome supports both players simultaneously losing,
  tested with a standalone GameState. No ordinary vanilla public-action sequence
  producing that state was found: attacker-first combat changes one casualty side
  at a time and checks victory immediately. No forced in-match draw was invented.
- **Attack-flag reset:** endTurn currently clears both players' board attack flags.
  This is consistent with current vanilla play; timing may matter for future
  mechanics. No timing change was made.
- **Engine operations:** moveToTomb can move a living unit, so Tomb does not imply
  non-positive HP. Generic damageUnit does not spill damage to its owner; only
  combat does. These operations remain outside CLI commands.
- **Reproducibility:** identical seeds replay identically on the tested toolchain.
  std::shuffle mappings are not portable guarantees across standard libraries.
- **Numeric scope:** boundary validation covers states reachable through Game.
  CombatSystem::resolve accepts mutable CardInstances directly and assumes valid
  living units/definitions; arbitrary fabricated extreme HP inputs are outside
  this validation. The unsigned turn counter has no explicit exhaustion guard;
  reaching its limit through normal gameplay was not exercised.

## Reproduction

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/tests/core/soulbound_core_tests complete_simulations
./build/cli/soulbound_cli tests/cli/fixtures < tests/cli/fixtures/full_match.txt

cmake -S . -B /tmp/soulbound-m3-ubsan -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fno-sanitize-recover=all' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined
cmake --build /tmp/soulbound-m3-ubsan -j2
ctest --test-dir /tmp/soulbound-m3-ubsan --output-on-failure
```

## Modified files

- `tests/core/CoreTests.cpp`, `tests/core/CMakeLists.txt`: stronger invariants and
  five focused validation cases, including complete deterministic simulations.
- `tests/cli/CliTests.cpp`: quit stops command processing; EOF exits cleanly.
- `tests/cli/Smoke.cmake`, `tests/cli/fixtures/full_match.txt`: stricter completion
  checks and all gameplay commands rejected after victory.
- `tests/cli/InvalidInput.cmake`, `tests/cli/fixtures/invalid_input.txt`,
  `tests/cli/CMakeLists.txt`: executable invalid-input/combat smoke test.
- `docs/validation/milestone-3.md`, `docs/validation/milestone-3-simulations.txt`:
  report and reproducible results; root README links to the report.
