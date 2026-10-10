/* Dryfield motel room 6's cutscene trigger and its glow, the same in the day and
 * night builds. Each carrier supplies its private _gMotelRoom6CutsceneRec
 * and _gMotelRoom6GlowPosition before including the implementation fragments.
 * The cutscene record is writable; the glow position is a const SVECTOR point.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MOTEL_ROOM_6_H
#define SRC_SHARED_MOTEL_ROOM_6_H

#include "dryfield_time.h"

/// Binds the shared glow task definition to its room overlay's public export.
///
/// The carrier must define `DRYFIELD_TIME` as `DRYFIELD_DAY` or
/// `DRYFIELD_NIGHT` before this header and retain it through
/// `motel_room_6_draw_glow.inc.c`. Its public room header declares the selected
/// `void (Task*)` callback. The replacement is a function identifier with no
/// arguments, captured values, evaluation, stringification or token pasting.
#if DRYFIELD_TIME == DRYFIELD_DAY
#define MOTEL_ROOM_6_DRAW_GLOW_TASK dryfieldMotelRoom6DrawGlowTask
#else
#define MOTEL_ROOM_6_DRAW_GLOW_TASK dryfieldNightMotelRoom6DrawGlowTask
#endif

#endif /* SRC_SHARED_MOTEL_ROOM_6_H */
