#ifndef ACTOR_800200_H
#define ACTOR_800200_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"

/// 8-byte fixed-point X/Z entry of a path table (`D_actor_800200_8016A128`
/// and its neighbours). `GpActorD4.field_CE` selects the entry; the Y
/// component of a destination comes from the actor's own `GsCOORDINATE2`.
typedef struct {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} GpActorPathStep;

void func_actor_800200_80162990(void);
void func_actor_800200_80162BFC(void);
void func_actor_800200_80163044(void);
void func_actor_800200_80163180(void);
void func_actor_800200_8016337C(void);
void func_actor_800200_80163584(void);
void func_actor_800200_801637B4(void);
void func_actor_800200_8016390C(void);
void func_actor_800200_80163A54(void);
void func_actor_800200_80163B90(void);
void func_actor_800200_80163CCC(void);
void func_actor_800200_80163E14(void);
void func_actor_800200_801654EC(GpActorWork* arg0, s32 arg1);
void func_actor_800200_80165104(GpActorWork* arg0);
void func_actor_800200_80165580(GpActorWork* arg0);
void func_actor_800200_80165644(GpActorWork* arg0);
void func_actor_800200_80165708(GpActorWork* arg0);
void func_actor_800200_80165814(void);
void func_actor_800200_801658E0();
void func_actor_800200_8016599C();
void func_actor_800200_801659CC(void);
void func_actor_800200_80165ACC();
s32  func_actor_800200_801660E8(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2);

#endif
