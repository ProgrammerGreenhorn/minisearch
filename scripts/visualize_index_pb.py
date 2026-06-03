#!/usr/bin/env python3
"""Generate a local HTML visualization for MiniSearch index protobuf files."""

from __future__ import annotations

import argparse
import ast
import collections
import datetime as dt
import html
import pathlib
import shutil
import subprocess
import sys
from dataclasses import dataclass


INDEX_MESSAGE = "minisearch.index.proto.Index"
CURRENT_MESSAGE = "minisearch.index.proto.CurrentIndex"


@dataclass(frozen=True)
class FileRecord:
  id: int
  path: str
  size: int
  modified_time: int
  text_indexed: bool
  content_hash: int


@dataclass(frozen=True)
class Posting:
  document_id: int
  lines: tuple[int, ...]


@dataclass(frozen=True)
class PostingList:
  term: str
  postings: tuple[Posting, ...]


@dataclass(frozen=True)
class IndexSnapshot:
  path: pathlib.Path
  version: int
  root_path: str
  records: tuple[FileRecord, ...]
  postings: tuple[PostingList, ...]
  current: bool = False


@dataclass(frozen=True)
class CurrentPointer:
  version: int
  root_path: str
  index_file: str


def escape(value: object) -> str:
  return html.escape(str(value), quote=True)


def repo_root() -> pathlib.Path:
  return pathlib.Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description=(
          "Visualize MiniSearch protobuf indexes as a self-contained HTML file."
      )
  )
  parser.add_argument(
      "path",
      nargs="?",
      default=str(pathlib.Path.home() / ".minisearch" / "indexes"),
      help=(
          "Index .pb file or directory of .pb files "
          "(default: ~/.minisearch/indexes)."
      ),
  )
  parser.add_argument(
      "-o",
      "--output",
      default=str(repo_root() / "build" / "index-pb-visualization.html"),
      help="Output HTML path (default: build/index-pb-visualization.html).",
  )
  parser.add_argument(
      "--proto",
      default=str(repo_root() / "proto" / "index.proto"),
      help="Path to index.proto (default: proto/index.proto).",
  )
  parser.add_argument(
      "--protoc",
      default="protoc",
      help="protoc executable to use for decoding (default: protoc).",
  )
  parser.add_argument(
      "--top-terms",
      type=int,
      default=50,
      help="Number of high-frequency terms to show per index (default: 50).",
  )
  parser.add_argument(
      "--current",
      default=None,
      help=(
          "Path to current.pb. By default the script tries the sibling "
          "current.pb for an indexes directory, then ~/.minisearch/current.pb."
      ),
  )
  return parser.parse_args()


def find_index_files(path: pathlib.Path) -> list[pathlib.Path]:
  expanded = pathlib.Path(path).expanduser()
  if expanded.is_file():
    return [expanded]
  if not expanded.exists():
    raise FileNotFoundError(f"index path does not exist: {expanded}")
  if not expanded.is_dir():
    raise RuntimeError(f"index path is not a file or directory: {expanded}")

  return sorted(candidate for candidate in expanded.glob("*.pb")
                if candidate.is_file())


def run_protoc_decode(
    pb_path: pathlib.Path,
    message: str,
    proto_path: pathlib.Path,
    protoc: str,
) -> str:
  if shutil.which(protoc) is None:
    raise RuntimeError(
        f"{protoc!r} was not found. Install protobuf-compiler or pass --protoc."
    )

  proto_dir = proto_path.parent
  command = [
      protoc,
      f"--proto_path={proto_dir}",
      f"--decode={message}",
      str(proto_path),
  ]
  completed = subprocess.run(
      command,
      input=pb_path.read_bytes(),
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      check=False,
  )
  if completed.returncode != 0:
    stderr = completed.stderr.decode("utf-8", errors="replace").strip()
    raise RuntimeError(f"failed to decode {pb_path}: {stderr}")
  return completed.stdout.decode("utf-8", errors="replace")


def parse_scalar(value: str) -> object:
  value = value.strip()
  if value == "true":
    return True
  if value == "false":
    return False
  if value.startswith('"') and value.endswith('"'):
    try:
      return ast.literal_eval(value)
    except (SyntaxError, ValueError):
      return value[1:-1]
  try:
    return int(value)
  except ValueError:
    return value


