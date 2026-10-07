/* Water effects: the splash quad and spin and tile sprites drawn at a coordinate, the
 * ripple and drift tasks built on them, and two tasks that resample the
 * other display buffer through a sine wave - a distortion band and a
 * refraction ripple - for water holes, sewers and the Neo Ark pools - and the
 * wave-crested strips of the Shelter's sewer channels
 * (water_wave_strips.inc.c, configured by the includer; see that file).
 *
 * Include this header once, in the room prologue, and include each
 * water_<name>.inc.c at the position of that function. A package includes
 * only the fragments it carries. Some tasks are static inline, with a room
 * entry point that gameplay's room-effect table imports. The water-spray
 * fragment instead defines the package's public callback selected by
 * WATER_SPRAY_TASK; the cached-coordinate ripple fragment uses
 * WATER_RIPPLE_CACHED_COORD_TASK. Each binding selects a public void (Task*)
 * callback declared in the carrier's room header before the fragment is included.
 *
 * _waterDriftTaskU16 feeds its drawers an unsigned 16-bit sprite index.
 * _waterDrawSpinU16 and _waterDrawTileU16 are those drawers. The flags below
 * choose whether this header declares the shared drawers or the room supplies
 * its own prototypes. The bodies stay in their own includes.
 */

#ifndef SRC_SHARED_WATER_EFFECTS_H
#define SRC_SHARED_WATER_EFFECTS_H

#include "main/areas.h"
#include "main/coord.h"
#include "main/task_types.h"

static void _waterDrawSplash(const GfxCoord* coord, s32 halfSize, s32 brightness);
/* WATER_SHARED_U16_DRAWERS is an empty presence flag. The including file
 * defines it, with no replacement list, before this header. defined() is the
 * only test. With WATER_OWN_U16_DRAWERS unset, the flag declares the shared
 * drawers below. The signatures match water_spin_u16.inc.c and
 * water_tile_u16.inc.c: a u16 sprite index, an s16 scale and, for the spin
 * drawer, an s16 angle. The including file includes those two bodies.
 * WATER_OWN_U16_DRAWERS selects the room-local drawers, and that file supplies
 * its own prototypes and bodies. Defining neither flag leaves these two
 * prototypes out of this header.
 */
#if defined(WATER_SHARED_U16_DRAWERS) && !defined(WATER_OWN_U16_DRAWERS)
static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTileU16(const GfxCoord* coord, u16 textureCell, s16 radiusScale);
#endif
void waterDistortBandTask(Task* task);

/// Redraws the Neo Ark water surfaces as vertically displaced framebuffer scanlines.
///
/// Draws only configured views of the bridge, island, garden, pavilion,
/// submarine gallery and woodland path. Rows and spans use pixels centred on
/// (160,120) of a 320x240 frame. Per-view horizontal planes use whole world Y;
/// their view-space intersections supply Z/4 depth, quantized to 1024 OT tags
/// before a signed view bias. The active camera must keep every biased index
/// in bounds and the signed depth arithmetic in range. No clamp or negative-
/// GTE-FLAG rejection occurs here.
///
/// Requires a composed view, valid projection distance, initialized scratch
/// storage and a writable actor-buffer-2 prefix holding two 488-POLY_FT4 banks
/// (0x9880 bytes). Each configured view emits at most 485 raw, opaque textured
/// packets into the bank `otBuffer` selects, sampling that buffer's VRAM page.
/// Both packet storage and sampled framebuffer contents must survive DMA;
/// concurrent loading must not overwrite the selected packet bank.
///
/// The first active call seeds `killCountdown` as a signed-halfword wave phase
/// and advances `state`. Later phase stores retain the low 16 bits. Phase
/// advances by 32 angle units per active update while actors run, or while
/// suspended in pavilion views 6 and 7. Sine/cosine use 4096 units
/// per turn; wave scale is Q12. Eight-row ramps begin at the configured first
/// row (unless it is row 1) and at a positive split's second span.
/// Borrows no task work, releases its scratch block and overwrites GTE state.
void waterRefractionTask(Task* task);

#endif /* SRC_SHARED_WATER_EFFECTS_H */
