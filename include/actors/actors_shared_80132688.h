#ifndef ACTORS_SHARED_80132688_H
#define ACTORS_SHARED_80132688_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/session.h"

#include "actors/actors_shared_80132d78.h"

/// Work block the carriers hang off their context's 0x1C slot -- the same
/// block `Actor402200Work` describes, seen through the fields this body
/// reaches.
///
/// `field_6CE` is the body's state and `field_6B4` / `field_6FA` the box table
/// `ActorsShared80132d78` also walks. `field_6A4`..`field_6AC` take the target
/// position, raised by bit 0x4000 of `field_5BA` / `field_5DA`, and
/// `field_5F4` is the head of the actor's `GpRec18` table.
typedef struct ActorShared80132688Work {
    /* 0x000 */ byte                       pad_0[0x5BA];
    /* 0x5BA */ u16                        field_5BA;
    /* 0x5BC */ byte                       pad_5BC[0x1E];
    /* 0x5DA */ u16                        field_5DA;
    /* 0x5DC */ byte                       pad_5DC[0x18];
    /* 0x5F4 */ GpRec18                    field_5F4;
    /* 0x60C */ byte                       pad_60C[0x98];
    /* 0x6A4 */ s32                        field_6A4;
    /* 0x6A8 */ s32                        field_6A8;
    /* 0x6AC */ s32                        field_6AC;
    /* 0x6B0 */ byte                       pad_6B0[4];
    /* 0x6B4 */ ActorShared80132d78Region* field_6B4;
    /* 0x6B8 */ byte                       pad_6B8[0x14];
    /* 0x6CC */ s16                        field_6CC;
    /* 0x6CE */ s16                        field_6CE;
    /* 0x6D0 */ byte                       pad_6D0[0x16];
    /* 0x6E6 */ s16                        field_6E6;
    /* 0x6E8 */ byte                       pad_6E8[0x12];
    /* 0x6FA */ s16                        field_6FA;
    /* 0x6FC */ byte                       pad_6FC[0xC];
    /* 0x708 */ s16                        field_708;
    /* 0x70A */ byte                       pad_70A[4];
    /* 0x70E */ s16                        field_70E;
} ActorShared80132688Work;
STATIC_ASSERT_SIZEOF(ActorShared80132688Work, 0x710);

/// Display object at the context's 0x2C slot; `field_8` is its per-part
/// coordinate array, whose root this body reads.
typedef struct ActorShared80132688Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80132688Obj2C;

/// Actor context: `field_1C` is the work block above and `field_2C` the
/// display object. The player (`gameGetPtrSlot(3)`) is read the same way.
typedef struct ActorShared80132688 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80132688Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80132688Obj2C* field_2C;
} ActorShared80132688;

/// 0x28-byte block the body takes from `G_SCRATCH_HEAD`: `out` first holds the
/// player's planar offset from a box centre, then the -0x5AA offset `in`
/// rotated through the player's root coordinate.
typedef struct ActorShared80132688Scratch {
    /* 0x00 */ VECTOR  out;
    /* 0x10 */ byte    pad_10[0x10];
    /* 0x20 */ SVECTOR in;
} ActorShared80132688Scratch;
STATIC_ASSERT_SIZEOF(ActorShared80132688Scratch, 0x28);

/// Scans the box table while `field_6CE` is 0: a kind-0 box whose radius
/// `field_2` holds the player's planar offset from its centre (`field_4`,
/// `field_6`) switches the state to 1 and parks the target 0x5AA behind the
/// player, raising bit 0x4000 of `field_5BA` / `field_5DA`; a kind-1 box
/// holding the player sets `field_6CC` / `field_70E` to 3 and records its index
/// in `field_708`. State 1 sets both to 1 unless the record table's head is
/// occupied, clears the target flags and the table, and drops back to 0.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared80132688(ActorShared80132688* arg0);

#endif
