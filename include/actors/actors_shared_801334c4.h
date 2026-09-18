#ifndef ACTORS_SHARED_801334C4_H
#define ACTORS_SHARED_801334C4_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Argument block of the script opcode this body handles: a world translation
/// followed by the Euler angles handed to `RotMatrix`, the same block
/// `ActorsShared8013231c` takes. The extra step here is clearing bit 0x80 of
/// the model's `TmdObject::flags` so the placed object is shown.
typedef struct ActorsShared801334c4Args {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorsShared801334c4Args;
STATIC_ASSERT_SIZEOF(ActorsShared801334c4Args, 0x18);

/// Overlay of `GsCOORDINATE2` at `TmdObject::coords`. Offset 0x44 (libgs
/// `param`, and `super` at 0x48) holds the Euler angles this body writes and
/// then hands straight to `RotMatrix`, the same reuse `ActorsShared8013231cCoord`
/// documents for its own overlay.
typedef struct ActorsShared801334c4Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} ActorsShared801334c4Coord;
STATIC_ASSERT_SIZEOF(ActorsShared801334c4Coord, 0x4C);

/// Places the actor at `args`: drops the opcode's translation straight into the
/// root part's local matrix, stores its Euler angles in the coordinate's own
/// `rot` slot, rebuilds the rotation from them, and clears `TmdObject::flags`
/// bit 0x80. Clearing `flg` makes `Gp_UpdateCoordTree` recompute the world
/// matrix from it.
s32 ActorsShared801334c4(Task* task, s32 arg1, ActorsShared801334c4Args* args, s32 arg3);

#endif
