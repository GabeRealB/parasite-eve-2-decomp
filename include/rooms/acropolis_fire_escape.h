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

void func_acropolis_fire_escape_8017EA68(Task* task);

void func_acropolis_fire_escape_8017FF7C(Task* task);

void func_acropolis_fire_escape_8017FF24(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FIRE_ESCAPE_H
