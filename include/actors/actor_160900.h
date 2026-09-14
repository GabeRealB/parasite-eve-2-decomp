#ifndef ACTOR_160900_H
#define ACTOR_160900_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// 0x20-byte block `func_actor_160900_80133F90` allocates with
/// `Mem_Calloc(0x20, 0)` for each of the two child tasks it spawns from index 7
/// of `ActorsShared80136280Desc`, and parks in that child's `Task::idMap` slot
/// (0x1C) -- a third idMap block in this overlay, not a `TaskIdMap`. The size
/// below is the allocation: the function zeroes all 0x20 bytes with `Mem_Set`.
///
/// The four vectors are the corners of an axis-aligned rectangle in the Y/Z
/// plane, written as differences from the child's own origin. Child 0 (spawn
/// arg 1) takes the `+Z` side -- `(0,-0x5DC,0x3E8)`, `(0,-0x5DC,0)`,
/// `(0,0,0x3E8)`, `(0,0,0)` -- and child 1 the `-Z` side, the same rectangle
/// reflected through Z. No vector's `pad` halfword is touched, and `vx` is
/// zero in every one of them.
typedef struct Actor160900ChildWork {
    /* 0x00 */ SVECTOR field_0;
    /* 0x08 */ SVECTOR field_8;
    /* 0x10 */ SVECTOR field_10;
    /* 0x18 */ SVECTOR field_18;
} Actor160900ChildWork;
STATIC_ASSERT_SIZEOF(Actor160900ChildWork, 0x20);

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Reach it with
/// `(Actor160900Work*)task->idMap`.
///
/// `func_actor_160900_8013418C` allocates it with `Mem_Malloc(0x68, 0)` and
/// zeroes all 0x68 bytes, so the size below is the allocation. That function
/// fills `field_34` with `Game_GetPtrSlot(3)` -- the task every `Gp_DispatchMsg`
/// in this overlay targets -- and 0x38/0x3C/0x40 with the tasks it spawns from
/// `ActorsShared80136280Desc` indices 3, 5 and 6.
///
/// `field_64` indexes `D_actor_160900_8013F1CC` and `field_66` counts frames
/// against the step's `field_0`.
typedef struct Actor160900Work {
    /* 0x00 */ byte  pad_0[0xC];
    /* 0x0C */ Task* field_C[10]; // child tasks, killed on death
    /* 0x34 */ Task* field_34;    // Game_GetPtrSlot(3), Gp_DispatchMsg target
    /* 0x38 */ byte  pad_38[0x14];
    /* 0x4C */ s16   field_4C;
    /* 0x4E */ s16   field_4E;
    /* 0x50 */ byte  pad_50[4];
    /* 0x54 */ s16   field_54;
    /* 0x56 */ s16   field_56;
    /* 0x58 */ byte  pad_58[4];
    /* 0x5C */ s16   field_5C;
    /* 0x5E */ s16   field_5E;
    /* 0x60 */ byte  pad_60[4];
    /* 0x64 */ u16   field_64;
    /* 0x66 */ u16   field_66;
} Actor160900Work;
STATIC_ASSERT_SIZEOF(Actor160900Work, 0x68);

/// One step of the animation script `D_actor_160900_8013F1CC`
/// `func_actor_160900_801326EC` walks: `field_0` is how many frames to hold the
/// step (`field_66` counts them up) and `field_2` the index of the next step,
/// sent to the player as message 0x3F4's animation id; a negative `field_2`
/// ends the script. Every `field_0` in the table is zero, so the hold is not
/// what paces the shipped script.
typedef struct Actor160900AnimStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} Actor160900AnimStep;
STATIC_ASSERT_SIZEOF(Actor160900AnimStep, 0x4);

/// 8-byte fade block `func_actor_160900_801343E4` allocates with
/// `Mem_Malloc(8, 0)` and parks in `Task::idMap` -- a second, smaller idMap
/// block in this overlay, distinct from `Actor160900Work` and owned by the
/// fade task that function drives.
///
/// The three halfwords are the RGB channels `Fade_DrawOverlay` draws: the task
/// raises all three by `(u16)Task::spawnArg1` each frame and clamps all three
/// to 0xFF once the red channel passes 0x100. `pad_0` is never touched.
typedef struct Actor160900FadeWork {
    /* 0x0 */ u8  pad_0[2];
    /* 0x2 */ u16 r;
    /* 0x4 */ u16 g;
    /* 0x6 */ u16 b;
} Actor160900FadeWork;
STATIC_ASSERT_SIZEOF(Actor160900FadeWork, 0x8);

extern Task* D_actor_160900_8013FBB4;

/// Animation script `func_actor_160900_801326EC` walks and the animation-set
/// table it hands the player task as message 0x3F4's `field_0`.
extern Actor160900AnimStep D_actor_160900_8013F1CC[];
extern u8                  D_actor_160900_8013F198[];

void func_actor_160900_80134710(void);

void func_actor_160900_80134790(s16 arg0);

void func_actor_160900_801347B0(s16 arg0);

void func_actor_160900_801347D0(s16 arg0);

void func_actor_160900_801347F0(void);

#endif
