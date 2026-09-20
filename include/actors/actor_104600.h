#ifndef ACTOR_104600_H
#define ACTOR_104600_H

#include "common.h"

#include "main/task.h"

/// Owning context of the actor, passed as the first argument of its per-frame
/// handler next to the `Task` that carries the model in `Task::extra`.
///
/// `field_14` is the pose flag the `D_801153F4` mode switch writes together
/// with the model part's flag word: mode 0 zeroes both, mode 2 pairs a 1 here
/// with `TmdObject::flags = 0x80` for the hidden pose. `Actor107000Ctx` is
/// the same type for the sibling carrier.
typedef struct Actor104600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor104600Ctx;

/// The actor's per-instance work block, reached through `Task::work`. Only
/// the fields the decompiled state machine touches are modelled so far: the
/// state selector at 0x286, the frame counter it cycles at 0x28A, and the
/// flags it clears alongside them.
typedef struct Actor104600Work {
    /* 0x000 */ byte pad_0[0x286];
    /* 0x286 */ s16  field_286; ///< state
    /* 0x288 */ byte pad_288[2];
    /* 0x28A */ s16  field_28A; ///< frames spent in the current step
    /* 0x28C */ byte pad_28C[2];
    /* 0x28E */ s16  field_28E;
    /* 0x290 */ s16  field_290;
    /* 0x292 */ s16  field_292;
    /* 0x294 */ byte pad_294[0x12];
    /* 0x2A6 */ s16  field_2A6;
} Actor104600Work;

void func_actor_104600_80135B74(Task* task);

#endif
