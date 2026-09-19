#ifndef ACTOR_403100_REGIONS_H
#define ACTOR_403100_REGIONS_H

#include "common.h"
#include "actors/actor_403100.h"

typedef struct {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s32 id;
} Actor403100Region;
extern Actor403100Region D_actor_403100_80155698[];

static __inline__ s32 Actor403100_FindRegion(s16 x, s16 z)
{
    Actor403100Region* region;
    for (region = D_actor_403100_80155698; region->id != -1; region++) {
        if (x >= region->x && x <= region->x + region->width &&
            z >= region->z && z <= region->z + region->depth) {
            return region->id;
        }
    }
    return 0;
}

static __inline__ s32 Actor403100_FindEffectRegion(s16 x, s16 z)
{
    Actor403100RectEntry* region;
    for (region = D_actor_403100_80155638; region->value != -1; region++) {
        if (x >= region->x && x <= region->x + region->w && z >= region->y && z <= region->y + region->h)
            return region->value;
    }
    return 0;
}

#endif
