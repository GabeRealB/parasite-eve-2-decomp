#ifndef ACTOR_120300_H
#define ACTOR_120300_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block this overlay hangs off `Actor120300.field_1C`; each pair at
/// 0x4C0 and 0x4C8 is a request code plus its phase counter, reset together.
/// `field_4DE` is a 0/1 latch, the same shape `Actor136100Work.field_4EC`
/// has: `func_actor_120300_80133E94` calls `Gp_SpawnWeaponEff` while it is
/// set, clears it, then hands `Gp_MsgPlayerWeapon` the zero that follows.
///
/// 0x4B8/0x4BC are `Gp_DispatchMsg` targets, not state:
/// `func_actor_120300_80133D04` sends message 0x7D5 to the actor, to 0x4B8 and
/// to 0x4BC in turn.
///
/// The block opens with the same 0x14-byte animation context and its twenty
/// 0x28-byte animation slots `Actor136100Work` opens with. `Mem_Malloc` is
/// asked for 0x4E4 bytes -- the whole block -- by
/// `func_actor_120300_80132004` and `func_actor_120300_801321C8`, while
/// `func_actor_120300_80133330` walks slots 1..19 through `Gp_AnimResetSlot`
/// after parking 8 in `field_4D4`, then lifts the scale at `field_4E0` to
/// 0x1000 once the actor is up.
typedef struct Actor120300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
    /* 0x334 */ byte       pad_334[0x180];
    /* 0x4B4 */ Task*      field_4B4; // Gp_DispatchMsg target for msgs 0x3E8/0x3E9
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ Task*      field_4BC;
    /* 0x4C0 */ s16        field_4C0;
    /* 0x4C2 */ s16        field_4C2;
    /* 0x4C4 */ byte       pad_4C4[0x4];
    /* 0x4C8 */ s16        field_4C8;
    /* 0x4CA */ s16        field_4CA;
    /* 0x4CC */ byte       pad_4CC[0x8];
    /* 0x4D4 */ s16        field_4D4; // written by func_actor_120300_80133330
    /* 0x4D6 */ byte       pad_4D6[0x8];
    /* 0x4DE */ s16        field_4DE; // player-eff flag: Gp_SpawnWeaponEff
    /* 0x4E0 */ s16        field_4E0; // uniform scale: broadcast to all three axes of a ScaleMatrix vector, so 0x1000 is 1.0
    /* 0x4E2 */ byte       pad_4E2[0x2];
} Actor120300Work;
STATIC_ASSERT_SIZEOF(Actor120300Work, 0x4E4);

/// `Task` as this overlay uses it: only the slot at 0x1C is retyped, holding
/// the actor's own work block rather than a `TaskIdMap`.
typedef struct Actor120300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor120300Work* field_1C;
} Actor120300;

extern Actor120300* D_actor_120300_80141BA8;

void func_actor_120300_80133E14(s16 arg0);
void func_actor_120300_80133E34(s16 arg0);
void func_actor_120300_80133E54(void);
void func_actor_120300_80133E94(void);

#endif
