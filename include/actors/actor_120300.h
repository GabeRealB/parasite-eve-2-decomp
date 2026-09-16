#ifndef ACTOR_120300_H
#define ACTOR_120300_H

#include "common.h"

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
typedef struct Actor120300Work {
    /* 0x000 */ byte  pad_0[0x4B8];
    /* 0x4B8 */ Task* field_4B8;
    /* 0x4BC */ Task* field_4BC;
    /* 0x4C0 */ s16   field_4C0;
    /* 0x4C2 */ s16   field_4C2;
    /* 0x4C4 */ byte  pad_4C4[0x4];
    /* 0x4C8 */ s16   field_4C8;
    /* 0x4CA */ s16   field_4CA;
    /* 0x4CC */ byte  pad_4CC[0x12];
    /* 0x4DE */ s16   field_4DE; // player-eff flag: Gp_SpawnWeaponEff
} Actor120300Work;
STATIC_ASSERT_SIZEOF(Actor120300Work, 0x4E0);

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
