/* The Dryfield water hole's door handler and water task, the same in the day and night
 * builds: the task prepares its actor buffer, then draws the room's water surfaces.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_WATER_HOLE_H
#define SRC_SHARED_WATER_HOLE_H

#include "common.h"

#include "main/task_types.h"

#include "rooms/room.h"

/// A rectangular patch of water in world coordinates that carries its own height.
///
/// The same rectangle as `RoomWaterSurface`, whose drawer supplies the height;
/// here the final word is the height instead of a subdivision count. The drawer
/// cuts `width` into 64 columns by integer division and `depth` into two halves,
/// one quad strip each, joined at a seam along X. A table ends at an entry
/// with `y == WATER_SURFACE_LIST_END`, whose other fields are not read, so no
/// surface can lie at that height.
typedef struct {
    s16 x;     // Starting X in world units
    s16 z;     // Starting Z in world units
    s16 width; // Extent along +X in world units
    s16 depth; // Extent along +Z in world units
    s32 y;     // Undisplaced height in world units, narrowed to a signed halfword when drawn (-1 list end)
} WaterHoleSurface;
STATIC_ASSERT_SIZEOF(WaterHoleSurface, 0xC);

s32         waterHoleDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void        waterHoleWaterTask(Task* task);
static void _waterHoleWaterStart(Task* task);
static void _waterHoleDrawSurfaces(Task* task);

/// The room's water surfaces, ended by an entry whose `y` is `WATER_SURFACE_LIST_END`.
extern WaterHoleSurface gWaterHoleSurfaces[];
/// Where the next water quad is written in the frame's primitive buffer.
extern u8* gWaterHolePrimCursor;
/// The seam wave's scroll, advanced while the room's actors run.
extern s16 gWaterHoleWaveScroll;

#endif /* SRC_SHARED_WATER_HOLE_H */
