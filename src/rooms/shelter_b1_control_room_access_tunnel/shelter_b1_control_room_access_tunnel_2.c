#include "rooms/shelter_b1_control_room_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_01100.h"
#include "actors/actor_02000.h"
#include "actors/actor_02100.h"
#include "actors/actor_02400.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/sound_ids.h"

#include "rooms/room.h"

#include "../../shared/room/room_visual_effects.h"

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

u8* gShelterB1ControlRoomAccessTunnelRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1ControlRoomAccessTunnelRoomViewCounts[1] = { ARRAY_SIZE(gShelterB1ControlRoomAccessTunnelViewCameras) };

DirectionWarpEntry gShelterB1ControlRoomAccessTunnelWarpEntries[2] = {
    {
        .player         = { { .word = 3072 }, 5389, -25, 194 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 5389, -25, 194 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 1024 }, 831, -24, 119 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 831, -24, 119 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL, 3),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

enum {
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GRID_CELLS_X = 2,
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GRID_CELLS_Z = 1,
};

/// Q12 room-axis normals indexed by the tunnel collision faces.
static SVECTOR _gShelterB1ControlRoomAccessTunnelCollisionGridNormals[6] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_normals.inc"
};

/// Room-space collision vertices, in signed world-coordinate units.
static SVECTOR _gShelterB1ControlRoomAccessTunnelCollisionGridVerts[12] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_verts.inc"
};

/// Enclosure and floor quads indexing the tunnel's normal and vertex pools.
static WorldCollisionGridFace _gShelterB1ControlRoomAccessTunnelCollisionGridFaces[6] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_faces.inc"
};

/// Two signed face-index lists, each terminated by WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB1ControlRoomAccessTunnelCollisionGridCells[12] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1ControlRoomAccessTunnelCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..11,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1ControlRoomAccessTunnelCollisionGridCells[(halfwordOffset)])
/// X-major cell directory borrowing the two face-index lists.
static s16* _gShelterB1ControlRoomAccessTunnelCollisionGridTable[SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GRID_CELLS_X * SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GRID_CELLS_Z] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_table.inc"
};
#undef SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1ControlRoomAccessTunnelCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1ControlRoomAccessTunnelCollisionGridNormals,
    .vertices    = _gShelterB1ControlRoomAccessTunnelCollisionGridVerts,
    .faces       = _gShelterB1ControlRoomAccessTunnelCollisionGridFaces,
    .cellFaceIds = _gShelterB1ControlRoomAccessTunnelCollisionGridTable,
    .xBias       = -106,
    .zBias       = 1242,
    .cellCountX  = SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GRID_CELLS_X,
    .cellCountZ  = SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_GRID_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB1ControlRoomAccessTunnelCollisionGridFaces),
};

ViewCamera gShelterB1ControlRoomAccessTunnelViewCameras[3] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3648, 8565, 0 } }, 235 },
    { { { { -16, 0, -4095 }, { -708, 4034, 2 }, { 4034, 708, -16 } }, { -837, 1954, -102 } }, 257 },
    { { { { 32, 0, 4095 }, { 347, 4081, -2 }, { -4081, 347, 32 } }, { -6111, 1435, -85 } }, 246 },
};

/// View 1's decoded-background selector and separate end record.
///
/// The zero first count makes cached linking advance before testing the sentinel.
static SpriteBatch _gShelterB1ControlRoomAccessTunnelView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's decoded-background selector and end record; no source sprites.
static SpriteBatch _gShelterB1ControlRoomAccessTunnelView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's decoded-background selector and end record; no source sprites.
static SpriteBatch _gShelterB1ControlRoomAccessTunnelView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1ControlRoomAccessTunnelSpriteViews[3] = {
    { { .empty = _gShelterB1ControlRoomAccessTunnelView1SpriteBatches }, _gShelterB1ControlRoomAccessTunnelView1SpriteBatches, NULL },
    { { .empty = _gShelterB1ControlRoomAccessTunnelView2SpriteBatches }, _gShelterB1ControlRoomAccessTunnelView2SpriteBatches, NULL },
    { { .empty = _gShelterB1ControlRoomAccessTunnelView3SpriteBatches }, _gShelterB1ControlRoomAccessTunnelView3SpriteBatches, NULL },
};

