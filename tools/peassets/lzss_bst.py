"""Persistent Okumura-style BST used by the CLUT LZSS encoder.

``lzss_clut.encode_clut`` keeps one tree across the encode: insert each
newly written absolute position, delete the byte that falls out of the
256-byte window.

Node ids are **absolute output positions**, not ring indices. A 256-byte
window already occupies every ring slot, so a classic ``node = pos & 0xFF``
tree cannot hold the current search key without evicting a still-legal
match source.

Classic Okumura (N=4096, F=18) parks F lookahead bytes *inside* the N-slot
ring, so history is N−F. PE2's decoder ring is 256 *written* bytes with
max match 17 — window is 256, F is only a compare bound.
"""

from __future__ import annotations

from lzss import DICT_SIZE, MAX_MATCH, _match_length, _offset_for_ref

NIL = -1


def _byte_at(data: bytes, i: int, seed_zeros: bool) -> int | None:
    if i < 0:
        return 0 if seed_zeros else None
    if i >= len(data):
        return None
    return data[i]


def match_length(data: bytes, pos: int, ref: int, limit: int, *, seed_zeros: bool) -> int:
    """PE2 match length; ``ref`` may be negative when the zero ring is seeded."""
    if not seed_zeros or ref >= 0:
        return _match_length(data, pos, ref, limit)
    n = len(data)
    max_len = min(limit, n - pos)
    dist = pos - ref
    length = 0
    while length < max_len:
        src = ref + length
        if src < pos:
            b = 0 if src < 0 else data[src]
        else:
            b = data[pos + ((src - pos) % dist)]
        if b != data[pos + length]:
            break
        length += 1
    return length


