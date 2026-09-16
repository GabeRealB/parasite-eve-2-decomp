#ifndef ACTOR_107000_H
#define ACTOR_107000_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"

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
    /* 0x2B0 */ byte    pad_2B0[2];
    /* 0x2B2 */ s16     field_2B2; // armed to 3 by the hit branch, with the pair below
    /* 0x2B4 */ s16     field_2B4; // cleared on the death branch
    /* 0x2B6 */ s16     field_2B6; // cleared next to `field_2B2`
    /* 0x2B8 */ s16     field_2B8; // animation id the work is playing
    /* 0x2BA */ s16     field_2BA; // id the three helper slots last saw
    /* 0x2BC */ u16     field_2BC; // frames spent on the current id
    /* 0x2BE */ s16     field_2BE; // cleared next to the pair above
    /* 0x2C0 */ byte    pad_2C0[0xA];
    /* 0x2CA */ s16     field_2CA; // angle the transform is scaled by
    /* 0x2CC */ s16     field_2CC; // countdown seeded by the damage branch
    /* 0x2CE */ byte    pad_2CE[4];
    /* 0x2D2 */ s16     field_2D2; // non-zero: the rebind is suppressed
    /* 0x2D4 */ u16     field_2D4; // frames the reaction has run; the death branch fires at 5
    /* 0x2D6 */ s16     field_2D6; // selects the sound event's high half
    /* 0x2D8 */ s16     field_2D8; // latched copy of `field_2B8`
    /* 0x2DA */ byte    pad_2DA[0x90];
    /* 0x36A */ s16     field_36A; // reaction sub-state, cleared once applied
    /* 0x36C */ s16     field_36C; // cleared next to `field_36A`
    /* 0x36E */ s16     field_36E; // cleared alongside `field_36A`
    /* 0x370 */ s16     field_370; // animation id the work is playing
    /* 0x372 */ u16     field_372; // id the six helper slots last saw
    /* 0x374 */ u16     field_374; // frames spent on the current id
    /* 0x376 */ byte    pad_376[0xC];
    /* 0x382 */ s16     field_382; // reaction branch the hit handler selects
} Actor107000Work;

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
