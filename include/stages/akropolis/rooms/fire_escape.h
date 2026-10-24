#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_FIRE_ESCAPE_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_FIRE_ESCAPE_H

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

/// Writable Q12 normals of the fire-escape collision mesh.
extern SVECTOR gAkropolisFireEscapeCollisionGridNormals[10];

/// Writable room-space collision vertices in signed game units.
extern SVECTOR gAkropolisFireEscapeCollisionGridVerts[55];

/// Writable indexed collision faces, including the placed enemy's first-face barrier.
extern WorldCollisionGridFace gAkropolisFireEscapeCollisionGridFaces[23];

/// Actor layout variants of the fire escape.
extern AreaVariant gAkropolisFireEscapeAreaVariants[5];

/// Room 1's collision mesh, eight view boundaries, twelve actions and two sight blockers.
extern WorldCollisionRoomResources gAkropolisFireEscapeCollisionRoomResources[1];

/// Room 1's logical-view map, borrowing gameplay's identity map while loaded.
extern u8* gAkropolisFireEscapeViewMaps[1];

/// Room 1's ten logical slots for reverse view lookup, independent of camera capacity.
extern ViewCount gAkropolisFireEscapeViewCounts[1];

/// Room 1's five point lights, with default per-view ambient minima.
extern WorldCoordRoomLighting gAkropolisFireEscapeRoomLightingTable[1];

/// Three fire-escape endpoints, addressed by 1-based warp IDs 1..3.
extern DirectionWarpEntry gAkropolisFireEscapeWarpEntries[3];

/// Sprite sources, batches and clipping lists for mapped views 1..10.
extern SpriteView gAkropolisFireEscapeSpriteViews[10];

/// Nine cameras selected by mapped indices 1..9, borrowing loaded overlay storage.
extern ViewCamera gAkropolisFireEscapeViewCameras[9];

/// Collision policies and footstep cue bases indexed by surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisFireEscapeSurfacePropertiesByClass[8];

/// Packed `spawnArg1` controls of the fire-escape flare task.
enum {
    AKROPOLIS_FIRE_ESCAPE_FLARE_PULSE_RATE_MASK = 0xFF,
    AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_SHIFT    = 8,
    AKROPOLIS_FIRE_ESCAPE_FLARE_RADIUS_MASK     = 0xFF,
    AKROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT      = 16,
    AKROPOLIS_FIRE_ESCAPE_FLARE_CYAN            = 1 << AKROPOLIS_FIRE_ESCAPE_FLARE_CYAN_SHIFT,
    AKROPOLIS_FIRE_ESCAPE_FLARE_STREAK_PACKETS  = 0x10000000,
    AKROPOLIS_FIRE_ESCAPE_FLARE_RADIAL          = 0x80000000,
};

/// Draws one frame of a pulsing red or cyan flare and retires its effect task.
void akropolisFireEscapeFlareTask(Task* task);

/// Updates and draws the fire-escape lamp's flickering glow, sounding bright transitions.
void akropolisFireEscapeFlickerLightTask(Task* task);

/// Runs the fire escape's telephone save menu and optional statistics panels.
void akropolisFireEscapeTelephoneMenuTask(Task* task);

/// Emits the fire escape's persistent flicker lamp and view-selected one-frame flares.
void akropolisFireEscapeLightEmitterTask(Task* task);

/// Runs the fire escape's room messages and actor interaction check.
void akropolisFireEscapeRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_FIRE_ESCAPE_H
