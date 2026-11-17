add_executable(soulbound_cli_tests CliTests.cpp)
target_link_libraries(soulbound_cli_tests PRIVATE soulbound_cli_support)
set_target_properties(soulbound_cli_tests PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME cli.parsing_dispatch COMMAND soulbound_cli_tests)
add_test(NAME cli.full_match COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:soulbound_cli>
    -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    -P ${CMAKE_CURRENT_SOURCE_DIR}/Smoke.cmake)
add_test(NAME cli.invalid_input COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:soulbound_cli>
    -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    -P ${CMAKE_CURRENT_SOURCE_DIR}/InvalidInput.cmake)
add_test(NAME cli.deck_selection COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:soulbound_cli>
    -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    -P ${CMAKE_CURRENT_SOURCE_DIR}/DeckSelection.cmake)
