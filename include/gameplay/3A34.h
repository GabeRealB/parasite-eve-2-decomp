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

/// Singly-linked node unlinked by `Gp_UnlinkNode` / linked by `Gp_LinkNode`.
/// `field_6 == 1` means the node is on the `Gp_LinkList` list.
/// `Gp_NodeSlotMask` returns a 2-bit mask of `Gp_ActorSlots[]` slots whose
/// `actor->field_90C` is this node.
/// `Gp_AssignNodeSlot0` assigns the node to `Gp_ActorSlots[0]->actor->field_90C`,
/// clears the previous node's `field_5`, sets this node's `field_5 = 1`,
/// and clears `field_4` bit 0.
/// `Gp_ClearNodeSlots` is the inverse: it nulls any `Gp_ActorSlots[]` slot whose
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
/// node is on the `Gp_ObjLists` list (set by `Gp_LinkObj`, cleared by
/// `Gp_UnlinkObj`, keeping bits 0x7). `func_8010C980` fills `field_8` /
/// `field_C` / the 0x10 SVECTOR / `field_18` / `field_1C` and ORs `flags`
/// with 0x8000 after linking. `field_8` is a `GsCOORDINATE2*`; `Gp_ObjWorldPos`
/// applies `workm` to the 0x10 SVECTOR and adds `workm.t` into a `VECTOR3`.
/// `Gp_FindNearestSlot` treats `field_C` as a `GpActorD4Rec*` whose `field_14`
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

/// 4-byte table entry packed by `Gp_PackPair` / `Gp_PackObjPair` as
/// `(field_0 & 0xFFF) | ((field_2 & 0xF) << 12) | 0x40000`.
typedef struct _GpU16Pair {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpU16Pair;
STATIC_ASSERT_SIZEOF(GpU16Pair, 0x4);

/// Pair-dispatch callback from `D_8010FA38`. `kind` is `GpU16Pair.field_0`.
typedef void (*GpPairFn)(GpObj* a, GpObj* b, s32 kind);

/// 4-byte table entry at `Gp_IdField0`. `Gp_LookupIdField(idx, 0)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec4 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpRec4;
STATIC_ASSERT_SIZEOF(GpRec4, 0x4);

/// 6-byte table entry at `Gp_IdField1`. `Gp_LookupIdField(idx, 1)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec6 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpRec6;
STATIC_ASSERT_SIZEOF(GpRec6, 0x6);

/// 10-byte table entry at `Gp_IdParamLo`. Selected when the id's 0x8000 bit
/// is clear. `Gp_GetIdParam0` / `Gp_GetIdParam1` / `Gp_GetIdParam2` return
/// `field_4` / `field_6` / `field_8` for index `id & 0x7F`.
typedef struct _GpRec10 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
} GpRec10;
STATIC_ASSERT_SIZEOF(GpRec10, 0xA);

/// 12-byte ranked slot inserted by `Gp_InsertRankedSlot`. That helper walks a
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

/// 12-byte location-keyed grant record walked by `Gp_GrantLocationItems`.
/// `field_0` is `(stage << 24) | (area << 16) | (sub << 8)` from
/// `GameSessionFrom4.field_3` / `field_2` / `field_5`, or `-1` to end
/// the list. `items[0..3]` are item ids granted with `Gp_GiveItem`
/// when `func_800B7420` is 0; a 0 slot is skipped. `items[3]` also
/// requires `func_800B9D80(0x80000)`.
typedef struct _GpGiveRec {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 items[4];
} GpGiveRec;
STATIC_ASSERT_SIZEOF(GpGiveRec, 0xC);

/// 16-byte VRAM upload record walked by `Gp_LoadImages`. `field_0 == 0`
/// uploads `rect` / `data` via `LoadImage`; non-zero ends the walk.
/// `Gp_LoadActorImage` fills `rect` from a source RECT plus the TMD tpage at
/// `TmdObject.field_24` (`x = tpage * 64 + (src.x + 1) / 2 + 0x180`,
/// `y = src.y + 0x100`).
typedef struct _GpImgRec {
    /* 0x0 */ u16     field_0;
    /* 0x2 */ u16     pad_2;
    /* 0x4 */ RECT    rect;
    /* 0xC */ u_long* data;
} GpImgRec;
STATIC_ASSERT_SIZEOF(GpImgRec, 0x10);

/// 16-byte table entry at `Gp_IdParamHi`. Selected when the id's 0x8000 bit
/// is set. `Gp_GetIdParam0` / `Gp_GetIdParam1` / `Gp_GetIdParam2` return
/// `field[5]` / `field[6]` / `field[7]` for index `id & 0x7F`.
/// `func_800D50D4` indexes `field[arg1]` after remapping a packed id as
/// `((id>>4&3)*3 + (id>>2&3))*3 + (id&3)`.
typedef struct _GpRec16 {
    /* 0x0 */ u16 field[8];
} GpRec16;
STATIC_ASSERT_SIZEOF(GpRec16, 0x10);

/// 20-byte damage-scale row at `Gp_DmgRows`. Indexed by `D_8011541B`.
/// `Gp_ScaleDamage` adds `D_80113F54[hp / 10] * 2` onto the row base and
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
/// `GpU16Pair` array packed by `Gp_PackObjPair`. Nearby helpers also
/// load bytes at +0xB / +0xD / +0xE of this object (`GpPairSrcE`).
typedef struct _GpPairSrc {
    /* 0x00 */ GpU16Pair* field_0;
} GpPairSrc;
STATIC_ASSERT_SIZEOF(GpPairSrc, 0x4);

/// Wider view of the object pointed to by `GpObj50.field_50`.
/// `Gp_TickObjFlag4` loads `field_4`; `Gp_ReleaseStateF0Add` adds `field_6` /
/// `field_8` / `field_A` into `Gp_StateF0.field_8` / `field_C` /
/// `field_10`. Nearby helpers also load bytes at +0xB / +0xC / +0xD /
/// +0xE (`Gp_TickObjFlag2` loads `field_C`; `Gp_SetObjFlag4` loads
/// `field_D`; `Gp_ObjFlag4Expired` loads `field_E`). Trailing pad keeps
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

/// Object whose pointer at 0x50 is a `GpPairSrc*` used by `Gp_PackObjPair`.
/// Same object family as `GpObj4C` (flags at 0x4C).
typedef struct _GpObj50 {
    /* 0x00 */ byte       pad_0[0x50];
    /* 0x50 */ GpPairSrc* field_50;
} GpObj50;
STATIC_ASSERT_SIZEOF(GpObj50, 0x54);

