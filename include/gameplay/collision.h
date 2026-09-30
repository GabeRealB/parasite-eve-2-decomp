#ifndef GAMEPLAY_COLLISION_H
#define GAMEPLAY_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

struct GfxCoord;

/// One 4-byte entry of the tables that name a collision body: the two halves
/// `Gp_PackPair` / `Gp_PackObjPair` pack into that body's `WorldCollisionBody.key`, taking
/// the low 12 bits of `field_0` and the low 4 bits of `field_2`. An actor's
/// spawn tables and the one an enemy's `GpPairSrcE.pairTable` points at hold
/// these.
///
/// The halves stay unnamed because their meaning belongs to the table rather
/// than to the type: `GpEdgePair` covers the same bytes as two corner indices,
/// read signed by the helpers that need that width and unsigned by the ones
/// that come through this type.
typedef struct GpU16Pair {
    u16 field_0;
    u16 field_2;
} GpU16Pair;
STATIC_ASSERT_SIZEOF(GpU16Pair, 0x4);

/// A trigger quad on the `Gp_PendingObj4C` / `Gp_Obj4CList` lists.
/// `next` and signed `field_4B` are the `Gp_PendingObj4C` list walked by
/// `Gp_ClearPendingObj4C`, which clears a non-zero `field_4B`. `Gp_TakePendingObj4C`
/// walks the same list and, on a pending `field_4B`, copies `field_46` /
/// `field_48` / `field_49` to its out-params and sets `Gp_PendingObj4CFlag`. The
/// same node type is the `Gp_Obj4CList` list walked by `Gp_CommitObj4CSave`: a
/// pending `field_4B` copies `field_49` into `Mc_SaveData[0].state.at4.loc.view` when
/// `field_48` matches `gGameSession->at4.loc.view`.
/// `func_800DF6AC` tests an object against the quad at `field_14`, using
/// `field_C` as its local origin, `field_34` as its normal, and `field_44`
/// as its bounding radius. `field_8` supplies the coordinate matrices.
/// `func_800DEF80` also tests `field_3C` against the object's forward axis
/// when the low three bits of `field_4A` are 2. Room resources store these
/// contiguously at stride 0x4C; field_4A bit 7 marks the final record.
typedef struct _GpObj4C {
    /* 0x00 */ struct _GpObj4C*  next;
    /* 0x04 */ struct _GpObj4C** prev;
    /* 0x08 */ GfxCoord*         field_8;
    /* 0x0C */ SVECTOR           field_C;
    /* 0x14 */ SVECTOR           field_14[4];
    /* 0x34 */ SVECTOR           field_34;
    /* 0x3C */ SVECTOR           field_3C;
    /* 0x44 */ u16               field_44;
    /* 0x46 */ u16               field_46;
    /* 0x48 */ u8                field_48;
    /* 0x49 */ u8                field_49;
    /* 0x4A */ u8                field_4A;
    /* 0x4B */ s8                field_4B;
} GpObj4C;

/// 0x4C list node appended to `Gp_Obj4ALists[index]` by `Gp_LinkObj4A` and
/// unlinked by `Gp_UnlinkObj4A`. `Gp_ClearObj4AList` empties the whole list.
/// `field_4A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87); bit 0x80 marks the last element of an array walked
/// at +0x4C. Callers also store `gGfxViewCoord` at +0x8 and OR bit 0x40 into
/// `field_4A`.
typedef GpObj4C GpObj4A;
STATIC_ASSERT_SIZEOF(GpObj4A, 0x4C);

/// 0x3C list node appended to `Gp_Obj3ALists[index]` by `Gp_LinkObj3A` and
/// unlinked by `Gp_UnlinkObj3A`. `Gp_ClearObj3AList` empties the whole list.
/// `field_3A` bit 0x20 means the node is on that list (cleared on unlink,
/// keeping bits 0x87). Bit 0x40 is the active filter used by
/// `func_800E0308` before it calls `func_800DFCCC`. Bit 0x80 marks the last
/// element of an array walked at +0x3C (`Gp_LinkRoomObjects`). Same link/flag
/// layout as `GpObj4A`, with the flag byte at 0x3A instead of 0x4A.
/// `func_800DFCCC` transforms the origin, four vertices and face normal
/// into view space to test a segment against the quad.
typedef struct _GpObj3A {
    /* 0x00 */ struct _GpObj3A*  next;
    /* 0x04 */ struct _GpObj3A** prev;
    /* 0x08 */ SVECTOR           origin;
    /* 0x10 */ SVECTOR           verts[4];
    /* 0x30 */ SVECTOR           normal;
    /* 0x38 */ byte              pad_38[2];
    /* 0x3A */ u8                field_3A;
    /* 0x3B */ byte              pad_3B;
} GpObj3A;
STATIC_ASSERT_SIZEOF(GpObj3A, 0x3C);

/// Collision face with corner and normal indices into the grid's vector pools.
typedef struct {
    u16 verts[4];     // Corner indices; 0xFFFF in the fourth slot marks a triangle
    u16 normalIndex;  // Index into the grid's normal pool
    s16 surfaceClass; // Room surface-property table index (0..7), copied into contact keys
} GpGridFace;
STATIC_ASSERT_SIZEOF(GpGridFace, 0xC);

/// Grid conversion params pointed to by `Gp_GridParams`.
/// `Gp_WorldToGrid` writes `out.vx = (pos.vx + field_14) / field_20` (or -1
/// if that sum is negative), `out.vy = 0`, and
/// `out.vz = (pos.vz + field_18) / field_20` (or -1). `Gp_LocalToGrid`
/// applies `field_0->workm` with `ApplyTransposeMatrixLV`, then subtracts
/// `field_0->coord.t[0]` / `t[2]` from the transformed X / Z.
/// `func_800DEAFC` does the same transform on two `SVECTOR`s, keeping only
/// the low 16 bits. `field_4` and `field_8` are `SVECTOR` pools holding face
/// normals and face corners; `field_C` is the `GpGridFace` table indexed by
/// the face ids stored in the `field_10` cell grid. That grid is
/// `field_1C` by `field_1E` cells of `s16*` face-id lists, each terminated by
/// -1, indexed as `field_10[x * field_1E + z]`. `field_22` is the face count.
typedef struct _GpGridParams {
    /* 0x00 */ struct GfxCoord* field_0;
    /* 0x04 */ SVECTOR*         field_4;
    /* 0x08 */ SVECTOR*         field_8;
    /* 0x0C */ GpGridFace*      field_C;
    /* 0x10 */ s16**            field_10;
    /* 0x14 */ s32              field_14;
    /* 0x18 */ s32              field_18;
    /* 0x1C */ u16              field_1C;
    /* 0x1E */ u16              field_1E;
    /* 0x20 */ u16              field_20;
    /* 0x22 */ u16              field_22;
} GpGridParams;
STATIC_ASSERT_SIZEOF(GpGridParams, 0x24);

/// The setup argument of a `D_8010FABC` descriptor: the location whose entry
/// starts the task, packed in decimal as `stage * 10000 + area * 100 + room`,
/// with room 0 matching the whole area.
#define GP_TASK_LOC_KEY(stage, area, room) ((stage) * 10000 + (area) * 100 + (room))

/// `GpImgRec.field_0` of the record that ends a list.
#define GP_IMG_REC_END 0xFF

#endif // GAMEPLAY_COLLISION_H
