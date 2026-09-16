#ifndef ACTOR_107000_H
#define ACTOR_107000_H

#include "common.h"

/// Owning context of the actor, passed as the first argument of its per-frame
/// handler next to the `Task` that carries the model in `Task::extra`.
///
/// `field_14` is the pose flag the `D_801153F4` mode switch writes together
/// with the model part's flag word: mode 0 zeroes both, mode 2 pairs a 1 here
/// with `TmdObject::field_C = 0x80` for the hidden pose.
typedef struct Actor107000Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor107000Ctx;

#endif
