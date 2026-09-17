#include "common.h"

#include "actors/actor_300700.h"
#include "actors/actor_300700_spawn.h"
#include "actors/actor_300700_spawn2.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "psyq/inline_c.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

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
void func_actor_300700_80165000(Actor300700* arg0);
void func_actor_300700_801650C0(Actor300700* arg0);
void func_actor_300700_801651A0(Actor300700* arg0);
void func_actor_300700_80165230(Actor300700* arg0);
void func_actor_300700_801652F4(Actor300700* arg0);
void func_actor_300700_8016534C(Actor300700* arg0);
void func_actor_300700_8016539C(Actor300700* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void func_800B4114(Actor300700Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void func_actor_300700_801622B4(Actor300700* arg0);
void func_actor_300700_8016252C(Actor300700* arg0);
void func_actor_300700_801626C0(Actor300700* arg0);
void func_actor_300700_801628C8(Actor300700* arg0);
void func_actor_300700_801633B8(Actor300700* arg0);
void func_actor_300700_80162EFC(Actor300700* arg0);

extern u8  D_801153F4;
extern s8  D_80115408;
extern s32 Gp_LcgState;
/// `D_80073B8C` is the camera-target matrix positions are measured from.
extern MATRIX* D_80073B8C;
/// Per-variant base speed, indexed by `Actor300700Params.field_F`.
extern u16 D_actor_300700_80165B78[];

/// Spawn pair record packed by `Gp_PackPair`, and the wider view of the same
/// object whose `field_4` seeds the enemy's `field_40`.
extern GpU16Pair  D_actor_300700_80165B64;
extern GpPairSrcE D_actor_300700_80165B68;
/// Pose source handed to `func_800B3F84` as its animation data record.
extern u32 D_actor_300700_80165B94;

/// The second variant's pair of the same kind, and its pose source.
extern GpU16Pair  D_actor_300700_80169328;
extern GpPairSrcE D_actor_300700_8016932C;
extern u32        D_actor_300700_801693B8;

/// Per-`field_F` roll thresholds and the timer tables picked by the second
/// roll, for the two idle transitions of `func_actor_300700_80163D64`.
extern s16 D_actor_300700_8016933C[];
extern u16 D_actor_300700_8016934C[];
extern s16 D_actor_300700_8016936C[];
extern u16 D_actor_300700_8016937C[];
extern s16 D_actor_300700_8016939C[];

void func_actor_300700_80161E80(GpEnemy* arg0, Task* arg1)
{
    Actor300700SpawnWork* work;
    GsCOORDINATE2*        coord;
    TmdObject*            obj;
    s32                   i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x2F4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap    = (TaskIdMap*)work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_24 += 1;
    obj->field_25 += 1;
    Tmd_ProcessStream(obj);
    Tmd_ProcessStream(obj);
    obj->field_1C  = &work->field_114;
    obj->field_20  = &work->field_F4;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = coord;
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_300700_80165B68;
    arg0->field_54     = (s32)&work->rec154;
    arg0->field_40     = (u16)D_actor_300700_80165B68.field_4;
    work->field_228    = 0x100;
    work->field_22A    = 1;
    work->field_224    = coord;
    func_800B3F84((GpAnimCtx*)work, &D_actor_300700_80165B94, (GpAnimObj*)obj,
                  work->field_B4, (GpAnimSlot*)work->field_14);
    for (i = 1; i < 4; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2D6       = 1;
    work->field_2AC       = (s32)coord->coord.t[0];
    work->field_2B0       = (s32)coord->coord.t[1];
    work->field_2B4       = (s32)coord->coord.t[2];
    work->field_2DC       = (u16)((Actor300700Ctx*)arg1->spawnArg2)->field_3C->field_A;
    work->obj134.field_8  = coord;
    work->obj134.field_C  = &work->rec154;
    work->obj134.field_10 = 0;
    work->obj134.field_12 = 0;
    work->obj134.field_14 = 0;
    work->obj134.field_18 = 0x30008;
    work->obj134.field_1C = 0xFA;
    work->obj134.flags    = 1;
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154, 1, 0);
    work->obj16C.field_8  = coord;
    work->obj16C.field_C  = &work->rec18C[0];
    work->obj16C.field_10 = 0;
    work->obj16C.field_12 = 0;
    work->obj16C.field_14 = 0;
    work->obj16C.field_18 = 0x30008;
    work->obj16C.field_1C = 0xFA;
    work->obj16C.flags    = 1;
    work->obj134.flags    = (u16)(work->obj134.flags | 0x8000);
    Gp_LinkObj(2, &work->obj16C);
    Gp_InitRec18Table(&work->rec18C[0], 4, 0);
    work->obj1EC.field_8  = coord;
    work->obj1EC.field_C  = &work->rec20C;
    work->obj1EC.field_10 = 0;
    work->obj1EC.field_12 = 0;
    work->obj1EC.field_14 = 0;
    work->obj16C.flags    = (u16)(work->obj16C.flags | 0x4000);
    work->obj1EC.field_18 = Gp_PackPair(&D_actor_300700_80165B64, 0);
    work->obj1EC.field_1C = 0x190;
    work->obj1EC.flags    = 1U;
    Gp_LinkObj(3, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    arg1->state        = 1;
}

void func_actor_300700_80162130(Actor300700Ctx* arg0, Actor300700* arg1)
{
    Actor300700Obj2C* obj;
    Actor300700Work*  work;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               one;

    work  = arg1->field_1C;
    obj   = arg1->field_2C;
    state = D_801153F4;
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
case1:
    func_actor_300700_801633B8(arg1);
    return;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
    return;
default_body:
    func_actor_300700_801622B4(arg1);
    func_actor_300700_8016252C(arg1);
    if (work->field_2E6 == 0 && D_80115408 != 0) {
        work->field_2E6 = 1;
        Gp_ArmStateF0(1);
    }
    func_actor_300700_801626C0(arg1);
    func_actor_300700_801628C8(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_300700_801633B8(arg1);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((u32)Gp_LcgState >> 16 & 0x7F) == 0) {
        s32 temp;
        s32 id;

        id   = ((arg0->field_8 >> 12) << 8) | 0x40070008;
        temp = (s8)Gp_GetObjPan(arg1->field_2C->field_8);
        SndEvt_EnqueueType6(id, temp, (s8)Gp_GetObjDepth(arg1->field_2C->field_8));
    }
}

/// Loads the frame's movement record off the scratchpad, folds it into the
/// render coordinate, then applies whatever the collision record still holds:
/// state 1 nudges the actor by the fractional delta, state 2 snaps it to the
/// recorded position. The second half turns the hit record's id halfword into
/// an arm/damage reaction - state 2 measures the distance to the recorded
/// opponent, rolls damage, and spawns the hit effect.
void func_actor_300700_801622B4(Actor300700* arg0)
{
    Actor300700Work* work;
    GsCOORDINATE2*   coord;
    s32              movement;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              amount;
    s32              damage;
    s32              z;
    u16              state;
    GsCOORDINATE2*   target;
    GpDeltaScratch*  head;
    GpDeltaScratch*  delta;

    work     = arg0->field_1C;
    head     = *(void**)0x1F8003FC;
    delta    = (*(void**)0x1F8003FC = head - 1);
    coord    = arg0->field_2C->field_8;
    movement = func_800E0C10(&work->field_18C, delta, 4, 0);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].vx.h.hi;
            coord->coord.t[1] += delta->vy.h.hi;
            z                  = coord->coord.t[2] + delta->vz.h.hi;
            coord->coord.t[2]  = z;
            break;
        case 2:
            coord->coord.t[0] = work->field_2BC;
            coord->coord.t[1] = work->field_2C0;
            coord->coord.t[2] = work->field_2C4;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_18C);
    state = (u16)work->field_154.hit.id.h.hi;
    switch ((u32)state) {
        case 0:
            break;
        case 1:
            arg0->field_30           = 2;
            arg0->field_20->field_40 = 0;
            Gp_ArmStateF0(1);
            break;
        case 2:
            arg0->field_30 = (s32)state;
            target         = Gp_ActorSlots[(u8)work->field_154.hit.id.h.lo >> 7]->extra->field_8;
            dx             = target->coord.t[0] - coord->coord.t[0];
            delta->vx.w    = dx;
            dy             = target->coord.t[1] - coord->coord.t[1];
            delta->vy.w    = dy;
            dz             = target->coord.t[2] - coord->coord.t[2];
            delta->vz.w    = dz;
            damage         = Gp_ComputeDamage((s32)work->field_154.hit.id.w,
                                              SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            amount         = damage;
            if (damage == 0) {
                damage = 1;
                amount = 1;
            }
            func_800DA6E8(&arg0->field_20->field_10, amount, 0);
            func_800E2C78((GpObj40*)arg0->field_20, (s32)work->field_154.hit.id.w, damage, 0);
            arg0->field_20->field_40 = 0;
            func_800FDB18(Gp_GetIdParam1((s32)work->field_154.hit.id.w) & 0xFFFF, arg0->field_2C->field_8, 0,
                          &work->field_224);
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154.rec);
    *(void**)0x1F8003FC += 0x10;
}

/// Sweeps the actor's spare rotation on the scratchpad: every 16th frame rolls
/// `Gp_LcgState` to pick a direction, then `field_2D8` ramps between `-0x100`
/// and `0x100` and flips the `field_2D6` sign each time it wraps. The ramped
/// value scaled by that sign is the pitch written into the scratch vector,
/// which is handed to `RotMatrix` twice - once against `coord[2]`, once with
/// the product negated against `coord[3]`.
void func_actor_300700_8016252C(Actor300700* arg0)
{
    Actor300700Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    SVECTOR*         sc;
    s32              direction;
    s32              direction2;
    s32              product;

    sc   = (SVECTOR*)(SCRATCH_SP -= 8);
    work = arg0->field_1C;
    if (++work->field_2E0 >= 16) {
        work->field_2E0 = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_2D4 = !(((u32)Gp_LcgState >> 16) & 1);
    }
    switch (work->field_2D4) {
        case 0:
            work->field_2D8 += 0x100;
            if (work->field_2D8 >= 0x200) {
                work->field_2D8 = -0x100;
                direction       = work->field_2D6;
                work->field_2D6 = -direction;
            }
            break;
        case 1:
            work->field_2D8 = 0x100;
            direction2      = work->field_2D6;
            work->field_2D6 = -direction2;
            break;
    }
    sc->vx = 0;
    sc->vy = 0;
    sc->vz = work->field_2D8 * work->field_2D6;
    coord  = arg0->field_2C->field_8;
    RotMatrix(sc, &coord[2].coord);
    coord[2].flg = 0;
    sc->vx       = 0;
    sc->vy       = 0;
    product      = work->field_2D8 * work->field_2D6;
    sc->vz       = -product;
    coord2       = arg0->field_2C->field_8;
    RotMatrix(sc, &coord2[3].coord);
    coord2[3].flg = 0;
    SCRATCH_SP   += 8;
}

void func_actor_300700_801626C0(Actor300700* arg0)
{
    Actor300700Work*       work;
    GsCOORDINATE2*         coord;
    Actor300700RotScratch* sc;
    s32                    random;
    s32                    amount;
    s32                    cur;
    s32                    cur2;
    s32                    cur3;
    s32                    random2;
    s32                    amount2;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;

    sc    = (Actor300700RotScratch*)(SCRATCH_SP -= 0x18);
    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    switch (work->field_2E6) {
        case 0:
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            random          = (u32)Gp_LcgState >> 16;
            amount          = random & 0x1F;
            cur             = work->field_2DC;
            work->field_2DC = !(random & 0x20) ? cur - amount : cur + amount;
            break;
        case 1:
            sc->vec.vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            sc->vec.vy = 0;
            sc->vec.vz = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
            diff       = want - (work->field_2DC & 0xFFF);
            adiff      = diff >= 0 ? diff : -diff;
            turn       = diff;
            if (adiff < 0x11) {
                work->field_2DC = want;
            } else {
                if (adiff >= 0x801) {
                    wrap = diff - 0x1000;
                    if (diff <= 0)
                        wrap = 0x1000 - diff;
                    turn = wrap;
                }
                cur2 = work->field_2DC;
                if (turn > 0) {
                    work->field_2DC = cur2 + 0x10;
                } else {
                    work->field_2DC = cur2 - 0x10;
                }
            }
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    random2         = (u32)Gp_LcgState >> 16;
    amount2         = random2 & 0x3F;
    cur3            = work->field_2DA;
    work->field_2DA = !(random2 & 0x40) ? cur3 - amount2 : cur3 + amount2;
    if (work->field_2DA > 0x100) {
        work->field_2DA = 0x100;
    } else if (work->field_2DA < -0x100) {
        work->field_2DA = -0x100;
    }
    sc->rot.vx = work->field_2DA;
    sc->rot.vy = work->field_2DC;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void func_actor_300700_801628C8(Actor300700* arg0)
{
    Actor300700Work* work;
    GsCOORDINATE2*   coord;
    u32              random;
    u32              random2;
    u32              random3;
    s32              amount;
    s32              amountB;
    s16              delta;
    s16              speed;
    s32              y;
    s32              newY;
    s16              base;

    work            = arg0->field_1C;
    coord           = arg0->field_2C->field_8;
    work->field_2BC = coord->coord.t[0];
    work->field_2C0 = coord->coord.t[1];
    work->field_2C4 = coord->coord.t[2];
    switch (work->field_2E6) {
        case 0:
            random = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount = random & 0x1F;
            if (!(random & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)(coord->coord.t[0] + (s16)delta) < work->field_2AC + 200 &&
                work->field_2AC - 200 < (s16)(coord->coord.t[0] + (s16)delta)) {
                coord->coord.t[0] += (s16)delta;
            } else {
                coord->coord.t[0] -= (s16)delta;
            }
            amountB = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F;
            if (work->field_2D4 != 0) {
                amountB = -amountB;
            }
            delta = amountB;
            if ((s16)(coord->coord.t[1] + (s16)delta) < work->field_2B0 + 500 &&
                work->field_2B0 - 500 < (s16)(coord->coord.t[1] + (s16)delta)) {
                coord->coord.t[1] += (s16)delta;
            } else {
                coord->coord.t[1] -= (s16)delta;
            }
            random3 = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            amount  = random3 & 0x1F;
            if (!(random3 & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)random3 < work->field_2B4 + 200 && work->field_2B4 - 200 < (s16)random3) {
                coord->coord.t[2] += (s16)delta;
            } else {
                coord->coord.t[2] -= (s16)delta;
            }
            break;
        case 1:
            speed = D_actor_300700_80165B78[arg0->field_20->field_3C->field_F] +
                    (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F);
            coord->coord.t[0] += (coord->coord.m[0][2] * speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * speed) >> 12;
            base               = D_80073B8C->t[1] - 0x4B0;
            random2            = (u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            y                  = coord->coord.t[1];
            if (y >= base + 400) {
                coord->coord.t[1] = y - (random2 & 0xF);
            } else {
                if (base - 400 >= y) {
                    newY = y + (random2 & 0xF);
                } else {
                    amountB = random2 & 0x1F;
                    if (work->field_2D4 != 0) {
                        newY = y - amountB;
                    } else {
                        newY = y + amountB;
                    }
                }
                coord->coord.t[1] = newY;
            }
            break;
    }
}

void func_actor_300700_80162BC8(GpEnemy* arg0, Actor300700* arg1)
{
    Actor300700Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         head;
    SVECTOR*         rot;
    s32              angle;
    u32              rnd;
    u32              seed;
    s32              id;
    s32              pan;

    coord = arg1->field_2C->field_8;
    work  = arg1->field_1C;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            arg1->field_2C->field_C = 0x80;
            break;
        case 0:
        default:
            head                   = *(SVECTOR**)0x1F8003FC;
            rot                    = head - 1;
            *(SVECTOR**)0x1F8003FC = rot;
            switch (work->field_2DE) {
                case 0:
                    D_80115408              = 1;
                    seed                    = Gp_LcgState * 5 + 0x71357911;
                    rnd                     = seed >> 16;
                    angle                   = rnd & 0xFF;
                    arg1->field_2C->field_C = 2;
                    Gp_LcgState             = seed;
                    work->field_2E2         = 0x1000;
                    work->field_22C.matrix  = coord->coord;
                    if (!(rnd & 0x100)) {
                        angle = -angle;
                    }
                    work->field_2E4                             = angle;
                    arg0->field_54                              = 0;
                    ((Actor300700SpawnWork*)work)->obj134.flags = ((Actor300700SpawnWork*)work)->obj134.flags & 0x7FFF;
                    ((Actor300700SpawnWork*)work)->obj16C.flags = ((Actor300700SpawnWork*)work)->obj16C.flags & 0xBFFF;
                    ((Actor300700SpawnWork*)work)->obj1EC.flags = ((Actor300700SpawnWork*)work)->obj1EC.flags | 0x8000;
                    id                                          = ((arg0->field_8 >> 12) << 8) | 0x40070006;
                    pan                                         = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                    Gp_UnlinkNode(&arg0->node);
                    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 8);
                    work->field_2E0 = 1;
                    work->field_2DE = 1;
                    break;
                case 1:
                    func_actor_300700_80163410(arg1);
                    work->field_2DA = (work->field_2DA + work->field_2E4) & 0xFFF;
                    work->field_2DC = (work->field_2DC + work->field_2E4) & 0xFFF;
                    rot->vx         = work->field_2DA;
                    rot->vy         = work->field_2DC;
                    rot->vz         = 0;
                    RotMatrix(rot, &coord->coord);
                    work->field_22C.matrix.t[1] += 0x18;
                    if ((s16)(work->field_2E0 / 3) < 8) {
                        func_actor_300700_80162EFC(arg1);
                    } else {
                        arg1->field_2C->field_C = 0x80;
                    }
                    work->field_2E0++;
                    if (work->field_2E0 >= 0x1E) {
                        Gp_UnlinkObj(&((Actor300700SpawnWork*)work)->obj134);
                        Gp_UnlinkObj(&((Actor300700SpawnWork*)work)->obj16C);
                        Gp_UnlinkObj(&((Actor300700SpawnWork*)work)->obj1EC);
                        work->field_2DE = 2;
                    }
                    break;
                case 2:
                    work->field_2E0--;
                    if (work->field_2E0 <= 0) {
                        Gp_DestroyEnemy(arg0, (Task*)arg1);
                    }
                    break;
            }
            *(SVECTOR**)0x1F8003FC += 1;
            break;
    }
}

void func_actor_300700_80162EFC(Actor300700* arg0)
{
    Actor300700QuadScratch* sc;
    Actor300700Work*        work;
    Actor300700Obj2C*       obj;
    GsCOORDINATE2*          coord;
    s32                     size, x, y;
    s16                     i;
    SVECTOR*                v;
    POLY_FT4*               prim;
    Actor300700TexEntry*    uv;
    obj         = arg0->field_2C;
    sc          = (Actor300700QuadScratch*)(SCRATCH_SP -= 0x28);
    coord       = obj->field_8;
    work        = arg0->field_1C;
    sc->v[0].vx = coord->workm.t[0];
    sc->v[0].vy = coord->workm.t[1];
    sc->v[0].vz = coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->v[0]);
    __asm__ volatile("nop; nop; .word 0x4A180001");
    gte_stsxy(&sc->sxy);
    gte_stszotz(&sc->otz);
    if (sc->otz < 20) {
        SCRATCH_SP += 0x28;
        return;
    }
    if (work->field_2E0 == 1) {
        sc->v[0].vx = 0;
        sc->v[0].vy = 0;
        sc->v[0].vz = ((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFFF;
        RotMatrix(&sc->v[0], &work->field_22C.quad.rotation);
    }
    size        = 0x7800 / sc->otz;
    x           = sc->sxy & 0xFFFF;
    y           = sc->sxy >> 16;
    sc->v[0].vx = -size;
    sc->v[0].vy = -size;
    sc->v[0].vz = 0;
    sc->v[1].vx = size;
    sc->v[1].vy = -size;
    sc->v[1].vz = 0;
    sc->v[2].vx = -size;
    sc->v[2].vy = size;
    sc->v[2].vz = 0;
    sc->v[3].vx = size;
    sc->v[3].vy = size;
    sc->v[3].vz = 0;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&work->field_22C.quad.rotation);
        v = &sc->v[i];
        gte_ldv0(v);
        __asm__ volatile("nop; nop; .word 0x4A486012");
        gte_stsv(v);
        v->vx += x;
        v->vy += y;
    }
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2E);
    setRGB0(prim, 0x80, 0x80, 0x80);
    setShadeTex(prim, 1);
    prim->tpage = (((obj->field_24 * 64 + 0x180) & 0x3FF) >> 6) | 0xD0;
    prim->clut  = (obj->field_25 << 6) + 0x3D40;
    uv          = &D_actor_300700_80165B9C[(s16)(work->field_2E0 / 3)];
    prim->u0    = uv->u;
    prim->v0    = uv->v;
    prim->u1    = uv->u + 31;
    prim->v1    = uv->v;
    prim->u2    = uv->u;
    prim->v2    = uv->v + 31;
    prim->u3    = uv->u + 31;
    prim->v3    = uv->v + 31;
    prim->x0    = sc->v[0].vx;
    prim->y0    = sc->v[0].vy;
    prim->x1    = sc->v[1].vx;
    prim->y1    = sc->v[1].vy;
    prim->x2    = sc->v[2].vx;
    prim->y2    = sc->v[2].vy;
    prim->x3    = sc->v[3].vx;
    prim->y3    = sc->v[3].vy;
    addPrim((u_long*)(((((u32)sc->otz << Display_State.field_128) >> 2) & 0xFFC) + (u32)Gpu_CurrentOt), prim);
    SCRATCH_SP += 0x28;
}

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
    coord->coord               = work->field_22C.matrix;
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

/// Second variant's spawn: allocates its 0x39C-byte work block, binds the two
/// pose matrices into the TMD object, then hangs the four render nodes on
/// their global lists with the record tables `Gp_InitRec18Table` zeroes.
void func_actor_300700_80163510(GpEnemy* arg0, Task* arg1)
{
    Actor300700Spawn2Work* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    s32                    i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap    = (TaskIdMap*)work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_1BC;
    obj->field_20  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = &((TmdObject*)arg1->extra)->field_8[4];
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &D_actor_300700_8016932C;
    arg0->field_54     = (s32)work->rec2;
    arg0->field_40     = (u16)D_actor_300700_8016932C.field_4;
    work->field_338    = 0x100;
    work->field_33A    = 1;
    work->field_334    = coord;
    func_800B3F84((GpAnimCtx*)work, &D_actor_300700_801693B8, (GpAnimObj*)obj,
                  work->field_12C, (GpAnimSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);

    work->field_37E     = 1;
    work->field_380     = 1;
    work->obj1.field_8  = coord;
    work->obj1.field_C  = work->rec1;
    work->obj1.field_10 = 0;
    work->obj1.field_12 = 0;
    work->obj1.field_14 = 0x2EE;
    work->obj1.field_18 = 0;
    work->obj1.field_1C = 0x12C;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj1.flags |= 0x8000;

    work->obj2.field_8  = &((TmdObject*)arg1->extra)->field_8[4];
    work->obj2.field_C  = work->rec2;
    work->obj2.field_10 = 0;
    work->obj2.field_12 = 0;
    work->obj2.field_14 = 0;
    work->obj2.field_18 = 0x30007;
    work->obj2.field_1C = 0x96;
    work->obj2.flags    = 1;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, 3, 0);

    work->obj3.field_8  = coord;
    work->obj3.field_C  = work->rec3;
    work->obj3.field_10 = 0;
    work->obj3.field_12 = -0xFA;
    work->obj3.field_14 = 0;
    work->obj3.field_18 = 0x30007;
    work->obj3.field_1C = 0xFA;
    work->obj3.flags    = 1;
    work->obj2.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, 4, 0);

    work->obj4.field_8  = coord;
    work->obj4.field_C  = work->rec4;
    work->obj4.field_10 = 0;
    work->obj4.field_12 = 0;
    work->obj4.field_14 = 0x1F4;
    work->obj3.flags   |= 0x4200;
    work->obj4.field_18 = Gp_PackPair(&D_actor_300700_80169328, 0);
    work->obj4.field_1C = 0xC8;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec4, 1, 0);
    work->obj4.flags &= 0x7FFF;
    arg1->state       = 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E30);

