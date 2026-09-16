#include "common.h"

#include "actors/actor_100400.h"
#include "main/gameflag.h"
#include "main/task.h"

extern u32 Gp_LcgState;
extern u8  D_801153F2[2];

/* This overlay calls the gameplay helpers through its own (wider) prototypes:
   the extra trailing arguments are set up at every call site but ignored by
   the definitions in src/gameplay/3A34.c. */

void       Gp_SetObjFlag1(Actor100400Obj* arg0);
void       Gp_SetObjFlag2(Actor100400Obj* arg0, s32 arg1, s32 arg2);
void       Gp_SetObjFlag4(Actor100400Obj* arg0, s32 arg1, s32 arg2);
s32        Gp_TickObjFlag4(Actor100400Obj* arg0);
s32        Gp_ObjFlag4Expired(Actor100400Obj* arg0);
u32        Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3);
s32        Gp_RollEnemyChance(Actor100400Obj* arg0, u32 arg1, s32 arg2);
s32        Gp_GetIdParam0(s32 arg0);
s32        Gp_GetIdParam1(s32 arg0);
s32        Gp_GetIdParam2(s32 arg0);
GpEffWork* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);
void       func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);
void       func_800E2C78(Actor100400Obj* arg0, s32 arg1, s32 arg2, s32 arg3);
void       func_800DA6E8(void* arg0, s32 arg1, s32 arg2);
s32        func_800E0C10(GpRec18* arg0, GpDeltaScratch* arg1, s32 arg2, s32* arg3);
void       Gp_ClearRec18Occupied(GpRec18* arg0);
void       Gp_SetLightMode(Actor100400Obj* arg0, s32 arg1);
void       Gp_ArmStateF0(s32 active);
void       Actor00400_Fn0237C(Actor100400* arg0);
void       Actor00400_Fn02FF8(Actor100400* arg0);
void       Gp_IncStateF0Ref(s32 arg0);
s32        Gp_GetObjPan(GsCOORDINATE2* arg0);
s32        Gp_GetObjDepth(GsCOORDINATE2* arg0);
s32        SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void       Actor00400_Fn060CC(Actor100400* arg0);
void       Actor00400_Fn09714(Actor100400* arg0);
void       Actor00400_Fn06EA4(Actor100400* arg0);
void       Actor00400_Fn08ADC(Actor100400* arg0);
void       Actor00400_Fn08B94(Actor100400* arg0);
void       Gp_AnimResetSlot(Actor100400Work* anim, s32 slot, s32 active);
void       Gp_AnimTickIndex(Actor100400Work* anim, s32 slot);
void       Actor00400_Fn08624(Actor100400* arg0);
void       Actor00400_Fn06F64(Actor100400* arg0);
void       Actor00400_Fn0A880(Actor100400* arg0);
void       Actor00400_Fn04900(Actor100400* arg0);
void       Actor00400_Fn0A940(Actor100400* arg0);
void       Actor00400_Fn04A1C(Actor100400* arg0);
void       Actor00400_Fn0A9F4(Actor100400* arg0);
void       Actor00400_Fn0AA40(Actor100400* arg0);
void       Actor00400_Fn0A3D4(Actor100400* arg0);
void       Actor00400_Fn0A414(Actor100400* arg0);
void       Actor00400_Fn098A8(Actor100400* arg0);
void       Actor00400_Fn09924(Actor100400* arg0);
s16        Actor00400_Fn02154(Actor100400* arg0);
s32        Actor00400_Fn02208(Actor100400* arg0);
void       Actor00400_Fn0A5B8(Actor100400* arg0);
void       Actor00400_Fn0A680(Actor100400* arg0);
void       Actor00400_Fn0A6B0(Actor100400* arg0);
void       Actor00400_Fn0A704(Actor100400* arg0);
void       Actor00400_Fn0A760(Actor100400* arg0);
void       Actor00400_Fn0A7F0(Actor100400* arg0);
void       Actor00400_Fn0A82C(Actor100400* arg0);
void       Gp_UpdateCoord(GsCOORDINATE2* arg0);
void       Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);
void       Actor00400_Fn00E3C(Actor100400* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);

