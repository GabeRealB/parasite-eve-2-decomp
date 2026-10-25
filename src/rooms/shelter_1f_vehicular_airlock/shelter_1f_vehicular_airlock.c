#include "rooms/shelter_1f_vehicular_airlock.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_02000.h"
#include "actors/actor_05700.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/room/room_events.h"
#include "../../shared/effects/glow_draw.h"

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and event the message handler latched for the room's event
/// task.
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void _shelter1fVehicularAirlockDrawPulsingLight(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale, s32 packedColor);

static s32 _shelter1fVehicularAirlockResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelter1fVehicularAirlockRejectKeyItem(Task* receiver, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _shelter1fVehicularAirlockHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32 _shelter1fVehicularAirlockIgnoreDirectionAction(Task* receiver, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static u32     _gShelter1fVehicularAirlockModel03A58PartVerts[1];
static SVECTOR _gShelter1fVehicularAirlockModel03A58Verts[116];
static TmdBone _gShelter1fVehicularAirlockModel03A58Skeleton[1];
static u32     _gShelter1fVehicularAirlockModel03A58Stream[1019];

static WorldCollisionGrid         _gShelter1fVehicularAirlockCollisionGrid;
static WorldCollisionTrigger      _gShelter1fVehicularAirlockViewBoundaryTriggers[2];
static WorldCollisionTrigger      _gShelter1fVehicularAirlockActionTriggers[7];
static WorldCoordRoomAmbientEntry _gShelter1fVehicularAirlockViewAmbientLights[4];
static WorldCoordRoomLights       _gShelter1fVehicularAirlockRoomLights;

static TmdBone _gShelter1fVehicularAirlockModel03A58Skeleton[1] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_skeleton.inc"
};

static u32 _gShelter1fVehicularAirlockModel03A58PartVerts[1] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_partVerts.inc"
};

static SVECTOR _gShelter1fVehicularAirlockModel03A58Verts[116] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_verts.inc"
};

static u32 _gShelter1fVehicularAirlockModel03A58Stream[1019] = {
#include "assets/shelter_1f_vehicular_airlock_model_03A58_stream.inc"
};

TmdSource gShelter1fVehicularAirlockModel03A58 = {
    0,
    6672,
    0,
    1,
    _gShelter1fVehicularAirlockModel03A58PartVerts,
    _gShelter1fVehicularAirlockModel03A58Verts,
    &_gShelter1fVehicularAirlockModel03A58Verts[116],
    _gShelter1fVehicularAirlockModel03A58Skeleton,
    _gShelter1fVehicularAirlockModel03A58Stream,
};

/// Bodyless priority-32 task recipe for a latched departure's CAP, sound and reload.
///
/// Borrows the room's singleton event records and fade storage until reload.
static TaskDesc _gShelter1fVehicularAirlockStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Room receiver's transition, key-item, direction-action and CAP-command handlers.
///
/// The final sentinel makes unsupported IDs return zero. Installed tasks borrow
/// this table and its callbacks while the room overlay remains loaded.
static TaskMessageEntry _gShelter1fVehicularAirlockRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fVehicularAirlockResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fVehicularAirlockRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fVehicularAirlockIgnoreDirectionAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fVehicularAirlockHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-space fixture endpoints and lamp centres for the room's light glows.
///
/// Entries 0..11 are six endpoint pairs, 12..13 another pair, and 14..17
/// individual lamps. View 3 uses this base; view 2 uses the suffix at entry 2.
/// The glow drawers borrow consecutive endpoints or one centre per call.
static SVECTOR _gShelter1fVehicularAirlockLightPoints[18] = {
    { -0x2774, -3090, 1790, 0 },
    { -8900, -3090, 1790, 0 },
    { -6600, -3090, 1790, 0 },
    { -5400, -3090, 1790, 0 },
    { -3100, -3090, 1790, 0 },
    { -1900, -3090, 1790, 0 },
    { -0x2774, -3090, -1790, 0 },
    { -8900, -3090, -1790, 0 },
    { -6600, -3090, -1790, 0 },
    { -5400, -3090, -1790, 0 },
    { -3100, -3090, -1790, 0 },
    { -1900, -3090, -1790, 0 },
    { -9400, -1960, -2250, 0 },
    { -8600, -1960, -2250, 0 },
    { -5000, -2130, 2240, 0 },
    { -0x28AA, -3490, 100, 0 },
    { -0x2882, -3610, 900, 0 },
    { -0x2882, -3610, -900, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gShelter1fVehicularAirlockCollisionResources[1] = {
    { &_gShelter1fVehicularAirlockCollisionGrid, _gShelter1fVehicularAirlockViewBoundaryTriggers, _gShelter1fVehicularAirlockActionTriggers, NULL },
};

WorldCoordRoomLighting gShelter1fVehicularAirlockRoomLighting[1] = {
    { &_gShelter1fVehicularAirlockRoomLights, _gShelter1fVehicularAirlockViewAmbientLights },
};

u8* gShelter1fVehicularAirlockViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelter1fVehicularAirlockViewCounts[1] = { 3 };

/// Door descriptor IDs and the optional marker set on Bulwark-door arrival.
enum {
    SHELTER_1F_VEHICULAR_AIRLOCK_WARP_AIRLOCK     = 1,
    SHELTER_1F_VEHICULAR_AIRLOCK_WARP_GUARDROOM   = 2,
    SHELTER_1F_VEHICULAR_AIRLOCK_WARP_BULWARK     = 3,
    SHELTER_1F_VEHICULAR_AIRLOCK_BULWARK_MAP_FLAG = 428,
};

DirectionWarpEntry gShelter1fVehicularAirlockWarpEntries[3] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -5000, 0, 1570 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -4960, 0, 1380 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 2), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -8900, 0, -1730 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -4960, 0, 1380 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 2), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 1), DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2 }, -10000, 0, 0 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2 }, -10000, 0, 0 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 4), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 3), DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, SHELTER_1F_VEHICULAR_AIRLOCK_BULWARK_MAP_FLAG },
};

