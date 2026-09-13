#ifndef ACTOR_403200_H
#define ACTOR_403200_H

#include "common.h"
#include "main/task.h"

typedef struct Actor403200Obj Actor403200Obj;

/// Per-actor state block for the `actor_403200` overlay.
///
/// `func_actor_403200_80138AFC` allocates it with `Mem_Calloc(0xF24, 0)` and
/// stores the result in the `Task::idMap` slot (0x1C), which this enemy actor
/// reuses for its own work block, so it is *not* a `TaskIdMap` here. Reach it
/// with `(Actor403200Work*)task->idMap`. The size below is the allocation, not
/// a guess.
typedef struct Actor403200Work {
    /// State index. `func_actor_403200_8013FB54` indexes the local copy of
    /// `D_actor_403200_80132154` with it, and whenever it differs from
    /// `field_2` it flags the change in `field_4` and re-arms the `field_6`
    /// counter. The same three fields and that same test appear in the sibling
    /// actor overlays that share this dispatcher.
    /* 0x000 */ s16 field_0;
    /// The state index `func_actor_403200_8013FB54` ran on the previous tick,
    /// so it can spot the change.
    /* 0x002 */ s16 field_2;
    /// Set on the tick the dispatcher sees a state change, cleared on every
    /// other tick. `func_actor_403200_8014123C` reads it to re-arm `field_6`
    /// once more, which the dispatcher has already done.
    /* 0x004 */ s16 field_4;
    /// Per-state counter: cleared on a state change, otherwise incremented
    /// (saturating at 0x7FFF). State handlers fire one-shot cues on the ticks
    /// it reaches a given value.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0xEAC - 0x8];
    /* 0xEAC */ s8   field_EAC;
    /* 0xEAD */ byte pad_EAD[0x77];
} Actor403200Work;
STATIC_ASSERT_SIZEOF(Actor403200Work, 0xF24);

s16 func_actor_403200_801344C4(Actor403200Obj* arg0, s16 arg1);

/// The actor's per-frame body: runs the animation resets and the collision /
/// damage ticks. Takes the task, and reaches the work block through its
/// `idMap` slot, as `func_actor_403200_8014123C` does.
void func_actor_403200_80133DD8(Task* task);

#endif