class PersistentBST:
    """First-byte-rooted BST over absolute positions in the sliding window."""

    def __init__(
        self,
        data: bytes,
        *,
        f: int = MAX_MATCH,
        window: int = DICT_SIZE,
        last_at_max: bool = True,
        replace_at_f: bool = True,
        seed_zeros: bool = True,
    ) -> None:
        self.data = data
        self.f = f
        self.window = window
        self.use_last_at_max = last_at_max
        self.replace_at_f = replace_at_f
        self.seed_zeros = seed_zeros
        self.lson: dict[int, int] = {}
        self.rson: dict[int, int] = {}
        self.dad: dict[int, int] = {}
        self.roots: dict[int, int] = {}
        self.alive: set[int] = set()
        if seed_zeros:
            for i in range(-window, 0):
                self._insert_node(i)

    def _cmp(self, a: int, b: int) -> tuple[int, int]:
        """Compare F-byte keys at absolute positions ``a`` vs ``b``."""
        data = self.data
        seed = self.seed_zeros
        lim = self.f
        j = 0
        while j < lim:
            ca = _byte_at(data, a + j, seed)
            cb = _byte_at(data, b + j, seed)
            if ca is None or cb is None:
                return 0, j
            if ca != cb:
                return ca - cb, j
            j += 1
        return 0, j

    def _root_byte(self, node: int) -> int:
        b = _byte_at(self.data, node, self.seed_zeros)
        assert b is not None
        return b

    def _get_root(self, first: int) -> int:
        return self.roots.get(first, NIL)

    def _set_root(self, first: int, node: int) -> None:
        if node == NIL:
            self.roots.pop(first, None)
        else:
            self.roots[first] = node

    def _link_left(self, parent: int, child: int) -> None:
        self.lson[parent] = child
        if child != NIL:
            self.dad[child] = parent

    def _link_right(self, parent: int, child: int) -> None:
        self.rson[parent] = child
        if child != NIL:
            self.dad[child] = parent

    def search(self, pos: int) -> tuple[int, int]:
        """Walk the tree for ``data[pos:]``. Does not insert ``pos``."""
        if pos >= len(self.data):
            return NIL, 0
        first = self.data[pos]
        key = self._get_root(first)
        best_ref = NIL
        best_len = 0
        while key != NIL:
            cmp, j = self._cmp(pos, key)
            true_len = match_length(
                self.data, pos, key, MAX_MATCH, seed_zeros=self.seed_zeros
            )
            if true_len > best_len or (
                self.use_last_at_max and true_len == best_len and true_len > 0
            ):
                best_len = true_len
                best_ref = key
            if j >= self.f:
                break
            key = self.lson.get(key, NIL) if cmp < 0 else self.rson.get(key, NIL)
        if best_ref != NIL and _offset_for_ref(best_ref) is None:
            return self._search_encodable(pos)
        return best_ref, best_len

    def _search_encodable(self, pos: int) -> tuple[int, int]:
        first = self.data[pos]
        key = self._get_root(first)
        best_ref = NIL
        best_len = 0
        while key != NIL:
            cmp, j = self._cmp(pos, key)
            if _offset_for_ref(key) is not None:
                true_len = match_length(
                    self.data, pos, key, MAX_MATCH, seed_zeros=self.seed_zeros
                )
                if true_len > best_len or (
                    self.use_last_at_max and true_len == best_len and true_len > 0
                ):
                    best_len = true_len
                    best_ref = key
            if j >= self.f:
                break
            key = self.lson.get(key, NIL) if cmp < 0 else self.rson.get(key, NIL)
        return best_ref, best_len

    def last_at_max(self, pos: int) -> int:
        """Last search-path node at maximum true match length, or NIL."""
        ref, _length = self.search(pos)
        return ref

    def feed(self, pos: int) -> None:
        """Record that ``pos`` has been written; drop ``pos - window``."""
        old = pos - self.window
        if old >= (-self.window if self.seed_zeros else 0) and old in self.alive:
            self.delete(old)
        self._insert_node(pos)

    def delete(self, p: int) -> None:
        """Classic Okumura ``DeleteNode`` on absolute position ``p``."""
        if p not in self.alive:
            return
        dad = self.dad
        lson = self.lson
        rson = self.rson
        first = self._root_byte(p)
        lp = lson.get(p, NIL)
        rp = rson.get(p, NIL)
        parent = dad.get(p, NIL)

        if rp == NIL:
            q = lp
        elif lp == NIL:
            q = rp
        else:
            q = lp
            if rson.get(q, NIL) != NIL:
                while rson.get(q, NIL) != NIL:
                    q = rson[q]
                q_dad = dad[q]
                q_l = lson.get(q, NIL)
                if rson.get(q_dad, NIL) == q:
                    self._link_right(q_dad, q_l)
                else:
                    self._link_left(q_dad, q_l)
                self._link_left(q, lp)
            self._link_right(q, rp)

        if parent == NIL:
            self._set_root(first, q)
            if q != NIL:
                dad[q] = NIL
        elif rson.get(parent, NIL) == p:
            self._link_right(parent, q)
        else:
            self._link_left(parent, q)

        self.alive.discard(p)
        lson.pop(p, None)
        rson.pop(p, None)
        dad.pop(p, None)

    def _insert_node(self, r: int) -> None:
        if r in self.alive:
            self.delete(r)
        first = self._root_byte(r)
        self.lson[r] = NIL
        self.rson[r] = NIL
        self.dad[r] = NIL
        self.alive.add(r)

        root = self._get_root(first)
        if root == NIL:
            self._set_root(first, r)
            return

        key = root
        while True:
            cmp, j = self._cmp(r, key)
            if self.replace_at_f and j >= self.f:
                self._replace_node(key, r)
                return
            if cmp < 0:
                nxt = self.lson.get(key, NIL)
                if nxt == NIL:
                    self._link_left(key, r)
                    return
                key = nxt
            else:
                nxt = self.rson.get(key, NIL)
                if nxt == NIL:
                    self._link_right(key, r)
                    return
                key = nxt

    def _replace_node(self, old: int, new: int) -> None:
        parent = self.dad.get(old, NIL)
        first = self._root_byte(old)
        lp = self.lson.get(old, NIL)
        rp = self.rson.get(old, NIL)
        self.lson[new] = lp
        self.rson[new] = rp
        self.dad[new] = parent
        if lp != NIL:
            self.dad[lp] = new
        if rp != NIL:
            self.dad[rp] = new
        if parent == NIL:
            self._set_root(first, new)
        elif self.rson.get(parent, NIL) == old:
            self.rson[parent] = new
        else:
            self.lson[parent] = new
        self.alive.discard(old)
        self.lson.pop(old, None)
        self.rson.pop(old, None)
        self.dad.pop(old, None)
        self.alive.add(new)
