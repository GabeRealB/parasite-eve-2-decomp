#ifndef INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
#define INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room data exported to the shared waypoint actor.
extern SVECTOR* D_neo_ark_submarine_gallery_80181B0C[4];

extern SVECTOR D_neo_ark_submarine_gallery_80181B1C[6];

extern s16 D_neo_ark_submarine_gallery_80181A48;

extern s16 D_neo_ark_submarine_gallery_801818B8;

extern TaskDesc D_neo_ark_submarine_gallery_801818BC[1];

extern AreaApplyRec D_neo_ark_submarine_gallery_8018590C[4];

extern TaskDesc D_neo_ark_submarine_gallery_8018186C;

extern AreaVariant D_neo_ark_submarine_gallery_80185860[13];

// neo_ark_submarine_gallery
extern u8* D_neo_ark_submarine_gallery_80181A08[];

extern ViewCount D_neo_ark_submarine_gallery_80181A0C[];

extern DirectionWarpEntry D_neo_ark_submarine_gallery_80181A10[];

extern WorldCollisionGrid D_neo_ark_submarine_gallery_8018239C;

extern ViewCamera D_neo_ark_submarine_gallery_801823C0[];

extern SpriteView D_neo_ark_submarine_gallery_80184D10[];

extern WorldCoordRoomLights D_neo_ark_submarine_gallery_80185284;

extern WorldCollisionTrigger D_neo_ark_submarine_gallery_8018529C[];

extern WorldCollisionTrigger D_neo_ark_submarine_gallery_801854FC[];

extern WorldCollisionSurfaceProperties* D_neo_ark_submarine_gallery_801858EC[];

/// Draws the gallery's fixed glows for the current mapped camera view.
///
/// State zero selects this room's water-ripple and water-spray effects and
/// changes to state one. Every invocation, including the first, draws the
/// capsules and discs visible in mapped views 2..6; view 2 also draws a pulsing
/// light prism. Other mapped views emit no primitives. The task remains live.
///
/// `task` must have a live coordinate body whose coordinate is already composed
/// for the prism. Requires the loaded gallery overlay, current view matrices,
/// initialized scratch stack and frame packet arena and ordering table with
/// room for up to 39 Gouraud quads and their additive blend commands. Points and
/// the coordinate are borrowed for this call; queued packets live until GPU
/// completion. Drawing also runs while effects are suspended or cancelled.
void neoArkSubmarineGalleryDrawViewGlowsTask(Task* task);

/// Advances and draws one expanding, fading water-surface ripple in this room.
///
/// `task` must be a counted effect with owned `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body and initial state zero.
/// `spawnArg1` bits 0..11 give the local half-side in game units (0..4095).
/// Running updates grow it by 32 and fade brightness from 64 by 2 per update;
/// a fresh ripple lasts 32 running updates. The first update selects a random
/// surface yaw, applied after that update's coordinate composition.
/// Non-running updates redraw without aging; cancellation redraws once before
/// retirement. Retirement frees the work and destroys the task and body,
/// decrementing the room's effect count. Pointers to retired objects expire.
void neoArkSubmarineGalleryWaterRippleTask(Task* task);

/// Advances and draws one eight-cell water-spray particle in this room.
///
/// `task` must be a counted effect with owned `EffectWork` in
/// `spawnArg2.pointer`, a coordinate body, initial state zero and cell index zero.
/// `spawnArg1` bits 0..11 give perspective size (0..4095), bits 12..15 updates
/// per cell (0 selects 1), and bits 16..23 launch speed in parent-coordinate
/// units per update (0 selects 64). Bits 24..27 select velocity: 0 stationary,
/// 1 upward burst, 2 all-axis spray, 3 narrow upward jet, 5 copied spawn-offset
/// direction; other values normalize the zero direction. Nonzero bits 28..31
/// select upright drawing; otherwise the sprite keeps a random rotation.
/// A supplied nonzero `EffectWork::move` bypasses velocity generation.
///
/// The first running update initializes without drawing or moving. Later
/// updates draw, move and add 6 to Y velocity, narrowing it to s16; stationary
/// particles skip movement and gravity. Cells 0..7 each last the decoded period.
/// Suspended updates redraw without aging; cancellation retires without drawing.
/// Retirement frees the work and destroys the task and body, decrementing the
/// room's effect count. Pointers to retired objects expire.
void neoArkSubmarineGalleryWaterDriftTaskU16(Task* task);

void func_neo_ark_submarine_gallery_8017EBCC(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBMARINE_GALLERY_H
