#ifndef ACTOR_341700_H
#define ACTOR_341700_H

#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

typedef struct Actor341700 {
    /* 0x00 */ byte       pad_0[0x2C];
    /* 0x2C */ TmdObject* field_2C;
} Actor341700;

/// Status flags at `Actor341700Work` + 0xEC, read through two widths.
///
/// Every guard in the overlay tests bit 0 as a halfword and then bits 0x102 as
/// a word (`func_actor_341700_80168468` is the out-of-line copy of the test),
/// so both views are modelled explicitly rather than casting at the use site.
typedef union Actor341700Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} Actor341700Flags;
STATIC_ASSERT_SIZEOF(Actor341700Flags, 0x4);

/// Per-actor state block for the `actor_341700` overlay's main enemy.
///
/// `func_actor_341700_80162974` and `func_actor_341700_80162B8C` both allocate
/// it with `memCalloc(0x454, 0)` and store it in the `Task::work` slot
/// (0x1C), so the size below is the allocation, not a guess: this actor reuses
/// that pointer field for its own work block and it is *not* a `TaskIdMap`
/// here. Reach it with `(Actor341700Work*)task->work`.
///
/// `field_420` / `field_422` are the state and sub-state indices the handler
/// tables walk; `field_412` is the per-state frame counter. `field_414` ..
/// `field_41C` are the animation request the actor hands to its player.
typedef struct Actor341700Work {
    /* 0x000 */ MATRIX    savedRootMtx; // root coord matrix saved at death, rescaled each frame while the model shrinks
    /* 0x020 */ MATRIX    colorMtx;     // the model's `TmdObject::colorMtx`
    /* 0x040 */ MATRIX    lightMtx;     // the model's `TmdObject::lightMtx`
    /* 0x060 */ VECTOR    field_60;     // position func_actor_341700_801640F8 snaps the root back to when blocked
    /* 0x070 */ SVECTOR   field_70;     // origin of slot 4 entry 0's coords[3], carried into view space by func_actor_341700_8016A2CC
    /* 0x078 */ s16       field_78;     // pitch, fed to RotMatrixX by func_actor_341700_80165DDC
    /* 0x07A */ s16       field_7A;     // heading fed to rsin / rcos
    /* 0x07C */ s16       field_7C;     // roll, fed to RotMatrixZ by func_actor_341700_80165DDC
    /* 0x07E */ byte      pad_7E[0x2];
    /* 0x080 */ u16       field_80;     // spawn position: root coord.t[0]
    /* 0x082 */ u16       field_82;     // root coord.t[1], after lifting it by 0x3C
    /* 0x084 */ u16       field_84;     // root coord.t[2]
    /* 0x086 */ byte      pad_86[0x2];
    /* 0x088 */ s16       field_88;     // x of the offset to the nearer player actor
    /* 0x08A */ s16       field_8A;     // y of that offset
    /* 0x08C */ s16       field_8C;     // z of that offset
    /* 0x08E */ byte      pad_8E[0x2];
    /* 0x090 */ u16       field_90;     // root coord.t[0], snapshotted by func_actor_341700_8016A2CC
    /* 0x092 */ u16       field_92;     // root coord.t[1]
    /* 0x094 */ u16       field_94;     // root coord.t[2]
    /* 0x096 */ byte      pad_96[0x2];
    /* 0x098 */ SVECTOR   field_98;     // translation of coords[6] relative to the view, from func_actor_341700_80163600
    /* 0x0A0 */ GpAnimCtx anim;
    /// First of the nine `GpAnimSlot`s (0xB4..0x21C) handed to `func_800B3F84`;
    /// the second overlaps `flags_EC`, so only the first is spelled out.
    /* 0x0B4 */ GpAnimSlot       slot_B4;
    /* 0x0DC */ byte             pad_DC[0x10];
    /* 0x0EC */ Actor341700Flags flags_EC;
    /* 0x0F0 */ byte             pad_F0[0x12C];
    /* 0x21C */ byte             field_21C[0x90]; // `func_800B3F84`'s arg3 buffer
    /* 0x2AC */ GpObj            obj_2AC;
    /* 0x2CC */ GpObj            obj_2CC;
    /* 0x2EC */ GpRec18          rec_2EC[8];
    /* 0x3AC */ GpObj            obj_3AC;
    /* 0x3CC */ GpRec18          rec_3CC[2]; // records of `obj_3AC`
    /* 0x3FC */ GpEffArg         eff_3FC;    // `func_800FDB18`'s arg3; field_0 is the model's second coord part
    /* 0x404 */ byte             pad_404[0x8];
    /* 0x40C */ s16              field_40C;  // heading func_actor_341700_80163600 moves the root along
    /* 0x40E */ s16              field_40E;  // hit cooldown: `Gp_GetIdParam2` of the last hit, counted down each frame
    /* 0x410 */ s16              field_410;
    /* 0x412 */ u16              field_412;  // per-state frame counter
    /* 0x414 */ s16              field_414;  // animation request kind
    /* 0x416 */ s16              field_416;  // animation id last applied to the slots
    /* 0x418 */ s16              field_418;  // animation id
    /* 0x41A */ u16              field_41A;  // frames since the animation was applied
    /* 0x41C */ s16              field_41C;  // animation speed / step scale
    /* 0x41E */ s16              field_41E;  // 1 lets `field_448` jump the state machine
    /* 0x420 */ u16              field_420;  // state index
    /* 0x422 */ u16              field_422;  // sub-state index
    /* 0x424 */ s16              field_424;  // yaw added to model parts 3..5, a third each; eased toward zero each frame
    /* 0x426 */ s16              field_426;
    /* 0x428 */ s16              field_428;
    /* 0x42A */ s16              field_42A;
    /* 0x42C */ s16              field_42C; // frames spent turning toward field_444; 16 enters state 3
    /* 0x42E */ byte             pad_42E[0x2];
    /* 0x430 */ u16              field_430; // Y scale while the model shrinks after death
    /* 0x432 */ s16              field_432; // 1 runs func_actor_341700_80168370 on the spawn position
    /* 0x434 */ s16              field_434; // pitch latched when a sway ends, then eased back to zero
    /* 0x436 */ s16              field_436; // turn step func_actor_341700_801685F0 applies to the heading
    /* 0x438 */ s16              field_438;
    /* 0x43A */ s16              field_43A; // distance to the nearer player actor
    /* 0x43C */ byte             pad_43C[0x2];
    /* 0x43E */ s16              field_43E; // counted down each frame by func_actor_341700_801640F8
    /* 0x440 */ s16              field_440; // picks animation 5 (zero) or 6 after animation 8
    /* 0x442 */ s16              field_442; // frame phase driving the pitch sway
    /* 0x444 */ u16              field_444; // heading to the nearer player actor relative to field_7A, masked to 0xFFF
    /* 0x446 */ s16              field_446; // randomised hold in frames
    /* 0x448 */ s16              field_448; // pending state request; 4 moves the task to state 4 once the enemy is dead
    /* 0x44A */ s16              field_44A;
    /* 0x44C */ u16              field_44C; // message 0x2C00's halfword, when its low nibble is 1..5
    /* 0x44E */ u8               field_44E; // set while the enemy carries status flag 4/8
    /* 0x44F */ u8               field_44F; // 1 = run func_actor_341700_8016AC0C after the sub-state
    /* 0x450 */ byte             pad_450[0x1];
    /* 0x451 */ u8               field_451;
    /* 0x452 */ byte             pad_452[0x2];
} Actor341700Work;
STATIC_ASSERT_SIZEOF(Actor341700Work, 0x454);

