#ifndef GAMEPLAY_SPRITES_H
#define GAMEPLAY_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

/// Sentinel in the 16-bit `SpriteBatch.firstSprite` index ending a view's batch list.
///
/// The terminal record contributes no sprites, regardless of `spriteCount`;
/// a zero-count nonterminal record does not end the list. The first record's
/// count still selects background presentation, even when that record is terminal.
enum { SPRITE_BATCH_END = 0xFFFF };

/// A contiguous range of source sprites in a view's drawing list.
///
/// Room tables own these mutable, eight-byte records. `GpSprtRec.field_4`
/// points to a list ending with `firstSprite == SPRITE_BATCH_END`. Each
/// nonterminal range indexes that view's `GpSprtRec.field_0.elements`; its
/// start plus count must fit the source array.
///
/// The first record's count also selects background presentation: zero requests
/// decoded image strips and is skipped by cached-sprite linking; nonzero
/// suppresses the strips. This test applies even to a terminal first record.
/// Hidden ranges retain their positions in the cached packet buffers, while
/// `skipCachedPackets` ranges do not advance the initialization/link/shade
/// cursors. Allocation still reserves space for all nonterminal counts.
typedef struct {
    u16  firstSprite;       // Zero-based source index, or SPRITE_BATCH_END
    u16  spriteCount;       // Number of source elements; first record also selects the background
    u8   hidden;            // Cached sprites (0 linked for drawing, nonzero hidden)
    u8   skipCachedPackets; // Cached packet processing (0 included, nonzero excluded)
    byte field_6[2];        // Serialized bytes; role and subdivision unproven
} SpriteBatch;
STATIC_ASSERT_SIZEOF(SpriteBatch, 8);

/// GPU command bits carried by `SpriteSource.codeFlags`.
///
/// Zero selects opaque, colour-modulated emission; the bits may be combined.
/// Raw texture emission ignores RGB and skips copying it into the packet.
enum {
    SPRITE_SOURCE_RAW_TEXTURE      = 0x01,
    SPRITE_SOURCE_SEMI_TRANSPARENT = 0x02,
};

/// Mutable texture rectangle and sorting depth for a room-view sprite.
///
/// Room overlays own arrays referenced by `GpSprtRec.field_0.elements`;
/// `SpriteBatch` selects ranges in elements, with no sentinel in the source
/// array itself. The selected overlay and each range must remain valid while
/// allocating packets or linking them for drawing.
///
/// Position is the rectangle's upper-left corner in pixels relative to the
/// drawing origin (normally the view centre). Depth is an unscaled sorting
/// value: its OT slot is `((u32)depth << gDisplayState.otDepthShift) >> 4 & 0x3FF`.
///
/// Packet allocation snapshots texture, position, size and code flags into both
/// frame buffers; cached linking reads depth from the source each frame. Cached
/// packets start with raw texture enabled and do not copy source RGB. Direct
/// emission copies RGB unless `SPRITE_SOURCE_RAW_TEXTURE` is set.
/// The SDK-compatible XY and RGB member names support aligned whole-word reads;
/// an RGB read includes `codeFlags` as its high byte.
typedef struct {
    u16 tpage; // Encoded GPU texture page and blend mode
    u16 clut;  // Encoded GPU colour lookup-table address
    union {
        struct {
            s16 w;  // Rectangle width in pixels, also the sampled texture width
            s16 h;  // Rectangle height in pixels, also the sampled texture height
        } fields;   // Individual dimensions in the SDK's signed-halfword representation
        u32 packed; // Width in the low halfword, height in the high halfword
    } size;         // Unscaled dimensions copied together into the SPRT packet
    s16 x0;         // Upper-left X in pixels relative to the drawing origin
    s16 y0;         // Upper-left Y in pixels relative to the drawing origin
    u16 depth;      // Unscaled OT sorting depth, quantized and masked when linking
    union {
        struct {
            u8 u0;  // Left texture coordinate in texels within the texture page
            u8 v0;  // Top texture coordinate in texels within the texture page
        } fields;   // Individual texture coordinates
        u16 packed; // U in the low byte, V in the high byte
    } uv;           // Texture origin copied together into the SPRT packet
    u8 r0;          // Red modulation (128 neutral), used by colour-modulated direct emission
    u8 g0;          // Green modulation (128 neutral), used by colour-modulated direct emission
    u8 b0;          // Blue modulation (128 neutral), used by colour-modulated direct emission
    u8 codeFlags;   // GPU code bits (0 opaque modulation, bit 0 raw texture, bit 1 semitransparent)
} SpriteSource;
STATIC_ASSERT_SIZEOF(SpriteSource, 0x14);

/// 10-byte draw-area record. `GpSprtRec.field_8` points at a list terminated
/// by depth 0xFFFF. Each rectangle clips the view sprites up to its OT depth.
typedef struct _GpDrawAreaRec {
    /* 0x0 */ RECT rect;
    /* 0x8 */ u16  depth;
} GpDrawAreaRec;
STATIC_ASSERT_SIZEOF(GpDrawAreaRec, 0xA);

/// 12-byte per-view record in tables pointed to by `Gp_SprtTables`.
/// Indexed 1-based by the `Gp_ViewIndexTables` camera / view byte.
/// `Gp_GetViewSprtExtra` returns `field_8`. `Gp_ViewSprtCmdEmpty` reads `field_4`.
typedef struct _GpSprtRec {
    /* 0x0 */ union {
        SpriteSource* elements;
        // Empty lists retain the command-table address here; no sprite is read.
        SpriteBatch* empty;
    } field_0;
    /* 0x4 */ SpriteBatch*   field_4;
    /* 0x8 */ GpDrawAreaRec* field_8;
} GpSprtRec;
STATIC_ASSERT_SIZEOF(GpSprtRec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpSprtRec*`, indexed
/// 1-based by `GameSession.location.loc.area` / `GameLocationKey.area`.
typedef struct _GpSprtTbl {
    /* 0x0 */ GpSprtRec** field_0;
} GpSprtTbl;

/// Halfword UV and word size transfers used when building GPU sprites.
typedef union GpSpritePacket {
    SPRT fields;
    struct {
        u32 tag;
        u32 color;
        u32 position;
        u16 uv;
        u16 clut;
        u32 size;
    } packed;
} GpSpritePacket;
STATIC_ASSERT_SIZEOF(GpSpritePacket, 0x14);

/// Merged `DR_TPAGE` + `SPRT` (0x1C) written into `gGpuPrimCursor` by
/// `Gp_EmitSprts`. `MargePrim` concatenates the tpage packet onto the
/// sprite so they share one OT entry.
typedef struct _GpTpageSprt {
    /* 0x00 */ DR_TPAGE       tpage;
    /* 0x08 */ GpSpritePacket sprt;
} GpTpageSprt;
STATIC_ASSERT_SIZEOF(GpTpageSprt, 0x1C);

#endif // GAMEPLAY_SPRITES_H
