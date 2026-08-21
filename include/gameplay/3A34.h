#ifndef GAMEPLAY_3A34_H
#define GAMEPLAY_3A34_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/268.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"

/// Singly-linked node unlinked by `func_800DAB38` / linked by `func_800DABEC`.
/// `field_6 == 1` means the node is on the `D_80115268` list.
/// `func_800DAC54` returns a 2-bit mask of `D_80115760[]` slots whose
/// `actor->field_90C` is this node.
/// `func_800DACAC` assigns the node to `D_80115760[0]->actor->field_90C`,
/// clears the previous node's `field_5`, sets this node's `field_5 = 1`,
/// and clears `field_4` bit 0.
/// `func_800DACF8` is the inverse: it nulls any `D_80115760[]` slot whose
/// `field_90C` is this node, clears `field_5`, and sets `field_4` bit 0.
typedef struct _GpLinkNode {
    /* 0x00 */ struct _GpLinkNode* next;
    /* 0x04 */ u8                  field_4;
    /* 0x05 */ u8                  field_5;
    /* 0x06 */ u8                  field_6;
} GpLinkNode;
STATIC_ASSERT_SIZEOF(GpLinkNode, 0x8);

/// Linked object used as a list head/node by the 3A34 pair/filter helpers.
/// `next` is at 0x0, `prev` at 0x4, and `flags` at 0x1E. Bit 0x8 means the
/// node is on the `D_8010FA8C` list (set by `func_800E15AC`, cleared by
/// `func_800E1638`, keeping bits 0x7). `func_8010C980` fills `field_8` /
/// `field_C` / the 0x10 SVECTOR / `field_18` / `field_1C` and ORs `flags`
/// with 0x8000 after linking. `field_8` is a `GsCOORDINATE2*`; `func_800E08CC`
/// applies `workm` to the 0x10 SVECTOR and adds `workm.t` into a `VECTOR3`.
/// `func_800E1380` treats `field_C` as a `GpActorD4Rec*` whose `field_14`
/// is the `GpRec18` table walked for the nearest matching slot.
/// `func_800DEC80` uses that same table: flag `0x800` copies the first
/// occupied slot's `field_8/A/C` (unless `arg3 != 0`), flag `0x400`
/// copies the first occupied slot whose `field_4` high 16 bits equal
/// `0x10`. Remaining of two world points come from `field_C` as
/// `SVECTOR[2]` plus this object's 0x10 SVECTOR, rotated by `workm`.
/// `func_800DBA20` selects that table from `flags & 7`: 1 is `field_C`
/// itself, 2 is `((GpObj*)field_C)->field_C`, 3 is `GpActorD4Rec.field_14`,
/// 4 is `((GpObj*)field_C)->field_8`.
/// Embedded as 0x20-byte nodes in `GameActor`
/// (`field_AC` / `field_CC` / `field_EC` / `field_10C` / `field_12C`).
/// Full object size is not known for other list users.
typedef struct _GpObj {
    /* 0x00 */ struct _GpObj* next;
    /* 0x04 */ struct _GpObj* prev;
    /* 0x08 */ void*          field_8;
    /* 0x0C */ GpRec18*       field_C;
    /* 0x10 */ s16            field_10;
    /* 0x12 */ s16            field_12;
    /* 0x14 */ s16            field_14;
    /* 0x16 */ byte           pad_16[2];
    /* 0x18 */ s32            field_18;
    /* 0x1C */ s16            field_1C;
    /* 0x1E */ u16            flags;
} GpObj;
STATIC_ASSERT_SIZEOF(GpObj, 0x20);

/// 4-byte table entry packed by `func_800E2C40` / `func_800E2BF8` as
/// `(field_0 & 0xFFF) | ((field_2 & 0xF) << 12) | 0x40000`.
typedef struct _GpU16Pair {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpU16Pair;
STATIC_ASSERT_SIZEOF(GpU16Pair, 0x4);

/// Pair-dispatch callback from `D_8010FA38`. `kind` is `GpU16Pair.field_0`.
typedef void (*GpPairFn)(GpObj* a, GpObj* b, s32 kind);

/// 4-byte table entry at `D_80114028`. `func_800E2CD4(idx, 0)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec4 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpRec4;
STATIC_ASSERT_SIZEOF(GpRec4, 0x4);

/// 6-byte table entry at `D_80114054`. `func_800E2CD4(idx, 1)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec6 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpRec6;
STATIC_ASSERT_SIZEOF(GpRec6, 0x6);

/// 10-byte table entry at `D_80113390`. Selected when the id's 0x8000 bit
/// is clear. `func_800E2D3C` / `func_800E2D90` / `func_800E3194` return
/// `field_4` / `field_6` / `field_8` for index `id & 0x7F`.
typedef struct _GpRec10 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
} GpRec10;
STATIC_ASSERT_SIZEOF(GpRec10, 0xA);

/// 12-byte ranked slot inserted by `func_800D9B9C`. That helper walks a
/// 4-entry table (indices 0..3) from `arg4` toward 0 and keeps slots in
/// descending `field_4` order. A non-positive key is ignored. When the
/// new key is larger than slot `arg4`, that slot is copied to `arg4+1`
/// (if `arg4 < 3`) and the search recurses; otherwise the record is
/// stored at `arg4+1` when there is room.
typedef struct _GpRec12 {
    /* 0x0 */ s32 field_0; // payload from arg2
    /* 0x4 */ s32 field_4; // descending sort key (arg1)
    /* 0x8 */ s32 field_8; // payload from arg3
} GpRec12;
STATIC_ASSERT_SIZEOF(GpRec12, 0xC);

/// 12-byte location-keyed grant record walked by `func_800DB128`.
/// `field_0` is `(stage << 24) | (area << 16) | (sub << 8)` from
/// `GameSessionFrom4.field_3` / `field_2` / `field_5`, or `-1` to end
/// the list. `items[0..3]` are item ids granted with `func_800BAD08`
/// when `func_800B7420` is 0; a 0 slot is skipped. `items[3]` also
/// requires `func_800B9D80(0x80000)`.
typedef struct _GpGiveRec {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 items[4];
} GpGiveRec;
STATIC_ASSERT_SIZEOF(GpGiveRec, 0xC);

/// 16-byte VRAM upload record walked by `func_800DB31C`. `field_0 == 0`
/// uploads `rect` / `data` via `LoadImage`; non-zero ends the walk.
/// `func_800DB28C` fills `rect` from a source RECT plus the TMD tpage at
/// `TmdObject.field_24` (`x = tpage * 64 + (src.x + 1) / 2 + 0x180`,
/// `y = src.y + 0x100`).
typedef struct _GpImgRec {
    /* 0x0 */ u16     field_0;
    /* 0x2 */ u16     pad_2;
    /* 0x4 */ RECT    rect;
    /* 0xC */ u_long* data;
} GpImgRec;
STATIC_ASSERT_SIZEOF(GpImgRec, 0x10);

/// 16-byte table entry at `D_8011398C`. Selected when the id's 0x8000 bit
/// is set. `func_800E2D3C` / `func_800E2D90` / `func_800E3194` return
/// `field[5]` / `field[6]` / `field[7]` for index `id & 0x7F`.
/// `func_800D50D4` indexes `field[arg1]` after remapping a packed id as
/// `((id>>4&3)*3 + (id>>2&3))*3 + (id&3)`.
typedef struct _GpRec16 {
    /* 0x0 */ u16 field[8];
} GpRec16;
STATIC_ASSERT_SIZEOF(GpRec16, 0x10);

