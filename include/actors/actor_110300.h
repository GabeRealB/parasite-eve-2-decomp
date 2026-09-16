#ifndef ACTOR_110300_H
#define ACTOR_110300_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

/// Per-actor work block for the `actor_110300` overlay, reached through the
/// global `ActorsShared80131f9c` publishes.
///
/// `anim` sits at offset 0 here - unlike `Actor143900Work` and
/// `Actor151000Work`, where the same 0x14-byte context follows a 0x40-byte
/// state prefix - so `&ActorsShared80131f9cWork->anim` compiles to the bare
/// pointer load. `slots` follows it directly, and the animation-id pair sits
/// at 0x476/0x478, 0x40 below the fields of the same name in
/// `ActorsShared80132538Work` - `field_4B6`/`field_4B8` there, the same ones
/// `Actor460200Work` calls `field_4B6`/`animId`.
typedef struct Actor110300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       pad_334[0x140];
    /* 0x474 */ s16        field_474; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x476 */ s16        field_476; // copy of `animId`, kept for change detection
    /* 0x478 */ u16        animId;    // animation id the slots are seeded with
    /* 0x47A */ s16        field_47A;
} Actor110300Work;
STATIC_ASSERT_SIZEOF(Actor110300Work, 0x47C);

/// Argument block of the message 0x7D3 handler `func_actor_110300_80132280`
/// implements: which animation to start. Same 4-byte-id prefix as
/// `Actor460200AnimArgs`, and the same `(u16)` narrowing on the store into the
/// work block's `animId`.
typedef struct Actor110300AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor110300AnimArgs;

extern Actor110300Work* ActorsShared80131f9cWork;

/// The actor itself, beside the work block above: `func_actor_110300_80131FF8`
/// takes its coordinate from `->extra`, and the step dispatcher is handed it.
extern GpActorWork* D_actor_110300_8013A0A4;

/// The task this actor was paired with when it spawned. The self-kill path
/// `func_actor_110300_80132088` takes it down, and the visibility control
/// `func_actor_110300_801322E0` drives its model alongside this actor's own -
/// the role `ActorsShared80132710Work::field_4B8` plays in the shared variant
/// of that body.
extern Task* D_actor_110300_8013A0A8;

void func_actor_110300_801320C4(GpActorWork* arg0);

/// The shared slot-reseed body `src/actors/lib/actors_shared_80132180.c`,
/// declared for the same reason as `ActorsShared80132138` below. Its other
/// carrier is `actor_110800`.
void ActorsShared80132180(void);

/// The shared tick body `src/actors/lib/actors_shared_80132138.c`. Declared
/// here rather than through `actors_shared_80132138.h` because that header
/// publishes `ActorsShared80131f9cWork` with its own work type.
void ActorsShared80132138(void);

/// The shared reseed body `src/actors/lib/actors_shared_80132208.c`, declared
/// for the same reason as `ActorsShared80132138` above. Its other carrier is
/// `actor_110800`.
void ActorsShared80132208(void);

#endif
