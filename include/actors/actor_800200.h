#ifndef ACTOR_800200_H
#define ACTOR_800200_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// 8-byte fixed-point X/Z entry of a path table (`D_actor_800200_8016A128`
/// and its neighbours). `GpActorD4.pathStep` selects the entry; the Y
/// component of a destination comes from the actor's own `GsCOORDINATE2`.
typedef struct {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} GpActorPathStep;

/// 0x18-byte `G_SCRATCH_HEAD` block `func_actor_800200_801622B0` takes for the
/// ground-quad heading it copies into `GameActor.field_88` / `_94` / `_A0`.
typedef struct {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} Actor800200VecScratch;

/// View of `GameActor.field_973` as the unsigned byte its rotation
/// multiply sign-extends.
typedef struct {
    byte pad[0x973];
    u8   field_973;
} ActorDirByte;

extern u32 Gp_LcgState;

void func_actor_800200_80162990(GpActorWork* arg0);
void func_actor_800200_80162BFC(GpActorWork* arg0);
void func_actor_800200_80163044(GpActorWork* arg0);
void func_actor_800200_80163180(GpActorWork* arg0);
void func_actor_800200_8016337C(GpActorWork* arg0);
void func_actor_800200_80163584(GpActorWork* arg0);
void func_actor_800200_801637B4(GpActorWork* arg0);
void func_actor_800200_8016390C(GpActorWork* arg0);
void func_actor_800200_80163A54(GpActorWork* arg0);
void func_actor_800200_80163B90();
void func_actor_800200_80163CCC(GpActorWork* arg0);
void func_actor_800200_80163E14(GpActorWork* arg0);
void func_actor_800200_801649D8(GpActorWork* arg0);
void func_actor_800200_80165408(GpActorWork* arg0, s32 arg1);
void func_actor_800200_80165434(GpActorWork* arg0, s16 arg1);
void func_actor_800200_8016545C(GpActorWork* arg0, s8 arg1);
void func_actor_800200_801654EC(GpActorWork* arg0, s32 arg1);
void func_actor_800200_80165534(GpActorWork* arg0);
s32  func_actor_800200_80165104(GpActorWork* arg0);
void func_actor_800200_80165380(GpActorWork* arg0);
void func_actor_800200_801653C0(GpActorWork* arg0);
void func_actor_800200_80165580(GpActorWork* arg0);
void func_actor_800200_80165644(GpActorWork* arg0);
void func_actor_800200_80165708(GpActorWork* arg0);
void func_actor_800200_801652EC(GpActorWork* arg0);
void func_actor_800200_80165814();
void func_actor_800200_801658E0();
void func_actor_800200_8016599C();
void func_actor_800200_801659CC();
void func_actor_800200_80165ACC();
s32  func_actor_800200_801660E8(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2);

#endif
