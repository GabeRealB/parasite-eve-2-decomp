#ifndef ROOMS_SHARED_8017D830_H
#define ROOMS_SHARED_8017D830_H

#include "common.h"

#include "gameplay/3FB8.h"
#include "main/session.h"
#include "overlay.h"

#include <psyq/libgs.h>
#include "main/coord.h"
#include <psyq/libgte.h>

/// The world delta the last call produced, kept for the room's own use. Every
/// carrying room holds its own at its own address, named there by the family's
/// symbol maps, so the shared object owns no data.
extern SVECTOR RoomsShared8017d830Delta;

/// Steps a coordinate frame along the movement `func_800E0C10` computes for
/// `rec`, rounding the sub-pixel remainder of X and Z away from zero and
/// publishing the whole delta. Returns 1 when the frame actually moved. Six
/// acropolis rooms carry this body.
s32 RoomsShared8017d830(GpCoord* coord, GpRec18* rec, s16 arg2);

#endif // ROOMS_SHARED_8017D830_H
