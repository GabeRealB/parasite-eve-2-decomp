#ifndef ACTOR_342400_H
#define ACTOR_342400_H

#include "common.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// Status flags at `Actor342400Work` + 0xEC, read through two widths: guards
/// test bit 0 as a halfword and then bits 0x102 as a word, the same shape as
/// `Actor341700Flags`.
typedef union Actor342400Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor342400Flags;
STATIC_ASSERT_SIZEOF(Actor342400Flags, 0x4);

/// Per-actor state block for the `actor_342400` overlay's main enemy.
///
/// `func_actor_342400_80163C58` and `func_actor_342400_80163E70` both allocate
/// it with `Mem_Calloc(0x454, 0)` and store it in the `Task::idMap` slot
/// (0x1C), so the size below is the allocation, not a guess: this actor reuses
/// that pointer field for its own work block and it is *not* a `TaskIdMap`
/// here. Reach it with `(Actor342400Work*)task->idMap`.
///
/// The layout mirrors the sibling `actor_341700` overlay, whose work block is
/// the same size and drives the same state machine: `field_420` / `field_422`
/// are the state and sub-state indices the handler table walks; `field_412`
/// is the per-state frame counter.
typedef struct Actor342400Work {
    /* 0x000 */ byte      pad_0[0x20];
    /* 0x020 */ MATRIX    colorMtx; // the model's `TmdObject::field_20`
    /* 0x040 */ MATRIX    lightMtx; // the model's `TmdObject::field_1C`
    /* 0x060 */ byte      pad_60[0x10];
    /* 0x070 */ SVECTOR   field_70; // origin of slot 4 entry 0's coords[3], carried into view space by func_actor_342400_8016B5B0
    /* 0x078 */ s16       field_78; // pitch, fed to RotMatrixX by func_actor_342400_801670C0
    /* 0x07A */ s16       field_7A; // heading fed to rsin / rcos
    /* 0x07C */ s16       field_7C; // roll, fed to RotMatrixZ by func_actor_342400_801670C0
    /* 0x07E */ byte      pad_7E[0x2];
    /* 0x080 */ u16       field_80; // spawn position: root coord.t[0]
    /* 0x082 */ u16       field_82; // root coord.t[1], after lifting it by 0x3C
    /* 0x084 */ u16       field_84; // root coord.t[2]
    /* 0x086 */ byte      pad_86[0xA];
    /* 0x090 */ u16       field_90; // root coord.t[0], snapshotted by func_actor_342400_8016B5B0
    /* 0x092 */ u16       field_92; // root coord.t[1]
    /* 0x094 */ u16       field_94; // root coord.t[2]
    /* 0x096 */ byte      pad_96[0x2];
    /* 0x098 */ SVECTOR   field_98; // translation of coords[6] relative to the view, from func_actor_342400_801648E4
    /* 0x0A0 */ GpAnimCtx anim;
    /// First of the nine `GpAnimSlot`s (0xB4..0x21C) handed to `func_800B3F84`;
    /// the second overlaps `flags_EC`, so only the first is spelled out.
    /* 0x0B4 */ GpAnimSlot       slot_B4;
    /* 0x0DC */ byte             pad_DC[0x10];
    /* 0x0EC */ Actor342400Flags flags_EC;
    /* 0x0F0 */ byte             pad_F0[0x12C];
    /* 0x21C */ byte             field_21C[0x90]; // `func_800B3F84`'s arg3 buffer
    /* 0x2AC */ GpObj            obj_2AC;
    /* 0x2CC */ GpObj            obj_2CC;
    /* 0x2EC */ GpRec18          rec_2EC[8];
    /* 0x3AC */ GpObj            obj_3AC;
    /* 0x3CC */ byte             pad_3CC[0x30];
    /* 0x3FC */ GpEffArg         eff_3FC;   // `func_800FDB18`'s arg3; field_0 is the model's second coord part
    /* 0x404 */ byte             pad_404[0x8];
    /* 0x40C */ s16              field_40C; // heading func_actor_342400_801648E4 moves the root along
    /* 0x40E */ byte             pad_40E[0x2];
    /* 0x410 */ s16              field_410;
    /* 0x412 */ u16              field_412; // per-state frame counter
    /* 0x414 */ s16              field_414; // animation request kind
    /* 0x416 */ byte             pad_416[0x2];
    /* 0x418 */ s16              field_418; // animation id
    /* 0x41A */ byte             pad_41A[0x2];
    /* 0x41C */ s16              field_41C; // animation speed / step scale
    /* 0x41E */ s16              field_41E; // 1 lets `field_448` jump the state machine
    /* 0x420 */ u16              field_420; // state index
    /* 0x422 */ u16              field_422; // sub-state index
    /* 0x424 */ byte             pad_424[0x2];
    /* 0x426 */ s16              field_426;
    /* 0x428 */ s16              field_428;
    /* 0x42A */ s16              field_42A;
    /* 0x42C */ byte             pad_42C[0x6];
    /* 0x432 */ s16              field_432; // 1 runs func_actor_342400_80169654 on the spawn position
    /* 0x434 */ byte             pad_434[0x2];
    /* 0x436 */ s16              field_436; // animation step applied by ActorsShared801698d4
    /* 0x438 */ s16              field_438;
    /* 0x43A */ s16              field_43A;
    /* 0x43C */ byte             pad_43C[0x4];
    /* 0x440 */ s16              field_440; // picks animation 5 (zero) or 6 after animation 8
    /* 0x442 */ u16              field_442;
    /* 0x444 */ u16              field_444; // angle, masked to 0xFFF
    /* 0x446 */ byte             pad_446[0x2];
    /* 0x448 */ s16              field_448; // pending state request; 4 moves the task to state 4 once the enemy is dead
    /* 0x44A */ s16              field_44A;
    /* 0x44C */ u16              field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    /* 0x44E */ byte             pad_44E[0x1];
    /* 0x44F */ u8               field_44F; // 1 = run ActorsShared8016bef0 after the sub-state
    /* 0x450 */ byte             pad_450[0x1];
    /* 0x451 */ u8               field_451;
    /* 0x452 */ byte             pad_452[0x2];
} Actor342400Work;
STATIC_ASSERT_SIZEOF(Actor342400Work, 0x454);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg`. The overlay's 0x7DB handler,
/// `func_actor_342400_801626AC`, reads the halfword at 0x2.
typedef struct Actor342400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor342400Msg;
STATIC_ASSERT_SIZEOF(Actor342400Msg, 0x4);

/// The same four bytes as the overlay builds them for its own 0x7DB send in
/// `func_actor_342400_80163010`: two id bytes followed by the halfword
/// (`Task::spawnArg1`) the receiver reads; see `Actor104000Msg7DA`.
typedef struct Actor342400Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor342400Msg7DB;
STATIC_ASSERT_SIZEOF(Actor342400Msg7DB, 0x4);

/// 8-byte record in the table at `D_actor_342400_8016BF58`, indexed by the
/// halfword at `Task` 0x36 (the high half of `spawnArg1`). A child task that
/// finishes writes 2 into `field_6` before killing itself.
typedef struct Actor342400Slot {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[0x2];
    /* 0x6 */ s16  field_6;
} Actor342400Slot;
STATIC_ASSERT_SIZEOF(Actor342400Slot, 0x8);

/// 4-byte record in the table at `D_actor_342400_8016C010`, indexed (1..16)
/// by `Game_Session->field_136`. `func_actor_342400_801626CC` compares
/// an enemy's x against `limit` when `axis` is 0 and its z otherwise.
typedef struct Actor342400Limit {
    /* 0x0 */ s16 axis;
    /* 0x2 */ s16 limit;
} Actor342400Limit;
STATIC_ASSERT_SIZEOF(Actor342400Limit, 0x4);

/// Work block of the child task handled by `func_actor_342400_80163178`,
/// stored in its `Task::idMap` slot; it is killed once `field_A` reaches 3.
/// `func_actor_342400_80162084` allocates it (`Mem_Calloc(0xC, 0)`) and
/// spawns the two enemies it holds.
typedef struct Actor342400ChildWork {
    /* 0x0 */ GpEnemy* enemy0;
    /* 0x4 */ GpEnemy* enemy1;
    /* 0x8 */ s16      field_8;
    /* 0xA */ s16      field_A;
} Actor342400ChildWork;
STATIC_ASSERT_SIZEOF(Actor342400ChildWork, 0xC);

/// Work block of the controller task set up by `func_actor_342400_801628F0`
/// (`Mem_Calloc(6, 0)`, stored in its `Task::idMap` slot).
/// `func_actor_342400_80162A34` counts `field_0` up to 15 before arming
/// `Gp_StateF0`.
typedef struct Actor342400CtrlWork {
    /* 0x0 */ s16 field_0; // frame counter
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4; // set to 4 by the 0x7DB handler; 4 idles the controller
} Actor342400CtrlWork;
STATIC_ASSERT_SIZEOF(Actor342400CtrlWork, 0x6);

/// Work block of the spawner task set up by `func_actor_342400_80162B60`
/// (`Mem_Calloc(8, 0)`, stored in its `Task::idMap` slot); holds the enemy it
/// spawned from `D_actor_342400_80173A54`. `func_actor_342400_80162C10`
/// counts `field_4` up past 60 before releasing the enemy.
typedef struct Actor342400SpawnWork {
    /* 0x0 */ GpEnemy* enemy;
    /* 0x4 */ s16      field_4; // frame counter
    /* 0x6 */ byte     pad_6[2];
} Actor342400SpawnWork;
STATIC_ASSERT_SIZEOF(Actor342400SpawnWork, 0x8);

extern u8               D_actor_342400_8016BF48[]; // stored into `Task::field_24` by func_actor_342400_801628F0
extern Actor342400Slot  D_actor_342400_8016BF58[];
extern Actor342400Limit D_actor_342400_8016C010[];
extern TaskDesc         D_actor_342400_80173A54;
extern TaskDesc         D_actor_342400_8016BFE0;
extern TaskDesc         D_801575F0;                // absolute, spawned by func_actor_342400_80162DA0
extern TaskDesc         D_80151E60;                // absolute, spawned twice by func_actor_342400_80162084
extern u16              D_actor_342400_80173AAC;   // spawn counter, `<< 12` into `GpEnemy::field_8`
extern u8               D_actor_342400_80173A84[]; // per animation id (1-based): value for `field_44F`
extern u8               D_actor_342400_80173A98[]; // per animation id (1-based): the animation to follow it
extern u8               D_801153F4;                // absolute; nonzero skips the controller's state handler
extern GpPairSrcE       D_actor_342400_80170588;   // the main enemy's `GpEnemy::field_50` record
extern u8               D_actor_342400_801739E8[]; // animation bank handed to `func_800B3F84`
extern u8               D_actor_342400_80173A3C[]; // stored into `Task::field_24` by func_actor_342400_80163C58

void func_actor_342400_80162084(Task* arg0);
void func_actor_342400_801621D8(Task* arg0);
void func_actor_342400_80162324(Task* arg0);
s16  func_actor_342400_801624A4(void);
s16  func_actor_342400_801626CC(s16 arg0, s16 arg1, s16 arg2);
void func_actor_342400_801628F0(Task* arg0);
void func_actor_342400_8016299C(Task* arg0);
void func_actor_342400_80162A34(Task* arg0);
void func_actor_342400_80162AB0(Task* arg0);
void func_actor_342400_80162FFC(Task* arg0);
void func_actor_342400_80163010(Task* arg0);
void func_actor_342400_801630A4(Task* arg0);
void func_actor_342400_80163178(Task* arg0);
void func_actor_342400_801637DC(Task* arg0);
void func_actor_342400_801653DC(Task* arg0, s16 arg1);
s32  func_actor_342400_80169518(Task* arg0);
void func_actor_342400_80169654(Task* arg0, s16 arg1, u16* arg2);
s16  func_actor_342400_80169728(Task* arg0, s32 arg1);
void func_actor_342400_8016A494(Task* arg0);
void func_actor_342400_80163200(s16 arg0, s16 arg1, s16 arg2);
void func_actor_342400_801632D4(Task* arg0);

#endif