extern GsCOORDINATE2        Gfx_ViewCoord;
extern MATRIX               Gfx_ViewWorldMtx;
extern const TaskFuncTable3 Actor00400_D0002C;
extern TaskFuncTable3       Actor00400_D00144;
extern const TaskFuncTable8 Actor00400_D00038;
extern TaskFuncTable7       Actor00400_D00178;

void Actor00400_Fn0805C(Actor100400* arg0, s32 arg1, Actor100400Msg* arg2)
{
    Actor100400Work* work;
    Actor100400Obj*  obj;
    GsCOORDINATE2*   coord;
    Actor100400Work* state;

    work  = arg0->field_1C;
    obj   = arg0->field_20;
    coord = arg0->field_2C->field_8;
    switch (arg2->field_2) {
        case 1:
            work->field_65E = 1;
            break;
        case 2:
            work->field_65E = 2;
            break;
        case 3:
            work->field_65E = 3;
            break;
        case 4:
            work->field_65E = 4;
            break;
        case 5:
            work->field_65E = 5;
            break;
        case 6:
            obj->field_14 = 0;
            Gp_SetLightMode(arg0->field_20, 0);
            work->field_666   = 0;
            work->field_65E   = 6;
            coord->coord.t[1] = 0;
            arg0->field_30    = 1;
            state             = arg0->field_1C;
            state->field_638  = 0;
            state->field_63A  = 0;
            state             = arg0->field_1C;
            state->field_638  = 2;
            state->field_63A  = 0;
            break;
    }
}

void Actor00400_Fn0814C(Actor100400* arg0, s16 arg1, SVECTOR* arg2, s16 arg3)
{
    MATRIX           m;
    VECTOR           d;
    VECTOR           r;
    GsCOORDINATE2*   coords;
    Actor100400Work* work;

    coords = arg0->field_2C->field_8;
    work   = arg0->field_1C;
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords[arg1].workm, &m);
    d.vx = work->field_5E4.vx - m.t[0];
    d.vy = work->field_5E4.vy - arg3 - m.t[1];
    d.vz = work->field_5E4.vz - m.t[2];
    ApplyTransposeMatrixLV(&coords->coord, &d, &r);
    arg2->vx = ratan2(-r.vy, r.vz) << 20 >> 20;
    arg2->vy = ratan2(r.vx, r.vz) << 20 >> 20;
    arg2->vz = 0;
}

void Actor00400_Fn0824C(Actor100400* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX         a;
    MATRIX         b;
    GsCOORDINATE2* coordA;
    GsCOORDINATE2* coordB;
    GsCOORDINATE2* coords;

    coords            = arg0->field_2C->field_8;
    Gfx_ViewCoord.flg = 0;
    coordA            = &coords[arg1];
    coordB            = &coords[arg2];
    Gp_UpdateCoord(&Gfx_ViewCoord);
    coordA->flg = 0;
    coordB->flg = 0;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coordA->workm, &a);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coordB->workm, &b);
    arg3->vx    = (a.t[0] + b.t[0]) / 2;
    arg3->vz    = (a.t[2] + b.t[2]) / 2;
    coordA->flg = 0;
    coordB->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn08354);

void Actor00400_Fn08464(Actor100400* arg0, s16 arg1, s16 arg2, SVECTOR* arg3)
{
    MATRIX         root;
    MATRIX         a;
    MATRIX         b;
    GsCOORDINATE2* coordA;
    GsCOORDINATE2* coordB;
    GsCOORDINATE2* coords;

    coords            = arg0->field_2C->field_8;
    Gfx_ViewCoord.flg = 0;
    coordA            = &coords[arg1];
    coordB            = &coords[arg2];
    Gp_UpdateCoord(&Gfx_ViewCoord);
    coordA->flg = 0;
    coordB->flg = 0;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coordA->workm, &a);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coordB->workm, &b);
    coords[0].coord.t[0] = arg3->vx - ((a.t[0] + b.t[0]) / 2 - root.t[0]);
    coords[0].coord.t[2] = arg3->vz - ((a.t[2] + b.t[2]) / 2 - root.t[2]);
    coords[0].flg        = 0;
    coordA->flg          = 0;
    coordB->flg          = 0;
    Gp_UpdateCoord(coordA);
    Gp_UpdateCoord(coordB);
    Gp_UpdateCoord(coords);
}

