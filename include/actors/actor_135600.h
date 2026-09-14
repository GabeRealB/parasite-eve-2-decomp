#ifndef ACTOR_135600_H
#define ACTOR_135600_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Recomputes `coord`'s world matrix (`Gp_UpdateCoord`) and then projects a
/// rotating pair of offsets through it, returning the signed `ratan2` of the
/// difference between the two projections - the actor's screen-space angle.
/// `arg1` selects how far the pair is rotated: it scales by 70/4096 of a
/// revolution, so the 0x800 the tick handler latches on a hit is 35 degrees.
s32 func_actor_135600_80131E68(GsCOORDINATE2* coord, s16 arg1);

#endif // ACTOR_135600_H
