#ifndef ACTOR_104900_H
#define ACTOR_104900_H

#include "common.h"

#include "actors/actors_shared_80137fb8.h"
#include "actors/actors_shared_801385e0.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/pairsrc.h"
#include "main/session.h"
#include "main/task.h"

/// Player flag byte read by the 0x801339B0 handler: when it is 1 the handler
/// takes its `GameActor::field_958` shortcut instead of measuring distance.
extern u8 D_801153F2;

typedef ActorsShared80137fb8Work Actor104900Work;

/// 0xBCC-byte work block `func_actor_104900_8013279C` allocates with
/// `memCalloc` and parks in `Task::work`. The coordinate at the front is
/// linked as `coords[1].sub`; the two `GpAnimCtx` runs are what
/// `func_800B3F84` seeds. Same size as `ActorsShared80138efcWork` /
/// `ActorShared801384acWork`, which are later views of this block.
typedef struct Actor104900SpawnWork {
    /* 0x000 */ GsCOORDINATE2  coord;
    /* 0x050 */ GpAnimCtx      anim;
    /* 0x064 */ GpAnimSlot     slots[21];
    /* 0x3AC */ byte           poses[0x150];
    /* 0x4FC */ GpAnimCtx      anim2;
    /* 0x510 */ GpAnimSlot     slots2[21];
    /* 0x858 */ byte           poses2[0x150];
    /* 0x9A8 */ byte           pad_9A8[0x1A0];
    /* 0xB48 */ MATRIX         lightMtx;
    /* 0xB68 */ MATRIX         colorMtx;
    /* 0xB88 */ u32            actorId;
    /* 0xB8C */ byte           pad_B8C[6];
    /* 0xB92 */ s16            field_B92;
    /* 0xB94 */ byte           pad_B94[0x10];
    /* 0xBA4 */ s8             field_BA4;
    /* 0xBA5 */ s8             field_BA5;
    /* 0xBA6 */ byte           pad_BA6;
    /* 0xBA7 */ s8             state;
    /* 0xBA8 */ byte           pad_BA8[6];
    /* 0xBAE */ u8             field_BAE;
    /* 0xBAF */ byte           pad_BAF;
    /* 0xBB0 */ GsCOORDINATE2* field_BB0;
    /* 0xBB4 */ s16            field_BB4;
    /* 0xBB6 */ s16            field_BB6;
    /* 0xBB8 */ u8             field_BB8;
    /* 0xBB9 */ byte           pad_BB9[2];
    /* 0xBBB */ u8             field_BBB;
    /* 0xBBC */ byte           pad_BBC[0x10];
} Actor104900SpawnWork;
STATIC_ASSERT_SIZEOF(Actor104900SpawnWork, 0xBCC);

extern s32        D_8007216C;
extern GpPairSrcE D_actor_104900_80139308;
extern GpPairSrcE D_actor_104900_80139330;
extern u8         D_actor_104900_80147424[];

/// Scale copied onto the stack and passed to `ActorsShared801385e0` when the
/// placement `entryId` is 0x31: 0x1400 on each axis. The trailing word is
/// present in the object and unread.
typedef struct Actor104900ScaleRodata {
    ActorsShared801385e0Scale scale;
    s32                       pad;
} Actor104900ScaleRodata;
STATIC_ASSERT_SIZEOF(Actor104900ScaleRodata, 0x14);

extern const Actor104900ScaleRodata D_actor_104900_80131E30;

/// Pair table the spawn state packs into the display node's `GpObj.key`.
extern GpU16Pair D_actor_104900_80139318;

/// Overlay-local spawn/setup state of `ActorsShared8013845c`: allocates the
/// 0x58-byte work block, plays the spawn cue, seeds the display node and
/// hands off to `ActorsShared8013845cSub1`.
void ActorsShared8013845cSub0(Task* task);
void ActorsShared8013845cSub1(Task* task);

#endif // ACTOR_104900_H
