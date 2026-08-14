"""LZSS codec used by Parasite Eve 2 (``.pe2pkg``, image strips, CLUTs).

Format (matches ``Fs_DecompressChunk`` / ``Fs_DecompressImage``)::

    Bitstream is MSB-first within each byte.

    loop:
        flag = get_bits(1)
        if flag == 1:
            literal = get_bits(8)          # emit + write to 256-byte ring
        else:
            offset = get_bits(8)           # 0 → end of stream
            if offset == 0: break
            length = get_bits(4) + 2       # copy length 2..17
            # copy from ring[offset - 1], advancing both read and write indices

The ring is 256 bytes, initially zero. The Python model writes the first
output byte at index 0 and encodes match starts as ``index + 1`` (so encoded
offset ``0`` stays reserved for EOS). This is isomorphic to the game's model
(write cursor starts at 1; offset is the absolute ring index).

Decoder is based on md_hyena's implementation with defensive bounds checks
for padded chunk tails.

Encoder (best-effort matching)
------------------------------
``encode_lzss`` is the **production** encoder for repack and image authoring.
It is a **best-effort** attempt to match retail bitstreams when possible, while
always remaining decoder-compatible:

* Always: ``decode_lzss(encode_lzss(data)) == data``.
* Default policy: greedy longest match in the last 256 *written* bytes;
  multi_max → **newest**; skip unencodable match starts (ring index ``0xFF``).
* **CLUTs** (``kind="clut"``): delegates to ``lzss_clut.encode_clut``, which
  tracks retail CLUT identity more closely (persistent Okumura last-at-max).

It does **not** guarantee byte-identical retail streams for packages or image
strips. Measure pack layout / round-trip with
``lzss_roundtrip_report.py``. Encoder policies and identity research:
``doc/LZSS_ENCODER.md``. Frozen package cascade: ``lzss_cascading``. CLUT
identity: ``lzss_clut``.
"""

from __future__ import annotations

DICT_SIZE = 256
OFFSET_BITS = 8
STRING_LEN_BITS = 4
LIT_SIZE = 8
MIN_MATCH = 2
MAX_MATCH = (1 << STRING_LEN_BITS) + 1  # 17: nibble 0..15 → length 2..17


def decode_lzss(stream: bytes) -> bytes:
    # Based on the lzss decoder by md_hyena
    DICT_COR = 1

    image_size = 0
    dictionary = bytearray(DICT_SIZE)
    output = bytearray()

    buf = 0
    mask = 0
    ibcar = 0  # Input buffer carriage
    dictcar = 0  # Dictionary carriage

    def get_bit(n: int) -> int:
        x = 0
        nonlocal mask, buf, ibcar
        for _ in range(n):
            if mask == 0:
                if ibcar >= len(stream):
                    # No more input: remaining bits are 0.
                    mask = 0
                    buf = 0
                else:
                    buf = stream[ibcar]
                    ibcar += 1
                    mask = 128
            x <<= 1
            if buf & mask != 0:
                x += 1
            mask >>= 1
        return x

    def write_to_dict(byte: int):
        nonlocal dictcar
        dictionary[dictcar] = byte
        dictcar = (dictcar + 1) & 0xFF

    def write_to_output(byte: int):
        nonlocal image_size
        output.append(byte)
        image_size += 1

    def is_eos() -> bool:
        return ibcar >= len(stream)

    def skip_zeroes() -> bool:
        nonlocal ibcar
        if is_eos():
            return True
        while ibcar < len(stream) and stream[ibcar] == 0:
            ibcar += 1
            if is_eos():
                return True
        return False

    def unpack():
        nonlocal dictcar, buf, mask, image_size
        for i in range(len(dictionary)):
            dictionary[i] = 0
        dictcar = 0
        buf = 0
        mask = 0

        while True:
            if get_bit(1) != 0:
                lit = get_bit(LIT_SIZE)
                write_to_dict(lit)
                write_to_output(lit)
            else:
                offset = get_bit(OFFSET_BITS)
                if offset == 0:
                    break
                length = get_bit(STRING_LEN_BITS)
                offset -= DICT_COR
                for _ in range(length + 2):
                    lit = dictionary[offset]
                    write_to_dict(lit)
                    write_to_output(lit)
                    offset = (offset + 1) & 0xFF

        if not skip_zeroes() and not is_eos():
            image_size -= 1
            if image_size == 32768:
                image_size = 0
            if image_size > 0 and output:
                output.pop()
            unpack()

    unpack()
    return bytes(output)


def _match_length(data: bytes, pos: int, ref: int, limit: int) -> int:
    """Longest match of data[pos:] against history starting at ref (with RLE overlap)."""
    n = len(data)
    max_len = min(limit, n - pos)
    # Distance of match source from current pos (for self-overlap).
    dist = pos - ref
    length = 0
    while length < max_len:
        src = ref + length
        if src < pos:
            b = data[src]
        else:
            b = data[pos + ((src - pos) % dist)]
        if b != data[pos + length]:
            break
        length += 1
    return length


