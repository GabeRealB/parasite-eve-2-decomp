"""PlayStation BS (bitstream) version 2 codec for PE2 room backgrounds.

Demultiplexed STR/BS v2 frame layout (little-endian)::

    u16  mdec_code_count_div2   # (rounded MDEC code count) / 2
    u16  0x3800                 # BS magic
    u16  quant_scale            # frame quantization scale
    u16  version                # 2 for v2
    …    VLC bitstream (16-bit LE words, MSB-first within word after byte-swap)

Macroblocks are 16×16, ordered **column-major** (top→bottom, then next column).
Each macroblock: Cr, Cb, Y1, Y2, Y3, Y4 (8×8 blocks).

Based on the public PlayStation1_STR_format documentation (jPSXdec / MIT text).
PE2 retail room backgrounds are consistently 320×240 (20×15 macroblocks).

Encode is lossy (DCT + quant); pack prefers ``raw/bs/`` for bit-identity when
available, and falls back to PNG→BS for hybrid/decoded or missing raw.
"""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image
except ImportError as e:  # pragma: no cover
    raise ImportError("Pillow required for BS decode") from e

import numpy as np

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

BS_MAGIC = 0x3800
BS_HEADER_SIZE = 8
DEFAULT_WIDTH = 320
DEFAULT_HEIGHT = 240

# MPEG-1 AC VLC (code bits without sign) → (zero_run, level)
# Sign bit follows each code in the bitstream.
_AC_CODES: list[tuple[str, int, int]] = [
    ("11", 0, 1),
    ("011", 1, 1),
    ("0100", 0, 2),
    ("0101", 2, 1),
    ("00101", 0, 3),
    ("00110", 4, 1),
    ("00111", 3, 1),
    ("000100", 7, 1),
    ("000101", 6, 1),
    ("000110", 1, 2),
    ("000111", 5, 1),
    ("0000100", 2, 2),
    ("0000101", 9, 1),
    ("0000110", 0, 4),
    ("0000111", 8, 1),
    ("00100000", 13, 1),
    ("00100001", 0, 6),
    ("00100010", 12, 1),
    ("00100011", 11, 1),
    ("00100100", 3, 2),
    ("00100101", 1, 3),
    ("00100110", 0, 5),
    ("00100111", 10, 1),
    ("0000001000", 16, 1),
    ("0000001001", 5, 2),
    ("0000001010", 0, 7),
    ("0000001011", 2, 3),
    ("0000001100", 1, 4),
    ("0000001101", 15, 1),
    ("0000001110", 14, 1),
    ("0000001111", 4, 2),
    ("000000010000", 0, 11),
    ("000000010001", 8, 2),
    ("000000010010", 4, 3),
    ("000000010011", 0, 10),
    ("000000010100", 2, 4),
    ("000000010101", 7, 2),
    ("000000010110", 21, 1),
    ("000000010111", 20, 1),
    ("000000011000", 0, 9),
    ("000000011001", 19, 1),
    ("000000011010", 18, 1),
    ("000000011011", 1, 5),
    ("000000011100", 3, 3),
    ("000000011101", 0, 8),
    ("000000011110", 6, 2),
    ("000000011111", 17, 1),
    ("0000000010000", 10, 2),
    ("0000000010001", 9, 2),
    ("0000000010010", 5, 3),
    ("0000000010011", 3, 4),
    ("0000000010100", 2, 5),
    ("0000000010101", 1, 7),
    ("0000000010110", 1, 6),
    ("0000000010111", 0, 15),
    ("0000000011000", 0, 14),
    ("0000000011001", 0, 13),
    ("0000000011010", 0, 12),
    ("0000000011011", 26, 1),
    ("0000000011100", 25, 1),
    ("0000000011101", 24, 1),
    ("0000000011110", 23, 1),
    ("0000000011111", 22, 1),
    ("00000000010000", 0, 31),
    ("00000000010001", 0, 30),
    ("00000000010010", 0, 29),
    ("00000000010011", 0, 28),
    ("00000000010100", 0, 27),
    ("00000000010101", 0, 26),
    ("00000000010110", 0, 25),
    ("00000000010111", 0, 24),
    ("00000000011000", 0, 23),
    ("00000000011001", 0, 22),
    ("00000000011010", 0, 21),
    ("00000000011011", 0, 20),
    ("00000000011100", 0, 19),
    ("00000000011101", 0, 18),
    ("00000000011110", 0, 17),
    ("00000000011111", 0, 16),
    ("000000000010000", 0, 40),
    ("000000000010001", 0, 39),
    ("000000000010010", 0, 38),
    ("000000000010011", 0, 37),
    ("000000000010100", 0, 36),
    ("000000000010101", 0, 35),
    ("000000000010110", 0, 34),
    ("000000000010111", 0, 33),
    ("000000000011000", 0, 32),
    ("000000000011001", 1, 14),
    ("000000000011010", 1, 13),
    ("000000000011011", 1, 12),
    ("000000000011100", 1, 11),
    ("000000000011101", 1, 10),
    ("000000000011110", 1, 9),
    ("000000000011111", 1, 8),
    ("0000000000010000", 1, 18),
    ("0000000000010001", 1, 17),
    ("0000000000010010", 1, 16),
    ("0000000000010011", 1, 15),
    ("0000000000010100", 6, 3),
    ("0000000000010101", 16, 2),
    ("0000000000010110", 15, 2),
    ("0000000000010111", 14, 2),
    ("0000000000011000", 13, 2),
    ("0000000000011001", 12, 2),
    ("0000000000011010", 11, 2),
    ("0000000000011011", 31, 1),
    ("0000000000011100", 30, 1),
    ("0000000000011101", 29, 1),
    ("0000000000011110", 28, 1),
    ("0000000000011111", 27, 1),
]

