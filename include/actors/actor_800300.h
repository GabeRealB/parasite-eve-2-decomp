#ifndef ACTOR_800300_H
#define ACTOR_800300_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// 0x18-byte `G_SCRATCH_HEAD` block `func_actor_800300_80162064` takes for the
/// ground-quad heading it copies into `GameActor.field_88` / `_94` / `_A0`.
/// `func_800EA1A8` also fills the block as a `VECTOR3` from `coord->workm.t`.
typedef struct {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} Actor800300VecScratch;
STATIC_ASSERT_SIZEOF(Actor800300VecScratch, 0x18);

/// View of `GameActor.field_973` as the unsigned byte its rotation
/// multiply sign-extends.
typedef struct {
    byte pad[0x973];
    u8   field_973;
} Actor800300DirByte;

void func_actor_800300_80162064(GpActorWork* arg0);
void func_actor_800300_801623F8(GpActorWork* arg0);
void func_actor_800300_80162C2C(GpActorWork* arg0);

#endif
