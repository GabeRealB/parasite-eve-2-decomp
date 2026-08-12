#!/usr/bin/env python3
"""PE2 assets viewer — browse by type or stage tree with previews.

Usage (from repo root)::

    python3 tools/peassets/viewer.py
    python3 tools/peassets/viewer.py assets/USA

Works with full extract (inflated type dirs + ``stages.json``) and
minimal/raw-only extract (``raw/{type}/`` + optional ``pe2pkg/`` overlays).
"""

from __future__ import annotations

import argparse
import json
import sys
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from tkinter import filedialog, ttk
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

TYPE_DIRS = ("pe2pkg", "pe2img", "pe2clut", "pe2cap2", "bs", "spk", "txt")
IMAGE_EXTS = {".png", ".pe2img", ".pe2clut"}
TEXT_EXTS = {".txt", ".json", ".md", ".xml", ".cnf"}
PREVIEW_HEX_BYTES = 512
PREVIEW_TEXT_CHARS = 200_000

# stages.json type id → raw type-dir + extension
_TYPE_RAW_EXT: dict[str, tuple[str, str]] = {
    "room_pkg": ("pe2pkg", ".pe2pkg"),
    "image": ("pe2img", ".pe2img"),
    "clut": ("pe2clut", ".pe2clut"),
    "music": ("spk", ".spk"),
    "ascii": ("txt", ".txt"),
    "cap2": ("pe2cap2", ".pe2cap2"),
    "room_background": ("bs", ".bs"),
}


@dataclass
class AssetItem:
    """One browsable asset (on-disk file)."""

    path: Path
    label: str
    kind: str  # type dir name or "stage" / "sidecar"
    source: str  # "inflated" | "raw" | "stage"
    meta: dict[str, Any] | None = None
    # Sibling CLUT paths when known from the same stage file (for pe2img).
    sibling_cluts: list[Path] | None = None


def _fmt_size(n: int) -> str:
    if n < 1024:
        return f"{n} B"
    if n < 1024 * 1024:
        return f"{n / 1024:.1f} KiB"
    return f"{n / (1024 * 1024):.2f} MiB"


def _hexdump(data: bytes, limit: int = PREVIEW_HEX_BYTES) -> str:
    lines: list[str] = []
    chunk = data[:limit]
    for i in range(0, len(chunk), 16):
        row = chunk[i : i + 16]
        hex_part = " ".join(f"{b:02x}" for b in row)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        lines.append(f"{i:08x}  {hex_part:<48}  {ascii_part}")
    if len(data) > limit:
        lines.append(f"… ({len(data) - limit} more bytes)")
    return "\n".join(lines)


def _printable_strings(data: bytes, min_len: int = 6, limit: int = 40) -> list[str]:
    out: list[str] = []
    cur: list[str] = []
    for b in data:
        if 32 <= b < 127:
            cur.append(chr(b))
        else:
            if len(cur) >= min_len:
                out.append("".join(cur))
                if len(out) >= limit:
                    return out
            cur = []
    if len(cur) >= min_len and len(out) < limit:
        out.append("".join(cur))
    return out


def _load_stages(assets_root: Path) -> dict[str, Any] | None:
    path = assets_root / "stages.json"
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def prefer_raw_blob(assets_root: Path, path: Path) -> Path:
    """Prefer ``raw/{type}/stem.pe2*`` over inflated PNG/duplicates.

    Sibling CLUT lists must point at on-disc ``.pe2clut`` blobs. Inflated
    ``pe2clut/*.png`` is a colour strip for viewing, not the palette payload;
    loading it as a CLUT produces wrong results vs ``raw/pe2clut/*.pe2clut``.
    """
    if not path:
        return path
    suf = path.suffix.lower()
    stem = path.stem
    # Already raw pe2
    if "raw" in path.parts and suf in (".pe2img", ".pe2clut", ".pe2pkg"):
        return path
    if suf == ".pe2img":
        raw = assets_root / "raw" / "pe2img" / f"{stem}.pe2img"
        return raw if raw.exists() else path
    if suf == ".pe2clut":
        raw = assets_root / "raw" / "pe2clut" / f"{stem}.pe2clut"
        return raw if raw.exists() else path
    if suf == ".png":
        # pe2img/foo.png or pe2clut/foo.png
        if "pe2img" in path.parts or path.parent.name == "pe2img":
            raw = assets_root / "raw" / "pe2img" / f"{stem}.pe2img"
            if raw.exists():
                return raw
        if "pe2clut" in path.parts or path.parent.name == "pe2clut":
            raw = assets_root / "raw" / "pe2clut" / f"{stem}.pe2clut"
            if raw.exists():
                return raw
    return path


def _resolve_on_disk(
    assets_root: Path, rel: str, type_id: str | None = None
) -> Path | None:
    """Resolve a stages.json path, preferring raw pe2 blobs for image/clut."""
    if not rel:
        return None
    stem = Path(rel).stem
    parts = Path(rel).parts

    # Prefer raw for image / clut (decode-ready payloads).
    if type_id in ("image", "clut") or (
        parts and parts[0] in ("pe2img", "pe2clut")
    ):
        if type_id in _TYPE_RAW_EXT:
            tdir, ext = _TYPE_RAW_EXT[type_id]
        elif parts:
            tdir = parts[0]
            ext = ".pe2img" if tdir == "pe2img" else ".pe2clut"
        else:
            tdir, ext = "pe2img", ".pe2img"
        raw = assets_root / "raw" / tdir / f"{stem}{ext}"
        if raw.exists():
            return raw

    p = assets_root / rel
    if p.exists():
        return prefer_raw_blob(assets_root, p)

    if type_id in _TYPE_RAW_EXT:
        tdir, ext = _TYPE_RAW_EXT[type_id]
        raw = assets_root / "raw" / tdir / f"{stem}{ext}"
        if raw.exists():
            return raw
    if len(parts) >= 2 and parts[0] in TYPE_DIRS:
        tdir = parts[0]
        for ext in (
            ".pe2img",
            ".pe2clut",
            ".pe2pkg",
            ".spk",
            ".txt",
            ".bs",
            ".pe2cap2",
            Path(rel).suffix,
        ):
            raw = assets_root / "raw" / tdir / f"{stem}{ext}"
            if raw.exists():
                return raw
    return None


