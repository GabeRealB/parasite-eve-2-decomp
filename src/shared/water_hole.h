/* The Dryfield water hole's door handler and water task, the same in the day and night
 * builds. Each build draws its own water surfaces.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_WATER_HOLE_H
#define SRC_SHARED_WATER_HOLE_H

#include "types.h"

#include "main/task_types.h"

s32  waterHoleDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);
void waterHoleWaterTask(Task* task);
void waterHoleWaterStart(Task* arg0);
void waterHoleDrawSurfaces(Task* task);

#endif /* SRC_SHARED_WATER_HOLE_H */
