#ifndef ACTORS_SHARED_80137E18_H
#define ACTORS_SHARED_80137E18_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/coord.h"

#include "actors/actor.h"
#include "main/task.h"

/// Work block the carriers hang off `Task::work`. `coord` is the extra
/// `GpCoord` this body wires as `sub` of the model's second part;
/// `scale` is that node's X/Y/Z in 4096-per-unit fixed point, which
/// `ActorsShared80137ea8` then reads as `field_34E` for the Y component.
/// `field_36A` is 5 when the 0x600A5 spawn is already armed.
typedef struct ActorShared80137e18Work {
    /* 0x000 */ byte    pad_0[0x2EC];
    /* 0x2EC */ GpCoord coord;
    /* 0x33C */ byte    pad_33C[0x10];
    /* 0x34C */ SVECTOR scale;
    /* 0x354 */ byte    pad_354[0x16];
    /* 0x36A */ s16     field_36A;
    /* 0x36C */ byte    pad_36C[2];
    /* 0x36E */ s16     field_36E;
} ActorShared80137e18Work;
STATIC_ASSERT_SIZEOF(ActorShared80137e18Work, 0x370);

/// Wires the extra coordinate as `sub` of the model's second part, splats an
/// identity rotation into it, clears both nodes' `flg`, and arms the X/Y/Z
/// scale to 0x1000. Unless `field_36A` is 5, also spawns effect 0x600A5 on
/// the model root. Shared verbatim by `actor_107000` and `actor_207000`.
void ActorsShared80137e18(Task* task);

#endif
