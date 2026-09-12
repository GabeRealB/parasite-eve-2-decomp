#ifndef ACTOR_405800_H
#define ACTOR_405800_H

#include "common.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include <psyq/libgte.h>

/// Packed halfwords `func_actor_405800_80138514` reads as the desired root
/// translation. Only `x` and `z` are used; the middle halfword is kept so the
/// layout matches `Actor400600ViewPos`.
typedef struct Actor405800ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor405800ViewPos;
STATIC_ASSERT_SIZEOF(Actor405800ViewPos, 0x6);

/// Per-actor work block for the `actor_405800` overlay.
///
/// `func_actor_405800_801334B8` allocates it with `Mem_Calloc(0x89C, 0)` and
/// stores the result straight into the `Task::idMap` slot (0x1C), so the size
/// below is the allocation and not a guess: this actor reuses that pointer
/// field for its own work block and it is *not* a `TaskIdMap` here. It is the
/// only allocator in the overlay, so every function reaches the same block
/// with `(Actor405800Work*)task->idMap`.
///
/// `field_846` / `field_848` are the state and sub-state indices the handler
/// table walks and `field_842` is the per-state frame counter.
typedef struct Actor405800Work {
    /* 0x000 */ MATRIX     matrix_0; // copy of the root coordinate's local matrix
    /* 0x020 */ byte       pad_20[0x64];
    /* 0x084 */ u16        field_84; // roll, see ActorsShared80139948
    /* 0x086 */ byte       pad_86[0xC];
    /* 0x092 */ u16        field_92; // copied into field_86A on state entry
    /* 0x094 */ byte       pad_94[0x4];
    /* 0x098 */ u16        field_98; // low half of the root coordinate's world X
    /* 0x09A */ s16        field_9A;
    /* 0x09C */ u16        field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte       pad_9E[0xA];
    /* 0x0A8 */ SVECTOR    field_A8; // world point `ActorsShared80139c00` turns to face (it reads `vx` / `vz`)
    /* 0x0B0 */ GpAnimCtx  anim;     // slots 1..0x11 reset by func_actor_405800_80138224
    /* 0x0C4 */ GpAnimSlot slots[0x12];
    /* 0x394 */ byte       pad_394[0x120];
    /* 0x4B4 */ GpObj      obj_4B4;   // collision node; unlinked on death
    /* 0x4D4 */ byte       pad_4D4[0xC0];
    /* 0x594 */ GpObj      obj_594;   // collision node; unlinked on death
    /* 0x5B4 */ byte       pad_5B4[0xC0];
    /* 0x674 */ GpObj      obj_674;   // collision node; unlinked on death
    /* 0x694 */ GpObj      obj_694;   // collision node; unlinked on death
    /* 0x6B4 */ GpObj      obj_6B4;   // collision node; unlinked on death
    /* 0x6D4 */ GpObj      obj_6D4;   // collision node; unlinked on death
    /* 0x6F4 */ byte       pad_6F4[0x30];
    /* 0x724 */ GpObj      obj_724;   // collision node; flags bit 0x4000 cleared by func_actor_405800_801379F8
    /* 0x744 */ byte       pad_744[0xE0];
    /* 0x824 */ Task*      field_824; // child task, killed on state exit
    /* 0x828 */ Task*      field_828; // child task, killed on state exit
    /* 0x82C */ byte       pad_82C[0x6];
    /* 0x832 */ s16        field_832;
    /* 0x834 */ s16        field_834;
    /* 0x836 */ s16        field_836;
    /* 0x838 */ s16        field_838;
    /* 0x83A */ byte       pad_83A[0x4];
    /* 0x83E */ s16        field_83E; // reset to 0x1000 on death
    /* 0x840 */ byte       pad_840[0x2];
    /* 0x842 */ u16        field_842; // per-state frame counter
    /* 0x844 */ byte       pad_844[0x2];
    /* 0x846 */ u16        field_846; // state index
    /* 0x848 */ u16        field_848; // sub-state index
    /* 0x84A */ s16        field_84A;
    /* 0x84C */ s16        field_84C;
    /* 0x84E */ s16        field_84E;
    /* 0x850 */ s16        field_850; // animation speed / step scale
    /* 0x852 */ s16        field_852; // compared against 2000 to pick state 8 vs 0xD
    /* 0x854 */ byte       pad_854[0x8];
    /* 0x85C */ s16        field_85C; // countdown, ticked by func_actor_405800_8013795C
    /* 0x85E */ s16        field_85E; // countdown, ticked by func_actor_405800_8013795C
    /* 0x860 */ byte       pad_860[0x6];
    /* 0x866 */ s16        field_866;
    /* 0x868 */ byte       pad_868[0x2];
    /* 0x86A */ u16        field_86A; // seeded from field_92
    /* 0x86C */ byte       pad_86C[0x2];
    /* 0x86E */ s16        field_86E; // animation request kind
    /* 0x870 */ s16        field_870; // animation id now playing
    /* 0x872 */ s16        field_872; // animation id
    /* 0x874 */ byte       pad_874[0x8];
    /* 0x87C */ u16        field_87C; // down-counter
    /* 0x87E */ byte       pad_87E[0x4];
    /* 0x882 */ s16        field_882; // randomised hold, 0x5A .. 0x99 frames
    /* 0x884 */ byte       pad_884[0x7];
    /* 0x88B */ s8         field_88B;
    /* 0x88C */ u8         field_88C;
    /* 0x88D */ s8         field_88D;
    /* 0x88E */ s8         field_88E;
    /* 0x88F */ s8         field_88F;
    /* 0x890 */ u8         field_890; // nonzero: allow the state-0xD transition when root X > 10000
    /* 0x891 */ u8         field_891;
    /* 0x892 */ byte       pad_892[0xA];
} Actor405800Work;
STATIC_ASSERT_SIZEOF(Actor405800Work, 0x89C);

#endif // ACTOR_405800_H
