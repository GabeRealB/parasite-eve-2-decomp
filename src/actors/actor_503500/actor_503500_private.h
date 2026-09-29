#ifndef ACTOR_503500_PRIVATE_H
#define ACTOR_503500_PRIVATE_H

#include "actors/actor_503500.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_actor_503500_8016E9F0[5];

void func_actor_503500_8013BE8C(Task* task);
void func_actor_503500_8013CA8C(Task* task);
void func_actor_503500_8013DBF4(Task* task);
void func_actor_503500_8013EC64(Task* task);
void func_actor_503500_8013FA1C(Task* task);
void func_actor_503500_80142370(Task* task);
void func_actor_503500_801442A8(Task* task);
void func_actor_503500_80144890(Task* task);
void func_actor_503500_80144E34(Task* task);
void func_actor_503500_8014554C(Task* task);
void func_actor_503500_801459D4(Task* task);
void func_actor_503500_80145F84(Task* task);

// Callbacks referenced by the overlay's shared data tables.
s32 func_actor_503500_80133BF4(Task *, Actor503500Work *);
s32 func_actor_503500_80134284(Task *, Actor503500Work *);
s32 func_actor_503500_801364D0(Task *, Actor503500Work *);
s32 func_actor_503500_8013656C(Task *, Actor503500Work *);
s32 func_actor_503500_8013667C(Task *, Actor503500Work *);
s32 func_actor_503500_80136770(Task *, Actor503500Work *);
s32 func_actor_503500_8013680C(Task *, Actor503500Work *);
s32 func_actor_503500_80136948(Task *, Actor503500Work *);
void func_actor_503500_80137238(Task *);
void func_actor_503500_801384D4(Task *);
void func_actor_503500_8013AD0C(Task *);
void func_actor_503500_80143AC0(Task *);

typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task *, s32, GpAnimArg *, s32);
        s32 (*call1)(Task *, s32, GpCmdArg *);
        s32 (*call2)(Task *, s32, GpXformArg *);
        s32 (*call3)(Task *, s32, s32);
    } handler;
} Actor5035003MsgEntry;
STATIC_ASSERT_SIZEOF(Actor5035003MsgEntry, 8);

extern Actor5035003MsgEntry D_actor_503500_8016EA2C[];

// Callbacks referenced by the overlay's shared data tables.
s32 func_actor_503500_80135950(Task *, s32, GpAnimArg *, s32);
s32 func_actor_503500_80135B74(Task *, s32, GpCmdArg *);
s32 func_actor_503500_80137088(Task *, s32, GpXformArg *);
s32 func_actor_503500_80137158(Task *, s32, s32);

#endif // ACTOR_503500_PRIVATE_H
