"""SPU-ADPCM sample bodies: Sony's encoder, and the search for input it turns into a given body.

    bodies = encode(sounds, modes, loops)    each sound 16-bit PCM, its length a multiple of 28; a mode name;
                                             a loop start (a frame index) or None for a one-shot
    pcm, mode, loop = decode(body)           input that `encode` turns into `body` exactly, and the mode it needs

The encoder is ENCVAG's `EncVag` (Psy-Q PSXSOUND/PC/ENCPC/LIBRARY/ENCVAG.DLL, and AIFF2VAG 3.2 and 3.3, which give
identical output), run over many sounds at once in numpy with its own float64 operation order. Per 28-sample frame:
the input clamped to -30720..30719; each of the five filters' residual peak over the frame, the filter the one with the
smallest weighted peak (the mode's weights) unless filter 0's own peak is 7 or less; the shift from the peak by
ENCVAG's bit test; then each sample quantized with error feedback, `(int)(s * 2^shift) + 0x800 & ~0xFFF`, saturating.

A body as the disc stores it: a zero start block; the frames; and after a one-shot, the end block `00 07 77..`. Flags:
a one-shot's first frame 4, its last 1; a loop's frames 2, the loop start 6, the last 3.
"""
from __future__ import annotations

import numpy as np

MODES = {"normal": (1.0, 1.0, 1.0, 1.0, 1.0), "high": (1.0, 1.0, 1000.0, 1000.0, 1.0),
         "low": (1.0, 1.0, 1000.0, 1.0, 1000.0), "4bit": (1.0, 1000.0, 1000.0, 1000.0, 1000.0)}
F0 = np.array([0.0, -60 / 64, -115 / 64, -98 / 64, -122 / 64])
F1 = np.array([0.0, 0.0, 52 / 64, 55 / 64, 60 / 64])
LO, HI = -30720, 30719
END_BLOCK = bytes([0x00, 0x07]) + bytes([0x77]) * 14


def _shift_table() -> np.ndarray:
    out = np.zeros(32768, dtype=np.int64)
    for m in range(32768):
        mask, shift = 0x4000, 0
        while not (mask & (m + (mask >> 3))):
            shift += 1; mask >>= 1
            if shift >= 12: break
        out[m] = shift
    return out


SHIFT = _shift_table()


def _frames(x, w, s1, s2, e1, e2):
    """One frame of N sounds: x (N, 28); w (N, 5) weights; states (N,), updated. -> (header, codes (N, 28))."""
    n = x.shape[0]
    xs = np.clip(x, LO, HI).astype(np.float64)
    a = np.concatenate([s1[:, None], xs[:, :-1]], axis=1)
    b = np.concatenate([s2[:, None], a[:, :-1]], axis=1)
    pk = np.empty((n, 5))
    for i in range(5):
        pk[:, i] = np.abs(xs + a * F0[i] + b * F1[i]).max(axis=1)
    best = np.full(n, 1.8e307); filt = np.zeros(n, dtype=np.int64); done = pk[:, 0] <= 7.0
    for i in range(5):
        take = (w[:, i] * pk[:, i] < best) & ~done
        best = np.where(take, w[:, i] * pk[:, i], best); filt = np.where(take, i, filt)
    m = np.clip(np.trunc(pk[np.arange(n), filt]), -32768, 32767).astype(np.int64)
    shift = SHIFT[np.maximum(m, 0)]
    f0, f1 = F0[filt], F1[filt]; scale = np.ldexp(1.0, shift)
    codes = np.empty((n, 28), dtype=np.int64)
    for k in range(28):
        d = xs[:, k] + a[:, k] * f0 + b[:, k] * f1
        s0 = d + e1 * f0 + e2 * f1
        di = np.clip((np.trunc(s0 * scale).astype(np.int64) + 0x800) & ~0xFFF, -32768, 32767)
        codes[:, k] = (di >> 12) & 0xF
        e2[:] = e1; e1[:] = (di >> shift).astype(np.float64) - s0
    s1[:] = xs[:, -1]; s2[:] = xs[:, -2]
    return (filt << 4) | shift, codes


