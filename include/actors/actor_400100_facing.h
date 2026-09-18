#ifndef ACTOR_400100_FACING_H
#define ACTOR_400100_FACING_H

#include "actors/actor_400100_motion.h"
#include "actors/actor_400100_update.h"
#include "main/wipsys.h"

s32                          Actor00100_Fn01388(GsCOORDINATE2*, GpRec18*, s16, SVECTOR*);
extern char                  Actor00100_D000D4;
extern Actor00100AnimCommand Actor00100_D1B9AC;
extern SVECTOR               Actor00100_D1BA90;

typedef struct {
    s16 vx, vy, vz, pad;
    u32 distanceSquared;
    s16 playerYaw;
    u16 contactYaw;
    s16 turnYaw, targetYaw;
    s16 messageResult;
    s16 pad16;
} Actor00100FacingScratch;

static __inline__ s16 Actor00100_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

static __inline__ s16 Actor00100_FacingAway(GsCOORDINATE2* p)
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

static __inline__ void Actor00100_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

static __inline__ s16 Actor00100_PositionYaw(Actor00100* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor00100_ConfigPositionDelta(config, actor->field_2C->coords, pos);
    coord = actor->field_2C->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor00100_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
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

static __inline__ s32 Actor00100_PlayerContactMessage(Actor00100Ctx* ctx, s32 mode)
{
    Task* player = Game_GetPtrSlot(3);
    return Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)ctx, mode), 0);
}

STATIC_ASSERT_SIZEOF(Actor00100FacingScratch, 0x18);
STATIC_ASSERT_SIZEOF(Actor00100FacingWork, 0xC20);

#endif