def parse_index_text(text: str, path: pathlib.Path) -> IndexSnapshot:
  root_fields: dict[str, object] = {}
  records: list[FileRecord] = []
  posting_lists: list[PostingList] = []
  stack: list[tuple[str, dict[str, object]]] = []

  for raw_line in text.splitlines():
    line = raw_line.strip()
    if not line:
      continue

    if line.endswith("{"):
      field = line[:-1].strip()
      if field == "records":
        stack.append(("record", {}))
      elif field == "postings":
        if stack and stack[-1][0] == "posting_list":
          stack.append(("posting", {"lines": []}))
        else:
          stack.append(("posting_list", {"postings": []}))
      else:
        raise RuntimeError(f"unexpected block {field!r} in {path}")
      continue

    if line == "}":
      if not stack:
        raise RuntimeError(f"unexpected closing brace in {path}")
      kind, values = stack.pop()
      if kind == "record":
        records.append(
            FileRecord(
                id=int(values.get("id", 0)),
                path=str(values.get("path", "")),
                size=int(values.get("size", 0)),
                modified_time=int(values.get("modified_time", 0)),
                text_indexed=bool(values.get("text_indexed", False)),
                content_hash=int(values.get("content_hash", 0)),
            )
        )
      elif kind == "posting":
        if not stack or stack[-1][0] != "posting_list":
          raise RuntimeError(f"posting appeared outside a posting list in {path}")
        parent = stack[-1][1]
        parent_postings = parent.setdefault("postings", [])
        assert isinstance(parent_postings, list)
        parent_postings.append(
            Posting(
                document_id=int(values.get("document_id", 0)),
                lines=tuple(int(line_no) for line_no in values.get("lines", [])),
            )
        )
      elif kind == "posting_list":
        posting_lists.append(
            PostingList(
                term=str(values.get("term", "")),
                postings=tuple(values.get("postings", [])),
            )
        )
      continue

    if ":" not in line:
      raise RuntimeError(f"cannot parse line in {path}: {line}")
    key, raw_value = line.split(":", 1)
    value = parse_scalar(raw_value)

    target = stack[-1][1] if stack else root_fields
    if key == "lines":
      lines = target.setdefault("lines", [])
      assert isinstance(lines, list)
      lines.append(value)
    else:
      target[key] = value

  if stack:
    raise RuntimeError(f"unterminated protobuf block in {path}")

  return IndexSnapshot(
      path=path,
      version=int(root_fields.get("version", 0)),
      root_path=str(root_fields.get("root_path", "")),
      records=tuple(records),
      postings=tuple(posting_lists),
  )


def parse_current_text(text: str) -> CurrentPointer:
  values: dict[str, object] = {}
  for raw_line in text.splitlines():
    line = raw_line.strip()
    if not line or ":" not in line:
      continue
    key, raw_value = line.split(":", 1)
    values[key] = parse_scalar(raw_value)
  return CurrentPointer(
      version=int(values.get("version", 0)),
      root_path=str(values.get("root_path", "")),
      index_file=str(values.get("index_file", "")),
  )


def candidate_current_path(input_path: pathlib.Path) -> pathlib.Path | None:
  expanded = input_path.expanduser()
  candidates: list[pathlib.Path] = []
  if expanded.is_dir() and expanded.name == "indexes":
    candidates.append(expanded.parent / "current.pb")
  if expanded.is_file() and expanded.parent.name == "indexes":
    candidates.append(expanded.parent.parent / "current.pb")
  candidates.append(pathlib.Path.home() / ".minisearch" / "current.pb")

  for candidate in candidates:
    if candidate.exists():
      return candidate
  return None


def canonical_key(path: pathlib.Path) -> str:
  return str(path.expanduser().resolve(strict=False))


def mark_current(
    snapshots: list[IndexSnapshot],
    current: CurrentPointer | None,
) -> list[IndexSnapshot]:
  if current is None:
    return snapshots

  marked: list[IndexSnapshot] = []
  for snapshot in snapshots:
    aliases = {
        str(snapshot.path),
        canonical_key(snapshot.path),
    }
    marked.append(
        IndexSnapshot(
            path=snapshot.path,
            version=snapshot.version,
            root_path=snapshot.root_path,
            records=snapshot.records,
            postings=snapshot.postings,
            current=current.index_file in aliases,
        )
    )
  return marked


def format_bytes(size: int) -> str:
  value = float(size)
  for unit in ("B", "KB", "MB", "GB", "TB"):
    if value < 1024.0 or unit == "TB":
      if unit == "B":
        return f"{int(value)} {unit}"
      return f"{value:.1f} {unit}"
    value /= 1024.0
  return f"{size} B"


def format_time(timestamp: int) -> str:
  if timestamp <= 0:
    return "-"
  return dt.datetime.fromtimestamp(timestamp).strftime("%Y-%m-%d %H:%M:%S")


def relative_record_path(record_path: str, root_path: str) -> str:
  path = pathlib.Path(record_path)
  if root_path:
    root = pathlib.Path(root_path)
    try:
      relative = path.resolve(strict=False).relative_to(
          root.resolve(strict=False)
      )
      value = str(relative)
      if value == ".":
        return path.name
      return value
    except ValueError:
      pass
  return record_path.lstrip("/") or record_path


