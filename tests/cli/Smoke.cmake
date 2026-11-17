# Run the real executable from the build directory with explicit fixture data.
execute_process(COMMAND "${CLI}" "${FIXTURES}"
    INPUT_FILE "${FIXTURES}/full_match.txt"
    OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result TIMEOUT 10)
if(NOT result EQUAL 0 OR NOT errors STREQUAL "")
    message(FATAL_ERROR "CLI failed: ${result}\n${errors}\n${output}")
endif()
string(REGEX MATCHALL "Error:" failures "${output}")
list(LENGTH failures failure_count)
string(REGEX MATCHALL "Game over: Player 1 wins!" results "${output}")
list(LENGTH results result_count)
if(NOT failure_count EQUAL 6 OR NOT result_count EQUAL 1)
    message(FATAL_ERROR "Expected six post-game errors and one victory announcement\n${output}")
endif()
foreach(expected "Game started!" "Knight #1 (ATK 5 | HP 8/8)"
        "Player 2: HP 0" "Turn 9 | Player 1 | Battle Phase"
        "Game over: Player 1 wins!" "Draw available: No"
        "Error: Game is not in progress." "Goodbye!")
    string(FIND "${output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing '${expected}'\n${output}")
    endif()
endforeach()
string(FIND "${output}" "Unknown command" unexpected)
if(NOT unexpected EQUAL -1)
    message(FATAL_ERROR "Unknown command in match script\n${output}")
endif()
