#ifndef ACTOR_107000_H
#define ACTOR_107000_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

/// Owning context of the actor, passed as the first argument of its per-frame
/// handler next to the `Task` that carries the model in `Task::extra`.
///
/// `field_14` is the pose flag the `D_801153F4` mode switch writes together
/// with the model part's flag word: mode 0 zeroes both, mode 2 pairs a 1 here
/// with `TmdObject::field_C = 0x80` for the hidden pose.
///
/// This is the same object the spawn handler takes as `GpEnemy*`, seen from the
/// side that needs only that one flag: `field_14` is `GpEnemy::node.field_4`,
/// and the handlers that also tear the enemy down (`ActorsShared80136288`
/// spends `field_54` and unlinks `node`) spell it as the `GpEnemy` it is.
typedef struct Actor107000Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor107000Ctx;

/// The 0x18 bytes `func_actor_107000_80132E9C` carves off the per-object
/// scratch stack (`0x1F8003FC`, one `addiu` of `-0x18`): `vec` is the
/// translation from the model's coordinate to the player, `rot` is the Euler
/// angle triple rebuilt from the heading the same call just steered. The same
/// block as `Actor100300RotScratch`, which the sibling body `Actor00300_Fn04528`
/// carves the same way.
typedef struct Actor107000RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor107000RotScratch;
STATIC_ASSERT_SIZEOF(Actor107000RotScratch, 0x18);

/// Animation work reached through `Task::idMap`. `field_2B8`/`field_2BA`/
/// `field_2BC` are the same (id, id the three helper slots last saw, frames
/// spent on it) triple as `Actor207200Work`'s `field_28C`/`field_28E`/
/// `field_290`; a non-zero `field_2D2` suppresses the per-frame rebind.
///
/// The second triple, `field_370`/`field_372`/`field_374`, is the same thing
/// over the work's *six* helper slots, mirroring `Actor207200Work`'s
/// `field_48C`/`field_48E`/`field_490`.
///
/// `field_36A`/`field_36E` are the same pair `Actor103800Work` and
/// `ActorShared80137e18Work` carry: the reaction sub-state the damage branch
/// writes (3 here, 5 once the 0x600A5 spawn is armed) and a word cleared
/// alongside it.
///
/// `field_28C`/`field_2CA` are the same pair as `Actor207200Work`'s
/// `field_264`/`field_2A0`: the transform `ActorsShared801349d8` folds onto the
/// model, and the angle it is scaled by.
typedef struct Actor107000Work {
    /* 0x000 */ byte    pad_0[0x214];
    /* 0x214 */ GpRec18 field_214; // collision record `func_actor_107000_801364D8` re-rolls
    /* 0x22C */ byte    pad_22C[0x60];
    /* 0x28C */ MATRIX  field_28C; // transform folded onto the model part
    /* 0x2AC */ s32     field_2AC; // advanced by 0xC8 a frame while the death flag runs
    /* 0x2B0 */ s16     field_2B0; // heading the specimen is turned toward; stepped 0x20 a frame
    /* 0x2B2 */ s16     field_2B2; // armed to 3 by the hit branch, with the pair below
    /* 0x2B4 */ s16     field_2B4; // cleared on the death branch
    /* 0x2B6 */ s16     field_2B6; // cleared next to `field_2B2`
    /* 0x2B8 */ s16     field_2B8; // animation id the work is playing
    /* 0x2BA */ s16     field_2BA; // id the three helper slots last saw
    /* 0x2BC */ u16     field_2BC; // frames spent on the current id
    /* 0x2BE */ s16     field_2BE; // cleared next to the pair above
    /* 0x2C0 */ byte    pad_2C0[0x8];
    /* 0x2C8 */ s16     field_2C8; // reaction stage the per-frame handler switches on
    /* 0x2CA */ s16     field_2CA; // angle the transform is scaled by
    /* 0x2CC */ s16     field_2CC; // countdown seeded by the damage branch
    /* 0x2CE */ byte    pad_2CE[2];
    /* 0x2D0 */ u16     field_2D0; // frames until the next sound cue; re-rolled from `Gp_LcgState`
    /* 0x2D2 */ s16     field_2D2; // non-zero: the rebind is suppressed
    /* 0x2D4 */ u16     field_2D4; // frames the reaction has run; the death branch fires at 5
    /* 0x2D6 */ s16     field_2D6; // selects the sound event's high half
    /* 0x2D8 */ s16     field_2D8; // latched copy of `field_2B8`
    /* 0x2DA */ byte    pad_2DA[0x90];
    /* 0x36A */ s16     field_36A; // reaction sub-state, cleared once applied
    /* 0x36C */ s16     field_36C; // cleared next to `field_36A`
    /* 0x36E */ u16     field_36E; // cleared alongside `field_36A`
    /* 0x370 */ s16     field_370; // animation id the work is playing
    /* 0x372 */ u16     field_372; // id the six helper slots last saw
    /* 0x374 */ u16     field_374; // frames spent on the current id
    /* 0x376 */ byte    pad_376[0xC];
    /* 0x382 */ s16     field_382; // reaction branch the hit handler selects
    /* 0x384 */ byte    pad_384[0xC];
    /* 0x390 */ u16     field_390; // frames until the next 0x60080 spawn
    /* 0x392 */ u16     field_392; // spawns so far; the cue fires at 5
    /* 0x394 */ u16     field_394; // non-zero: this frame has spent its reaction (see ActorShared80136288Work)
} Actor107000Work;

