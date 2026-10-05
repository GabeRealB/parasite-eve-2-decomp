/* Water effects: the splash quad and spin and tile sprites drawn at a coordinate, the
 * ripple and drift tasks built on them, and two tasks that resample the
 * other display buffer through a sine wave - a distortion band and a
 * refraction ripple - for water holes, sewers and the Neo Ark pools - and the
 * wave-crested strips of the Shelter's sewer channels
 * (water_wave_strips.inc.c, configured by the includer; see that file).
 *
 * Include this header once, in the room prologue, and include each
 * water_<name>.inc.c at the position of that function. A package includes
 * only the fragments it carries. The ripple and drift tasks are static
 * inline: gameplay's room-effect tables name each room's copy, so a room
 * keeps its own entry point, which calls the task.
 *
 * waterDriftTaskU16 feeds its drawers an unsigned 16-bit sprite index.
 * _waterDrawSpinU16 and _waterDrawTileU16 are those drawers. The flags below
 * choose whether this header declares the shared drawers or the room supplies
 * its own prototypes. The bodies stay in their own includes.
 */

#ifndef SRC_SHARED_WATER_EFFECTS_H
#define SRC_SHARED_WATER_EFFECTS_H

#include "main/coord.h"
#include "main/task_types.h"

static void _waterDrawSplash(const GfxCoord* coord, s32 halfSize, s32 brightness);
/* WATER_SHARED_U16_DRAWERS is an empty presence flag. The including file
 * defines it, with no replacement list, before this header. defined() is the
 * only test. With WATER_OWN_U16_DRAWERS unset, the flag declares the shared
 * drawers below. The signatures match water_spin_u16.inc.c and
 * water_tile_u16.inc.c: s32 parameters, and each body keeps the low 16 bits
 * of the sprite index. The including file includes those two bodies.
 * WATER_OWN_U16_DRAWERS selects the room-local drawers, and that file supplies
 * its own prototypes and bodies. Defining neither flag leaves these two
 * prototypes out of this header.
 */
#if defined(WATER_SHARED_U16_DRAWERS) && !defined(WATER_OWN_U16_DRAWERS)
static void _waterDrawSpinU16(const GfxCoord* coord, s32 textureColumn, s32 radiusScale, s32 spinAngle);
static void _waterDrawTileU16(const GfxCoord* coord, s32 textureCell, s32 radiusScale);
#endif
void waterDistortBandTask(Task* task);
void waterRefractionTask(Task* task);

void waterDriftTaskNoUpdate(Task* task);

void waterRippleTaskFixedCoord(Task* task);
void waterDriftTaskU16FixedCoord(Task* task);

#endif /* SRC_SHARED_WATER_EFFECTS_H */
