#ifndef ACTORS_SHARED_8016331C_H
#define ACTORS_SHARED_8016331C_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Animation view of the work block `actor_323000` and `actor_323400` park in
/// `Task::work` -- that slot is not a `TaskIdMap` here. The pose context at
/// 0x1C and its blend twin at 0x420 are each followed by 0x28-byte
/// `GpAnimSlot`s, and 0x83C holds the blend weight the two are written with.
/// The pads stand in for the rest of the block, which the two overlays name
/// differently: 0x420 is not a whole number of slots past 0x30, and a slot
/// array cannot span the animation-state halfwords at 0x828..0x830.
typedef struct ActorShared8016331cAnimWork {
    /* 0x000 */ byte       pad_0[0x1C];
    /* 0x01C */ GpAnimCtx  anim;
    /* 0x030 */ GpAnimSlot slots[25];
    /* 0x418 */ byte       pad_418[8];
    /* 0x420 */ GpAnimCtx  blendAnim;
    /* 0x434 */ GpAnimSlot blendSlots[25];
    /* 0x81C */ byte       pad_81C[0x16];
    /// Clip id the primary slots are seeded from, three behind the one they
    /// play.
    /* 0x832 */ u8   field_832;
    /* 0x833 */ byte pad_833[7];
    /// Clip id the blend slots are seeded from as it stands.
    /* 0x83A */ u8   field_83A;
    /* 0x83B */ byte pad_83B;
    /// Blend weight `field_83C`; the blend context receives its 0x1000
    /// complement.
    /* 0x83C */ s16 field_83C;
} ActorShared8016331cAnimWork;

/// Blends pose slots 1..0x11 against a second animation context: the first
/// eleven copy the two clip-id bytes into their slot records and are written
/// from both contexts with `0x1000 - field_83C` as the blend weight, the rest
/// only tick. Identical bodies in `actor_323000` and `actor_323400`.
void ActorsShared8016331c(Task* task);

#endif