/// The overlay's *other* work block, for the task `func_actor_341700_8016D130`
/// starts: that function calls `memCalloc(0x80, 0)` and stores the result in
/// the same `Task::work` slot, so the two blocks never coexist on one task.
///
/// `field_0` is the state index `func_actor_341700_8016CC9C` dispatches
/// through its three-entry handler table; `field_2` holds the previous value
/// and `field_4` the state-change flag. `func_actor_341700_8016CEB4` copies
/// the three leading bytes of an incoming command over `field_18` .. `field_1A`.
///
/// `light` / `color` are the matrices this block is allocated for:
/// `func_actor_341700_8016D130` stores their addresses into the model's
/// `TmdObject.lightMtx` / `field_20` light and colour matrix slots, so the
/// actor rasterises through its own work block rather than a separate
/// `MATRIX` allocation.
typedef struct Actor341700SubWork {
    /* 0x00 */ s16    field_0;
    /* 0x02 */ s16    field_2;
    /* 0x04 */ s16    field_4;
    /* 0x06 */ s16    field_6;
    /* 0x08 */ byte   pad_8[0x10];
    /* 0x18 */ u8     field_18;
    /* 0x19 */ u8     field_19;
    /* 0x1A */ u8     field_1A;
    /* 0x1B */ byte   pad_1B[0x1];
    /* 0x1C */ MATRIX light;
    /* 0x3C */ MATRIX color;
    /* 0x5C */ byte   pad_5C[0x24];
} Actor341700SubWork;
STATIC_ASSERT_SIZEOF(Actor341700SubWork, 0x80);

/// The three leading bytes of a command packet, as `func_actor_341700_8016CEB4`
/// copies them into the work block.
typedef struct Actor341700CmdBytes {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} Actor341700CmdBytes;

/// A command packet an actor task receives. The same four bytes are read both
/// byte-wise (the copy above) and as halfwords — offset 0 is the opcode the
/// handler tests against 0x2704 and offset 2 the sub-command it switches on —
/// so the two views are modelled here rather than cast at the use sites.
typedef union Actor341700Cmd {
    /* 0x0 */ Actor341700CmdBytes bytes;
    /* 0x0 */ u16                 halfs[2];
} Actor341700Cmd;
STATIC_ASSERT_SIZEOF(Actor341700Cmd, 0x4);

