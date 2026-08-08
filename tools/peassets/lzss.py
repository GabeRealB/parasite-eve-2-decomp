"""LZSS codec used by Parasite Eve 2 room packages (.pe2pkg).

Decoder is based on md_hyena's implementation (as used historically in
extract.py), with defensive bounds checks for padded chunk tails.

The encoder emits a valid stream for newly edited assets (literal-only).
Pack prefers the original raw compressed blob when the decoded payload
is unchanged.
"""

from __future__ import annotations


def decode_lzss(stream: bytes) -> bytes:
    # Based on the lzss decoder by md_hyena
    DICT_SIZE = 256
    OFFSET_BITS = 8
    STRING_LEN_BITS = 4
    LIT_SIZE = 8
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


def encode_lzss(data: bytes) -> bytes:
    """Encode payload as an LZSS bitstream (literal-only, always valid)."""
    bits: list[int] = []

    def put_bit(bit: int) -> None:
        bits.append(1 if bit else 0)

    def put_bits(value: int, n: int) -> None:
        for i in range(n - 1, -1, -1):
            put_bit((value >> i) & 1)

    for byte in data:
        put_bit(1)  # literal
        put_bits(byte & 0xFF, 8)

    # End of stream: match flag + offset 0
    put_bit(0)
    put_bits(0, 8)

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