/// 0x18-byte slot in the table at `GpObj54.field_54`. Occupied when the
/// first word's low 2 bits equal 1. `Gp_ClaimSlot18` claims the first free
/// slot: payload in `field_4`, clears `field_2` / `field_8` / `field_A` /
/// `field_C` / `field_10` / `field_12` / `field_14`, ORs bit 0 into
/// `field_0`, and increments `Gp_StateF0.field_5`.
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
/// `Gp_ClaimSlot18`. Same object family as `GpObj50` / `GpObj4C`.
typedef struct _GpObj54 {
    /* 0x00 */ byte      pad_0[0x54];
    /* 0x54 */ GpSlot18* field_54;
} GpObj54;
STATIC_ASSERT_SIZEOF(GpObj54, 0x58);

/// 8-byte nested table entry pointed to by `GpRoomCoordRec.field_4`.
/// Entry 0's `field_0` is the max valid index. `Gp_GetRoomBound` returns
/// `&table[GameSessionFrom4.field_0]` when that index is in range,
/// otherwise `(GpRoomBoundVec*)&Gp_RoomBoundDefault`. `func_800D7A9C` reads
/// `field_0` / `field_2` / `field_4` as signed XYZ minimums.
typedef struct _GpRoomBoundVec {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} GpRoomBoundVec;
STATIC_ASSERT_SIZEOF(GpRoomBoundVec, 8);

/// 8-byte record in tables pointed to by `Gp_RoomCoordTables`. Indexed 1-based
/// by `GameSessionFrom4.field_1`. `Gp_GetRoomCoordRec` returns the record
/// (or NULL). `Gp_GetRoomCoordSet` returns `field_0` as a `GpRoomCoordSet*` (or 0).
/// `Gp_GetRoomBound` walks `field_4` as a nested `GpRoomBoundVec` table, falling
/// back to `Gp_RoomBoundDefault`.
typedef struct _GpRoomCoordRec {
    /* 0x0 */ s32             field_0;
    /* 0x4 */ GpRoomBoundVec* field_4;
} GpRoomCoordRec;
STATIC_ASSERT_SIZEOF(GpRoomCoordRec, 8);

/// 0x58-byte coordinate object in `GpRoomCoordSet.arr58`. `Gp_UpdateRoomCoords`
/// parents `coord.sub` to `D_80070F10` and clears `coord.flg`.
typedef struct _GpCoord58 {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[8];
} GpCoord58;
STATIC_ASSERT_SIZEOF(GpCoord58, 0x58);

/// 0x60-byte coordinate object in `GpRoomCoordSet.arr60`. Same `coord.sub` /
/// `coord.flg` init as `GpCoord58`. Same size as `GpCoordTail`.
typedef struct _GpCoord60 {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad_50[0x10];
} GpCoord60;
STATIC_ASSERT_SIZEOF(GpCoord60, 0x60);

/// 0x6C-byte coordinate object in `GpRoomCoordSet.arr6C`. `Gp_UpdateRoomCoords`
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

/// Room coordinate tables returned by `Gp_GetRoomCoordSet` (`GpRoomCoordRec.field_0`).
/// `Gp_UpdateRoomCoords` parents each array to `D_80070F10` on first run, then
/// updates them every frame via `Gp_UpdateCoord` / `Gp_UpdateCoordEx`.
typedef struct _GpRoomCoordSet {
    /* 0x00 */ s32        n58;
    /* 0x04 */ GpCoord58* arr58;
    /* 0x08 */ s32        n60;
    /* 0x0C */ GpCoord60* arr60;
    /* 0x10 */ s32        n6C;
    /* 0x14 */ GpCoord6C* arr6C;
} GpRoomCoordSet;
STATIC_ASSERT_SIZEOF(GpRoomCoordSet, 0x18);

/// 0x6C-byte walk overlay of `GpCoord6C` starting at `dir`. `Gp_UpdateRoomCoords`
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
/// object. `Gp_UpdateRoomCoords` writes `sub` as `D_80070F10`.
typedef struct _Gp6CMid {
    /* 0x00 */ GsCOORDINATE2* sub;
    /* 0x04 */ byte           pad[8];
    /* 0x0C */ SVECTOR        dir;
} Gp6CMid;

/// 0x64-byte walk overlay of `GpCoord64` starting at `coord`. `Gp_UpdateRoomCoords`
/// writes `coord.sub` while a parallel `GpCoord64*` writes `field_0`.
typedef struct _GpCoord64View {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ byte          pad[0x14];
} GpCoord64View;
STATIC_ASSERT_SIZEOF(GpCoord64View, 0x64);

/// Record in the 8-entry arrays pointed to by `Gp_RoomParamTables`.
/// `Gp_LoadRoomParams` copies `field_3` into `Gp_RoomParams[]`. Nearby helpers
/// also load `field_1` (`func_800DDDF8`, `func_800DE7CC`) and `field_2`
/// (`func_80105BC4` keeps a slot only when this is nonzero). `func_80105ED4`
/// loads a pointer at +0x4.
typedef struct _GpRoomParamRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpRoomParamRec;
STATIC_ASSERT_SIZEOF(GpRoomParamRec, 4);

/// Three packed `SVECTOR3`s filled by `Gp_FillSVec3x3`. Each vector's
/// components are set to the same s16 argument.
typedef struct _GpSVec3x3 {
    /* 0x00 */ SVECTOR3 field_0;
    /* 0x06 */ SVECTOR3 field_6;
    /* 0x0C */ SVECTOR3 field_C;
} GpSVec3x3;
STATIC_ASSERT_SIZEOF(GpSVec3x3, 0x12);

/// Object whose pointers at 0x1C / 0x20 are `MATRIX*`s. `Gp_SetObjTrans`
/// writes translation `t[0]/t[1]/t[2]` through `field_20`. `Gp_BindDefaultMtx`
/// installs the default matrices `Gp_DefaultMtx` / `Gp_DefaultMtx2` here (same
/// overlay as `GameActorExt`).
typedef struct _GpObj20 {
    /* 0x00 */ byte    pad_0[0x1C];
    /* 0x1C */ MATRIX* field_1C;
    /* 0x20 */ MATRIX* field_20;
} GpObj20;
STATIC_ASSERT_SIZEOF(GpObj20, 0x24);

/// Object with a `MATRIX` at 0x24 (`GsCOORDINATE2.workm` when this overlays
/// an actor coordinate). `Gp_GetObjPan` loads that matrix for RTPS.
/// `Gp_GetObjTransX` returns `field_24.t[0]`. Light helpers take
/// `field_24.t` as a `VECTOR*` (`t[0]/t[1]/t[2]`).
typedef struct _GpObj38 {
    /* 0x00 */ byte   pad_0[0x24];
    /* 0x24 */ MATRIX field_24;
} GpObj38;
STATIC_ASSERT_SIZEOF(GpObj38, 0x44);

