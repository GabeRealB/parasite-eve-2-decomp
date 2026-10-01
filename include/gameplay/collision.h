#ifndef GAMEPLAY_COLLISION_H
#define GAMEPLAY_COLLISION_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// One attack delivered through a collision body's key.
///
/// The packed identity has contact category 4 in the high halfword
/// (`DAMAGE_ATTACK_CATEGORY`), the low 12 bits of `power` below that, and the
/// low 4 bits of `reaction` in bits 12..15. Enemy parameter records and actor
/// spawn tables hold these entries, and several bodies may share one. A null
/// pointer packs as the zero key, which omits the body from pair contacts; a
/// stored `{ 0, 0 }` still packs as category 4.
///
/// `reaction` selects the player's hit reaction. 0 is the ordinary hit and
/// applies no status. 1 darkness, 2 paralysis, 8 silence, 10 confusion and
/// 11 berserker use that same presentation and also apply that status. 3
/// poison uses its own presentation and applies poison. 9 uses the ordinary
/// presentation and applies status flag 0x20, whose gameplay effect is
/// unproven. 4 sets the parasite-energy fade mask to 8, applies no status,
/// and has no hit presentation of its own. 5, 6 and 7 each use their own hit
/// presentation and set no status flag.
typedef struct DamageAttack {
    u16 power;    // Base damage before the HP-band scale; low 12 bits of the identity
    u16 reaction; // Hit reaction in the low 4 bits; values listed above
} DamageAttack;
STATIC_ASSERT_SIZEOF(DamageAttack, 0x4);

/// Low bits of `DamageAttack::power` copied into an attack identity.
#define DAMAGE_ATTACK_POWER_MASK 0xFFF
/// Low bits of `DamageAttack::reaction` copied into an attack identity.
#define DAMAGE_ATTACK_REACTION_MASK 0xF
/// Bit position of the reaction nibble in the identity halfword.
#define DAMAGE_ATTACK_REACTION_SHIFT 12
/// Packed attack identity's contact category: 4 in the high halfword.
#define DAMAGE_ATTACK_CATEGORY 0x40000

/// A trigger quad on the `Gp_PendingObj4C` / `Gp_Obj4CList` lists.
/// `next` and signed `field_4B` are the `Gp_PendingObj4C` list walked by
/// `Gp_ClearPendingObj4C`, which clears a non-zero `field_4B`. `Gp_TakePendingObj4C`
/// walks the same list and, on a pending `field_4B`, copies `field_46` /
/// `field_48` / `field_49` to its out-params and sets `Gp_PendingObj4CFlag`. The
/// same node type is the `Gp_Obj4CList` list walked by `Gp_CommitObj4CSave`: a
/// pending `field_4B` copies `field_49` into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` when
/// `field_48` matches `gGameSession->location.loc.view`.
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

/// A triangle's missing fourth vertex index.
enum { WORLD_COLLISION_GRID_FACE_NO_VERTEX = 0xFFFF };

/// An indexed triangle or quad in a room's collision grid.
///
/// Indices refer to the owning `WorldCollisionGrid` vertex and normal pools, whose
/// storage must remain alive with the face table. Vertices use grid-local game
/// coordinates; normals use 4096 for unit length. The fourth vertex is
/// `WORLD_COLLISION_GRID_FACE_NO_VERTEX` for a triangle. Sphere-grid passes
/// skip a face whose first two vertex indices are both zero.
///
/// The surface class selects the current room's collision and footstep
/// properties and is copied into the low bits of grid-contact keys. Class
/// meanings are local to the room.
typedef struct {
    u16 vertexIndices[4]; // Vertex-pool indices; only slot 3 may be NO_VERTEX
    u16 normalIndex;      // Index into the grid's normal pool
    s16 surfaceClass;     // Room collision/footstep property index (0..7), copied into contact keys
} WorldCollisionGridFace;
STATIC_ASSERT_SIZEOF(WorldCollisionGridFace, 0xC);

/// End of a cell's signed face-index list; NULL denotes an empty cell.
enum { WORLD_COLLISION_GRID_CELL_END = -1 };

/// Cell coordinate returned when the position lies below the grid's origin.
enum { WORLD_COLLISION_GRID_INVALID_CELL = -1 };

/// A room collision mesh and the XZ cell index used to select its faces.
///
/// Storage is borrowed: the descriptor, mesh pools, cell table and face-index
/// lists must remain live while the grid is active. Templates may have a NULL
/// `viewCoord`; activation binds it to `gGfxViewCoord`. Its composed matrix
/// maps room vertices and normals into view space. Room and actor updates may
/// rebuild the mesh pools in place without changing the cell lists.
///
/// Cell coordinates are `(roomX + xBias) / cellSize` and
/// `(roomZ + zBias) / cellSize`, with `WORLD_COLLISION_GRID_INVALID_CELL` for
/// a negative numerator. `cellSize` must be nonzero. View-space queries first
/// apply the transpose of `viewCoord->workm` and subtract `viewCoord->coord.t`
/// on X and Z. A cell needs `0 <= x < cellCountX` and `0 <= z < cellCountZ`;
/// the cell table
/// has `cellCountX * cellCountZ` entries, indexed as `x * cellCountZ + z`.
///
/// Nonempty cells hold signed indices in `[0, faceCount)`, terminated by
/// `WORLD_COLLISION_GRID_CELL_END`. Active grids must fit the collision pass's
/// 256-face candidate array. Vertex and normal pool lengths are asset-specific,
/// are not stored here, and need not equal `faceCount`; face indices and any
/// runtime edits must fit those pools.
typedef struct WorldCollisionGrid {
    GfxCoord*               viewCoord;   // Borrowed view transform; NULL before binding to the current view
    SVECTOR*                normals;     // Writable room-space normals, 4096 for unit length
    SVECTOR*                vertices;    // Writable room-space vertices in signed game coordinates
    WorldCollisionGridFace* faces;       // Writable indexed triangles/quads; faceCount entries
    s16**                   cellFaceIds; // X-major cell table of face-index lists; NULL entries are empty
    s32                     xBias;       // Additive X offset in game coordinates; negative of the grid origin
    s32                     zBias;       // Additive Z offset in game coordinates; negative of the grid origin
    u16                     cellCountX;  // Number of cells along X
    u16                     cellCountZ;  // Number of cells along Z; stride between X rows
    u16                     cellSize;    // Square cell side length in game coordinates, nonzero
    u16                     faceCount;   // Face-table length; at most 256 for an active grid
} WorldCollisionGrid;
STATIC_ASSERT_SIZEOF(WorldCollisionGrid, 0x24);

/// The setup argument of a `D_8010FABC` descriptor: the location whose entry
/// starts the task, packed in decimal as `stage * 10000 + area * 100 + room`,
/// with room 0 matching the whole area.
#define GP_TASK_LOC_KEY(stage, area, room) ((stage) * 10000 + (area) * 100 + (room))

/// `GpImgRec.field_0` of the record that ends a list.
#define GP_IMG_REC_END 0xFF

#endif // GAMEPLAY_COLLISION_H