def _offset_for_ref(ref: int) -> int | None:
    """Encoded match offset for absolute output index ``ref``, or None if unencodable.

    Ring index is ``ref & 0xFF``. Encoded offset is ``index + 1``; index 255
    cannot be a match *start* because that would encode as 0 (EOS).
    """
    idx = ref & 0xFF
    if idx == 0xFF:
        return None
    return idx + 1


def compressed_size(stream: bytes) -> int:
    """Return the number of input bytes consumed through the first EOS.

    Trailing zeros after EOS are sector/body padding from extract, not part of
    the LZSS payload. ``stream[:compressed_size(stream)]`` is the minimal
    bitstream ``decode_lzss`` needs (and what the encoders emit).
    """
    buf = 0
    mask = 0
    ibcar = 0

    def get_bit(n: int) -> int:
        nonlocal mask, buf, ibcar
        x = 0
        for _ in range(n):
            if mask == 0:
                if ibcar >= len(stream):
                    buf = 0
                    mask = 0
                    return x  # truncated
                buf = stream[ibcar]
                ibcar += 1
                mask = 128
            x = (x << 1) | (1 if (buf & mask) else 0)
            mask >>= 1
        return x

    while True:
        if get_bit(1):
            get_bit(LIT_SIZE)
        else:
            offset = get_bit(OFFSET_BITS)
            if offset == 0:
                return ibcar
            get_bit(STRING_LEN_BITS)


def trim_lzss(stream: bytes) -> bytes:
    """Drop trailing padding after EOS (keeps a valid minimal bitstream)."""
    if not stream:
        return stream
    end = compressed_size(stream)
    return stream[:end]


def _pack_tokens(tokens: list[tuple]) -> bytes:
    bits: list[int] = []

    def put_bit(bit: int) -> None:
        bits.append(1 if bit else 0)

    def put_bits(value: int, n: int) -> None:
        for i in range(n - 1, -1, -1):
            put_bit((value >> i) & 1)

    for t in tokens:
        kind = t[0]
        if kind == "L":
            put_bit(1)
            put_bits(t[1] & 0xFF, LIT_SIZE)
        elif kind == "M":
            put_bit(0)
            put_bits(t[1] & 0xFF, OFFSET_BITS)
            put_bits((t[2] - MIN_MATCH) & 0xF, STRING_LEN_BITS)
        elif kind == "EOS":
            put_bit(0)
            put_bits(0, OFFSET_BITS)
        else:
            raise ValueError(f"unknown token {t!r}")

    out = bytearray()
    acc = 0
    count = 0
    for bit in bits:
        acc = (acc << 1) | bit
        count += 1
        if count == 8:
            out.append(acc)
            acc = 0
            count = 0
    if count:
        acc <<= 8 - count
        out.append(acc)
    return bytes(out)


def encode_lzss(data: bytes, *, kind: str | None = None) -> bytes:
    """Best-effort PE2 LZSS encode (always decoder-compatible).

    Parameters
    ----------
    data:
        Uncompressed payload.
    kind:
        Optional asset class. ``"clut"`` routes to
        :func:`lzss_clut.encode_clut` (CLUT identity policy). Any other value
        (including ``None``) uses the default greedy newest multi_max encoder.

    Always: ``decode_lzss(encode_lzss(data)) == data``. Retail bit-identity is
    best-effort only (stronger for CLUTs via ``kind="clut"``).
    """
    if kind == "clut":
        # Lazy import: lzss_clut imports helpers from this module.
        from lzss_clut import encode_clut

        return encode_clut(data)

    return _encode_lzss_greedy(data)


def _encode_lzss_greedy(data: bytes) -> bytes:
    """Greedy longest-match encode; multi_max picks newest absolute ref."""
    n = len(data)
    if n == 0:
        return _pack_tokens([("EOS",)])

    head = [-1] * 256
    prev = [-1] * n

    def insert(i: int) -> None:
        b = data[i]
        prev[i] = head[b]
        head[b] = i

    tokens: list[tuple] = []
    pos = 0

    while pos < n:
        window_start = max(0, pos - DICT_SIZE)
        best_len = 0
        cands: list[int] = []

        p = head[data[pos]]
        while p >= window_start:
            if _offset_for_ref(p) is not None:
                length = _match_length(data, pos, p, MAX_MATCH)
                if length >= MIN_MATCH:
                    if length > best_len:
                        best_len = length
                        cands = [p]
                    elif length == best_len:
                        cands.append(p)
            p = prev[p]

        if best_len >= MIN_MATCH and cands:
            chosen = max(cands)  # newest
            off = _offset_for_ref(chosen)
            assert off is not None
            tokens.append(("M", off, best_len))
            end = pos + best_len
            while pos < end:
                insert(pos)
                pos += 1
        else:
            tokens.append(("L", data[pos]))
            insert(pos)
            pos += 1

    tokens.append(("EOS",))
    return _pack_tokens(tokens)