# JPEG/MPEG zigzag: ZIGZAG[y][x] = coefficient list index
ZIGZAG = [
    [0, 1, 5, 6, 14, 15, 27, 28],
    [2, 4, 7, 13, 16, 26, 29, 42],
    [3, 8, 12, 17, 25, 30, 41, 43],
    [9, 11, 18, 24, 31, 40, 44, 53],
    [10, 19, 23, 32, 39, 45, 52, 54],
    [20, 22, 33, 38, 46, 51, 55, 60],
    [21, 34, 37, 47, 50, 56, 59, 61],
    [35, 36, 48, 49, 57, 58, 62, 63],
]
# Inverse zigzag: list index → flat (y*8+x)
_UNZIGZAG = [0] * 64
for _y in range(8):
    for _x in range(8):
        _UNZIGZAG[ZIGZAG[_y][_x]] = _y * 8 + _x

# Binary trie for AC VLCs: node = (child0, child1, terminal_or_None)
# terminal is (run, level) or the special markers "EOB" / "ESC"
_AC_TRIE: list = [None, None, None]  # filled below


def _build_ac_trie() -> list:
    root: list = [None, None, None]

    def insert(bits: str, term: object) -> None:
        node = root
        for ch in bits:
            b = 1 if ch == "1" else 0
            if node[b] is None:
                node[b] = [None, None, None]
            node = node[b]
        node[2] = term

    insert("10", "EOB")
    insert("000001", "ESC")
    for code, run, level in _AC_CODES:
        insert(code, (run, level))
    return root


_AC_TRIE = _build_ac_trie()

# PSX intra quant table (MPEG1-like, DC entry = 2)
QUANT = np.array(
    [
        [2, 16, 19, 22, 26, 27, 29, 34],
        [16, 16, 22, 24, 27, 29, 34, 37],
        [19, 22, 26, 27, 29, 34, 34, 38],
        [22, 22, 26, 27, 29, 34, 37, 40],
        [22, 26, 27, 29, 32, 35, 40, 48],
        [26, 27, 29, 32, 35, 40, 48, 58],
        [26, 27, 29, 34, 38, 46, 56, 69],
        [27, 29, 35, 38, 46, 56, 69, 83],
    ],
    dtype=np.float64,
)