def term_stats(snapshot: IndexSnapshot) -> list[tuple[str, int, int]]:
  stats: list[tuple[str, int, int]] = []
  for posting_list in snapshot.postings:
    doc_count = len(posting_list.postings)
    line_count = sum(len(posting.lines) for posting in posting_list.postings)
    stats.append((posting_list.term, doc_count, line_count))
  return sorted(stats, key=lambda item: (-item[2], -item[1], item[0]))


def extension_stats(snapshot: IndexSnapshot) -> list[tuple[str, int, int]]:
  counts: collections.Counter[str] = collections.Counter()
  sizes: collections.Counter[str] = collections.Counter()
  for record in snapshot.records:
    suffix = pathlib.Path(record.path).suffix.lower() or "(none)"
    counts[suffix] += 1
    sizes[suffix] += record.size
  return sorted(
      ((suffix, count, sizes[suffix]) for suffix, count in counts.items()),
      key=lambda item: (-item[1], item[0]),
  )


def snapshot_totals(snapshot: IndexSnapshot) -> dict[str, int]:
  return {
      "files": len(snapshot.records),
      "text_files": sum(1 for record in snapshot.records if record.text_indexed),
      "terms": len(snapshot.postings),
      "posting_docs": sum(len(posting_list.postings)
                          for posting_list in snapshot.postings),
      "posting_lines": sum(len(posting.lines)
                           for posting_list in snapshot.postings
                           for posting in posting_list.postings),
      "bytes": sum(record.size for record in snapshot.records),
  }


def render_stat_cards(totals: dict[str, int]) -> str:
  cards = [
      ("Files", f"{totals['files']:,}"),
      ("Text indexed", f"{totals['text_files']:,}"),
      ("Terms", f"{totals['terms']:,}"),
      ("Posting docs", f"{totals['posting_docs']:,}"),
      ("Line refs", f"{totals['posting_lines']:,}"),
      ("Total size", format_bytes(totals["bytes"])),
  ]
  return "\n".join(
      f'<div class="stat"><span>{escape(label)}</span><strong>{escape(value)}</strong></div>'
      for label, value in cards
  )


def render_bars(
    rows: list[tuple[str, int, int]],
    value_index: int,
    limit: int,
    value_formatter,
) -> str:
  if not rows:
    return '<p class="empty">No data.</p>'

  visible = rows[:limit]
  max_value = max(row[value_index] for row in visible) or 1
  output: list[str] = []
  for row in visible:
    label = str(row[0])
    value = int(row[value_index])
    width = max(2, int((value / max_value) * 100))
    meta = value_formatter(row)
    output.append(
        '<div class="bar-row">'
        f'<span class="bar-label" title="{escape(label)}">{escape(label)}</span>'
        '<span class="bar-track">'
        f'<span class="bar-fill" style="width: {width}%"></span>'
        '</span>'
        f'<span class="bar-value">{escape(meta)}</span>'
        '</div>'
    )

  if len(rows) > limit:
    output.append(
        f'<p class="note">Showing top {limit:,} of {len(rows):,} rows.</p>'
    )
  return "\n".join(output)


def build_tree(snapshot: IndexSnapshot) -> dict[str, object]:
  root: dict[str, object] = {
      "dirs": collections.OrderedDict(),
      "files": [],
      "count": 0,
      "size": 0,
  }

  for record in sorted(
      snapshot.records,
      key=lambda item: relative_record_path(item.path, snapshot.root_path),
  ):
    display_path = relative_record_path(record.path, snapshot.root_path)
    parts = tuple(part for part in pathlib.PurePath(display_path).parts
                  if part not in ("", "."))
    if not parts:
      parts = (pathlib.Path(record.path).name or record.path,)

    node = root
    node["count"] = int(node["count"]) + 1
    node["size"] = int(node["size"]) + record.size
    for part in parts[:-1]:
      dirs = node["dirs"]
      assert isinstance(dirs, collections.OrderedDict)
      if part not in dirs:
        dirs[part] = {
            "dirs": collections.OrderedDict(),
            "files": [],
            "count": 0,
            "size": 0,
        }
      node = dirs[part]
      node["count"] = int(node["count"]) + 1
      node["size"] = int(node["size"]) + record.size

    files = node["files"]
    assert isinstance(files, list)
    files.append((parts[-1], display_path, record))

  return root


