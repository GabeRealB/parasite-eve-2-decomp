/* The Dryfield water hole's door handler and water task, the same in the day and night
 * builds: the task's first state is the room's own, then it draws the room's
 * water surfaces.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_WATER_HOLE_H
#define SRC_SHARED_WATER_HOLE_H

#include "types.h"

#include "main/task_types.h"

/// One rectangle of water surface drawn by `waterHoleDrawSurfaces`,
/// in world coordinates: it spans `width` along X from `x` and `depth` along Z
/// from `z`, at height `y`. The table ends at the first entry whose `y` word is
/// -1; the drawing code reads only its low half as the height.
typedef struct WaterHoleSurface {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} WaterHoleSurface;

s32  waterHoleDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void waterHoleWaterTask(Task* task);
void waterHoleWaterStart(Task* arg0);
void waterHoleDrawSurfaces(Task* task);

/// The room's water surfaces, ended by an entry whose `y` is -1.
extern WaterHoleSurface gWaterHoleSurfaces[];
/// Where the next water quad is written in the frame's primitive buffer.
extern u8* gWaterHolePrimCursor;
/// The seam wave's scroll, advanced while the room's actors run.
extern s16 gWaterHoleWaveScroll;

#endif /* SRC_SHARED_WATER_HOLE_H */