# PSX MDEC cosine matrix (jPSXdec PsxMdecIDCT_double), scaled by 1/65536.
# Row-major [freq, spatial]; approx. orthonormal DCT basis.
_PSX_COS = (
    np.array(
        [
            23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170,
            32138, 27245, 18204, 6392, -6393, -18205, -27246, -32139,
            30273, 12539, -12540, -30274, -30274, -12540, 12539, 30273,
            27245, -6393, -32139, -18205, 18204, 32138, 6392, -27246,
            23170, -23171, -23171, 23170, 23170, -23171, -23171, 23170,
            18204, -32139, 6392, 27245, -27246, -6393, 32138, -18205,
            12539, -30274, 30273, -12540, -12540, 30273, -30274, 12539,
            6392, -18205, 27245, -32139, 32138, -27246, 18204, -6393,
        ],
        dtype=np.float64,
    ).reshape(8, 8)
    / 65536.0
)


def _psx_idct(block: np.ndarray) -> np.ndarray:
    """2-D IDCT matching jPSXdec ``PsxMdecIDCT_double``.

    ``block`` / return value are 8×8 natural order ``[row, col]`` (row-major
    flat index ``row*8+col``). Uses the PSX fixed-point cosine table.
    """
    # jPSXdec stores flat index as x + y*8 (x=col, y=row). Vectorised form of:
    #   aTemp[x,y] = sum_i F[i,x] * C[i,y]
    #   out[x,y]   = sum_i C[i,x] * aTemp[i,y]
    # with natural image = out.T (row=y, col=x).
    temp_xy = block.T @ _PSX_COS  # [x, y]
    out_xy = _PSX_COS.T @ temp_xy  # [x, y]
    return out_xy.T


def _psx_dct(spatial: np.ndarray) -> np.ndarray:
    """2-D DCT matching jPSXdec ``PsxMdecIDCT_double.DCT`` (encode path)."""
    # aTemp[x,y] = sum_i spatial[i,x] * C[y,i]
    # out[x,y]   = sum_i C[x,i] * aTemp[i,y]
    temp_xy = spatial.T @ _PSX_COS.T
    out_xy = _PSX_COS @ temp_xy
    return out_xy.T


# (run, abs_level) → VLC bits without sign (sign bit appended at encode time)
_AC_ENCODE: dict[tuple[int, int], str] = {
    (run, level): code for code, run, level in _AC_CODES
}

# v2 end-of-frame padding (10 bits)
_EOF_PADDING_BITS = "0111111111"

# Common PE2 / PSX background sizes (width, height), tried by macroblock count
COMMON_SIZES = (
    (320, 240),
    (320, 224),
    (256, 240),
    (256, 256),
    (640, 240),
    (640, 480),
    (512, 240),
    (384, 240),
    (160, 120),
    (320, 160),
)


@dataclass
class BsHeader:
    mdec_code_count_div2: int
    magic: int
    quant_scale: int
    version: int

    @property
    def is_v2(self) -> bool:
        return self.magic == BS_MAGIC and self.version == 2


@dataclass
class BsInfo:
    """Decoded BS frame metadata (for PNG sidecar JSON)."""

    width: int
    height: int
    quant_scale: int
    version: int
    mdec_code_count_div2: int
    macroblocks: int
    original_size: int


