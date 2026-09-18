#ifndef ACTOR_450900_H
#define ACTOR_450900_H

#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// Head-aim record `func_actor_450900_80132548` allocates and parks in
/// `Task::work`, handed straight to `func_800B17D4` as its `arg2`: the yaw and
/// pitch clamps that function widens against the head's current pose, and the
/// `rate` fraction of the remaining angle the save-point capture task ramps one
/// 0x200 step per frame.
///
/// The field roles are `GpHeadAim`'s, but the two readings of the record are
/// not the same size. This overlay allocates 12 bytes where `GpHeadAim` is 10,
/// matching the other `func_800B17D4` callers that build the record the same
/// way -- `Actor361100HeadAim`, `func_mine_mesa_8017E15C` and
/// `func_actor_450200_80131FA8` -- so 12 is the record's size and gameplay's 10
/// is the most `func_800B17D4` alone can see of it.
///
/// `rate` is `u16` here because this body reads it as an unsigned halfword and
/// reinterprets the stored value as `s16` for the clamp, which is what the
/// `lhu` / `sll` / `sra` sequence in the ROM says. That is a statement about the
/// access, not about the field: declaring it `s16` here compiles to the same
/// bytes, so the ROM cannot distinguish the two at this site, and the field
/// never leaves [0, 0x1000], where both readings agree. See the
/// `DECOMPILATION_LEARNINGS.md` entries on `lhu` and halfword signedness.
typedef struct Actor450900HeadAim {
    /* 0x0 */ s16  yawLimit;
    /* 0x2 */ s16  pitchLimit;
    /* 0x4 */ u16  rate;
    /* 0x6 */ s16  lastPitch;
    /* 0x8 */ s8   inited;
    /* 0x9 */ byte pad_9[0x3];
} Actor450900HeadAim;
STATIC_ASSERT_SIZEOF(Actor450900HeadAim, 0xC);

#endif // ACTOR_450900_H