/// 20-byte damage-scale row at `D_80113EF0`. Indexed by `D_8011541B`.
/// `func_800E2438` adds `D_80113F54[hp / 10] * 2` onto the row base and
/// then loads `field_A` (arg3 == 0, player HP) or `field_0` (arg3 != 0,
/// `Mc_SaveData.field_6C8`).
typedef struct _GpDmgRow {
    /* 0x00 */ u16 field_0[5];
    /* 0x0A */ u16 field_A[5];
} GpDmgRow;
STATIC_ASSERT_SIZEOF(GpDmgRow, 0x14);

/// Overlay of one column inside a `GpDmgRow` (`col * 2` from the row base).
typedef struct _GpDmgSlot {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 pad_2[4];
    /* 0xA */ u16 field_A;
} GpDmgSlot;
STATIC_ASSERT_SIZEOF(GpDmgSlot, 0xC);

/// Table source pointed to by `GpObj50.field_50`. `field_0` is the
/// `GpU16Pair` array packed by `func_800E2BF8`. Nearby helpers also
/// load bytes at +0xB / +0xD / +0xE of this object (`GpPairSrcE`).
typedef struct _GpPairSrc {
    /* 0x00 */ GpU16Pair* field_0;
} GpPairSrc;
STATIC_ASSERT_SIZEOF(GpPairSrc, 0x4);

/// Wider view of the object pointed to by `GpObj50.field_50`.
/// `func_800E2EC4` loads `field_4`; `func_800DB558` adds `field_6` /
/// `field_8` / `field_A` into `D_801153F0.field_8` / `field_C` /
/// `field_10`. Nearby helpers also load bytes at +0xB / +0xC / +0xD /
/// +0xE (`func_800E3084` loads `field_C`; `func_800E2DE4` loads
/// `field_D`; `func_800E2F7C` loads `field_E`). Trailing pad keeps
/// 4-byte alignment.
typedef struct _GpPairSrcE {
    /* 0x00 */ GpU16Pair* field_0;
    /* 0x04 */ u16        field_4;
    /* 0x06 */ u16        field_6;
    /* 0x08 */ u16        field_8;
    /* 0x0A */ u8         field_A;
    /* 0x0B */ u8         field_B;
    /* 0x0C */ u8         field_C;
    /* 0x0D */ u8         field_D;
    /* 0x0E */ u8         field_E;
    /* 0x0F */ byte       pad_F;
} GpPairSrcE;
STATIC_ASSERT_SIZEOF(GpPairSrcE, 0x10);

/// Object whose pointer at 0x50 is a `GpPairSrc*` used by `func_800E2BF8`.
/// Same object family as `GpObj4C` (flags at 0x4C).
typedef struct _GpObj50 {
    /* 0x00 */ byte       pad_0[0x50];
    /* 0x50 */ GpPairSrc* field_50;
} GpObj50;
STATIC_ASSERT_SIZEOF(GpObj50, 0x54);

/// 0x18-byte slot in the table at `GpObj54.field_54`. Occupied when the
/// first word's low 2 bits equal 1. `func_800E1C58` claims the first free
/// slot: payload in `field_4`, clears `field_2` / `field_8` / `field_A` /
/// `field_C` / `field_10` / `field_12` / `field_14`, ORs bit 0 into
/// `field_0`, and increments `D_801153F0.field_5`.
typedef struct _GpSlot18 {
    /* 0x00 */ u16   field_0;
    /* 0x02 */ s16   field_2;
    /* 0x04 */ void* field_4;
    /* 0x08 */ s16   field_8;
    /* 0x0A */ s16   field_A;
    /* 0x0C */ s16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ s16   field_10;
    /* 0x12 */ s16   field_12;
    /* 0x14 */ s16   field_14;
    /* 0x16 */ s16   field_16;
} GpSlot18;
STATIC_ASSERT_SIZEOF(GpSlot18, 0x18);

/// Sparse overlay whose pointer at 0x54 is a `GpSlot18` table walked by
/// `func_800E1C58`. Same object family as `GpObj50` / `GpObj4C`.
typedef struct _GpObj54 {
    /* 0x00 */ byte      pad_0[0x54];
    /* 0x54 */ GpSlot18* field_54;
} GpObj54;
STATIC_ASSERT_SIZEOF(GpObj54, 0x58);

/// 8-byte nested table entry pointed to by `GpCbA4Rec.field_4`.
/// Entry 0's `field_0` is the max valid index. `func_800D957C` returns
/// `&table[GameSessionFrom4.field_0]` when that index is in range,
/// otherwise `(GpCbA4Vec*)&D_8010F9E4`. `func_800D7A9C` reads
/// `field_0` / `field_2` / `field_4` as signed XYZ minimums.
typedef struct _GpCbA4Vec {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} GpCbA4Vec;
STATIC_ASSERT_SIZEOF(GpCbA4Vec, 8);

/// 8-byte record in tables pointed to by `D_8010CBA4`. Indexed 1-based
/// by `GameSessionFrom4.field_1`. `func_800D9C64` returns the record
/// (or NULL). `func_800D9654` returns `field_0` as a `GpCbA4Set*` (or 0).
/// `func_800D957C` walks `field_4` as a nested `GpCbA4Vec` table, falling
/// back to `D_8010F9E4`.
typedef struct _GpCbA4Rec {
    /* 0x0 */ s32        field_0;
    /* 0x4 */ GpCbA4Vec* field_4;
} GpCbA4Rec;
STATIC_ASSERT_SIZEOF(GpCbA4Rec, 8);

/// 0x58-byte coordinate object in `GpCbA4Set.arr58`. `func_800D6B20`
/// parents `coord.sub` to `D_80070F10` and clears `coord.flg`.
typedef struct _GpCoord58 {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[8];
} GpCoord58;
STATIC_ASSERT_SIZEOF(GpCoord58, 0x58);

/// 0x60-byte coordinate object in `GpCbA4Set.arr60`. Same `coord.sub` /
/// `coord.flg` init as `GpCoord58`. Same size as `GpCoordTail`.
typedef struct _GpCoord60 {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[0x10];
} GpCoord60;
STATIC_ASSERT_SIZEOF(GpCoord60, 0x60);

/// 0x6C-byte coordinate object in `GpCbA4Set.arr6C`. `func_800D6B20`
/// parents `coord.sub` to `D_80070F10`, builds `coord.coord` as an
/// orthonormal basis from `dir` (and a perpendicular scratch vector),
/// and clears `coord.flg`.
typedef struct _GpCoord6C {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[8];
    /* 0x58 */ SVECTOR       dir;
    /* 0x60 */ byte          pad_60[0xC];
} GpCoord6C;
STATIC_ASSERT_SIZEOF(GpCoord6C, 0x6C);

/// Room coordinate tables returned by `func_800D9654` (`GpCbA4Rec.field_0`).
/// `func_800D6B20` parents each array to `D_80070F10` on first run, then
/// updates them every frame via `func_80098F58` / `func_80098F98`.
typedef struct _GpCbA4Set {
    /* 0x00 */ s32        n58;
    /* 0x04 */ GpCoord58* arr58;
    /* 0x08 */ s32        n60;
    /* 0x0C */ GpCoord60* arr60;
    /* 0x10 */ s32        n6C;
    /* 0x14 */ GpCoord6C* arr6C;
} GpCbA4Set;
STATIC_ASSERT_SIZEOF(GpCbA4Set, 0x18);

