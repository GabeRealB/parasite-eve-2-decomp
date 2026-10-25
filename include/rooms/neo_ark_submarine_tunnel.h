#ifndef INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
#define INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

enum {
    NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT               = 1,
    NEO_ARK_SUBMARINE_TUNNEL_VIEW_COUNT               = 10,
    NEO_ARK_SUBMARINE_TUNNEL_WARP_COUNT               = 2,
    NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_SET_COUNT = 4,
    NEO_ARK_SUBMARINE_TUNNEL_SURFACE_CLASS_COUNT      = 8,
};

/// Four eight-point patrol rings borrowed by the tunnel's Bog Divers.
///
/// Spawn-argument bits 4..5 select ring 0..3; each waypoint index wraps 0..7.
/// XZ positions and Y swim heights use signed room-coordinate units. Normal
/// patrol adds the Diver's water Y to waypoint Y; the scripted tunnel patrol
/// uses waypoint Y directly. Keep the room overlay loaded through actor teardown.
extern SVECTOR* gNeoArkSubmarineTunnelDiverWaypointSets[NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_SET_COUNT];

/// Water-surface Y sampled by each tunnel Bog Diver at spawn, in room-coordinate units.
///
/// Zero throughout this room's lifetime; the Diver uses it for surfacing and
/// normal patrol heights. The room publishes no live water-height controller.
extern s16 gNeoArkSubmarineTunnelDiverWaterY;

/// Two bodyless water recipes: index 0 refraction, index 1 band distortion.
///
/// Both use priority `TASK_PRIORITY_CONTROL` and zero metadata; there is no terminator. Room-entry
/// dispatch selects index 1. Spawning reads the recipe synchronously; both
/// callbacks borrow the loaded tunnel overlay throughout their task lifetime.
extern TaskDesc gNeoArkSubmarineTunnelWaterRefractionTaskDesc[2];

/// Number of retained placement-variant slots, including absent slots.
enum { NEO_ARK_SUBMARINE_TUNNEL_AREA_VARIANT_COUNT = 13 };

/// Neo Ark submarine tunnel placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variants 1..3 publish actor layouts; variant 4 publishes empty lists.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..12; there is no terminator or bounds check.
extern AreaVariant gNeoArkSubmarineTunnelAreaVariants[NEO_ARK_SUBMARINE_TUNNEL_AREA_VARIANT_COUNT];

/// Lighting for the tunnel's single room, indexed by room minus one.
///
/// Borrows mutable directional and point lights from the loaded room overlay.
/// No ambient table is supplied, so lighting uses the default ambient minima.
/// Keep the overlay loaded while these records or their lights are in use.
extern WorldCoordRoomLighting gNeoArkSubmarineTunnelRoomLighting[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT];

/// Collision resources for the tunnel's single room, indexed by room minus one.
///
/// Borrows the room's writable grid, six view boundaries, eleven action quads
/// and three sight occluders. Setup binds and links these resources; retain the
/// room overlay until the active grid is replaced and all its lists are unlinked.
extern WorldCollisionRoomResources gNeoArkSubmarineTunnelRoomCollisionResources[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT];

/// Logical-view maps for the tunnel's single room, indexed by room minus one.
///
/// Borrows `gViewIdentityMap`: logical views 1..10 select the same camera,
/// image and sprite indices. The identity map's larger capacity does not add
/// tunnel views. Keep this room directory's overlay loaded while using it.
extern u8* gNeoArkSubmarineTunnelViewMaps[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT];

/// Ten logical views for the tunnel's single room, indexed by room minus one.
///
/// Limits reverse lookup in the borrowed identity map independently of its
/// larger capacity. The map directory borrows this array while the room is loaded.
extern ViewCount gNeoArkSubmarineTunnelViewCounts[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT];

/// Tunnel arrival/departure endpoints, indexed by 1-based warp ID minus one.
///
/// Warp 1 faces -X at the forest-zone end and starts view 2; warp 2 faces +X
/// at the pavilion end and starts view 5. Both place the player in scripted
/// control for the entry action to release, and supply no sounds or map flags.
/// Placements use room-coordinate units and yaw uses 4096 units per turn.
/// The stage warp directory borrows both records while this room is loaded.
extern DirectionWarpEntry gNeoArkSubmarineTunnelWarpEntries[NEO_ARK_SUBMARINE_TUNNEL_WARP_COUNT];

