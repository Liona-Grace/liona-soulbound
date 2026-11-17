execute_process(COMMAND "${CLI}" "${FIXTURES}"
    INPUT_FILE "${FIXTURES}/invalid_input.txt"
    OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result
    TIMEOUT 10)
if(NOT result EQUAL 0 OR NOT errors STREQUAL "")
    message(FATAL_ERROR "CLI failed: ${result}\n${errors}\n${output}")
endif()
foreach(expected "Unknown command. Type help" "Usage: summon <instanceId> <zoneIndex>"
        "Usage: quit" "Usage: draw" "Invalid number:"
        "Error: Deck is empty." "Error: Card instance was not found."
        "Error: Invalid board zone." "Error: Attacking is unavailable on the first turn."
        "Error: Action is unavailable in this phase." "Error: Invalid attack target."
        "Error: Unit has already attacked this turn."
        "Turn 4 | Player 2 | Main Phase" "Player 1: HP 12" "Player 2: HP 10"
        "Knight #1 (ATK 5 | HP 5/8)" "Goodbye!")
    string(FIND "${output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing '${expected}'\n${output}")
    endif()
endforeach()
# Exactly ten valid actions; malformed or rejected commands cannot sneak through.
string(REGEX MATCHALL "Action succeeded[.]" successes "${output}")
list(LENGTH successes count)
if(NOT count EQUAL 10)
    message(FATAL_ERROR "Expected 10 successful actions, got ${count}\n${output}")
endif()