/// 0x6C-byte walk overlay of `GpCoord6C` starting at `dir`. `func_800D6B20`
/// increments this by one object per loop.
typedef struct _Gp6CDirWalk {
    /* 0x00 */ SVECTOR dir;
    /* 0x08 */ byte    pad[0x64];
} Gp6CDirWalk;
STATIC_ASSERT_SIZEOF(Gp6CDirWalk, 0x6C);

/// 0x6C-byte walk overlay of `GpCoord6C` starting at `coord.coord`.
typedef struct _Gp6CMatWalk {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ byte   pad[0x4C];
} Gp6CMatWalk;
STATIC_ASSERT_SIZEOF(Gp6CMatWalk, 0x6C);

/// Overlay of `GpCoord6C` starting at `coord.sub`. `dir` is at +0xC, so a
/// pointer to `Gp6CDirWalk.dir` minus `OFFSET_OF(Gp6CMid, dir)` is this
/// object. `func_800D6B20` writes `sub` as `D_80070F10`.
typedef struct _Gp6CMid {
    /* 0x00 */ GsCOORDINATE2* sub;
    /* 0x04 */ byte           pad[8];
    /* 0x0C */ SVECTOR        dir;
} Gp6CMid;

/// 0x64-byte walk overlay of `GpCoord64` starting at `coord`. `func_800D6B20`
/// writes `coord.sub` while a parallel `GpCoord64*` writes `field_0`.
typedef struct _GpCoord64View {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad[0x14];
} GpCoord64View;
STATIC_ASSERT_SIZEOF(GpCoord64View, 0x64);

/// Record in the 8-entry arrays pointed to by `D_8010CBB8`.
/// `func_800E192C` copies `field_3` into `D_80115428[]`. Nearby helpers
/// also load `field_1` (`func_800DDDF8`, `func_800DE7CC`) and `field_2`
/// (`func_80105BC4` keeps a slot only when this is nonzero). `func_80105ED4`
/// loads a pointer at +0x4.
typedef struct _GpCbB8Rec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpCbB8Rec;
STATIC_ASSERT_SIZEOF(GpCbB8Rec, 4);

/// Three packed `SVECTOR3`s filled by `func_800D9C3C`. Each vector's
/// components are set to the same s16 argument.
typedef struct _GpSVec3x3 {
    /* 0x00 */ SVECTOR3 field_0;
    /* 0x06 */ SVECTOR3 field_6;
    /* 0x0C */ SVECTOR3 field_C;
} GpSVec3x3;
STATIC_ASSERT_SIZEOF(GpSVec3x3, 0x12);

/// Object whose pointers at 0x1C / 0x20 are `MATRIX*`s. `func_800D9550`
/// writes translation `t[0]/t[1]/t[2]` through `field_20`. `func_800D9D18`
/// installs the default matrices `D_80114E98` / `D_80114EB8` here (same
/// overlay as `GameActorExt`).
typedef struct _GpObj20 {
    /* 0x00 */ byte    pad_0[0x1C];
    /* 0x1C */ MATRIX* field_1C;
    /* 0x20 */ MATRIX* field_20;
} GpObj20;
STATIC_ASSERT_SIZEOF(GpObj20, 0x24);

/// Object with a `MATRIX` at 0x24 (`GsCOORDINATE2.workm` when this overlays
/// an actor coordinate). `func_800D937C` loads that matrix for RTPS.
/// `func_800D9788` returns `field_24.t[0]`. Light helpers take
/// `field_24.t` as a `VECTOR*` (`t[0]/t[1]/t[2]`).
typedef struct _GpObj38 {
    /* 0x00 */ byte   pad_0[0x24];
    /* 0x24 */ MATRIX field_24;
} GpObj38;
STATIC_ASSERT_SIZEOF(GpObj38, 0x44);

/// Sparse overlay of the same light object as `GpObj38`. `func_800D9718`
/// treats `field_44` as a room-id filter against `Game_Session->field_4`
/// (0 = any room), writes `0x1000` (GTE ONE) to `field_4A`, and returns a
/// weighted `field_50/52/54` luminance. `func_800D9794` casts to
/// `GpObj38` for `field_24.t` as a `VECTOR*`, loads `field_4A` into GTE
/// IR0, and `gte_ldsv`s the three halfwords at 0x50. `func_800D98C4` /
/// `func_800D9A30` subtract `field_24.t` from a world `VECTOR` and write
/// the negated normalized direction.
/// `func_800D9138` halves `field_18` as XYZ, compares distance² against
/// inner `field_58` and outer `field_5C` (each squared then `>> 2`), and
/// writes the attenuated luminance to `field_38.vx` (same word as
/// `GpObj38.field_24.t[0]`) plus the 12.4 scale to `field_4A`.
/// `func_800D70E4` instead subtracts a world `VECTOR3` from `field_38`
/// (same words as `GsCOORDINATE2.workm.t` / `GpObj38.field_24.t`),
/// writes the scale to `field_4A`, and returns the luminance.
/// `func_800D6E5C` is that same subtract, plus the `field_44` room-id
/// filter and an `|dx|` / `|dz|` reject against `field_5C / 2` before
/// the squared-radius test.
typedef struct _GpObj44 {
    /* 0x00 */ byte    pad_0[0x18];
    /* 0x18 */ VECTOR3 field_18;
    /* 0x24 */ byte    pad_24[0x14];
    /* 0x38 */ VECTOR3 field_38;
    /* 0x44 */ s16     field_44;
    /* 0x46 */ byte    pad_46[4];
    /* 0x4A */ s16     field_4A;
    /* 0x4C */ byte    pad_4C[4];
    /* 0x50 */ s16     field_50;
    /* 0x52 */ s16     field_52;
    /* 0x54 */ s16     field_54;
    /* 0x56 */ byte    pad_56[2];
    /* 0x58 */ s32     field_58;
    /* 0x5C */ s32     field_5C;
} GpObj44;
STATIC_ASSERT_SIZEOF(GpObj44, 0x60);

/// Sparse overlay whose signed halfword at 0x40 is added (unsigned-clamped by
/// `arg2`) into `D_801153F0.field_14` by `func_800E2C78` when
/// `(arg1 & 0x7F)` is 0x19..0x1B.
typedef struct _GpObj40 {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40;
} GpObj40;
STATIC_ASSERT_SIZEOF(GpObj40, 0x42);

/// Object whose flags byte at 0x4C is OR'd by `func_800E3008`. Nearby
/// helpers treat 0x4C as a flag field (bits 0x1, 0x2, 0x4). `field_4E`
/// packs two 2-bit modes (current in bits 0-1, previous in bits 2-3)
/// plus a high-nibble flag; `func_800D930C` rotates the current mode
/// into the previous slot and starts `field_4F` as a 0x10 blend timer.
/// `next` and signed `field_4B` are the `D_8011556C` list walked by
/// `func_800E0B08`, which clears a non-zero `field_4B`. `func_800E1BF0`
/// walks the same list and, on a pending `field_4B`, copies `field_46` /
/// `field_48` / `field_49` to its out-params and sets `D_80115424`. The
/// same node type is the `D_80115554` list walked by `func_800E1B80`: a
/// pending `field_4B` copies `field_49` into `Mc_SaveData.field_4` when
/// `field_48` matches `Game_Session->field_4`.
typedef struct _GpObj4C {
    /* 0x00 */ struct _GpObj4C* next;
    /* 0x04 */ byte             pad_4[0x42];
    /* 0x46 */ u16              field_46;
    /* 0x48 */ u8               field_48;
    /* 0x49 */ u8               field_49;
    /* 0x4A */ u8               field_4A;
    /* 0x4B */ s8               field_4B;
    /* 0x4C */ u8               field_4C;
    /* 0x4D */ byte             pad_4D;
    /* 0x4E */ u8               field_4E;
    /* 0x4F */ u8               field_4F;
} GpObj4C;
STATIC_ASSERT_SIZEOF(GpObj4C, 0x50);

