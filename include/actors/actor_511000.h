#ifndef ACTOR_511000_H
#define ACTOR_511000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. `light` / `color` are the
/// matrices the TMD object's `field_1C` / `field_20` are republished from.
/// Only that pair is spelled out here; the prefix fields are unreferenced by
/// the bodies matched so far.
typedef struct Actor511000Work {
    /* 0x00 */ byte   pad_0[0x30];
    /* 0x30 */ MATRIX light;
    /* 0x50 */ MATRIX color;
} Actor511000Work;

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`. Offset 0x44 (libgs
/// `param`, and `super` at 0x48) holds the Euler angles
/// `func_actor_511000_801336E0` writes and then hands straight to `RotMatrix`,
/// the same reuse `ActorsShared8013231cCoord` documents for its own overlay.
typedef struct Actor511000Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor511000Coord;
STATIC_ASSERT_SIZEOF(Actor511000Coord, 0x4C);

/// Work block of the spawner this overlay's model-attach handlers parent to,
/// reached through the parent task's `Task::idMap`. Only the two matrices the
/// model object is pointed at are modelled here; the same pair sits at the
/// same offsets in `ActorsShared80135b64Work` / `Actor02000Work`.
typedef struct Actor511000ParentWork {
    /* 0x000 */ byte   pad_0[0x43C];
    /* 0x43C */ MATRIX field_43C; ///< colour matrix, handed to TmdObject::field_20
    /* 0x45C */ MATRIX field_45C; ///< light matrix, handed to TmdObject::field_1C
} Actor511000ParentWork;

/// Work block `func_actor_511000_80132480` allocates (`Mem_Calloc(0x4D4, 0)`)
/// and parks in that task's `Task::idMap` slot. Distinct from
/// `Actor511000Work`: this variant's light/color pair sits at 0x484 / 0x4A4
/// and is republished onto model part 1, not the root coordinate.
typedef struct Actor511000Work2 {
    /* 0x000 */ byte   pad_0[0x484];
    /* 0x484 */ MATRIX light;
    /* 0x4A4 */ MATRIX color;
    /* 0x4C4 */ byte   pad_4C4[0x10];
} Actor511000Work2;
STATIC_ASSERT_SIZEOF(Actor511000Work2, 0x4D4);

#endif // ACTOR_511000_H