void func_actor_300700_801637E4(Actor300700* actor)
{
    Actor300700Ctx*  ctx;
    u32              lastId;
    Actor300700Work* work;
    Actor300700Work* contactWork;
    GpDeltaScratch*  allocated;
    GpDeltaScratch*  oldScratch;
    GpDeltaScratch*  scratch;
    GpDeltaScratch*  normal;
    GsCOORDINATE2*   coord;
    s16              cooldown;
    s16              health;
    s32              cooldownParam;
    s32              wallDx;
    s32              wallDy;
    s32              wallDz;
    s32              result;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              depth;
    s32              push;
    s32              z;
    s32              boundedDepth;
    s8*              effectRec;
    s8*              contactRec;
    u32              id;
    u32              kind;
    u32              hitId;
    u32              effect;
    u32              damage;
    GsCOORDINATE2*   sourceCoord;

    push       = 0;
    lastId     = 0;
    oldScratch = *(GpDeltaScratch**)0x1F8003FC;
    work       = actor->field_1C;
    allocated  = oldScratch - 3;
    SOFT_TOUCH_REG(allocated);
    scratch                       = allocated;
    *(GpDeltaScratch**)0x1F8003FC = scratch;
    coord                         = actor->field_2C->field_8;
    ctx                           = actor->field_20;
    result                        = func_800E0C10((GpRec18*)&work->field_27C[0x20], scratch, 4, NULL);
    USE_REG(oldScratch);
    if (result == 1)
        goto move_delta;
    if (result < 2)
        goto move_done;
    if (result == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += oldScratch[-3].vx.h.hi;
    coord->coord.t[1] += scratch->vy.h.hi;
    z                  = coord->coord.t[2] + scratch->vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_360;
    coord->coord.t[1] = work->field_364;
    z                 = work->field_368;
move_z:
    coord->coord.t[2] = z;
move_done:
    Gp_ClearRec18Occupied(&work->field_27C[0x20]);
    if (work->field_378 != 0) {
        cooldown        = (u16)work->field_378 - 1;
        work->field_378 = cooldown;
        normal          = scratch + 1;
        if ((cooldown << 0x10) <= 0) {
            work->field_378 = 0;
            goto cooldown_done;
        }
    } else {
    cooldown_done:
        normal = scratch + 1;
    }
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG4(scratch, scratch, scratch, scratch);
    USE_REG2(scratch, scratch);
    contactWork = work;
contact_loop: {
    USE_REG2(contactWork, contactWork);
    id   = contactWork->field_22C.contacts.recs[0].field_4;
    kind = id >> 0x10;
    if (kind == 1)
        goto physical_contact;
    if (kind == 0)
        goto next_contact;
    if (kind == 2)
        goto damage_contact;
    if (kind == 3)
        goto physical_contact;
    contactWork = (Actor300700Work*)((u8*)contactWork + 0x18);
    goto contact_test;
damage_contact:
    if (work->field_378 == 0) {
        sourceCoord   = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        dx            = sourceCoord->coord.t[0] - coord->coord.t[0];
        scratch->vx.w = dx;
        dy            = sourceCoord->coord.t[1] - coord->coord.t[1];
        scratch->vy.w = dy;
        dz            = sourceCoord->coord.t[2] - coord->coord.t[2];
        scratch->vz.w = dz;
        damage        = Gp_ComputeDamage(contactWork->field_22C.contacts.recs[0].field_4, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
        USE_REG(damage);
        if (Gp_RollEnemyChance((GpEnemy*)actor->field_20, contactWork->field_22C.contacts.recs[0].field_4, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(0x6009C, actor->field_2C->field_8, 0, NULL);
        }
        func_800DA6E8(&actor->field_20->field_10, (s32)damage, 0);
        func_800E2C78((GpObj40*)actor->field_20, (s32)contactWork->field_22C.contacts.recs[0].field_4, (s32)damage, 0);
        health        = (u16)ctx->field_40 - damage;
        ctx->field_40 = health;
        if ((health << 0x10) <= 0) {
            work->field_37A = 5;
            work->field_37C = 0;
            actor->field_30 = (s32)kind;
        } else if (work->field_398 == 0) {
            work->field_37A = 4;
            work->field_37C = 0;
        }
        work->field_31A &= 0x7FFF;
        effect           = Gp_GetIdParam0(contactWork->field_22C.contacts.recs[0].field_4) & 0xFFFF;
        switch (effect) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)actor->field_20, contactWork->field_22C.contacts.recs[0].field_4, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)actor->field_20, contactWork->field_22C.contacts.recs[0].field_4, 0);
                break;
            case 1:
            case 9:
                Gp_SetObjFlag1((GpObj4C*)actor->field_20);
                break;
        }
        hitId = contactWork->field_22C.contacts.recs[0].field_4;
        if (lastId != hitId) {
            lastId = hitId;
            func_800FDB18(Gp_GetIdParam1((s32)hitId) & 0xFFFF, coord, NULL, (GpEffArg*)&work->pad_31C[0x18]);
        }
        cooldownParam = Gp_GetIdParam2(contactWork->field_22C.contacts.recs[0].field_4);
        if (cooldownParam > 0) {
            work->field_378 = cooldownParam;
        }
    }
    goto next_contact;
physical_contact:
    wallDx        = coord->workm.t[0] - contactWork->field_22C.contacts.recs[0].field_8;
    scratch->vx.w = wallDx;
    wallDy        = coord->workm.t[1] - contactWork->field_22C.contacts.recs[0].field_A;
    scratch->vy.w = wallDy;
    wallDz        = coord->workm.t[2] - contactWork->field_22C.contacts.recs[0].field_C;
    scratch->vz.w = wallDz;
    depth         = contactWork->field_22C.contacts.recs[0].field_2 - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
    boundedDepth  = depth;
    if (depth <= 0) {
        boundedDepth = 0;
    }
    SOFT_TOUCH_REG_USE(boundedDepth, depth);
    depth = boundedDepth;
    if (push < depth) {
        push = depth;
        VectorNormal((VECTOR*)scratch, (VECTOR*)normal);
        ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, (VECTOR*)normal, (VECTOR*)(scratch + 2));
    }