/// Sparse overlay of the same object family as `GpObj4C` (flags at 0x4C).
/// `func_800E2A24` looks up a kind from `D_80113390` / `D_8011398C` (same
/// field as `func_800E2D3C`) and starts one of three effects: bit 0x1
/// (`func_800E3008`), bit 0x2 (`func_800E301C`, also `field_58` /
/// `field_5B` / `field_5D`), or bit 0x4 (`func_800E2DE4`, also
/// `field_59` / `field_5A` / `field_5C`). `func_800E301C` ORs bit 0x2
/// into `field_4C`, clears `field_58` / `field_5B`, and writes `field_5D`
/// from `D_80114C08.field_0 % 10` when the id has the 0x8000 bit and low
/// 6 bits != 0x31. `func_800E3084` compares `field_58` against
/// `field_50->field_C * D_80113D30[field_5D] / 100` and ticks `field_5B`.
/// Trailing pad keeps pointer alignment; full object size is not known yet.
typedef struct _GpObj5D {
    /* 0x00 */ byte        pad_0[0x4C];
    /* 0x4C */ u8          field_4C;
    /* 0x4D */ byte        pad_4D[3];
    /* 0x50 */ GpPairSrcE* field_50;
    /* 0x54 */ byte        pad_54[4];
    /* 0x58 */ u8          field_58;
    /* 0x59 */ u8          field_59;
    /* 0x5A */ u8          field_5A;
    /* 0x5B */ u8          field_5B;
    /* 0x5C */ u8          field_5C;
    /* 0x5D */ u8          field_5D;
    /* 0x5E */ byte        pad_5E[2];
} GpObj5D;
STATIC_ASSERT_SIZEOF(GpObj5D, 0x60);

/// Sparse overlay of the same object family as `GpObj5D` / `GpObj50`.
/// `func_800E2DE4` rolls `(D_80070F60 * 5 + 0x71357911) >> 16 & 0xFFF`
/// against `field_50->field_D << 12 / 100`. On a hit it clears
/// `field_5A`, ORs bit 0x4 into `field_4C`, reseeds `field_59` from
/// `D_80070F60`, and writes `field_5C` from `D_80114C08.field_0 % 10`
/// when `arg1` has the 0x8000 bit (else 0).
/// `func_800E2F7C` tests bit 0x4 of `field_4C` and compares `field_5A`
/// against `field_50->field_E * D_80113D28[field_5C] / 100`.
/// `func_800E2EC4` decrements `field_59` and, on expiry, increments
/// `field_5A`, reseeds `field_59` from `D_80070F60`, and returns
/// `field_50->field_4 * D_80113D38[field_5C] / 100` (or 1 if that is 0).
/// Trailing pad keeps pointer alignment; full object size is not known yet.
typedef struct _GpObj5C {
    /* 0x00 */ byte        pad_0[0x4C];
    /* 0x4C */ u8          field_4C;
    /* 0x4D */ byte        pad_4D[3];
    /* 0x50 */ GpPairSrcE* field_50;
    /* 0x54 */ byte        pad_54[5];
    /* 0x59 */ u8          field_59;
    /* 0x5A */ u8          field_5A;
    /* 0x5B */ byte        pad_5B;
    /* 0x5C */ u8          field_5C;
    /* 0x5D */ byte        pad_5D[3];
} GpObj5C;
STATIC_ASSERT_SIZEOF(GpObj5C, 0x60);

/// Sparse overlay whose pointer at 0x20 is a `GpObj5C*` (same family as
/// `GpObj50`). `func_800DB558` reads `field_20->field_50` and adds that
/// `GpPairSrcE`'s `field_6` / `field_8` / `field_A` into
/// `D_801153F0.field_8` / `field_C` / `field_10`.
typedef struct _GpObj20E {
    /* 0x00 */ byte     pad_0[0x20];
    /* 0x20 */ GpObj5C* field_20;
} GpObj20E;
STATIC_ASSERT_SIZEOF(GpObj20E, 0x24);

/// 0x4C list node appended to `D_8010FAB0[index]` by `func_800E1688` and
/// unlinked by `func_800E1708`. `func_800E1758` empties the whole list.
/// `field_4A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87); bit 0x80 marks the last element of an array walked
/// at +0x4C. Callers also store `D_80070F10` at +0x8 and OR bit 0x40 into
/// `field_4A`.
typedef struct _GpObj4A {
    /* 0x00 */ struct _GpObj4A* next;
    /* 0x04 */ struct _GpObj4A* prev;
    /* 0x08 */ void*            field_8; // GsCOORDINATE2*; callers store &D_80070F10
    /* 0x0C */ byte             pad_C[0x3E];
    /* 0x4A */ u8               field_4A;
    /* 0x4B */ byte             pad_4B;
} GpObj4A;
STATIC_ASSERT_SIZEOF(GpObj4A, 0x4C);

/// 0x3C list node appended to `D_8010FAB8[index]` by `func_800E17B4` and
/// unlinked by `func_800E1834`. `func_800E1884` empties the whole list.
/// `field_3A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87). Bit 0x40 is the active filter used by
/// `func_800E0308` before it calls `func_800DFCCC`. Bit 0x80 marks the last
/// element of an array walked at +0x3C (`func_800ACD2C`). Same link/flag
/// layout as `GpObj4A`, with the flag byte at 0x3A instead of 0x4A. Full
/// object size is not known yet.
typedef struct _GpObj3A {
    /* 0x00 */ struct _GpObj3A* next;
    /* 0x04 */ struct _GpObj3A* prev;
    /* 0x08 */ byte             pad_8[0x32];
    /* 0x3A */ u8               field_3A;
    /* 0x3B */ byte             pad_3B;
} GpObj3A;
STATIC_ASSERT_SIZEOF(GpObj3A, 0x3C);

/// Grid conversion params pointed to by `D_80115448`.
/// `func_800E0B48` writes `out.vx = (pos.vx + field_14) / field_20` (or -1
/// if that sum is negative), `out.vy = 0`, and
/// `out.vz = (pos.vz + field_18) / field_20` (or -1). `func_800E0774`
/// applies `field_0->workm` with `ApplyTransposeMatrixLV`, then subtracts
/// `field_0->coord.t[0]` / `t[2]` from the transformed X / Z.
/// `func_800DEAFC` does the same transform on two `SVECTOR`s, keeping only
/// the low 16 bits. Full object size is not known yet.
typedef struct _GpGridParams {
    /* 0x00 */ struct _GsCOORDINATE2* field_0;
    /* 0x04 */ byte                   pad_4[0x10];
    /* 0x14 */ s32                    field_14;
    /* 0x18 */ s32                    field_18;
    /* 0x1C */ byte                   pad_1C[4];
    /* 0x20 */ u16                    field_20;
    /* 0x22 */ byte                   pad_22[2];
} GpGridParams;
STATIC_ASSERT_SIZEOF(GpGridParams, 0x24);