/// Global render mode: 2 hides the model, 0 runs the state handler then
/// colours it, 1 only colours it.
extern u8 D_801153F4;

extern u32 Gp_LcgState;

/// Main-executable byte; while it is 1 the contact-record push and avoid
/// helpers return at once without touching the coordinate.
extern u8      D_80072729;
extern SVECTOR D_actor_341700_80175F7C[];
extern u8      D_actor_341700_801760FC[];

void func_8017FCF4(GsCOORDINATE2* coord, SVECTOR* vec);

/// Nine state handlers, indexed by `Actor341700Work::field_420`; copied onto the
/// stack before dispatch, as the sibling `D_actor_342400_80161F50` is.
extern TaskFuncTable9 D_actor_341700_80161F0C;

/// Ten state handlers for `func_actor_341700_80165DDC`, indexed by
/// `Actor341700Work::field_420`; copied onto the stack before dispatch.
extern TaskFuncTable10 D_actor_341700_80161FA4;

/// Eleven state handlers for `func_actor_341700_80162DCC`, indexed by
/// `Actor341700Work::field_420`; copied onto the stack before dispatch.
extern TaskFuncTable11 D_actor_341700_80161E64;

/// `field_41C * arg1`, fixed point `<< 0xC >> 0x10` — the per-frame walk step
/// the turning states add to the root coord through `rsin` / `rcos`.
/// Root-coordinate update run after the rotation rebuild; see
/// `func_actor_342400_801653DC`.
void func_actor_341700_801640F8(Task* arg0, s16 arg1);

void func_actor_341700_80168370(Task* arg0, s16 arg1, SVECTOR3* arg2);

s32 func_actor_341700_80168444(Task* arg0, s16 arg1);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2);

s32 func_actor_341700_8016CEB4(Task* task, s32 arg1, Actor341700Cmd* cmd);

/// Dispatch table of the controlled enemy's state handlers: spawn/setup,
/// per-frame tick and teardown (`Gp_DestroyEnemy`).
extern GpEnemyTaskFuncTable3 D_actor_341700_80162064;

void func_actor_341700_80162070(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height, u8 shade);
void func_actor_341700_801626C4(Task* arg0);
void func_actor_341700_80163268(Task* arg0);
void func_actor_341700_801639C0(Task* arg0);
void func_actor_341700_80163AF0(Task* arg0);
void func_actor_341700_80163E58(Task* arg0);
void func_actor_341700_80163FBC(Task* arg0);
void func_actor_341700_801649DC(Task* arg0);
void func_actor_341700_80164B68(Task* task);
void func_actor_341700_80164E9C(Task* arg0);
void func_actor_341700_80165008(Task* arg0);
void func_actor_341700_8016583C(Task* arg0);
void func_actor_341700_80165984(Task* arg0);
void func_actor_341700_80165AF0(Task* arg0);
void func_actor_341700_80165C70(Task* arg0);
void func_actor_341700_80166114(Task* arg0);
void func_actor_341700_801663F0(Task* arg0);
void func_actor_341700_80166568(Task* arg0);
void func_actor_341700_8016688C(Task* arg0);
void func_actor_341700_801669F8(Task* arg0);
void func_actor_341700_80166B94(Task* arg0);
void func_actor_341700_80166D2C(Task* arg0);
void func_actor_341700_80166E90(Task* arg0);
void func_actor_341700_80167744(Task* arg0);
void func_actor_341700_80168004(void);
s16  func_actor_341700_80168178(Task* arg0);
void func_actor_341700_8016833C(Task* task, s16 part, VECTOR3* pos);
s16  func_actor_341700_80168468(Task* arg0);
void func_actor_341700_801685F0(Task* arg0, s32 step);
void func_actor_341700_80168A48(Task* arg0);
void func_actor_341700_80168B40(Task* arg0);
void func_actor_341700_80168BE0(Task* arg0);
void func_actor_341700_80168D3C(Task* arg0);
void func_actor_341700_80168DA0(Task* arg0);
void func_actor_341700_80168EA0(Task* arg0);
void func_actor_341700_80169018(Task* arg0);
void func_actor_341700_80169254(Task* arg0);
void func_actor_341700_801695A0(Task* task);
void func_actor_341700_80169DBC(Task* arg0);
void func_actor_341700_80169E20(Task* arg0);
void func_actor_341700_80169EE4(Task* arg0);
void func_actor_341700_80169F38(Task* arg0);
void func_actor_341700_80169FB0(Task* arg0);
void func_actor_341700_8016A08C(Task* arg0);
void func_actor_341700_8016A130(Task* arg0);
void func_actor_341700_8016A1A8(Task* arg0);
void func_actor_341700_8016A21C(Task* arg0);
void func_actor_341700_8016A890(Task* arg0);
void func_actor_341700_8016A98C(Task* task);
void func_actor_341700_8016AAB4(Task* arg0);
s32  func_actor_341700_8016AC0C(Task* arg0);

#endif
