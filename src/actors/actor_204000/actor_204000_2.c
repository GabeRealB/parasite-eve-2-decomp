#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/sound.h"

extern MATRIX* D_80073B8C;
extern u32     Gp_LcgState;

void func_actor_204000_8014AC8C(Actor104000* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014AED8);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014B4AC);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014BC3C);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor204000_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor104000RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)0x1F8003FC;
    ((Actor104000RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor104000RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor104000RangeScratch*)(head - 0xC))->dx *= ((Actor104000RangeScratch*)(head - 0xC))->dx;
    *(Actor104000RangeScratch**)0x1F8003FC        = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)0x1F8003FC                             = head;
    ret                                           = ((Actor104000RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

/// Restarts the actor when `field_4` is set; otherwise steps it, occasionally
/// switches to state 3 on a random roll, and arms the player state when the
/// camera target comes within 2000 units.
void func_actor_204000_8014C51C(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    GsCOORDINATE2*    coord;
    SVECTOR           delta;
    SVECTOR*          d;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 5;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_36E |= 0x8000;
        work->field_3A6 &= 0x7FFF;
        work->field_3DE &= 0x7FFF;
        work->field_28E |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        return;
    }
    func_actor_204000_8014AC8C(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = arg1->field_2C->field_8;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor204000_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014C710);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014CD68);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014D5B8);

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Picks a random offset and coordinate index for an effect from the hit
/// angle `arg1` (front, back, right or left), copies it into `work->eff` and
/// spawns the effect for hit id `arg2`.
void func_actor_204000_8014DB50(Actor104000* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    Actor104000Work* work;
    s32              mag;
    GsCOORDINATE2*   coord;

    sc   = (SVECTOR*)(SCRATCH_SP -= sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    work->effOfs      = *sc;
    coord             = &arg0->field_2C->field_8[sc->pad];
    work->eff.field_4 = 0x100;
    work->eff.field_6 = 1;
    work->eff.field_0 = coord;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->field_8[sc->pad], &work->effOfs, &work->eff);
    SCRATCH_SP += sizeof(SVECTOR);
}

/// Wraps a 12-bit angle difference into [-0x800, 0x800].
static __inline__ s16 Actor204000_WrapAngle(s16 angle)
{
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    return angle;
}

