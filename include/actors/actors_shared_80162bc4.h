#ifndef ACTORS_SHARED_80162BC4_H
#define ACTORS_SHARED_80162BC4_H

#include "common.h"

#include "main/task.h"

/// The one field this handler touches in the carrier's work block, the
/// `Task::idMap` slot each carrier parks its per-instance state in. The pad
/// stands in for the rest of the block, which `Actor317000Work` and
/// `Actor113000Work` name differently; both spell this halfword `field_4C8`,
/// `s16`, and both seed it to -1 out of their spawn handler.
typedef struct ActorsShared80162bc4Work {
    /* 0x000 */ byte pad_0[0x4C8];
    /* 0x4C8 */ s16  field_4C8;
} ActorsShared80162bc4Work;

/// Display handler for the `GpMsgEntry` table each carrier installs -- message
/// 0x7D5 in `actor_317000`, byte-identical in `actor_113000`. `mode` drives
/// `TmdObject::field_C`: bit 0x80 marks the actor hidden and bit 0x4 the
/// display buffers being live.
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, latch the frame countdown `field_4C8` that ends in
///           `Tmd_FreeBuffers`, raise 0x4
///   mode 3  show, raise 0x4
///
/// Any other mode returns 1; the four known ones return 0. `arg3` is unused --
/// the dispatch passes four arguments, and the fourth is the mode again.
s32 ActorsShared80162bc4(Task* task, s32 arg1, s32 mode, s32 arg3);

#endif
