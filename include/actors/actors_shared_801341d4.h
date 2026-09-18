#ifndef ACTORS_SHARED_801341D4_H
#define ACTORS_SHARED_801341D4_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"

/// Partial view of the actor's 0x1C work block. `field_12A` is the scale the
/// cue's volume is derived from; `field_14A` counts the frames between cues.
typedef struct ActorShared801341d4Work {
    /* 0x000 */ byte pad_0[0x12A];
    /* 0x12A */ s16  field_12A;
    /* 0x12C */ byte pad_12C[0x1E];
    /* 0x14A */ u16  field_14A;
} ActorShared801341d4Work;

/// `field_8` is the first of the actor's part coordinates, the one the pan and
/// depth are measured from (same slot as `TmdObject.coords`).
typedef struct ActorShared801341d4Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared801341d4Obj2C;

typedef struct ActorShared801341d4 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared801341d4Work*  field_1C;
    /* 0x20 */ GpEnemy*                  field_20;
    /* 0x24 */ byte                      pad_24[8];
    /* 0x2C */ ActorShared801341d4Obj2C* field_2C;
} ActorShared801341d4;

/// Every 0x19 frames, cues `SndEvt_EnqueueType6`: the id takes the placement's
/// sound index from `GpEnemy.field_8`, the pan and depth come from the part
/// coordinate's first entry, and the depth is attenuated by the work block's
/// `field_12A` scale, clamped to 0x600..0x1D01 and mapped onto 0x32..0x64
/// percent.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared801341d4(ActorShared801341d4* arg0);

#endif