def encode(sounds, modes, loops) -> list[bytes]:
    n = len(sounds)
    if n == 0:
        return []
    lens = np.array([len(s) // 28 for s in sounds])
    if any(len(s) % 28 for s in sounds):
        raise ValueError("a sound's length is not a multiple of 28 samples")
    w_all = np.array([MODES[m] for m in modes])
    s1, s2, e1, e2 = (np.zeros(n) for _ in range(4))
    out = [bytearray(16 * int(L)) for L in lens]
    for k in range(int(lens.max())):
        live = np.nonzero(lens > k)[0]
        x = np.stack([np.asarray(sounds[i][28 * k:28 * k + 28]) for i in live]).astype(np.int64)
        st = [arr[live] for arr in (s1, s2, e1, e2)]
        hdr, codes = _frames(x, w_all[live], *st)
        for arr, v in zip((s1, s2, e1, e2), st): arr[live] = v
        packed = (codes[:, 0::2] | (codes[:, 1::2] << 4)).astype(np.uint8)
        for j, i in enumerate(live):
            o = 16 * k; out[i][o] = hdr[j]; out[i][o + 2:o + 16] = packed[j].tobytes()
    result = []
    for b, loop in zip(out, loops):
        frames = len(b) // 16
        for k in range(frames):
            if loop is None: flag = 1 if k == frames - 1 else 4 if k == 0 else 0
            else: flag = 3 if k == frames - 1 else 6 if k == loop else 2
            b[16 * k + 1] = flag
        result.append(bytes(16) + bytes(b) + (END_BLOCK if loop is None else b""))
    return result


# --- the decoder ------------------------------------------------------------------------------------------------
#
# Which input does `encode` turn into a stored body? Write Y = clamped input + quantization error. The encoder's
# arithmetic gives Y[k] = Q[k] - f0 Y[k-1] - f1 Y[k-2], with Q the quantized value it feeds back: the stored code
# shifted, except where a code 7 saturates (the encoder clamps to 32767, so it feeds back 32767 >> shift rather than
# 28672 >> shift). So each input lies in an interval fixed by the codes and those saturation choices alone, and
# inputs interact only through each frame's filter and shift selection: the stored filter's residual peak must fall
# in the shift's range (ENCVAG's bit test, which also maps some peaks near 32767 to small shifts) and the other
# filters' weighted peaks must not be smaller. Per frame the decoder tries, in order of cost:
#
#   the nearest inputs with the stored filter's residual under the cap; the inputs closest to their prediction;
#   local repair of either; an exact solve over the frame and up to two before it - with the saturation choices
#   fixed, the constraints are linear in three consecutive inputs, so the feasible (x[k-1], x[k]) pairs form a
#   convex polygon, carried forward and read back, with the other filters' and the floor's conditions added as
#   witness half-planes where a candidate misses them; repair under each saturation choice; joint repair and
#   annealing (seeded, so deterministic) with earlier frames;
#
# backing up to an earlier frame's next candidate when a frame has none (a depth-first search). A sample is solved in
# the most restrictive mode its filters fit (4bit, low, high, normal), which every sample on the disc reproduces in,
# and then in normal mode from that solution: only the frames normal mode selects differently on are searched again,
# with the cheap candidates and no backing up. The sample's mode is normal where that succeeds; a mode is stated only
# where normal does not reproduce the body, since a sound whose frames never needed some filter fits the restrictive
# modes too.

import math as _m
import random as _random

_F = ((0.0, 0.0), (-60 / 64, 0.0), (-115 / 64, 52 / 64), (-98 / 64, 55 / 64), (-122 / 64, 60 / 64))
_SHIFTS = [int(v) for v in SHIFT]
_RUNS: dict[int, list[list[int]]] = {}
_prev = None
for _p, _s in enumerate(_SHIFTS):
    if _s != _prev: _RUNS.setdefault(_s, []).append([_p, _p]); _prev = _s
    else: _RUNS[_s][-1][1] = _p
_EPS = 1e-7
_ALLOWED = {"4bit": {0}, "low": {0, 1, 3}, "high": {0, 1, 4}, "normal": {0, 1, 2, 3, 4}}


class _State:
    __slots__ = ("s1", "s2", "e1", "e2")

    def __init__(self, s1=0.0, s2=0.0, e1=0.0, e2=0.0):
        self.s1, self.s2, self.e1, self.e2 = s1, s2, e1, e2

    def copy(self):
        return _State(self.s1, self.s2, self.e1, self.e2)


def _clamp(v):
    return float(HI if v > HI else LO if v < LO else v)


def _peaks(st, x):
    out = []
    for f0, f1 in _F:
        a, b, m = st.s1, st.s2, 0.0
        for v in x:
            s = _clamp(v); d = abs(s + a * f0 + b * f1)
            if d > m: m = d
            b, a = a, s
        out.append(m)
    return out


def _choose(pk, w):
    best, filt = 1.8e307, 0
    for i, m in enumerate(pk):
        if w[i] * m < best: best, filt = w[i] * m, i
        if i == 0 and m <= 7.0: return 0
    return filt


def _encode_frame(st, x, flags, w):
    """ENCVAG for one frame, scalar (the same arithmetic as `_frames`); advances `st`."""
    pk = _peaks(st, x); filt = _choose(pk, w)
    f0, f1 = _F[filt]
    m = max(-32768, min(32767, int(pk[filt]))); shift = _SHIFTS[m] if m >= 0 else 0
    codes = []; a, b = st.s1, st.s2; e1, e2 = st.e1, st.e2
    for v in x:
        s = _clamp(v)
        d = s + a * f0 + b * f1; b, a = a, s
        s0 = d + e1 * f0 + e2 * f1
        di = max(-32768, min(32767, (int(s0 * (1 << shift)) + 0x800) & ~0xFFF))
        codes.append((di >> 12) & 0xF); e2, e1 = e1, float(di >> shift) - s0
    st.s1, st.s2, st.e1, st.e2 = a, b, e1, e2
    return bytes([(filt << 4) | shift, flags] + [codes[k] | (codes[k + 1] << 4) for k in range(0, 28, 2)])


def _nib(fr, i):
    n = (fr[2 + i // 2] >> (4 * (i & 1))) & 0xF
    return n - 16 if n & 8 else n


def _body_frames(body):
    frames = [body[o:o + 16] for o in range(16, len(body), 16)]
    return frames[:-1] if frames and frames[-1] == END_BLOCK else frames


def _residuals(s1, s2, x):
    xs = [float(HI if v > HI else LO if v < LO else v) for v in x]   # _clamp, once an input
    a = [s1] + xs[:-1]; b = [s2, s1] + xs[:-2]
    return [[s + p * f0 + q * f1 for s, p, q in zip(xs, a, b)] for f0, f1 in _F]


def _violation(s1, s2, x, filt, shift, w):
    """0 when the encoder selects (filt, shift) on x; otherwise a penalty that falls with each residual moved the
    right way: residuals over the shift's cap, the peak under its floor, the stored filter's residuals over another
    filter's weighted peak, filter 0's peak at 7 or below. (The sums leave out their zero terms, which do not change
    a float sum.)"""
    r = _residuals(s1, s2, x); pk = [max(map(abs, rj)) for rj in r]

    def shift_pen(rj, m):
        best = None
        for lo, hi in _RUNS[shift]:
            lim = hi + 1 - _EPS
            pen = max(0.0, lo - m) + sum([d - lim for v in rj if (d := abs(v)) > lim])
            best = pen if best is None or pen < best else best
        return best
    if filt == 0 and pk[0] <= 7.0:
        return shift_pen(r[0], pk[0]), r, pk
    v = shift_pen(r[filt], pk[filt]); wf = w[filt]
    for j in range(5):
        if j != filt:
            cap = w[j] * pk[j] - (_EPS if j < filt else 0.0)
            if wf * pk[filt] > cap:
                v += sum([d for t in r[filt] if (d := wf * abs(t) - cap) > 0.0]) / wf
    if filt != 0:
        v += max(0.0, 7.0 + _EPS - pk[0])
    return v, r, pk


def _frame_bounds(fr, st, sat):
    """Each input's interval and centre, the code-7 positions where both saturation branches are possible, the
    default input (nearest the centre with the stored filter's residual under the shift's cap), and the state after;
    or ("fail", branch points so far) where a code cannot be met. `sat`: the code-7 positions taken saturating."""
    filt, shift = fr[0] >> 4, fr[0] & 15
    f0, f1 = _F[filt]; scale = float(1 << shift)
    cap = _RUNS[shift][0][1] + 1 - 1e-6
    a, b, e1, e2 = st.s1, st.s2, st.e1, st.e2
    los, his, cen, two, xs = [], [], [], [], []
    for i in range(28):
        c = _nib(fr, i)

        def s0_of(v):
            return _clamp(v) + a * f0 + b * f1 + e1 * f0 + e2 * f1

        def code(v):
            di = max(-32768, min(32767, (int(s0_of(v) * scale) + 0x800) & ~0xFFF))
            return di >> 12, di
        c0 = a * f0 + b * f1; fb = e1 * f0 + e2 * f1
        centre = (c << 12 >> shift) - c0 - fb
        P = c0 + fb
        # the code's bucket (trunc(s0 * scale) in [4096c - 2048, 4096c + 2047]) in closed form, nudged exact
        lo = LO if c == -8 else max(LO, _m.floor((c * 4096 - 2048) / scale - P) - 1)
        hi = HI if c == 7 else min(HI, _m.ceil((c * 4096 + 2048) / scale - P) + 1)
        while lo <= hi and code(lo)[0] != c: lo += 1
        while hi >= lo and code(hi)[0] != c: hi -= 1
        if lo > hi: return ("fail", two)
        if c == 7:
            t = max(lo, _m.floor(30720 / scale - P) - 1)      # the first saturating input
            while t <= hi and code(t)[1] != 32767: t += 1
            while t > lo and code(t - 1)[1] == 32767: t -= 1
            if lo < t <= hi: two.append(i)
            if (i in sat and t <= hi) or t == lo:
                lo = t; centre = (32767 >> shift) - c0 - fb
            else:
                hi = t - 1
        l3, h3 = max(lo, _m.ceil(-cap - c0)), min(hi, _m.floor(cap - c0))
        v = min(max(round(centre), l3), h3) if l3 <= h3 else min(max(round(centre), lo), hi)
        los.append(lo); his.append(hi); cen.append(centre); xs.append(v)
        s0 = s0_of(v); di = max(-32768, min(32767, (int(s0 * scale) + 0x800) & ~0xFFF))
        e2, e1 = e1, float(di >> shift) - s0; b, a = a, _clamp(v)
    return los, his, cen, two, xs, _State(a, b, e1, e2)


def _tight(fr, st, got):
    """The second default: each input as close to its prediction as its interval allows, then, if the peak falls
    under the shift's floor, the input with most room pushed out to meet it."""
    filt, shift = fr[0] >> 4, fr[0] & 15
    f0, f1 = _F[filt]; lo, hi = got[0], got[1]
    floor = _RUNS[shift][0][0]
    a, b = st.s1, st.s2; x = []; reach = []
    for k in range(28):
        c0 = a * f0 + b * f1
        v = min(max(round(-c0), lo[k]), hi[k]); x.append(v)
        reach.append(max(abs(lo[k] + c0), abs(hi[k] + c0)))
        b, a = a, _clamp(v)
    r = _residuals(st.s1, st.s2, x)[filt]
    if max(abs(t) for t in r) < floor:
        k = max(range(28), key=lambda i: reach[i])
        a = st.s1 if k == 0 else float(x[k - 1]); b = st.s2 if k == 0 else st.s1 if k == 1 else float(x[k - 2])
        c0 = a * f0 + b * f1
        cand = [v for v in (_m.ceil(floor - c0), _m.floor(-floor - c0)) if lo[k] <= v <= hi[k]]
        if cand:
            x[k] = min(cand, key=lambda v: abs(v - x[k]))
    return x


class _Win:
    """Consecutive frames repaired together: the inputs before the first, and per frame [header, intervals' lows,
    highs, centres, input]."""

    def __init__(self, s1, s2, frames, w):
        self.s1, self.s2, self.f, self.w = s1, s2, frames, w

    def total(self):
        s1, s2, tot, info = self.s1, self.s2, 0.0, []
        for hdr, lo, hi, c, x in self.f:
            v, r, pk = _violation(s1, s2, x, hdr >> 4, hdr & 15, self.w)
            tot += v; info.append((v, r, pk))
            s1, s2 = _clamp(x[-1]), _clamp(x[-2])
        self.vs = [v for v, _r, _pk in info]
        return tot, info

    def trial(self, moved):
        """total()'s penalty, given that only the inputs `moved` ((frame, k) pairs) differ from those of the last
        total() or commit(): a frame's penalty depends on its own inputs and its predecessor's last two, so only
        those frames are evaluated again. -> (penalty, the frames' new penalties) for commit()."""
        touched = set()
        for w, k in moved:
            touched.add(w)
            if k >= 26 and w + 1 < len(self.f): touched.add(w + 1)
        tot, new = 0.0, {}
        for i, v in enumerate(self.vs):
            if i in touched:
                if i == 0: s1, s2 = self.s1, self.s2
                else:
                    xp = self.f[i - 1][4]; s1, s2 = _clamp(xp[-1]), _clamp(xp[-2])
                hdr = self.f[i][0]
                v = new[i] = _violation(s1, s2, self.f[i][4], hdr >> 4, hdr & 15, self.w)[0]
            tot += v
        return tot, new

    def commit(self, new):
        for i, v in new.items(): self.vs[i] = v


def _hot(win, info):
    hot = set()
    for w, (v, r, pk) in enumerate(info):
        if v == 0.0: continue
        filt = win.f[w][0] >> 4; rf = r[filt]
        ks = {max(range(28), key=lambda k: abs(r[j][k])) for j in range(5)}
        ks.update(k for k, val in enumerate(rf) if abs(val) > min(pk) - 2)
        for k in ks:
            for d in (0, 1, 2):
                kk, ww = k - d, w
                if kk < 0: ww, kk = w - 1, kk + 28
                if ww >= 0: hot.add((ww, kk))
    return sorted(hot)


def _values(fr, k, targets, prev):
    """Candidate inputs at k: the interval's ends and a few steps, and those putting a filter's residual exactly at
    a target magnitude, given the two inputs before k."""
    hdr, lo, hi, c, x = fr; span = hi[k] - lo[k]
    vals = {lo[k], hi[k], x[k] - 1, x[k] + 1, x[k] - max(1, span // 4), x[k] + max(1, span // 4),
            round(c[k]), (lo[k] + hi[k]) // 2}
    a, b = prev
    for j, T in targets:
        p = a * _F[j][0] + b * _F[j][1]
        for sgn in (1, -1):
            v = sgn * T - p
            vals.update((_m.floor(v), _m.ceil(v)))
    return {v for v in vals if lo[k] <= v <= hi[k] and v != x[k]}


def _repair(win, rounds=80):
    """Greedy moves (single, then pairs) among the inputs feeding the violated peaks, until the selection holds."""
    cur, info = win.total()
    for _ in range(rounds):
        if cur == 0.0: return True
        hot = _hot(win, info); best = None; targets = []
        for wi, (v_, r_, pk_) in enumerate(info):
            if v_ == 0.0: continue
            filt, shift = win.f[wi][0] >> 4, win.f[wi][0] & 15
            for lo_r, hi_r in _RUNS[shift]:
                targets += [(filt, hi_r + 0.5), (filt, lo_r + 0.5)]
            for j in range(5):
                targets.append((j, pk_[filt] + 1.0)); targets.append((filt, pk_[j] - 1.0))

        def prev_of(w, k):
            xw = win.f[w][4]
            if k >= 2: return _clamp(xw[k - 1]), _clamp(xw[k - 2])
            if w == 0: s1, s2 = win.s1, win.s2
            else:
                xp = win.f[w - 1][4]; s1, s2 = _clamp(xp[-1]), _clamp(xp[-2])
            return (_clamp(xw[0]), s1) if k == 1 else (s1, s2)
        for w, k in hot:
            fr = win.f[w]; old = fr[4][k]
            for v in _values(fr, k, targets, prev_of(w, k)):
                fr[4][k] = v; val = win.trial(((w, k),))[0]; fr[4][k] = old
                key = (val, abs(v - fr[3][k]) - abs(old - fr[3][k]))
                if best is None or key < best[0]: best = (key, ((w, k, v),))
        if best is None or best[0][0] >= cur:
            for i, (w1, k1) in enumerate(hot):
                for (w2, k2) in hot[i + 1:]:
                    f1, f2 = win.f[w1], win.f[w2]; o1, o2 = f1[4][k1], f2[4][k2]
                    for v1 in _values(f1, k1, targets, prev_of(w1, k1)):
                        f1[4][k1] = v1
                        for v2 in _values(f2, k2, targets, prev_of(w2, k2)):
                            f2[4][k2] = v2; val = win.trial(((w1, k1), (w2, k2)))[0]; f2[4][k2] = o2
                            if val < cur and (best is None or (val, 0) < best[0]):
                                best = ((val, 0), ((w1, k1, v1), (w2, k2, v2)))
                        f1[4][k1] = o1
            if best is None or best[0][0] >= cur:
                return False
        for w, k, v in best[1]: win.f[w][4][k] = v
        cur, info = win.total()
    return cur == 0.0


def _anneal(win, iters, seed):
    """Simulated annealing over the window's inputs, seeded; True when the penalty reaches 0."""
    rnd = _random.Random(seed)
    cur, _info = win.total()
    if cur == 0.0: return True
    pos = [(w, k) for w in range(len(win.f)) for k in range(28) if win.f[w][2][k] > win.f[w][1][k]]
    if not pos: return False
    T = max(1.0, cur)
    best = (cur, [list(f[4]) for f in win.f])
    for _ in range(iters):
        w, k = pos[rnd.randrange(len(pos))]
        fr = win.f[w]; lo, hi = fr[1][k], fr[2][k]; old = fr[4][k]
        span = hi - lo
        if rnd.random() < 0.3: v = rnd.choice((lo, hi))
        else: v = min(hi, max(lo, old + int(rnd.gauss(0, max(1.0, span / 6)))))
        if v == old: continue
        fr[4][k] = v
        new, frames = win.trial(((w, k),))
        if new <= cur or rnd.random() < _m.exp((cur - new) / T):
            cur = new; win.commit(frames)
            if cur < best[0]: best = (cur, [list(f[4]) for f in win.f])
            if cur == 0.0: return True
        else:
            fr[4][k] = old
        T = max(1e-3, T * 0.9995)
    for f, x in zip(win.f, best[1]): f[4][:] = x
    return best[0] == 0.0


# the exact solve: convex polygons of (x[k-1], x[k])

def _hull(pts):
    pts = sorted(set((round(x, 9), round(y, 9)) for x, y in pts))
    if len(pts) <= 2: return pts

    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0: lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0: upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def _halfplanes(poly):
    n = len(poly)
    if n == 1:
        w, u = poly[0]; return [(1, 0, w), (-1, 0, -w), (0, 1, u), (0, -1, -u)]
    if n == 2:
        (w1, u1), (w2, u2) = poly; dw, du = w2 - w1, u2 - u1
        return [(du, -dw, du * w1 - dw * u1), (-du, dw, -(du * w1 - dw * u1)),
                (dw, du, dw * w2 + du * u2), (-dw, -du, -(dw * w1 + du * u1))]
    rows = []
    for i in range(n):
        (w1, u1), (w2, u2) = poly[i], poly[(i + 1) % n]
        a, b = (u2 - u1), -(w2 - w1)
        rows.append((a, b, a * w1 + b * u1))
    return rows


def _poly_step(poly, lo, hi, f0, f1, T, extra):
    """The polygon of (x[k-1], x[k]) from that of (x[k-2], x[k-1]): the input's interval and the residual slab, the
    earlier input projected out; `extra` are more a w + b u + c v <= d rows."""
    planes = [(a, b, 0.0, c) for a, b, c in _halfplanes(poly)]
    planes += [(0, 0, 1, hi), (0, 0, -1, -lo), (f1, f0, 1, T), (-f1, -f0, -1, T)] + list(extra)
    pts = []; m = len(planes)
    for i in range(m):
        for j in range(i + 1, m):
            for k in range(j + 1, m):
                A = [planes[i][:3], planes[j][:3], planes[k][:3]]; bvec = [planes[i][3], planes[j][3], planes[k][3]]
                det = (A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
                       + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]))
                if abs(det) < 1e-12: continue

                def col(c):
                    M = [list(row) for row in A]
                    for r in range(3): M[r][c] = bvec[r]
                    return (M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
                            + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0])) / det
                w, u, v = col(0), col(1), col(2)
                if all(p[0] * w + p[1] * u + p[2] * v <= p[3] + 1e-6 for p in planes):
                    pts.append((u, v))
    return _hull(pts)


def _slice_w(poly, u):
    lo, hi = _m.inf, -_m.inf
    n = len(poly)
    for i in range(n):
        (w1, u1), (w2, u2) = poly[i], poly[(i + 1) % n] if n > 1 else poly[i]
        if abs(u1 - u) < 1e-9: lo, hi = min(lo, w1), max(hi, w1)
        if (u1 - u) * (u2 - u) < 0:
            t = (u - u1) / (u2 - u1); w = w1 + t * (w2 - w1); lo, hi = min(lo, w), max(hi, w)
    return lo, hi


def _poly_window(frames, st, witnesses, margin=2.5, max_nodes=20000):
    """Continuous inputs for `frames` keeping every stored-filter residual within the shift's main cap less
    `margin`, saturation branched depth-first; or None."""
    seq = []
    for fr in frames:
        filt, shift = fr[0] >> 4, fr[0] & 15
        T = _RUNS[shift][0][1] + 1 - margin
        for k in range(28): seq.append((fr, filt, shift, k, T))
    stack = [(0, [(st.s2, st.s1)], st.s1 + st.e1, st.s2 + st.e2, [])]
    nodes = 0
    while stack:
        pos, poly, y1, y2, hist = stack.pop(); nodes += 1
        if nodes > max_nodes: return None
        if pos == len(seq):
            return _poly_read_back(hist, poly)
        fr, filt, shift, k, T = seq[pos]
        f0, f1 = _F[filt]; c = _nib(fr, k); scale = float(1 << shift)
        P = f0 * y1 + f1 * y2
        if c == 7:
            branches = [(26624 / scale - P + 0.5 / scale, 30720 / scale - P - 1.5 / scale, 28672 >> shift),
                        (30720 / scale - P + 0.5 / scale, HI, 32767 >> shift)]
        elif c == -8:
            branches = [(LO, -30720 / scale - P - 1.5 / scale, -32768 >> shift)]
        else:
            branches = [((c * 4096 - 2048) / scale - P + 1.5 / scale, (c * 4096 + 2048) / scale - P - 1.5 / scale,
                         (c * 4096) >> shift)]
        extra = witnesses.get(pos, ())
        for lo, hi, q in branches:
            lo2, hi2 = max(lo, LO) + 0.5, min(hi, HI) - 0.5
            if lo2 > hi2: continue
            npoly = _poly_step(poly, lo2, hi2, f0, f1, T, extra)
            if npoly:
                stack.append((pos + 1, npoly, q - P, y1, hist + [(poly, f0, f1, T, extra)]))
    return None


def _poly_read_back(hist, last):
    n = len(hist)
    x = [0.0] * n
    x[n - 1] = sum(p[1] for p in last) / len(last)
    if n >= 2: x[n - 2] = sum(p[0] for p in last) / len(last)
    for p in range(n - 1, 1, -1):
        poly, f0, f1, T, extra = hist[p]
        u, v = x[p - 1], x[p]
        wl, wh = _slice_w(poly, u)
        if f1 > 0:
            wl = max(wl, (-T - v - f0 * u) / f1); wh = min(wh, (T - v - f0 * u) / f1)
        for a, b, c, d in extra:
            if a > 1e-12: wh = min(wh, (d - b * u - c * v) / a)
            elif a < -1e-12: wl = max(wl, (d - b * u - c * v) / a)
        x[p - 2] = (wl + wh) / 2 if wl <= wh else (wl if wl != _m.inf else 0.0)
    return x


def _poly_solve(frames, st, w, tries=24, margin=2.5):
    """Integer inputs reproducing `frames` from `st`, by the polygon solve plus witnesses for each condition a
    candidate misses (another filter's peak, the shift's floor, filter 0 over 7); or None."""
    witnesses: dict[int, list] = {}
    for _ in range(tries):
        got = _poly_window(frames, st, witnesses, margin=margin)
        if got is None: return None
        x = [int(round(v)) for v in got]
        t = st.copy(); bad = None
        for i, fr in enumerate(frames):
            xi = x[28 * i:28 * i + 28]; before = t.copy()
            if _encode_frame(t, xi, fr[1], w) != fr:
                bad = (i, before, xi); break
        if bad is None:
            return [x[28 * i:28 * i + 28] for i in range(len(frames))]
        i, before, xi = bad; fr = frames[i]; filt, shift = fr[0] >> 4, fr[0] & 15
        r = _residuals(before.s1, before.s2, xi); pk = [max(abs(v) for v in rj) for rj in r]
        T = _RUNS[shift][0][1] + 1 - margin; floor = _RUNS[shift][0][0]
        targets = []
        if pk[filt] < floor: targets.append((filt, floor + 0.5))
        for j in range(5):
            if j != filt and w[j] * pk[j] < w[filt] * pk[filt] + (1e-6 if j < filt else 0.0):
                targets.append((j, T * w[filt] / w[j] + 0.5))
        if filt != 0 and pk[0] <= 7.0: targets.append((0, 7.5))
        added = False
        for j, thr in targets:
            k = max(range(28), key=lambda kk: abs(r[j][kk]))
            sgn = 1.0 if r[j][k] >= 0 else -1.0
            row = (-sgn * _F[j][1], -sgn * _F[j][0], -sgn, -thr)
            if row not in witnesses.get(28 * i + k, ()):
                witnesses.setdefault(28 * i + k, []).append(row); added = True
        if not added: return None
    return None


# the search

def _flag_sets(two):
    yield set()
    if two:
        yield set(two)
        if len(two) > 1:
            for p in two: yield {p}
            for p in two: yield set(two) - {p}


def _candidates(frames, k, xs, after, flagsets, w, thorough, hint=None, cheap=False):
    """Inputs for frame k, in order of cost: (x, state after, changed earlier frames {j: (x, state after)}, flags);
    `hint`'s input first, where one is given; with `cheap`, only the defaults, their repair and the polygon solve."""
    fr = frames[k]
    st = _State() if k == 0 else after[k - 1]
    seen = set()
    if hint is not None:
        t = st.copy(); seen.add(tuple(hint[k]))
        if _encode_frame(t, hint[k], fr[1], w) == fr:
            yield list(hint[k]), t, {}, set()
    first = _frame_bounds(fr, st, set())
    two = first[3] if first[0] != "fail" else first[1]
    if first[0] != "fail":
        starts0 = (first[4], _tight(fr, st, first))
        for x0 in starts0:
            if tuple(x0) in seen: continue
            seen.add(tuple(x0)); t = st.copy()
            if _encode_frame(t, x0, fr[1], w) == fr:
                yield list(x0), t, {}, set()
        for x0 in starts0:
            win = _Win(st.s1, st.s2, [[fr[0], first[0], first[1], first[2], list(x0)]], w)
            if _repair(win):
                x = win.f[0][4]; t = st.copy()
                if tuple(x) not in seen and _encode_frame(t, x, fr[1], w) == fr:
                    seen.add(tuple(x)); yield list(x), t, {}, set()

    def polygons():
        for back in (0, 1, 2):
            if k - back < 0: break
            base = _State() if k - back == 0 else after[k - back - 1]
            sol = _poly_solve(frames[k - back:k + 1], base, w)
            if sol is None: continue
            t = base.copy(); changed = {}; good = True
            for i, xi in enumerate(sol):
                j = k - back + i
                if _encode_frame(t, xi, frames[j][1], w) != frames[j]: good = False; break
                if j < k: changed[j] = (list(xi), t.copy())
            if good and tuple(sol[-1]) not in seen:
                seen.add(tuple(sol[-1])); yield list(sol[-1]), t, changed, set()
    if not thorough:
        yield from polygons()
    if cheap:
        return
    tried = []
    for flags in _flag_sets(two):
        got = first if not flags else _frame_bounds(fr, st, flags)
        if got[0] == "fail": continue
        tried.append((flags, got))
        if not flags: continue
        lo, hi, cen = got[0], got[1], got[2]
        starts = [got[4], _tight(fr, st, got)]
        for x0 in starts:
            if tuple(x0) in seen: continue
            seen.add(tuple(x0)); t = st.copy()
            if _encode_frame(t, x0, fr[1], w) == fr:
                yield list(x0), t, {}, flags
        for x0 in starts:
            win = _Win(st.s1, st.s2, [[fr[0], lo, hi, cen, list(x0)]], w)
            if _repair(win):
                x = win.f[0][4]; t = st.copy()
                if tuple(x) not in seen and _encode_frame(t, x, fr[1], w) == fr:
                    seen.add(tuple(x)); yield list(x), t, {}, flags
    if thorough:
        yield from polygons()
    for back in (1, 2):
        if k - back < 0: break
        base = _State() if k - back == 0 else after[k - back - 1]
        for flags, got in tried:
            wf = []; s_ = base.copy(); ok = True
            for j in range(k - back, k):
                g = _frame_bounds(frames[j], s_, flagsets[j])
                if g[0] == "fail": ok = False; break
                wf.append([frames[j][0], g[0], g[1], g[2], list(xs[j])])
                t = s_.copy(); _encode_frame(t, xs[j], frames[j][1], w); s_ = t
            if not ok: continue
            wf.append([fr[0], got[0], got[1], got[2], list(got[4])])
            for attempt in ("repair", "anneal"):
                win = _Win(base.s1, base.s2, [[a_, b_, c_, d_, list(e_)] for a_, b_, c_, d_, e_ in wf], w)
                done = _repair(win) if attempt == "repair" else _anneal(win, 8000, back)
                if not done: continue
                t = base.copy(); changed = {}; good = True
                for i, f in enumerate(win.f):
                    j = k - back + i
                    if _encode_frame(t, f[4], frames[j][1], w) != frames[j]: good = False; break
                    if j < k: changed[j] = (list(f[4]), t.copy())
                if good:
                    yield list(win.f[-1][4]), t, changed, flags
                    break
    for flags, got in tried:
        for seed in (1, 2):
            win = _Win(st.s1, st.s2, [[fr[0], got[0], got[1], got[2], list(got[4])]], w)
            if _anneal(win, 6000, seed):
                x = win.f[0][4]; t = st.copy()
                if tuple(x) not in seen and _encode_frame(t, x, fr[1], w) == fr:
                    seen.add(tuple(x)); yield list(x), t, {}, flags


def _solve_frames(frames, w, thorough, depth=3, budget=400, hint=None, first_failure=False, cheap=False):
    """(inputs per frame, unsolved frame indices), by depth-first search over each frame's candidates; with
    `first_failure`, returning at the first unsolved frame."""
    n = len(frames)
    xs = [None] * n; after = [None] * n; gens = [None] * n; flagsets = [set() for _ in range(n)]
    failures = []
    k = 0; frontier = 0; low = 0; steps = 0

    def before(j): return _State() if j == 0 else after[j - 1]
    while k < n:
        if gens[k] is None:
            gens[k] = _candidates(frames, k, xs, after, flagsets, w, thorough, hint, cheap)
        got = next(gens[k], None); steps += 1
        if got is not None:
            xs[k], after[k], changed, flagsets[k] = got
            for j, (xj, aj) in changed.items(): xs[j], after[j] = xj, aj
            k += 1
            if k > frontier: frontier = k; steps = 0
            continue
        gens[k] = None
        if k - 1 >= low and frontier - (k - 1) <= depth and steps < budget:
            k -= 1
            continue
        # give up on the frontier frame: first candidates up to it, the default there
        j = frontier
        for jj in range(k, frontier):
            gens[jj] = _candidates(frames, jj, xs, after, flagsets, w, thorough, hint, cheap)
            got = next(gens[jj], None)
            if got is None: j = jj; break
            xs[jj], after[jj], changed, flagsets[jj] = got
            for j2, (xj, aj) in changed.items(): xs[j2], after[j2] = xj, aj
        g = _frame_bounds(frames[j], before(j), set())
        x = g[4] if g[0] != "fail" else [0] * 28
        t = before(j).copy(); _encode_frame(t, x, frames[j][1], w)
        xs[j], after[j] = x, t; flagsets[j] = set(); failures.append(j)
        if first_failure: break
        k = j + 1; frontier = max(frontier, k); low = k; steps = 0
        for jj in range(low): gens[jj] = None
    return xs, failures


def mode_of(body: bytes) -> str:
    """The most restrictive mode whose filters include every filter the body's frames use."""
    used = {fr[0] >> 4 for fr in _body_frames(body) if any(fr[2:])}
    return next(m for m in ("4bit", "low", "high", "normal") if used <= _ALLOWED[m])


def loop_of(body: bytes) -> int | None:
    """A body's loop start (a frame index), or None for a one-shot."""
    if body[-16:] == END_BLOCK:
        return None
    flags = [body[o + 1] for o in range(16, len(body), 16)]
    return flags.index(6)


def decode(body: bytes):
    """(pcm, mode, loop start) that `encode` turns into `body`, normal mode wherever it can; raises ValueError where
    no input is found."""
    mode = mode_of(body); frames = _body_frames(body); loop = loop_of(body)
    for thorough in (False, True):
        xs, failures = _solve_frames(frames, MODES[mode], thorough)
        if not failures:
            break
    else:
        raise ValueError(f"no input reproduces frames {failures[:8]} (mode {mode})")
    if mode != "normal":
        again, missed = _solve_frames(frames, MODES["normal"], False, depth=0, hint=xs, first_failure=True, cheap=True)
        if not missed:
            xs, mode = again, "normal"
    pcm = np.array([v for x in xs for v in x], dtype=np.int64)
    if encode([pcm], [mode], [loop])[0] != body:
        raise ValueError("the solved input does not re-encode to the body")
    return pcm, mode, loop
