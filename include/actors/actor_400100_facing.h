#ifndef ACTOR_400100_FACING_H
#define ACTOR_400100_FACING_H

#include "actors/actor_400100_motion.h"
#include "actors/actor_400100_update.h"
#include "gameplay/D4.h"
#include "main/wipsys.h"
#include "main/coord.h"

s32                          Actor00100_Fn01388(GpCoord*, GpRec18*, s16, SVECTOR*);
extern Actor00100AnimCommand Actor00100_D1B9AC;
extern SVECTOR               Actor00100_D1BA90;

static __inline__ s16 Actor00100_FacingAway(GpCoord* p)
{
    s16 angle = ratan2(-p->coord.m[2][0], p->coord.m[2][2]);
    s32 value = angle;
    if (value < 0)
        value = -value;
    if (value >= 0x501) {
        if (p->coord.t[0] < 0x2AF9)
            return 1;
        if ((s16)ratan2(-p->coord.m[2][0], p->coord.m[2][2]) >= 0x708)
            return 1;
        if ((s16)ratan2(-p->coord.m[2][0], p->coord.m[2][2]) <= 0)
            return 1;
    }
    return 0;
}

typedef struct {
    u8  pad0[0x8C0];
    s32 field_8C0, field_8C4, field_8C8;
    u8  pad8CC[4];
    s16 field_8D0, field_8D2, field_8D4;
    u8  pad8D6[0x2A];
    s32 field_900;
    u8  field_904, field_905;
    s16 field_906;
    u8  pad908[0x2E8];
    s16 field_BF0, field_BF2, field_BF4;
    u8  padBF6[0x28];
    s16 field_C1E;
} Actor00100FacingWork;

STATIC_ASSERT_SIZEOF(Actor00100FacingWork, 0xC20);

#endif