/// Global at `D_801153F0`. `func_800DB3FC` zeros the object, then writes
/// `field_2B` from `Mc_SaveData.field_F` (as `u8`), or 4 when that byte is
/// 0 and `Mc_SaveData.field_E != 0`. `func_800A74C4() == 1` forces
/// `field_2B = 0` instead. `field_0` is a state byte (1 if first set by
/// `func_800DB4E0`; 2 when the last `field_6` ref is released). `field_1`
/// is an alternate-active flag (`func_800A7508` / `func_800A7CB0` /
/// `func_800A7CF4` / `func_800A7D54`); last-ref release sets it to 0x3C.
/// `field_2` is a bitset (`func_800DB500` sets bit `arg0 - 1` when
/// `arg0 != 0`; also written as `D_801153F2`). `field_3` is cleared with
/// `field_2` on last-ref release (also written as `D_801153F3` by
/// `func_800DB530`). `field_4` is also `D_801153F4`. `field_5` is a u8 count incremented by `func_800E1C58`
/// when it claims a `GpSlot18`. `field_6` is a u16
/// refcount incremented by `func_800DB53C` and decremented by
/// `func_800DB558` / `func_800DB630` / `func_800DB6B4`. Last-ref
/// release in `func_800DB630` also clears words at 0x8 / 0xC / 0x10.
/// `func_800DB558` then adds `arg0->field_20->field_50` `field_6` /
/// `field_8` / `field_A` into those same words.
/// `func_800E2C78` adds into
/// `field_14` when `(arg1 & 0x7F)` is 0x19..0x1B.
/// `field_18`..`field_2A` are unknown bytes cleared by `func_800DB3FC`.
/// Full object may still be larger than 0x2C (`D_80115424` is a separate
/// symbol at +0x34).
typedef struct _GpStateF0 {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
    /* 0x18 */ u8  field_18;
    /* 0x19 */ u8  field_19;
    /* 0x1A */ u8  field_1A;
    /* 0x1B */ u8  field_1B;
    /* 0x1C */ u8  field_1C;
    /* 0x1D */ u8  field_1D;
    /* 0x1E */ u8  field_1E;
    /* 0x1F */ u8  field_1F;
    /* 0x20 */ u8  field_20;
    /* 0x21 */ u8  field_21;
    /* 0x22 */ u8  field_22;
    /* 0x23 */ u8  field_23;
    /* 0x24 */ u8  field_24;
    /* 0x25 */ u8  field_25;
    /* 0x26 */ u8  field_26;
    /* 0x27 */ u8  field_27;
    /* 0x28 */ u8  field_28;
    /* 0x29 */ u8  field_29;
    /* 0x2A */ u8  field_2A;
    /* 0x2B */ u8  field_2B;
} GpStateF0;
STATIC_ASSERT_SIZEOF(GpStateF0, 0x2C);

/// 0xC slot in the 32-entry table at `D_80115270`. `func_800DAF98` clears
/// `field_0` / `field_4` / `field_6`. `func_800DA6E8` binds `field_0` and
/// bumps `field_4`; `func_800DA7B8` treats `field_6` as a countdown and
/// stores projected screen XY at 0x8.
typedef struct _GpSlot70 {
    /* 0x0 */ void* field_0;
    /* 0x4 */ s16   field_4;
    /* 0x6 */ s16   field_6;
    /* 0x8 */ s16   field_8;
    /* 0xA */ s16   field_A;
} GpSlot70;
STATIC_ASSERT_SIZEOF(GpSlot70, 0xC);

/// Overlay used by `func_800DB004`. `field_8` is a `GsCOORDINATE2*`
/// (`workm` is loaded as both rotation and translation). `field_C` /
/// `field_10` / `field_14` are the low halves of a `VECTOR3` at +0xC
/// (same layout `GpLockPos.pos` / `func_800DAE50` loads as three words).
typedef struct _GpPerspSrc {
    /* 0x00 */ byte  pad_0[8];
    /* 0x08 */ void* field_8;
    /* 0x0C */ u16   field_C;
    /* 0x0E */ byte  pad_E[2];
    /* 0x10 */ u16   field_10;
    /* 0x12 */ byte  pad_12[2];
    /* 0x14 */ u16   field_14;
    /* 0x16 */ byte  pad_16[2];
} GpPerspSrc;
STATIC_ASSERT_SIZEOF(GpPerspSrc, 0x18);

/// Source for `func_800DAE50` (`get_lock_pos`). `coord` is the world
/// transform; `pos` is the local `VECTOR3`. Overlays `GpPerspSrc`.
typedef struct _GpLockPos {
    /* 0x00 */ byte                   pad_0[8];
    /* 0x08 */ struct _GsCOORDINATE2* coord;
    /* 0x0C */ VECTOR3                pos;
} GpLockPos;
STATIC_ASSERT_SIZEOF(GpLockPos, 0x18);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `func_800DB004`.
/// `vec` is the packed `SVECTOR` fed to RTPS. `p` / `flag` / `otz` hold
/// IR0, FLAG, and `SZ3 >> 2`.
typedef struct _GpPerspScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     p;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     otz;
} GpPerspScratch;
STATIC_ASSERT_SIZEOF(GpPerspScratch, 0x14);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `func_800D937C`.
/// Same RTPS outputs as `GpPerspScratch`, plus the packed `SXY2` at 0x14
/// (`sx` is the signed screen X used as stereo pan).
typedef struct _GpPanScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     p;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s16     sx;
} GpPanScratch;
STATIC_ASSERT_SIZEOF(GpPanScratch, 0x18);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `func_800D8EA0`.
/// `mtx` holds the previous-mode 3x3 copy. `col0` / `col1` are the
/// current and previous columns packed for GPF/GPL.
typedef struct _GpColorScratch {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ SVECTOR col0;
    /* 0x28 */ SVECTOR col1;
} GpColorScratch;
STATIC_ASSERT_SIZEOF(GpColorScratch, 0x30);

