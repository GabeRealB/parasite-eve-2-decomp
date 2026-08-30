"""A small software-rendered mesh viewport for the asset viewer.

Tk has no 3D, and the models are tiny by modern standards - a few hundred
vertices - so the whole renderer is a projection plus a painter's-algorithm
sort onto a ``tk.Canvas``. That keeps the viewer dependency-free: the meshes
are here to *identify* a character, not to look good.

Projection is orthographic on purpose. These are low-poly PlayStation models
seen from arbitrary angles, and perspective mostly adds foreshortening that
makes silhouettes harder to compare between two candidate actors.

Vertices arrive already Y-flipped (see ``pkg_overlay.decode_model``), so the
viewport shares the OBJ export's orientation and a model that looks upright
here looks upright in Blender.
"""

from __future__ import annotations

import math
import tkinter as tk
from tkinter import ttk

BG = "#23262b"
WIRE = "#7fd1ff"
SOFT = "#5a6675"  # muted edge for the solid+wire overlay
EDGE = "#1b1e22"
LIGHT = (0.35, 0.45, 0.82)  # view-space direction, normalised below


def _norm(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / n for c in v)


class MeshView(ttk.Frame):
    """Rotate/zoom/pan viewport for one indexed mesh."""

    def __init__(self, parent: tk.Misc) -> None:
        super().__init__(parent)
        self._verts: list[tuple[float, float, float]] = []
        self._faces: list[tuple[int, ...]] = []
        self._normals: list[tuple[float, float, float]] = []
        self._parts: list[int] = []
        self._yaw = 0.55
        self._pitch = 0.25
        self._zoom = 1.0
        self._pan = [0.0, 0.0]
        self._drag: tuple[str, int, int] | None = None
        self._fit: tuple[float, float, float, float] | None = None
        self._anim_sampler = None      # (anim_index, frame) -> (verts, normals)
        self._anim_frames: list[int] = []
        self._frame = 0.0
        self._playing = False
        self._play_after: str | None = None
        self._redraw_after: str | None = None
        self._light = _norm(LIGHT)

        bar = ttk.Frame(self)
        bar.pack(side=tk.TOP, fill=tk.X, pady=(0, 4))
        self._mode = tk.StringVar(value="Solid")
        ttk.Label(bar, text="Shading:").pack(side=tk.LEFT)
        combo = ttk.Combobox(
            bar,
            textvariable=self._mode,
            values=("Solid + wire", "Solid", "Wireframe", "Points"),
            state="readonly",
            width=14,
        )
        combo.pack(side=tk.LEFT, padx=(4, 10))
        combo.bind("<<ComboboxSelected>>", lambda _e: self._draw())
        self._cull = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            bar, text="Backface cull", variable=self._cull, command=self._draw
        ).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(bar, text="Reset view", command=self.reset_view).pack(side=tk.LEFT)
        ttk.Label(bar, text="  Part:").pack(side=tk.LEFT)
        self._part = tk.StringVar(value="All")
        self._part_combo = ttk.Combobox(
            bar, textvariable=self._part, state="readonly", width=14, values=("All",)
        )
        self._part_combo.pack(side=tk.LEFT, padx=(4, 0))
        self._part_combo.bind("<<ComboboxSelected>>", lambda _e: self._draw())
        self._info = ttk.Label(bar, text="", foreground="#888")
        self._info.pack(side=tk.RIGHT)

        # Animation transport. Hidden until a model actually has animations.
        self.anim_bar = ttk.Frame(self)
        ttk.Label(self.anim_bar, text="Animation:").pack(side=tk.LEFT)
        self._anim = tk.StringVar(value="Rest pose")
        self._anim_combo = ttk.Combobox(
            self.anim_bar, textvariable=self._anim, state="readonly", width=22,
            values=("Rest pose",),
        )
        self._anim_combo.pack(side=tk.LEFT, padx=(4, 8))
        self._anim_combo.bind("<<ComboboxSelected>>", lambda _e: self._on_anim_changed())
        self._play_btn = ttk.Button(self.anim_bar, text="\u25b6 Play", width=9, command=self._toggle_play)
        self._play_btn.pack(side=tk.LEFT, padx=(0, 8))
        self._frame_var = tk.DoubleVar(value=0.0)
        self._frame_scale = ttk.Scale(
            self.anim_bar, from_=0, to=1, orient=tk.HORIZONTAL,
            variable=self._frame_var, command=self._on_frame_drag,
        )
        self._frame_scale.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self._frame_lbl = ttk.Label(self.anim_bar, text="0", width=10, foreground="#888")
        self._frame_lbl.pack(side=tk.LEFT, padx=(6, 0))

        self.canvas = tk.Canvas(self, bg=BG, highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda _e: self._draw())
        self.canvas.bind("<ButtonPress-1>", lambda e: self._press("rot", e))
        self.canvas.bind("<B1-Motion>", self._motion)
        self.canvas.bind("<ButtonRelease-1>", lambda _e: self._release())
        self.canvas.bind("<ButtonPress-3>", lambda e: self._press("pan", e))
        self.canvas.bind("<B3-Motion>", self._motion)
        self.canvas.bind("<ButtonRelease-3>", lambda _e: self._release())
        self.canvas.bind("<Shift-ButtonPress-1>", lambda e: self._press("pan", e))
        # Wheel: Windows/macOS send <MouseWheel>, X11 sends buttons 4 and 5.
        self.canvas.bind("<MouseWheel>", self._wheel)
        self.canvas.bind("<Button-4>", lambda _e: self._scale_zoom(1.1))
        self.canvas.bind("<Button-5>", lambda _e: self._scale_zoom(1 / 1.1))

    # ------------------------------------------------------------------ data
    def show(self, verts, faces, normals=None, parts=None, note: str = "") -> None:
        """Load a mesh, normalised into a unit box centred on its bounds.

        ``normals`` is one outward direction per face, in the same space as
        ``verts``. It must come from the model's own normal array rather than
        from the winding - see ``pkg_overlay.Mesh``. Without it every face
        renders inside out.
        """
        keep = [i for i, f in enumerate(faces) if len(f) >= 3]
        self._faces = [faces[i] for i in keep]
        self._normals = [normals[i] for i in keep] if normals else []
        self._parts = [parts[i] for i in keep] if parts else []
        # One entry per part that actually carries geometry, so a single limb
        # can be inspected on its own.
        if verts:
            xs = [v[0] for v in verts]
            ys = [v[1] for v in verts]
            zs = [v[2] for v in verts]
            extent = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)) or 1.0
            self._fit = (
                (min(xs) + max(xs)) / 2,
                (min(ys) + max(ys)) / 2,
                (min(zs) + max(zs)) / 2,
                extent,
            )
        present = sorted(set(self._parts))
        self._part_combo.configure(values=["All"] + [f"part {i}" for i in present])
        if self._part.get() not in self._part_combo.cget("values"):
            self._part.set("All")
        self._verts = self._normalise(verts)
        self._info.configure(text=note)
        self._draw()

    def _normalise(self, verts):
        """Scale into a unit box using the fit taken from the first pose.

        Re-fitting every animation frame would make the model swim about as its
        bounds change; the rest pose sets the frame once.
        """
        if not verts or self._fit is None:
            return []
        cx, cy, cz, extent = self._fit
        s = 1.0 / extent
        return [((v[0] - cx) * s, (v[1] - cy) * s, (v[2] - cz) * s) for v in verts]

    # ------------------------------------------------------------- animation
    def set_animations(self, labels: list[str], frames: list[int], sampler) -> None:
        """Offer these animations; ``sampler(index, frame) -> (verts, normals)``."""
        self._anim_sampler = sampler
        self._anim_frames = list(frames)
        self._stop_play()
        self._anim.set("Rest pose")
        self._anim_combo.configure(values=["Rest pose"] + list(labels))
        if labels:
            self.anim_bar.pack(side=tk.BOTTOM, fill=tk.X, pady=(4, 0))
        else:
            self.anim_bar.pack_forget()
        self._frame = 0.0
        self._frame_var.set(0)
        self._frame_lbl.configure(text="rest")

    def _anim_index(self) -> int | None:
        sel = self._anim.get()
        values = list(self._anim_combo.cget("values"))
        if sel == "Rest pose" or sel not in values:
            return None
        return values.index(sel) - 1

    def _on_anim_changed(self) -> None:
        idx = self._anim_index()
        self._frame = 0.0
        self._frame_var.set(0)
        if idx is None:
            self._stop_play()
            self._frame_scale.configure(to=1)
        else:
            self._frame_scale.configure(to=max(1, self._anim_frames[idx] - 1))
        self._apply_frame()

    def _on_frame_drag(self, _v=None) -> None:
        if self._playing:
            return
        self._frame = float(self._frame_var.get())
        self._apply_frame()

    def _apply_frame(self) -> None:
        idx = self._anim_index()
        if self._anim_sampler is None:
            return
        got = self._anim_sampler(idx, self._frame)
        if got is None:
            return
        verts, normals = got
        self._verts = self._normalise(verts)
        if normals:
            self._normals = normals
        self._frame_lbl.configure(
            text="rest" if idx is None else f"{self._frame:.0f}/{self._anim_frames[idx]}"
        )
        self._draw()

    def _toggle_play(self) -> None:
        if self._playing:
            self._stop_play()
        elif self._anim_index() is not None:
            self._playing = True
            self._play_btn.configure(text="\u25a0 Pause")
            self._tick()

    def _stop_play(self) -> None:
        self._playing = False
        if self._play_after is not None:
            self.after_cancel(self._play_after)
            self._play_after = None
        self._play_btn.configure(text="\u25b6 Play")

    def _tick(self) -> None:
        if not self._playing:
            return
        idx = self._anim_index()
        if idx is None:
            self._stop_play()
            return
        total = max(1, self._anim_frames[idx])
        # One record tick per 16 ms is the game's 60 Hz; interpolation inside a
        # record does the smoothing, so no sub-stepping is needed.
        self._frame = (self._frame + 1.0) % total
        self._frame_var.set(self._frame)
        self._apply_frame()
        # The game runs these at 60 Hz; a record's duration is in those ticks.
        self._play_after = self.after(16, self._tick)

    def clear(self, msg: str = "") -> None:
        self._stop_play()
        self._verts = []
        self._faces = []
        self._normals = []
        self._parts = []
        self._info.configure(text="")
        self.canvas.delete("all")
        if msg:
            self._center_text(msg)

    def reset_view(self) -> None:
        self._yaw, self._pitch, self._zoom = 0.55, 0.25, 1.0
        self._pan = [0.0, 0.0]
        self._draw()

    # ----------------------------------------------------------------- input
    def _press(self, mode: str, event: tk.Event) -> None:
        self._drag = (mode, event.x, event.y)

    def _release(self) -> None:
        self._drag = None

    def _motion(self, event: tk.Event) -> None:
        if not self._drag:
            return
        mode, x0, y0 = self._drag
        dx, dy = event.x - x0, event.y - y0
        if mode == "rot":
            self._yaw += dx * 0.01
            self._pitch = max(-1.55, min(1.55, self._pitch + dy * 0.01))
        else:
            self._pan[0] += dx
            self._pan[1] += dy
        self._drag = (mode, event.x, event.y)
        self._schedule()

    def _wheel(self, event: tk.Event) -> None:
        self._scale_zoom(1.1 if getattr(event, "delta", 0) > 0 else 1 / 1.1)

    def _scale_zoom(self, factor: float) -> None:
        self._zoom = max(0.05, min(40.0, self._zoom * factor))
        self._schedule()

    def _schedule(self) -> None:
        # Coalesce drag events: one redraw per idle beat keeps rotation smooth
        # on meshes where a full repaint costs more than a mouse-move interval.
        if self._redraw_after is None:
            self._redraw_after = self.after_idle(self._draw_now)

    def _draw_now(self) -> None:
        self._redraw_after = None
        self._draw()

    # ---------------------------------------------------------------- render
    def _center_text(self, msg: str) -> None:
        w = self.canvas.winfo_width() or 400
        h = self.canvas.winfo_height() or 300
        self.canvas.create_text(w / 2, h / 2, text=msg, fill="#888")

    def _selected_faces(self) -> list[int]:
        """Indices into self._faces for the current part selection."""
        sel = self._part.get()
        if not sel.startswith("part ") or not self._parts:
            return list(range(len(self._faces)))
        want = int(sel.split()[1])
        return [i for i, p in enumerate(self._parts) if p == want]

    def _draw(self) -> None:
        c = self.canvas
        c.delete("all")
        if not self._verts:
            self._center_text("No mesh")
            return
        w = c.winfo_width() or 400
        h = c.winfo_height() or 300
        scale = min(w, h) * 0.85 * self._zoom
        cx = w / 2 + self._pan[0]
        cy = h / 2 + self._pan[1]

        cy_, sy_ = math.cos(self._yaw), math.sin(self._yaw)
        cp, sp = math.cos(self._pitch), math.sin(self._pitch)
        pts = []
        for x, y, z in self._verts:
            # yaw about Y, then pitch about X
            xr = x * cy_ + z * sy_
            zr = -x * sy_ + z * cy_
            yr = y * cp - zr * sp
            zr = y * sp + zr * cp
            pts.append((cx + xr * scale, cy - yr * scale, zr))

        mode = self._mode.get()
        if mode == "Points":
            for px, py, _ in pts:
                c.create_rectangle(px - 1, py - 1, px + 1, py + 1, outline="", fill=WIRE)
            return

        if not self._faces:
            for px, py, _ in pts:
                c.create_rectangle(px - 1, py - 1, px + 1, py + 1, outline="", fill=WIRE)
            self._center_text("No faces decoded — showing vertices")
            return

        solid = mode in ("Solid", "Solid + wire")
        wire = mode in ("Wireframe", "Solid + wire")
        n = len(pts)

        # Rotate the face normals with the model. Deriving them from the
        # projected points instead would be wrong twice over: screen y grows
        # downward, which reflects the frame and flips every cross product,
        # and the winding is clockwise to begin with.
        def rot(v):
            x, y, z = v
            xr = x * cy_ + z * sy_
            zr = -x * sy_ + z * cy_
            yr = y * cp - zr * sp
            return xr, yr, y * sp + zr * cp

        order = []
        for k in self._selected_faces():
            f = self._faces[k]
            if any(i >= n for i in f):
                continue
            order.append((sum(pts[i][2] for i in f) / len(f), k, f))
        # Painter's algorithm, farthest first. The PlayStation's SZ3 grows with
        # distance and negating Y for a Y-up viewer does not touch Z, so +Z is
        # away here too and descending depth paints back to front.
        order.sort(key=lambda t: -t[0])

        for _d, k, f in order:
            flat = []
            for i in f:
                flat.extend(pts[i][:2])
            if solid:
                # Cull the way the game does. Tmd_StreamHandler_Op38 runs NCLIP
                # on the *projected* points and drops the primitive when MAC0
                # <= 0; it never consults a normal for visibility. The stored
                # normals feed NCCS, which is lighting only - using them to
                # decide facing drops real surface, which is what made heads
                # and legs disappear.
                (x0, y0, _), (x1, y1, _), (x2, y2, _) = pts[f[0]], pts[f[1]], pts[f[2]]
                mac0 = x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1)
                if self._cull.get() and mac0 <= 0:
                    continue
                nx, ny, nz = rot(self._normals[k]) if k < len(self._normals) else (0.0, 0.0, 1.0)
                ln = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
                lam = abs(
                    (nx * self._light[0] + ny * self._light[1] + nz * self._light[2]) / ln
                )
                v = int(60 + 185 * max(0.0, min(1.0, lam)))
                fill = f"#{v:02x}{v:02x}{min(255, v + 10):02x}"
                c.create_polygon(
                    *flat, fill=fill, outline=EDGE if not wire else SOFT, width=1
                )
            else:
                c.create_polygon(*flat, fill="", outline=WIRE, width=1)