void Actor00400_Fn085B8(Actor100400* arg0)
{
    Actor100400Work*       work;
    Actor100400AnimStride* stride;
    s32                    i;

    work   = arg0->field_1C;
    i      = 1;
    stride = (Actor100400AnimStride*)work + 1;
    do {
        Gp_AnimResetSlot(work, i, work->field_628);
        i++;
        stride->field_1D = (u8)work->field_632;
        stride++;
    } while (i < 0xF);
    work->field_626 = (u16)work->field_628;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn08624);

/// Scales `arg1` (a 12-bit angle) by the ratio `work->field_632`, returning 0
/// while that field is unset.
s16 Actor00400_Fn086FC(Actor100400* arg0, s16 arg1)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_632 == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_632 << 12) >> 16;
}

/// Turns `work->field_556` toward `arg1` by at most `arg2` per call, but only
/// once the shortest signed 12-bit angle difference leaves the deadband
/// `arg3 & 0x7FF`. The two conditions share one arm rather than nesting, which
/// collapses the arms into the entry block: `work` then lives over 32 insns,
/// exactly tying its allocator priority with `range`'s, and the tie falls
/// through to the declaration order - hence `range` is declared ahead of `work`.
void Actor00400_Fn0875C(Actor100400* arg0, Actor100400Entry8* arg1, s32 arg2, s32 arg3)
{
    s32              range;
    Actor100400Work* work;
    GsCOORDINATE2*   coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    work        = arg0->field_1C;
    coords      = arg0->field_2C->field_8;
    coords->flg = 0;
    range       = arg3 & 0x7FF;
    vec.vx      = arg1->field_0 - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = arg1->field_4 - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_556;
    diff  = ((angle - yaw) << 20) >> 20;
    if ((diff > range) || (diff < -range)) {
        work->field_556 = (diff > range) ? (angle - arg2) : (angle + arg2);
    }
}

/// One animation-step: state 1 starts the clip `field_628` (or advances the
/// current one through `Actor00400_Fn086FC` when it is already in place, and
/// resets `field_62A` when it is not), state 2 finishes the old clip and state
/// 3 counts `field_62A` up a frame at a time. All three land in state 3 and
/// then tick animation slots 1..14.
void Actor00400_Fn08814(Actor100400* arg0)
{
    Actor100400Work* work;
    s32              i;

    work = arg0->field_1C;
    if (work->field_624 == 1) {
        if (work->field_626 != work->field_628) {
            work->field_62A = 0;
        } else {
            work->field_62A = Actor00400_Fn086FC(arg0, work->field_62A);
        }
        Actor00400_Fn08624(arg0);
        work->field_624 = 3;
    } else if (work->field_624 == 2) {
        Actor00400_Fn085B8(arg0);
        work->field_624 = 3;
        work->field_62A = 0;
    } else if (work->field_624 == 3) {
        work->field_62A++;
    }
    i = 1;
    do {
        Gp_AnimTickIndex(work, i);
        i++;
    } while (i < 0xF);
}

void Actor00400_Fn088EC(Actor100400* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = arg3;
    work->field_632 = arg2;
    work->field_628 = arg1;
    work->field_624 = 1;
}

/// Same body as src/actors/lib/actors_shared_8016974c.c.
s16 Actor00400_Fn08908(Actor100400* arg0)
{
    Actor100400Work* work = arg0->field_1C;

    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        return 1;
    }
    return 0;
}

void Actor00400_Fn08948(Actor100400* arg0)
{
    TaskFuncTable8 fns;

    fns = Actor00400_D00038;
    fns.funcs[arg0->field_30]((Task*)arg0);
}

void Actor00400_Fn089C8(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A3D4,
        Actor00400_Fn0A414,
    };

    states[work->field_638](arg0);
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. Same body as src/actors/lib/actors_shared_80132c4c.c.
void Actor00400_Fn08A1C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

