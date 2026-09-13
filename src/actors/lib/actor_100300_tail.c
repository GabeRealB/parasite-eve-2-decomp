#include "common.h"

#include "actors/actor_100300.h"
#include "actors/actors_shared_80132074.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/wipsys.h"

s32 SndEvt_EnqueueType6(s32 sound, s32 pan, s32 depth);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8  D_801153F4;
extern s32 D_80115728;
extern s32 D_8011573C;
extern s32 Gp_LcgState;

void Actor00300_Fn00078(GsCOORDINATE2* arg0, s32 arg1);
void Actor00300_Fn04528(Actor100300* arg0);
void Actor00300_Fn00E54(Actor100300* arg0);
void Actor00300_Fn01678(Actor100300* arg0);
void Actor00300_Fn019C0(Actor100300* arg0);
void Actor00300_Fn01F9C(Actor100300* arg0);
void Actor00300_Fn02620(Actor100300* arg0);
void Actor00300_Fn028D0(Actor100300* arg0);
void Actor00300_Fn02CE8(Actor100300* arg0);
void Actor00300_Fn030B8(Actor100300* arg0);
void Actor00300_Fn032BC(Actor100300* arg0);
void Actor00300_Fn0340C(Actor100300* arg0);
void Actor00300_Fn03A1C(Actor100300* arg0);
void Actor00300_Fn04A2C(Actor100300* arg0);
void Actor00300_Fn04C20(Actor100300* arg0);
void Actor00300_Fn04D28(Actor100300* arg0);
void Actor00300_Fn04E30(Actor100300* arg0);
void Actor00300_Fn04ED4(Actor100300* arg0);
void Actor00300_Fn04FB0(Actor100300* arg0);
void Actor00300_Fn05008(Actor100300* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void Actor00300_Fn04C20(Actor100300* arg0)
{
    Actor100300Work* work;

    work = arg0->field_1C;
    switch (work->field_684) {
        case 0:
            Actor00300_Fn01678(arg0);
            break;
        case 1:
            Actor00300_Fn019C0(arg0);
            break;
        case 2:
            Actor00300_Fn01F9C(arg0);
            break;
        case 3:
            Actor00300_Fn02620(arg0);
            break;
        case 4:
            Actor00300_Fn028D0(arg0);
            break;
        case 5:
            Actor00300_Fn02CE8(arg0);
            break;
        case 6:
            Actor00300_Fn04D28(arg0);
            break;
        case 7:
            Actor00300_Fn030B8(arg0);
            break;
        case 8:
            break;
    }

    if (work->field_684 != 3) {
        if (work->field_676 != 0) {
            if (work->field_676 > 0) {
                work->field_676 -= 0x100;
            } else if (work->field_676 < 0) {
                work->field_676 = 0;
            }
        }
    }
}

void Actor00300_Fn04D28(Actor100300* arg0)
{
    Actor100300Work* work;
    GpEnemy*         enemy;
    s32              state;
    s32              value;
    GpEffWork*       effect;

    work  = arg0->field_1C;
    state = work->field_686;
    switch (state) {
        case 0:
            effect          = work->field_654;
            work->field_67A = 0;
            work->field_66E = 0xF;
            if (effect != NULL) {
                effect->field_0->state = 3;
                work->field_654        = NULL;
                work->field_69C        = 0;
                SndEvt_EnqueueType7(work->field_658, 1);
            }
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
                enemy            = arg0->field_20;
                enemy->field_4C &= 0xFD;
                work->field_66E  = 0x12;
                work->field_686  = 1;
            }
            break;
        case 1:
            if ((s16)work->field_672 >= 0xB) {
                work->field_684 = state;
                work->field_686 = state;
                value           = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = value;
                work->field_688 = ((u32)value >> 16) & 0x1F;
            }
            break;
    }
}

void Actor00300_Fn04E30(Actor100300* arg0)
{
    Actor100300Work* work;
    GsCOORDINATE2*   coord;

    coord              = arg0->field_2C->field_8;
    work               = arg0->field_1C;
    work->field_5F8    = coord->coord.t[0];
    work->field_5FC    = coord->coord.t[1];
    work->field_600    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_67A) >> 12;
    if (work->field_69A < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_67A) >> 12;
}

void Actor00300_Fn04ED4(Actor100300* arg0)
{
    Actor100300Work* work;
    s32              i;
    s32              val;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_66E != work->field_670) {
        TOUCH_REG(i);
        work->field_670 = work->field_66E;
        work->field_672 = 0;
        if (work->field_69E == 0) {
            val = Actor00300_D16394[work->field_66E];
        } else {
            val = 8;
            i   = 1;
        }
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_66E, 0, val);
            i++;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        work->field_672 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}

