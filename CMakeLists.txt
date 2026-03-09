cmake_minimum_required(VERSION 3.3...4.2)
project(libdoudizhu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(project_warnings INTERFACE)
if (MSVC)
    target_compile_options(project_warnings INTERFACE /W4 /permissive-)
else ()
    target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif ()

file(GLOB SOURCES CONFIGURE_DEPENDS libdoudizhu/src/*.c)
add_library(doudizhu STATIC ${SOURCES})
target_include_directories(doudizhu PUBLIC libdoudizhu/include)
target_link_libraries(doudizhu PRIVATE project_warnings)

add_executable(game main/main.c)
target_link_libraries(game PRIVATE doudizhu)
target_link_libraries(game PRIVATE project_warnings)

add_executable(selfplay main/selfplay.c)
target_link_libraries(selfplay PRIVATE doudizhu)
target_link_libraries(selfplay PRIVATE project_warnings)

enable_testing()
add_executable(test_moves tests/test_moves.c)
target_link_libraries(test_moves PRIVATE doudizhu)
add_test(NAME test_moves COMMAND test_moves)

add_executable(test_bot tests/test_bot.c)
target_link_libraries(test_bot PRIVATE doudizhu)
add_test(NAME test_bot COMMAND test_bot)

add_executable(test_game tests/test_game.c)
target_link_libraries(test_game PRIVATE doudizhu)
add_test(NAME test_game COMMAND test_game)

add_executable(test_eval tests/test_eval.c)
target_link_libraries(test_eval PRIVATE doudizhu)
add_test(NAME test_eval COMMAND test_eval)

add_executable(test_selfplay tests/test_selfplay.c)
target_link_libraries(test_selfplay PRIVATE doudizhu)
add_test(NAME test_selfplay COMMAND test_selfplay)