def render_tree_node(name: str, node: dict[str, object], depth: int = 0) -> str:
  dirs = node["dirs"]
  files = node["files"]
  assert isinstance(dirs, collections.OrderedDict)
  assert isinstance(files, list)

  child_html: list[str] = []
  for child_name, child_node in dirs.items():
    child_html.append(render_tree_node(child_name, child_node, depth + 1))

  for file_name, display_path, record in files:
    indexed = " indexed" if record.text_indexed else ""
    child_html.append(
        '<li class="tree-file">'
        f'<span class="file-name{indexed}" title="{escape(display_path)}">'
        f'{escape(file_name)}</span>'
        f'<span class="tree-meta">{escape(format_bytes(record.size))}</span>'
        '</li>'
    )

  open_attr = " open" if depth < 2 else ""
  return (
      f"<details{open_attr}>"
      "<summary>"
      f'<span class="dir-name">{escape(name)}</span>'
      f'<span class="tree-meta">{int(node["count"]):,} files, '
      f'{escape(format_bytes(int(node["size"])))}</span>'
      "</summary>"
      f'<ul class="tree-list">{"".join(child_html)}</ul>'
      "</details>"
  )


def render_file_table(snapshot: IndexSnapshot, table_id: str) -> str:
  rows: list[str] = []
  for record in sorted(
      snapshot.records,
      key=lambda item: relative_record_path(item.path, snapshot.root_path),
  ):
    rel_path = relative_record_path(record.path, snapshot.root_path)
    indexed = "yes" if record.text_indexed else "no"
    rows.append(
        "<tr>"
        f'<td class="mono">{record.id}</td>'
        f'<td class="path" title="{escape(record.path)}">{escape(rel_path)}</td>'
        f"<td>{escape(indexed)}</td>"
        f'<td class="numeric">{escape(format_bytes(record.size))}</td>'
        f'<td class="mono">{escape(format_time(record.modified_time))}</td>'
        f'<td class="mono hash">{record.content_hash:016x}</td>'
        "</tr>"
    )

  body = "\n".join(rows) if rows else (
      '<tr><td colspan="6" class="empty">No indexed files.</td></tr>'
  )
  return (
      f'<input class="filter" type="search" placeholder="Filter files" '
      f'oninput="filterTable(this, \'{table_id}\')">'
      f'<table id="{table_id}" class="files-table">'
      "<thead><tr>"
      "<th>ID</th><th>Path</th><th>Text</th><th>Size</th>"
      "<th>Modified</th><th>Content hash</th>"
      "</tr></thead>"
      f"<tbody>{body}</tbody></table>"
  )


def format_field_value(value: object) -> str:
  if isinstance(value, bool):
    return "true" if value else "false"
  if isinstance(value, tuple):
    return "[" + ", ".join(str(item) for item in value) + "]"
  return str(value)


def render_field_table(fields: list[tuple[str, str, object]]) -> str:
  rows = "\n".join(
      "<tr>"
      f'<th scope="row" class="mono">{escape(name)}</th>'
      f'<td class="mono field-type">{escape(field_type)}</td>'
      f'<td class="mono field-value">{escape(format_field_value(value))}</td>'
      "</tr>"
      for name, field_type, value in fields
  )
  return (
      '<table class="field-table">'
      "<thead><tr><th>Field</th><th>Type</th><th>Value</th></tr></thead>"
      f"<tbody>{rows}</tbody></table>"
  )


def object_filter_value(*values: object) -> str:
  return escape(" ".join(str(value) for value in values).lower())


def render_file_record_object(record: FileRecord, root_path: str) -> str:
  rel_path = relative_record_path(record.path, root_path)
  fields = [
      ("id", "uint32", record.id),
      ("path", "string", record.path),
      ("size", "uint64", record.size),
      ("modified_time", "int64", record.modified_time),
      ("text_indexed", "bool", record.text_indexed),
      ("content_hash", "uint64", record.content_hash),
  ]
  return (
      '<details class="object-card" '
      f'data-filter="{object_filter_value("filerecord", record.id, record.path, rel_path)}">'
      "<summary>"
      f'<span class="object-name">FileRecord #{record.id}</span>'
      f'<span class="object-meta">{escape(rel_path)} - '
      f'{escape(format_bytes(record.size))}</span>'
      "</summary>"
      f"{render_field_table(fields)}"
      "</details>"
  )


def render_posting_object(
    posting: Posting,
    position: int,
    document_by_id: dict[int, FileRecord],
    root_path: str,
) -> str:
  record = document_by_id.get(posting.document_id)
  document_label = (
      relative_record_path(record.path, root_path) if record is not None
      else "(missing record)"
  )
  fields = [
      ("document_id", "uint32", posting.document_id),
      ("lines", "repeated uint32", posting.lines),
  ]
  return (
      '<details class="nested-object" '
      f'data-filter="{object_filter_value("posting", posting.document_id, document_label)}">'
      "<summary>"
      f'<span class="object-name">Posting #{position}</span>'
      f'<span class="object-meta">document_id={posting.document_id} - '
      f'{escape(document_label)} - {len(posting.lines):,} lines</span>'
      "</summary>"
      f"{render_field_table(fields)}"
      "</details>"
  )


