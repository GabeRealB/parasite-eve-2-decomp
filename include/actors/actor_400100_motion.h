#ifndef ACTOR_400100_MOTION_H
#define ACTOR_400100_MOTION_H

#include "actors/actor.h"
#include "actors/actor_400100.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/wipsys.h"
#include <psyq/inline_c.h>
#include "gte.h"

extern s8 D_80114C12;
extern u8 D_80071075;

void func_801811C4(s32 amount);

static __inline__ void Actor00100_ScaleTransform(MATRIX* matrix, s16 amount)
{
    ActorScaleMatrixScratch* head;
    ActorScaleMatrixScratch* scratch;
    SVECTOR*                 vec;

    head                                  = SCRATCH_HEAD(ActorScaleMatrixScratch);
    scratch                               = head - 1;
    SCRATCH_HEAD(ActorScaleMatrixScratch) = scratch;
    scratch->scale.vz                     = amount;
    scratch->scale.vy                     = amount;
    head[-1].scale.vx                     = amount;
    ScaleMatrix(matrix, &scratch->scale);
    scratch->trans.vx = matrix->t[0];
    scratch->trans.vy = matrix->t[1];
    scratch->trans.vz = matrix->t[2];
    gte_lddp(amount);
    vec = &(head - 1)->trans;
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    matrix->t[0] = scratch->trans.vx;
    matrix->t[1] = scratch->trans.vy;
    matrix->t[2] = scratch->trans.vz;
    SCRATCH_POP(ActorScaleMatrixScratch);
}

static __inline__ s16 Actor00100_HasRecord10(Task* actor)
{
    Actor00100RecordWork* work  = (Actor00100RecordWork*)((Actor00100Work*)actor->work);
    s16                   found = 0;
    s16                   i;
    for (i = 0; i < 5; i++) {
        if (!work->records[i].field_0) {
            break;
        }
        if ((work->records[i].field_0 & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

static __inline__ void Actor00100_PositionDelta(GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    pos->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    pos->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
}

static __inline__ s32 Actor00100_OutsideRadius(SVECTOR* pos, s32 radius)
{
    OverlayRangeScratch* head;
    OverlayRangeScratch* scratch;
    head                              = SCRATCH_HEAD(OverlayRangeScratch);
    scratch                           = head - 1;
    SCRATCH_HEAD(OverlayRangeScratch) = scratch;
    scratch->dx                       = pos->vx;
    scratch->dz                       = pos->vz;
    scratch->r                        = radius;
    scratch->dx                      *= scratch->dx;
    scratch->dz                      *= scratch->dz;
    scratch->r                       *= scratch->r;
    SCRATCH_POP(OverlayRangeScratch);
    return scratch->dx + scratch->dz >= scratch->r;
}

#endif