def _iter_stage_files(
    stages: dict[str, Any],
) -> list[dict[str, dict[str, Any]]]:
    """Yield each file's chunks dict from stages.json."""
    files: list[dict[str, dict[str, Any]]] = []
    for stage in stages.values():
        if not isinstance(stage, dict):
            continue
        if isinstance(stage.get("files"), dict):
            for chunks in stage["files"].values():
                if isinstance(chunks, dict):
                    files.append(chunks)
        if isinstance(stage.get("folders"), dict):
            for folder_files in stage["folders"].values():
                if not isinstance(folder_files, dict):
                    continue
                for chunks in folder_files.values():
                    if isinstance(chunks, dict):
                        files.append(chunks)
    return files


def build_image_clut_index(
    assets_root: Path, stages: dict[str, Any] | None
) -> dict[str, list[Path]]:
    """Map image identity → sibling CLUT paths from the same stage file.

    Keys include resolved path, file name, and stem so lookups work for both
    inflated PNGs and raw ``.pe2img`` paths.
    """
    index: dict[str, list[Path]] = {}
    if not stages:
        return index

    def _add(key: str, cluts: list[Path]) -> None:
        existing = index.setdefault(key, [])
        for c in cluts:
            if c not in existing:
                existing.append(c)

    for chunks in _iter_stage_files(stages):
        images: list[Path] = []
        cluts: list[Path] = []
        for ent in chunks.values():
            if not isinstance(ent, dict):
                continue
            t = ent.get("type")
            path = _resolve_on_disk(assets_root, ent.get("path") or "", t)
            if path is None:
                continue
            is_img = t == "image" or path.suffix.lower() == ".pe2img" or (
                path.suffix.lower() == ".png" and "pe2img" in path.parts
            )
            is_clut = t == "clut" or path.suffix.lower() == ".pe2clut" or (
                path.suffix.lower() == ".png" and "pe2clut" in path.parts
            )
            if is_img:
                path = prefer_raw_blob(assets_root, path)
                if path.exists() and path.suffix.lower() == ".pe2img":
                    images.append(path.resolve())
            elif is_clut:
                path = prefer_raw_blob(assets_root, path)
                if path.exists() and path.suffix.lower() == ".pe2clut":
                    cluts.append(path.resolve())
        if not images or not cluts:
            continue
        seen: set[Path] = set()
        uniq_cluts: list[Path] = []
        for c in cluts:
            if c not in seen:
                seen.add(c)
                uniq_cluts.append(c)
        for img in images:
            _add(str(img), uniq_cluts)
            _add(img.name, uniq_cluts)
            _add(img.stem, uniq_cluts)
    return index


def list_all_cluts(assets_root: Path) -> list[Path]:
    """All available pe2clut blobs (raw preferred)."""
    out: list[Path] = []
    raw = assets_root / "raw" / "pe2clut"
    infl = assets_root / "pe2clut"
    if raw.is_dir():
        out.extend(sorted(p for p in raw.glob("*.pe2clut") if p.is_file()))
    elif infl.is_dir():
        # only pngs — skip, need pe2 for palette load
        pass
    return out


def pe2img_path_for_preview(assets_root: Path, path: Path) -> Path | None:
    """Return a .pe2img path for decoding (raw), or None if only PNG available."""
    if path.suffix.lower() == ".pe2img":
        return path
    if path.suffix.lower() == ".png":
        raw = assets_root / "raw" / "pe2img" / f"{path.stem}.pe2img"
        if raw.exists():
            return raw
    return None


def scan_type_assets(assets_root: Path) -> dict[str, list[AssetItem]]:
    """Group assets by type dir, preferring inflated over raw of the same name."""
    by_type: dict[str, list[AssetItem]] = {t: [] for t in TYPE_DIRS}
    for type_dir in TYPE_DIRS:
        infl_dir = assets_root / type_dir
        raw_dir = assets_root / "raw" / type_dir
        seen_stems: set[str] = set()

        if infl_dir.is_dir():
            for p in sorted(infl_dir.iterdir()):
                if not p.is_file():
                    continue
                # Skip sidecar meta next to PNGs in listing primary? keep them.
                if p.suffix.lower() == ".json" and p.name.endswith(
                    (".pe2img.json", ".pe2clut.json")
                ):
                    continue
                seen_stems.add(p.stem)
                by_type[type_dir].append(
                    AssetItem(
                        path=p,
                        label=p.name,
                        kind=type_dir,
                        source="inflated",
                    )
                )

        if raw_dir.is_dir():
            for p in sorted(raw_dir.iterdir()):
                if not p.is_file():
                    continue
                # Prefer inflated if same stem exists (e.g. pe2img_0.png vs .pe2img)
                if p.stem in seen_stems:
                    continue
                # If inflated has .png for this stem, skip raw pe2
                if any(
                    (assets_root / type_dir / f"{p.stem}{s}").exists()
                    for s in (".png", p.suffix)
                ):
                    continue
                by_type[type_dir].append(
                    AssetItem(
                        path=p,
                        label=f"{p.name}  [raw]",
                        kind=type_dir,
                        source="raw",
                    )
                )

    return by_type