/// Sparse overlay of the same light object as `GpObj38`. `Gp_GetObjLuma`
/// treats `field_44` as a room-id filter against `Game_Session->field_4`
/// (0 = any room), writes `0x1000` (GTE ONE) to `field_4A`, and returns a
/// weighted `field_50/52/54` luminance. `func_800D9794` casts to
/// `GpObj38` for `field_24.t` as a `VECTOR*`, loads `field_4A` into GTE
/// IR0, and `gte_ldsv`s the three halfwords at 0x50. `func_800D98C4` /
/// `func_800D9A30` subtract `field_24.t` from a world `VECTOR` and write
/// the negated normalized direction.
/// `Gp_LightFalloff` halves `field_18` as XYZ, compares distance² against
/// inner `field_58` and outer `field_5C` (each squared then `>> 2`), and
/// writes the attenuated luminance to `field_38.vx` (same word as
/// `GpObj38.field_24.t[0]`) plus the 12.4 scale to `field_4A`.
/// `Gp_LightPoint` instead subtracts a world `VECTOR3` from `field_38`
/// (same words as `GsCOORDINATE2.workm.t` / `GpObj38.field_24.t`),
/// writes the scale to `field_4A`, and returns the luminance.
/// `Gp_LightPointRoom` is that same subtract, plus the `field_44` room-id
/// filter and an `|dx|` / `|dz|` reject against `field_5C / 2` before
/// the squared-radius test.
/// `Gp_LightCone` is the cone-light variant (`GpObj68`): same room-id
/// filter and halved `field_24.t -` world `VECTOR3`, but outer/inner
/// radii are `field_64` / `field_60`, and the normalized direction is
/// dotted with `field_24` column 2 against `rcos(field_68 >> 1)`.
/// `func_800D759C` overlays `GsCOORDINATE2` at offset 0: `field_18` is
/// `coord.t`, `field_4C` is `sub`. It normalizes `-field_18`, rotates that
/// direction by `Transpose(D_80070F34) * sub->workm`, then writes the
/// negated row into `arg3->field_1C` and the IR0-scaled `field_50` color
/// into `arg3->field_20` (same matrix slots as `func_800D9794`).
typedef struct _GpObj44 {
    /* 0x00 */ byte           pad_0[0x18];
    /* 0x18 */ VECTOR3        field_18;
    /* 0x24 */ byte           pad_24[0x14];
    /* 0x38 */ VECTOR3        field_38;
    /* 0x44 */ s16            field_44;
    /* 0x46 */ byte           pad_46[4];
    /* 0x4A */ s16            field_4A;
    /* 0x4C */ GsCOORDINATE2* field_4C;
    /* 0x50 */ s16            field_50;
    /* 0x52 */ s16            field_52;
    /* 0x54 */ s16            field_54;
    /* 0x56 */ byte           pad_56[2];
    /* 0x58 */ s32            field_58;
    /* 0x5C */ s32            field_5C;
} GpObj44;
STATIC_ASSERT_SIZEOF(GpObj44, 0x60);

/// Cone-light overlay of the same object as `GpObj44` / `GpObj38`.
/// `field_24` is the light matrix (Z column is the cone axis; `t` is the
/// world position, same words as `GpObj44.field_38`). `field_60` /
/// `field_64` are inner/outer radii (squared then `>> 2`, like
/// `GpObj44.field_58` / `field_5C`). `field_68` is the cone angle fed
/// to `rcos` as `field_68 >> 1`.
typedef struct _GpObj68 {
    /* 0x00 */ byte   pad_0[0x24];
    /* 0x24 */ MATRIX field_24;
    /* 0x44 */ s16    field_44;
    /* 0x46 */ byte   pad_46[4];
    /* 0x4A */ s16    field_4A;
    /* 0x4C */ byte   pad_4C[4];
    /* 0x50 */ s16    field_50;
    /* 0x52 */ s16    field_52;
    /* 0x54 */ s16    field_54;
    /* 0x56 */ byte   pad_56[2];
    /* 0x58 */ s32    field_58;
    /* 0x5C */ s32    field_5C;
    /* 0x60 */ s32    field_60;
    /* 0x64 */ s32    field_64;
    /* 0x68 */ s32    field_68;
} GpObj68;
STATIC_ASSERT_SIZEOF(GpObj68, 0x6C);

/// Sparse overlay whose signed halfword at 0x40 is added (unsigned-clamped by
/// `arg2`) into `Gp_StateF0.field_14` by `func_800E2C78` when
/// `(arg1 & 0x7F)` is 0x19..0x1B.
typedef struct _GpObj40 {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40;
} GpObj40;
STATIC_ASSERT_SIZEOF(GpObj40, 0x42);

/// Object whose flags byte at 0x4C is OR'd by `Gp_SetObjFlag1`. Nearby
/// helpers treat 0x4C as a flag field (bits 0x1, 0x2, 0x4). `field_4E`
/// packs two 2-bit modes (current in bits 0-1, previous in bits 2-3)
/// plus a high-nibble flag; `Gp_SetLightMode` rotates the current mode
/// into the previous slot and starts `field_4F` as a 0x10 blend timer.
/// `next` and signed `field_4B` are the `Gp_PendingObj4C` list walked by
/// `Gp_ClearPendingObj4C`, which clears a non-zero `field_4B`. `Gp_TakePendingObj4C`
/// walks the same list and, on a pending `field_4B`, copies `field_46` /
/// `field_48` / `field_49` to its out-params and sets `Gp_PendingObj4CFlag`. The
/// same node type is the `Gp_Obj4CList` list walked by `Gp_CommitObj4CSave`: a
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
/// `Gp_ApplyObjKind` looks up a kind from `Gp_IdParamLo` / `Gp_IdParamHi` (same
/// field as `Gp_GetIdParam0`) and starts one of three effects: bit 0x1
/// (`Gp_SetObjFlag1`), bit 0x2 (`Gp_SetObjFlag2`, also `field_58` /
/// `field_5B` / `field_5D`), or bit 0x4 (`Gp_SetObjFlag4`, also
/// `field_59` / `field_5A` / `field_5C`). `Gp_SetObjFlag2` ORs bit 0x2
/// into `field_4C`, clears `field_58` / `field_5B`, and writes `field_5D`
/// from `Gp_StateC08.field_0 % 10` when the id has the 0x8000 bit and low
/// 6 bits != 0x31. `Gp_TickObjFlag2` compares `field_58` against
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
/// `Gp_SetObjFlag4` rolls `(Gp_LcgState * 5 + 0x71357911) >> 16 & 0xFFF`
/// against `field_50->field_D << 12 / 100`. On a hit it clears
/// `field_5A`, ORs bit 0x4 into `field_4C`, reseeds `field_59` from
/// `Gp_LcgState`, and writes `field_5C` from `Gp_StateC08.field_0 % 10`
/// when `arg1` has the 0x8000 bit (else 0).
/// `Gp_ObjFlag4Expired` tests bit 0x4 of `field_4C` and compares `field_5A`
/// against `field_50->field_E * D_80113D28[field_5C] / 100`.
/// `Gp_TickObjFlag4` decrements `field_59` and, on expiry, increments
/// `field_5A`, reseeds `field_59` from `Gp_LcgState`, and returns
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
/// `GpObj50`). `Gp_ReleaseStateF0Add` reads `field_20->field_50` and adds that
/// `GpPairSrcE`'s `field_6` / `field_8` / `field_A` into
/// `Gp_StateF0.field_8` / `field_C` / `field_10`.
typedef struct _GpObj20E {
    /* 0x00 */ byte     pad_0[0x20];
    /* 0x20 */ GpObj5C* field_20;
} GpObj20E;
STATIC_ASSERT_SIZEOF(GpObj20E, 0x24);

