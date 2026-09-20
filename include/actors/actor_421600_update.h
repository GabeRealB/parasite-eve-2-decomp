#ifndef ACTOR_421600_UPDATE_H
#define ACTOR_421600_UPDATE_H

#include "actors/actor_421600.h"
#include "main/gfx.h"
#include <psyq/inline_c.h>

/// Per-frame scratch: the view-space body position and its arena zone.
typedef struct Actor421600UpdateScratch {
    /* 0x00 */ VECTOR  unused;
    /* 0x10 */ SVECTOR pos;
    /* 0x18 */ s16     zone;
    /* 0x1A */ s16     pad;
} Actor421600UpdateScratch;
STATIC_ASSERT_SIZEOF(Actor421600UpdateScratch, 0x1C);

typedef struct Actor421600StateTable {
    void (*fn[40])(Actor421600*);
} Actor421600StateTable;
STATIC_ASSERT_SIZEOF(Actor421600StateTable, 0xA0);

/// Command fields filled from the player's current animation block.
typedef struct Actor421600AnimCommand {
    /* 0x00 */ s32 entries[4];
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
} Actor421600AnimCommand;

typedef struct Actor421600PlayerAnim {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ s32  field_1C;
    /* 0x20 */ byte pad_20[4];
    /* 0x24 */ s32  field_24;
} Actor421600PlayerAnim;

typedef struct Actor421600AnimWord {
    s32 value;
} Actor421600AnimWord;

extern const Actor421600StateTable D_actor_421600_80131EFC;
extern Actor421600PlayerAnim*      Gp_PlayerAnimBlkTbl[];
extern u16                         Gp_WeaponIdBase[];
extern s8                          D_8007218A;
extern u8                          D_80073BA9;
extern u16                         D_801876A8, D_801876AA;
extern Actor421600AnimWord         D_actor_421600_801510A0;

/// The fallback command's +0x14 endpoint overlaps the next table's first word.
/// Keep a separate C view of the endpoint so its address is materialized at
/// the store, independently of comparisons against the table base.
extern Actor421600AnimWord Actor421600FallbackEnd __asm__("D_actor_421600_801510A4");

void func_actor_421600_8013E858(Actor421600*);
void func_actor_421600_80135F6C(Actor421600*);
void func_actor_421600_80136138(Actor421600*);
void func_actor_421600_8013A554(Actor421600*);
void func_actor_421600_8013E8AC(Actor421600*);
void func_actor_421600_8013B00C(Actor421600*);
void func_actor_421600_8013B4C4(Actor421600*);
void func_actor_421600_8013B8E0(Actor421600*);
void func_actor_421600_8013C8E0(Actor421600*);
void func_actor_421600_8013CD3C(Actor421600*);
void func_actor_421600_8013D1DC(Actor421600*);
void func_actor_421600_8013ED24(Actor421600*);
void func_actor_421600_8013EE0C(Actor421600*);
void func_actor_421600_8013A404(Actor421600*);
void func_actor_421600_8013EC28(Actor421600*);
void func_actor_421600_801366F4(Actor421600*);
void func_actor_421600_801369A0(Actor421600*);
void func_actor_421600_80136C88(Actor421600*);
void func_actor_421600_801373D4(Actor421600*);
void func_actor_421600_8013E9D8(Actor421600*);
void func_actor_421600_8013848C(Actor421600*);
void func_actor_421600_80138D24(Actor421600*);
void func_actor_421600_8013903C(Actor421600*);
void func_actor_421600_801392A8(Actor421600*);
void func_actor_421600_8013EAAC(Actor421600*);
void func_actor_421600_8013947C(Actor421600*);
void func_actor_421600_8013EB7C(Actor421600*);
void func_actor_421600_80138750(Actor421600*);
void func_actor_421600_80139718(Actor421600*);
void func_actor_421600_8013BA70(Actor421600*);

void func_actor_421600_80132EC0(Actor421600*, s16, s16, s16, s16, u8);
void func_actor_421600_80133444(GsCOORDINATE2*);
void func_actor_421600_801354D8(Actor421600*);

static __inline__ void Actor421600_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
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

static __inline__ Actor421600UpdateScratch* Actor421600_AllocUpdateScratch(Actor421600UpdateScratch** head)
{
    Actor421600UpdateScratch* p = *head - 1;
    *head                       = p;
    return p;
}
static __inline__ s32 Actor421600_HasPlayerContact(GpRec18* records)
{
    s16 i;
    for (i = 0; i < 12; i++) {
        if (records[i].key == 0)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x10000)
            return 1;
    }
    return 0;
}

#endif
