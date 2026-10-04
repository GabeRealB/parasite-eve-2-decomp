#ifndef GAMEPLAY_ROOM_H
#define GAMEPLAY_ROOM_H

#include "common.h"

#include "gameplay/light.h"

struct WorldCollisionGrid;
struct WorldCollisionOccluder;
struct WorldCollisionTrigger;

/// Entry in a room's table of minimum ambient light levels.
///
/// Entry zero holds `viewCount`; entries 1..viewCount hold each view's colour.
/// RGB levels use 16 units per 8-bit colour level, matching the GTE back colour.
/// Lighting clamps the model colour matrix's ambient term to these minima.
/// The room overlay owns the table, referenced by `WorldCoordRoomLighting.ambientTable`;
/// a missing table or view beyond its count uses `Gp_RoomBoundDefault`.
typedef union {
    struct {
        s16 r;     // Minimum red ambient level.
        s16 g;     // Minimum green ambient level.
        s16 b;     // Minimum blue ambient level.
        s16 luma;  // Stored weighted brightness (3*r + 4*g + b)/8; unused by lighting.
    } color;
    s16 viewCount; // Header only: number of view entries following entry zero.
} WorldCoordRoomAmbientEntry;
STATIC_ASSERT_SIZEOF(WorldCoordRoomAmbientEntry, 8);

/// A room's directional, point and cone lights for model shading and light queries.
///
/// The loaded room overlay owns these contiguous arrays; the room table selects
/// this collection independently of the view. Each light's view filter controls
/// its contribution. Counts are non-negative element counts, with (0, NULL) for
/// an absent array; retained storage after a live array is outside its count.
///
/// Elements stay writable: coordinate updates parent them to `gGfxViewCoord`,
/// initialize cone orientations and compose their transforms; lighting queries
/// overwrite attenuation. The record, arrays and borrowed parent remain live
/// while used, and room pointers must not outlive the loaded overlay.
typedef struct {
    s32                   directionalLightCount; // Number of directional-light elements.
    WorldCoordLight*      directionalLights;     // Borrowed mutable directional-light array, or NULL.
    s32                   pointLightCount;       // Number of point-light elements.
    WorldCoordPointLight* pointLights;           // Borrowed mutable point-light array, or NULL.
    s32                   coneLightCount;        // Number of live cone-light elements.
    WorldCoordSpotLight*  coneLights;            // Borrowed mutable cone-light array, or NULL.
} WorldCoordRoomLights;
STATIC_ASSERT_SIZEOF(WorldCoordRoomLights, 0x18);

/// A room's light collection and minimum ambient colours by view.
///
/// `Gp_RoomCoordTables` selects a stage's area table, then an area's room array.
/// Lookups require valid 1-based `GameLocationKey` stage, area and room indices;
/// each containing table determines its own extent. Ambient lookups require a
/// 1-based view: entry zero holds `viewCount`, followed by colours at entries
/// 1..viewCount. A missing record/table or a view beyond that count uses
/// `Gp_RoomBoundDefault`.
///
/// The collection and ambient table are borrowed from the loaded room overlay.
/// Room scripts may replace `lights`; its light arrays remain writable for
/// coordinate updates and shading queries. Pointers to room-overlay records or
/// data must not survive unloading that overlay.
typedef struct {
    WorldCoordRoomLights*             lights;       // Borrowed mutable room light collection, or NULL for no room lights.
    const WorldCoordRoomAmbientEntry* ambientTable; // Borrowed read-only ambient table, or NULL for the default minima.
} WorldCoordRoomLighting;
STATIC_ASSERT_SIZEOF(WorldCoordRoomLighting, 8);

/// Independent policies stored in the surface record's three flag bytes.
enum {
    /// Makes a surface block segment probes and clipped capsule contacts.
    ///
    /// Stored in `WorldCollisionSurfaceProperties.probePassThrough`: zero
    /// blocks, and every nonzero value passes. Capsule grid tests honor this
    /// policy only with `WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT`; otherwise
    /// they collect contacts with either policy. Projectile handlers also use
    /// blocking contacts to end flight or select their impact response.
    /// Pushback and weapon hit effects have independent surface policies.
    WORLD_COLLISION_SURFACE_BLOCK_PROBES = 0,
    /// Lets segment probes and grid-clipped capsules pass through a surface.
    ///
    /// Canonical nonzero value for the byte
    /// `WorldCollisionSurfaceProperties.probePassThrough`; readers accept any
    /// nonzero value. Segment queries skip these surfaces. Capsules omit their
    /// contacts and remain unshortened only with
    /// `WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT`; unclipped capsules, spheres
    /// and floor queries still record contacts.
    /// Projectile handlers skip their blocking-surface response; grenades retain
    /// separate actor-hit, timeout and scripted detonation rules. Pushback,
    /// weapon hit effects and footstep cues have independent surface policies.
    WORLD_COLLISION_SURFACE_PASS_PROBES = 1,
    /// Suppresses weapon hit effects and grenade detonation at surface contacts.
    ///
    /// Zero value for `WorldCollisionSurfaceProperties.weaponImpactEnabled`;
    /// every nonzero value enables the response. Weapon hit effects skip these
    /// contacts independently of probe passage and pushback. Grenades consult
    /// this policy only at blocking surfaces: zero selects their exit state,
    /// with movement and the flight-timeout check still run in that update.
    /// Actor hits and the scripted pass-through detonation bypass this policy.
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS = 0,
    /// Allows weapon hit effects and grenade detonation at surface contacts.
    ///
    /// Stored in the byte `WorldCollisionSurfaceProperties.weaponImpactEnabled`;
    /// readers treat any nonzero value as enabled. Weapon hit effects use this
    /// policy independently of probe passage and pushback. Grenades consult it
    /// only on surfaces with `WORLD_COLLISION_SURFACE_BLOCK_PROBES`; actor hits,
    /// flight timeouts and the scripted pass-through detonation are separate.
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS = 1,
    /// Enables displacement calculation from this surface class's grid contacts.
    ///
    /// Stored in `WorldCollisionSurfaceProperties.suppressPushback` and cached
    /// by surface class (0..7) in `Gp_RoomParams`. Ordinary, floor and edge
    /// responses honor this zero value. Suppressed contacts still appear in
    /// surface masks and count as hits.
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK    = 0,
    WORLD_COLLISION_SURFACE_SUPPRESS_PUSHBACK = 1
};

