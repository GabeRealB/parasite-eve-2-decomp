#include "common.h"

#include "actors/actor_300700.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/1BC.h"
#include "main/sound.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_300700_80161E24;
extern GpEnemyTaskFuncTable3 D_actor_300700_80161E30;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_300700_801637E4(Actor300700* arg0);
void func_actor_300700_80164794(Actor300700* arg0);
void func_actor_300700_80163D64(Actor300700* arg0);
void func_actor_300700_80164070(Actor300700* arg0);
void func_actor_300700_801643D0(Actor300700* arg0);
void func_actor_300700_801645F8(Actor300700* arg0);
void func_actor_300700_80164E38(Actor300700* arg0, Actor300700Obj2C* arg1, s32 arg2);
void func_actor_300700_80164F68(Actor300700* arg0);
void func_actor_300700_801650C0(Actor300700* arg0);
void func_actor_300700_801651A0(Actor300700* arg0);
void func_actor_300700_80165230(Actor300700* arg0);
void func_actor_300700_801652F4(Actor300700* arg0);
void func_actor_300700_8016534C(Actor300700* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_800B4114(Actor300700Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern u8  D_801153F4;
extern s32 Gp_LcgState;

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80161E80);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80162130);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801622B4);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_8016252C);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801626C0);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801628C8);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80162EFC);

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E24);

void func_actor_300700_8016335C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_300700_80161E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801633B8);

void func_actor_300700_80163410(Actor300700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor300700Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_2E2 >= 0x201) {
        work->field_2E2 = (u16)work->field_2E2 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2E2;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_22C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    *(u8**)0x1F8003FC += 0x30;
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80163510);

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E30);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801637E4);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80163D64);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164070);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801643D0);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801645F8);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164794);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801648E4);

void func_actor_300700_80164CE0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_300700_80161E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_300700_80164D3C(Actor300700Ctx* arg0, Actor300700* arg1)
{
    GsCOORDINATE2*    coord;
    Actor300700Obj2C* obj;
    Actor300700Work*  work;
    s32               state;
    s32               one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C   = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        func_actor_300700_80164E38(arg1, obj, one);
    }
    func_actor_300700_801637E4(arg1);
    func_actor_300700_80164F68(arg1);
    SOFT_USE_REG(work);
    if (work->field_386 != 0) {
        func_actor_300700_80164794(arg1);
    }
    func_actor_300700_801651A0(arg1);
    func_actor_300700_80165230(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    func_actor_300700_801652F4(arg1);
    func_actor_300700_8016534C(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164E38);

void func_actor_300700_80164F68(Actor300700* arg0)
{
    switch (arg0->field_1C->field_37A) {
        case 0:
            func_actor_300700_80163D64(arg0);
            break;
        case 1:
            func_actor_300700_80164070(arg0);
            break;
        case 2:
            func_actor_300700_801643D0(arg0);
            break;
        case 3:
            func_actor_300700_801645F8(arg0);
            break;
        case 4:
            func_actor_300700_801650C0(arg0);
            break;
        case 5:
            break;
    }
}

/// Randomised footstep timer. Each tick decrements the counter and, when it
/// runs out, reseeds it from the shared LCG and plays the step sound at the
/// object's pan and depth.
void func_actor_300700_80165000(Actor300700* arg0)
{
    Actor300700Work* work;
    GsCOORDINATE2*   coord;
    s32              snd;
    s32              pan;
    u16              timer;
    u32              random;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    timer           = work->field_392 - 1;
    work->field_392 = timer;
    if ((s16)timer <= 0) {
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_392 = (u16)(((random >> 0x10) & 0x7F) + 0x96);
        Gp_LcgState     = (s32)random;
        snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40070001;
        pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(snd, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    }
}

/// State machine for the actor's contact sound: state 0 arms the timer and
/// plays the hit sound once, state 1 clears the state pair once the
/// countdown reaches 0x18.
void func_actor_300700_801650C0(Actor300700* arg0)
{
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               snd;
    s32               pan;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    return;
case0:
    work->field_37E = 5;
    work->field_380 = 1;
    work->field_384 = 0;
    work->field_386 = 0;
    work->field_37C = 1;
    snd             = (((u16)arg0->field_20->field_8 >> 0xC) << 8) | 0x40070002;
    pan             = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    return;
case1:
    if ((s16)work->field_382 < 0x18) {
        return;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = state;
    work->field_38C = 0;
    work->field_394 = state;
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801651A0);

void func_actor_300700_80165230(Actor300700* arg0)
{
    Actor300700Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_37E != work->field_380) {
        work->field_380 = work->field_37E;
        work->field_382 = 0;
        value           = D_actor_300700_801693E4[(s16)work->field_37E];
        for (; i < 7; i++) {
            func_800B4114(work, i, (s16)work->field_37E, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 7);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801652F4);

void func_actor_300700_8016534C(Actor300700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

void func_actor_300700_8016539C(Actor300700* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor300700Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = arg0->field_2C->field_8;
    if (work->field_390 >= 0x201) {
        work->field_390 = (u16)work->field_390 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_390;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_340;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}
