#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

extern void* D_80067704[1];
extern u8    Actor05500_D05F18[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void Actor05500_Fn0006C(Actor105500* arg0);
void Actor05500_Fn00754(Actor105500* arg0);
void Actor05500_Fn00914(Actor105500* arg0);
void Actor05500_Fn00A94(Actor105500* arg0);
void Actor05500_Fn00FA0(Actor105500* arg0);
void Actor05500_Fn012E8(Actor105500* arg0);
void Actor05500_Fn0143C(Actor105500* arg0);
void Actor05500_Fn01A0C(Actor105500* arg0);
void Actor05500_Fn01B30(Actor105500* arg0);
void Actor05500_Fn020D4(Actor105500* arg0);
void Actor05500_Fn02214(Actor105500* arg0);
void Actor05500_Fn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2);
void Actor05500_Fn0378C(Actor105500* arg0);
void Actor05500_Fn03864(Actor105500* arg0);
void Actor05500_Fn03918(Actor105500* arg0);
void Actor05500_Fn039AC(Actor105500* arg0);
void Actor05500_Fn03A70(Actor105500* arg0);
void Actor05500_Fn03AC8(Actor105500* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0006C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00754);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00914);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00A94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00FA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn012E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0143C);

#include "actors_shared_fn01a0c.c"

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn01B30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn020D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02214);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02364);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02780);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02954);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02C94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02FFC);

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105500Obj2C* obj;
    Actor105500Work*  work;
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
        Actor05500_Fn03674(arg1, obj, one);
    }
    Actor05500_Fn0006C(arg1);
    Actor05500_Fn0378C(arg1);
    if (work->field_3B0 != 0) {
        Actor05500_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor05500_Fn02214(arg1);
    }
    Actor05500_Fn03918(arg1);
    Actor05500_Fn039AC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor05500_Fn03A70(arg1);
    Actor05500_Fn03AC8(arg1);
}

#define ActorsSharedFn03674 Actor05500_Fn03674
#include "actors_shared_fn03674.c"
#undef ActorsSharedFn03674

void Actor05500_Fn0378C(Actor105500* arg0)
{
    s16 state;

    state = arg0->field_1C->field_39A;
    switch (state) {
        case 0:
            Actor05500_Fn00754(arg0);
            break;
        case 1:
            Actor05500_Fn00914(arg0);
            break;
        case 2:
            Actor05500_Fn00A94(arg0);
            break;
        case 3:
            Actor05500_Fn00FA0(arg0);
            break;
        case 4:
            Actor05500_Fn012E8(arg0);
            break;
        case 5:
            Actor05500_Fn0143C(arg0);
            break;
        case 6:
            Actor05500_Fn01A0C(arg0);
            break;
        case 7:
            Actor05500_Fn03864(arg0);
            break;
        case 8:
            Actor05500_Fn01B30(arg0);
            break;
        case 9:
            break;
    }
}

#define ActorsShared801355a4_Fn3567C Actor05500_Fn03864
#include "actors_shared_fn03864.c"
#undef ActorsShared801355a4_Fn3567C

void Actor05500_Fn03918(Actor105500* arg0)
{
    GsCOORDINATE2*   coord;
    Actor105500Work* work;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_35C.vx = coord->coord.t[0];
    work->field_35C.vy = coord->coord.t[1];
    work->field_35C.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_398) >> 12;
    coord->coord.t[1] += work->field_3A8;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_398) >> 12;
}

void Actor05500_Fn039AC(Actor105500* arg0)
{
    Actor105500Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_392 != work->field_394) {
        work->field_394 = work->field_392;
        work->field_396 = 0;
        value           = Actor05500_D08A18[work->field_392];
        for (; i < 8; i++) {
            func_800B4114(work, i, work->field_392, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_396 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 8);
    }
}

void Actor05500_Fn03A70(Actor105500* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03AC8);