def render_posting_list_object(
    posting_list: PostingList,
    position: int,
    document_by_id: dict[int, FileRecord],
    root_path: str,
) -> str:
  line_count = sum(len(posting.lines) for posting in posting_list.postings)
  nested_postings = "\n".join(
      render_posting_object(posting, idx, document_by_id, root_path)
      for idx, posting in enumerate(posting_list.postings)
  )
  nested_postings = nested_postings or '<p class="empty">No Posting objects.</p>'
  fields = [
      ("term", "string", posting_list.term),
      ("postings", "repeated Posting", f"{len(posting_list.postings)} objects"),
  ]
  return (
      '<details class="object-card" '
      f'data-filter="{object_filter_value("postinglist", position, posting_list.term)}">'
      "<summary>"
      f'<span class="object-name">PostingList #{position}</span>'
      f'<span class="object-meta">term="{escape(posting_list.term)}" - '
      f'{len(posting_list.postings):,} postings - {line_count:,} lines</span>'
      "</summary>"
      f"{render_field_table(fields)}"
      '<div class="nested-objects">'
      f"{nested_postings}"
      "</div>"
      "</details>"
  )


def render_stored_objects(snapshot: IndexSnapshot, object_id: str) -> str:
  document_by_id = {record.id: record for record in snapshot.records}
  index_fields = [
      ("version", "uint32", snapshot.version),
      ("records", "repeated FileRecord", f"{len(snapshot.records)} objects"),
      ("postings", "repeated PostingList", f"{len(snapshot.postings)} objects"),
      ("root_path", "string", snapshot.root_path),
  ]
  records_html = "\n".join(
      render_file_record_object(record, snapshot.root_path)
      for record in sorted(snapshot.records, key=lambda item: item.id)
  )
  records_html = records_html or '<p class="empty">No FileRecord objects.</p>'
  postings_html = "\n".join(
      render_posting_list_object(posting_list, idx, document_by_id,
                                 snapshot.root_path)
      for idx, posting_list in enumerate(snapshot.postings)
  )
  postings_html = postings_html or '<p class="empty">No PostingList objects.</p>'

  return (
      f'<input class="filter" type="search" placeholder="Filter objects" '
      f'oninput="filterObjects(this, \'{object_id}\')">'
      f'<div id="{object_id}" class="object-list">'
      '<details class="object-card" open data-filter="index root version records postings">'
      "<summary>"
      '<span class="object-name">Index</span>'
      f'<span class="object-meta">{len(snapshot.records):,} records - '
      f'{len(snapshot.postings):,} posting lists</span>'
      "</summary>"
      f"{render_field_table(index_fields)}"
      "</details>"
      '<details class="object-group" open>'
      f'<summary><span class="object-name">FileRecord objects</span>'
      f'<span class="object-meta">{len(snapshot.records):,} objects</span></summary>'
      f"{records_html}"
      "</details>"
      '<details class="object-group">'
      f'<summary><span class="object-name">PostingList objects</span>'
      f'<span class="object-meta">{len(snapshot.postings):,} objects</span></summary>'
      f"{postings_html}"
      "</details>"
      "</div>"
  )


def render_snapshot(snapshot: IndexSnapshot, index: int, top_terms: int) -> str:
  totals = snapshot_totals(snapshot)
  terms = term_stats(snapshot)
  extensions = extension_stats(snapshot)
  tree = build_tree(snapshot)
  current_badge = '<span class="badge">current</span>' if snapshot.current else ""

  terms_html = render_bars(
      terms,
      value_index=2,
      limit=top_terms,
      value_formatter=lambda row: f"{row[2]:,} lines / {row[1]:,} docs",
  )
  extensions_html = render_bars(
      extensions,
      value_index=1,
      limit=20,
      value_formatter=lambda row: f"{row[1]:,} files / {format_bytes(row[2])}",
  )
  tree_html = render_tree_node("root", tree)
  file_table = render_file_table(snapshot, f"files-{index}")
  stored_objects = render_stored_objects(snapshot, f"objects-{index}")

  return f"""
<section class="index-section" id="index-{index}">
  <div class="section-title">
    <div>
      <h2>{escape(snapshot.path.name)} {current_badge}</h2>
      <p class="subtle mono">{escape(canonical_key(snapshot.path))}</p>
    </div>
    <span class="version">format v{snapshot.version}</span>
  </div>
  <dl class="metadata">
    <div><dt>Root path</dt><dd class="mono">{escape(snapshot.root_path or "-")}</dd></div>
    <div><dt>Index file</dt><dd class="mono">{escape(str(snapshot.path))}</dd></div>
  </dl>
  <div class="stats-grid">
    {render_stat_cards(totals)}
  </div>
  <div class="grid-2">
    <section class="panel">
      <h3>Top Terms</h3>
      {terms_html}
    </section>
    <section class="panel">
      <h3>Extensions</h3>
      {extensions_html}
    </section>
  </div>
  <section class="panel">
    <h3>File Tree</h3>
    <div class="tree">{tree_html}</div>
  </section>
  <section class="panel">
    <h3>Files</h3>
    {file_table}
  </section>
  <section class="panel objects-panel">
    <h3>Stored Objects</h3>
    {stored_objects}
  </section>
</section>
"""