/// 0x4C list node appended to `Gp_Obj4ALists[index]` by `Gp_LinkObj4A` and
/// unlinked by `Gp_UnlinkObj4A`. `Gp_ClearObj4AList` empties the whole list.
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

/// 0x3C list node appended to `Gp_Obj3ALists[index]` by `Gp_LinkObj3A` and
/// unlinked by `Gp_UnlinkObj3A`. `Gp_ClearObj3AList` empties the whole list.
/// `field_3A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87). Bit 0x40 is the active filter used by
/// `func_800E0308` before it calls `func_800DFCCC`. Bit 0x80 marks the last
/// element of an array walked at +0x3C (`Gp_LinkRoomObjects`). Same link/flag
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

/// Grid conversion params pointed to by `Gp_GridParams`.
/// `Gp_WorldToGrid` writes `out.vx = (pos.vx + field_14) / field_20` (or -1
/// if that sum is negative), `out.vy = 0`, and
/// `out.vz = (pos.vz + field_18) / field_20` (or -1). `Gp_LocalToGrid`
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

/// Global at `Gp_StateF0`. `Gp_InitStateF0` zeros the object, then writes
/// `field_2B` from `Mc_SaveData.field_F` (as `u8`), or 4 when that byte is
/// 0 and `Mc_SaveData.field_E != 0`. `Gp_IsDebugAttachRoom() == 1` forces
/// `field_2B = 0` instead. `field_0` is a state byte (1 if first set by
/// `Gp_ArmStateF0`; 2 when the last `field_6` ref is released). `field_1`
/// is an alternate-active flag (`Gp_IsStateF0Active` / `func_800A7CB0` /
/// `Gp_EnqueueSndCdIfF0` / `Gp_CdIdleIfF0Active`); last-ref release sets it to 0x3C.
/// `field_2` is a bitset (`Gp_SetStateF0Bit` sets bit `arg0 - 1` when
/// `arg0 != 0`; also written as `D_801153F2`). `field_3` is cleared with
/// `field_2` on last-ref release (also written as `D_801153F3` by
/// `Gp_SetStateF0Byte3`). `field_4` is also `D_801153F4`. `field_5` is a u8 count incremented by `Gp_ClaimSlot18`
/// when it claims a `GpSlot18`. `field_6` is a u16
/// refcount incremented by `Gp_IncStateF0Ref` and decremented by
/// `Gp_ReleaseStateF0Add` / `Gp_ReleaseStateF0Clear` / `Gp_ReleaseStateF0`. Last-ref
/// release in `Gp_ReleaseStateF0Clear` also clears words at 0x8 / 0xC / 0x10.
/// `Gp_ReleaseStateF0Add` then adds `arg0->field_20->field_50` `field_6` /
/// `field_8` / `field_A` into those same words.
/// `func_800E2C78` adds into
/// `field_14` when `(arg1 & 0x7F)` is 0x19..0x1B.
/// `field_18`..`field_2A` are unknown bytes cleared by `Gp_InitStateF0`.
/// Full object may still be larger than 0x2C (`Gp_PendingObj4CFlag` is a separate
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

/// 0xC slot in the 32-entry table at `Gp_LockSlots`. `Gp_ClearLockSlots` clears
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

/// Overlay used by `Gp_ProjectToSxy`. `field_8` is a `GsCOORDINATE2*`
/// (`workm` is loaded as both rotation and translation). `field_C` /
/// `field_10` / `field_14` are the low halves of a `VECTOR3` at +0xC
/// (same layout `GpLockPos.pos` / `Gp_GetLockPos` loads as three words).
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

/// Source for `Gp_GetLockPos` (`get_lock_pos`). `coord` is the world
/// transform; `pos` is the local `VECTOR3`. Overlays `GpPerspSrc`.
typedef struct _GpLockPos {
    /* 0x00 */ byte                   pad_0[8];
    /* 0x08 */ struct _GsCOORDINATE2* coord;
    /* 0x0C */ VECTOR3                pos;
} GpLockPos;
STATIC_ASSERT_SIZEOF(GpLockPos, 0x18);

/// 0x38-byte scratch from `G_SCRATCH_HEAD` used by `func_800DA2A0`.
/// `src` is the actor's `coord.t` (lowered by 1000 on Y) before
/// `D_80070F34` rotates it into `self`, the world-space aim origin.
/// `node` is the candidate `Gp_LinkList` node's world position; both are
/// handed to `func_800E0308` as the line-of-sight segment.
typedef struct _GpLockScanScratch {
    /* 0x00 */ SVECTOR self;
    /* 0x08 */ SVECTOR node;
    /* 0x10 */ SVECTOR src;
    /* 0x18 */ byte    pad_18[0x20];
} GpLockScanScratch;
STATIC_ASSERT_SIZEOF(GpLockScanScratch, 0x38);

/// 0x14-byte scratch from `G_SCRATCH_HEAD` used by `Gp_ProjectToSxy`.
/// `vec` is the packed `SVECTOR` fed to RTPS. `p` / `flag` / `otz` hold
/// IR0, FLAG, and `SZ3 >> 2`.
typedef struct _GpPerspScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     p;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     otz;
} GpPerspScratch;
STATIC_ASSERT_SIZEOF(GpPerspScratch, 0x14);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `Gp_GetObjPan` and
/// `func_800D9DFC`. Same RTPS outputs as `GpPerspScratch`, plus the packed
/// `SXY2` at 0x14 (`sx` / `sy` are the projected screen coords).
typedef struct _GpPanScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     p;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} GpPanScratch;
STATIC_ASSERT_SIZEOF(GpPanScratch, 0x18);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `Gp_UpdateActorColor`.
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

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `Gp_LightFalloff` /
/// `Gp_LightPoint` / `Gp_LightPointRoom`.
/// `vec` is the halved XYZ from `GpObj44.field_18` (`Gp_LightFalloff`) or
/// from `field_38 -` a world `VECTOR3` (`Gp_LightPoint` / `Gp_LightPointRoom`).
/// `distSq` is `vx²+vy²+vz²`. `outerSq` / `innerSq` are `(radius²) >> 2`
/// from `field_5C` / `field_58` (`Gp_LightPointRoom` first stores
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

