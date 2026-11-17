add_library(soulbound_cli_support STATIC src/CliApp.cpp src/CommandParser.cpp)
target_link_libraries(soulbound_cli_support PUBLIC soulbound::core)
target_include_directories(soulbound_cli_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(soulbound_cli_support PROPERTIES CXX_EXTENSIONS OFF)
add_executable(soulbound_cli src/main.cpp)
target_link_libraries(soulbound_cli PRIVATE soulbound_cli_support)
target_compile_features(soulbound_cli PRIVATE cxx_std_20)
target_compile_definitions(soulbound_cli PRIVATE
    SOULBOUND_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
)
set_target_properties(soulbound_cli PROPERTIES CXX_EXTENSIONS OFF)
