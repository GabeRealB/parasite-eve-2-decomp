/* Room effect tasks, drawing helpers and their colour and trail tables.
 *
 * The library comes in three sections, and a room carries the ones it uses, in
 * this order and each complete:
 *
 *   halo   _halo, _glow_quad, _flash
 *   flash  _flash_task, _trails, _trail_task, _sparks, _glow
 *   flying _flying_tasks, _burst, _burst_draw
 *
 * (each room_visual_effects<name>.inc.c). A room includes this header in its
 * prologue, then room_visual_effects.inc.c before its first section, then every
 * fragment of its sections at the position of that code, with its task entry
 * points between them. The twin-trail entry includes _trail_task.inc.c as its
 * body, with parameter Task* task, since GCC 2.8.1 changes spills when that task
 * is inlined. Task implementations are static inline, so a room compiles only
 * the ones its entry points call; the drawing helpers are static functions,
 * which GCC emits whether used or not, which is why a section's fragments go in
 * whole. A room with no task of the library may still use a helper fragment on
 * its own, as one uses _glow_quad.
 *
 * The tables have external linkage: some rooms define a table in one file and
 * use it from another. Include _halo_data, _trail_data and _disc_data for the
 * room's sections, at the tables' position in the data. The halo storage's
 * layout varies by room - RoomFx_GetHaloShades supplies a typed array view of
 * it, which may retain a trailing halfword - so the room sets
 * ROOM_FX_HALO_STORAGE_TYPE, _BOUND and _INITIALIZER before including
 * _halo_data and defines RoomFx_GetHaloShades where its halo code is.
 */

#ifndef SRC_SHARED_ROOM_VISUAL_EFFECTS_H
#define SRC_SHARED_ROOM_VISUAL_EFFECTS_H

#include "gameplay/display.h"

#include "main/task_types.h"

#include "rooms/room.h"

/* Interface for the including source. */

static void RoomFx_DrawMote(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void RoomFx_DrawHaloRing(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void RoomFx_DrawHaloDisc(GfxCoord* arg0, s16 arg1, u8* rgb);
static void RoomFx_DrawBurstGlow(GfxCoord* coord, s16 size);
static void RoomFx_DrawGroundQuad(GfxCoord* arg0, s32 arg1);
static void RoomFx_DrawFlashStar(GfxCoord* arg0, s16 arg1, u8* arg2);
static void RoomFx_DrawFlashRing(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void RoomFx_DrawFlashDisc(GfxCoord* arg0, s16 arg1, u8* rgb);
static void RoomFx_DrawTwinTrail(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void RoomFx_DrawBurstStar(GfxCoord* arg0, s16 arg1, u8* arg2);
static void RoomFx_DrawFlyingSpark(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void RoomFx_DrawFlyingRing(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void RoomFx_DrawFlyingDisc(GfxCoord* arg0, s32 arg1, u8* rgb);
static void RoomFx_DrawBurst2Glow(GfxCoord* coord, s16 size);
static void RoomFx_DrawGround2Quad(GfxCoord* arg0, s32 arg1);

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
