add_executable(soulbound_core_tests CoreTests.cpp)
target_link_libraries(soulbound_core_tests PRIVATE soulbound::core)
target_compile_definitions(soulbound_core_tests PRIVATE
    CONFIG_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/game_default.json"
    CONFIG_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/game_config.json"
    CARD_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cards.json"
)
set_target_properties(soulbound_core_tests PROPERTIES CXX_EXTENSIONS OFF)

foreach(case IN ITEMS cards json start shuffle config config_json load_order draw summon zero_hp turns
                      survival spillover counterattack targets direct_attack hp
                      game_over simulations manual_phases combat_lifetime lifecycle_rejections
                      healing_boundaries complete_simulations)
    add_test(NAME core.${case} COMMAND soulbound_core_tests ${case})
endforeach()

add_executable(soulbound_deck_tests DeckTests.cpp)
target_link_libraries(soulbound_deck_tests PRIVATE soulbound::core)
target_compile_definitions(soulbound_deck_tests PRIVATE CARD_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cards.json")
set_target_properties(soulbound_deck_tests PROPERTIES CXX_EXTENSIONS OFF)
foreach(case IN ITEMS catalog validation crud corrupt_storage game_decks copy_limits)
    add_test(NAME decks.${case} COMMAND soulbound_deck_tests ${case})
endforeach()
