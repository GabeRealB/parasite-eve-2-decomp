#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

extern s16               D_80072830;
extern GpActorFuncTable3 D_actor_800300_80161E34;
extern GpActorFuncTable7 D_actor_800300_80161E64;

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", ActorsShared801328ccTable);

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E34);

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E40);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80162658);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_801628D0);

void func_actor_800300_80162A98(GpActorWork* arg0)
{
    u8*            head;
    VECTOR3*       vec;
    GameActor*     actor;
    GpLinkNode*    node;
    TmdObject*     extra;
    GsCOORDINATE2* src;
    s32            val;
    s32            arg;
    s32            flag;

    actor             = arg0->actor;
    extra             = (TmdObject*)((Task*)Game_GetPtrSlot(3))->extra;
    head              = *(u8**)0x1F8003FC;
    *(u8**)0x1F8003FC = head - 0x10;
    vec               = (VECTOR3*)(head - 0x10);
    node              = actor->field_90C;
    src               = extra->field_8;
    if (node != NULL) {
        if (!(node->field_4 & 1)) {
            Gp_GetLockPos((GpLockPos*)node, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = src->coord.t[0];
        vec->vy                       = src->coord.t[1];
        vec->vz                       = src->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            if (func_8010BCF4((Task*)arg0, vec) < 0) {
                actor->field_93E = -1;
                arg              = 5;
            } else {
                actor->field_93E = 1;
                arg              = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            val              = func_8010BCF4((Task*)arg0, vec);
            if (val < 0) {
                val = -val;
            }
            if ((val < 0x81) || (actor->field_95E == 2)) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)src->coord.t);
    func_80105ED4(arg0);
    *(u8**)0x1F8003FC += 0x10;
}

void func_actor_800300_80162C2C(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable3 sp;

    sp               = D_actor_800300_80161E34;
    actor            = arg0->actor;
    actor->field_973 = 0;
    actor->field_975 = 0;
    sp.funcs[actor->field_954](arg0);
    actor->field_986 = 0;
}

s32  func_8010BC70(GsCOORDINATE2* arg0);
s32  func_8010BCF4(Task* arg0, VECTOR3* arg1);
void func_8010BD88(GpActorWork* arg0, VECTOR3* arg1);
void func_8010BE5C(GpActorWork* arg0, VECTOR3* arg1);
s32  func_80105ED4(GpActorWork* arg0);

void func_actor_800300_80163048(GpActorWork* arg0);
void func_actor_800300_80163074(GpActorWork* arg0);

void func_actor_800300_80162C98(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    s32            val;

    actor  = arg0->actor;
    coord  = arg0->extra->field_8;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    if (arg0->actor->field_910->field_C4 <= 0) {
        func_8010BF7C(arg0, 0x14, 0x3F);
        if ((u32)(func_8010BC70(coord) - 0x581) < 0x87F) {
            func_actor_800300_80163048(arg0);
        }
        val = func_8010BCF4((Task*)arg0, (VECTOR3*)target->coord.t);
        if (val < 0) {
            val = -val;
        }
        if (val >= 0x200) {
            actor->field_90C = NULL;
            func_actor_800300_80163074(arg0);
        }
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
    func_80105ED4(arg0);
}

void func_actor_800300_80162D74(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    GpLockPos*     lock;
    u8*            head;
    VECTOR3*       vec;
    u16            state;

    coord             = arg0->extra->field_8;
    target            = (GsCOORDINATE2*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    head              = *(u8**)0x1F8003FC;
    *(u8**)0x1F8003FC = head - 0x10;
    vec               = (VECTOR3*)(head - 0x10);
    actor             = arg0->actor;
    lock              = (GpLockPos*)actor->field_90C;
    if (lock != NULL) {
        if (!(((GpLinkNode*)lock)->field_4 & 1)) {
            Gp_GetLockPos(lock, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->field_95E;
    switch (state) {
        case 0:
            actor->field_95E = 1;
            actor->field_934 = 0;
            actor->field_958 = 3;
            Gp_AnimPlayChildSlotsEx(arg0, 0xC, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_973 = 1;
            if (func_8010BC70(coord) < 0x601) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
    func_80105ED4(arg0);
    *(u8**)0x1F8003FC += 0x10;
}

void func_actor_800300_80162EEC(GpActorWork* arg0)
{
    if (arg0->actor->field_95E == 1) {
        Gp_ResetActorMove(arg0, 0);
    }
}

void func_actor_800300_80162F24(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    switch (actor->field_95E) {
        case 0:
            flag               = 1;
            actor->field_95E   = flag;
            coord->coord.t[1] += 0xC0;
        case 1:
            Gp_AnimTickChildSlots(arg0);
            Gp_TurnPlayer(arg0);
            Gp_StepPlayerMove(arg0);
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_800300/actor_800300", D_actor_800300_80161E64);

void func_actor_800300_80162F98(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable7 sp;

    sp    = D_actor_800300_80161E64;
    actor = arg0->actor;
    sp.funcs[(u16)actor->field_956](arg0);
    Gp_TurnPlayer(arg0);
    if (D_80072830 <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80163048);

INCLUDE_ASM("actors/nonmatchings/actor_800300/actor_800300_2", func_actor_800300_80163074);