/// The same 0x2E4-byte work block as its spawn handler builds it, seen from the
/// side that names the render nodes: four `GpObj`s at 0xFC / 0x134 / 0x1B4 /
/// 0x1EC, each followed by the `GpRec18` collision table its `field_C` points
/// at, `node + 0x20` (`&work->rec11C` and friends, handed to
/// `Gp_InitRec18Table`), then the transform node at 0x27C whose coordinate the
/// handler only wires up through `field_284`.
///
/// The record table seeded at 0x20C overlaps `Actor107000Work::field_214`: the
/// handler view starts that run eight bytes later, and the matched
/// `func_actor_107000_801364D8` keeps its own spelling, so the two views cannot
/// be merged without moving one of the two offsets.
typedef struct Actor107000SpawnWork {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[3];
    /* 0x08C */ byte       field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX     field_BC;       // colour matrix, TmdObject::field_20
    /* 0x0DC */ MATRIX     field_DC;       // light matrix, TmdObject::field_1C
    /* 0x0FC */ GpObj      objFC;
    /* 0x11C */ GpRec18    rec11C;
    /* 0x134 */ GpObj      obj134;
    /* 0x154 */ GpRec18    rec154[4];
    /* 0x1B4 */ GpObj      obj1B4;
    /* 0x1D4 */ GpRec18    rec1D4;
    /* 0x1EC */ GpObj      obj1EC;
    /* 0x20C */ GpRec18    rec20C;
    /* 0x224 */ byte       pad_224[0x58];
    /* 0x27C */ byte       field_27C[8];
    /* 0x284 */ void*      field_284; // render node at 0x27C: its coordinate
    /* 0x288 */ u16        field_288;
    /* 0x28A */ u16        field_28A;
    /* 0x28C */ byte       pad_28C[0x20];
    /* 0x2AC */ s32        field_2AC;
    /* 0x2B0 */ byte       pad_2B0[4];
    /* 0x2B4 */ s16        field_2B4; // cleared by the spawn handler, as Actor107000Work::field_2B4
    /* 0x2B6 */ byte       pad_2B6[2];
    /* 0x2B8 */ s16        field_2B8;
    /* 0x2BA */ s16        field_2BA;
    /* 0x2BC */ byte       pad_2BC[0x10];
    /* 0x2CC */ s16        field_2CC;
    /* 0x2CE */ s16        field_2CE;
    /* 0x2D0 */ byte       pad_2D0[2];
    /* 0x2D2 */ s16        field_2D2;
    /* 0x2D4 */ u16        field_2D4;
    /* 0x2D6 */ s16        field_2D6;
    /* 0x2D8 */ byte       pad_2D8[2];
    /* 0x2DA */ s16        field_2DA;
    /* 0x2DC */ s16        field_2DC;
    /* 0x2DE */ byte       pad_2DE[4];
    /* 0x2E2 */ s16        field_2E2;
} Actor107000SpawnWork;
STATIC_ASSERT_SIZEOF(Actor107000SpawnWork, 0x2E4);

