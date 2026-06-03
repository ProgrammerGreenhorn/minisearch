# Repository Guidelines

## Project Structure & Module Organization

MiniSearch is a C++17 local file indexing and search tool built with CMake and Protobuf. Public headers live under `include/minisearch/`, grouped by module: `cli`, `index`, `search`, `shell`, and `util`. Implementations mirror that structure under `src/`. The executable entry point is `src/main.cpp`. Protobuf schema lives in `proto/index.proto`; generated `.pb.cc` and `.pb.h` files are created inside `build/<preset>/generated/` and should not be edited directly. Build output belongs under `build/`.

## Build, Test, and Development Commands

- `cmake --preset debug`: configure a debug build in `build/debug`.
- `cmake --build --preset debug`: compile the debug binary at `build/debug/minisearch`.
- `cmake --preset release && cmake --build --preset release`: create an optimized build in `build/release`.
- `./build/debug/minisearch ./src`: index the source tree and open the interactive shell.
- `cmake --install build/debug`: install the selected build to `~/.local/bin`.

Install Protobuf before building: `sudo apt install -y protobuf-compiler libprotobuf-dev`.

## Coding Style & Naming Conventions

Use C++17 and follow the surrounding style: two-space indentation, `#pragma once` headers, standard-library includes before project includes, and trailing return types such as `auto parseFile(...) -> std::vector<std::string>`. Keep code inside the relevant `minisearch::<module>` namespace. Use PascalCase for classes (`InvertedIndex`), lowerCamelCase for functions and variables (`recordKey`), and trailing underscores for private data members (`records_`). Keep compiler warnings clean; CMake enables `-Wall -Wextra -Wpedantic` for GNU and Clang.

## Testing Guidelines

There is no dedicated automated test suite yet. At minimum, validate changes with `cmake --build --preset debug` and a smoke run such as `./build/debug/minisearch ./src`, then exercise shell commands like `find main`, `grep ThreadPool`, `show`, and `stats`. When adding testable behavior, prefer adding a `tests/` directory wired through CTest, with files named after the unit under test, for example `TextParserTest.cpp`.

## Commit & Pull Request Guidelines

Recent commits use short imperative subjects such as `Add show command` and `Update startup command and logging`. Keep subjects focused, capitalized, and without trailing punctuation. Pull requests should include a concise summary, the commands used to build or smoke test, linked issues when applicable, and sample CLI output for user-visible shell behavior changes.

## Security & Configuration Tips

Index metadata is stored under `~/.minisearch/`; do not commit local indexes, generated build trees, or machine-specific paths. Treat `proto/index.proto` as the persistence contract and update storage compatibility deliberately.
