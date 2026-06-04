# MiniSearch

MiniSearch is a C++17 local file indexing and search tool. It builds a
Protobuf-backed inverted index for files on disk, then exposes that index
through both a CLI interactive shell and a native wxWidgets desktop GUI.

## Features

- Index a file or directory into a persistent local Protobuf index.
- Reopen the most recently built index without passing a path again.
- Reuse unchanged text-file postings during refreshes.
- Search indexed file names and paths.
- Search indexed text content by tokenized terms and line numbers.
- Browse results through either the interactive shell or the GUI.
- Visualize stored `.pb` index files as a local HTML report.

## Requirements

Install Protobuf before building. Install wxWidgets too when building the GUI:

```bash
sudo apt update
sudo apt install -y protobuf-compiler libprotobuf-dev libwxgtk3.2-dev
```

The GUI is enabled by default. Configure with
`-DMINISEARCH_BUILD_GUI=OFF` if only the CLI target is needed.

Tests are enabled by default and use GoogleTest through CMake
`FetchContent`, so the first test-enabled configure may need network access.

## Build

Use the helper script for the normal workflows:

```bash
./build.sh debug
./build.sh release
./build.sh all
```

`./build.sh` defaults to `debug` when no mode is passed. It configures the
matching CMake preset and then builds it.

The equivalent direct CMake commands are:

```bash
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
```

Build outputs:

```text
build/debug/minisearch
build/debug/minisearch-gui
build/release/minisearch
build/release/minisearch-gui
```

Install a configured build to `~/.local/bin`:

```bash
cmake --install build/debug
```

## CLI Usage

Index a path and open the interactive shell:

```bash
./build/debug/minisearch ./src
```

Open the most recently built index:

```bash
./build/debug/minisearch
```

Interactive shell commands:

```text
(minisearch) find main
(minisearch) grep ThreadPool
(minisearch) show
(minisearch) stats
(minisearch) path
(minisearch) help
(minisearch) quit
```

Command summary:

| Command | Description |
| --- | --- |
| `find <query>` | Search indexed file names and paths. |
| `grep <query>` | Search indexed text content and print matching lines. |
| `show` | Show all indexed files. |
| `stats` | Show file, text-file, and term counts. |
| `path` | Show the indexed root and index file path. |
| `help` | Show shell help. |
| `quit` | Exit the shell. |

## GUI Usage

Run the native wxWidgets GUI:

```bash
./build/debug/minisearch-gui
```

The GUI can:

- Load the current index on startup.
- Choose a file or directory and build an index for it.
- Reindex the currently loaded root.
- Search file names or indexed text content.
- Show matching line context in the result table and preview pane.
- Open result files with the desktop default application.

## Index Storage

Index metadata is stored under `~/.minisearch`:

```text
~/.minisearch/
  current.pb
  indexes/
    <canonical-path-hash>.pb
```

MiniSearch normalizes the indexed path to an absolute canonical path, hashes
that path, and stores the real index at
`~/.minisearch/indexes/<canonical-path-hash>.pb`.

After a successful indexing run, `~/.minisearch/current.pb` points to the most
recent index. That is why running `minisearch` without arguments reopens the
last indexed root.

When refreshing an existing index, unchanged text files reuse their stored
postings. New or modified text files are parsed again, while removed files are
left out of the rebuilt index.

## Schema Versioning

Persisted index data carries an explicit schema version. MiniSearch currently
writes index schema version `2` and can load index versions `1` through `2`.
Version `2` adds `root_path` metadata; version `1` indexes are migrated in
memory when loaded.

The `current.pb` pointer has its own schema version. MiniSearch currently
writes and supports current-index pointer version `1`. Unsupported future
versions and missing version fields are rejected with explicit errors instead
of being parsed as current data.

## Index Visualization

Visualize stored Protobuf index files as a local HTML report:

```bash
python3 scripts/visualize_index_pb.py
```

By default, the script reads `~/.minisearch/indexes/*.pb`, decodes data with
`proto/index.proto`, and writes:

```text
build/index-pb-visualization.html
```

Inspect a specific index file or directory:

```bash
python3 scripts/visualize_index_pb.py ~/.minisearch/indexes/<hash>.pb -o /tmp/index.html
```

## Logging

MiniSearch queues log messages in memory and writes them from a background
worker thread in batches. It logs to the console by default. Set
`MINISEARCH_LOG_FILE` to append the same log events to a plain-text file
without terminal color escape codes:

```bash
MINISEARCH_LOG_FILE=/tmp/minisearch.log ./build/debug/minisearch ./src
MINISEARCH_LOG_FILE=/tmp/minisearch-gui.log ./build/debug/minisearch-gui
```

The logger is process-wide and can also be configured in code through
`minisearch::util::Logger::instance().setLogFile(...)`. Call
`minisearch::util::Logger::instance().flush()` when tests or tools need to wait
for queued log messages to reach disk.

## Tests

Build and run the GoogleTest suite:

```bash
cmake --build --preset debug --target check-tests
```

Build test binaries without running them:

```bash
cmake --build --preset debug --target build-tests
```

## Project Layout

```text
include/minisearch/   Public headers
src/                  Implementations and entry points
proto/index.proto     Persistent index schema
scripts/              Development and inspection scripts
test/                 GoogleTest unit tests
build/                Generated build output
```

Main modules:

- `app`: application startup flow.
- `cli`: command-line parsing.
- `gui`: wxWidgets desktop application.
- `index`: file scanning, text tokenization, inverted index, and storage.
- `search`: query API used by CLI and GUI.
- `shell`: interactive shell commands.
- `util`: logger, hashing, and thread pool helpers.

## Roadmap

- Ranking and fuzzy matching.
- File-type and path filters.
- Config file support.
- Additional schema migrations as new persistent fields are added.
