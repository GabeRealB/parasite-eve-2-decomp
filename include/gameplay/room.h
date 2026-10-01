#ifndef GAMEPLAY_ROOM_H
#define GAMEPLAY_ROOM_H

#include "common.h"

#include "gameplay/light.h"

struct WorldCollisionGrid;
struct _GpObj3A;
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
    WORLD_COLLISION_SURFACE_BLOCK_PROBES          = 0,
    WORLD_COLLISION_SURFACE_PASS_PROBES           = 1,
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
/// Each field is a 32-bit encoded request accepted by `SndEvt_EnqueueType6`.
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

/// 0x10-byte per-room record in tables pointed to by `Gp_RoomObjTables`.
/// Indexed 1-based by `GameSession.location.loc.room` / `GameLocationKey.room`.
/// `Gp_LinkRoomObjects` / `Gp_LinkRoomObjectsSpawn` bind the grid's `viewCoord` to `&gGfxViewCoord` and
/// link the `field_4` / `field_8` (`WorldCollisionTrigger`) and `field_C` (`GpObj3A`) arrays.
typedef struct _GpRoomObjRec {
    /* 0x0 */ struct WorldCollisionGrid*    field_0;
    /* 0x4 */ struct WorldCollisionTrigger* field_4;
    /* 0x8 */ struct WorldCollisionTrigger* field_8;
    /* 0xC */ struct _GpObj3A*              field_C;
} GpRoomObjRec;
STATIC_ASSERT_SIZEOF(GpRoomObjRec, 0x10);

/// Per-stage wrapper. `field_0` is an array of `GpRoomObjRec*`, indexed
/// 1-based by `GameSession.location.loc.area` / `GameLocationKey.area`.
typedef struct _GpRoomObjTbl {
    /* 0x0 */ GpRoomObjRec** field_0;
} GpRoomObjTbl;

#endif // GAMEPLAY_ROOM_H
