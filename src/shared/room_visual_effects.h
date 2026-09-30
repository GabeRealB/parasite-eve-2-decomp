/* Room effect tasks, drawing helpers and private colour/trail tables.
 *
 * Static-inline task implementations sit behind ordinary overlay entry points;
 * all drawing helpers and tables are private. RoomFx_GetHaloShades supplies a
 * typed array view of the halo allocation, which may retain a trailing halfword.
 * ROOM_FX_HALO_STORAGE_TYPE, _BOUND and _INITIALIZER describe that storage layout.
 * They do not configure linkage or symbol names.
 *
 * Include this header in the prologue and the data fragment at its position.
 * Include code fragments around the overlay task wrappers in this order:
 * room_visual_effects, _halo, _flash, _trails, _sparks, _glow, _burst, _burst_draw
 * (each with the .inc.c suffix). The twin-trail entry includes _trail_task.inc.c
 * as its body, with parameter Task* task: GCC 2.8.1 changes spills when inlined.
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

/* Array view of the including overlay's halo allocation. */
static inline RoomHaloShade* RoomFx_GetHaloShades(void);

#endif /* SRC_SHARED_ROOM_VISUAL_EFFECTS_H */