/// Ten tunnel cameras, indexed by mapped view minus one (views 1..10).
///
/// World-to-camera matrices use ONE-scaled rotation and negated world origins;
/// projection distances are pixels. The map directory and queued camera tasks
/// borrow these records while the room overlay is loaded.
extern ViewCamera gNeoArkSubmarineTunnelViewCameras[NEO_ARK_SUBMARINE_TUNNEL_VIEW_COUNT];

/// Sprite descriptors for mapped tunnel views 1..10, indexed by view minus one.
///
/// Views 1, 6 and 10 have no source sprites and retain their empty batch address
/// in sources.empty. Every batch list ends with SPRITE_BATCH_END; no view supplies
/// clipping areas. Keep the room and selected view textures loaded during drawing.
extern SpriteView gNeoArkSubmarineTunnelSpriteViews[NEO_ARK_SUBMARINE_TUNNEL_VIEW_COUNT];

/// Collision policies for the tunnel's eight surface classes, indexed by class 0..7.
///
/// Every class blocks probes, permits weapon impacts and applies pushback.
/// Only class 4 supplies footstep cues. Records and cues belong to this room
/// overlay and must remain loaded while collision or footstep queries use them.
extern WorldCollisionSurfaceProperties* gNeoArkSubmarineTunnelSurfaceProperties[NEO_ARK_SUBMARINE_TUNNEL_SURFACE_CLASS_COUNT];

/// Runs an anchored charge disc with player-joint sparks, flicker and a fading ring.
///
/// Requires a coordinate body and owned, zeroed `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1.value` selects tint 0 or 1; the work's parent
/// coordinate and its ancestors must outlive the task. State 0 attaches without
/// drawing; growth emits an adopted spark every fourth active age from player
/// parts 3..18. The owner requests flicker, release or cancel with
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`. Control 1..3 pauses and 4 or above
/// cancels. Teardown releases work and adopted sparks. Keep the player model,
/// installed flying-spark callback, room overlay and effect controller live.
void neoArkSubmarineTunnelRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs the tunnel's animated spark along a step fixed from a target's initial position.
///
/// Requires a composed coordinate body and a counted, zero-initialized
/// `EffectWork` owned through `spawnArg2.pointer`. `spawnArg1.pointer` borrows
/// a target `GfxCoord` until the first active tick, when both cached matrices
/// must be current in the same view space. That tick derives a parent-space
/// step of 204/4096 of the initial separation, with signed 16-bit intermediate
/// components; later ticks move by that step without resampling the target.
/// Drawing occurs on odd ages after initialization, and age 20 releases the
/// work and task. A live `gRoomEffectState` pauses at nonzero effect control
/// and cancels at four or above. The tunnel overlay must remain loaded.
void neoArkSubmarineTunnelRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs the tunnel's expanding orange disc, layered glow and fading outer ring.
///
/// Requires a coordinate body, a live `gRoomEffectState` and a counted,
/// zero-initialized `EffectWork` owned through `spawnArg2.pointer`. Active ticks
/// grow the glow half-extent by 16 world units and refresh a transient orange
/// light. The ring fades first, then the central brightness falls until the
/// work and task are released. Nonzero effect control pauses the task; four
/// or above cancels it. The tunnel overlay must remain loaded.
void neoArkSubmarineTunnelRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Selects the tunnel's glow-disc, flying-spark and projectile-burst effects once.
///
/// Bank-6 slot 0x152 installs the three room effect IDs in state 0 and advances
/// to state 1. Later ticks leave them unchanged and keep the task alive. The
/// tunnel overlay must stay loaded while its selected effects can spawn or run.
void neoArkSubmarineTunnelConfigureEffectsTask(Task* task);

/// Runs the Neo Ark Submarine Tunnel room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and ambience, remain idle between messages, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkSubmarineTunnelRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_H