def render_html(
    snapshots: list[IndexSnapshot],
    warnings: list[str],
    current: CurrentPointer | None,
    top_terms: int,
) -> str:
  generated_at = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
  summary_totals = {
      "indexes": len(snapshots),
      "files": sum(len(snapshot.records) for snapshot in snapshots),
      "terms": sum(len(snapshot.postings) for snapshot in snapshots),
      "bytes": sum(sum(record.size for record in snapshot.records)
                   for snapshot in snapshots),
  }
  nav = "\n".join(
      f'<a href="#index-{idx}">{escape(snapshot.path.name)}'
      f' <span>{len(snapshot.records):,} files</span></a>'
      for idx, snapshot in enumerate(snapshots)
  )
  warning_html = ""
  if warnings:
    warning_html = (
        '<section class="warnings"><h2>Warnings</h2><ul>'
        + "".join(f"<li>{escape(warning)}</li>" for warning in warnings)
        + "</ul></section>"
    )

  current_html = ""
  if current is not None:
    current_html = (
        '<dl class="current-pointer">'
        f"<div><dt>Current version</dt><dd>{current.version}</dd></div>"
        f"<div><dt>Current root</dt><dd>{escape(current.root_path)}</dd></div>"
        f"<div><dt>Current index</dt><dd>{escape(current.index_file)}</dd></div>"
        "</dl>"
    )

  sections = "\n".join(
      render_snapshot(snapshot, idx, top_terms)
      for idx, snapshot in enumerate(snapshots)
  )

  return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>MiniSearch Index Visualization</title>
  <style>
    :root {{
      color-scheme: light;
      --bg: #f6f5ef;
      --surface: #ffffff;
      --text: #242424;
      --muted: #676b73;
      --border: #d9d7ce;
      --accent: #2c6b5b;
      --accent-2: #a85d2a;
      --track: #e9e6dc;
      --shadow: 0 1px 2px rgba(0, 0, 0, 0.05);
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont,
        "Segoe UI", sans-serif;
      line-height: 1.5;
    }}
    header {{
      padding: 28px 32px 18px;
      border-bottom: 1px solid var(--border);
      background: #fbfaf6;
    }}
    h1, h2, h3, p {{ margin: 0; }}
    h1 {{ font-size: 28px; font-weight: 700; }}
    h2 {{ font-size: 20px; font-weight: 700; }}
    h3 {{ font-size: 15px; font-weight: 700; margin-bottom: 12px; }}
    main {{
      display: grid;
      grid-template-columns: minmax(180px, 240px) minmax(0, 1fr);
      gap: 24px;
      padding: 24px 32px 40px;
    }}
    nav {{
      position: sticky;
      top: 20px;
      align-self: start;
      display: grid;
      gap: 8px;
    }}
    nav a {{
      display: grid;
      gap: 2px;
      color: var(--text);
      text-decoration: none;
      padding: 9px 10px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--surface);
      box-shadow: var(--shadow);
      overflow-wrap: anywhere;
    }}
    nav a span {{ color: var(--muted); font-size: 12px; }}
    .overview {{
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-top: 18px;
    }}
    .overview div, .stat {{
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 10px 12px;
      box-shadow: var(--shadow);
    }}
    .overview span, .stat span, dt {{
      display: block;
      color: var(--muted);
      font-size: 12px;
    }}
    .overview strong, .stat strong {{
      display: block;
      font-size: 19px;
      margin-top: 2px;
    }}
    .content {{ min-width: 0; display: grid; gap: 20px; }}
    .index-section {{
      display: grid;
      gap: 16px;
      padding-bottom: 28px;
      border-bottom: 1px solid var(--border);
    }}
    .section-title {{
      display: flex;
      justify-content: space-between;
      gap: 16px;
      align-items: start;
    }}
    .subtle {{ color: var(--muted); margin-top: 4px; overflow-wrap: anywhere; }}
    .mono {{
      font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
      font-size: 12px;
    }}
    .badge, .version {{
      display: inline-flex;
      align-items: center;
      white-space: nowrap;
      border: 1px solid #b9d3ca;
      background: #e7f1ed;
      color: #1f5a4c;
      border-radius: 999px;
      padding: 3px 8px;
      font-size: 12px;
      font-weight: 700;
      vertical-align: middle;
    }}
    .version {{
      border-color: #d9c5ad;
      background: #fbefe2;
      color: #8a4c23;
    }}
    .metadata, .current-pointer {{
      display: grid;
      gap: 8px;
      margin: 0;
    }}
    .metadata div, .current-pointer div {{
      display: grid;
      grid-template-columns: 120px minmax(0, 1fr);
      gap: 12px;
      align-items: baseline;
    }}
    dd {{ margin: 0; overflow-wrap: anywhere; }}
    .stats-grid {{
      display: grid;
      grid-template-columns: repeat(6, minmax(0, 1fr));
      gap: 10px;
    }}
    .grid-2 {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px;
    }}
    .panel {{
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 14px;
      box-shadow: var(--shadow);
      overflow: auto;
    }}
    .bar-row {{
      display: grid;
      grid-template-columns: minmax(80px, 180px) minmax(100px, 1fr) minmax(120px, auto);
      align-items: center;
      gap: 10px;
      margin: 7px 0;
      min-width: 0;
    }}
    .bar-label {{
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
      font-size: 12px;
    }}
    .bar-track {{
      height: 10px;
      background: var(--track);
      border-radius: 999px;
      overflow: hidden;
    }}
    .bar-fill {{
      display: block;
      height: 100%;
      background: linear-gradient(90deg, var(--accent), #4f947f);
    }}
    .bar-value {{
      color: var(--muted);
      font-size: 12px;
      text-align: right;
      white-space: nowrap;
    }}
    .tree details {{
      margin-left: 14px;
      border-left: 1px solid var(--border);
      padding-left: 10px;
    }}
    .tree > details {{ margin-left: 0; border-left: 0; padding-left: 0; }}
    summary {{
      cursor: pointer;
      display: flex;
      justify-content: space-between;
      gap: 12px;
      padding: 4px 0;
    }}
    .tree-list {{
      list-style: none;
      padding: 0;
      margin: 0 0 0 8px;
    }}
    .tree-file {{
      display: flex;
      justify-content: space-between;
      gap: 12px;
      padding: 3px 0 3px 18px;
    }}
    .file-name, .dir-name {{ overflow-wrap: anywhere; }}
    .file-name.indexed::after {{
      content: " text";
      color: var(--accent);
      font-size: 11px;
      font-weight: 700;
    }}
    .tree-meta {{
      color: var(--muted);
      font-size: 12px;
      white-space: nowrap;
    }}
    .filter {{
      width: min(420px, 100%);
      margin-bottom: 10px;
      padding: 8px 10px;
      border: 1px solid var(--border);
      border-radius: 8px;
      font: inherit;
      background: #fff;
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      font-size: 13px;
    }}
    th, td {{
      border-bottom: 1px solid var(--border);
      padding: 7px 8px;
      text-align: left;
      vertical-align: top;
    }}
    th {{
      color: var(--muted);
      font-size: 12px;
      font-weight: 700;
      background: #fbfaf6;
      position: sticky;
      top: 0;
    }}
    .path {{ min-width: 260px; overflow-wrap: anywhere; }}
    .numeric {{ text-align: right; white-space: nowrap; }}
    .hash {{ color: var(--muted); white-space: nowrap; }}
    .object-list {{
      display: grid;
      gap: 10px;
    }}
    .object-card, .object-group, .nested-object {{
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #fff;
      padding: 0 10px 8px;
    }}
    .object-group {{
      background: #fbfaf6;
    }}
    .object-card > summary,
    .object-group > summary,
    .nested-object > summary {{
      padding: 8px 0;
    }}
    .object-name {{
      font-weight: 700;
      min-width: max-content;
    }}
    .object-meta {{
      color: var(--muted);
      font-size: 12px;
      text-align: right;
      overflow-wrap: anywhere;
    }}
    .nested-objects {{
      display: grid;
      gap: 8px;
      margin: 10px 0 0 14px;
    }}
    .field-table {{
      margin: 6px 0 2px;
      table-layout: auto;
    }}
    .field-table th,
    .field-table td {{
      position: static;
      background: transparent;
    }}
    .field-table th[scope="row"] {{
      color: var(--text);
      width: 170px;
    }}
    .field-type {{
      color: var(--muted);
      white-space: nowrap;
      width: 150px;
    }}
    .field-value {{
      white-space: pre-wrap;
      overflow-wrap: anywhere;
    }}
    .note, .empty {{ color: var(--muted); font-size: 13px; }}
    .warnings {{
      background: #fff7e8;
      border: 1px solid #e3c798;
      border-radius: 8px;
      padding: 14px;
    }}
    .warnings h2 {{ font-size: 16px; margin-bottom: 8px; }}
    .warnings ul {{ margin: 0; padding-left: 20px; }}
    @media (max-width: 920px) {{
      main {{ grid-template-columns: 1fr; padding: 18px; }}
      nav {{ position: static; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); }}
      .stats-grid, .grid-2 {{ grid-template-columns: repeat(2, minmax(0, 1fr)); }}
    }}
    @media (max-width: 620px) {{
      header {{ padding: 20px 18px 14px; }}
      .stats-grid, .grid-2 {{ grid-template-columns: 1fr; }}
      .metadata div, .current-pointer div {{ grid-template-columns: 1fr; gap: 2px; }}
      .bar-row {{ grid-template-columns: 1fr; gap: 4px; }}
      .bar-value {{ text-align: left; }}
      .section-title {{ display: grid; }}
    }}
  </style>
