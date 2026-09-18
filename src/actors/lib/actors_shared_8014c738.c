#include "common.h"
#include "actors/actor_201200.h"
#include "gameplay/3A34.h"
#include "main/mem.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

void ActorsShared8014c4c0(Actor201200* arg0, s16 arg1, u32 arg2);

static __inline__ s16 Actor201200_WrapAngle(s16 angle)
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

/// Hit check: finds the first type-2 record among the five at `rec250`, and
/// on a hit applies its damage, turns the model toward it and, once the hit
/// points run out, moves to substate 6.
///
/// Shared: the three actor slots (`actor_101200` / `201200` / `301200`) carry
/// the same body, so one object serves every overlay that lists this unit in
/// `configs/USA/overlays.toml`.
void ActorsShared8014c738(Actor201200Ctx* arg0, Actor201200* arg1)
{
    Actor201200HitScratch* sc;
    Actor201200Work*       work;
    GpRec18*               recs;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s16                    angle;
    s16                    i;

    work = arg1->field_1C;
    sc   = (Actor201200HitScratch*)(SCRATCH_SP -= sizeof(Actor201200HitScratch));
    pos  = &sc->pos;
    recs = &work->rec250;
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
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        sc->dmg                     = Gp_ComputeDamage(sc->id, 0, 0, 0x1000);
        arg1->field_2C->coords->flg = 0;
        Gp_UpdateCoord(arg1->field_2C->coords);
        sc->d.vx = arg1->field_2C->coords->workm.t[0];
        sc->d.vy = arg1->field_2C->coords->workm.t[1];
        sc->d.vz = arg1->field_2C->coords->workm.t[2];
        sc->d.vx = sc->pos.vx - arg1->field_2C->coords->workm.t[0];
        sc->d.vy = sc->pos.vy - arg1->field_2C->coords->workm.t[1];
        sc->d.vz = sc->pos.vz - arg1->field_2C->coords->workm.t[2];
        angle    = ratan2(sc->d.vx, sc->d.vz) -
                ratan2(-arg1->field_2C->coords->workm.m[2][0], arg1->field_2C->coords->workm.m[2][2]);
        sc->angle = angle;
        sc->angle = Actor201200_WrapAngle(angle);
        ActorsShared8014c4c0(arg1, sc->angle, sc->id);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->field_40 -= sc->dmg;
        if (arg0->field_40 <= 0) {
            arg0->field_4B = 0;
            work->field_0  = 6;
        }
    }
    SCRATCH_SP += sizeof(Actor201200HitScratch);
}
