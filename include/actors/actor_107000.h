#ifndef ACTOR_107000_H
#define ACTOR_107000_H

#include "common.h"
#include <psyq/libgte.h>

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
/// `field_28C`/`field_2CA` are the same pair as `Actor207200Work`'s
/// `field_264`/`field_2A0`: the transform `ActorsShared801349d8` folds onto the
/// model, and the angle it is scaled by.
typedef struct Actor107000Work {
    /* 0x000 */ byte   pad_0[0x28C];
    /* 0x28C */ MATRIX field_28C; // transform folded onto the model part
    /* 0x2AC */ byte   pad_2AC[8];
    /* 0x2B4 */ s16    field_2B4; // cleared on the death branch
    /* 0x2B6 */ byte   pad_2B6[2];
    /* 0x2B8 */ s16    field_2B8; // animation id the work is playing
    /* 0x2BA */ s16    field_2BA; // id the three helper slots last saw
    /* 0x2BC */ u16    field_2BC; // frames spent on the current id
    /* 0x2BE */ byte   pad_2BE[0xC];
    /* 0x2CA */ s16    field_2CA; // angle the transform is scaled by
    /* 0x2CC */ s16    field_2CC; // countdown seeded by the damage branch
    /* 0x2CE */ byte   pad_2CE[4];
    /* 0x2D2 */ s16    field_2D2; // non-zero: the rebind is suppressed
    /* 0x2D4 */ byte   pad_2D4[2];
    /* 0x2D6 */ s16    field_2D6; // selects the sound event's high half
    /* 0x2D8 */ s16    field_2D8; // latched copy of `field_2B8`
    /* 0x2DA */ byte   pad_2DA[0x96];
    /* 0x370 */ s16    field_370; // animation id the work is playing
    /* 0x372 */ u16    field_372; // id the six helper slots last saw
    /* 0x374 */ u16    field_374; // frames spent on the current id
} Actor107000Work;

#endif