class BitReader:
    """16-bit little-endian word bitstream, MSB-first within each word.

    Matches jPSXdec ``LITTLE_ENDIAN_SHORT_ORDER``: byte offset within each
    short is XOR 1, so the high bit of the first LE short is bit 7 of the
    second on-disc byte.
    """

    __slots__ = ("_data", "_pos", "_end", "_cur", "_left")

    def __init__(self, data: bytes, start: int = BS_HEADER_SIZE):
        self._data = data
        self._pos = start & ~1  # byte index into stream (even)
        self._end = len(data) & ~1
        self._cur = 0
        self._left = 0

    def _refill(self) -> None:
        if self._left > 0:
            return
        if self._pos + 2 > self._end:
            raise EOFError("end of BS bitstream")
        # readShort with getByteOffset(i)=i^1 at start=_pos relative 0:
        # b1 = data[_pos+1], b0 = data[_pos+0] → (b1<<8)|b0 as unsigned bits
        b0 = self._data[self._pos]
        b1 = self._data[self._pos + 1]
        self._pos += 2
        self._cur = (b1 << 8) | b0
        self._left = 16

    def read_bits(self, n: int) -> int:
        if n <= 0:
            return 0
        out = 0
        while n > 0:
            self._refill()
            take = min(n, self._left)
            out = (out << take) | (
                (self._cur >> (self._left - take)) & ((1 << take) - 1)
            )
            self._left -= take
            n -= take
        return out

    def read_signed(self, n: int) -> int:
        """Read n bits as two's-complement signed.

        Python ints are unbounded, so C-style ``(v<<(32-n))>>(32-n)`` does
        **not** sign-extend; subtract ``1<<n`` when the sign bit is set.
        """
        v = self.read_bits(n)
        if n > 0 and (v & (1 << (n - 1))):
            v -= 1 << n
        return v


def parse_bs_header(data: bytes) -> BsHeader:
    if len(data) < BS_HEADER_SIZE:
        raise ValueError("BS too small")
    a, magic, q, ver = struct.unpack_from("<4H", data, 0)
    return BsHeader(a, magic, q, ver)


def is_bs_v2(data: bytes) -> bool:
    if len(data) < BS_HEADER_SIZE:
        return False
    try:
        return parse_bs_header(data).is_v2
    except Exception:
        return False


def _match_ac(br: BitReader) -> tuple[int, int] | None:
    """Return (zero_run, level) or None for EOB."""
    node = _AC_TRIE
    for _ in range(17):
        bit = br.read_bits(1)
        nxt = node[bit]
        if nxt is None:
            raise ValueError("unknown AC VLC prefix")
        node = nxt
        term = node[2]
        if term is None:
            continue
        if term == "EOB":
            return None
        if term == "ESC":
            run = br.read_bits(6)
            ac = br.read_signed(10)
            return run, ac
        run, level = term  # type: ignore[misc]
        sign = br.read_bits(1)
        return run, -level if sign else level
    raise ValueError("unknown AC VLC prefix (too long)")


def _decode_block_coeffs(br: BitReader, q_scale: int) -> np.ndarray:
    """Decode one 8×8 block to spatial domain (after IDCT)."""
    dc = br.read_signed(10)
    flat = np.zeros(64, dtype=np.float64)
    flat[0] = dc
    i = 0
    while True:
        ac = _match_ac(br)
        if ac is None:
            break
        run, level = ac
        i += 1 + run
        if i >= 64:
            break
        flat[_UNZIGZAG[i]] = level

    mat = flat.reshape(8, 8)

    # Dequantize (PSX MDEC / jPSXdec):
    #   DC:  coeff * Q[0]
    #   AC:  coeff * Q[i] * q_scale / 8
    # (equivalent to the STR doc form 2*coeff*q*Q/16 for AC).
    dq = mat * QUANT
    scale = q_scale / 8.0
    dq[0, 1:] *= scale
    dq[1:, :] *= scale

    return _psx_idct(dq)