/// Four all-view point lights with full strength to 1500 units, fading out at 2000.
///
/// Positions are room-space world units; RGB intensities are Q12. Coordinate
/// composition and light selection mutate the records while the room is loaded.
static WorldCoordPointLight _gShelterB1ControlRoomAccessTunnelPointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4469, -223, 260 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1886, -223, 190 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1923, -223, 280 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3158, -223, 433 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
};

WorldCoordRoomLights gShelterB1ControlRoomAccessTunnelRoomLights = {
    .pointLightCount = ARRAY_SIZE(_gShelterB1ControlRoomAccessTunnelPointLights),
    .pointLights     = _gShelterB1ControlRoomAccessTunnelPointLights,
};

WorldCollisionTrigger gShelterB1ControlRoomAccessTunnelViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { 3454, -1167, 45, 0 }, { { -10, -1520, 2263, 0 }, { 11, -1520, -2263, 0 }, { -10, 1520, 2263, 0 }, { 11, 1520, -2263, 0 } }, { -4099, 0, -20, 0 }, { 0, 0, ONE, 0 }, 2721, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3311, -1152, 47, 0 }, { { 27, -1520, -2262, 0 }, { -26, -1520, 2263, 0 }, { 27, 1520, -2262, 0 }, { -26, 1520, 2263, 0 } }, { 4095, 0, 47, 0 }, { 0, 0, ONE, 0 }, 2721, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed departure/arrival warp slots: current slot in bits 4..7, arrival in 0..3.
enum {
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_MAIN_CORRIDOR_WARP_SLOTS = (1 << 4) | 2,
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_CONTROL_ROOM_WARP_SLOTS  = (2 << 4) | 1,
};

WorldCollisionTrigger gShelterB1ControlRoomAccessTunnelWarpTriggers[2] = {
    {
        .origin       = { 5568, -48, 64, 0 },
        .vertices     = { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } },
        .normal       = { 0, ONE, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 1144,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_SHELTER_B1_MAIN_CORRIDOR,
        .parameter1   = SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_MAIN_CORRIDOR_WARP_SLOTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 800, -48, 64, 0 },
        .vertices     = { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } },
        .normal       = { 0, ONE, 0, 0 },
        .facingNormal = { ONE, 0, 0, 0 },
        .radius       = 1144,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_SHELTER_B1_CONTROL_ROOM,
        .parameter1   = SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_CONTROL_ROOM_WARP_SLOTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
    },
};

/// Actor-specific placement selectors used by this room's encounter layouts.
enum {
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM2 = 2,
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM3 = 3,
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_PAWN_PATROL   = 1,
};

