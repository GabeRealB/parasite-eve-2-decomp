#ifndef ACTOR_113100_H
#define ACTOR_113100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"

/// Work block of the `actor_113100` enemy task. `func_actor_113100_80131E58`
/// (state 0) `Mem_Calloc`s 0x540 bytes and parks the pointer in the task's
/// `Task::work` slot -- that slot is not a `TaskIdMap` here. The same
/// function builds the display node at +0x4B8: it points `GpObj::field_C` at
/// the record that follows the node (block + 0x4D8), sets `field_18` to
/// 0x30000 and `field_1C` to 0x100, zeroes the position triple and sets
/// `flags` to 1. The exit callback `func_actor_113100_80132EF0` hands that
/// node back to `Gp_UnlinkObj`.
/// `field_53E` latches the `GameFlag_GetNibble(0xED)` result that
/// `func_actor_113100_80132F40` uses, so the setup it triggers runs only on
/// the edge where the flag turns positive and the latch is still clear.
/// `field_534` is a `Task_SpawnFromTable` result this same setup stores, and
/// `field_53C` is the mode byte the 0x7DB handler writes and
/// `func_actor_113100_80132104` switches on.
/// `field_52A` is the heading the turn-to-face body `func_actor_113100_801330E8`
/// steps the root coordinate's yaw toward, 0x40 a frame, snapping to it once
/// within 0x41, and `field_477` is the animation id that body puts in the
/// preset it sends when it snaps. Both reset `field_530` / `field_532` there.
/// The block is fronted by a `GpAnimCtx` -- `func_actor_113100_801331E8` and
/// `func_actor_113100_801328EC` pass the block itself, its `slots` array and
/// its `GpAnimMtxRec` table at 0x334 to `func_800B3F84`, the same three
/// addresses `Actor503500WorkBoss` hands over. Twenty 0x28-byte slots fit
/// exactly between 0x14 and 0x334, and both tick loops walk indices 1..0x13.
typedef struct Actor113100Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[20];
    /// The matrix table `func_800B3F84` fills, immediately after the slots.
    /// `field_474` is the latch the 0x7D3 handler raises once the slots have
    /// been started; `func_actor_113100_801328EC` seeds it the same way.
    /* 0x334 */ byte field_334[0x140];
    /* 0x474 */ s8   field_474;
    /// Cleared to -1 by the setup handler, next to `field_476` and `field_53D`;
    /// `field_477` is the animation id that body puts in the preset it sends,
    /// so the three sentinels are the same "no id yet" default.
    /* 0x475 */ s8    field_475;
    /* 0x476 */ s8    field_476;
    /* 0x477 */ s8    field_477;
    /* 0x478 */ byte  pad_478[0x40];
    /* 0x4B8 */ GpObj obj;
    /// The `GpRec18` table `GpObj::field_C` points at (`Gp_InitRec18Table` is
    /// called on it right after `Gp_LinkObj`).
    /* 0x4D8 */ GpRec18 field_4D8;
    /* 0x4F0 */ s32     field_4F0;
    /* 0x4F4 */ s32     field_4F4;
    /* 0x4F8 */ s32     field_4F8;
    /* 0x4FC */ byte    pad_4FC[0x4];
    /// Local-space offset `func_actor_113100_801324DC` hands `ApplyMatrixLV`
    /// as its source and destination: it rotates the constant (0, 0, 0x200000)
    /// through the root coordinate and writes the world-space result here.
    /* 0x500 */ VECTOR field_500;
    /// Zeroed by the setup handler next to `field_514` / `field_518`.
    /* 0x510 */ s32  field_510;
    /* 0x514 */ s32  field_514;
    /* 0x518 */ s32  field_518;
    /* 0x51C */ byte pad_51C[0x4];
    /// Raised to 0x7FFF on all three halves by `func_actor_113100_801324DC`
    /// just before it publishes the 0x7D3 preset.
    /* 0x520 */ SVECTOR field_520;
    /// The rotation triple the 0x7DD placement handler
    /// `func_actor_113100_801328EC` latches from its payload, beside the
    /// translation it drops into `field_4F0`..`field_4F8`. `field_52A` is the
    /// yaw it holds there.
    /* 0x528 */ u16   field_528;
    /* 0x52A */ u16   field_52A;
    /* 0x52C */ u16   field_52C;
    /* 0x52E */ byte  pad_52E[0x2];
    /* 0x530 */ s16   field_530;
    /* 0x532 */ u16   field_532;
    /* 0x534 */ Task* field_534;
    /* 0x538 */ s16   field_538;
    /* 0x53A */ s16   field_53A;
    /* 0x53C */ u8    field_53C;
    /// -1 sentinel written with `field_475` / `field_476`; the 0x7D5 visibility
    /// handler `func_actor_113100_80132790` re-arms it to 2 in its hide-and-free
    /// mode. `func_actor_113100_80132104` walks it down: negative does nothing,
    /// 0 calls `Tmd_FreeBuffers`, and every non-negative value is decremented.
    /* 0x53D */ s8   field_53D;
    /* 0x53E */ s8   field_53E;
    /* 0x53F */ byte pad_53F[1];
} Actor113100Work;
STATIC_ASSERT_SIZEOF(Actor113100Work, 0x540);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor342400Msg7DB`, whose halfword at 0x2 is the
/// only part this overlay's handler reads.
typedef struct Actor113100Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor113100Msg7DB;
STATIC_ASSERT_SIZEOF(Actor113100Msg7DB, 0x4);

/// Spawn placement the 0x7DD handler `func_actor_113100_801328EC` copies into
/// the work block: the position into `field_4F0`..`field_4F8`, the rotation
/// into `field_528`..`field_52C` (the yaw being the turn-to-face target). The
/// same 0x18-byte record as `Actor141000Placement`, whose placement handler is
/// the same body for its own overlay.
typedef struct Actor113100Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor113100Placement;
STATIC_ASSERT_SIZEOF(Actor113100Placement, 0x18);

/// Optional start animation for the same handler: the preset's `field_4` and
/// the `field_477` id byte. Absent, the defaults are anim 2 and id 1.
typedef struct Actor113100SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor113100SpawnAnim;

/// The 0x7DD entry of `D_actor_113100_80144338`: the placement command. It
/// latches its payload's position and rotation into the work block, flags the
/// actor as placed (`field_530` / `field_532`), then applies the start preset
/// in place -- the body of the 0x7D3 handler `func_actor_113100_801331E8`
/// written out inline against a preset built on this function's own stack.
/// `anim` picking the preset's `field_4` is what selects the animation id.
s32 func_actor_113100_801328EC(Task* task, s32 msgId, Actor113100Placement* place, Actor113100SpawnAnim* anim);

/// Message 0x7DB handler, listed in `D_actor_113100_80144338` after the 0x7D3 /
/// 0x7D5 / 0x7DD ones. The payload halfword selects one of four actions: 0 and
/// 1 clear and raise bit 0x80 of the child task's `TmdObject::field_C`, the
/// deferred-kill flag that decides whether the model is drawn; 2 and 3 set the
/// work block's `field_53C` mode byte to 1 and 0. Nothing reads the opcode
/// itself, hence `msgId`.
s32 func_actor_113100_801333B8(Task* task, s32 msgId, Actor113100Msg7DB* msg);

/// Animation preset `func_actor_113100_8013301C` builds on its stack and hands
/// to `func_actor_113100_801331E8` as message 0x7D3. That function compares
/// `field_0` against `Actor113100Work::field_476` and, when they differ,
/// latches it and re-seeds the slot tables through `Gp_AnimResetSlot` /
/// `func_800B3F84`; `field_8` selects between that path and the plain
/// `Gp_AnimTickIndex` loop, and `field_C` is passed on as the per-slot
/// argument. The trailing `field_10` is stored but never read by the callee.
typedef struct Actor113100AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor113100AnimPreset;
STATIC_ASSERT_SIZEOF(Actor113100AnimPreset, 0x14);

/// A `MATRIX`'s word-wise view, for the identity splat
/// `func_actor_113100_801330E8` writes over the root coordinate before
/// `RotMatrix` refills the 3x3: five aligned stores rather than nine halfword
/// ones (the same shape as `Actor141000MatWords`, whose comment on the idiom is
/// the fuller one).
typedef struct Actor113100MatWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor113100MatWords;
STATIC_ASSERT_SIZEOF(Actor113100MatWords, 0x14);

/// The four main-body handlers, dispatched by `Actor113100Work::field_532`.
extern TaskFuncTable4 D_actor_113100_80131E48;

/// Child task table the setup handler `func_actor_113100_80131E58` spawns
/// from, four `TaskDesc` entries. Index 1 is spawned only when
/// `gGameSession->loc.place == 2` and its task lands in
/// `Actor113100Work::field_534`; indices 2 and 3 are the two modelled parts the
/// handler re-dresses from the area record.
extern TaskDesc D_actor_113100_80144308;

/// The actor's message table, stored in `Task::field_24`: 0x7D3
/// (`func_actor_113100_801331E8`), 0x7D4 (`ActorsShared8013231c`), 0x7D5
/// (`func_actor_113100_80132790`), 0x7DD (`func_actor_113100_801328EC`) and
/// 0x7DB (`func_actor_113100_801333B8`), terminated by 0x7FFFFFFF.
extern GpMsgEntry D_actor_113100_80144338[];

/// Animation bank table the 0x7D3 handler `func_actor_113100_801331E8` indexes
/// by the animation id it has latched into `Actor113100Work::field_476`; the
/// entry is the `void*` its `func_800B3F84` call passes on.
extern void* D_actor_113100_801442E0[];

/// Per-animation byte the same handler copies into
/// `Actor113100Work::field_53C` from `Actor113100AnimPreset::field_4`.
extern u8 D_actor_113100_801442E4[];

/// Builds the yaw rotation for `angle` (4096 = a full turn) over a `MATRIX`,
/// the Psy-Q `RotMatrix`-family helper this overlay shares with every other
/// actor family; two call sites here re-splat the 3x3 to the identity first.
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// The 0x7D3 entry of `D_actor_113100_80144338`: applies the animation preset
/// `arg2` to `arg0`'s parts. `arg1` is the message id and `arg3` an unused
/// extra the overlay's call sites pass as zero. Like the 0x7DB handler it
/// returns zero.
s32 func_actor_113100_801331E8(Task* task, s32 msgId, Actor113100AnimPreset* preset, s32 arg3);

/// The task's exit callback: it unlinks the work block's display node and
/// destroys the task.
void func_actor_113100_80132EF0(Task* task);

/// The 0x7D5 entry of `D_actor_113100_80144338`: the actor's own visibility,
/// switched on the `mode` its payload carries rather than on a pointer. Both
/// overlay call sites are plain calls that leave `msgId` at zero -- the setup
/// handler with (task, 0, 0, 0) and `func_actor_113100_80132F40` with
/// (task, 0, 1, 0) next to `func_80183BAC(0)`. Modes 0..3 are handled and
/// return 0; anything else returns 1. Its body documents what each mode does to
/// `TmdObject::field_C`, the display node and `field_53D`.
s32 func_actor_113100_80132790(Task* task, s32 msgId, s32 mode, s32 arg3);

/// Gameplay import (`actors.imports.txt`), called with 1 by the setup handler
/// and with 0 by `func_actor_113100_80132F40`.
void func_80183BAC(s32 arg0);

#endif