def _decode_macroblock(br: BitReader, q_scale: int) -> np.ndarray:
    """Return 16×16×3 RGB uint8 for one macroblock (Cr, Cb, Y1..Y4)."""
    cr_b, cb_b, y1, y2, y3, y4 = [
        _decode_block_coeffs(br, q_scale) for _ in range(6)
    ]
    # Upsample chroma to 16×16 and assemble Y
    y = np.empty((16, 16), dtype=np.float64)
    y[0:8, 0:8] = y1
    y[0:8, 8:16] = y2
    y[8:16, 0:8] = y3
    y[8:16, 8:16] = y4
    y = y + 128.0

    cb = np.repeat(np.repeat(cb_b, 2, axis=0), 2, axis=1)
    cr = np.repeat(np.repeat(cr_b, 2, axis=0), 2, axis=1)

    r = y + 1.402 * cr
    g = y - 0.3437 * cb - 0.7143 * cr
    b = y + 1.772 * cb
    rgb = np.stack([r, g, b], axis=-1)
    np.clip(rgb, 0, 255, out=rgb)
    return rgb.astype(np.uint8)


def _infer_size(n_mb: int) -> tuple[int, int]:
    for w, h in COMMON_SIZES:
        if ((w + 15) // 16) * ((h + 15) // 16) == n_mb:
            return w, h
    # Factor n_mb into w_mb × h_mb preferring ~4:3 landscape
    best: tuple[int, int] | None = None
    for w_mb in range(1, n_mb + 1):
        if n_mb % w_mb == 0:
            h_mb = n_mb // w_mb
            cand = (w_mb * 16, h_mb * 16)
            if best is None or abs(cand[0] / cand[1] - 4 / 3) < abs(
                best[0] / best[1] - 4 / 3
            ):
                best = cand
    if best is None:
        s = int(math.ceil(math.sqrt(n_mb)))
        best = (s * 16, ((n_mb + s - 1) // s) * 16)
    return best


def decode_bs_v2(
    data: bytes,
    *,
    width: int | None = None,
    height: int | None = None,
) -> tuple[Image.Image, BsInfo]:
    """Decode a BS version-2 frame to an RGB PIL image + info.

    PE2 backgrounds default to 320×240. If width/height are omitted the
    decoder uses that default macroblock grid (300 MBs). Pass both to force
    another size.
    """
    hdr = parse_bs_header(data)
    if hdr.magic != BS_MAGIC:
        raise ValueError(
            f"not a BS frame (magic=0x{hdr.magic:04X}, want 0x3800)"
        )
    if hdr.version != 2:
        raise ValueError(f"unsupported BS version {hdr.version} (only v2)")

    if width is None and height is None:
        width, height = DEFAULT_WIDTH, DEFAULT_HEIGHT
    elif width is None or height is None:
        raise ValueError("width and height must both be set or both omitted")

    w_mb = (width + 15) // 16
    h_mb = (height + 15) // 16
    need = w_mb * h_mb

    br = BitReader(data, BS_HEADER_SIZE)
    q = hdr.quant_scale
    mbs: list[np.ndarray] = []
    for _ in range(need):
        try:
            mbs.append(_decode_macroblock(br, q))
        except (EOFError, ValueError) as e:
            if not mbs:
                raise ValueError(f"no macroblocks decoded from BS stream: {e}") from e
            break

    n = len(mbs)
    if n < need:
        # Partial frame — re-infer size if we didn't force defaults mid-stream
        if n > 0 and (width, height) == (DEFAULT_WIDTH, DEFAULT_HEIGHT):
            width, height = _infer_size(n)
            w_mb = (width + 15) // 16
            h_mb = (height + 15) // 16
            need = w_mb * h_mb
        black = np.zeros((16, 16, 3), dtype=np.uint8)
        mbs = mbs + [black] * max(0, need - n)
    elif n > need:
        mbs = mbs[:need]

    # Column-major placement
    out = np.zeros((h_mb * 16, w_mb * 16, 3), dtype=np.uint8)
    idx = 0
    for col in range(w_mb):
        for row in range(h_mb):
            out[row * 16 : (row + 1) * 16, col * 16 : (col + 1) * 16] = mbs[idx]
            idx += 1

    img = Image.fromarray(out[:height, :width], "RGB")
    info = BsInfo(
        width=width,
        height=height,
        quant_scale=hdr.quant_scale,
        version=hdr.version,
        mdec_code_count_div2=hdr.mdec_code_count_div2,
        macroblocks=min(n, need),
        original_size=len(data),
    )
    return img, info


def decode_bs_file(
    path: Path | str,
    *,
    width: int | None = None,
    height: int | None = None,
) -> tuple[Image.Image, BsInfo]:
    return decode_bs_v2(
        Path(path).read_bytes(), width=width, height=height
    )


def info_to_json_bs(info: BsInfo) -> dict:
    return {
        "format": "bs",
        "version": info.version,
        "width": info.width,
        "height": info.height,
        "quant_scale": info.quant_scale,
        "mdec_code_count_div2": info.mdec_code_count_div2,
        "macroblocks": info.macroblocks,
        "original_size": info.original_size,
        "note": (
            "PSX BS v2 MDEC background (column-major macroblocks). "
            "Pack prefers raw/.bs for bit-identity; PNG→BS re-encode is lossy."
        ),
    }


# ---------------------------------------------------------------------------
# Encoder
# ---------------------------------------------------------------------------


class BitWriter:
    """Bit packer matching jPSXdec ``BitStreamWriter`` + LE-short byte order.

    Bits are written MSB-first into a logical byte stream, then remapped with
    ``offset ^= 1`` (swap within each 16-bit word) and padded to 4 bytes.
    """

    __slots__ = ("_bytes", "_cur", "_nbits")

    def __init__(self) -> None:
        self._bytes = bytearray()
        self._cur = 0
        self._nbits = 0

    def write_bit(self, bit: int) -> None:
        self._cur = (self._cur << 1) | (1 if bit else 0)
        self._nbits += 1
        if self._nbits == 8:
            self._bytes.append(self._cur & 0xFF)
            self._cur = 0
            self._nbits = 0

    def write_bits(self, value: int, n: int) -> None:
        """Write ``n`` bits of ``value`` (MSB first). ``value`` may be signed."""
        if n <= 0:
            return
        mask = (1 << n) - 1
        v = value & mask
        for i in range(n - 1, -1, -1):
            self.write_bit((v >> i) & 1)

    def write_bitstring(self, bits: str) -> None:
        for ch in bits:
            if ch == "1":
                self.write_bit(1)
            elif ch == "0":
                self.write_bit(0)
            else:
                raise ValueError(f"invalid bit char {ch!r}")

    def to_bytes(self) -> bytes:
        # Flush partial byte (zero-pad LSBs)
        if self._nbits:
            self._cur <<= 8 - self._nbits
            self._bytes.append(self._cur & 0xFF)
            self._cur = 0
            self._nbits = 0
        raw = bytes(self._bytes)
        # Pad to 4-byte alignment (jPSXdec LITTLE_ENDIAN_SHORT_ORDER)
        pad = (-len(raw)) % 4
        if pad:
            raw = raw + bytes(pad)
        # Byte-swap within each short: mapped[i^1] = raw[i]
        out = bytearray(len(raw))
        for i, b in enumerate(raw):
            out[i ^ 1] = b
        return bytes(out)


def _clamp_s10(v: int) -> int:
    return max(-512, min(511, v))


def _rgb_to_ycbcr_planes(
    rgb: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """RGB uint8 HxWx3 → Y, Cb, Cr float planes (PSX/JFIF-centered, ~−128..127)."""
    r = rgb[:, :, 0].astype(np.float64) - 128.0
    g = rgb[:, :, 1].astype(np.float64) - 128.0
    b = rgb[:, :, 2].astype(np.float64) - 128.0
    y = 0.299 * r + 0.587 * g + 0.114 * b
    cb = -0.16871 * r - 0.33130 * g + 0.5 * b
    cr = 0.5 * r - 0.4187 * g - 0.0813 * b
    return y, cb, cr


def _downsample_chroma(plane: np.ndarray) -> np.ndarray:
    """2×2 box average → half resolution (4:2:0)."""
    h, w = plane.shape
    # pad to even
    if h % 2 or w % 2:
        plane = np.pad(
            plane,
            ((0, h % 2), (0, w % 2)),
            mode="edge",
        )
    return (
        plane[0::2, 0::2]
        + plane[0::2, 1::2]
        + plane[1::2, 0::2]
        + plane[1::2, 1::2]
    ) * 0.25


def _quantize_block(dct: np.ndarray, q_scale: int) -> list[int]:
    """DCT 8×8 → zigzag coefficient list (DC + up to 63 AC, trailing zeros dropped later)."""
    # Natural-order quantised levels
    levels = np.zeros(64, dtype=np.int32)
    levels[0] = _clamp_s10(int(round(dct[0, 0] / QUANT[0, 0])))
    for scan in range(1, 64):
        yx = _UNZIGZAG[scan]
        y, x = divmod(yx, 8)
        # AC: round(dct * 8 / (Q * q_scale))
        q = QUANT[y, x]
        levels[scan] = _clamp_s10(
            int(round(dct[y, x] * 8.0 / (q * q_scale)))
        )
    return levels.tolist()


def _encode_ac_vlc(bw: BitWriter, run: int, level: int) -> None:
    code = _AC_ENCODE.get((run, abs(level)))
    if code is not None:
        # Tabulated MPEG-1 VLC; sign bit follows the prefix
        bw.write_bitstring(code)
        bw.write_bit(1 if level < 0 else 0)
        return
    # Escape: 000001 + 6-bit run + 10-bit signed AC
    if run < 0 or run > 63:
        raise ValueError(f"AC run out of range: {run}")
    bw.write_bitstring("000001")
    bw.write_bits(run, 6)
    bw.write_bits(_clamp_s10(level), 10)


def _encode_block(bw: BitWriter, spatial: np.ndarray, q_scale: int) -> int:
    """Encode one 8×8 spatial block. Returns number of MDEC codes written."""
    dct = _psx_dct(spatial)
    levels = _quantize_block(dct, q_scale)
    # DC: 10-bit signed
    bw.write_bits(levels[0], 10)
    n_codes = 1
    run = 0
    for scan in range(1, 64):
        lev = levels[scan]
        if lev == 0:
            run += 1
            continue
        _encode_ac_vlc(bw, run, lev)
        n_codes += 1
        run = 0
    # EOB
    bw.write_bitstring("10")
    n_codes += 1
    return n_codes


def _encode_macroblock(
    bw: BitWriter, y: np.ndarray, cb: np.ndarray, cr: np.ndarray, q_scale: int
) -> int:
    """Encode one 16×16 MB from full-res Y and half-res chroma planes.

    ``y`` is 16×16, ``cb``/``cr`` are the corresponding 8×8 chroma samples.
    """
    n = 0
    n += _encode_block(bw, cr, q_scale)
    n += _encode_block(bw, cb, q_scale)
    n += _encode_block(bw, y[0:8, 0:8], q_scale)
    n += _encode_block(bw, y[0:8, 8:16], q_scale)
    n += _encode_block(bw, y[8:16, 0:8], q_scale)
    n += _encode_block(bw, y[8:16, 8:16], q_scale)
    return n


def _half_ceiling_32(mdec_code_count: int) -> int:
    """``((count+1)//2`` rounded up to multiple of 32) — jPSXdec ``Calc``."""
    return (((mdec_code_count + 1) // 2) + 31) & ~31


def encode_bs_v2(
    image: Image.Image | np.ndarray,
    *,
    quant_scale: int = 2,
    width: int | None = None,
    height: int | None = None,
) -> bytes:
    """Encode an RGB image to a BS version-2 frame.

    Parameters
    ----------
    image:
        PIL Image or HxWx3 uint8 array.
    quant_scale:
        Frame quantisation scale (1–63). Higher = smaller / lower quality.
        PE2 retail backgrounds commonly use 1–4.
    width / height:
        Output dimensions (default: image size, rounded up to 16 for coding;
        stored crop follows the source size).

    Returns
    -------
    bytes
        Complete BS v2 blob (8-byte header + VLC + EOF padding).
    """
    if quant_scale < 1 or quant_scale > 63:
        raise ValueError(f"quant_scale must be 1..63, got {quant_scale}")

    if isinstance(image, Image.Image):
        img = image.convert("RGB")
        rgb = np.asarray(img, dtype=np.uint8)
    else:
        rgb = np.asarray(image, dtype=np.uint8)
        if rgb.ndim != 3 or rgb.shape[2] != 3:
            raise ValueError("image array must be HxWx3")

    src_h, src_w = rgb.shape[:2]
    width = src_w if width is None else width
    height = src_h if height is None else height

    w_mb = (width + 15) // 16
    h_mb = (height + 15) // 16
    pad_w, pad_h = w_mb * 16, h_mb * 16

    # Pad to macroblock grid (edge replicate)
    if src_h != pad_h or src_w != pad_w:
        canvas = np.zeros((pad_h, pad_w, 3), dtype=np.uint8)
        canvas[: min(src_h, pad_h), : min(src_w, pad_w)] = rgb[
            : min(src_h, pad_h), : min(src_w, pad_w)
        ]
        if src_w < pad_w and src_h > 0:
            canvas[:src_h, src_w:] = rgb[:src_h, src_w - 1 : src_w]
        if src_h < pad_h and src_w > 0:
            canvas[src_h:, :src_w] = rgb[src_h - 1 : src_h, :src_w]
        if src_h < pad_h and src_w < pad_w:
            canvas[src_h:, src_w:] = rgb[src_h - 1, src_w - 1]
        rgb = canvas

    y_plane, cb_full, cr_full = _rgb_to_ycbcr_planes(rgb)
    cb_plane = _downsample_chroma(cb_full)
    cr_plane = _downsample_chroma(cr_full)

    bw = BitWriter()
    n_codes = 0
    # Column-major macroblocks
    for col in range(w_mb):
        for row in range(h_mb):
            y0, x0 = row * 16, col * 16
            cy, cx = row * 8, col * 8
            n_codes += _encode_macroblock(
                bw,
                y_plane[y0 : y0 + 16, x0 : x0 + 16],
                cb_plane[cy : cy + 8, cx : cx + 8],
                cr_plane[cy : cy + 8, cx : cx + 8],
                quant_scale,
            )

    bw.write_bitstring(_EOF_PADDING_BITS)
    body = bw.to_bytes()
    header = struct.pack(
        "<4H",
        _half_ceiling_32(n_codes),
        BS_MAGIC,
        quant_scale,
        2,
    )
    return header + body


def encode_bs_file(
    image: Image.Image | np.ndarray,
    path: Path | str,
    *,
    quant_scale: int = 2,
) -> bytes:
    """Encode image to BS v2 and write ``path``. Returns the blob."""
    data = encode_bs_v2(image, quant_scale=quant_scale)
    Path(path).write_bytes(data)
    return data


def info_from_json_bs(meta: dict) -> tuple[int, int, int]:
    """Return ``(width, height, quant_scale)`` from ``*.bs.json`` meta."""
    w = int(meta.get("width", DEFAULT_WIDTH))
    h = int(meta.get("height", DEFAULT_HEIGHT))
    q = int(meta.get("quant_scale", 2))
    return w, h, q