next_contact:
    contactWork = (Actor300700Work*)((u8*)contactWork + 0x18);
}
contact_test:
    if ((s32)contactWork < (s32)&work->pad_0[0x48])
        goto contact_loop;
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * scratch[2].vx.w) >> 0xC;
        coord->coord.t[2] += (s32)(push * scratch[2].vz.w) >> 0xC;
    }
    Gp_ClearRec18Occupied((s8*)work->field_22C.contacts.recs);
    effectRec = work->pad_31C;
    if (Gp_FindRec18(effectRec, 0) != 0) {
        work->field_31A &= 0x7FFF;
        Gp_ClearRec18Occupied(effectRec);
    }
    contactRec = work->field_1FC;
    if (Gp_CountRec18Hi(contactRec, 0x10000) != 0) {
        sourceCoord      = Gp_ActorSlots[(u8)work->field_1FC[4] >> 7]->extra->field_8;
        work->field_394  = 1;
        work->field_1FA &= 0x7FFF;
        work->field_33C  = sourceCoord;
    }
    Gp_ClearRec18Occupied(contactRec);
    *(GpDeltaScratch**)0x1F8003FC += 3;
}

void func_actor_300700_80163D64(Actor300700* arg0)
{
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               one;
    s32               rng0;
    s32               rng1;
    s32               rng2;
    s32               rng3;
    s32               rng4;
    s32               rng5;
    s32               rng6;
    s32               timer;
    s32               next;
    s32               flags;
    s32               ang;
    s32               snd;
    s32               pan;

    one   = 1;
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto tail;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto tail;
case0:
    flags           = work->field_1FA;
    work->field_384 = 0;
    work->field_1FA = flags | 0x8000;
    timer           = work->field_38C + 1;
    work->field_38C = timer;
    if ((s16)timer < 0x1E) {
        goto tail;
    }
    rng0        = Gp_LcgState * 5 + 0x71357911;
    Gp_LcgState = rng0;
    if ((s32)(((u32)rng0 >> 16) & 0xF) <
        D_actor_300700_8016933C[arg0->field_20->field_3C->field_F]) {
        work->field_37E = 7;
        next            = D_actor_300700_8016934C[((u32)(rng1 = rng0 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng1;
        work->field_37C = one;
        work->field_38C = next;
        goto tail;
    }
    rng2        = rng0 * 5 + 0x71357911;
    Gp_LcgState = rng2;
    if ((s32)(((u32)rng2 >> 16) & 0xF) <
        D_actor_300700_8016936C[arg0->field_20->field_3C->field_F]) {
        work->field_37E = 2;
        next            = D_actor_300700_8016937C[((u32)(rng3 = rng2 * 5 + 0x71357911) >> 16) & 0xF];
        Gp_LcgState     = rng3;
        work->field_37C = 2;
        work->field_38C = next;
        goto tail;
    }
    work->field_38C = 0;
    goto tail;
case1:
    work->field_384 = 0x14;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
    goto tail;
case2:
    work->field_384 = 0x32;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto tail;
    }
    work->field_37E = one;
    work->field_38C = 0;
    work->field_37C = 0;
tail:
    work->field_38E = work->field_38E - 1;
    if ((s16)work->field_38E > 0) {
        goto post;
    }
    work->field_386 = 0x19;
    rng4            = Gp_LcgState * 5 + 0x71357911;
    rng5            = rng4 * 5 + 0x71357911;
    ang             = ((u32)rng5 >> 16) & 0x3FF;
    Gp_LcgState     = rng4;
    work->field_38E = ((u32)rng4 >> 16) & 0x1F;
    Gp_LcgState     = rng5;
    if ((((u32)rng5 >> 16) & 0x400) == 0) {
        ang = -ang;
    }
    work->field_38A = ((u16)work->field_388 + ang) & 0xFFF;
post:
    if (work->field_394 != 0) {
        work->field_37A = 1;
        work->field_394 = 0;
        work->field_37C = 0;
        work->field_37E = 2;
        rng6            = Gp_LcgState * 5 + 0x71357911;
        work->field_38C = (((u32)rng6 >> 16) & 0x1F) + 0x3C;
        snd             = ((arg0->field_20->field_8 >> 12) << 8) | 0x40070003;
        Gp_LcgState     = rng6;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    }
    func_actor_300700_80165000(arg0);
}

void func_actor_300700_80164070(Actor300700* arg0)
{
    VECTOR*           vec;
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    target;
    s32               state;
    s32               one;
    s32               dist;
    s32               raw;
    s16               diff;
    s32               adiff;
    s32               ang;
    s32               vel;
    s32               pan;
    s32               snd;

    one   = 1;
    vec   = (VECTOR*)(SCRATCH_SP -= 0x10);
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto pop;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto pop;
case0:
    Gp_ArmStateF0(1);
    if (work->field_33C == 0) {
        work->field_33C = ((Actor300700*)Game_GetPtrSlot(3))->field_2C->field_8;
    }
    target          = work->field_33C;
    vec->vx         = target->coord.t[0] - coord->coord.t[0];
    vec->vy         = 0;
    vec->vz         = target->coord.t[2] - coord->coord.t[2];
    work->field_38A = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
    work->field_386 = 0x19;
    work->field_38C = work->field_38C - 1;
    if ((s16)work->field_38C > 0) {
        goto dist;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
dist:
    dist = SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz);
    if (dist < 0x2BC) {
        raw   = work->field_38A - (u16)work->field_388;
        diff  = raw;
        adiff = diff >= 0 ? diff : -diff;
        if (adiff < 0x800) {
            ang = adiff;
            goto wrap_done;
        }
        if (diff > 0) {
            ang = 0x1000 - raw;
            goto wrap_done;
        }
        ang = raw + 0x1000;
    wrap_done:
        if ((s16)ang < 0x32) {
            work->field_37E = 4;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_37C = 1;
            goto pop;
        }
        work->field_384 = 0;
        goto pop;
    }
    work->field_384 = 0x32;
    goto pop;
case1:
    if ((s16)work->field_382 == 0x14) {
        work->field_31A |= 0x8000;
    }
    if ((s16)work->field_382 < 0x20) {
        goto pop;
    }
    work->field_37E  = 3;
    work->field_37C  = 2;
    work->field_31A &= 0x7FFF;
    goto pop;
case2:
    vel = 0;
    if ((s16)work->field_382 < 0xB) {
        vel = -0x78;
    }
    work->field_384 = vel;
    if ((s16)work->field_382 < 0x1F) {
        goto pop;
    }
    snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40070004;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((s32)(((u32)Gp_LcgState >> 16) & 0xF) < D_actor_300700_8016939C[arg0->field_20->field_3C->field_F]) {
        work->field_37C = 0;
        work->field_37E = state;
        goto pop;
    }
    work->field_37A = 0;
    work->field_37C = 0;
    work->field_37E = one;
    work->field_38C = 0;
    work->field_38E = 0;
    work->field_394 = 0;
pop:
    SCRATCH_SP += 0x10;
}

/// Three-state launcher. State 0 arms the timer from `Gp_LcgState` and stores
/// the direction from the camera target to `field_2C->field_8` into
/// `field_370` with `VectorNormalS`; state 1 pushes the coordinate along that
/// normal while `field_382` is below `0xF`, runs the `field_38C` countdown and
/// hands over to the teardown state 3 (or 2) when it expires; state 2 clears
/// the state machine once `field_382` reaches `0x20`.
void func_actor_300700_801643D0(Actor300700* arg0)
{
    VECTOR            vec;
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    s32               state;
    s32               one;
    s32               rng;
    s32               posX;

    one   = 1;
    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    state = work->field_37C;
    coord = obj->field_8;
    switch (state) {
        case 0:
            work->field_37E = 0xA;
            work->field_380 = one;
            work->field_384 = 0;
            work->field_386 = 0;
            work->field_396 = one;
            work->field_37C = one;
            rng             = Gp_LcgState * 5 + 0x71357911;
            work->field_38C = (((u32)rng >> 16) & 0x1F) + 0xF;
            Gp_LcgState     = rng;
            posX            = coord->coord.t[0];
            vec.vx          = D_80073B8C->t[0] - posX;
            vec.vy          = D_80073B8C->t[1] - coord->coord.t[1];
            vec.vz          = D_80073B8C->t[2] - coord->coord.t[2];
            VectorNormalS(&vec, &work->field_370);
            return;
        case 1:
            if ((s16)work->field_382 < 0xF) {
                coord->coord.t[0] += -(work->field_370.vx * 50) >> 12;
                coord->coord.t[2] += -(work->field_370.vz * 50) >> 12;
            }
            if ((u32)(work->field_382 - 6) < 9) {
                work->field_386 = 0x93;
                work->field_38A = (work->field_38A + 0x5C7) & 0xFFF;
            } else {
                work->field_386 = 0;
            }
            work->field_38C = work->field_38C - 1;
            if ((s16)work->field_38C <= 0) {
                if ((arg0->field_20->field_4C & 2) != 0) {
                    work->field_37E = 8;
                    work->field_37A = 3;
                    work->field_37C = 3;
                    return;
                }
                work->field_37E = 9;
                work->field_37C = 2;
            }
            return;
        case 2:
            if ((s16)work->field_382 >= 0x20) {
                work->field_37A = 0;
                work->field_37C = 0;
                work->field_37E = one;
                work->field_38C = 0;
                work->field_394 = one;
                work->field_396 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 arms the timer and latches `field_396`; state 1
/// waits it out; state 2 counts `field_38C` down and moves to the teardown
/// state 3, which waits for `Gp_TickObjFlag2` on the spawn block and then
/// clears the hit flag and resets the state machine.
void func_actor_300700_801645F8(Actor300700* arg0)
{
    Actor300700Ctx*  ctx;
    Actor300700Work* work;
    s16              state;
    s32              rng;
    s32              rng2;
    u16              timer;

    work  = arg0->field_1C;
    state = work->field_37C;
    switch (state) {
        case 0:
            work->field_384 = 0;
            work->field_386 = 0;
            if (work->field_396 == 0) {
                work->field_37C = 1;
                work->field_37E = 6;
            } else {
                work->field_37C = 2;
                rng             = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rng;
                work->field_38C = ((u32)rng >> 0x10) & 0xF;
            }
            work->field_396 = 1;
            work->field_380 = 1;
            return;
        case 1:
            if ((s16)work->field_382 >= 0x1D) {
                work->field_37C = 2;
                rng2            = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = rng2;
                work->field_38C = ((u32)rng2 >> 0x10) & 0xF;
                return;
            }
            return;
        case 2:
            timer           = work->field_38C - 1;
            work->field_38C = timer;
            if ((timer << 0x10) <= 0) {
                work->field_37E = 8;
                work->field_37C = 3;
                return;
            }
            break;
        case 3:
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
                ctx             = arg0->field_20;
                ctx->field_4C  &= 0xFD;
                work->field_37A = 0;
                work->field_37C = 0;
                work->field_37E = 1;
                work->field_38C = 0;
                work->field_394 = 1;
                work->field_396 = 0;
                work->field_398 = 0;
            }
            break;
    }
}

void func_actor_300700_80164794(Actor300700* arg0)
{
    Actor300700Work*       work;
    GsCOORDINATE2*         coord;
    Actor300700RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor300700RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_38A;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_388 = ang;
    if (adiff < 0x800) {
        step = work->field_386;
        if (step >= adiff) {
            work->field_388 = want;
        } else {
            next = work->field_388;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_388 = next;
        }
    } else {
        step = work->field_386;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_388 = work->field_38A;
        goto done;
    turn:
        wrapStep = work->field_386;
        cur      = work->field_388;
        if (diff > 0) {
            work->field_388 = cur - wrapStep;
        } else {
            work->field_388 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_388;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

void func_actor_300700_801648E4(GpEnemy* arg0, Actor300700* arg1)
{
    Actor300700Work*  work;
    Actor300700Obj2C* obj;
    GsCOORDINATE2*    coord;
    Actor300700Work*  work2;
    GsCOORDINATE2*    c;
    VECTOR            vec;
    s32               state;
    s32               i;
    s16               st;
    s16               phase;
    s16               val;
    s32               snd;
    s32               pan;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    state = D_801153F4;
    coord = obj->field_8;
    if (state == 1) {
        goto case1;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    return;
case2:
    obj->field_C = 0x80;
    return;
default_body:
    st = work->field_37C;
    if (st == 1) {
        goto dying;
    }
    if (st >= 2) {
        goto ge2;
    }
    if (st == 0) {
        goto death;
    }
    return;
ge2:
    if (st == 2) {
        goto destroy;
    }
    return;
death:
    work->field_37E = 6;
    work->field_38C = 0;
    work->field_390 = 0x1000;
    work->field_340 = coord->coord;
    arg0->field_54  = 0;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj1);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj2);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj3);
    Gp_UnlinkObj(&((Actor300700Spawn2Work*)work)->obj4);
    Gp_SetLightMode((GpObj4C*)arg0, 1);
    Gp_ReleaseStateF0Add((GpObj20E*)arg1, 7);
    work->field_37C = 1;
    work2           = arg1->field_1C;
    i               = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = D_actor_300700_801693E4[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 7);
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    snd = ((arg1->field_20->field_8 >> 12) << 8) | 0x40070005;
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(coord));
    return;
dying:
    func_actor_300700_8016539C(arg1);
    phase           = work->field_38C + 1;
    work->field_38C = phase;
    if (phase == 10) {
        obj->field_C = 2;
    }
    if ((s16)work->field_38C == 15) {
        Gp_SpawnEff(0x600A5, coord, 1, NULL);
    }
    if ((s16)work->field_38C >= 0x3C) {
        work->field_37C = 2;
    }
    work2 = arg1->field_1C;
    i     = 1;
    if ((s16)work2->field_37E != work2->field_380) {
        work2->field_380 = work2->field_37E;
        work2->field_382 = 0;
        val              = D_actor_300700_801693E4[(s16)work2->field_37E];
        do {
            func_800B4114(work2, i, (s16)work2->field_37E, 0, val);
            i++;
        } while (i < 7);
    } else {
        TOUCH_REG(i);
        work2->field_382 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work2, i);
            i++;
        } while (i < 7);
    }
    c      = arg1->field_2C->field_8;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
    return;
destroy:
    Gp_DestroyEnemy(arg0, (Task*)arg1);
    return;
}

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

/// Damage tick: folds the generic hit flags into the work state, applies the
/// pending hit and drops the actor to its death state once the hit points run
/// out.
void func_actor_300700_80164E38(Actor300700* arg0, Actor300700Obj2C* arg1, s32 arg2)
{
    Actor300700Ctx*  ctx;
    Actor300700Work* work;
    s32              tick;
    u16              health;
    u8               flags;

    ctx   = arg0->field_20;
    flags = ctx->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        ctx->field_4C   = flags & 0xFE;
        work->field_37A = 2;
        work->field_37C = 0;
    }
    if ((ctx->field_4C & 2) && ((u32)((u16)work->field_37A - 2) >= 2U)) {
        work->field_37A = 3;
        work->field_37C = 0;
        work->field_398 = 1;
    }
    if (ctx->field_4C & 0xC) {
        tick = Gp_TickObjFlag4((GpObj5C*)ctx);
        if (tick != 0) {
            func_800DA6E8(&ctx->field_10, tick, 0);
            health        = ctx->field_40 - tick;
            ctx->field_40 = health;
            if ((health << 0x10) <= 0) {
                work->field_37A = 5;
                work->field_37C = 0;
                arg0->field_30  = 2;
            } else {
                work->field_37A = 4;
                work->field_37C = 0;
            }
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}

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
