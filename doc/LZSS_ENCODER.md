# Parasite Eve 2 — the LZSS encoder

The bitstream and its decoders are in [`ASSET_FORMATS.md`](ASSET_FORMATS.md) §4. This is the encoder that made it.

## The original

The streams were made by Mark Nelson's LZSS from *The Data Compression Book* (1991), built with
`INDEX_BIT_COUNT` 8 and `LENGTH_BIT_COUNT` 4:

| Parameter | Value |
|---|---|
| Window | 256 bytes, the first byte at position 1 |
| Lookahead | `RAW_LOOK_AHEAD_SIZE` + `BREAK_EVEN` = 16 + 1 = 17 |
| `BREAK_EVEN` | (1 + 8 + 4) / 9 = 1: a reference is 2 to 17 bytes, shorter is a literal |
| `END_OF_STREAM`, `UNUSED` | both 0 |

`formats::lzss::encode` (`editor/crates/formats/src/lzss.rs`) is that encoder. It makes every stream of both
discs' stage containers byte for byte: 448 packages, 13 364 image strips and 414 palettes, each counted once.

```text
cargo run --release -p formats --example lzss_identity -- ../rom/USA
```

The example takes each stream as stored through its end token - a package chunk's payload, each strip of an image
chunk after its work table (zero bytes between strips), a palette chunk's payload after its 16-byte header -,
decodes it, encodes it again and compares; it exits non-zero if any differs.

## How it chooses

- **Window**: as each byte enters the lookahead, the string 239 bytes back (the window less the lookahead) is
  deleted, so no reference reaches farther.
- **Search**: one binary tree of the 17-byte strings at each window position, under a single root. Inserting the
  string at the current position walks the tree; the match is the **last** node at the longest length on that
  path (`if (i >= match_length)`), so among equally long sources it takes the key's neighbour in sorted order that
  lies deeper in the tree.
- **Equal strings**: a string equal to one in the tree for all 17 bytes takes its place.
- **Deletion**: a node with two children is replaced by its predecessor, the largest of its smaller subtree.
- **`UNUSED` = 0**: position 0 is never inserted nor searched from, so a literal is always emitted there. `tree[0]`
  receives the writes meant for an absent child, and deleting position 0, once a lap of the window, acts on them:
  it cuts one node's smaller subtree off the tree. That loses matches the tree would otherwise find; it never makes
  a wrong one.
- **End**: as the input runs out the lookahead shrinks and a match is capped at what is left; the stream ends with
  a reference at offset 0.

## The Python encoders

`tools/peassets/lzss.py` (`encode_lzss`), `lzss_clut.py` and `lzss_cascading.py` are not this encoder. They make
valid streams that decode to the same bytes, but not the stored ones.