void Actor00400_Fn08A88(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn08ADC,
        Actor00400_Fn06EA4,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn08ADC(Actor100400* arg0)
{
    Actor100400Work* state;
    Actor100400Work* work;
    u32              random;

    work             = arg0->field_1C;
    random           = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState      = random;
    state            = arg0->field_1C;
    state->field_63C = 8;
    state->field_632 = ((random >> 16) & 3) + 3;
    state->field_628 = 0xF;
    state->field_624 = 1;
    work->field_636  = 0;
    work->field_63A++;
}

void Actor00400_Fn08B40(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn08B94,
        Actor00400_Fn06F64,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn08B94(Actor100400* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;
    Actor100400Work* state;

    work  = arg0->field_1C;
    sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
    pan   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    state            = arg0->field_1C;
    state->field_63C = 2;
    state->field_632 = 0x10;
    state->field_628 = 0x13;
    state->field_624 = 1;
    work->field_63A++;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn08C54);

void Actor00400_Fn08D70(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_61E = 0x1000;
    work->field_5BC = coord->coord;
    Gp_SetLightMode(arg0->field_20, 1);
    work->field_636 = 0;
    work->field_638 = work->field_638 + 1;
}

void Actor00400_Fn08DFC(Actor100400* arg0)
{
    Actor100400Ctx*  ctx;
    Actor100400Work* work;

    work = arg0->field_1C;
    ctx  = arg0->field_2C;
    if (++work->field_636 >= 0x18) {
        ctx->field_C   |= 2;
        work->field_636 = 0;
        work->field_638++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn08E50);

void Actor00400_Fn08FB0(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn08FC8(Actor100400* arg0)
{
    Actor100400Ctx*  ctx;
    Actor100400Work* work;

    ctx             = arg0->field_2C;
    work            = arg0->field_1C;
    ctx->field_C   |= 0x80;
    work->field_636 = 0;
    work->field_638++;
}

void Actor00400_Fn08FF4(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (++work->field_636 >= 2) {
        work->field_638++;
    }
}

void Actor00400_Fn09038(Actor100400* arg0)
{
    TmdObject*       model;
    Actor100400Work* work;

    model = (TmdObject*)arg0->field_2C;
    work  = arg0->field_1C;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    Actor00400_Fn0237C(arg0);
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn0909C(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    arg0->field_30  = 5;
    work->field_638 = 0;
    work->field_63A = 0;
}

void Actor00400_Fn090B4(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;

    work                     = arg0->field_1C;
    arg0->field_20->field_14 = 0;
    Gp_IncStateF0Ref(0);
    work->flags_62C.hi.field_62E = 0;
    work->field_630              = 0x174B;
    state                        = arg0->field_1C;
    state->field_632             = 0x10;
    state->field_628             = 2;
    state->field_624             = 2;
    state2                       = arg0->field_1C;
    state2->field_638            = 1;
    state2->field_63A            = 0;
}

/// Every path out of the range test funnels through `set`, where the arm flag
/// is copied for the test below: the in-range edge arrives with the same value
/// (0), so the copy is redundant there and cse drops it, which leaves `done`
/// defined only here - and reorg then fills the branch's delay slot with a copy
/// of that one move.
void Actor00400_Fn09124(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    Actor100400Work* state2;
    s32              active;
    s32              done;

    work   = arg0->field_1C;
    active = 0;
    if (work->field_640 >= 0xDAC) {
        goto set;
    }
    done = 0;
    if ((u32)(work->field_634 - 0x600) >= 0x400U) {
        D_801153F2[1] = 1;
        Gp_ArmStateF0(1);
        active           = 1;
        state            = arg0->field_1C;
        state->field_638 = 2;
        state->field_63A = 0;
    }
set:
    done = active;
    if (done == 0) {
        if ((Actor00400_Fn02154(arg0) << 0x10) != 0) {
            Gp_ArmStateF0(1);
            return;
        }
        if (work->field_642 != 0) {
            state2            = arg0->field_1C;
            state2->field_638 = 2;
            state2->field_63A = 0;
        }
    }
}

void Actor00400_Fn091F8(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[1])(Actor100400*) = {
        Actor00400_Fn0A5B8,
    };

    if (Actor00400_Fn02154(arg0) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn09260(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A680,
        Actor00400_Fn0A6B0,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn092D4(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A704,
        Actor00400_Fn0A760,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn09348(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A7F0,
        Actor00400_Fn0A82C,
    };

    if ((Actor00400_Fn02154(arg0) << 0x10) == 0) {
        states[(s16)work->field_63A](arg0);
    }
}

void Actor00400_Fn093BC(void)
{
}

void Actor00400_Fn093C4(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A880,
        Actor00400_Fn04900,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn09418(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A940,
        Actor00400_Fn04A1C,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn0946C(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A9F4,
        Actor00400_Fn0AA40,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn094C0(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_65B = 0;
    work->field_63A = work->field_63A + 1;
}

void Actor00400_Fn094DC(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* work2;
    s32              pan;
    s32              sound;

    work = arg0->field_1C;
    if ((Actor00400_Fn02208(arg0) << 0x10) != 0) {
        if (work->field_628 != 5) {
            work2            = arg0->field_1C;
            work2->field_63C = 0x10;
            work2->field_632 = 0x10;
            work2->field_628 = 5;
            work2->field_624 = 1;
        }
        work->field_63E = work->field_64E;
        Gp_SetLightMode(arg0->field_20, 0);
        work->field_636 = 0;
        work->field_63A = work->field_63A + 1;
        return;
    }
    work->field_660 = 1;
    if (!(work->field_630 & 0xF)) {
        sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040001;
        pan   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
}

void Actor00400_Fn095D8(Actor100400* arg0)
{
    s32              cond;
    Actor100400Work* work;
    Actor100400Work* work2;

    work            = arg0->field_1C;
    work->field_660 = 1;
    work2           = arg0->field_1C;
    if ((work2->flags_62C.half & 1) || (work2->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_63A = 1;
    }
}

void Actor00400_Fn0962C(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;
    GsCOORDINATE2*   coord;

    work               = arg0->field_1C;
    coord              = arg0->field_2C->field_8;
    work->field_574.vx = work->field_56C.vx;
    work->field_636    = 0;
    work->field_574.vy = work->field_56C.vy;
    work->field_574.vz = work->field_56C.vz;
    coord->coord.t[0] += ((s16)work->field_574.vx - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += ((s16)work->field_574.vz - coord->coord.t[2]) >> 2;
    state              = arg0->field_1C;
    state->field_63C   = 0xA;
    state->field_632   = 0x10;
    state->field_628   = 1;
    state->field_624   = 1;
    work->field_63A    = work->field_63A + 1;
}

void Actor00400_Fn096C0(Actor100400* arg0)
{
    s32              cond;
    Actor100400Work* work;

    work = arg0->field_1C;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09714(Actor100400* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = 6;
    work->field_632 = 0x10;
    work->field_628 = 0xA;
    work->field_624 = 1;
    sound           = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    work->field_63A++;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn097C8);

void Actor00400_Fn098A8(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = 8;
    work->field_632 = 0x10;
    work->field_628 = 0xE;
    work->field_624 = 1;
    work->field_63E = (u16)work->field_64E + 0x64;
    Gp_SetLightMode(arg0->field_20, 0);
    work->field_610 = 0x64;
    work->field_636 = 0;
    work->field_664 = 1;
    work->field_63A = work->field_63A + 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn09924);

void Actor00400_Fn09A1C(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_660 = 0;
    work->field_636 = 0;
    work->field_63E = (u16)work->field_64E + 0x64;
    work->field_63A = work->field_63A + 1;
}

void Actor00400_Fn09A48(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work             = arg0->field_1C;
    work->field_65B  = 0;
    state            = arg0->field_1C;
    state->field_63C = 0xA;
    state->field_632 = 0x10;
    state->field_628 = 3;
    state->field_624 = 1;
    work->field_63A  = work->field_63A + 1;
}

void Actor00400_Fn09A8C(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_65E == 2 || GameFlag_GetNibble(0xBC) != 0) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09AE0(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    Actor100400Work* state;

    work              = arg0->field_1C;
    coord             = arg0->field_2C->field_8;
    coord->coord.t[0] = 0x10E0;
    coord->coord.t[1] = 0x178;
    work->field_63E   = 0x178;
    coord->coord.t[2] = -0xDAC;
    work->field_554   = 0;
    work->field_556   = 0;
    work->field_558   = 0;
    state             = arg0->field_1C;
    state->field_632  = 0x10;
    state->field_628  = 3;
    state->field_624  = 2;
    work->field_636   = 0;
    work->field_63A   = work->field_63A + 1;
}

void Actor00400_Fn09B44(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_65E == 1) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09B74(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (++work->field_636 < 0x30) {
        Actor00400_Fn0762C(arg0, 0xA0, work->field_556);
        return;
    }
    work->field_63A++;
}

void Actor00400_Fn09BDC(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_65E == 2) {
        work->field_638 = 0xB;
        work->field_63A = 0;
    }
}

void Actor00400_Fn09C04(Actor100400* arg0)
{
    Actor100400Work* work;
    TaskFuncTable7   fns;

    work = arg0->field_1C;
    fns  = Actor00400_D00178;
    fns.funcs[(s16)work->field_63A]((Task*)arg0);
}

void Actor00400_Fn09C84(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[1])(Actor100400*) = {
        Actor00400_Fn0A034,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn09CCC(Actor100400* arg0)
{
    Actor100400Work* work;
    GsCOORDINATE2*   coord;
    Actor100400Work* state;

    work              = arg0->field_1C;
    coord             = arg0->field_2C->field_8;
    work->field_660   = 1;
    coord->coord.t[0] = -0x6C0;
    coord->coord.t[1] = 0x3E8;
    work->field_63E   = 0x3E8;
    coord->coord.t[2] = -0xBB8;
    work->field_554   = 0;
    work->field_556   = 0x800;
    work->field_558   = 0;
    state             = arg0->field_1C;
    state->field_632  = 0x10;
    state->field_628  = 3;
    state->field_624  = 2;
    work->field_636   = 0;
    work->field_63A   = work->field_63A + 1;
}

void Actor00400_Fn09D3C(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (GameFlag_GetNibble(0xEB) == 1) {
        work->field_63A = 3;
    } else if (work->field_65E == 3) {
        work->field_63A = work->field_63A + 1;
    }
}

void Actor00400_Fn09D98(Actor100400* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->field_1C;
    count           = work->field_636 + 1;
    work->field_636 = count;
    if ((s16)count < 0x30) {
        Actor00400_Fn0762C(arg0, 0x60, work->field_556);
        if (!(work->field_630 & 0xF)) {
            sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040001;
            pan   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
        }
    } else {
        work->field_63A += 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_fn0805c", Actor00400_Fn09E70);

void Actor00400_Fn09F18(Actor100400* arg0)
{
    u16              count;
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->field_1C;
    count           = work->field_636 + 1;
    work->field_636 = count;
    if ((s16)count == 0x26) {
        sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x54220006;
        pan   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    }
    if ((s16)work->field_636 == 0x30) {
        work->field_63A += 1;
    }
}

void Actor00400_Fn09FDC(Actor100400* arg0)
{
    Actor100400Obj*  obj;
    Actor100400Work* work;

    obj = arg0->field_20;
    if (arg0->field_1C->field_65E == 5) {
        obj->field_14 = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work            = arg0->field_1C;
        work->field_638 = 4;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A034(Actor100400* arg0)
{
    Actor100400Obj*  obj;
    Actor100400Work* work;

    obj = arg0->field_20;
    if (arg0->field_1C->field_65E == 5) {
        obj->field_14 = 0;
        Actor00400_Fn02FF8(arg0);
        Gp_IncStateF0Ref(0);
        work            = arg0->field_1C;
        work->field_638 = 4;
        work->field_63A = 0;
    }
}