/// Unit normals for the vehicular airlock collision grid, with 4096 per unit.
static SVECTOR _gShelter1fVehicularAirlockCollisionGridNormals[10] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_normals.inc"
};

/// Room-space vertices of the vehicular airlock collision grid, in game units.
static SVECTOR _gShelter1fVehicularAirlockCollisionGridVerts[33] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_verts.inc"
};

/// Indexed collision quads; the main floor uses surface class 1 and the others class 0.
static WorldCollisionGridFace _gShelter1fVehicularAirlockCollisionGridFaces[12] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_faces.inc"
};

/// Packed signed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelter1fVehicularAirlockCollisionGridCells[52] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_cells.inc"
};

// The generated table supplies element offsets into the packed cell lists.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fVehicularAirlockCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..51,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_VEHICULAR_AIRLOCK_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fVehicularAirlockCollisionGridCells[(halfwordOffset)])
/// X-major 3-by-3 cell directory; NULL entries select empty cells.
static s16* _gShelter1fVehicularAirlockCollisionGridTable[9] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_table.inc"
};
#undef SHELTER_1F_VEHICULAR_AIRLOCK_CELL_FACE_LIST_AT_OFFSET

/// Vehicular airlock's room-space collision mesh and 3-by-3 spatial index.
///
/// Cells span 4000 game units from origin (-11500, -3000) on X/Z. Room setup
/// binds the mutable descriptor to the view coordinate while this overlay is live.
static WorldCollisionGrid _gShelter1fVehicularAirlockCollisionGrid = {
    NULL,
    _gShelter1fVehicularAirlockCollisionGridNormals,
    _gShelter1fVehicularAirlockCollisionGridVerts,
    _gShelter1fVehicularAirlockCollisionGridFaces,
    _gShelter1fVehicularAirlockCollisionGridTable,
    11500,
    3000,
    3,
    3,
    4000,
    ARRAY_SIZE(_gShelter1fVehicularAirlockCollisionGridFaces),
};

ViewCamera gShelter1fVehicularAirlockViewCameras[3] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 723, 0, -4031 }, { -887, 3995, -159 }, { 3932, 901, 706 } }, { 9160, 2690, 790 } }, 257 },
    { { { { 596, 0, 4052 }, { -95, 4094, 14 }, { -4051, -97, 596 } }, { 1760, 1260, 790 } }, 257 },
};

