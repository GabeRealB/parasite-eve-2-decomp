#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/mem.h"
#include "main/session.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"

#include "actors/actor_403900.h"
#include "actors/actors_shared_80137ca4.h"

void func_actor_403900_80131F54(Actor403900* arg0, Actor403900Obj2C* arg1, s32 arg2);
void func_actor_403900_80132688(Actor403900* arg0);
void func_actor_403900_801329A4(Actor403900* arg0);
void func_actor_403900_8013314C(Actor403900* arg0);
void func_actor_403900_80133AEC(Actor403900* arg0);
void func_actor_403900_80134194(Actor403900* arg0);
void func_actor_403900_801347F4(Actor403900* arg0);
void func_actor_403900_80134968(Actor403900* arg0);
void func_actor_403900_8013539C(Actor403900* arg0);
void func_actor_403900_801354B0(Actor403900* arg0);
void func_actor_403900_80135630(Actor403900* arg0);
void func_actor_403900_8013592C(Actor403900* arg0);
void func_actor_403900_80135A24(Actor403900* arg0);
void func_actor_403900_80135BE0(Actor403900* arg0);
void func_actor_403900_80137B78(Actor403900* arg0);
void func_actor_403900_80137D7C(Actor403900* arg0);
void func_actor_403900_80137E4C(Actor403900* arg0);
void func_actor_403900_80137EF0(Actor403900* arg0);
void func_actor_403900_80137FB4(Actor403900* arg0);
void func_actor_403900_80138070(Actor403900* arg0);

extern u8 D_801153F4;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Frame handler for the scene's `D_801153F4` mode. Mode 1 only refreshes the
/// tint and the ground shadow, and mode 2 hides the model; both return at once.
/// Mode 0 shows the model again while the `field_6DA` timer runs and makes the
/// enemy lockable only while a hit is pending (bit 0x8000 of `field_49A`).
/// Then, once the box table is set, the frame runs: the hit handler, the
/// sequence dispatch, the step forward, the animation reseed, the vocal cue,
/// the coordinate refresh, the tint and shadow, the projection at depth +0xC,
/// the fade and `func_8009EA50`.
void func_actor_403900_80137A20(GpEnemy* arg0, Actor403900* arg1)
{
    Actor403900Work*  temp_s1;
    Actor403900Obj2C* temp_a1;
    Actor403900Coord* temp_s2;
    s32               state;
    s32               one;

    temp_s1 = arg1->field_1C;
    temp_a1 = arg1->field_2C;
    temp_s2 = temp_a1->field_8;
    state   = D_801153F4;
    one     = 1;
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
    if (temp_s1->field_6DA != 0) {
        temp_a1->field_C = 0;
    }
    arg0->node.flags = (temp_s1->field_49A >> 0xF) ^ 1;
    goto default_body;
case1:
    func_actor_403900_80137FB4(arg1);
    func_actor_403900_80138070(arg1);
    return;
case2:
    temp_a1->field_C = 0x80;
    arg0->node.flags = one;
    return;
default_body:
    if (temp_s1->field_6B4 != 0) {
        func_actor_403900_80131F54(arg1, temp_a1, one);
        func_actor_403900_80137B78(arg1);
        func_actor_403900_80137E4C(arg1);
        func_actor_403900_80137EF0(arg1);
        func_actor_403900_80135BE0(arg1);
        temp_s2->field_0.flg                  = 0;
        arg1->field_2C->field_8->field_F0.flg = 0;
        Gp_UpdateCoord(&temp_s2->field_0);
        func_actor_403900_80137FB4(arg1);
        func_actor_403900_80138070(arg1);
        func_actor_403900_8013820C(&arg1->field_2C->field_8->field_F0, 0xC);
        func_actor_403900_80134968(arg1);
        func_8009EA50(temp_s1->field_6D8);
    }
}

/// Runs the sequence `field_6CC` names (0 to 0xB), then the vocal cue unless
/// the sequence is 1.
void func_actor_403900_80137B78(Actor403900* arg0)
{
    s16              temp_v1;
    Actor403900Work* temp_s1;

    temp_s1 = arg0->field_1C;
    temp_v1 = temp_s1->field_6CC;
    switch (temp_v1) {
        case 0:
            func_actor_403900_801329A4(arg0);
            break;
        case 1:
            func_actor_403900_8013314C(arg0);
            break;
        case 2:
            func_actor_403900_80133AEC(arg0);
            break;
        case 3:
            func_actor_403900_80134194(arg0);
            break;
        case 4:
            func_actor_403900_801347F4(arg0);
            break;
        case 5:
            func_actor_403900_8013539C(arg0);
            break;
        case 6:
            func_actor_403900_801354B0(arg0);
            break;
        case 7:
            func_actor_403900_80135630(arg0);
            break;
        case 8:
            func_actor_403900_8013592C(arg0);
            break;
        case 9:
            func_actor_403900_80135A24(arg0);
            break;
        case 10:
            func_actor_403900_80137D7C(arg0);
            break;
        case 11:
            func_actor_403900_80132688(arg0);
            break;
    }
    if (temp_s1->field_6CC != 1) {
        func_actor_403900_801380DC(arg0);
    }
}

/// Rebuilds the root part's rotation from the saved attach matrix
/// `field_674`, scaled per axis by `field_694`..`field_69C`: the saved matrix is
/// copied into the root coordinate, and an identity scaled in a scratchpad
/// matrix is multiplied into it.
void func_actor_403900_80137CA8(Actor403900* arg0)
{
    void**                  scratch;
    void*                   head;
    ActorShared80137ca4Mat* m;
    GsCOORDINATE2*          coord;
    Actor403900Work*        work;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    m        = (ActorShared80137ca4Mat*)((u8*)head - 0x20);
    *scratch = m;
    coord    = &arg0->field_2C->field_8->field_0;
    work     = arg0->field_1C;

    coord->coord     = work->field_674;
    m->ident.m00_m01 = 0x1000;
    m->ident.m02_m10 = 0;
    m->ident.m11_m12 = 0x1000;
    m->ident.m20_m21 = 0;
    m->ident.m22     = 0x1000;
    ScaleMatrix(&m->mat, &work->field_694);
    MulMatrix(&coord->coord, &m->mat);
    *scratch = (u8*)*scratch + 0x20;
}

/// Sequence 0xA, the entrance: state 0 picks animation 0xE (and state 1) when
/// `field_6F0` is 1, otherwise 0x12 and state 2, and arms the timers
/// `field_6DA`..`field_6DE`; states 1 and 2 wait for the frame counter to reach
/// 0x10 or 0x16, then park 2 in the context's `field_30` and drop back to 0.
void func_actor_403900_80137D7C(Actor403900* arg0)
{
    Actor403900Work* work;
    s16              state;
    s32              next;

    work  = arg0->field_1C;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0xA;
            work->field_6DE = 5;
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                arg0->field_30  = 2;
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                arg0->field_30  = state;
                work->field_6CE = 0;
            }
            break;
    }
}

/// Saves the root's translation in `field_664`..`field_66C` and steps it
/// `field_6C8` along the root's facing (its matrix's third column), adding
/// 0x80 to its y while `field_714` is below 2.
void func_actor_403900_80137E4C(Actor403900* arg0)
{
    Actor403900Work* work;
    GsCOORDINATE2*   coord;

    coord              = &arg0->field_2C->field_8->field_0;
    work               = arg0->field_1C;
    work->field_664    = coord->coord.t[0];
    work->field_668    = coord->coord.t[1];
    work->field_66C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_6C8) >> 12;
    if (work->field_714 < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_6C8) >> 12;
}