/// Column overlay of a packed 3x3 (`MATRIX.m`). `x` / `y` / `z` are
/// `m[0][i]` / `m[1][i]` / `m[2][i]`. Advancing to `&col->_0` walks to
/// column `i+1`.
typedef struct _GpMtxCol {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 _0;
    /* 0x4 */ s16 _1;
    /* 0x6 */ s16 y;
    /* 0x8 */ s16 _2;
    /* 0xA */ s16 _3;
    /* 0xC */ s16 z;
} GpMtxCol;
STATIC_ASSERT_SIZEOF(GpMtxCol, 0xE);

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `func_800D9138` /
/// `func_800D70E4` / `func_800D6E5C`.
/// `vec` is the halved XYZ from `GpObj44.field_18` (`func_800D9138`) or
/// from `field_38 -` a world `VECTOR3` (`func_800D70E4` / `func_800D6E5C`).
/// `distSq` is `vx²+vy²+vz²`. `outerSq` / `innerSq` are `(radius²) >> 2`
/// from `field_5C` / `field_58` (`func_800D6E5C` first stores
/// `field_5C / 2` in `outerSq` for the `|dx|` / `|dz|` test). `scale`
/// is 0, `0x1000`, or the 12.4 falloff copied to `field_4A`.
typedef struct _GpAttnScratch {
    /* 0x00 */ VECTOR vec;
    /* 0x10 */ s32    distSq;
    /* 0x14 */ s32    outerSq;
    /* 0x18 */ s32    innerSq;
    /* 0x1C */ s32    scale;
} GpAttnScratch;
STATIC_ASSERT_SIZEOF(GpAttnScratch, 0x20);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by `func_800E1380`.
/// `local` is `GpActorD4Rec.field_8/A/C` plus `GpObj.field_10/12/14`,
/// rotated by `field_8->workm`. `vec` is that GTE output (then overwritten
/// with per-slot XYZ deltas). `world` is `vec + workm.t`.
typedef struct _GpNearScratch {
    /* 0x00 */ VECTOR3 vec;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ VECTOR3 world;
    /* 0x1C */ s32     pad_1C;
    /* 0x20 */ SVECTOR local;
} GpNearScratch;
STATIC_ASSERT_SIZEOF(GpNearScratch, 0x28);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_800DEAFC`.
/// `in` is the SVECTOR promoted to VECTOR for `ApplyTransposeMatrixLV`;
/// `out` is that transform; `pos0` / `pos1` are the 16-bit grid-space
/// results passed to `func_800DE2C0`.
typedef struct _GpGridPairScratch {
    /* 0x00 */ VECTOR in;
    /* 0x10 */ VECTOR out;
    /* 0x20 */ VECTOR pos0;
    /* 0x30 */ VECTOR pos1;
} GpGridPairScratch;
STATIC_ASSERT_SIZEOF(GpGridPairScratch, 0x40);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `func_800DEC80`.
/// `local` is `field_C` as `SVECTOR[2]` plus the object's 0x10 SVECTOR,
/// rotated by `field_8->workm` into `vec` then added to `workm.t`.
/// `vec` is reused as `arg1[0] - arg1[1]` for `VectorNormalS`.
typedef struct _GpNormScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR local;
} GpNormScratch;
STATIC_ASSERT_SIZEOF(GpNormScratch, 0x18);

/// 0x50-byte scratch from `G_SCRATCH_HEAD` used by `func_800DDC2C`.
/// `src[0]` / `src[1]` are the local XZ endpoints of `GpObj.field_10/14`
/// offset by `field_C` (as an SVECTOR) scaled by `field_1C >> 12`. `mat`
/// is `D_80070F34 * field_8->workm`. `pos` holds the rotated endpoints
/// plus `mat.t[0]/t[2]` and `D_80115448` grid offsets, then passed to
/// `func_800DE2C0`.
typedef struct _GpEdgeScratch {
    /* 0x00 */ VECTOR  pos[2];
    /* 0x20 */ SVECTOR src[2];
    /* 0x30 */ MATRIX  mat;
} GpEdgeScratch;
STATIC_ASSERT_SIZEOF(GpEdgeScratch, 0x50);

/// 0x1C-byte scratch from `G_SCRATCH_HEAD` used by `func_800D9794` /
/// `func_800D98C4` / `func_800D9A30`. `in` is the direction
/// `func_800D98C4` / `func_800D9A30` feed to `Gfx_NormalizeLightDir`.
/// `dir` is that output (then overwritten by the GPF-scaled color).
/// `scale` holds `GpObj44.field_4A` loaded into IR0.
typedef struct _GpLightScratch {
    /* 0x00 */ VECTOR  in;
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ s32     scale;
} GpLightScratch;
STATIC_ASSERT_SIZEOF(GpLightScratch, 0x1C);

/// 0x4C-byte scratch from `G_SCRATCH_HEAD` used by `func_800E1CD4`.
/// `vec` is the `VectorNormalS` result, reused as the `RotMatrix` angle
/// vector. `mat1` is RotY(yaw), then RotY * RotX(-pitch). `mat2` is
/// RotX(-pitch), then RotZ(roll). `pitch` / `yaw` are `ratan2` angles
/// in `0..0xFFF`.
typedef struct _GpDirMatScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mat1;
    /* 0x28 */ MATRIX  mat2;
    /* 0x48 */ s16     pitch;
    /* 0x4A */ s16     yaw;
} GpDirMatScratch;
STATIC_ASSERT_SIZEOF(GpDirMatScratch, 0x4C);

/// 0x48-byte scratch from `G_SCRATCH_HEAD` used by `func_800DBCAC`.
/// `func_800E08CC` writes world-space positions into `pos0` / `pos1`.
/// `delta` is `pos0 - pos1`. On overlap, `src` / `extra` / `rsum` are
/// filled for `func_800DBA20` (other object's truncated position, a
/// zeroed extra SVECTOR, and the summed radii). `func_800DBA20` writes
/// `src` / `extra` into the chosen `GpRec18` at `field_8` / `field_10`,
/// stores `arg1->field_18` at `field_4`, and ORs `(flags & 0xF0) + 1`
/// into `field_0`.
typedef struct _GpSphereScratch {
    /* 0x00 */ SVECTOR src;
    /* 0x08 */ SVECTOR extra;
    /* 0x10 */ s16     rsum;
    /* 0x12 */ s16     pad_12;
    /* 0x14 */ VECTOR  pos0;
    /* 0x24 */ VECTOR  pos1;
    /* 0x34 */ VECTOR  delta;
    /* 0x44 */ s32     rsum32;
} GpSphereScratch;
STATIC_ASSERT_SIZEOF(GpSphereScratch, 0x48);

/// Pending flags written by `func_800D5B14` and consumed by `func_800CE294`.
/// `D_8010F888 == 1` requests `func_800AC464(..., 0x402, ...)`.
extern s32 D_8010F888;

/// Signed pending item id consumed by `func_800D68C4`. `func_800D5B14`
/// stores the id, or its negation for the second `GpItemSlot` pair.
/// `func_800CE294` also consumes it (with `D_8010F890`) via `func_801088D4`.
extern s32 D_8010F88C;

/// Non-zero when `D_8010F88C` should be applied by `func_800CE294`.
extern s32 D_8010F890;

/// Pending id consumed by `func_800CE294`; `0x3E` also calls `func_8010A1B0`.
extern s32 D_8010F894;

/// Head of the `GpLinkNode` list walked by `func_800DAB38` / `func_800DABEC`
/// / `func_800A784C`.
extern GpLinkNode* D_80115268;

/// 32-entry marker/slot table cleared by `func_800DAF98`.
extern GpSlot70 D_80115270[0x20];

/// Per-stage pointer table. Index is `GameSessionFrom4.field_3 - 1`.
/// Each entry is an array of `GpCbA4Rec*`, indexed by `field_2 - 1`.
extern GpCbA4Rec** D_8010CBA4[];

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
/// Each entry is an array of `GpCbB8Rec**`, indexed by `field_6 - 1`.
/// Each of those is an 8-entry array of `GpCbB8Rec*` copied into
/// `D_80115428` by `func_800E192C`.
extern GpCbB8Rec*** D_8010CBB8[];

/// Default 8-byte record copied by `func_800D9CE8`. Also the fallback
/// pointer returned by `func_800D957C` when a table lookup fails.
extern GBytes8 D_8010F9E4;

/// Default `MATRIX` installed at `GameActorExt.field_1C` by `func_800D9D18`.
extern MATRIX D_80114E98;

/// Default `MATRIX` installed at `GameActorExt.field_20` by `func_800D9D18`.
extern MATRIX D_80114EB8;

/// Flag set by `func_800D94B8` when an override SVECTOR is stored at
/// `D_80114F20`. Cleared when that function is called with NULL, and
/// also by `func_800D9D18`.
extern u8 D_80114F18;

/// Override SVECTOR copied by `func_800D94B8` from its argument.
extern SVECTOR D_80114F20;

/// Word cleared by `func_800D9D18`. Also written by `func_800A45F0` and
/// read/cleared by `func_800D8684`.
extern s32 D_80114F28;

/// Flag set by `func_800D9504` when an override SVECTOR is stored at
/// `D_80115258`. Cleared when that function is called with NULL, and
/// also by `func_800D9D18`.
extern u8 D_80115250;

/// Override SVECTOR copied by `func_800D9504` from its argument.
extern SVECTOR D_80115258;

/// 8.8 fixed-point pair lerped toward projected screen coords by
/// `func_800D9DFC`. Reset to `0xFFF00000` by `func_800DAFD0`.
extern s32 D_8010F9EC;
extern s32 D_8010F9F0;

/// Per-stage `GpGiveRec` lists selected by `func_800DB128` when
/// `Mc_SaveData.field_F` is 0 or 2. Indexed by `GameSession.field_7`.
extern GpGiveRec* D_8010F9F4[];

/// Per-stage `GpGiveRec` lists selected by `func_800DB128` when
/// `Mc_SaveData.field_F` is not 0 or 2. Indexed by `GameSession.field_7`.
extern GpGiveRec* D_8010FA0C[];

/// Pair-handler table used by `func_800DB900` / `func_800E0414`.
/// Indexed by `D_8010FA4C[].field_0` (`func_800E076C` / `func_800DBCAC` /
/// `func_800DBE7C`).
extern GpPairFn D_8010FA38[5];

/// 4x4 pair-rule table used by `func_800DB900` / `func_800E0414`.
/// Rows/cols are `(flags & 7) - 1`. `field_0` selects `D_8010FA38`;
/// a non-zero `field_2` swaps the two `GpObj` arguments.
extern GpU16Pair D_8010FA4C[4][4];

/// Nine-entry table of `GpObj` list heads (`D_80115570` .. `D_80115590`).
/// `func_800E15AC` appends to `D_8010FA8C[index]`; `func_800E1638` unlinks.
extern GpObj* D_8010FA8C[9];

/// Two-entry table of `GpObj4A` list heads. `func_800E1688` appends to
/// `D_8010FAB0[index]`; `func_800E1758` walks and clears that list.
extern GpObj4A* D_8010FAB0[2];

/// One-entry table of `GpObj3A` list heads. `func_800E17B4` appends to
/// `D_8010FAB8[index]`; `func_800E1884` walks and clears that list.
extern GpObj3A* D_8010FAB8[1];

extern GpStateF0 D_801153F0;
extern u8        D_801153F2;
extern u8        D_801153F3;
extern u8        D_801153F4;
/// Head of the `GpObj3A` list pointed to by `D_8010FAB8[0]`.
extern GpObj3A* D_80115550;
/// Head of the `GpObj4C` list walked by `func_800E1B80`.
extern GpObj4C* D_80115554;
extern GpObj4C* D_8011556C;
extern GpObj* D_80115570;
extern GpObj* D_80115574;
extern GpObj* D_80115578;
extern GpObj* D_8011557C;
extern GpObj* D_80115580;
extern GpObj* D_80115584;
extern GpObj* D_80115588;
extern GpObj* D_8011558C;
extern GpObj* D_80115590;
extern u8     D_80115598;
/// Set to 1 by `func_800E1BF0` when a pending `D_8011556C` node is found;
/// `func_800DB72C` then calls `func_800E0B08` to clear those flags.
extern s32    D_80115424;
/// 8-word table filled by `func_800E192C` from the current room's
/// `D_8010CBB8` records (`field_3`). Indexed by `(id & 7)` in
/// `func_800E0C10` / `func_800E0FEC`.
extern s32    D_80115428[8];
/// Grid conversion params used by `func_800E0B48` / `func_800E0774`.
/// Cleared by `func_800E0294`; `func_800E0540` skips work when this is NULL.
extern GpGridParams* D_80115448;

/// 4-byte records selected by `func_800E2CD4(..., 0)`.
extern GpRec4 D_80114028[];

/// 6-byte records selected by `func_800E2CD4(..., 1)`.
extern GpRec6 D_80114054[];

/// 10-byte records selected by `func_800E3194` when the id's 0x8000 bit is
/// clear. Indexed by `id & 0x7F`.
extern GpRec10 D_80113390[];

/// 16-byte records selected by `func_800E3194` when the id's 0x8000 bit is
/// set. Indexed by `id & 0x7F`.
extern GpRec16 D_8011398C[];

/// u16 scale table indexed by `GpObj5C.field_5C`. `func_800E2F7C` multiplies
/// `field_50->field_E` by the selected entry and divides by 100.
extern u16 D_80113D28[];

/// u16 scale table indexed by `GpObj5D.field_5D`. `func_800E3084` multiplies
/// `field_50->field_C` by the selected entry and divides by 100.
extern u16 D_80113D30[];

/// u16 scale table indexed by `GpObj5C.field_5C`. `func_800E2EC4` multiplies
/// `field_50->field_4` by the selected entry and divides by 100.
extern u16 D_80113D38[];

/// Damage-scale rows used by `func_800E2438`. Indexed by `D_8011541B`.
extern GpDmgRow D_80113EF0[];

/// Column index table for `D_80113EF0`, indexed by signed HP / 10.
extern u16 D_80113F54[];

/// Percent scale table used by `func_800E2438` when `D_80114C08.field_C`
/// is non-zero. Indexed by `((field_C / 16) - 1) * 2 + (s8)(field_C % 16)`.
extern u16 D_80113CFC[];

/// Row index into `D_80113EF0` for `func_800E2438`.
extern u8 D_8011541B;

/// "Weapon" string drawn by `func_800D6AA4` (trailing 0x60 byte).
extern const char D_80097454[];

/// "#######get_lock_pos ---> NULL!!!\n" printed by `func_800DAE50`
/// (trailing 0x8C 0x16 bytes).
extern const char D_80097460[];

/// Returns 1 if item `arg0` cannot be used, 0 if it can.
/// `arg1` supplies `field_2` (capacity) for ammo ids 0xA0–0xBF.
s32        func_800D6170(s32 arg0, GpItemRec* arg1);
s32        func_800D68C4(s32 arg0);
GpItemRec* func_800D6910(s32 arg0);
GpItemRec* func_800D6994(s32 arg0);
GpItemRec* func_800D6A24(s32 arg0, GpItemScan* arg1);
void       func_800D6AA4(Task* arg0);
/// First-run init plus per-frame update of the current room's `GpCbA4Set`
/// coordinate arrays (parented to `D_80070F10`) and the `D_80114F30` slots.
/// Kills `arg0` when `func_800D9654` returns 0.
void       func_800D6B20(Task* arg0);
s32        func_800D6E5C(GpObj44* arg0, VECTOR3* arg1);
s32        func_800D70E4(GpObj44* arg0, VECTOR3* arg1);
void       func_800D7A9C(GameActorExt* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void  func_800D8684(Task* arg0);
/// Remaps a 3x3 color matrix (`MATRIX.m`) from lighting mode `arg2`
/// (`field_4E` bits 0-1, or bits 2-3 when blending). Mode 1 weights
/// RGB as (7,6,3)/33 then *4/*2/*1. Mode 2 zeros the matrix. Mode 3
/// fills 0x180/0x100/0x100. Default remaps to *3/*1/*3 when
/// `field_4C & 0xC`. Bit 0x80 of `field_4E` with `field_4B == 0` applies
/// a `rsin(Display_State.field_14 << 6)` flicker and clears the bit.
void  func_800D8C0C(struct _GpEnemy* arg0, MATRIX* arg1, s32 arg2);
/// Rebuilds the actor color matrix via `func_800D7A9C`, then remaps it
/// from `field_4E` lighting mode (`func_800D8C0C`). While `field_4F` is
/// a positive blend timer, GPF/GPL-interpolates the previous mode
/// (`field_4E` bits 2-3) toward the current mode (bits 0-1). Skips work
/// when `Game_Session->field_65 == 1` unless `GameActorExt.field_C` bit
/// 0x80 is clear and `field_18` is set. `D_801153F4` freezes the timer.
void  func_800D8EA0(struct _GpEnemy* arg0, VECTOR* arg1);
void  func_800D9138(GpObj44* arg0);
void  func_800D930C(GpObj4C* arg0, s32 arg1);
s32   func_800D9340(GpObj38* arg0);
s32   func_800D937C(GpObj38* arg0);
void  func_800D94B8(SVECTOR* arg0);
void  func_800D9504(SVECTOR* arg0);
void  func_800D9550(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3);
GpCbA4Vec* func_800D957C(GameSessionFrom4* arg0);
s32   func_800D9618(void);
s32   func_800D9654(GameSessionFrom4* arg0);
void  func_800D96C8(Task* arg0);
s32   func_800D9718(GpObj44* arg0);
s32   func_800D9788(GpObj38* arg0);
void  func_800D9794(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void  func_800D98C4(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void  func_800D9A30(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void  func_800D9B9C(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void  func_800D9D18(Task* arg0);
void  func_800D9C3C(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3);
GpCbA4Rec* func_800D9C64(GameSessionFrom4* arg0);
void  func_800D9CC8(Task* arg0);
void  func_800D9CE8(GBytes8* arg0);
void  func_800D9DFC(void);
void  func_800DA6E8(void* arg0, s32 arg1);
void  func_800DAB38(GpLinkNode* node);
void  func_800DABEC(GpLinkNode* node);
s32   func_800DAC54(GpLinkNode* arg0);
void  func_800DACAC(GpLinkNode* node);
void  func_800DACF8(GpLinkNode* node);
void* func_800DA2A0(GpActorWork* arg0, VECTOR3* pos, s32 arg2);
void* func_800DAD54(GpActorWork* arg0);
void* func_800DAD78(GpActorWork* arg0);
void* func_800DADE4(GpActorWork* arg0, VECTOR3* pos);
void  func_800DAE50(GpLockPos* arg0, VECTOR3* out);
void  func_800DAF98(void);
void  func_800DAFD0(void);
s32   func_800DB004(GpPerspSrc* arg0, s32* sxy);
void  func_800DB0D8(void);
s32   func_800DB128(GpItemScan* arg0);
s32   func_800DB28C(GpActorWork* arg0, GpImgRec* arg1, RECT* arg2);
void  func_800DB31C(GpImgRec* arg0);
void  func_800DB3FC(void);
void  func_800DB4E0(s32 arg0);
void  func_800DB500(s32 arg0);
void  func_800DB530(s32 arg0);
void  func_800DB53C(void);
void  func_800DB558(GpObj20E* arg0);
void  func_800DB630(void);
void  func_800DB6B4(void);
void  func_800DB72C(void);
void func_800DB900(GpObj* node);
void func_800DBA20(GpObj* arg0, GpObj* arg1, GpSphereScratch* arg2);
s32  func_800DBCAC(GpObj* arg0, GpObj* arg1);
void func_800DC528(GpObj* node);
void func_800DCB80(GpObj* node);
void func_800DD940(GpObj* node);
void func_800DDC2C(GpObj* arg0);
void func_800DDDF8(GpObj* node);
void func_800DE2C0(VECTOR* arg0, s32 arg1);
s32  func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3);
void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1);
void func_800DEC80(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3);
void func_800DEF80(GpObj* node, GpObj4C* other);
s32  func_800DFCCC(GpObj3A* arg0, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3);
void func_800E0294(void);
s32  func_800E0308(SVECTOR* arg0, SVECTOR* arg1);
void func_800E0414(GpObj* a, GpObj* b);
void func_800E0540(GpObj* node);
void func_800E0608(GpObj* node, s32 mask, s32 match);
void func_800E06AC(GpObj* node, s32 mask, s32 match);
s32  func_800E076C(void);
void func_800E0774(VECTOR3* arg0, SVECTOR3* arg1);
void func_800E08CC(GpObj* arg0, VECTOR3* arg1);
void func_800E0B08(void);
void func_800E0B48(VECTOR3* arg0, SVECTOR3* arg1);
s32  func_800E0FEC(s32 arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
/// Transforms `arg0`'s local offset (`GpActorD4Rec` at `field_C` plus the
/// 0x10 SVECTOR) by `field_8->workm` and returns the 1-based index of the
/// closest occupied `GpRec18` in `rec->field_14` whose `field_4` high 16
/// bits match `arg1`, or 0 if none match.
s32  func_800E1380(GpObj* arg0, s32 arg1);
void func_800E15AC(s32 arg0, GpObj* arg1);
void func_800E1638(GpObj* node);
void func_800E1688(s32 arg0, GpObj4A* arg1);
void func_800E1708(s32 arg0, GpObj4A* arg1);
void func_800E1758(s32 arg0);
void func_800E17B4(s32 arg0, GpObj3A* arg1);
void func_800E1834(s32 arg0, GpObj3A* arg1);
void func_800E1884(s32 arg0);
void func_800E18E0(GpRec18* arg0, s32 arg1, s32 arg2);
void func_800E192C(void);
s32  func_800E19B8(GpRec18* arg0, s32 arg1);
s32  func_800E1A1C(GpRec18* arg0, s32 arg1);
void func_800E1A6C(GpRec18* arg0);
s32  func_800E1ACC(u8* arg0);
s32  func_800E1B24(s32 arg0);
void func_800E1B80(void);
s32  func_800E1BF0(u16* arg0, u8* arg1, u8* arg2);
void func_800E1C58(GpObj54* arg0, void* arg1);
/// Builds a rotation matrix in `arg1` that orients along normalized `arg0`
/// (yaw from XZ, pitch from Y vs the XZ length, then roll by `arg2`).
void func_800E1CD4(VECTOR* arg0, MATRIX* arg1, s32 arg2);
/// Packed-id damage scale. `arg0` must have high bits `0x40000`; low 12 bits
/// are the power and bits 12-15 are written to `*arg2` when it is non-NULL.
/// `arg3 == 0` uses `Wip_SysConfig.field_18` and `GpDmgRow.field_A`;
/// otherwise `Mc_SaveData.field_6C8` and `GpDmgRow.field_0`.
s32  func_800E2438(s32 arg0, s32 arg1, s32* arg2, s32 arg3);
void func_800E2A24(GpObj5D* arg0, s32 arg1);
s32  func_800E2BF8(GpObj50* arg0, s32 arg1);
s32  func_800E2C40(GpU16Pair* arg0, s32 arg1);
void func_800E2C78(GpObj40* arg0, s32 arg1, s32 arg2);
s32  func_800E2CD4(s32 arg0, s32 arg1);
s32  func_800E2D3C(s32 arg0);
s32  func_800E2D90(s32 arg0);
void func_800E2DE4(GpObj5C* arg0, s32 arg1);
s32  func_800E2EC4(GpObj5C* arg0);
s32  func_800E2F7C(GpObj5C* arg0);
void func_800E3008(GpObj4C* arg0);
void func_800E301C(GpObj5D* arg0, s32 arg1);
s32  func_800E3084(GpObj5D* arg0);
s32  func_800E3194(s32 arg0);
void func_800E337C(Task* arg0);
void func_8010154C(void);

#endif // GAMEPLAY_3A34_H