/// View 1's empty sprite range and terminator; the zero count selects image strips.
static SpriteBatch _gShelter1fVehicularAirlockView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite range and terminator; the zero count selects image strips.
static SpriteBatch _gShelter1fVehicularAirlockView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite range and terminator; the zero count selects image strips.
static SpriteBatch _gShelter1fVehicularAirlockView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelter1fVehicularAirlockSpriteViews[3] = {
    { { .empty = _gShelter1fVehicularAirlockView1SpriteBatches }, _gShelter1fVehicularAirlockView1SpriteBatches, NULL },
    { { .empty = _gShelter1fVehicularAirlockView2SpriteBatches }, _gShelter1fVehicularAirlockView2SpriteBatches, NULL },
    { { .empty = _gShelter1fVehicularAirlockView3SpriteBatches }, _gShelter1fVehicularAirlockView3SpriteBatches, NULL },
};

/// Five all-view model-shading lights with Q12 RGB intensities and world-unit radii.
///
/// Coordinate setup and shading queries update their parents, caches and attenuation.
static WorldCoordPointLight _gShelter1fVehicularAirlockPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5000, -2130, 1990 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 1638, 1638 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2500, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6000, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9500, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9000, -1635, -1950 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 500, 1000 },
};

/// Model-shading light collection: five point lights, with no directional or cone lights.
static WorldCoordRoomLights _gShelter1fVehicularAirlockRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gShelter1fVehicularAirlockPointLights),
    _gShelter1fVehicularAirlockPointLights,
    0,
    NULL,
};