/// A footstep base that suppresses playback before either cue offset is added.
enum { WORLD_COLLISION_FOOTSTEP_SILENT = 0 };

/// Paired footstep sound-script requests for one collision surface.
///
/// Each field is a 32-bit encoded request accepted by `sndEvtRequestScriptStart`.
/// Room records use type-1 requests, which select the currently loaded bank.
/// Standard playback uses the base for animation cue 2 and base + 1 for cue 1,
/// then adds 100 to select a companion's entries. A zero base is silent and
/// receives neither offset. Scripted jumps take precedence over running.
/// The loaded room overlay owns the storage; surface records borrow it without
/// modifying it and must not retain it after that overlay is unloaded.
typedef struct {
    s32 walk;         // Default cue-2 base outside running and scripted jumps (0 silent).
    s32 run;          // Running cue-2 base (movementMode 3; 0 silent).
    s32 scriptedJump; // Scripted jump cue-2 base (actor mode 2, state 3; 0 silent).
} WorldCollisionFootstepSounds;
STATIC_ASSERT_SIZEOF(WorldCollisionFootstepSounds, 12);

/// Collision and footstep properties of one room-local surface class.
///
/// `Gp_RoomParamTables` selects the loaded room overlay's records by stage - 1,
/// area - 1, then surface class (0..7). Grid faces and grid-contact keys carry
/// that class; actors retain it for footstep cues. Records and their sound
/// tables remain borrowed only while the room overlay is loaded.
/// `suppressPushback` is also cached in `Gp_RoomParams` for collision response.
typedef struct {
    u8                                  field_0;             // Stored as 0 or 1; no reader established, role unproven.
    u8                                  probePassThrough;    // Probe/projectile passage (0 blocks, 1 passes).
    u8                                  weaponImpactEnabled; // Surface hit effects and grenade detonation (0 disabled, 1 enabled).
    u8                                  suppressPushback;    // Pushback from grid contacts (0 apply, 1 suppress).
    const WorldCollisionFootstepSounds* footstepSounds;      // Borrowed cue bases, or NULL for no surface footstep cues.
} WorldCollisionSurfaceProperties;
STATIC_ASSERT_SIZEOF(WorldCollisionSurfaceProperties, 8);

/// A room's collision grid, view boundaries, action triggers and sight occluders.
///
/// Stage and area tables select a room array using valid 1-based
/// `GameLocationKey` indices; each containing table determines its own extent.
/// Descriptors reside in the stage-map or room overlay and borrow mutable
/// resources from the loaded room overlay. Different rooms may share resources.
/// Keep the grid and its pools alive while active, and the trigger and occluder
/// storage alive until unlinked. Access resources only while that room overlay
/// is loaded.
///
/// Each pointer may be NULL independently. Non-NULL trigger and occluder arrays
/// include a final record with `WORLD_COLLISION_TRIGGER_LAST` or
/// `WORLD_COLLISION_OCCLUDER_LAST` set. Setup links and enables that record before
/// stopping, and binds the grid and trigger coordinates to the current view.
/// These array markers are independent of the runtime lists' NULL next links.
typedef struct {
    struct WorldCollisionGrid*     grid;                 // Borrowed mutable collision mesh and cell index, or NULL.
    struct WorldCollisionTrigger*  viewBoundaryTriggers; // Borrowed mutable source/destination view boundaries, or NULL.
    struct WorldCollisionTrigger*  actionTriggers;       // Borrowed mutable interaction and transition triggers, or NULL.
    struct WorldCollisionOccluder* occluders;            // Borrowed mutable line-of-sight blocking quads, or NULL.
} WorldCollisionRoomResources;
STATIC_ASSERT_SIZEOF(WorldCollisionRoomResources, 0x10);

/// A stage's directory of per-area collision room resources.
///
/// `areaRooms[area - 1][room - 1]` is the `WorldCollisionRoomResources` that
/// room setup links for a 1-based `GameLocationKey` area and room. Areas
/// without a room folder have NULL entries, which leave the room with no grid,
/// triggers or occluders. Neither level stores a count or terminator, so
/// lookups require a valid 1-based area within the stage's directory and, for
/// a populated area, a room inside that area's array.
///
/// The stage map overlay owns this record and its area directory, and borrows
/// each room overlay's resource array, which stays valid only while that room
/// is loaded. Consumers only read through this record; it allocates and
/// releases nothing.
typedef struct {
    WorldCollisionRoomResources** areaRooms; // Borrowed per-area room-resource arrays, indexed by area - 1; NULL when that area has none
} WorldCollisionStageResources;
STATIC_ASSERT_SIZEOF(WorldCollisionStageResources, 4);

#endif // GAMEPLAY_ROOM_H
