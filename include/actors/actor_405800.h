#ifndef ACTOR_405800_H
#define ACTOR_405800_H

#include "common.h"
#include "main/task.h"
#include <psyq/libgte.h>

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
    /* 0x000 */ byte    pad_0[0x9A];
    /* 0x09A */ s16     field_9A;
    /* 0x09C */ byte    pad_9C[0xC];
    /* 0x0A8 */ SVECTOR field_A8;  // world point `func_actor_405800_801383CC` turns to face (it reads `vx` / `vz`)
    /* 0x0B0 */ byte    pad_B0[0x692];
    /* 0x742 */ u16     field_742; // flag word; bit 0x4000 cleared by func_actor_405800_801379F8
    /* 0x744 */ byte    pad_744[0xE0];
    /* 0x824 */ Task*   field_824; // child task, killed on state exit
    /* 0x828 */ Task*   field_828; // child task, killed on state exit
    /* 0x82C */ byte    pad_82C[0x6];
    /* 0x832 */ s16     field_832;
    /* 0x834 */ s16     field_834;
    /* 0x836 */ s16     field_836;
    /* 0x838 */ s16     field_838;
    /* 0x83A */ byte    pad_83A[0x8];
    /* 0x842 */ u16     field_842; // per-state frame counter
    /* 0x844 */ byte    pad_844[0x2];
    /* 0x846 */ u16     field_846; // state index
    /* 0x848 */ u16     field_848; // sub-state index
    /* 0x84A */ s16     field_84A;
    /* 0x84C */ byte    pad_84C[0x4];
    /* 0x850 */ s16     field_850; // animation speed / step scale
    /* 0x852 */ byte    pad_852[0xA];
    /* 0x85C */ s16     field_85C; // countdown, ticked by func_actor_405800_8013795C
    /* 0x85E */ s16     field_85E; // countdown, ticked by func_actor_405800_8013795C
    /* 0x860 */ byte    pad_860[0x6];
    /* 0x866 */ s16     field_866;
    /* 0x868 */ byte    pad_868[0x6];
    /* 0x86E */ s16     field_86E; // animation request kind
    /* 0x870 */ byte    pad_870[0x2];
    /* 0x872 */ s16     field_872; // animation id
    /* 0x874 */ byte    pad_874[0x8];
    /* 0x87C */ u16     field_87C; // down-counter
    /* 0x87E */ byte    pad_87E[0xD];
    /* 0x88B */ s8      field_88B;
    /* 0x88C */ byte    pad_88C[0x1];
    /* 0x88D */ s8      field_88D;
    /* 0x88E */ s8      field_88E;
    /* 0x88F */ byte    pad_88F[0xD];
} Actor405800Work;
STATIC_ASSERT_SIZEOF(Actor405800Work, 0x89C);

#endif // ACTOR_405800_H
