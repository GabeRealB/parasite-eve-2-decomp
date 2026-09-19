#ifndef ACTOR_400100_UPDATE_H
#define ACTOR_400100_UPDATE_H

#include "actors/actor_400100.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include <psyq/inline_c.h>

/// Overlay-local view of the player animation entries, including word 0x24.
typedef struct Actor00100PlayerAnim {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ s32  field_1C;
    /* 0x20 */ byte pad_20[4];
    /* 0x24 */ s32  field_24;
} Actor00100PlayerAnim;

typedef struct Actor00100StateTable {
    void (*fn[39])(Actor00100*);
} Actor00100StateTable;
STATIC_ASSERT_SIZEOF(Actor00100StateTable, 0x9C);

typedef struct Actor00100AnimWord {
    s32 value;
} Actor00100AnimWord;
typedef struct Actor00100StateC08 {
    byte pad_0[6];
    u8   field_6;
} Actor00100StateC08;

extern Actor00100StateTable  Actor00100_D000F0;
extern Actor00100AnimCommand Actor00100_D1B9D0;
extern Actor00100PlayerAnim* Gp_PlayerAnimBlkTbl[];
extern u16                   Gp_WeaponIdBase[];
extern Actor00100StateC08    Gp_StateC08;
extern Actor00100AnimWord    Actor00100_D1B9BC, Actor00100_D1B9C0;
extern s8                    D_8007218A;
extern u8                    D_80073BA9, D_801153F4;
void                         Actor00100_Fn01900(Actor00100*, s16, s16, s16, s16, u8);
void                         Actor00100_Fn0375C(Actor00100*);

static __inline__ s16 Actor00100_InRegion(Actor00100* actor)
{
    GsCOORDINATE2* coord = actor->field_2C->coords;
    if ((u32)(coord->coord.t[0] - 0x1541) < 0x196DU) {
        if (coord->coord.t[2] < 0x5B4)
            return 1;
    }
    return 0;
}
static __inline__ s16 Actor00100_InDirection(Actor00100* actor, VECTOR* motion)
{
    GsCOORDINATE2* coord = actor->field_2C->coords;
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
static __inline__ void Actor00100_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &gGfxViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}
static __inline__ SVECTOR* Actor00100_AllocVector(SVECTOR** head)
{
    SVECTOR* p = *head - 1;
    *head      = p;
    return p;
}

#endif
