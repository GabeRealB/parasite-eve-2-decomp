#ifndef ACTOR_341900_H
#define ACTOR_341900_H

#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include <psyq/libgte.h>

/// Position and Euler rotation payload sent to slot 3 as message 0x3E9.
typedef struct Actor341900MsgPos {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor341900MsgPos;
STATIC_ASSERT_SIZEOF(Actor341900MsgPos, 0x18);

/// Work block of the overlay's sequence/event task -- the one
/// `D_actor_341900_80164208` points at.
///
/// `func_actor_341900_80162EFC` allocates it with `memCalloc(0x70, 0)`,
/// `Mem_Set`s the same 0x70 bytes over it and stores it in its own task's
/// `Task::work` slot (0x1C), which is not a `TaskIdMap` here, then publishes
/// that task in `D_actor_341900_80164208`. Every helper in unit
/// `actor_341900_3` reaches the block that way,
/// `(Actor341900Work*)D_actor_341900_80164208->work`; the two dispatchers
/// `func_actor_341900_801628B8` / `func_actor_341900_80162AD4` in the base unit
/// are handed the same task as their argument and index it identically.
///
/// `field_0` is the `gameGetPtrSlot(3)` task the overlay's messages are aimed
/// at (0x3E8 and 0x3F3), and `field_8` / `field_C` / `field_10` are child tasks
/// the senders null-check first (0x7D5 goes to `field_8`);
/// `func_actor_341900_80163488` disposes of `field_8` by killing it and clearing
/// the slot, and `func_actor_341900_801634D0` does the same for `field_C` and
/// `field_10` in turn.
///
/// `field_5C` and `field_64` are one-shot request states: a dispatcher switches
/// on the state through a jump table and clears it back to 0 on the way out, so
/// writing it runs that state once. `func_actor_341900_80163564` requests state
/// `field_5C`, `func_actor_341900_80163584` state `field_64`, and each also
/// resets the halfword beside it -- `field_5E` / `field_66` -- the step within
/// the state, which the dispatcher compares against 0 and 1 and increments.
/// `field_68` is cleared as that step advances, and `field_6C` is a 0/1 latch
/// shared by `func_actor_341900_801633F8` (sets it, then calls
/// `Gp_KillPlayerEffs`) and `func_actor_341900_80163438` (calls
/// `Gp_SpawnWeaponEff` while it is set, then clears it).
typedef struct Actor341900Work {
    /* 0x00 */ Task*             field_0; // gameGetPtrSlot(3)
    /* 0x04 */ Task*             field_4; // Gp_FindWorkById(session slot)->field_0
    /* 0x08 */ Task*             field_8;
    /* 0x0C */ Task*             field_C;
    /* 0x10 */ Task*             field_10;
    /* 0x14 */ Actor341900MsgPos field_14;
    /* 0x2C */ Actor341900MsgPos field_2C;
    /* 0x44 */ Actor341900MsgPos field_44;
    /* 0x5C */ s16               field_5C;
    /* 0x5E */ s16               field_5E;
    /* 0x60 */ byte              pad_60[0x4];
    /* 0x64 */ s16               field_64;
    /* 0x66 */ s16               field_66;
    /* 0x68 */ s16               field_68;
    /* 0x6A */ byte              pad_6A[0x2];
    /* 0x6C */ u16               field_6C;
    /* 0x6E */ byte              pad_6E[0x2];
} Actor341900Work;
STATIC_ASSERT_SIZEOF(Actor341900Work, 0x70);

/// Session id payload `func_actor_341900_80162EFC` sends to slot 4 as message
/// 0x7DA, asking for the 0x7DB reply. `field_0` takes `GameSession.at4.loc.stage`
/// and `field_1` takes `field_6`; the pair spells the id `Gp_FindWorkById`
/// matches on (`field_6 | field_7 << 8`), which the same function uses to find
/// the session's work object. `field_2` is zeroed.
typedef struct Actor341900Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor341900Msg7DA;
STATIC_ASSERT_SIZEOF(Actor341900Msg7DA, 0x4);

/// Work block allocated by `func_actor_341900_80162200` (`Mem_Malloc(0x44, 0)`)
/// and parked in that task's `Task::work` slot, which is not a `TaskIdMap`
/// here. The two matrices are the light/colour pair the function republishes
/// onto `TmdObject::lightMtx` / `field_20` -- the pair `Gp_BindDefaultMtx`
/// otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2` -- and `field_40` is
/// the `Task::spawnArg2` spawner, which the same function reparents to the
/// actor. `Actor503500ColorMtx` opens with this same 0x40 bytes and runs longer.
typedef struct Actor341900ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ Task*  field_40;
} Actor341900ColorMtx;
STATIC_ASSERT_SIZEOF(Actor341900ColorMtx, 0x44);

