#ifndef ACTOR_310600_H
#define ACTOR_310600_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/task.h"

/// 0x538-byte work block `func_actor_310600_80161E64` allocates with
/// `Mem_Calloc` and hangs off `Task::idMap`. The display node at `obj` is
/// linked by `Gp_LinkObj` at spawn (its `field_C` points at `rec`, the
/// `GpRec18` table `Gp_InitRec18Table` fills) and unlinked again by the
/// exit callback `func_actor_310600_80162A24`.
///
/// `light` / `color` are the actor's own lighting and colour matrices;
/// `func_actor_310600_80162A58` republishes them onto the model's
/// `TmdObject::field_1C` / `field_20` in place of the shared defaults
/// `Gp_BindDefaultMtx` installs.
typedef struct Actor310600Work {
    /* 0x000 */ byte    pad_0[0x47C];
    /* 0x47C */ s16     field_47C;
    /* 0x47E */ u16     field_47E;
    /* 0x480 */ MATRIX  light;
    /* 0x4A0 */ MATRIX  color;
    /* 0x4C0 */ GpObj   obj;
    /* 0x4E0 */ GpRec18 rec;
    /* 0x4F8 */ s32     field_4F8;
    /* 0x4FC */ s32     field_4FC;
    /* 0x500 */ s32     field_500;
    /* 0x504 */ byte    pad_504[0x34];
} Actor310600Work;
STATIC_ASSERT_SIZEOF(Actor310600Work, 0x538);

/// Overlay of the `GsCOORDINATE2` at `TmdObject::field_8`, the actor's root
/// part. Offset 0x44 (libgs `param`, `super` at 0x48) holds the Euler angles
/// `func_actor_310600_80162AD8` writes and then hands straight to `RotMatrix`,
/// the same reuse `ActorsShared80132860Coord` documents for its own overlay.
typedef struct Actor310600Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor310600Coord;
STATIC_ASSERT_SIZEOF(Actor310600Coord, 0x4C);

/// The 0x14-byte command block the actor's state handlers build on the stack
/// and hand to `func_actor_310600_8016246C`, which reads it as
/// `{animId, state, path, param}`.
typedef struct Actor310600Cmd {
    /* 0x00 */ s32 animId;
    /* 0x04 */ s32 state;
    /* 0x08 */ s32 path;
    /* 0x0C */ s32 param;
    /* 0x10 */ s32 unk10;
} Actor310600Cmd;

void func_actor_310600_80162A58(Task* arg0);

void func_actor_310600_8016246C(Task* task, s32 arg1, Actor310600Cmd* cmd, s32 arg3);

#endif