</head>
<body>
  <header>
    <h1>MiniSearch Index Visualization</h1>
    <p class="subtle">Generated {escape(generated_at)}</p>
    <div class="overview">
      <div><span>Indexes</span><strong>{summary_totals["indexes"]:,}</strong></div>
      <div><span>Files</span><strong>{summary_totals["files"]:,}</strong></div>
      <div><span>Terms</span><strong>{summary_totals["terms"]:,}</strong></div>
      <div><span>Total size</span><strong>{escape(format_bytes(summary_totals["bytes"]))}</strong></div>
    </div>
    {current_html}
  </header>
  <main>
    <nav aria-label="Indexes">
      {nav}
    </nav>
    <div class="content">
      {warning_html}
      {sections}
    </div>
  </main>
  <script>
    function filterTable(input, tableId) {{
      const query = input.value.toLowerCase();
      const table = document.getElementById(tableId);
      for (const row of table.tBodies[0].rows) {{
        row.style.display = row.textContent.toLowerCase().includes(query)
          ? ""
          : "none";
      }}
    }}
    function filterObjects(input, containerId) {{
      const query = input.value.toLowerCase();
      const container = document.getElementById(containerId);
      for (const object of container.querySelectorAll(".object-card, .nested-object")) {{
        const value = object.dataset.filter || object.textContent;
        object.style.display = value.toLowerCase().includes(query) ? "" : "none";
      }}
    }}
  </script>
