/* Water effects: the splash, spin and tile sprites drawn at a coordinate, the
 * ripple and drift tasks built on them, and two tasks that resample the
 * other display buffer through a sine wave - a distortion band and a
 * refraction ripple - for water holes, sewers and the Neo Ark pools.
 *
 * Include this header in the prologue and each water_<name>.inc.c at the
 * position of that function; a package includes only the ones it carries. The
 * ripple and drift tasks are static inline: gameplay's room effect
 * tables name each room's copy, so a room keeps its own entry point, which
 * calls the task.
 */

#ifndef SRC_SHARED_WATER_EFFECTS_H
#define SRC_SHARED_WATER_EFFECTS_H

#include "main/coord.h"
#include "main/task_types.h"

void waterDrawSplash(GfxCoord* arg0, s32 arg1, s32 arg2);
void waterDrawSpin(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void waterDrawTile(GfxCoord* arg0, s16 arg1, s16 arg2);
void waterDistortBandTask(Task* task);
void waterRefractionTask(Task* task);

#endif /* SRC_SHARED_WATER_EFFECTS_H */