/// Reciprocal view-2/view-3 crossing quads, ending at the second record's LAST flag.
///
/// Room setup binds, links and enables these mutable triggers until room teardown.
static WorldCollisionTrigger _gShelter1fVehicularAirlockViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { -5709, -3248, -16, 0 }, { { -255, -3712, -3633, 0 }, { 254, -3712, 3632, 0 }, { -255, 3712, -3633, 0 }, { 254, 3712, 3632, 0 } }, { 4087, 0, -287, 0 }, { 0, 0, 4096, 0 }, 5196, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5519, -3312, -32, 0 }, { { 238, -3712, 3884, 0 }, { -245, -3712, -3892, 0 }, { 238, 3712, 3884, 0 }, { -245, 3712, -3892, 0 } }, { -4090, 0, 253, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room response command carried by the three message-routed interaction regions.
enum { SHELTER_1F_VEHICULAR_AIRLOCK_ROOM_COMMAND_RESPONSE = 3 };

/// Three door warps and four CAP/room-command regions, ending at the LAST flag.
///
/// Warp parameters encode destination area and departure/arrival slots. CAP
/// regions send room command 3 except the direct CAP command 1 region.
/// Room setup binds, links and enables these mutable triggers until teardown.
static WorldCollisionTrigger _gShelter1fVehicularAirlockActionTriggers[7] = {
    { NULL, NULL, NULL, { -4800, -48, 2080, 0 }, { { -576, 0, -416, 0 }, { 576, 0, -416, 0 }, { -576, 0, 416, 0 }, { 576, 0, 416, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 709, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_AIRLOCK, (SHELTER_1F_VEHICULAR_AIRLOCK_WARP_AIRLOCK << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8864, -48, -2112, 0 }, { { -800, 0, -416, 0 }, { 800, 0, -416, 0 }, { -800, 0, 416, 0 }, { 800, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_GUARDROOM, (SHELTER_1F_VEHICULAR_AIRLOCK_WARP_GUARDROOM << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x2820, -48, 16, 0 }, { { 416, 0, -2512, 0 }, { 416, 0, 2512, 0 }, { -416, 0, -2512, 0 }, { -416, 0, 2512, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 2534, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_BULWARK, (SHELTER_1F_VEHICULAR_AIRLOCK_WARP_BULWARK << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9072, -64, 704, 0 }, { { -1520, 0, -928, 0 }, { 1520, 0, 96, 0 }, { -1520, 0, -96, 0 }, { 1520, 0, 928, 0 } }, { 0, 4116, 0, 0 }, { 1380, 0, -3857, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_VEHICULAR_AIRLOCK_ROOM_COMMAND_RESPONSE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7168, -64, 1736, 0 }, { { -416, 0, -936, 0 }, { 416, 0, -648, 0 }, { -416, 0, 792, 0 }, { 416, 0, 792, 0 } }, { 0, 4100, 0, 0 }, { 4052, 0, 601, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_VEHICULAR_AIRLOCK_ROOM_COMMAND_RESPONSE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1920, -64, 0, 0 }, { { 416, 0, -2512, 0 }, { 416, 0, 2512, 0 }, { -416, 0, -2512, 0 }, { -416, 0, 2512, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2534, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7312, -64, 1072, 0 }, { { -880, 0, -1080, 0 }, { 880, 0, -696, 0 }, { -880, 0, 840, 0 }, { 880, 0, 936, 0 } }, { 0, 4097, 0, 0 }, { 3166, 0, -2599, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_VEHICULAR_AIRLOCK_ROOM_COMMAND_RESPONSE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// View-count header and minimum ambient RGB levels for logical views 1..3.
///
/// RGB and stored luma use 16 units per 8-bit colour level; queries read RGB only.
static WorldCoordRoomAmbientEntry _gShelter1fVehicularAirlockViewAmbientLights[4] = {
    { .viewCount = ARRAY_SIZE(_gShelter1fVehicularAirlockViewAmbientLights) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 700, 700, 700, 700 } },
    { .color = { 700, 700, 700, 700 } },
};

/// Default placement variant's Pawn/Rook GOLEM file selections and task recipes.
///
/// Placement keys 20 and 57 use descriptor 0 in their actor tables. The final
/// AREA_PLACEMENT_END record terminates the loader's scan. Actor task tables
/// remain borrowed from their loaded overlays.
static AreaResource _gShelter1fVehicularAirlockActorResources[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102000PawnGolemTasks },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gShelter1fVehicularAirlockAreaVariants[SHELTER_1F_VEHICULAR_AIRLOCK_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkVehicularAirlockDefaultPlacements, .resources = _gShelter1fVehicularAirlockActorResources },
};

/// Surface-1 walking, running and scripted-jump cue bases in the loaded type-1 bank.
///
/// Cue 1 uses the next entry; companion playback adds 100 to the base.
static WorldCollisionFootstepSounds _gShelter1fVehicularAirlockSurface1FootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
};

/// Blocking, impact-enabled surface with pushback and no footsteps (classes 0, 2..7).
static WorldCollisionSurfaceProperties _gShelter1fVehicularAirlockSilentSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Blocking, impact-enabled floor surface with pushback and class-1 footstep cues.
static WorldCollisionSurfaceProperties _gShelter1fVehicularAirlockSurface1 = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelter1fVehicularAirlockSurface1FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelter1fVehicularAirlockSurfaceProperties[8] = {
    &_gShelter1fVehicularAirlockSilentSurface,
    &_gShelter1fVehicularAirlockSurface1,
    &_gShelter1fVehicularAirlockSilentSurface,
    &_gShelter1fVehicularAirlockSilentSurface,
    &_gShelter1fVehicularAirlockSilentSurface,
    &_gShelter1fVehicularAirlockSilentSurface,
    &_gShelter1fVehicularAirlockSilentSurface,
    &_gShelter1fVehicularAirlockSilentSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest departure-event gate call spawned its staged task (0 no, 1 yes).
///
/// Every gate call clears it, including queries; this byte does not track task lifetime.
static s8 _gShelter1fVehicularAirlockEventStarted = 0;

/// Retained, unreferenced bytes after the event-start byte; their role and grouping are unproven.
static u8 D_shelter_1f_vehicular_airlock_80182AB1 = 2;

static u8 D_shelter_1f_vehicular_airlock_80182AB2 = 240;

static u8 D_shelter_1f_vehicular_airlock_80182AB3 = 65;

RoomLatchedEvent gRoomEventLatched = { 0 };

static void _shelter1fVehicularAirlockInitializeRoom(Task* task);
static void _shelter1fVehicularAirlockIdleMessageTask(Task* unusedTask);

static void _glowDrawAngledCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

void shelter1fVehicularAirlockUpdatePlacedModelVisibilityTask(Task* task)
{
    enum { OBJECT_STATE_HIDDEN = 2 };
    TmdObject*   model        = task->extra.tmd;
    const Enemy* placedObject = task->spawnArg2.pointer;

    if (areaGetCurrentObjectState((u8)placedObject->placeKey) == OBJECT_STATE_HIDDEN) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

#include "../../shared/room/room_event_staged_task.inc.c"

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 when the room handles the departure, including an eligible query;
/// returns 1 for ordinary departure when a nonzero event flag is already set.
/// Every call clears the latest-start byte. Only `ROOM_EVENT_EXECUTE` copies
/// both records, writes 1 to a nonzero flag and raises that byte after spawning.
/// Borrows complete records for this call; flag IDs must be 0..503. The room's
/// singleton copies and CAP/sound resources must remain live until its task ends;
/// do not latch another event while that task still uses them.
static inline s32 _shelter1fVehicularAirlockStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    _gShelter1fVehicularAirlockEventStarted = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *message;
            gRoomEventLatched             = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelter1fVehicularAirlockStagedEventTaskDesc, 0, 0, 0);
            _gShelter1fVehicularAirlockEventStarted = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

/// Resolves destination variants and gates the airlock's Bulwark and airlock scenes.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`, borrowing complete eight-byte request
/// and writable reply records, which may alias. A locked Bulwark returns 0;
/// executing that request also marks its optional trigger flag and starts the
/// blocked-door CAP command. Unseen departure scenes return 2, latching only
/// on execution; other destinations and seen scenes return 1. Keep the map and
/// room resources loaded while a staged scene uses the copied records.
static s32 _shelter1fVehicularAirlockResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        TRANSITION_REFUSED      = 0,
        TRANSITION_DIRECT       = 1,
        BLOCKED_TRIGGER_MARK    = 2,
        CAP_BULWARK_BLOCKED     = 2,
        CAP_BULWARK_DEPARTURE   = 4,
        CAP_AIRLOCK_DEPARTURE   = 6,
        BULWARK_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 3),
        AIRLOCK_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, 1),
        DEPARTURE_FADE_NONE     = 0
    };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_1F_BULWARK) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, BLOCKED_TRIGGER_MARK);
                capRunCommandWithTransition(CAP_BULWARK_BLOCKED);
            }
            return TRANSITION_REFUSED;
        }
        event.capCmd   = CAP_BULWARK_DEPARTURE;
        event.stageSnd = BULWARK_DEPARTURE_SOUND;
        event.flagId   = GAME_FLAG_VEHICULAR_AIRLOCK_TO_BULWARK_SCENE;
        event.fade     = DEPARTURE_FADE_NONE;
        return _shelter1fVehicularAirlockStartEvent(reply, &event);
    }
    if (request->areaId == GAME_AREA_SHELTER_1F_AIRLOCK) {
        event.capCmd   = CAP_AIRLOCK_DEPARTURE;
        event.stageSnd = AIRLOCK_DEPARTURE_SOUND;
        event.flagId   = GAME_FLAG_VEHICULAR_AIRLOCK_TO_AIRLOCK_SCENE;
        event.fade     = DEPARTURE_FADE_NONE;
        return _shelter1fVehicularAirlockStartEvent(reply, &event);
    }
    return TRANSITION_DIRECT;
}

