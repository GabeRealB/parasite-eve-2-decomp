#ifndef ROOMS_SHARED_8017F10C_H
#define ROOMS_SHARED_8017F10C_H

#include "common.h"

#include <psyq/libgs.h>

#include "main/task.h"

/// Draws one mote of a room's ambient effect. Each overlay carrying
/// `RoomsShared8017f10c` has its own body for this immediately after the shared
/// one, and they are not the same routine - the two acropolis rooms share a
/// 196-instruction draw, the two neo_ark ones a different 211-instruction one -
/// so only the *name* is shared, aliased per overlay in
/// `configs/USA/sym/rooms/<overlay>.txt`. `arg1` is the mote's half-extent and
/// `arg2` its fade level, zero meaning "draw the raw texture".
void RoomsShared8017f10cSub(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);

/// One drifting mote of a room's ambient effect. State 0 seeds the mote from
/// `Gp_LcgState`: a 0x20 brightness, a tilt pair (`field_28` / `field_2A`) and
/// a per-frame drift vector in `field_10`. State 1 flies it - the drift is
/// added to the coordinate's translation, the tilt drives `Gfx_RotMatrixX` /
/// `Gfx_RotMatrixZ`, and each axis of the drift walks back towards zero one
/// unit per frame, re-rolling to a fresh multiple of 8 once it reaches it, so
/// the mote wanders instead of settling. Once it has risen past the origin
/// (`t[1] > 0`) state 2 fades the mote in and state 3 fades it out, releasing
/// the work block when the ramp runs out.
void RoomsShared8017f10c(Task* task);

#endif // ROOMS_SHARED_8017F10C_H
