/* Dryfield motel room 6's cutscene trigger and its glow, the same in the day and
 * night builds. Each build keeps its own cutscene record and glow position.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MOTEL_ROOM_6_H
#define SRC_SHARED_MOTEL_ROOM_6_H

#include "types.h"

#include "main/task_types.h"

#include "dryfield_time.h"

/// The cutscene record the 0x16 event fills in and spawns the cutscene task on.
/// An uninitialized common: the image places commons in first-declaration
/// order, so a room declares it before including this header.
extern RoomCutsceneRec gMotelRoom6CutsceneRec;
/// Where the room's glow is drawn.
extern SVECTOR gMotelRoom6GlowPos[];

s32 motelRoom6CutsceneMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

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

/// The 0x13F0 handler's fallback for every event but the cutscene's: empty in
/// the day room, the room's other actions at night.
#if DRYFIELD_TIME == DRYFIELD_DAY
void motelRoom6ActionMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
#else
s32 motelRoom6ActionMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
#endif

#endif /* SRC_SHARED_MOTEL_ROOM_6_H */
