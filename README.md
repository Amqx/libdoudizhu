# Libdoudizhu

Classic Chinese doudizhu game written fully in C as a statically linked library. Includes a small CLI game to play with.

Licensed via GPL v3

## Features:

- All card types in the actual game
    - Singles, doubles, pairs, triples, triples + N/ 2N, consecutive pairs/ triples, triple straights + N/2N, 4 + 2,
      bomb, joker bomb
- Fully fledged bidding logic for players/ AI to bid for landlord
- Working AI bot that can be added to play against real human players
- Weighted self-play framework to pit bot variants against each other and hill-climb bot weights

## Building

This library should build on most if not all compilers. We have only tested standard gcc (15.2.1 20260103),
riscv64-elf-gcc (Arch Linux Repositories - 15.2.0), riscv32-unknown-elf-gcc included with Quartus Lite (), and MSVC (
19.44.35222 for x64).

You will need CMake, Ninja, and a C compiler.

```bash
# From project root
mkdir build && cd build

# For the library use this
cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" --target doudizhu -S ..

# To build the CLI game use this
cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" --target game -S ..

# For the test suite use this
cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" --target all -S ..

cmake --build

# Depending on what you built, the following should be in your build directory

## CLI game
./game

## Self-play tuner
./selfplay [generations] [games_per_generation] [initial_step] [seed]

## Library
./libdoudizhu.a

## Run tests
ctest

```
