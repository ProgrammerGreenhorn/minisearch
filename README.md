# MiniSearch

A C++17 local file indexing and search tool.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Usage

```bash
./build/minisearch index ./src
./build/minisearch find main
./build/minisearch grep "class parser"
./build/minisearch stats
```

The default index file is `.minisearch.index` in the current directory.

## Modules

- `cli`: command-line parsing
- `index`: file scanning, text tokenization, inverted index, index storage
- `search`: search API used by CLI commands
- `util`: logger and thread pool

## Possible next steps

- Incremental index updates
- Fuzzy matching and ranking
- Highlighted search output
- Config file support
- Compact binary index format

