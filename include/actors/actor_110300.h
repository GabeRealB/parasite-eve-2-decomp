#ifndef ACTOR_110300_H
#define ACTOR_110300_H

#include "common.h"
#include "gameplay/1BC.h"

/// Per-actor work block for the `actor_110300` overlay, reached through the
/// global `ActorsShared80131f9c` publishes.
///
/// Only the prefix the overlay's matched functions need is reconstructed so
/// far. `anim` sits at offset 0 here - unlike `Actor143900Work` and
/// `Actor151000Work`, where the same 0x14-byte context follows a 0x40-byte
/// state prefix - so `&ActorsShared80131f9cWork->anim` compiles to the bare
/// pointer load. The block continues with `GpAnimSlot slots[0x14]` at 0x14
/// (0x28 apart, as `Gp_AnimResetSlot` is handed `work + i * 0x28`) and the
/// animation-id pair at 0x476/0x478, 0x40 below the fields of the same name
/// in those two structs.
typedef struct Actor110300Work {
    /* 0x000 */ GpAnimCtx anim;
} Actor110300Work;

extern Actor110300Work* ActorsShared80131f9cWork;

void func_actor_110300_80132138(void);

#endif