/// 0x2C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_LightCone`.
/// `vec` is the halved `field_24.t -` world `VECTOR3`. `dir` is the
/// `Gfx_NormalizeLightDir` result at `head - 0x1C`. `distSq` / `outerSq`
/// / `innerSq` / `scale` match `GpAttnScratch`. `cosAng` is
/// `-(dir · matrix column 2) >> 12`, compared with `rcos(field_68 >> 1)`.
typedef struct _GpSpotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ s32     distSq;
    /* 0x1C */ s32     outerSq;
    /* 0x20 */ s32     innerSq;
    /* 0x24 */ s32     scale;
    /* 0x28 */ s32     cosAng;
} GpSpotScratch;
STATIC_ASSERT_SIZEOF(GpSpotScratch, 0x2C);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by `Gp_FindNearestSlot`.
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

/// 0x20-byte scratch from `G_SCRATCH_HEAD` used by `func_800E25F8`.
/// `local` first holds `GpEnemy.field_1C`, which `field_18->workm` rotates
/// into `world`; `world` then gets `workm.t[]` added to become a world
/// position, and `local` is reused for the delta against the player
/// coordinate whose length feeds `SquareRoot0`.
typedef struct _GpDistScratch {
    /* 0x00 */ VECTOR3 local;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ VECTOR3 world;
    /* 0x1C */ s32     pad_1C;
} GpDistScratch;
STATIC_ASSERT_SIZEOF(GpDistScratch, 0x20);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_800E0FEC`.
/// Each `GpRec18` whose `field_4` high halfword is `0x10` contributes to
/// one accumulator, selected by `field_4` bits `0xF00`: kind 0 sums
/// `field_2 * field_10/12/14` into `acc[0]`, kind 1 writes the lift
/// `-(field_2 << 12)` into `acc[1].vy`, and kind 2 writes the slide
/// `field_2 * field_10/14` into `acc[2]` for the record with the smallest
/// `field_2`. `acc[3]` holds the pairwise XZ products of the kind-0
/// records used to detect opposing pushes.
typedef struct _GpPushScratch {
    /* 0x00 */ VECTOR acc[4];
} GpPushScratch;
STATIC_ASSERT_SIZEOF(GpPushScratch, 0x40);

/// 0x34-byte scratch from `G_SCRATCH_HEAD` used by `func_800E0C10`.
/// `acc[0]` sums `field_10/12/14 * field_2` for every contributing
/// `GpRec18` that sits at or above the floor cutoff (`field_12 >=
/// -0xDDA`); records below it instead accumulate into `acc[1].vy` and
/// bump `count`, so the average of that column can be folded in at the
/// end. `acc[2]` holds the pairwise XZ products used to detect two
/// records pushing in opposing directions.
typedef struct _GpSlideScratch {
    /* 0x00 */ VECTOR acc[3];
    /* 0x30 */ s32    count;
} GpSlideScratch;
STATIC_ASSERT_SIZEOF(GpSlideScratch, 0x34);

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
/// plus `mat.t[0]/t[2]` and `Gp_GridParams` grid offsets, then passed to
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

/// 0x3C-byte scratch from `G_SCRATCH_HEAD` used by `func_800D759C`.
/// `in` is `-GpObj44.field_18` fed to `Gfx_NormalizeLightDir`. `dir` is
/// that output, then the view-rotated copy, then the GPF-scaled color.
/// `mtx` is `Transpose(D_80070F34) * field_4C->workm` (rotation only).
/// `scale` holds `GpObj44.field_4A` loaded into IR0.
typedef struct _GpViewLightScratch {
    /* 0x00 */ VECTOR  in;
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ MATRIX  mtx;
    /* 0x38 */ s32     scale;
} GpViewLightScratch;
STATIC_ASSERT_SIZEOF(GpViewLightScratch, 0x3C);

/// 0x4C-byte scratch from `G_SCRATCH_HEAD` used by `Gp_OrientAlong`.
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
/// `Gp_ObjWorldPos` writes world-space positions into `pos0` / `pos1`.
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
/// `D_8010F888 == 1` requests `Gp_DispatchMsg(..., 0x402, ...)`.
extern s32 D_8010F888;

/// Signed pending item id consumed by `Gp_FlushPendingRelated`. `func_800D5B14`
/// stores the id, or its negation for the second `GpItemSlot` pair.
/// `func_800CE294` also consumes it (with `D_8010F890`) via `func_801088D4`.
extern s32 Gp_PendingRelatedId;

/// Non-zero when `Gp_PendingRelatedId` should be applied by `func_800CE294`.
extern s32 D_8010F890;

/// Pending id consumed by `func_800CE294`; `0x3E` also calls `func_8010A1B0`.
extern s32 D_8010F894;

/// Head of the `GpLinkNode` list walked by `Gp_UnlinkNode` / `Gp_LinkNode`
/// / `Gp_HudTrackSlot0`.
extern GpLinkNode* Gp_LinkList;

/// 32-entry marker/slot table cleared by `Gp_ClearLockSlots`.
extern GpSlot70 Gp_LockSlots[0x20];

/// Per-stage pointer table. Index is `GameSessionFrom4.field_3 - 1`.
/// Each entry is an array of `GpRoomCoordRec*`, indexed by `field_2 - 1`.
extern GpRoomCoordRec** Gp_RoomCoordTables[];

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
/// Each entry is an array of `GpRoomParamRec**`, indexed by `field_6 - 1`.
/// Each of those is an 8-entry array of `GpRoomParamRec*` copied into
/// `Gp_RoomParams` by `Gp_LoadRoomParams`.
extern GpRoomParamRec*** Gp_RoomParamTables[];

/// Default 8-byte record copied by `Gp_CopyDefaultBound`. Also the fallback
/// pointer returned by `Gp_GetRoomBound` when a table lookup fails.
extern GBytes8 Gp_RoomBoundDefault;

/// Default `MATRIX` installed at `GameActorExt.field_1C` by `Gp_BindDefaultMtx`.
extern MATRIX Gp_DefaultMtx;

/// Default `MATRIX` installed at `GameActorExt.field_20` by `Gp_BindDefaultMtx`.
extern MATRIX Gp_DefaultMtx2;

/// Flag set by `Gp_SetOverrideVec` when an override SVECTOR is stored at
/// `Gp_OverrideVec`. Cleared when that function is called with NULL, and
/// also by `Gp_BindDefaultMtx`.
extern u8 Gp_OverrideVecFlag;

/// Override SVECTOR copied by `Gp_SetOverrideVec` from its argument.
extern SVECTOR Gp_OverrideVec;

/// Word cleared by `Gp_BindDefaultMtx`. Also written by `func_800A45F0` and
/// read/cleared by `func_800D8684`.
extern s32 D_80114F28;

/// Flag set by `Gp_SetOverrideVec2` when an override SVECTOR is stored at
/// `Gp_OverrideVec2`. Cleared when that function is called with NULL, and
/// also by `Gp_BindDefaultMtx`.
extern u8 Gp_OverrideVec2Flag;

/// Override SVECTOR copied by `Gp_SetOverrideVec2` from its argument.
extern SVECTOR Gp_OverrideVec2;

