#ifndef GAMEPLAY_SPRITES_H
#define GAMEPLAY_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

/// 8-byte command record. `GpSprtRec.field_4` points at a 0xFFFF-terminated
/// list of these. `Gp_ViewSprtCmdEmpty` returns whether `field_2` is zero; when it
/// is, `Gp_LinkViewSprts` skips the first record, otherwise it clears
/// `gDisplayState.at100.flags.imageSource`. `field_0` is the start index into
/// `GpSprtRec.field_0`; `field_2` is the count. `field_4` nonzero skips
/// OT-linking each prim. `field_5` nonzero skips `Gp_LinkSprtCmd` and
/// `Gp_SetSprtShadeBits`.
typedef struct _GpSprtCmd {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ u8   field_5;
    /* 0x6 */ byte pad_6[2];
} GpSprtCmd;
STATIC_ASSERT_SIZEOF(GpSprtCmd, 8);

/// 0x14-byte SPRT source record. `GpSprtRec.field_0` is an array of these.
/// `Gp_LinkSprtCmd` / `Gp_EmitSprts` index from `GpSprtCmd.field_0` for
/// `field_2` entries. `otz` is the OT depth. `Gp_EmitSprts` copies the
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
        GpSprtCmd* empty;
    } field_0;
    /* 0x4 */ GpSprtCmd*     field_4;
    /* 0x8 */ GpDrawAreaRec* field_8;
} GpSprtRec;
STATIC_ASSERT_SIZEOF(GpSprtRec, 0xC);

/// Per-stage wrapper. `field_0` is an array of `GpSprtRec*`, indexed
/// 1-based by `GameSession.at4.loc.area` / `GpAreaKey.area`.
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
