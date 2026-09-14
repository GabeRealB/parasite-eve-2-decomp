#ifndef ACTOR_400100_MOTION_H
#define ACTOR_400100_MOTION_H

#include "actors/actor_400100.h"
#include "main/gfx.h"
#include "main/mem.h"
#include <psyq/inline_c.h>

extern u8 D_80072729;

static __inline__ void Actor00100_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

static __inline__ s16 Actor00100_HasRecord10(Actor00100* actor)
{
    Actor00100RecordWork* work  = (Actor00100RecordWork*)actor->field_1C;
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

extern MATRIX* D_80073B8C;

typedef struct Actor00100RadiusScratch {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 z;
    /* 0x8 */ s32 radius;
} Actor00100RadiusScratch;
STATIC_ASSERT_SIZEOF(Actor00100RadiusScratch, 0xC);

static __inline__ void Actor00100_PositionDelta(GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = D_80073B8C->t[0] - coord->coord.t[0];
    pos->vy = D_80073B8C->t[1] - coord->coord.t[1];
    pos->vz = D_80073B8C->t[2] - coord->coord.t[2];
}

static __inline__ s32 Actor00100_OutsideRadius(SVECTOR* pos, s32 radius)
{
    Actor00100RadiusScratch* head;
    Actor00100RadiusScratch* scratch;
    head                                        = *(Actor00100RadiusScratch**)G_SCRATCH_HEAD;
    scratch                                     = head - 1;
    *(Actor00100RadiusScratch**)G_SCRATCH_HEAD  = scratch;
    scratch->x                                  = pos->vx;
    scratch->z                                  = pos->vz;
    scratch->radius                             = radius;
    scratch->x                                 *= scratch->x;
    scratch->z                                 *= scratch->z;
    scratch->radius                            *= scratch->radius;
    *(Actor00100RadiusScratch**)G_SCRATCH_HEAD += 1;
    return scratch->x + scratch->z >= scratch->radius;
}

#endif
