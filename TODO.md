# MiniSearch TODO

## P0 - Core User Value

- [ ] Add search result ranking.
  - Score file-name matches, path matches, term frequency, same-line matches, and recent modification time.
- [ ] Add file type and path filters.
  - Support filters such as `*.cpp`, `src/`, excluded directories, and extension-only searches.
- [ ] Improve GUI search interactions.
  - Add search history, clear search, context menus, open containing folder, copy path, and stronger match highlighting.
- [ ] Add indexing progress and cancellation.
  - Show scanned files, parsed files, reused files, elapsed time, and a cancel action during long indexing runs.
- [ ] Add config file support.
  - Store defaults such as thread count, excluded paths, max text-file size, startup behavior, and logging options.

## P1 - Search And Index Management

- [x] Add multiple index management.
  - Track recently indexed projects instead of only the current index.
- [ ] Add stale-index detection and refresh prompts.
  - Detect changed files and prompt users to refresh before searching old data.
- [ ] Add fuzzy file-name search.
  - Handle near matches such as `thredpool` matching `ThreadPool`.
- [ ] Add query syntax.
  - Support exact phrases, negative terms, path filters, extension filters, and structured expressions such as `path:src ext:cpp`.
- [ ] Improve search result preview.
  - Highlight matched terms, make context line count configurable, and add next/previous match navigation.

## P2 - Product Experience

- [ ] Add installable packages.
  - Provide a `.deb`, AppImage, or install script so users do not need to run CMake manually.
- [ ] Improve first-run GUI experience.
  - Show a useful default home screen that guides users to select a file or directory and build their first index.
- [ ] Add recent searches and saved searches.
  - Let users quickly repeat common searches.
- [ ] Add result export.
  - Export search results as text, CSV, or JSON.
- [ ] Add a GUI settings page.
  - Configure thread count, excluded paths, max file size, log file location, and default search mode.

## P3 - Engineering And Production Readiness

- [ ] Add a `LogSink` abstraction.
  - Prepare for console, file, JSON, Kafka, and future logging backends without bloating `Logger`.
- [ ] Add JSON Lines logging.
  - Make logs easier for external collectors to parse.
- [ ] Add benchmarks.
  - Measure indexing speed, incremental reuse, search latency, and memory usage.
- [ ] Expand schema migration helpers.
  - Move toward a clearer migration registry as more persistent fields are added.
- [ ] Add package/build CI.
  - Build, test, and optionally package CLI and GUI artifacts in GitHub Actions.
