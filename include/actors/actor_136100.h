#ifndef ACTOR_136100_H
#define ACTOR_136100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block for the `actor_136100` overlay's cutscene actor.
///
/// `func_actor_136100_80133A88` allocates it with `Mem_Malloc(0x4F0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::work`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor136100Work*)task->work`.  The same function publishes the task
/// itself in `D_actor_136100_8014078C` and stores the `gameGetPtrSlot(3)`
/// task in `field_4B4`.
///
/// The block opens with the 0x14-byte animation context and its twenty
/// 0x28-byte animation slots -- `func_actor_136100_80133A88` hands
/// `func_800B3F84` both `work + 0x14` and `work + 0x334`, and
/// `func_actor_136100_801347B8` walks slots 1..19 through `Gp_AnimResetSlot`;
/// the `SVECTOR` it declares is dead and exists only to reserve the 8-byte
/// local slot its frame has.
/// The three pairs at 0x4C4, 0x4CC and 0x4D4 are value/countdown pairs the
/// overlay's small setters write together.
typedef struct Actor136100Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
    /* 0x334 */ byte       pad_334[0x140];
    /* 0x474 */ MATRIX     field_474; // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX     field_494; // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*      field_4B4; // gameGetPtrSlot(3) task
    /* 0x4B8 */ Task*      field_4B8; // Task_SpawnFromTable(ActorsShared80134898Desc, 2) task
    /* 0x4BC */ Task*      field_4BC; // Task_SpawnFromTable(ActorsShared80134898Desc, 3) task
    /* 0x4C0 */ Task*      field_4C0; // second dispatch task (NULL-checked senders)
    /* 0x4C4 */ s16        field_4C4; // set by func_actor_136100_80134838
    /* 0x4C6 */ s16        field_4C6; // cleared alongside field_4C4
    /* 0x4C8 */ s16        field_4C8; // tick counter: func_actor_136100_801323F8
    /* 0x4CA */ byte       pad_4CA[0x2];
    /* 0x4CC */ s16        field_4CC; // set by func_actor_136100_80134858
    /* 0x4CE */ s16        field_4CE; // cleared alongside field_4CC
    /* 0x4D0 */ s16        field_4D0; // request 5 tick counter
    /* 0x4D2 */ byte       pad_4D2[0x2];
    /* 0x4D4 */ s16        field_4D4; // set by func_actor_136100_80134878
    /* 0x4D6 */ s16        field_4D6; // cleared alongside field_4D4
    /* 0x4D8 */ s16        field_4D8; // shot count: func_actor_136100_80132BC0
    /* 0x4DA */ s16        field_4DA; // countdown: func_actor_136100_80132BC0
    /* 0x4DC */ u16        field_4DC; // cue step: func_actor_136100_80133904
    /* 0x4DE */ s16        field_4DE; // set by func_actor_136100_80133690
    /* 0x4E0 */ s16        field_4E0; // animation slot count reset by func_actor_136100_801347B8
    /* 0x4E2 */ u16        field_4E2; // index into the D_actor_136100_8013F218 animation chain
    /* 0x4E4 */ u16        field_4E4; // cue phase: func_actor_136100_80133904
    /* 0x4E6 */ byte       pad_4E6[0x4];
    /* 0x4EA */ s16        field_4EA; // fourth model part's Y rotation
    /* 0x4EC */ s16        field_4EC; // player-eff flag: Gp_KillPlayerEffs / Gp_SpawnWeaponEff
    /* 0x4EE */ byte       pad_4EE[0x2];
} Actor136100Work;
STATIC_ASSERT_SIZEOF(Actor136100Work, 0x4F0);

/// Payload `func_actor_136100_80134A18` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x3F7: the null-terminated pointer table at
/// `D_actor_136100_8013F180` (seven live entries followed by a null word) and
/// the number of live entries the sender counted in it.
typedef struct Actor136100Msg3F7 {
    /* 0x0 */ s32* table;
    /* 0x4 */ s32  count;
} Actor136100Msg3F7;
STATIC_ASSERT_SIZEOF(Actor136100Msg3F7, 0x8);

/// Work block for the overlay's display-fade task
/// (`func_actor_136100_80134588`).
///
/// The task allocates it with `Mem_Malloc(8, 0)` on its first tick and parks it
/// in `Task::work`, then advances all three channels by the task's
/// `spawnArg1` every frame and hands `r` and `g` to `Fade_DrawOverlay`.  It is
/// the same 8-byte shape the room overlays use (see `DumpingHoleFadeWork` in
/// `shelter_b3_dumping_hole`), and `r` is what the task's own
/// `>= 0x100` end-of-fade test reads.
typedef struct Actor136100FadeWork {
    /* 0x0 */ u8  pad_0[0x2];
    /* 0x2 */ s16 r;
    /* 0x4 */ s16 g;
    /* 0x6 */ s16 b; // advanced but never read back
} Actor136100FadeWork;
STATIC_ASSERT_SIZEOF(Actor136100FadeWork, 0x8);

/// Set by `func_actor_136100_801348F8` when the cutscene wants the display
/// back on; while it is non-zero the fade task kills itself instead of fading.
extern u16 D_actor_136100_8013F17C;

/// Next-animation table indexed by field_4DE - 0x2F; negative entries end
/// the chain, and live entries are sent as animation ids with 0x2F added.
extern s16 D_actor_136100_8013F1EC[];

/// Next animation indexed by field_4E0; negative entries skip the restart.
extern s16 D_actor_136100_8013F1FC[];

extern s32 D_actor_136100_8013F3C4;
extern s32 D_actor_136100_8013F3DC;

s32 func_actor_136100_80131EC4(Task* task);
s32 func_actor_136100_80131FBC(Task* task);

#endif // ACTOR_136100_H