/// Variant 1's Watcher and Amoeba file selections and body-task recipes.
///
/// Slot-1 file 102100 and slot-2 file 202400; AREA_PLACEMENT_END ends the list.
static AreaResource _gShelterB1ControlRoomAccessTunnelVariant1Resources[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { 24, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202400Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Watcher and Mossback resources, terminated by AREA_PLACEMENT_END.
///
/// Slot-1 file 102100 and slot-2 file 201100 each spawn body-task index zero.
static AreaResource _gShelterB1ControlRoomAccessTunnelVariant2Resources[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor201100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's slot-1 Mossback resource (file 101100) and end record.
static AreaResource _gShelterB1ControlRoomAccessTunnelVariant3Resources[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's Watcher and Pawn GOLEM resources and end record.
///
/// Slot-1 file 102100 and slot-2 file 202000 each spawn body-task index zero.
static AreaResource _gShelterB1ControlRoomAccessTunnelVariant11Resources[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { 20, 20, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202000PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Present but empty resource list for variant 21; queues no actor files.
static AreaResource _gShelterB1ControlRoomAccessTunnelVariant21Resources[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 1's two Watchers with beam variant 2 and two Amoebas, plus the end record.
///
/// XYZ are world units; yaw uses 4096 units per turn. Watchers use sight row 1.
static AreaPlacement _gShelterB1ControlRoomAccessTunnelVariant1Placements[5] = {
    { 21, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM2, 0, 400, -2000, 800, 1024, 0, 0, 2, 1 },
    { 21, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM2, 0, 400, -2000, -650, 1024, 0, 0, 2, 1 },
    { 24, 0, 0, 2650, 0, -350, 700, 0, 2, 4, 0 },
    { 24, 0, 0, 2250, 0, 650, 1300, 0, 2, 4, 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's two Watchers with beam variant 2 and one standard Mossback, plus the end record.
///
/// World-unit positions and 4096-unit-turn yaw; Watchers use sight row 1.
static AreaPlacement _gShelterB1ControlRoomAccessTunnelVariant2Placements[4] = {
    { 21, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM2, 0, 400, -2000, 800, 1024, 0, 0, 2, 1 },
    { 21, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM2, 0, 400, -2000, -650, 1024, 0, 0, 2, 1 },
    { 11, 0, 0, 2600, 0, 0, 3072, 0, 2, 4, 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's single standard Mossback and end record, using slot-1 texture placement.
///
/// Position is in world units; yaw uses 4096 units per turn.
static AreaPlacement _gShelterB1ControlRoomAccessTunnelVariant3Placements[2] = {
    { 11, 0, 0, 1500, 0, 140, 0, 0, 0, 2, 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 11's two Watchers with beam variant 3 and one patrolling Pawn GOLEM.
///
/// Watchers use sight row 4; the Pawn's variant 4 selects a 4000-world-unit patrol.
/// XYZ are world units; yaw uses 4096 units per turn.
static AreaPlacement _gShelterB1ControlRoomAccessTunnelVariant11Placements[4] = {
    { 21, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM3, 0, 400, -2000, 800, 1024, 0, 0, 2, 4 },
    { 21, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_WATCHER_BEAM3, 0, 400, -2000, -650, 1024, 0, 0, 2, 4 },
    { 20, 4, SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_PAWN_PATROL, 1500, 0, 0, 1024, 0, 2, 4, 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Present but empty placement list for variant 21; spawns no actors.
static AreaPlacement _gShelterB1ControlRoomAccessTunnelVariant21Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB1ControlRoomAccessTunnelAreaVariants[22] = {
    [AREA_DEFAULT_VARIANT] = { _gShelterB1ControlRoomAccessTunnelVariant1Placements, _gShelterB1ControlRoomAccessTunnelVariant1Resources },
    [2]                    = { _gShelterB1ControlRoomAccessTunnelVariant2Placements, _gShelterB1ControlRoomAccessTunnelVariant2Resources },
    [3]                    = { _gShelterB1ControlRoomAccessTunnelVariant3Placements, _gShelterB1ControlRoomAccessTunnelVariant3Resources },
    [11]                   = { _gShelterB1ControlRoomAccessTunnelVariant11Placements, _gShelterB1ControlRoomAccessTunnelVariant11Resources },
    [21]                   = { _gShelterB1ControlRoomAccessTunnelVariant21Placements, _gShelterB1ControlRoomAccessTunnelVariant21Resources },
};

enum {
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FOOTSTEP_WALK_SCRIPT = 0x5D,
    SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FOOTSTEP_RUN_SCRIPT  = 0x5F,
};

/// Surface 1's walk, run and scripted-jump cue bases in the loaded type-1 bank.
///
/// Cue 2 uses the base; cue 1 adds one, and companion playback adds 100.
static WorldCollisionFootstepSounds _gShelterB1ControlRoomAccessTunnelFootstepSounds = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FOOTSTEP_WALK_SCRIPT,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FOOTSTEP_RUN_SCRIPT,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL_FOOTSTEP_WALK_SCRIPT,
};

/// Probe-blocking surface with weapon impacts and pushback, without footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1ControlRoomAccessTunnelSilentSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Probe-blocking floor with weapon impacts, pushback and the room's footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1ControlRoomAccessTunnelFootstepSurface = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1ControlRoomAccessTunnelFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1ControlRoomAccessTunnelSurfacePropertiesByClass[8] = {
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
    &_gShelterB1ControlRoomAccessTunnelFootstepSurface,
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
    &_gShelterB1ControlRoomAccessTunnelSilentSurface,
};

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

void shelterB1ControlRoomAccessTunnelRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void shelterB1ControlRoomAccessTunnelRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
