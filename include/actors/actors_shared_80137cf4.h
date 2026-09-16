#ifndef ACTORS_SHARED_80137CF4_H
#define ACTORS_SHARED_80137CF4_H

#include "common.h"

#include "main/task.h"

/// The part of the carriers' work block this body touches. Each carrier's block
/// is its own type (`Actor107000Work`, ...); the shared unit only names the
/// animation triple - the id being played, the id the six helper slots last
/// saw and the frames spent on it.
typedef struct ActorsShared80137cf4Work {
    /* 0x000 */ byte pad_0[0x370];
    /* 0x370 */ s16  field_370; // animation id the work is playing
    /* 0x372 */ u16  field_372; // id the six helper slots last saw
    /* 0x374 */ u16  field_374; // frames spent on the current id
} ActorsShared80137cf4Work;

/// Rebinds the work's animation id to its six helper slots; carried by
/// `actor_107000` and `actor_207000`.
void ActorsShared80137cf4(Task* arg0);

#endif