void Actor00300_Fn04FB0(Actor100300* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

void Actor00300_Fn05008(Actor100300* arg0)
{
    VECTOR3        vec;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* part;

    coord  = arg0->field_2C->field_8;
    part   = coord + 3;
    vec.vx = part->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, 0x80);
}

void Actor00300_Fn0505C(Actor100300* arg0, MATRIX* arg1, s16 arg2)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;

    head                       = *(MATRIX**)0x1F8003FC;
    scratch                    = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC        = scratch;
    coord                      = arg0->field_2C->field_8;
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = arg2;
    scratch->scale.vz          = 0x1000;
    coord->coord               = *arg1;
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

void Actor00300_Fn05138(Actor100300* arg0)
{
    Actor100300StateFuncTable3 sp;

    sp = Actor00300_D0003C;
    sp.funcs[arg0->field_30]((Actor100300Ctx*)arg0->field_20, arg0);
}

void Actor00300_Fn05194(Actor100300Ctx* arg0, Actor100300* arg1)
{
    Actor100300*      parent;
    Actor100300Obj2C* obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    parentCoord;
    Actor100300Work*  work;

    parent          = arg1->field_8;
    obj             = arg1->field_2C;
    parentCoord     = parent->field_2C->field_8;
    coord           = obj->field_8;
    work            = parent->field_1C;
    obj->field_C    = 0;
    coord->flg      = 0;
    coord->sub      = parentCoord + 7;
    obj->field_1C   = work->field_460;
    obj->field_20   = work->field_440;
    work->field_628 = coord->coord;
    work->field_676 = 0;
    arg1->field_30  = 1;
}

void Actor00300_Fn0521C(Actor100300* arg0)
{
    Actor100300StateFuncTable3 sp;

    sp = Actor00300_D00048;
    sp.funcs[arg0->field_30]((Actor100300Ctx*)arg0->field_20, arg0);
}

void Actor00300_Fn05278(GpEnemy* arg0, Task* arg1)
{
    Actor100300Work* work;
    u16              timer;

    work = (Actor100300Work*)arg1->idMap;
    switch (work->field_8A) {
        case 0:
            Gp_UnlinkObj(&work->obj0);
            Gp_UnlinkObj(&work->obj38);
            work->field_88 = 0x3C;
            work->field_8A = 1;
            return;
        case 1:
            timer          = work->field_88 - 1;
            work->field_88 = timer;
            if ((s16)timer <= 0) {
                Gp_DestroyEnemy(arg0, arg1);
            }
            return;
    }
}

s32 Actor00300_Fn05304(Actor100300* arg0, s32 arg1, Actor100300AnimArgs* args)
{
    Actor100300Work* work;
    s32              i;
    s32              frames;
    s16              anim;

    work            = arg0->field_1C;
    anim            = args->field_4 + 0x13;
    work->field_66E = anim;
    work->field_670 = anim;
    frames          = 0;
    if (args->field_8 != 0) {
        frames = args->field_C;
    }
    for (i = 1; i < 0x13; i++) {
        func_800B4114((GpAnimCtx*)work, i, work->field_66E, 0, frames);
    }
    return 0;
}

s32 Actor00300_Fn05388(Actor100300* arg0, s32 arg1, ActorsShared80132074Args* args)
{
    GsCOORDINATE2* coord = arg0->field_2C->field_8;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 Actor00300_Fn053EC(Actor100300* arg0, s32 arg1, s32 arg2)
{
    Actor100300Obj2C* obj;
    Actor100300Work*  work;

    obj  = arg0->field_2C;
    work = arg0->field_1C;
    if (!(arg2 & 1)) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    if (arg2 & 2) {
        obj->field_C |= 4;
    }
    work->field_678 = arg2;
    return 0;
}

s32 Actor00300_Fn05434(Actor100300* arg0, s32 arg1, Actor100300DestroyArgs* args)
{
    Actor100300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (args->field_2 != 0) {
        enemy->field_54 = 0;
        Gp_UnlinkNode(&enemy->node);
        Gp_UnlinkObj(&work->obj480);
        Gp_UnlinkObj(&work->obj538);
        Gp_UnlinkObj(&work->obj4D0);
        Gp_UnlinkObj(&work->obj5B8);
        Gp_DestroyEnemy(enemy, (Task*)arg0);
    }
    return 0;
}
