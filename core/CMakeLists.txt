add_library(soulbound_core STATIC
    src/Game.cpp
    src/Board.cpp
    src/CardDatabase.cpp
    src/JsonLoader.cpp
    src/CombatSystem.cpp
    src/DeckValidator.cpp
    src/DeckStorage.cpp
    src/DeckManager.cpp
)
add_library(soulbound::core ALIAS soulbound_core)
target_include_directories(soulbound_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(soulbound_core SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party)
target_compile_features(soulbound_core PUBLIC cxx_std_20)
set_target_properties(soulbound_core PROPERTIES CXX_EXTENSIONS OFF)
