#ifndef ACTORS_SHARED_80134DB4_H
#define ACTORS_SHARED_80134DB4_H

#include "common.h"

/// Written through `field_130` while it is still set: the object's `field_30`
/// takes the value 4, which is how this actor asks for the detach.
typedef struct ActorShared80134db4Obj130 {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} ActorShared80134db4Obj130;

/// Partial view of the actor's 0x1C work block. `field_12A` is the lateral
/// offset the sweep walks, 0x200 a call between the two bounds 0x1400 and
/// 0x1801, with `field_142` recording which way it is going. `field_140`
/// counts the calls since the last pulse: on the sixteenth the block reloads
/// it from `Gp_LcgState` and raises bit 15 of `field_DE`, which every call
/// clears first.
typedef struct ActorShared80134db4Work {
    /* 0x000 */ byte                        pad_0[0xDE];
    /* 0x0DE */ u16                         field_DE;
    /* 0x0E0 */ byte                        pad_E0[0x4A];
    /* 0x12A */ u16                         field_12A;
    /* 0x12C */ byte                        pad_12C[4];
    /* 0x130 */ ActorShared80134db4Obj130** field_130;
    /* 0x134 */ byte                        pad_134[8];
    /* 0x13C */ s16                         field_13C;
    /* 0x13E */ s16                         field_13E;
    /* 0x140 */ u16                         field_140;
    /* 0x142 */ s16                         field_142;
} ActorShared80134db4Work;

typedef struct ActorShared80134db4 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80134db4Work* field_1C;
} ActorShared80134db4;

/// Per-frame tick for the sweep actor: advances `field_12A` by 0x200 towards
/// the far bound, reversing `field_142` once it passes 0x1800 and again once
/// it falls back under 0x1400, drops whatever object `field_130` still holds
/// by marking it 4, and pulses bit 15 of `field_DE` every sixteenth call with
/// the reload drawn from `Gp_LcgState`.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80134db4(ActorShared80134db4* arg0);

#endif
