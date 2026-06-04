# MiniSearch

A C++17 local file indexing and search tool.

## Build

Install Protobuf first. Install wxWidgets too when building the GUI:

```bash
sudo apt update
sudo apt install -y protobuf-compiler libprotobuf-dev libwxgtk3.2-dev
```

The GUI is built by default. To build only the CLI, configure with
`-DMINISEARCH_BUILD_GUI=OFF`.

Then build:

```bash
cmake --preset debug
cmake --build --preset debug
```

The debug binary is written to:

```bash
build/debug/minisearch
build/debug/minisearch-gui
```

For an optimized build:

```bash
cmake --preset release
cmake --build --preset release
```

Install the selected build to `~/.local/bin`:

```bash
cmake --install build/debug
```

## Usage

```bash
./build/debug/minisearch ./src
```

After indexing, MiniSearch enters an interactive shell:

```text
(minisearch) find main
(minisearch) grep "class parser"
(minisearch) stats
(minisearch) quit
```

By default, index files are stored under `~/.minisearch`:

```text
~/.minisearch/
  current.pb
  indexes/
    <canonical-path-hash>.pb
```

MiniSearch first converts the indexed path to a normalized absolute path, then
hashes that path to choose the real Protobuf index file. For example, indexing
`./src` stores an index for the canonical `src` path under
`~/.minisearch/indexes/<hash>.pb`.

After a successful indexing run, MiniSearch writes
`~/.minisearch/current.pb`. That Protobuf file points to the most recently
built index, so running MiniSearch without arguments reopens that index in the
interactive shell:

```bash
./build/debug/minisearch ./src
./build/debug/minisearch
```

Use shell commands for searching and stats:

```text
(minisearch) grep ThreadPool
(minisearch) show
(minisearch) stats
(minisearch) path
```

Run the native wxWidgets GUI:

```bash
./build/debug/minisearch-gui
```

The GUI can load the current index, build or refresh an index for a selected
file or directory, search file names, grep indexed text content, and open result
files with the desktop default application.

Visualize stored Protobuf index files as a local HTML report:

```bash
python3 scripts/visualize_index_pb.py
```

By default, the script reads `~/.minisearch/indexes/*.pb`, uses
`proto/index.proto` for decoding, and writes
`build/index-pb-visualization.html`. The report includes aggregate stats plus
expandable field views for the persisted `Index`, `FileRecord`, `PostingList`,
and `Posting` objects. Pass a file or directory to inspect a different
location:

```bash
python3 scripts/visualize_index_pb.py ~/.minisearch/indexes/<hash>.pb -o /tmp/index.html
```

Run `minisearch <path>` again to refresh the index or switch to a different
indexed path. When an existing index is available, unchanged text files reuse
their persisted postings and only new or modified text files are parsed again.

## Modules

- `cli`: command-line parsing
- `gui`: wxWidgets desktop application
- `index`: file scanning, text tokenization, inverted index, index storage
- `search`: search API used by CLI commands
- `util`: logger and thread pool

## Possible next steps

- Fuzzy matching and ranking
- Highlighted search output
- Config file support
- Compact binary index format
