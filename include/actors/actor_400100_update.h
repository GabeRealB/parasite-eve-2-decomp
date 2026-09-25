#ifndef ACTOR_400100_UPDATE_H
#define ACTOR_400100_UPDATE_H

#include "actors/actor_400100.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include <psyq/inline_c.h>
#include "gte.h"

typedef struct Actor00100StateTable {
    TaskFunc fn[39];
} Actor00100StateTable;
STATIC_ASSERT_SIZEOF(Actor00100StateTable, 0x9C);

typedef struct Actor00100AnimWord {
    s32 value;
} Actor00100AnimWord;

extern Actor00100AnimCommand Actor00100_D1B9D0;
extern GpAnimBlk*            Gp_PlayerAnimBlkTbl[];
extern u16                   Gp_WeaponIdBase[];
extern Actor00100AnimWord    Actor00100_D1B9BC, Actor00100_D1B9C0;
void                         Actor00100_Fn01900(Task*, s16, s16, s16, s16, u8);
void                         Actor00100_Fn0375C(Task*);

static __inline__ s16 Actor00100_InRegion(Task* actor)
{
    GpCoord* coord = actor->extra.tmd->coords;
    if ((u32)(coord->coord.t[0] - 0x1541) < 0x196DU) {
        if (coord->coord.t[2] < 0x5B4)
            return 1;
    }
    return 0;
}
static __inline__ s16 Actor00100_InDirection(Task* actor, VECTOR* motion)
{
    GpCoord* coord = actor->extra.tmd->coords;
    if (abs((s16)ratan2(motion->vx, motion->vz)) >= 0x501) {
        if (coord->coord.t[0] < 0x2AF9)
            return 1;
        if ((s16)ratan2(motion->vx, motion->vz) >= 0x708)
            return 1;
        if ((s16)ratan2(motion->vx, motion->vz) <= 0)
            return 1;
    }
    return 0;
}
static __inline__ SVECTOR* Actor00100_AllocVector(SVECTOR** head)
{
    SVECTOR* p                     = SCRATCH_HEAD_AT(head, SVECTOR) - 1;
    SCRATCH_HEAD_AT(head, SVECTOR) = p;
    return p;
}

#endif
