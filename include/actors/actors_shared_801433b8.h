#ifndef ACTORS_SHARED_801433B8_H
#define ACTORS_SHARED_801433B8_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// One of the nine display nodes the enemy keeps in a row from 0x7F4 of its
/// work block: a `GpObj` followed by the five-entry `GpRec18` table its
/// `field_C` points at. `func_actor_444000_801423C4` hands the tables at 0x814
/// and 0x8AC to `Gp_ClearRec18Occupied`, which is what fixes the 0x98 stride.
typedef struct ActorsShared801433b8Node {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[5];
} ActorsShared801433b8Node;
STATIC_ASSERT_SIZEOF(ActorsShared801433b8Node, 0x98);

/// One of the seven sub-part blocks the enemy points at from `parts`. Only the
/// owning task at 0x0 is known: the teardown below parks `Task::state` 2 there
/// to kill the part, and the readers in `actor_444000_2` reach that task's
/// `extra` slot the same way.
typedef struct ActorsShared801433b8Part {
    /* 0x0 */ Task* task;
} ActorsShared801433b8Part;

/// The 0xF24-byte work block both overlays allocate with `memCalloc(0xF24, 0)`
/// and park in the `Task::work` slot (0x1C), which is not a `TaskIdMap` here.
/// Named after the shared body because the two overlays' own headers describe
/// the same allocation as `Actor444000Work` / `Actor403200Work`; only the
/// fields this body touches are filled in.
typedef struct ActorsShared801433b8Work {
    /* 0x000 */ byte                      pad_0[0x7F4];
    /* 0x7F4 */ ActorsShared801433b8Node  nodes[9];
    /* 0xD4C */ byte                      pad_D4C[0x180];
    /* 0xECC */ ActorsShared801433b8Part* parts[7];
    /* 0xEE8 */ byte                      pad_EE8[0x3C];
} ActorsShared801433b8Work;
STATIC_ASSERT_SIZEOF(ActorsShared801433b8Work, 0xF24);

/// Exit callback of the enemy task: kills every sub-part task, unlinks the
/// display nodes, clears the enemy's back-pointer slot and destroys it.
void ActorsShared801433b8(Task* arg0);

#endif
