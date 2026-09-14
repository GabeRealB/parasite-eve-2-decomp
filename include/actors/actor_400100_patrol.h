#ifndef ACTOR_400100_PATROL_H
#define ACTOR_400100_PATROL_H

#include "actors/actor_400100_motion.h"

typedef struct Actor00100PatrolScratch {
    SVECTOR vec;
    s16     yaw;
    s16     pad;
} Actor00100PatrolScratch;

static __inline__ s32 Actor00100_PatrolOutsideRadius(SVECTOR* pos, s32 radius)
{
    Actor00100RadiusScratch* head;
    Actor00100RadiusScratch* scratch;
    s32                      x;
    head    = *(Actor00100RadiusScratch**)G_SCRATCH_HEAD;
    scratch = (*(Actor00100RadiusScratch**)G_SCRATCH_HEAD = head - 1);
    x       = pos->vx;
    SOFT_TOUCH_REG_USE(x, scratch);
    scratch->x                                  = x;
    scratch->z                                  = pos->vz;
    scratch->radius                             = radius;
    scratch->x                                 *= scratch->x;
    scratch->z                                 *= scratch->z;
    scratch->radius                            *= scratch->radius;
    *(Actor00100RadiusScratch**)G_SCRATCH_HEAD += 1;
    return scratch->x + scratch->z >= scratch->radius;
}

STATIC_ASSERT_SIZEOF(Actor00100PatrolScratch, 0xC);

#endif
