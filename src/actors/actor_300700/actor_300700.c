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
/// Per-variant base speed, indexed by `GpAreaPlace::rowIndex`.
extern u16 D_actor_300700_80165B78[];

/// Spawn pair table packed by `Gp_PackPair`, and the enemy record whose
/// `pairTable` points at it and whose `hpMax` seeds the enemy's `field_40`.
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
    coord = obj->coords;
    work  = memCalloc(0x2F4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work  = (TaskIdMap*)work;
    obj->flags  = 0;
    coord->flg  = 0;
    obj->tpage += 1;
    obj->clut  += 1;
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    obj->lightMtx  = &work->field_114;
    obj->colorMtx  = &work->field_F4;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = coord;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_300700_80165B68;
    arg0->recs       = &work->rec154;
    arg0->hp         = (u16)D_actor_300700_80165B68.hpMax;
    work->field_228  = 0x100;
    work->field_22A  = 1;
    work->field_224  = coord;
    func_800B3F84((GpAnimCtx*)work, &D_actor_300700_80165B94, obj,
                  work->field_B4, (GpAnimSlot*)work->field_14);
    for (i = 1; i < 4; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2D6       = 1;
    work->field_2AC       = (s32)coord->coord.t[0];
    work->field_2B0       = (s32)coord->coord.t[1];
    work->field_2B4       = (s32)coord->coord.t[2];
    work->field_2DC       = (u16)((Actor300700Ctx*)arg1->spawnArg2)->field_3C->yaw;
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154;
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = 0;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x30008;
    work->obj134.radius   = 0xFA;
    work->obj134.flags    = 1;
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154, 1, 0);
    work->obj16C.coord    = coord;
    work->obj16C.ctx.recs = &work->rec18C[0];
    work->obj16C.pos.vx   = 0;
    work->obj16C.pos.vy   = 0;
    work->obj16C.pos.vz   = 0;
    work->obj16C.key      = 0x30008;
    work->obj16C.radius   = 0xFA;
    work->obj16C.flags    = 1;
    work->obj134.flags    = (u16)(work->obj134.flags | 0x8000);
    Gp_LinkObj(2, &work->obj16C);
    Gp_InitRec18Table(&work->rec18C[0], 4, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj16C.flags    = (u16)(work->obj16C.flags | 0x4000);
    work->obj1EC.key      = Gp_PackPair(&D_actor_300700_80165B64, 0);
    work->obj1EC.radius   = 0x190;
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
        SndEvt_EnqueueType6(id, temp, (s8)gpGetObjDepth(arg1->field_2C->field_8));
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
            target         = Gp_ActorSlots[(u8)work->field_154.hit.id.h.lo >> 7]->extra->coords;
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
            sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->vec.vy = 0;
            sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
            speed = D_actor_300700_80165B78[arg0->field_20->field_3C->rowIndex] +
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
                    arg0->recs                                  = 0;
                    ((Actor300700SpawnWork*)work)->obj134.flags = ((Actor300700SpawnWork*)work)->obj134.flags & 0x7FFF;
                    ((Actor300700SpawnWork*)work)->obj16C.flags = ((Actor300700SpawnWork*)work)->obj16C.flags & 0xBFFF;
                    ((Actor300700SpawnWork*)work)->obj1EC.flags = ((Actor300700SpawnWork*)work)->obj1EC.flags | 0x8000;
                    id                                          = ((arg0->placeKey >> 12) << 8) | 0x40070006;
                    pan                                         = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(coord));
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
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
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
    addPrim((u_long*)(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), prim);
    SCRATCH_SP += 0x28;
}
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
    coord = obj->coords;
    work  = memCalloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_1BC;
    obj->colorMtx  = &work->field_19C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = &((TmdObject*)arg1->extra)->coords[4];
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &D_actor_300700_8016932C;
    arg0->recs       = work->rec2;
    arg0->hp         = (u16)D_actor_300700_8016932C.hpMax;
    work->field_338  = 0x100;
    work->field_33A  = 1;
    work->field_334  = coord;
    func_800B3F84((GpAnimCtx*)work, &D_actor_300700_801693B8, obj,
                  work->field_12C, (GpAnimSlot*)work->field_14);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);

    work->field_37E     = 1;
    work->field_380     = 1;
    work->obj1.coord    = coord;
    work->obj1.ctx.recs = work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0x2EE;
    work->obj1.key      = 0;
    work->obj1.radius   = 0x12C;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 1, 0);
    work->obj1.flags |= 0x8000;

    work->obj2.coord    = &((TmdObject*)arg1->extra)->coords[4];
    work->obj2.ctx.recs = work->rec2;
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = 0;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x30007;
    work->obj2.radius   = 0x96;
    work->obj2.flags    = 1;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, 3, 0);

    work->obj3.coord    = coord;
    work->obj3.ctx.recs = work->rec3;
    work->obj3.pos.vx   = 0;
    work->obj3.pos.vy   = -0xFA;
    work->obj3.pos.vz   = 0;
    work->obj3.key      = 0x30007;
    work->obj3.radius   = 0xFA;
    work->obj3.flags    = 1;
    work->obj2.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, 4, 0);

    work->obj4.coord    = coord;
    work->obj4.ctx.recs = work->rec4;
    work->obj4.pos.vx   = 0;
    work->obj4.pos.vy   = 0;
    work->obj4.pos.vz   = 0x1F4;
    work->obj3.flags   |= 0x4200;
    work->obj4.key      = Gp_PackPair(&D_actor_300700_80169328, 0);
    work->obj4.radius   = 0xC8;
    work->obj4.flags    = 1;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(work->rec4, 1, 0);
    work->obj4.flags &= 0x7FFF;
    arg1->state       = 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E30);
