#ifndef ACTOR_535700_H
#define ACTOR_535700_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Per-actor work block for the `actor_535700` overlay.
///
/// The overlay's state-0 handler (`ActorsShared80131f9cSub0`, here at
/// 0x80131FA0) allocates it with `Mem_Calloc(0x4C0, 0)` and stores it in
/// `ActorsShared80131f9cWork`, so the size below is the allocation and not a
/// guess. Every other function in the overlay reaches the block through that
/// global. The block belongs to the `ActorsShared80131f9c` family, so its
/// leading animation context sits where the family's other carriers have it
/// (see `include/actors/actor_151000.h` for the same 0x4C0 layout).
///
/// `state` is what this overlay's runner `func_actor_535700_80132108`
/// dispatches on: 1 runs the animation through `func_actor_535700_80132730`,
/// 2 through `ActorsShared80132610`. `animId` is the clip the reseed is handed
/// and `field_47E` records it once a walk has re-aimed every slot.
typedef struct Actor535700Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       pad_34C[0x130];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ u16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x3C];
} Actor535700Work;
STATIC_ASSERT_SIZEOF(Actor535700Work, 0x4C0);

extern Actor535700Work* ActorsShared80131f9cWork;

/// Fade countdown at 0x80146840, the word just below the work pointer.
/// `func_actor_535700_80131EF0` seeds it from its argument and spawns the fade
/// task from `D_actor_535700_8013346C`; that task (0x80131E2C) draws a
/// full-screen black `TILE` into ordering table slot 0xA while the count is
/// non-zero, kills itself once it reaches zero, and decrements the count every
/// frame.
///
/// The task body reads the count from a live-in `$v0`: GCC hoists its `lui` /
/// `lw` above `addiu $sp`, so splat cuts the function at the prologue and the
/// load lands in the overlay's leading rodata. See "Splat cuts the first
/// function after a hoisted `Game_Session` load" in `DECOMPILATION_LEARNINGS.md`.
extern s32 D_actor_535700_80146840;

/// Reset argument the "play animation" opcode above leaves behind: the
/// state-1 handler `func_actor_535700_80132730` reads it and the runner
/// rewrites it on the way past. The counterpart of `D_actor_451100_8013F700`.
extern s16 D_actor_535700_8013DAA8;

/// Payload of the overlay's "play animation" script opcode: `field_4` is the
/// animation id, `field_8` picks the state the runner is left in -- non-zero
/// for 1, which runs the clip through `func_actor_535700_80132730` using
/// `field_C` as the reset argument, zero for 2, the shared
/// `ActorsShared80132610` reseed -- and `field_0` is not read here. Same shape
/// as `Actor451100AnimArgs` and `Actor461800AnimPreset`, which the twin
/// opcodes of this one take.
typedef struct Actor535700AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor535700AnimPreset;

/// Drives the actor for one frame off the state the opcode below left in the
/// work block.
void func_actor_535700_80132108(Task* task);

/// Script opcode 0x7D3: applies the animation preset to the work block --
/// the id is copied in, the reset mode is picked by the preset's flag and the
/// reset argument is either taken from the preset or left at 2 -- then
/// restarts the animation through `func_actor_535700_80132108`. Only ids up to
/// 0x22 are accepted; anything else leaves the work block untouched and
/// reports the failure.
s32 func_actor_535700_801327BC(Task* task, s32 arg1, Actor535700AnimPreset* preset, s32 arg3);

#endif
