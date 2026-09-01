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
    /* 0x000 */ byte    pad_0[0x480];
    /* 0x480 */ MATRIX  light;
    /* 0x4A0 */ MATRIX  color;
    /* 0x4C0 */ GpObj   obj;
    /* 0x4E0 */ GpRec18 rec;
    /* 0x4F8 */ byte    pad_4F8[0x40];
} Actor310600Work;
STATIC_ASSERT_SIZEOF(Actor310600Work, 0x538);

void func_actor_310600_80162A58(Task* arg0);

#endif
