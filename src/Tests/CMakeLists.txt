add_executable(coretests CoreTests.cpp)
target_link_libraries(coretests PRIVATE Engine)
add_test(NAME coretests COMMAND coretests)
target_compile_definitions(coretests PRIVATE DELTASQUAD_GAME_DIR="${PROJECT_SOURCE_DIR}/game")
