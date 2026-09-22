#include "common.h"

#include "actors/actor_105500.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_80135c4c.h"

#include "main/mem.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "psyq/inline_c.h"

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
void ActorsSharedFn020d4(Actor105500* arg0);
void ActorsSharedFn02214(Actor105500* arg0);
void Actor05500_Fn02954(Actor105500* arg0, s32 arg1);
void ActorsSharedFn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2);
void Actor05500_Fn0378C(Actor105500* arg0);
void ActorsShared801355a4_Fn3567C(Actor105500* arg0);
void Actor05500_Fn03918(Actor105500* arg0);
void Actor05500_Fn039AC(Actor105500* arg0);
void Actor05500_Fn03A70(Actor105500* arg0);
void Actor05500_Fn03AC8(Actor105500* arg0);
void Actor05500_Fn03B60(Actor105500* arg0);
void Actor05500_Fn03C54(Actor105500* arg0);
void Actor05500_Fn03D40(Actor105500* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;
void      Actor05500_Fn03918(Actor105500* arg0)
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

INCLUDE_ASM("actors/nonmatchings/actor_05500/actor_105500_text_6", Actor05500_Fn03AC8);
