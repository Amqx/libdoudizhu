cmake_minimum_required(VERSION 3.3...4.2)
project(libdoudizhu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DOUDIZHU_TUNING "Build weight tuning and self-play simulation tools" OFF)

add_library(project_warnings INTERFACE)
if (MSVC)
    target_compile_options(project_warnings INTERFACE /W4 /permissive-)
else ()
    target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif ()

set(DOUDIZHU_SOURCES
        libdoudizhu/src/bot.c
        libdoudizhu/src/eval.c
        libdoudizhu/src/game.c
        libdoudizhu/src/moves.c
        libdoudizhu/src/utils.c)

# Prevent the compiler from replacing the byte loops in utils.c with calls to
# memset/memcpy, which would cause infinite recursion in release builds.
if (NOT MSVC)
    set_source_files_properties(libdoudizhu/src/utils.c PROPERTIES
            COMPILE_FLAGS "-fno-tree-loop-distribute-patterns")
endif ()

if (DOUDIZHU_TUNING)
    list(APPEND DOUDIZHU_SOURCES
            libdoudizhu/src/tune.c
            libdoudizhu/src/selfplay.c)
endif ()

add_library(doudizhu STATIC ${DOUDIZHU_SOURCES})
target_include_directories(doudizhu PUBLIC libdoudizhu/include)
target_link_libraries(doudizhu PRIVATE project_warnings)

if (DOUDIZHU_TUNING)
    target_compile_definitions(doudizhu PUBLIC DOUDIZHU_TUNING)
endif ()

add_executable(game main/main.c
        main/display.c
)
target_link_libraries(game PRIVATE doudizhu)
target_link_libraries(game PRIVATE project_warnings)

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

if (DOUDIZHU_TUNING)
    add_executable(selfplay main/selfplay.c)
    target_link_libraries(selfplay PRIVATE doudizhu)
    target_link_libraries(selfplay PRIVATE project_warnings)

    add_executable(test_selfplay tests/test_selfplay.c)
    target_link_libraries(test_selfplay PRIVATE doudizhu)
    add_test(NAME test_selfplay COMMAND test_selfplay)
endif ()