/// Refuses key-item use in this room, returning 0 without consuming the item.
///
/// The message supplies an inventory item ID and a zero second payload word;
/// neither payload nor the receiver is accessed.
static s32 _shelter1fVehicularAirlockRejectKeyItem(Task* receiver, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return 0;
}

/// Selects the normal or late-chapter CAP response for room command 3.
///
/// `ROOM_MESSAGE_COMMAND` carries an integer command. Command 3 becomes CAP
/// event 5 when placed-object state 6 is 2 and story chapter is at least 6;
/// otherwise it requests event 3. Busy CAP playback suppresses the event.
/// Other commands do nothing. All calls return zero; the second word is unused.
static s32 _shelter1fVehicularAirlockHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        COMMAND_RESPONSE        = 3,
        RESPONSE_OBJECT_FLAG    = 6,
        RESPONSE_OBJECT_STATE   = 2,
        RESPONSE_LATE_CHAPTER   = 6,
        RESPONSE_LATE_CAP_EVENT = 5
    };
    if (commandId == COMMAND_RESPONSE) {
        if (areaGetCurrentObjectState(RESPONSE_OBJECT_FLAG) == RESPONSE_OBJECT_STATE && gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) >= RESPONSE_LATE_CHAPTER) {
            commandId = RESPONSE_LATE_CAP_EVENT;
        }
        capSpawnEventIfIdle(commandId, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

/// Ignores direction-trigger room actions and returns 0.
///
/// Dispatch borrows the request for the call and supplies a zero second
/// payload word; this handler reads neither and retains no pointer.
static s32 _shelter1fVehicularAirlockIgnoreDirectionAction(Task* receiver, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the vehicular airlock's room message receiver and enters idle state 1.
///
/// Called in state 0 with initialized gameplay resources and a live room task.
/// Publishes the borrowed task in `GAME_TASK_SLOT_ROOM`; the room overlay and
/// message table must remain loaded while it receives messages.
static void _shelter1fVehicularAirlockInitializeRoom(Task* task)
{
    task->msgTable = _gShelter1fVehicularAirlockRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Leaves the initialized room-message task idle in state 1.
static void _shelter1fVehicularAirlockIdleMessageTask(Task* unusedTask)
{
}

/// Room-task handlers for states 0 initialize, 1 idle and 2 release.
///
/// Dispatch copies this three-entry record before indexing; no bounds check is made.
static const TaskFuncTable3 _gShelter1fVehicularAirlockRoomStateHandlers = {
    { _shelter1fVehicularAirlockInitializeRoom, _shelter1fVehicularAirlockIdleMessageTask, taskKill },
};

void shelter1fVehicularAirlockRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelter1fVehicularAirlockRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void shelter1fVehicularAirlockDrawLightsTask(Task* task)
{
    enum {
        LIGHTS_INITIALIZE,
        LIGHTS_DRAW,
        LIGHT_COLOR_RED   = 2 << 8,
        LIGHT_COLOR_AMBER = (2 << 8) | (1 << 4),
        LIGHT_COLOR_GREEN = (2 << 4) | 1,
        LIGHT_COLOR_WHITE = (1 << 8) | (1 << 4) | 1,
    };
    u8 view;

    if (task->state == LIGHTS_INITIALIZE) {
        // Supply the room's effects for actor-triggered flashes, trails and sparks.
        gRoomEffectFlashId      = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_VEHICULAR_AIRLOCK_SPARK_BURST;
        task->state             = LIGHTS_DRAW;
    }

    // Only the two mapped room views with visible light fixtures emit glows.
    view = viewGetMappedIndex();
    switch (view) {
        case 2: {
            const SVECTOR* lightPoints = &_gShelter1fVehicularAirlockLightPoints[2];

            _glowDrawAngledCapsule(&lightPoints[0], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[2], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[6], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[8], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawFactorDisc(&lightPoints[12], 0x200, LIGHT_COLOR_RED);
            break;
        }
        case 3: {
            const SVECTOR* lightPoints = _gShelter1fVehicularAirlockLightPoints;

            _glowDrawAngledCapsule(&lightPoints[0], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[2], 0x200, GLOW_HALF_TURN, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[6], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[8], 0x200, 0, LIGHT_COLOR_AMBER);
            _glowDrawAngledCapsule(&lightPoints[12], 0x200, 0, LIGHT_COLOR_WHITE);
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_BULWARK_UNLOCKED) == 1) {
                _shelter1fVehicularAirlockDrawPulsingLight(&lightPoints[15], 0x804, 0x140, LIGHT_COLOR_GREEN);
                _shelter1fVehicularAirlockDrawPulsingLight(&lightPoints[16], 0xC0, 0x120, LIGHT_COLOR_AMBER);
                _shelter1fVehicularAirlockDrawPulsingLight(&lightPoints[17], -0xC0, 0x120, LIGHT_COLOR_AMBER);
            } else {
                _glowDrawFactorDisc(&lightPoints[15], 0x180, LIGHT_COLOR_GREEN);
                _glowDrawFactorDisc(&lightPoints[16], 0x140, LIGHT_COLOR_AMBER);
                _glowDrawFactorDisc(&lightPoints[17], 0x140, LIGHT_COLOR_AMBER);
            }
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_angled_capsule.inc.c"

#include "../../shared/effects/glow_draw_factor_disc.inc.c"

/// Initializes a Gouraud light diagonal with a coloured centre and black ends.
///
/// Borrows one writable `LINE_G3`; the caller retains ownership. The centre's
/// byte intensities wrap wider inputs modulo 256. Sets the packet length,
/// opaque line command and polyline terminator. The caller must then supply
/// all three screen positions, link the packet and enable blending for the glow.
static inline void _shelter1fVehicularAirlockInitLightDiagonal(LINE_G3* diagonal, u8 red, u8 green, u8 blue)
{
    setLineG3(diagonal);
    setRGB0(diagonal, 0, 0, 0);
    setRGB1(diagonal, red, green, blue);
    setRGB2(diagonal, 0, 0, 0);
}

/// Draws an additive pulsing diamond and two diagonals around a world point.
///
/// Borrows `worldPoint` through view projection. Camera Z / 4 must be at
/// least `GLOW_MIN_DEPTH`; GTE flags are not tested. The signed low halfwords
/// of `radiusScale` and `pulseRate` supply pixel radius = radiusScale * 32 / depth
/// and phase advance in 4096 units per turn per animation frame. Intensity is
/// rsin(phase) / 68 + 60, narrowed to a byte. `packedColor` supplies a signed
/// red factor in bits 8..15 and unsigned green/blue factors in bits 4..5 and
/// 0..1; channel stores wrap to bytes. Requires 20 free scratch-stack bytes,
/// current view matrices and room for four packets plus additive blend commands
/// in the frame arena; packets must stay live until GPU completion.
static void _shelter1fVehicularAirlockDrawPulsingLight(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale, s32 packedColor)
{
    enum { LIGHT_PULSE_DIVISOR      = 68,
           LIGHT_PULSE_BASE         = 60,
           LIGHT_DEPTH_TO_TAG_SHIFT = 4,
           LIGHT_DEPTH_TAG_MASK     = GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt) };
    RoomGlowSpriteScratch* projection;
    POLY_G4*               diamond;
    LINE_G3*               diagonal;
    s32                    pulseSine;
    u8                     intensity;
    s32                    red;
    s32                    green;
    s32                    blue;
    s32                    screenRadius;
    s32                    segmentIndex;
    s32                    diagonalXFactor;
    s32                    diagonalScale;
    s32                    diamondYFactor;
    u16                    centerXBits;
    u16                    centerYBits;

    projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenPos);
    gte_stszotz(&projection->otz);
    // Project first so lights too near the view consume no packets.
    if (projection->otz >= GLOW_MIN_DEPTH) {
        pulseSine              = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius           = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / projection->otz;
        intensity              = pulseSine / LIGHT_PULSE_DIVISOR + LIGHT_PULSE_BASE;
        red                    = intensity * ((s16)packedColor >> 8);
        green                  = intensity * (((s16)packedColor >> 4) & 3);
        blue                   = intensity * (packedColor & 3);
        segmentIndex           = 0;
        projection->halfExtent = screenRadius;
        // Two centre-lit triangles form the diamond, with a black outer rim.
        do {
            diamond        = gGpuPrimCursor;
            gGpuPrimCursor = diamond + 1;
            _glowInitAngledCapsuleWedge(diamond, red, green, blue);
            diamond->x0    = projection->screenPos.vx - projection->halfExtent;
            centerXBits    = projection->screenPos.vx;
            diamond->x2    = centerXBits;
            diamond->x1    = centerXBits;
            diamond->x3    = projection->screenPos.vx + projection->halfExtent;
            centerYBits    = projection->screenPos.vy;
            diamond->y3    = centerYBits;
            diamond->y2    = centerYBits;
            diamond->y0    = centerYBits;
            diamondYFactor = segmentIndex * 2;
            diamond->y1    = (projection->screenPos.vy - projection->halfExtent) + (projection->halfExtent * diamondYFactor);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                    diamond);
            gpuSetPrimitiveBlendMode(diamond, GPU_BLEND_ADD, projection->otz);
            segmentIndex++;
        } while (segmentIndex < 2);

        // Add two crossed diagonals; the second reaches twice the radius.
        segmentIndex = 0;
        do {
            diagonal       = gGpuPrimCursor;
            gGpuPrimCursor = diagonal + 1;
            _shelter1fVehicularAirlockInitLightDiagonal(diagonal, red, green, blue);
            diagonalXFactor = segmentIndex * 3 - 1;
            diagonalScale   = segmentIndex + 1;
            diagonal->x0    = projection->screenPos.vx + (projection->halfExtent * diagonalXFactor);
            diagonal->y0    = projection->screenPos.vy - (projection->halfExtent * diagonalScale);
            diagonal->x1    = projection->screenPos.vx;
            diagonal->y1    = projection->screenPos.vy;
            diagonal->x2    = projection->screenPos.vx - (projection->halfExtent * diagonalXFactor);
            diagonal->y2    = projection->screenPos.vy + (projection->halfExtent * diagonalScale);
            addPrim(&gGpuCurrentOt[((u32)projection->otz << gDisplayState.otDepthShift) >> LIGHT_DEPTH_TO_TAG_SHIFT & LIGHT_DEPTH_TAG_MASK],
                    diagonal);
            gpuSetPrimitiveBlendMode(diagonal, GPU_BLEND_ADD, projection->otz);
            segmentIndex = diagonalScale;
        } while (segmentIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelter1fVehicularAirlockRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelter1fVehicularAirlockRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelter1fVehicularAirlockRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