/// 8.8 fixed-point pair lerped toward projected screen coords by
/// `func_800D9DFC`. Reset to `0xFFF00000` by `Gp_ResetLinkState`.
extern s32 D_8010F9EC;
extern s32 D_8010F9F0;

/// Current `Gp_LinkList` node whose lock-on reticle `func_800D9DFC` is
/// drawing. Cleared when the walk finds no live target.
struct _GpLinkXform;
extern struct _GpLinkXform* D_80115260;

/// Lerp / settle counter for that reticle. `< 5` eases `D_8010F9EC` /
/// `D_8010F9F0` toward the projected coords (small sprite); `0xFF` snaps.
/// Reset to `0` on target change and when the list is empty.
extern s32 D_80115264;

/// Per-stage `GpGiveRec` lists selected by `Gp_GrantLocationItems` when
/// `Mc_SaveData.field_F` is 0 or 2. Indexed by `GameSession.field_7`.
extern GpGiveRec* D_8010F9F4[];

/// Per-stage `GpGiveRec` lists selected by `Gp_GrantLocationItems` when
/// `Mc_SaveData.field_F` is not 0 or 2. Indexed by `GameSession.field_7`.
extern GpGiveRec* D_8010FA0C[];

/// Pair-handler table used by `func_800DB900` / `func_800E0414`.
/// Indexed by `D_8010FA4C[].field_0` (`Gp_PairNop` / `func_800DBCAC` /
/// `func_800DBE7C`).
extern GpPairFn D_8010FA38[5];

/// 4x4 pair-rule table used by `func_800DB900` / `func_800E0414`.
/// Rows/cols are `(flags & 7) - 1`. `field_0` selects `D_8010FA38`;
/// a non-zero `field_2` swaps the two `GpObj` arguments.
extern GpU16Pair D_8010FA4C[4][4];

/// Nine-entry table of `GpObj` list heads (`D_80115570` .. `D_80115590`).
/// `Gp_LinkObj` appends to `Gp_ObjLists[index]`; `Gp_UnlinkObj` unlinks.
extern GpObj* Gp_ObjLists[9];

/// Two-entry table of `GpObj4A` list heads. `Gp_LinkObj4A` appends to
/// `Gp_Obj4ALists[index]`; `Gp_ClearObj4AList` walks and clears that list.
extern GpObj4A* Gp_Obj4ALists[2];

/// One-entry table of `GpObj3A` list heads. `Gp_LinkObj3A` appends to
/// `Gp_Obj3ALists[index]`; `Gp_ClearObj3AList` walks and clears that list.
extern GpObj3A* Gp_Obj3ALists[1];

extern GpStateF0 Gp_StateF0;
extern u8        D_801153F2;
extern u8        D_801153F3;
extern u8        D_801153F4;
/// Head of the `GpObj3A` list pointed to by `Gp_Obj3ALists[0]`.
extern GpObj3A* D_80115550;
/// Head of the `GpObj4C` list walked by `Gp_CommitObj4CSave`.
extern GpObj4C* Gp_Obj4CList;
extern GpObj4C* Gp_PendingObj4C;
extern GpObj*   D_80115570;
extern GpObj*   D_80115574;
extern GpObj*   D_80115578;
extern GpObj*   D_8011557C;
extern GpObj*   D_80115580;
extern GpObj*   D_80115584;
extern GpObj*   D_80115588;
extern GpObj*   D_8011558C;
extern GpObj*   D_80115590;
extern u8       D_80115598;
/// Set to 1 by `Gp_TakePendingObj4C` when a pending `Gp_PendingObj4C` node is found;
/// `func_800DB72C` then calls `Gp_ClearPendingObj4C` to clear those flags.
extern s32 Gp_PendingObj4CFlag;
/// 8-word table filled by `Gp_LoadRoomParams` from the current room's
/// `Gp_RoomParamTables` records (`field_3`). Indexed by `(id & 7)` in
/// `func_800E0C10` / `func_800E0FEC`.
extern s32 Gp_RoomParams[8];
/// Grid conversion params used by `Gp_WorldToGrid` / `Gp_LocalToGrid`.
/// Cleared by `Gp_ClearObjHeads`; `func_800E0540` skips work when this is NULL.
extern GpGridParams* Gp_GridParams;

/// 4-byte records selected by `Gp_LookupIdField(..., 0)`.
extern GpRec4 Gp_IdField0[];

/// 6-byte records selected by `Gp_LookupIdField(..., 1)`.
extern GpRec6 Gp_IdField1[];

/// 10-byte records selected by `Gp_GetIdParam2` when the id's 0x8000 bit is
/// clear. Indexed by `id & 0x7F`.
extern GpRec10 Gp_IdParamLo[];

/// 16-byte records selected by `Gp_GetIdParam2` when the id's 0x8000 bit is
/// set. Indexed by `id & 0x7F`.
extern GpRec16 Gp_IdParamHi[];

/// u16 scale table indexed by `GpObj5C.field_5C`. `Gp_ObjFlag4Expired` multiplies
/// `field_50->field_E` by the selected entry and divides by 100.
extern u16 D_80113D28[];

/// u16 scale table indexed by `GpObj5D.field_5D`. `Gp_TickObjFlag2` multiplies
/// `field_50->field_C` by the selected entry and divides by 100.
extern u16 D_80113D30[];

/// u16 scale table indexed by `GpObj5C.field_5C`. `Gp_TickObjFlag4` multiplies
/// `field_50->field_4` by the selected entry and divides by 100.
extern u16 D_80113D38[];

/// Damage-scale rows used by `Gp_ScaleDamage`. Indexed by `D_8011541B`.
extern GpDmgRow Gp_DmgRows[];

/// Column index table for `Gp_DmgRows`, indexed by signed HP / 10.
extern u16 D_80113F54[];

/// Percent scale table used by `Gp_ScaleDamage` when `Gp_StateC08.field_C`
/// is non-zero. Indexed by `((field_C / 16) - 1) * 2 + (s8)(field_C % 16)`.
extern u16 D_80113CFC[];

/// Row index into `Gp_DmgRows` for `Gp_ScaleDamage`.
extern u8 D_8011541B;

/// Per-sub-id damage rows used by `func_800E1FEC`. The row is the id's
/// `(id >> 8) & 0x3F` nibble pair; the column is the class picked from
/// `D_80113864` (or 5). The selected entry is scaled `<< 8` then / 100.
extern u16 D_80113568[][8];

/// Column table used when `GpRec10.field_4` is 6, indexed by the distance
/// class picked from `D_80113864`. Scaled `<< 12` then / 100 by
/// `func_800E25F8`.
extern u16 D_80113858[];

/// Distance/hit class table for `D_80113568`, indexed by `hits / 1000` (or by
/// `SquareRoot0(distance) / 1000` in `func_800E25F8`) when that value is
/// below 0x10. `func_800E1FEC` only keeps the low byte of the entry.
extern u16 D_80113864[];

