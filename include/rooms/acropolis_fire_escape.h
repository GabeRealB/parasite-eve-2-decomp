#ifndef INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H
#define INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern SVECTOR gAcropolisFireEscapeCollision04CE8Normals[10];

extern SVECTOR gAcropolisFireEscapeCollision04CE8Verts[55];

extern WorldCollisionGridFace gAcropolisFireEscapeCollision04CE8Faces[23];

extern AreaVariant D_acropolis_fire_escape_8018294C[5];

// acropolis_fire_escape
extern WorldCollisionRoomResources D_acropolis_fire_escape_80181DAC[];

extern u8* D_acropolis_fire_escape_80181DBC[];

extern ViewCount D_acropolis_fire_escape_80181DC0[];

extern WorldCoordRoomLighting D_acropolis_fire_escape_80181DC4[];

extern DirectionWarpEntry D_acropolis_fire_escape_80181DCC[];

extern SpriteView D_acropolis_fire_escape_80182E18[];

extern ViewCamera D_acropolis_fire_escape_80182E90[];

extern WorldCollisionSurfaceProperties* D_acropolis_fire_escape_80183020[];

/// Packed `spawnArg1` controls of the fire-escape flare task.
///
/// The low byte advances the pulse per animation frame; the next byte scales
/// the perspective radius. Bit 16 selects cyan rather than red. Bit 28 allocates
/// line packets but requeues the last diamond quad; bit 31 selects radial wedges.
enum {
    ACROPOLIS_FIRE_ESCAPE_FLARE_PULSE_RATE_MASK = 0xFF,
    ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT    = 8,
    ACROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK     = 0xFF,
    ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT      = 16,
    ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN            = 1 << ACROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT,
    ACROPOLIS_FIRE_ESCAPE_FLARE_STREAK_PACKETS  = 0x10000000,
    ACROPOLIS_FIRE_ESCAPE_FLARE_RADIAL          = 0x80000000,
};

/// Draws one frame of a pulsing red or cyan flare and retires its effect task.
///
/// Requires a single-coordinate task body and an owned, counted `EffectWork`
/// in `spawnArg2.pointer`. The composed origin is truncated to signed 16-bit
/// world units and projected through the view matrix. Depth is camera Z / 4;
/// depths below 17 draw nothing. `spawnArg1` uses the packed controls above.
/// The pulse's 0..127 triangle wave is doubled for the centre colour.
///
/// Radial mode queues sixteen disc wedges and four rays. Diamond mode queues
/// two quads. The streak option initializes two lines in the arena but links
/// the preceding quad again; it does not queue the lines. Queued quads use
/// additive blending. The task releases its work even when depth-clipped.
/// Requires the current ordering table, initialized scratch stack and enough
/// packet-arena space; queued packets live through GPU completion.
void acropolisFireEscapeFlareTask(Task* task);

/// Updates and draws the fire-escape lamp's flickering glow, sounding bright transitions.
///
/// Requires a single-coordinate body and an owned `EffectWork` in
/// `spawnArg2.pointer`; its `index` and `scale` retain the flicker mode and
/// brightness between frames. Draws in views 2, 3 and 7 while room-effect
/// control is below its cancellation threshold, including paused/hidden actors.
/// Every 32 animation frames a random mode selects brightness changes every
/// frame, on odd frames, a steady 48, or dim random changes. A rise from at
/// most 16 to at least 32 starts the positional flicker sound.
///
/// The signed 16-bit world origin is view-projected. Depth is camera Z / 4
/// minus 32; depths below 17 draw nothing and leave flicker state unchanged.
/// Bits 8..15 of `spawnArg1` scale the full pixel radius as byte * 1536 / depth.
/// Twenty-four additive Gouraud quads form full-, half- and eighth-radius layers;
/// the innermost colour wraps to a byte. The task keeps its work until teardown.
/// Requires the current ordering table, initialized scratch stack and enough
/// frame-arena space; queued packets live through GPU completion.
void acropolisFireEscapeFlickerLightTask(Task* task);

/// Runs the fire escape's telephone save menu and optional statistics panels.
///
/// Borrows the UI object in `spawnArg2.pointer` and its child objects. Start
/// in state 0 with the room overlay, UI and memory-card resources loaded.
/// Cleared games and demo scene 1 show the four-row menu; other games open
/// saving directly. The UI owns object lifetime and consumes the result.
void acropolisFireEscapeTelephoneMenuTask(Task* task);

/// Emits the fire escape's persistent flicker lamp and view-selected one-frame flares.
///
/// Bank-6 slot 0x4E requires a single-coordinate body and counted `EffectWork`
/// in `spawnArg2.pointer`. State 0 places the lamp at local (2904, -2082, -229),
/// then enters state 1. Spawn failures are ignored, including the initial lamp.
/// State 1 emits while room-effect control is below the cancellation threshold,
/// including actor pause/hide modes; it remains live until task teardown.
///
/// Views 3/8 place red diamond/radial flares at (1167, -913, 1670), with radius
/// bytes 6/3 and pulse step 14. Views 6/9 place cyan diamond/radial flares at
/// (-3103, -3344, 2272), with radius bytes 4/2 and pulse step 8. Pulse steps
/// advance the low-byte phase per animation frame; radii use perspective scaling.
/// Offsets are signed game units in the emitter's local frame, copied on spawn.
/// The emitter reuses its offset storage; these light callbacks use the copied
/// coordinates. Keep room, gameplay and coordinate resources live through teardown.
void acropolisFireEscapeLightEmitterTask(Task* task);

/// Runs the fire escape's room messages and actor interaction check.
///
/// State 0 registers the receiver and starts the ambience task; state 1
/// disables interaction with an absent placed actor; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisFireEscapeRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H
