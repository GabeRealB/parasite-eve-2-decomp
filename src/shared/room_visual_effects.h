/* Room effect tasks, drawing helpers and their colour and trail tables.
 *
 * The library comes in three sections, and a room carries the ones it uses, in
 * this order and each complete:
 *
 *   halo   room_visual_effects, _halo, _glow_quad, _flash
 *   flash  _flash_task, _trails, _trail_task, _sparks, _glow
 *   flying _flying_tasks, _burst, _burst_draw
 *
 * (each with the .inc.c suffix). A room includes every fragment of its sections
 * at the position of that code, with its task entry points between them; the
 * twin-trail entry includes _trail_task.inc.c as its body, with parameter
 * Task* task, since GCC 2.8.1 changes spills when that task is inlined. Task
 * implementations are static inline, so a room compiles only the ones its entry
 * points call; the drawing helpers are static functions, which GCC emits
 * whether used or not, which is why a section's fragments go in whole.
 *
 * The tables have external linkage: some rooms define a table in one file and
 * use it from another. Include _halo_data, _trail_data and _disc_data for the
 * room's sections, at the tables' position in the data. The halo storage's
 * layout varies by room - RoomFx_GetHaloShades supplies a typed array view of
 * it, which may retain a trailing halfword - so the room declares it and sets
 * ROOM_FX_HALO_STORAGE_TYPE, _BOUND and _INITIALIZER before including
 * _halo_data. These macros describe the storage; they do not set its name.
 */

#ifndef SRC_SHARED_ROOM_VISUAL_EFFECTS_H
#define SRC_SHARED_ROOM_VISUAL_EFFECTS_H

#include "gameplay/display.h"

#include "main/task_types.h"

#include "rooms/room.h"

/* Interface for the including source. */

static inline void RoomFx_MoteTask(Task* task);

static inline void RoomFx_HaloTask(Task* arg0);

static inline void RoomFx_OrangeBurstTask(Task* arg0);

static inline void RoomFx_SparkEmitterTask(Task* arg0);

static inline void RoomFx_FlashTask(Task* arg0);

static inline void RoomFx_SparkBurstTask(Task* task);

static inline void RoomFx_GlowDiscTask(Task* arg0);

static inline void RoomFx_FlyingSparkTask(Task* task);

static inline void RoomFx_OrangeBurst2Task(Task* arg0);

/// Halo shade storage whose three shades are followed by a retained halfword.
/// The halfword differs between rooms and nothing reads it; whether it belongs
/// to this object is unresolved.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} RoomFxHaloStorage;

extern SVECTOR       RoomFx_TrailOffsets[2];
extern RoomHaloShade RoomFx_DiscShades[2];

/* Array view of the including overlay's halo allocation. */
static inline RoomHaloShade* RoomFx_GetHaloShades(void);

#endif /* SRC_SHARED_ROOM_VISUAL_EFFECTS_H */