</body>
</html>
"""


def main() -> int:
  args = parse_args()
  input_path = pathlib.Path(args.path).expanduser()
  output_path = pathlib.Path(args.output).expanduser()
  proto_path = pathlib.Path(args.proto).expanduser()

  if not proto_path.exists():
    print(f"error: proto schema not found: {proto_path}", file=sys.stderr)
    return 2

  warnings: list[str] = []
  try:
    index_files = find_index_files(input_path)
  except (FileNotFoundError, RuntimeError) as error:
    print(f"error: {error}", file=sys.stderr)
    return 2

  if not index_files:
    print(f"error: no .pb files found under {input_path}", file=sys.stderr)
    return 2

  current: CurrentPointer | None = None
  current_path = pathlib.Path(args.current).expanduser() if args.current else (
      candidate_current_path(input_path)
  )
  if current_path is not None and current_path.exists():
    try:
      current_text = run_protoc_decode(
          current_path, CURRENT_MESSAGE, proto_path, args.protoc
      )
      current = parse_current_text(current_text)
    except RuntimeError as error:
      warnings.append(str(error))

  snapshots: list[IndexSnapshot] = []
  for index_file in index_files:
    try:
      decoded = run_protoc_decode(index_file, INDEX_MESSAGE, proto_path,
                                  args.protoc)
      snapshots.append(parse_index_text(decoded, index_file))
    except RuntimeError as error:
      warnings.append(str(error))

  snapshots = mark_current(snapshots, current)
  if not snapshots:
    print("error: no index protobuf files could be decoded", file=sys.stderr)
    for warning in warnings:
      print(f"warning: {warning}", file=sys.stderr)
    return 1

  output_path.parent.mkdir(parents=True, exist_ok=True)
  output_path.write_text(
      render_html(snapshots, warnings, current, args.top_terms),
      encoding="utf-8",
  )
  print(f"wrote {output_path}")
  print(f"decoded {len(snapshots)} index file(s)")
  for warning in warnings:
    print(f"warning: {warning}", file=sys.stderr)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