/// The 0x39C-byte work block the actor's *other* spawn handler
/// (`func_actor_107000_80136E88`) allocates, next to `Actor107000SpawnWork`:
/// the same `GpAnimCtx`, seven animation slots instead of three, then three
/// `GpObj` render nodes where that one has four.
///
/// Node 1's `field_C` is not a record table but the `GpActorD4Rec` at 0x1FC -
/// the shape `GpActorD4` keeps, where the record's own `field_14` points at the
/// `GpRec18` run beside it (here the single record at 0x214). Nodes 2 and 3
/// hold plain tables of four and one, the way `Actor107000SpawnWork`'s do.
///
/// The tail from 0x360 is the same run `Actor107000Work` names from 0x360:
/// `field_370`/`field_372` are its animation id and the id the six helper slots
/// last saw, which is why the reset loop walks slots 1..6 and not 1..2.
typedef struct Actor107000Spawn2Work {
    /* 0x000 */ GpAnimCtx      context;
    /* 0x014 */ GpAnimSlot     slots[7];        // six helper slots + slot 0
    /* 0x12C */ byte           field_12C[0x70]; // pose buffer, func_800B3F84 arg3
    /* 0x19C */ MATRIX         field_19C;       // colour matrix, TmdObject::field_20
    /* 0x1BC */ MATRIX         field_1BC;       // light matrix, TmdObject::field_1C
    /* 0x1DC */ GpObj          obj1;
    /* 0x1FC */ GpActorD4Rec   field_1FC;
    /* 0x214 */ GpRec18        field_214[1]; // the table `field_1FC` names
    /* 0x22C */ GpObj          obj2;
    /* 0x24C */ GpRec18        field_24C[4];
    /* 0x2AC */ GpObj          obj3;
    /* 0x2CC */ GpRec18        field_2CC[1];
    /* 0x2E4 */ byte           pad_2E4[0x78];
    /* 0x35C */ GsCOORDINATE2* field_35C; // the model's second coordinate
    /* 0x360 */ u16            field_360;
    /* 0x362 */ u16            field_362;
    /* 0x364 */ s16            field_364; // spawn arg's high half
    /* 0x366 */ u16            field_366; // spawn arg's low half
    /* 0x368 */ byte           pad_368[0x8];
    /* 0x370 */ s16            field_370; // animation id the work is playing
    /* 0x372 */ u16            field_372; // id the six helper slots last saw
    /* 0x374 */ byte           pad_374[0x10];
    /* 0x384 */ s16            field_384;
    /* 0x386 */ s16            field_386;
    /* 0x388 */ s16            field_388;
    /* 0x38A */ s16            field_38A;
    /* 0x38C */ s16            field_38C;
    /* 0x38E */ s16            field_38E;
    /* 0x390 */ u16            field_390; // frames until the next 0x60080 spawn
    /* 0x392 */ u16            field_392; // spawns so far; the cue fires at 5
    /* 0x394 */ u16            field_394; // non-zero: this frame has spent its reaction
    /* 0x396 */ u16            field_396;
    /* 0x398 */ byte           pad_398[0x4];
} Actor107000Spawn2Work;
STATIC_ASSERT_SIZEOF(Actor107000Spawn2Work, 0x39C);

/// Free-running linear congruential state every overlay draws its random numbers
/// from: `state = state * 5 + 0x71357911`, read back through the high halfword.
extern u32 Gp_LcgState;

/// Picks the reaction branch the specimen takes on this hit and stores it in
/// `field_382`, then hands back the collision record the caller armed. A
/// countdown of 0xBB8 or more, or a record with no slot matching the 0x10000
/// kind, clears the branch and `field_36E` instead. Otherwise the branch is 3
/// when the first `Gp_LcgState` draw folds to under 11, 2 when the target is
/// 2500 units or further. Closer than that, a second draw is taken: it lands on
/// 1 when that draw folds to 11 or more, and the branch stays 2 when it does
/// not. Either way `field_374`/`field_372` are reset, and the record is released.
void func_actor_107000_801364D8(Task* arg0);

#endif