/// Applies the first type-2 hit in `work->hits`: computes its damage, turns the
/// model toward the hit, plays the impact sound and subtracts the damage from
/// `arg0->field_40`, switching to state 6 once it runs out.
void func_actor_204000_8014DDD4(GpEnemy* arg0, Actor104000* arg1)
{
    Actor104000HitScratch* sc;
    Actor104000Work*       work;
    GpRec18*               recs;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s16                    angle;
    s32                    snd;
    s32                    pan;
    s16                    i;

    work = arg1->field_1C;
    sc   = (Actor104000HitScratch*)(SCRATCH_SP -= sizeof(Actor104000HitScratch));
    pos  = &sc->pos;
    recs = work->hits;
    i    = 0;
    mask = 0xFFFF0000;
    kind = 0x20000;
scan:
    if (recs[i].field_4 == 0) {
        goto missed;
    }
    if ((recs[i].field_4 & mask) == kind) {
        pos->vx = recs[i].field_8;
        pos->vy = recs[i].field_A;
        pos->vz = recs[i].field_C;
        id      = recs[i].field_4;
        goto found;
    }
    i++;
    if (i < 8) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        sc->dmg                      = Gp_ComputeDamage(sc->id, 0, 0, 0x1000);
        arg1->field_2C->field_8->flg = 0;
        Gp_UpdateCoord(arg1->field_2C->field_8);
        sc->d.vx = arg1->field_2C->field_8->workm.t[0];
        sc->d.vy = arg1->field_2C->field_8->workm.t[1];
        sc->d.vz = arg1->field_2C->field_8->workm.t[2];
        sc->d.vx = sc->pos.vx - arg1->field_2C->field_8->workm.t[0];
        sc->d.vy = sc->pos.vy - arg1->field_2C->field_8->workm.t[1];
        sc->d.vz = sc->pos.vz - arg1->field_2C->field_8->workm.t[2];
        angle    = ratan2(sc->d.vx, sc->d.vz) -
                ratan2(-arg1->field_2C->field_8->workm.m[2][0], arg1->field_2C->field_8->workm.m[2][2]);
        sc->angle = angle;
        sc->angle = Actor204000_WrapAngle(angle);
        func_actor_204000_8014DB50(arg1, sc->angle, sc->id);
        snd = ((arg0->field_8 >> 12) << 8) | 0x40280003;
        pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
        SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
        func_800E2C78((GpObj40*)arg0, sc->id, sc->dmg, 0);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->field_40 -= sc->dmg;
        if (arg0->field_40 <= 0) {
            work->field_0 = 6;
        }
        if (work->field_496 == 1) {
            if (((GpActorWork*)Game_GetPtrSlot(3))->actor->field_954 == 2) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    SCRATCH_SP += sizeof(Actor104000HitScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014E14C);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014E7E0);

/// Restarts the actor when `field_4` is set; otherwise waits 50 frames, then
/// drops the model with growing speed, unwinding its Z roll by at most 0x92 a
/// frame, and on landing plays the impact sound and switches to state 3.
void func_actor_204000_8014EDC4(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work* work;
    s32              id;
    s32              pan;
    s32              mag;
    s32              rot;
    s32              step;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        arg1->field_2C->field_C = 0;
        work->field_174         = 5;
        work->field_170         = 1;
        work->field_178         = 0;
        work->field_36E        |= 0x8000;
        work->field_3A6        &= 0x7FFF;
        work->field_3DE        &= 0x7FFF;
        work->field_28E        |= 0x4000;
        func_actor_204000_8014AC8C(arg1);
        ratan2(-arg1->field_2C->field_8->coord.m[2][0], arg1->field_2C->field_8->coord.m[2][2]);
        arg1->field_2C->field_8->flg = 0;
        work->field_19A              = 10;
        work->field_198              = 0;
        work->field_6                = 0;
        work->field_19C              = 0x800;
        work->field_479              = 1;
        return;
    }
    if ((s16)work->field_6 < 0x32) {
        work->field_6++;
        return;
    }
    work->field_19A                     += 4;
    work->field_198                     += work->field_19A;
    arg1->field_2C->field_8->coord.t[1] += (s16)work->field_198;
    if (arg1->field_2C->field_8->coord.t[1] >= -0x12B) {
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x03100000) {
            id  = ((arg0->field_8 >> 12) << 8) | 0x53100006;
            pan = (s8)Gp_GetObjPan((GpObj38*)arg1->field_2C->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)arg1->field_2C->field_8));
        }
        arg1->field_2C->field_8->coord.t[1] = 0;
        work->field_0                       = 3;
        work->field_479                     = 0;
        Gfx_RotMatrixZ(&arg1->field_2C->field_8->coord, -work->field_19C, 0);
    } else {
        rot = work->field_19C;
        if (rot != 0) {
            mag  = __builtin_abs(rot);
            step = rot;
            SOFT_TOUCH_REG(step);
            step = -step;
            if (mag >= 0x93) {
                step = -0x92;
                if (rot < 0) {
                    step = 0x92;
                }
            }
            Gfx_RotMatrixZ(&arg1->field_2C->field_8->coord, (s16)step, 0);
            work->field_19C += step;
        }
    }
    arg1->field_2C->field_8->flg = 0;
    func_actor_204000_8014AC8C(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014F04C);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014F3E8);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014F908);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014FD2C);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_801501A0);

INCLUDE_RODATA("actors/nonmatchings/actor_204000/actor_204000_2", ActorsShared80135df4Table);

INCLUDE_RODATA("actors/nonmatchings/actor_204000/actor_204000_2", func_actor_204000_8014A06C);
