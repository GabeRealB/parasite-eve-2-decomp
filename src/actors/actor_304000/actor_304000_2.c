#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/sound.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Wraps a 12-bit angle difference into [-0x800, 0x800].
static __inline__ s16 Actor304000_WrapAngle(s16 angle)
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

void func_actor_304000_80165B50(Actor104000* arg0, s16 arg1, u32 arg2);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80162ED8);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_801634AC);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80163C3C);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_8016451C);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80164710);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80164D68);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_801655B8);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80165B50);

/// Applies the first type-2 hit in `work->hits`: computes its damage, turns the
/// model toward the hit, plays the impact sound and subtracts the damage from
/// `arg0->hp`, switching to state 6 once it runs out.
void func_actor_304000_80165DD4(GpEnemy* arg0, Actor104000* arg1)
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
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
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
        sc->angle = Actor304000_WrapAngle(angle);
        func_actor_304000_80165B50(arg1, sc->angle, sc->id);
        snd = ((arg0->placeKey >> 12) << 8) | 0x40280003;
        pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)arg1->field_2C->field_8);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)arg1->field_2C->field_8));
        func_800E2C78((GpObj40*)arg0, sc->id, sc->dmg, 0);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->hp -= sc->dmg;
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
        if (work->field_496 == 1) {
            if (((GpActorWork*)gameGetPtrSlot(3))->actor->field_954 == 2) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            }
            work->field_496 = 0;
        }
    }
    SCRATCH_SP += sizeof(Actor104000HitScratch);
}

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_8016614C);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_801667E0);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80166DC4);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_8016704C);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_801673E8);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80167908);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_80167D2C);

INCLUDE_ASM("actors/nonmatchings/actor_304000/actor_304000_2", func_actor_304000_801681A0);
