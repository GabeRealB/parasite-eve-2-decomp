#ifndef GAMEPLAY_ROOM_H
#define GAMEPLAY_ROOM_H

#include "common.h"

#include "gameplay/light.h"

struct _GpGridParams;
struct _GpObj3A;
struct _GpObj4C;

/// Entry in a room's table of minimum ambient light levels.
///
/// Entry zero holds `viewCount`; entries 1..viewCount hold each view's colour.
/// RGB levels use 16 units per 8-bit colour level, matching the GTE back colour.
/// Lighting clamps the model colour matrix's ambient term to these minima.
/// The room overlay owns the table, referenced by `GpRoomCoordRec.field_4`;
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

/// A room view's own lights, returned by `Gp_GetRoomCoordSet`
/// (`GpRoomCoordRec.field_0`): its directional, point and spot lights.
/// `Gp_UpdateRoomCoords` parents each light to `gGfxViewCoord` on first run,
/// builds each spot light's orientation from its `axis`, then updates them every
/// frame via `Gp_UpdateCoordEx`.
typedef struct _GpRoomCoordSet {
    /* 0x00 */ s32                   n58;
    /* 0x04 */ WorldCoordLight*      arr58; // directional lights
    /* 0x08 */ s32                   n60;
    /* 0x0C */ WorldCoordPointLight* arr60; // point lights
    /* 0x10 */ s32                   n6C;
    /* 0x14 */ WorldCoordSpotLight*  arr6C; // spot lights
} GpRoomCoordSet;
STATIC_ASSERT_SIZEOF(GpRoomCoordSet, 0x18);

/// 8-byte record in tables pointed to by `Gp_RoomCoordTables`. Indexed 1-based
/// by `GameLocationKey.room`. `Gp_GetRoomCoordRec` returns the record (or NULL).
/// `Gp_GetRoomCoordSet` returns `field_0`, the room view's lights (or NULL).
/// `Gp_GetRoomBound` walks `field_4` as a `WorldCoordRoomAmbientEntry` table, falling
/// back to `Gp_RoomBoundDefault`.
typedef struct _GpRoomCoordRec {
    /* 0x0 */ GpRoomCoordSet*             field_0;
    /* 0x4 */ WorldCoordRoomAmbientEntry* field_4;
} GpRoomCoordRec;
STATIC_ASSERT_SIZEOF(GpRoomCoordRec, 8);

/// Independent policies stored in the surface record's three flag bytes.
enum {
    WORLD_COLLISION_SURFACE_BLOCK_PROBES          = 0,
    WORLD_COLLISION_SURFACE_PASS_PROBES           = 1,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS = 0,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS  = 1,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK        = 0,
    WORLD_COLLISION_SURFACE_SUPPRESS_PUSHBACK     = 1
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
/// `Gp_LinkRoomObjects` / `Gp_LinkRoomObjectsSpawn` parent `field_0` to `&gGfxViewCoord` and
/// link the `field_4` / `field_8` (`GpObj4A`) and `field_C` (`GpObj3A`) arrays.
typedef struct _GpRoomObjRec {
    /* 0x0 */ struct _GpGridParams* field_0;
    /* 0x4 */ struct _GpObj4C*      field_4;
    /* 0x8 */ struct _GpObj4C*      field_8;
    /* 0xC */ struct _GpObj3A*      field_C;
} GpRoomObjRec;
STATIC_ASSERT_SIZEOF(GpRoomObjRec, 0x10);

/// Per-stage wrapper. `field_0` is an array of `GpRoomObjRec*`, indexed
/// 1-based by `GameSession.location.loc.area` / `GameLocationKey.area`.
typedef struct _GpRoomObjTbl {
    /* 0x0 */ GpRoomObjRec** field_0;
} GpRoomObjTbl;

#endif // GAMEPLAY_ROOM_H