/// Controller task of this overlay, published by `func_actor_341900_80162EFC`
/// and read by the sequence helpers that hang their work off its `Task::work`.
extern Task* D_actor_341900_80164208;

/// 8-byte record of `D_actor_341900_80163A98`, indexed by `Task::spawnArg1`.
/// `func_actor_341900_801625B4` copies the first three halves onto part 0's
/// `GsCOORDINATE2::coord.t` and hangs that part off entry `field_6` of the
/// spawner model's own coordinate array, so a record is a spawn offset plus the
/// bone the actor is attached to. The first three records are all zero and only
/// `field_6` is under 9 in the rest, which is what sizes a model's part array.
typedef struct Actor341900SpawnPos {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor341900SpawnPos;
STATIC_ASSERT_SIZEOF(Actor341900SpawnPos, 0x8);

extern Actor341900SpawnPos D_actor_341900_80163A98[6];

/// Work block `func_actor_341900_80162330` allocates with `Mem_Malloc(0x258, 0)`
/// and parks in its own task's `Task::work` slot, which is a `TaskIdMap*` only
/// by type. `field_248` is the task that spawned this actor, copied there from
/// `Task::spawnArg2`; `func_actor_341900_801625B4` walks it to the spawner's
/// model to inherit its spawn position and its colour flag.
///
/// `field_66` is the animation frame, masked to 10 bits, and
/// `func_actor_341900_80162708` acts on two of its values: at 0x12 and 0x18 it
/// reparents the actor to a freshly spawned script and clears its message
/// state, recording each in `field_230` so a frame fires once rather than
/// every tick it is current. That whole check runs behind `field_254`, which
/// is matched against `Task::state` and so gates it to the one state the
/// actor's dispatcher handles it in. `field_24C` and `field_250` are the
/// actor's second and third child tasks, refreshed every tick alongside the
/// model.
typedef struct Actor341900TaskWork {
    /* 0x000 */ byte  pad_0[0x66];
    /* 0x066 */ u16   field_66;
    /* 0x068 */ byte  pad_68[0x1C8];
    /* 0x230 */ s32   field_230;
    /* 0x234 */ byte  pad_234[0x14];
    /* 0x248 */ Task* field_248;
    /* 0x24C */ Task* field_24C;
    /* 0x250 */ Task* field_250;
    /* 0x254 */ u16   field_254;
    /* 0x256 */ byte  pad_256[0x2];
} Actor341900TaskWork;
STATIC_ASSERT_SIZEOF(Actor341900TaskWork, 0x258);

/// The same 0x258-byte block as `Actor341900TaskWork`, seen from
/// `func_actor_341900_80162330`, which fills it: an animation context over
/// eight slots (`func_800B3F84` gets `pad_154` as its scratch area) and the
/// light/colour matrix pair the model draws with.
typedef struct Actor341900AnimWork {
    /* 0x000 */ GpAnimCtx  ctx;
    /* 0x014 */ GpAnimSlot slots[8];
    /* 0x154 */ byte       pad_154[0x80];
    /* 0x1D4 */ MATRIX     light;
    /* 0x1F4 */ MATRIX     color;
    /* 0x214 */ s32        field_214;
    /* 0x218 */ s32        field_218;
    /* 0x21C */ s32        field_21C;
    /* 0x220 */ s32        field_220;
    /* 0x224 */ s32        field_224;
    /* 0x228 */ byte       pad_228[0x20];
    /* 0x248 */ Task*      field_248;
    /* 0x24C */ Task*      field_24C;
    /* 0x250 */ Task*      field_250;
    /* 0x254 */ u16        field_254;
    /* 0x256 */ byte       pad_256[0x2];
} Actor341900AnimWork;
STATIC_ASSERT_SIZEOF(Actor341900AnimWork, 0x258);

/// Animation command `func_actor_341900_80161FD0` copies into
/// `Actor341900AnimWork::field_214..field_224`: `field_4` is the animation id
/// and the low half of `field_C` the blend handed to `func_800B4114` (0 resets
/// the slots instead).
typedef struct Actor341900AnimCmd {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 pad_6;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor341900AnimCmd;
STATIC_ASSERT_SIZEOF(Actor341900AnimCmd, 0x14);

void func_actor_341900_80161FD0(Task* arg0, s32 arg1, Actor341900AnimCmd* cmd);

void func_actor_341900_80162330(Task* arg0);

s32 func_actor_341900_80161E58(Task* arg0, u16 arg1);

void func_actor_341900_801628B8(Task* arg0);

void func_actor_341900_80162AD4(Task* arg0);

#endif
