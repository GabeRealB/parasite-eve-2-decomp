#ifndef ACTORS_SHARED_8013231C_H
#define ACTORS_SHARED_8013231C_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Argument block of the script opcode this body handles: a world translation
/// followed by the Euler angles handed to `RotMatrix`, the same block
/// `ActorsShared80132074` takes. The two opcodes differ only in the order they
/// build the coordinate, which is what makes them separate bodies.
typedef struct ActorsShared8013231cArgs {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} ActorsShared8013231cArgs;
STATIC_ASSERT_SIZEOF(ActorsShared8013231cArgs, 0x18);

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`. Offset 0x44 (libgs
/// `param`, and `super` at 0x48) holds the Euler angles this body writes and
/// then hands straight to `RotMatrix`, the same reuse `ActorsShared80132860Coord`,
/// `HyperCoord` and `GpCoordExt` document for their own overlays.
typedef struct ActorsShared8013231cCoord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} ActorsShared8013231cCoord;
STATIC_ASSERT_SIZEOF(ActorsShared8013231cCoord, 0x4C);

/// Places the actor at `args`: drops the opcode's translation straight into the
/// root part's local matrix, stores its Euler angles in the coordinate's own
/// `rot` slot and rebuilds the rotation from them. Clearing `flg` makes
/// `Gp_UpdateCoordTree` recompute the world matrix from it.
s32 ActorsShared8013231c(Task* task, s32 arg1, ActorsShared8013231cArgs* args);

#endif
