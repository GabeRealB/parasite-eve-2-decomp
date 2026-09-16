#ifndef ACTORS_SHARED_80133610_H
#define ACTORS_SHARED_80133610_H

#include "common.h"

/// Work block reached as `task->field_1C`, shared by the two enemy overlays
/// that carry this body. `field_320` is the pose the actor asks for, `field_322`
/// the pose its slots were last re-queued for and `field_324` the frame count
/// accumulated while the two agree.
typedef struct ActorShared80133610Work {
    /* 0x000 */ byte pad_0[0x320];
    /* 0x320 */ u16  field_320;
    /* 0x322 */ s16  field_322;
    /* 0x324 */ u16  field_324;
} ActorShared80133610Work;

/// The task this body is handed.
typedef struct ActorShared80133610 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80133610Work* field_1C;
} ActorShared80133610;

/// Per-state animation id handed to `func_800B4114`, indexed by `field_320`.
/// The name is shared, the table is not: each carrier defines it at its own
/// address.
extern s16 ActorsShared80133610Table[];

void ActorsShared80133610(ActorShared80133610* arg0);

#endif // ACTORS_SHARED_80133610_H
