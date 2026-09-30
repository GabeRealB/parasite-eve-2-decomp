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

/// 0x14-byte SPRT source record. `GpSprtRec.field_0` is an array of these.
/// `Gp_LinkSprtCmd` / `Gp_EmitSprts` index from `SpriteBatch.firstSprite` for
/// `SpriteBatch.spriteCount` entries. `otz` is the OT depth. `Gp_EmitSprts` copies the
/// remaining fields into a merged `DR_TPAGE`+`SPRT` in `gGpuPrimCursor`.
/// `flags` bit 0 skips the RGB copy (shade-tex); the byte is OR'd into
/// the SPRT code.
typedef struct _GpSprtElem {
    /* 0x00 */ u16 tpage;
    /* 0x02 */ u16 clut;
    /* 0x04 */ union {
        struct {
            s16 w;
            s16 h;
        } fields;
        u32 packed;
    } size;
    /* 0x08 */ s16 x0;
    /* 0x0A */ s16 y0;
    /* 0x0C */ u16 otz;
    /* 0x0E */ union {
        struct {
            u8 u0;
            u8 v0;
        } fields;
        u16 packed;
    } uv;
    /* 0x10 */ u8 r0;
    /* 0x11 */ u8 g0;
    /* 0x12 */ u8 b0;
    /* 0x13 */ u8 flags;
} GpSprtElem;
STATIC_ASSERT_SIZEOF(GpSprtElem, 0x14);

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
        GpSprtElem* elements;
        // Empty lists retain the command-table address here; no sprite is read.
        SpriteBatch* empty;
    } field_0;
    /* 0x4 */ SpriteBatch*   field_4;
    /* 0x8 */ GpDrawAreaRec* field_8;
} GpSprtRec;
STATIC_ASSERT_SIZEOF(GpSprtRec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpSprtRec*`, indexed
/// 1-based by `GameSession.at4.loc.area` / `GameLocationKey.area`.
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
