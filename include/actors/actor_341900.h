#ifndef ACTOR_341900_H
#define ACTOR_341900_H

#include "common.h"

#include "main/task.h"

/// Work block of the overlay's sequence/event task -- the one
/// `D_actor_341900_80164208` points at.
///
/// `func_actor_341900_80162EFC` allocates it with `Mem_Calloc(0x70, 0)`,
/// `Mem_Set`s the same 0x70 bytes over it and stores it in its own task's
/// `Task::idMap` slot (0x1C), which is not a `TaskIdMap` here, then publishes
/// that task in `D_actor_341900_80164208`. Every helper in unit
/// `actor_341900_3` reaches the block that way,
/// `(Actor341900Work*)D_actor_341900_80164208->idMap`; the two dispatchers
/// `func_actor_341900_801628B8` / `func_actor_341900_80162AD4` in the base unit
/// are handed the same task as their argument and index it identically.
///
/// `field_0` is the `Game_GetPtrSlot(3)` task the overlay's messages are aimed
/// at (0x3E8 and 0x3F3), and `field_8` / `field_C` are child tasks the senders
/// null-check first (0x7D5 goes to `field_8`).
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
    /* 0x00 */ Task* field_0; // Game_GetPtrSlot(3)
    /* 0x04 */ byte  pad_4[0x4];
    /* 0x08 */ Task* field_8;
    /* 0x0C */ Task* field_C;
    /* 0x10 */ byte  pad_10[0x4C];
    /* 0x5C */ s16   field_5C;
    /* 0x5E */ s16   field_5E;
    /* 0x60 */ byte  pad_60[0x4];
    /* 0x64 */ s16   field_64;
    /* 0x66 */ s16   field_66;
    /* 0x68 */ s16   field_68;
    /* 0x6A */ byte  pad_6A[0x2];
    /* 0x6C */ s16   field_6C;
    /* 0x6E */ byte  pad_6E[0x2];
} Actor341900Work;
STATIC_ASSERT_SIZEOF(Actor341900Work, 0x70);

/// Controller task of this overlay, published by `func_actor_341900_80162EFC`
/// and read by the sequence helpers that hang their work off its `Task::idMap`.
extern Task* D_actor_341900_80164208;

#endif