class AssetViewer(tk.Tk):
    def __init__(self, assets_root: Path):
        super().__init__()
        self.title("PE2 Assets Viewer")
        self.geometry("1100x720")
        self.minsize(800, 500)

        self.assets_root = assets_root.resolve()
        self.stages = _load_stages(self.assets_root)
        self._photo_ref: Any = None  # keep PhotoImage alive
        self._bg_photo: Any = None
        self._palette_photo: Any = None
        self._item_map: dict[str, AssetItem] = {}
        self._type_items: dict[str, list[AssetItem]] = {}  # type_dir → items
        self._filter_var = tk.StringVar(value="")
        self._status_var = tk.StringVar(value="")
        self._filter_after: str | None = None
        # image → sibling CLUTs from stages.json
        self._img_clut_index: dict[str, list[Path]] = {}
        # current pe2img decode state for CLUT swapping
        self._pe2img_path: Path | None = None
        self._pe2img_data: bytes | None = None
        self._clut_choices: list[tuple[str, Path | None]] = []  # label, path|None
        self._clut_color_cache: dict[str, list[int] | None] = {}
        self._current_clut_colors: list[int] | None = None

        self._build_ui()
        self.reload()

    # ------------------------------------------------------------------ UI
    def _build_ui(self) -> None:
        # Menu
        menubar = tk.Menu(self)
        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="Open assets folder…", command=self._pick_root)
        file_menu.add_command(label="Reload", command=self.reload)
        file_menu.add_separator()
        file_menu.add_command(label="Quit", command=self.destroy)
        menubar.add_cascade(label="File", menu=file_menu)
        self.config(menu=menubar)

        # Top bar
        top = ttk.Frame(self, padding=(8, 6))
        top.pack(side=tk.TOP, fill=tk.X)
        ttk.Label(top, text="Assets:").pack(side=tk.LEFT)
        self.root_label = ttk.Label(top, text=str(self.assets_root), foreground="#444")
        self.root_label.pack(side=tk.LEFT, padx=(4, 12))
        ttk.Label(top, text="Filter:").pack(side=tk.LEFT)
        filt = ttk.Entry(top, textvariable=self._filter_var, width=28)
        filt.pack(side=tk.LEFT, padx=4)
        filt.bind("<KeyRelease>", self._on_filter_key)
        ttk.Button(top, text="Reload", command=self.reload).pack(side=tk.RIGHT)

        # Main split
        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 4))

        left = ttk.Frame(paned)
        right = ttk.Frame(paned)
        paned.add(left, weight=1)
        paned.add(right, weight=2)

        self.notebook = ttk.Notebook(left)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        # By type
        type_frame = ttk.Frame(self.notebook)
        self.notebook.add(type_frame, text="By type")
        self.type_tree = ttk.Treeview(
            type_frame, columns=("info",), show="tree headings", selectmode="browse"
        )
        self.type_tree.heading("#0", text="Asset")
        self.type_tree.heading("info", text="Size")
        self.type_tree.column("#0", width=260, stretch=True)
        self.type_tree.column("info", width=90, stretch=False, anchor=tk.E)
        type_scroll = ttk.Scrollbar(
            type_frame, orient=tk.VERTICAL, command=self.type_tree.yview
        )
        self.type_tree.configure(yscrollcommand=type_scroll.set)
        self.type_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        type_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.type_tree.bind("<<TreeviewSelect>>", lambda e: self._on_select(self.type_tree))
        self.type_tree.bind("<<TreeviewOpen>>", self._on_type_open)

        # Stage tree
        stage_frame = ttk.Frame(self.notebook)
        self.notebook.add(stage_frame, text="Stage tree")
        self.stage_tree = ttk.Treeview(
            stage_frame, columns=("info",), show="tree headings", selectmode="browse"
        )
        self.stage_tree.heading("#0", text="Stage / file / chunk")
        self.stage_tree.heading("info", text="Type")
        self.stage_tree.column("#0", width=280, stretch=True)
        self.stage_tree.column("info", width=90, stretch=False)
        stage_scroll = ttk.Scrollbar(
            stage_frame, orient=tk.VERTICAL, command=self.stage_tree.yview
        )
        self.stage_tree.configure(yscrollcommand=stage_scroll.set)
        self.stage_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        stage_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.stage_tree.bind(
            "<<TreeviewSelect>>", lambda e: self._on_select(self.stage_tree)
        )
        self.stage_tree.bind("<<TreeviewOpen>>", self._on_stage_open)

        # Preview
        prev_header = ttk.Frame(right)
        prev_header.pack(side=tk.TOP, fill=tk.X, pady=(0, 4))
        ttk.Label(prev_header, text="Preview", font=("", 11, "bold")).pack(side=tk.LEFT)
        self.info_label = ttk.Label(prev_header, text="", foreground="#555")
        self.info_label.pack(side=tk.LEFT, padx=12)

        self.preview_nb = ttk.Notebook(right)
        self.preview_nb.pack(fill=tk.BOTH, expand=True)

        # Image canvas + CLUT controls
        img_frame = ttk.Frame(self.preview_nb)
        self.preview_nb.add(img_frame, text="Image")
        clut_bar = ttk.Frame(img_frame)
        clut_bar.pack(side=tk.TOP, fill=tk.X, pady=(0, 4))
        ttk.Label(clut_bar, text="CLUT:").pack(side=tk.LEFT)
        self._clut_apply = tk.BooleanVar(value=True)
        self._clut_apply_cb = ttk.Checkbutton(
            clut_bar,
            text="Apply",
            variable=self._clut_apply,
            command=self._on_clut_changed,
        )
        self._clut_apply_cb.pack(side=tk.LEFT, padx=(4, 8))
        self._clut_var = tk.StringVar(value="(none)")
        self._clut_combo = ttk.Combobox(
            clut_bar,
            textvariable=self._clut_var,
            state="disabled",
            width=42,
        )
        self._clut_combo.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self._clut_combo.bind("<<ComboboxSelected>>", lambda _e: self._on_clut_changed())
        self._clut_scope = tk.StringVar(value="siblings")
        ttk.Radiobutton(
            clut_bar,
            text="Siblings",
            variable=self._clut_scope,
            value="siblings",
            command=self._on_clut_scope_changed,
        ).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Radiobutton(
            clut_bar,
            text="All CLUTs",
            variable=self._clut_scope,
            value="all",
            command=self._on_clut_scope_changed,
        ).pack(side=tk.LEFT)
        self.img_canvas = tk.Canvas(img_frame, bg="#2b2b2b", highlightthickness=0)
        self.img_canvas.pack(fill=tk.BOTH, expand=True)
        self.img_canvas.bind("<Configure>", self._on_canvas_resize)

        # Txt/hex (decoded / inflated view) and Raw (on-disc bytes)
        self.text_deflated = self._make_scrolled_text(
            self.preview_nb, "Txt / hex"
        )
        self.text_raw = self._make_scrolled_text(self.preview_nb, "Raw")

        # Status
        ttk.Label(self, textvariable=self._status_var, anchor=tk.W, padding=(8, 2)).pack(
            side=tk.BOTTOM, fill=tk.X
        )

        self._current_pil = None
        self._current_item: AssetItem | None = None

    def _make_scrolled_text(self, notebook: ttk.Notebook, title: str) -> tk.Text:
        frame = ttk.Frame(notebook)
        notebook.add(frame, text=title)
        mono = ("Menlo", 11) if sys.platform == "darwin" else ("Consolas", 10)
        text = tk.Text(
            frame,
            wrap=tk.NONE,
            font=mono,
            bg="#1e1e1e",
            fg="#d4d4d4",
            insertbackground="#d4d4d4",
            relief=tk.FLAT,
        )
        sy = ttk.Scrollbar(frame, orient=tk.VERTICAL, command=text.yview)
        sx = ttk.Scrollbar(frame, orient=tk.HORIZONTAL, command=text.xview)
        text.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
        text.grid(row=0, column=0, sticky="nsew")
        sy.grid(row=0, column=1, sticky="ns")
        sx.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        return text

    @staticmethod
    def _set_text_widget(widget: tk.Text, content: str) -> None:
        widget.configure(state=tk.NORMAL)
        widget.delete("1.0", tk.END)
        widget.insert(tk.END, content)
        widget.configure(state=tk.DISABLED)

    def _clear_text_tabs(self, msg: str = "") -> None:
        self._set_text_widget(self.text_deflated, msg)
        self._set_text_widget(self.text_raw, msg if msg else "")

    # -------------------------------------------------------------- data load
    def _pick_root(self) -> None:
        path = filedialog.askdirectory(
            initialdir=str(self.assets_root), title="Select assets root"
        )
        if path:
            self.assets_root = Path(path).resolve()
            self.root_label.configure(text=str(self.assets_root))
            self.reload()

    def reload(self) -> None:
        self.stages = _load_stages(self.assets_root)
        self._img_clut_index = build_image_clut_index(self.assets_root, self.stages)
        self._item_map.clear()
        self._populate_type_tree()
        self._populate_stage_tree()
        mode = "full" if self.stages else "raw/minimal"
        n_types = sum(
            1
            for t in TYPE_DIRS
            if (self.assets_root / t).is_dir()
            or (self.assets_root / "raw" / t).is_dir()
        )
        n_links = sum(len(v) for v in self._img_clut_index.values())
        self._status_var.set(
            f"{self.assets_root}  ·  {mode} extract  ·  {n_types} type dirs  ·  "
            f"{len(self._img_clut_index)} images with sibling CLUTs ({n_links} links)"
        )
        self._clear_preview("Select an asset")

    def _populate_type_tree(self) -> None:
        tree = self.type_tree
        tree.delete(*tree.get_children())
        # Drop stale asset iids for this tree
        self._item_map = {
            k: v for k, v in self._item_map.items() if not k.startswith("asset:")
        }
        self._type_items = scan_type_assets(self.assets_root)
        filt = self._filter_var.get().strip().lower()

        for type_dir in TYPE_DIRS:
            items = self._type_items.get(type_dir, [])
            if filt:
                items = [i for i in items if filt in i.label.lower()]
            if not items and not (
                (self.assets_root / type_dir).is_dir()
                or (self.assets_root / "raw" / type_dir).is_dir()
            ):
                continue
            # Lazy: placeholder child so the type node is expandable
            parent = tree.insert(
                "",
                tk.END,
                iid=f"type:{type_dir}",
                text=f"{type_dir}  ({len(items)})",
                values=("",),
                open=False,
            )
            if items:
                tree.insert(parent, tk.END, iid=f"typeph:{type_dir}", text="…")

    def _on_type_open(self, event: tk.Event) -> None:
        tree = self.type_tree
        iid = tree.focus()
        if not iid.startswith("type:") or iid.startswith("typeph:"):
            return
        type_dir = iid.split(":", 1)[1]
        # Already populated?
        kids = tree.get_children(iid)
        if kids and not str(kids[0]).startswith("typeph:"):
            return
        for kid in kids:
            tree.delete(kid)
        items = self._type_items.get(type_dir, [])
        filt = self._filter_var.get().strip().lower()
        if filt:
            items = [i for i in items if filt in i.label.lower()]
        for item in items:
            path = item.path
            if type_dir == "pe2img":
                pe2 = pe2img_path_for_preview(self.assets_root, path)
                if pe2 is not None:
                    path = pe2
                cluts = self._lookup_cluts(path)
                item = AssetItem(
                    path=path,
                    label=item.label,
                    kind=item.kind,
                    source=item.source,
                    meta=item.meta,
                    sibling_cluts=cluts or None,
                )
            aid = f"asset:{item.source}:{item.path}"
            self._item_map[aid] = item
            try:
                size = _fmt_size(item.path.stat().st_size)
            except OSError:
                size = "?"
            tree.insert(iid, tk.END, iid=aid, text=item.label, values=(size,))

    def _lookup_cluts(self, pe2img_path: Path) -> list[Path]:
        keys = [
            str(pe2img_path.resolve()),
            pe2img_path.name,
            pe2img_path.stem,
        ]
        for k in keys:
            if k in self._img_clut_index:
                return list(self._img_clut_index[k])
        return []

    def _on_filter_key(self, _event: tk.Event | None = None) -> None:
        if self._filter_after is not None:
            self.after_cancel(self._filter_after)
        self._filter_after = self.after(250, self._apply_filter)

    def _on_stage_open(self, _event: tk.Event) -> None:
        # Stage tree is fully built; nothing lazy yet.
        pass

    def _populate_stage_tree(self) -> None:
        tree = self.stage_tree
        tree.delete(*tree.get_children())
        if not self.stages:
            tree.insert(
                "",
                tk.END,
                text="(no stages.json — run full extract)",
                values=("",),
            )
            # Fall back: list stage*/ sidecar folders if present
            for stage_dir in sorted(self.assets_root.glob("stage*")):
                if stage_dir.is_dir() and stage_dir.name.startswith("stage"):
                    tree.insert(
                        "",
                        tk.END,
                        text=f"{stage_dir.name}/  [sidecars only]",
                        values=("sidecar",),
                    )
            return

        filt = self._filter_var.get().strip().lower()

        for stage_name in sorted(self.stages.keys()):
            stage = self.stages[stage_name]
            if not isinstance(stage, dict):
                continue
            stage_id = f"stage:{stage_name}"
            stage_node = tree.insert(
                "", tk.END, iid=stage_id, text=stage_name, values=("",), open=False
            )

            if "files" in stage and isinstance(stage["files"], dict):
                self._add_files_to_stage_tree(
                    tree, stage_node, stage["files"], stage_name, None, filt
                )
            if "folders" in stage and isinstance(stage["folders"], dict):
                for folder_name, files in stage["folders"].items():
                    if not isinstance(files, dict):
                        continue
                    fid = f"folder:{stage_name}/{folder_name}"
                    folder_node = tree.insert(
                        stage_node,
                        tk.END,
                        iid=fid,
                        text=str(folder_name),
                        values=("folder",),
                        open=False,
                    )
                    self._add_files_to_stage_tree(
                        tree,
                        folder_node,
                        files,
                        stage_name,
                        str(folder_name),
                        filt,
                    )

    def _add_files_to_stage_tree(
        self,
        tree: ttk.Treeview,
        parent: str,
        files: dict[str, Any],
        stage_name: str,
        folder: str | None,
        filt: str,
    ) -> None:
        for file_name, chunks in files.items():
            if not isinstance(chunks, dict):
                continue
            prefix = f"{stage_name}/{folder + '/' if folder else ''}{file_name}"
            if filt and filt not in prefix.lower() and not any(
                filt in str(k).lower() or filt in str(v.get("path", "")).lower()
                for k, v in chunks.items()
                if isinstance(v, dict)
            ):
                continue
            file_id = f"file:{prefix}"
            file_node = tree.insert(
                parent,
                tk.END,
                iid=file_id,
                text=str(file_name),
                values=("file",),
                open=False,
            )
            for chunk_key, ent in chunks.items():
                if not isinstance(ent, dict):
                    continue
                rel = ent.get("path") or ""
                path = _resolve_on_disk(self.assets_root, rel, ent.get("type"))
                iid = f"chunk:{prefix}/{chunk_key}"
                item = None
                if path is not None and path.exists():
                    # Sibling CLUTs from this same stage file
                    sibs: list[Path] = []
                    if ent.get("type") == "image":
                        for okey, oent in chunks.items():
                            if (
                                isinstance(oent, dict)
                                and oent.get("type") == "clut"
                            ):
                                cp = _resolve_on_disk(
                                    self.assets_root,
                                    oent.get("path") or "",
                                    "clut",
                                )
                                if cp is not None:
                                    cp = prefer_raw_blob(self.assets_root, cp)
                                if (
                                    cp is not None
                                    and cp.exists()
                                    and cp.suffix.lower() == ".pe2clut"
                                ):
                                    sibs.append(cp)
                    item = AssetItem(
                        path=path,
                        label=f"{chunk_key} → {rel}",
                        kind=str(ent.get("type") or ""),
                        source="stage",
                        meta=ent,
                        sibling_cluts=sibs or None,
                    )
                    self._item_map[iid] = item
                info = ent.get("type") or ""
                if ent.get("load_addr"):
                    info = f"{info}  {ent['load_addr']}"
                tree.insert(
                    file_node,
                    tk.END,
                    iid=iid,
                    text=str(chunk_key) + ("" if item else "  (missing)"),
                    values=(info,),
                )

    def _apply_filter(self) -> None:
        # Rebuild trees with filter (simple approach)
        self._populate_type_tree()
        self._populate_stage_tree()

    # -------------------------------------------------------------- selection
    def _on_select(self, tree: ttk.Treeview) -> None:
        sel = tree.selection()
        if not sel:
            return
        iid = sel[0]
        item = self._item_map.get(iid)
        if item is None:
            self._clear_preview(tree.item(iid, "text"))
            return
        self._show_item(item)

    def _clear_preview(self, msg: str = "") -> None:
        self._current_item = None
        self._current_pil = None
        self._photo_ref = None
        self._pe2img_path = None
        self._pe2img_data = None
        self._clut_choices = []
        self._set_clut_controls_enabled(False)
        self.img_canvas.delete("all")
        self._clear_text_tabs(msg)
        self.info_label.configure(text="")

    def _set_clut_controls_enabled(self, enabled: bool) -> None:
        if enabled:
            self._clut_combo.configure(state="readonly")
            self._clut_apply_cb.configure(state=tk.NORMAL)
        else:
            self._clut_combo.configure(state="disabled", values=[])
            self._clut_var.set("(none)")
            self._clut_apply_cb.configure(state=tk.DISABLED)
            self._clut_choices = []
            self._current_clut_colors = None

    def _show_item(self, item: AssetItem) -> None:
        self._current_item = item
        self._current_clut_colors = None
        path = item.path
        try:
            data = path.read_bytes()
        except OSError as e:
            self._clear_preview(f"Cannot read {path}: {e}")
            return

        size = _fmt_size(len(data))
        rel = path
        try:
            rel = path.relative_to(self.assets_root)
        except ValueError:
            pass
        meta_bits = [f"{rel}", size, item.source]
        if item.meta:
            if item.meta.get("chunk_size"):
                meta_bits.append(f"chunk_size={item.meta['chunk_size']}")
            if item.meta.get("load_addr"):
                meta_bits.append(f"load={item.meta['load_addr']}")
        self.info_label.configure(text="  ·  ".join(meta_bits))
        self._status_var.set(str(path))

        suf = path.suffix.lower()
        kind = item.kind

        # --- Fast paths first (images) — avoid heavy pairing until needed ---
        pe2img = pe2img_path_for_preview(self.assets_root, path)
        if pe2img is not None or suf == ".pe2img" or kind == "image":
            src = pe2img or path
            try:
                img_data = data if src == path else src.read_bytes()
            except OSError as e:
                self._clear_preview(f"Cannot read {src}: {e}")
                return
            raw_img = (
                src
                if src.suffix.lower() == ".pe2img"
                else self.assets_root / "raw" / "pe2img" / f"{src.stem}.pe2img"
            )
            if not item.sibling_cluts:
                item = AssetItem(
                    path=item.path,
                    label=item.label,
                    kind=item.kind,
                    source=item.source,
                    meta=item.meta,
                    sibling_cluts=self._lookup_cluts(src) or None,
                )
                self._current_item = item
            if self._show_pe2img(src, img_data, item=item):
                self.preview_nb.select(0)
                # Lightweight text tabs (short hex only — selection stays snappy)
                raw_bytes = img_data
                if raw_img.exists() and raw_img != src:
                    try:
                        raw_bytes = raw_img.read_bytes()
                    except OSError:
                        pass
                self._fill_binary_pair(
                    defl_data=img_data,
                    raw_data=raw_bytes,
                    defl_path=src,
                    raw_path=raw_img if raw_img.exists() else src,
                    image_note=True,
                    defl_label="pe2img clean payload",
                    short=True,
                )
                return

        if suf == ".pe2clut" or kind == "clut":
            self._set_clut_controls_enabled(False)
            if self._show_pe2clut(path, data):
                self.preview_nb.select(0)
                self._fill_binary_pair(
                    defl_data=data,
                    raw_data=data,
                    defl_path=path,
                    raw_path=path,
                    image_note=True,
                    defl_label="pe2clut payload",
                    short=True,
                )
                return

        if suf == ".png" and kind not in ("image",):
            self._set_clut_controls_enabled(False)
            if self._show_png(path):
                self.preview_nb.select(0)
                self._fill_binary_pair(
                    defl_data=data,
                    raw_data=data,
                    defl_path=path,
                    raw_path=path,
                    image_note=True,
                    defl_label="PNG",
                    short=True,
                )
                return

        # Pair raw/deflated only for non-image types (can read a second file)
        raw_path, raw_data, defl_path, defl_data = self._pair_raw_deflated(
            path, data, kind=kind
        )

        if suf == ".txt" or kind in ("txt", "ascii"):
            self._fill_ascii_tabs(raw_data, defl_data, raw_path, defl_path)
            self.preview_nb.select(1)  # Txt / hex
            self._set_clut_controls_enabled(False)
            return

        if suf in TEXT_EXTS:
            self._fill_text_tabs(raw_data, defl_data, raw_path, defl_path)
            self.preview_nb.select(1)
            self._set_clut_controls_enabled(False)
            return

        self._set_clut_controls_enabled(False)
        if suf == ".pe2pkg" or kind in ("pe2pkg", "room_pkg"):
            self._fill_pe2pkg_tabs(raw_path, raw_data, defl_path, defl_data)
            self.preview_nb.select(1)
            return

        self._fill_binary_pair(
            defl_data=defl_data,
            raw_data=raw_data,
            defl_path=defl_path,
            raw_path=raw_path,
            short=True,
        )
        self.preview_nb.select(1)

    def _pair_raw_deflated(
        self, path: Path, data: bytes, *, kind: str
    ) -> tuple[Path, bytes, Path, bytes]:
        """Return (raw_path, raw_bytes, deflated_path, deflated_bytes)."""
        # Default: selected file is both
        raw_path, raw_data = path, data
        defl_path, defl_data = path, data

        # If under inflated type dir, pair with raw/
        try:
            rel = path.relative_to(self.assets_root)
        except ValueError:
            return raw_path, raw_data, defl_path, defl_data

        parts = rel.parts
        if parts and parts[0] == "raw" and len(parts) >= 3:
            # Viewing raw — try inflate counterpart
            type_dir, name = parts[1], parts[-1]
            infl = self.assets_root / type_dir / name
            # png for images
            if not infl.exists() and Path(name).suffix in (".pe2img", ".pe2clut"):
                infl = self.assets_root / type_dir / f"{Path(name).stem}.png"
            if not infl.exists() and Path(name).suffix == ".pe2pkg":
                # inflated pe2pkg keeps same name
                infl = self.assets_root / type_dir / name
            raw_path, raw_data = path, data
            if infl.exists():
                try:
                    defl_path, defl_data = infl, infl.read_bytes()
                except OSError:
                    defl_path, defl_data = path, data
            elif Path(name).suffix == ".pe2pkg":
                try:
                    from asset_decode import decode_pe2pkg_payload

                    defl_data = decode_pe2pkg_payload(data)
                    defl_path = path  # virtual
                except Exception:
                    defl_data = data
            elif Path(name).suffix == ".txt":
                try:
                    from asset_decode import decode_ascii_payload

                    defl_data = decode_ascii_payload(data)
                except Exception:
                    end = len(data)
                    while end > 0 and data[end - 1] == 0:
                        end -= 1
                    defl_data = data[:end]
            else:
                defl_path, defl_data = path, data
            return raw_path, raw_data, defl_path, defl_data

        if parts and parts[0] in TYPE_DIRS:
            # Viewing inflated — find raw
            type_dir = parts[0]
            stem = path.stem
            ext_candidates = [
                path.suffix,
                ".pe2img",
                ".pe2clut",
                ".pe2pkg",
                ".txt",
                ".spk",
                ".bs",
                ".pe2cap2",
            ]
            defl_path, defl_data = path, data
            for ext in ext_candidates:
                cand = self.assets_root / "raw" / type_dir / f"{stem}{ext}"
                if cand.exists():
                    try:
                        raw_path, raw_data = cand, cand.read_bytes()
                    except OSError:
                        pass
                    break
            # txt inflated is already stripped; raw has pad
            if path.suffix == ".txt" or kind in ("txt", "ascii"):
                try:
                    from asset_decode import decode_ascii_payload

                    defl_data = decode_ascii_payload(
                        raw_data if raw_path != path else data
                    )
                except Exception:
                    defl_data = data
            return raw_path, raw_data, defl_path, defl_data

        return raw_path, raw_data, defl_path, defl_data

    def _fill_ascii_tabs(
        self,
        raw_data: bytes,
        defl_data: bytes,
        raw_path: Path,
        defl_path: Path,
    ) -> None:
        try:
            from asset_decode import decode_ascii_payload

            body = decode_ascii_payload(raw_data)
        except Exception:
            body = defl_data
            end = len(body)
            while end > 0 and body[end - 1] == 0:
                end -= 1
            body = body[:end]
        try:
            text = body.decode("utf-8")
        except UnicodeDecodeError:
            text = body.decode("latin-1", errors="replace")
        if len(text) > PREVIEW_TEXT_CHARS:
            text = text[:PREVIEW_TEXT_CHARS] + "\n… (truncated)"
        stripped = len(raw_data) - len(body)
        header = (
            f"# deflated (inflated form)  {defl_path.name}\n"
            f"# {len(body)} bytes text"
            + (f", stripped {stripped} trailing zeros from raw\n\n" if stripped else "\n\n")
        )
        self._set_text_widget(self.text_deflated, header + text)

        raw_header = (
            f"# raw on-disc clean payload  {raw_path.name}\n"
            f"# size {len(raw_data)} ({_fmt_size(len(raw_data))})\n\n"
        )
        self._set_text_widget(
            self.text_raw,
            raw_header + _hexdump(raw_data, min(len(raw_data), PREVIEW_HEX_BYTES * 4)),
        )

    def _fill_text_tabs(
        self,
        raw_data: bytes,
        defl_data: bytes,
        raw_path: Path,
        defl_path: Path,
    ) -> None:
        def as_text(b: bytes) -> str:
            try:
                t = b.decode("utf-8")
            except UnicodeDecodeError:
                t = b.decode("latin-1", errors="replace")
            if len(t) > PREVIEW_TEXT_CHARS:
                t = t[:PREVIEW_TEXT_CHARS] + "\n… (truncated)"
            return t

        self._set_text_widget(
            self.text_deflated,
            f"# {defl_path}\n# {_fmt_size(len(defl_data))}\n\n{as_text(defl_data)}",
        )
        self._set_text_widget(
            self.text_raw,
            f"# {raw_path}\n# {_fmt_size(len(raw_data))}\n\n{as_text(raw_data)}",
        )

    def _fill_pe2pkg_tabs(
        self,
        raw_path: Path,
        raw_data: bytes,
        defl_path: Path,
        defl_data: bytes,
    ) -> None:
        # If defl still looks compressed (same as raw), try decode
        body = defl_data
        if defl_path == raw_path or defl_data == raw_data:
            try:
                from asset_decode import decode_pe2pkg_payload

                body = decode_pe2pkg_payload(raw_data)
            except Exception:
                body = defl_data
        defl_txt = (
            f"# deflated (LZSS-decoded)  {defl_path.name}\n"
            f"# size {len(body)} ({_fmt_size(len(body))})\n\n"
            f"strings:\n"
            + "\n".join(f"  {s[:120]}" for s in _printable_strings(body)[:40])
            + "\n\nhex:\n"
            + _hexdump(body)
        )
        raw_txt = (
            f"# raw LZSS stream  {raw_path.name}\n"
            f"# size {len(raw_data)} ({_fmt_size(len(raw_data))})\n\n"
            f"hex:\n"
            + _hexdump(raw_data)
        )
        self._set_text_widget(self.text_deflated, defl_txt)
        self._set_text_widget(self.text_raw, raw_txt)

    def _fill_binary_pair(
        self,
        *,
        defl_data: bytes,
        raw_data: bytes,
        defl_path: Path,
        raw_path: Path,
        image_note: bool = False,
        defl_label: str = "txt / hex (decoded view)",
        short: bool = False,
    ) -> None:
        hex_limit = 256 if short else PREVIEW_HEX_BYTES
        str_limit = 12 if short else 30

        def summary(data: bytes, path: Path, label: str) -> str:
            lines = [
                f"# {label}",
                f"# path: {path}",
                f"# size: {len(data)} ({_fmt_size(len(data))})",
            ]
            if image_note:
                lines.append("# (see Image tab for raster preview)")
            lines.append("")
            if not short:
                strings = _printable_strings(data, limit=str_limit)
                if strings:
                    lines.append("strings:")
                    for s in strings:
                        lines.append(f"  {s[:120]}")
                    lines.append("")
            lines.append("hex:")
            lines.append(_hexdump(data, hex_limit))
            return "\n".join(lines)

        self._set_text_widget(
            self.text_deflated, summary(defl_data, defl_path, defl_label)
        )
        self._set_text_widget(
            self.text_raw, summary(raw_data, raw_path, "raw on-disc")
        )

    def _show_png(self, path: Path) -> bool:
        try:
            from PIL import Image
        except ImportError:
            self._show_text_message("Pillow not installed — cannot preview PNG")
            return False
        try:
            img = Image.open(path)
            img.load()
        except Exception as e:
            self._show_text_message(f"PNG open failed: {e}")
            return False
        self._pe2img_path = None
        self._pe2img_data = None
        self._set_pil_image(img)
        return True

    def _sibling_cluts_for(self, pe2img_path: Path, item: AssetItem | None) -> list[Path]:
        raw: list[Path] = []
        if item is not None and item.sibling_cluts:
            raw = list(item.sibling_cluts)
        if not raw:
            raw = self._lookup_cluts(pe2img_path)
        # Always normalise to raw .pe2clut blobs (never inflated PNG strips).
        out: list[Path] = []
        seen: set[Path] = set()
        for p in raw:
            p = prefer_raw_blob(self.assets_root, p)
            if p.suffix.lower() != ".pe2clut" or not p.exists():
                continue
            rp = p.resolve()
            if rp not in seen:
                seen.add(rp)
                out.append(rp)
        return out

    def _refresh_clut_list(self) -> None:
        """Rebuild CLUT combobox for the current pe2img."""
        if self._pe2img_path is None:
            self._set_clut_controls_enabled(False)
            return
        item = self._current_item
        choices: list[tuple[str, Path | None]] = [("(none — index greyscale)", None)]
        if self._clut_scope.get() == "all":
            for p in list_all_cluts(self.assets_root):
                try:
                    label = str(p.relative_to(self.assets_root))
                except ValueError:
                    label = p.name
                choices.append((label, p))
        else:
            sibs = self._sibling_cluts_for(self._pe2img_path, item)
            if not sibs:
                choices.append(
                    ("(no stage siblings — try All CLUTs)", None)
                )
            for p in sibs:
                try:
                    label = str(p.relative_to(self.assets_root))
                except ValueError:
                    label = p.name
                choices.append((f"{label}  [sibling]", p))
        self._clut_choices = choices
        labels = [c[0] for c in choices]
        # Prefer first real CLUT when applying by default
        prefer = None
        for lab, p in choices:
            if p is not None:
                prefer = lab
                break
        self._clut_combo.configure(values=labels)
        if prefer is not None and self._clut_apply.get():
            self._clut_var.set(prefer)
        elif self._clut_var.get() not in labels:
            self._clut_var.set(labels[0])
        self._clut_combo.configure(state="readonly")
        self._clut_apply_cb.configure(state=tk.NORMAL)

    def _selected_clut_path(self) -> Path | None:
        label = self._clut_var.get()
        for lab, path in self._clut_choices:
            if lab == label:
                return path
        return None

    def _get_clut_colors(self, clut_path: Path, bpp: int) -> list[int] | None:
        clut_path = prefer_raw_blob(self.assets_root, clut_path)
        if clut_path.suffix.lower() != ".pe2clut":
            return None
        cache_key = f"{clut_path.resolve()}|{bpp}"
        if cache_key in self._clut_color_cache:
            return self._clut_color_cache[cache_key]
        try:
            from asset_decode import load_clut_palette

            colors = load_clut_palette(clut_path, bpp=bpp)
        except Exception:
            colors = None
        self._clut_color_cache[cache_key] = colors
        return colors

    def _on_clut_scope_changed(self) -> None:
        self._refresh_clut_list()
        self._on_clut_changed()

    def _on_clut_changed(self) -> None:
        if self._pe2img_path is None or self._pe2img_data is None:
            return
        self._decode_pe2img_to_canvas(
            self._pe2img_path,
            self._pe2img_data,
            apply_clut=bool(self._clut_apply.get()),
            clut_path=self._selected_clut_path(),
        )

    def _show_pe2img(
        self, path: Path, data: bytes, *, item: AssetItem
    ) -> bool:
        self._pe2img_path = path
        self._pe2img_data = data
        self._clut_apply.set(True)
        self._refresh_clut_list()
        clut_path = self._selected_clut_path()
        return self._decode_pe2img_to_canvas(
            path,
            data,
            apply_clut=True,
            clut_path=clut_path,
        )

    def _decode_pe2img_to_canvas(
        self,
        path: Path,
        data: bytes,
        *,
        apply_clut: bool,
        clut_path: Path | None,
    ) -> bool:
        """Preview pe2img via shared :func:`asset_decode.render_pe2img`."""
        try:
            from asset_decode import render_pe2img
        except ImportError as e:
            self._show_text_message(f"Image decode unavailable: {e}")
            return False
        try:
            clut = prefer_raw_blob(self.assets_root, clut_path) if clut_path else None
            if clut is not None and clut.suffix.lower() != ".pe2clut":
                clut = None
            img, info, colors = render_pe2img(
                data,
                apply_clut=bool(apply_clut and clut is not None),
                clut_path=clut if apply_clut else None,
            )
            self._current_clut_colors = colors
            if apply_clut and clut is not None and colors is not None:
                note = (
                    f"cols={len(info.entries)}  h={info.height}  "
                    f"clut={clut.name}  {len(colors)} entries  "
                    f"render_bpp={info.bpp}"
                )
            elif apply_clut and clut is not None:
                note = (
                    f"bpp≈{info.bpp}  cols={len(info.entries)}  "
                    f"h={info.height}  clut load failed: {clut.name}"
                )
            else:
                note = (
                    f"bpp≈{info.bpp}  cols={len(info.entries)}  "
                    f"h={info.height}  clut=off"
                )
        except Exception as e:
            self._show_text_message(f"pe2img decode failed:\n{e}")
            return False

        self._set_pil_image(img)
        self._status_var.set(f"{path}  ·  {note}")
        return True

    def _show_pe2clut(self, path: Path, data: bytes) -> bool:
        try:
            from asset_decode import load_clut_palette, render_pe2clut
            from image_codec import _load_clut_blob
        except ImportError as e:
            self._show_text_message(f"Image decode unavailable: {e}")
            return False
        try:
            path = prefer_raw_blob(self.assets_root, path)
            data = path.read_bytes()
            img, info = render_pe2clut(data)
            blob = _load_clut_blob(path)
            self._current_clut_colors = blob[4] if blob else load_clut_palette(path)
        except Exception as e:
            self._show_text_message(f"pe2clut decode failed:\n{e}")
            return False
        self._pe2img_path = None
        self._pe2img_data = None
        self._set_pil_image(img)
        return True

    def _set_pil_image(self, img: Any) -> None:
        self._current_pil = img
        self._draw_image()

    def _on_canvas_resize(self, _event: tk.Event) -> None:
        if self._current_pil is not None:
            self._draw_image()

    def _checkerboard(self, w: int, h: int, cell: int = 8) -> Any:
        """Light/dark checkerboard for transparent pixel visibility."""
        from PIL import Image

        img = Image.new("RGB", (max(w, 1), max(h, 1)))
        px = img.load()
        c0, c1 = (60, 60, 60), (90, 90, 90)
        for y in range(h):
            for x in range(w):
                px[x, y] = c0 if ((x // cell) ^ (y // cell)) & 1 == 0 else c1
        return img

    def _draw_image(self) -> None:
        img = self._current_pil
        if img is None:
            return
        try:
            from PIL import Image, ImageTk
            from image_codec import _abgr1555_to_rgba
        except ImportError:
            return

        self.img_canvas.update_idletasks()
        cw = max(self.img_canvas.winfo_width(), 64)
        ch = max(self.img_canvas.winfo_height(), 64)
        palette_h = 28 if self._current_clut_colors else 0
        iw, ih = img.size
        avail_h = max(ch - palette_h - 8, 32)
        scale = min(cw / iw, avail_h / ih, 4.0)
        if scale >= 1:
            scale = max(1, int(scale))
        nw, nh = max(1, int(iw * scale)), max(1, int(ih * scale))
        resample = Image.Resampling.NEAREST if scale >= 1 else Image.Resampling.BILINEAR
        # Composite onto checkerboard so alpha is visible
        rgba = img.convert("RGBA")
        disp = rgba.resize((nw, nh), resample)
        board = self._checkerboard(nw, nh, cell=max(4, int(8 * min(scale, 2))))
        board = board.convert("RGBA")
        composed = Image.alpha_composite(board, disp)
        photo = ImageTk.PhotoImage(composed)
        self._photo_ref = photo
        self.img_canvas.delete("all")
        # Fill canvas with checkerboard too (letterboxing)
        full_board = self._checkerboard(cw, max(ch - palette_h, 1), cell=10)
        self._bg_photo = ImageTk.PhotoImage(full_board)
        self.img_canvas.create_image(0, 0, image=self._bg_photo, anchor=tk.NW)
        img_cy = (ch - palette_h) // 2
        self.img_canvas.create_image(cw // 2, img_cy, image=photo, anchor=tk.CENTER)
        self.img_canvas.create_text(
            8,
            8,
            anchor=tk.NW,
            text=f"{iw}×{ih}  ×{scale:g}",
            fill="#ddd",
            font=("", 10),
        )
        # CLUT swatch strip along the bottom
        if self._current_clut_colors:
            n = min(len(self._current_clut_colors), 256)
            sw = Image.new("RGBA", (max(n, 1), 1), (0, 0, 0, 255))
            px = sw.load()
            for i in range(n):
                px[i, 0] = _abgr1555_to_rgba(self._current_clut_colors[i])
            strip = sw.resize(
                (max(cw - 16, 8), palette_h - 6), Image.Resampling.NEAREST
            )
            self._palette_photo = ImageTk.PhotoImage(strip)
            self.img_canvas.create_image(
                cw // 2, ch - palette_h // 2, image=self._palette_photo, anchor=tk.CENTER
            )
            self.img_canvas.create_text(
                8,
                ch - palette_h + 2,
                anchor=tk.NW,
                text=f"CLUT  {n} colours",
                fill="#ccc",
                font=("", 9),
            )
        else:
            self._palette_photo = None

    def _show_text_message(self, msg: str) -> None:
        self._set_text_widget(self.text_deflated, msg)
        self._set_text_widget(self.text_raw, "")
        self.preview_nb.select(1)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "assets",
        type=Path,
        nargs="?",
        default=Path("assets/USA"),
        help="Assets root (default: assets/USA)",
    )
    args = parser.parse_args(argv)
    root = args.assets
    if not root.exists():
        # try relative to repo root
        repo = Path(__file__).resolve().parents[2]
        alt = repo / root
        if alt.exists():
            root = alt
        else:
            print(f"assets root not found: {args.assets}", file=sys.stderr)
            return 1

    app = AssetViewer(root)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