/// Percent scale table used by `func_800E1FEC` / `func_800E25F8` when
/// `Gp_StateC08.field_D` is non-zero. Indexed by
/// `((field_D / 16) - 1) * 2 + (s8)(field_D % 16)`; `func_800E1FEC` reads
/// `field_0` and `func_800E25F8` reads `field_2` of each 4-byte slot.
extern u16 D_80113D0C[][2];

/// Final percent scale applied by `func_800E1FEC`, indexed by `D_8011541B`.
extern u16 D_80113F90[];

/// "Weapon" string drawn by `Gp_DrawWeaponLabel` (trailing 0x60 byte).
extern const char Gp_StrWeapon[];

/// "#######get_lock_pos ---> NULL!!!\n" printed by `Gp_GetLockPos`
/// (trailing 0x8C 0x16 bytes).
extern const char Gp_StrGetLockPosNull[];

/// Returns 1 if item `arg0` cannot be used, 0 if it can.
/// `arg1` supplies `field_2` (capacity) for ammo ids 0xA0–0xBF.
s32 Gp_ItemIsUnusable(s32 arg0, GpItemRec* arg1);
/// `arg1` is passed by `func_80106C6C` (the actor's `field_960`) but the body
/// ignores it.
s32        Gp_FlushPendingRelated(s32 arg0, s32 arg1);
GpItemRec* Gp_FindItemById(s32 arg0);
GpItemRec* Gp_FindItemByKind(s32 arg0);
GpItemRec* Gp_FindItemInScan(s32 arg0, GpItemScan* arg1);
void       Gp_DrawWeaponLabel(Task* arg0);
/// First-run init plus per-frame update of the current room's `GpRoomCoordSet`
/// coordinate arrays (parented to `D_80070F10`) and the `Gp_RoomCoords` slots.
/// Kills `arg0` when `Gp_GetRoomCoordSet` returns 0.
void Gp_UpdateRoomCoords(Task* arg0);
s32  Gp_LightPointRoom(GpObj44* arg0, VECTOR3* arg1);
s32  Gp_LightPoint(GpObj44* arg0, VECTOR3* arg1);
s32  Gp_LightCone(GpObj68* arg0, VECTOR3* arg1);
void func_800D759C(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void func_800D7A9C(GameActorExt* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_800D8684(Task* arg0);
/// Remaps a 3x3 color matrix (`MATRIX.m`) from lighting mode `arg2`
/// (`field_4E` bits 0-1, or bits 2-3 when blending). Mode 1 weights
/// RGB as (7,6,3)/33 then *4/*2/*1. Mode 2 zeros the matrix. Mode 3
/// fills 0x180/0x100/0x100. Default remaps to *3/*1/*3 when
/// `field_4C & 0xC`. Bit 0x80 of `field_4E` with `field_4B == 0` applies
/// a `rsin(Display_State.field_14 << 6)` flicker and clears the bit.
void Gp_RemapActorColor(struct _GpEnemy* arg0, MATRIX* arg1, s32 arg2);
/// Rebuilds the actor color matrix via `func_800D7A9C`, then remaps it
/// from `field_4E` lighting mode (`Gp_RemapActorColor`). While `field_4F` is
/// a positive blend timer, GPF/GPL-interpolates the previous mode
/// (`field_4E` bits 2-3) toward the current mode (bits 0-1). Skips work
/// when `Game_Session->field_65 == 1` unless `GameActorExt.field_C` bit
/// 0x80 is clear and `field_18` is set. `D_801153F4` freezes the timer.
void            Gp_UpdateActorColor(struct _GpEnemy* arg0, VECTOR* arg1);
void            Gp_LightFalloff(GpObj44* arg0);
void            Gp_SetLightMode(GpObj4C* arg0, s32 arg1);
s32             Gp_GetObjDepth(GpObj38* arg0);
s32             Gp_GetObjPan(GpObj38* arg0);
void            Gp_SetOverrideVec(SVECTOR* arg0);
void            Gp_SetOverrideVec2(SVECTOR* arg0);
void            Gp_SetObjTrans(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3);
GpRoomBoundVec* Gp_GetRoomBound(GameSessionFrom4* arg0);
s32             Gp_CountRoomCoords(void);
s32             Gp_GetRoomCoordSet(GameSessionFrom4* arg0);
void            func_800D96C8(Task* arg0);
s32             Gp_GetObjLuma(GpObj44* arg0);
s32             Gp_GetObjTransX(GpObj38* arg0);
void            func_800D9794(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void            func_800D98C4(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void            func_800D9A30(s32 arg0, GpObj44* arg1, VECTOR* arg2, GpObj20* arg3);
void            Gp_InsertRankedSlot(GpRec12* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void            Gp_BindDefaultMtx(Task* arg0);
void            Gp_FillSVec3x3(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3);
GpRoomCoordRec* Gp_GetRoomCoordRec(GameSessionFrom4* arg0);
void            func_800D9CC8(Task* arg0);
void            Gp_CopyDefaultBound(GBytes8* arg0);
void            func_800D9DFC(void);
void            func_800DA6E8(void* arg0, s32 arg1);
void            func_800DA7B8(void);
void            Gp_UnlinkNode(GpLinkNode* node);
void            Gp_LinkNode(GpLinkNode* node);
s32             Gp_NodeSlotMask(GpLinkNode* arg0);
void            Gp_AssignNodeSlot0(GpLinkNode* node);
void            Gp_ClearNodeSlots(GpLinkNode* node);
void*           func_800DA2A0(GpActorWork* arg0, VECTOR3* out, s32 flag);
void*           Gp_FindLockNode(GpActorWork* arg0);
void*           Gp_FindLockNodePad(GpActorWork* arg0);
void*           Gp_FindLockNodeAt(GpActorWork* arg0, VECTOR3* pos);
void            Gp_GetLockPos(GpLockPos* arg0, VECTOR3* out);
void            Gp_ClearLockSlots(void);
void            Gp_ResetLinkState(void);
s32             Gp_ProjectToSxy(GpPerspSrc* arg0, s32* sxy);
void            Gp_ClearSlotNodeFlags(void);
s32             Gp_GrantLocationItems(GpItemScan* arg0);
s32             Gp_LoadActorImage(GpActorWork* arg0, GpImgRec* arg1, RECT* arg2);
void            Gp_LoadImages(GpImgRec* arg0);
void            Gp_InitStateF0(void);
void            Gp_ArmStateF0(s32 arg0);
void            Gp_SetStateF0Bit(s32 arg0);
void            Gp_SetStateF0Byte3(s32 arg0);
void            Gp_IncStateF0Ref(void);
void            Gp_ReleaseStateF0Add(GpObj20E* arg0);
void            Gp_ReleaseStateF0Clear(void);
void            Gp_ReleaseStateF0(void);
void            func_800DB72C(void);
void            func_800DB900(GpObj* node);
void            func_800DBA20(GpObj* arg0, GpObj* arg1, GpSphereScratch* arg2);
s32             func_800DBCAC(GpObj* arg0, GpObj* arg1);
void            func_800DC528(GpObj* node);
void            func_800DCB80(GpObj* node);
void            func_800DD940(GpObj* node);
void            func_800DDC2C(GpObj* arg0);
void            func_800DDDF8(GpObj* node);
void            func_800DE2C0(VECTOR* arg0, s32 arg1);
s32             func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3);
void            func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1);
void            func_800DEC80(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3);
void            func_800DEF80(GpObj* node, GpObj4C* other);
s32             func_800DFCCC(GpObj3A* arg0, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3);
void            Gp_ClearObjHeads(void);
s32             func_800E0308(SVECTOR* arg0, SVECTOR* arg1);
void            func_800E0414(GpObj* a, GpObj* b);
void            func_800E0540(GpObj* node);
void            func_800E0608(GpObj* node, s32 mask, s32 match);
void            func_800E06AC(GpObj* node, s32 mask, s32 match);
s32             Gp_PairNop(void);
void            Gp_LocalToGrid(VECTOR3* arg0, SVECTOR3* arg1);
void            Gp_ObjWorldPos(GpObj* arg0, VECTOR3* arg1);
void            Gp_ClearPendingObj4C(void);
void            Gp_WorldToGrid(VECTOR3* arg0, SVECTOR3* arg1);
/// Averages the first `arg2` `GpRec18` records of `arg0` into `arg1`
/// (a 16.16 delta scaled by 16) and, when `arg3` is non-NULL, stores the
/// `1 << field_4` bitmask of the contributing records there. Records
/// below the floor cutoff are averaged separately and added on top.
/// Returns 0 when nothing contributed, 2 when two records push in
/// opposing directions, and 1 otherwise.
s32 func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
/// Accumulates the push-back of the first `arg2` `GpRec18` records of
/// `arg0` into `arg1` (a 16.16 delta scaled by 16) and, when `arg3` is
/// non-NULL, stores the `1 << field_4` bitmask of the contributing
/// records there. Returns 0 when nothing contributed, 2 when two kind-0
/// records push in opposing directions, and 1 otherwise.
s32 func_800E0FEC(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
/// Transforms `arg0`'s local offset (`GpActorD4Rec` at `field_C` plus the
/// 0x10 SVECTOR) by `field_8->workm` and returns the 1-based index of the
/// closest occupied `GpRec18` in `rec->field_14` whose `field_4` high 16
/// bits match `arg1`, or 0 if none match.
s32  Gp_FindNearestSlot(GpObj* arg0, s32 arg1);
void Gp_LinkObj(s32 arg0, GpObj* arg1);
void Gp_UnlinkObj(GpObj* node);
void Gp_LinkObj4A(s32 arg0, GpObj4A* arg1);
void Gp_UnlinkObj4A(s32 arg0, GpObj4A* arg1);
void Gp_ClearObj4AList(s32 arg0);
void Gp_LinkObj3A(s32 arg0, GpObj3A* arg1);
void Gp_UnlinkObj3A(s32 arg0, GpObj3A* arg1);
void Gp_ClearObj3AList(s32 arg0);
void Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2);
void Gp_LoadRoomParams(void);
s32  Gp_FindRec18(GpRec18* arg0, s32 arg1);
s32  Gp_CountRec18Hi(GpRec18* arg0, s32 arg1);
void Gp_ClearRec18Occupied(GpRec18* arg0);
s32  func_800E1ACC(u8* arg0);
s32  func_800E1B24(s32 arg0);
void Gp_CommitObj4CSave(void);
s32  Gp_TakePendingObj4C(u16* arg0, u8* arg1, u8* arg2);
void Gp_ClaimSlot18(GpObj54* arg0, void* arg1);
/// Builds a rotation matrix in `arg1` that orients along normalized `arg0`
/// (yaw from XZ, pitch from Y vs the XZ length, then roll by `arg2`).
void Gp_OrientAlong(VECTOR* arg0, MATRIX* arg1, s32 arg2);
/// Packed-id enemy damage roll. `arg0` must have high bits `0x20000`. Ids
/// without bit 0x8000 read `Gp_IdParamLo`, scale by a random 100..119 percent,
/// by the `D_80113568` row for `(arg0 >> 8) & 0x3F`, and by `arg3` when
/// `arg2` matches the record's `field_4`; ids with bit 0x8000 read
/// `Gp_IdParamHi` and scale by a random 100..109 percent. `arg1` is a hit
/// count that selects the `D_80113568` column through `D_80113864`.
u32 func_800E1FEC(u32 arg0, u32 arg1, s32 arg2, s32 arg3);
/// Packed-id damage scale. `arg0` must have high bits `0x40000`; low 12 bits
/// are the power and bits 12-15 are written to `*arg2` when it is non-NULL.
/// `arg3 == 0` uses `Wip_SysConfig.field_18` and `GpDmgRow.field_A`;
/// otherwise `Mc_SaveData.field_6C8` and `GpDmgRow.field_0`.
s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3);
/// Rolls a status/effect chance for `arg0` against the player. Returns 0 for
/// ids with bit 0x8000 set, when no slot 3 is active, or when
/// `GpPairSrcE.field_B` scaled by 1/100 is zero. Otherwise the enemy's world
/// distance to the player picks a `D_80113864` class, that class selects a
/// percentage from `D_80113858` (when `GpRec10.field_4` is 6) or from the
/// `D_80113568` row for `(arg1 >> 8) & 0x3F`, and column 6 (or 7 with bit
/// 0x4000) of that same row scales `field_B`. `GpEnemy.field_4C` bit 1
/// doubles the chance, `Gp_StateC08.field_D` applies a `D_80113D0C` percent,
/// and `arg2` multiplies it when non-zero. The result is compared against a
/// 12-bit `Gp_LcgState` draw.
s32  func_800E25F8(struct _GpEnemy* arg0, u32 arg1, s32 arg2);
void Gp_ApplyObjKind(GpObj5D* arg0, s32 arg1);
s32  Gp_PackObjPair(GpObj50* arg0, s32 arg1);
s32  Gp_PackPair(GpU16Pair* arg0, s32 arg1);
void func_800E2C78(GpObj40* arg0, s32 arg1, s32 arg2);
s32  Gp_LookupIdField(s32 arg0, s32 arg1);
s32  Gp_GetIdParam0(s32 arg0);
s32  Gp_GetIdParam1(s32 arg0);
void Gp_SetObjFlag4(GpObj5C* arg0, s32 arg1);
s32  Gp_TickObjFlag4(GpObj5C* arg0);
s32  Gp_ObjFlag4Expired(GpObj5C* arg0);
void Gp_SetObjFlag1(GpObj4C* arg0);
void Gp_SetObjFlag2(GpObj5D* arg0, s32 arg1);
s32  Gp_TickObjFlag2(GpObj5D* arg0);
s32  Gp_GetIdParam2(s32 arg0);
void func_800E337C(Task* arg0);
void func_8010154C(void);

#endif // GAMEPLAY_3A34_H
