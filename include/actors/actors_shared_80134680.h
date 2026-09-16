#ifndef ACTORS_SHARED_80134680_H
#define ACTORS_SHARED_80134680_H

#include "common.h"

#include "main/task.h"

/// The part of the carriers' work block this body touches. Each carrier's block
/// is its own type (`Actor107000Work`, ...); the shared unit only names the
/// animation triple - the id being played, the id the three helper slots last
/// saw and the frames spent on it - plus the flag that suppresses the rebind.
typedef struct ActorsShared80134680Work {
    /* 0x000 */ byte pad_0[0x2B8];
    /* 0x2B8 */ s16  field_2B8; // animation id the work is playing
    /* 0x2BA */ s16  field_2BA; // id the three helper slots last saw
    /* 0x2BC */ u16  field_2BC; // frames spent on the current id
    /* 0x2BE */ byte pad_2BE[0x14];
    /* 0x2D2 */ s16  field_2D2; // non-zero: the rebind is suppressed
} ActorsShared80134680Work;

/// Rebinds the work's animation id to its three helper slots; carried by
/// `actor_104600`, `actor_107000`, `actor_204600` and `actor_207000`.
void ActorsShared80134680(Task* arg0);

#endif
