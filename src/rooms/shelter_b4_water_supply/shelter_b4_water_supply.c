#include "rooms/shelter_b4_water_supply.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00400.h"
#include "actors/actor_02400.h"
#include "actors/actor_07000.h"
#include "actors/actor_207200.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/effects/water_effects.h"
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

/// The staged event block, read by the departure task.
extern RoomDeparture gRoomDeparture;

/// Next byte for mixed water quad and draw-mode packets in a borrowed actor-load buffer.
///
/// Reset once per frame to the current word-aligned 0xC000-byte half, then
/// advanced by both the Z-running and X-running drawers (at most 0xC00 bytes
/// together). The buffer stays reserved until the GPU finishes the ordering table.
static u8* _gShelterB4WaterSupplyWaterPacketCursor;

static void _shelterB4WaterSupplyDepartToWaterHole(void);
static void _shelterB4WaterSupplyInitializeRoom(Task* task);
static void _shelterB4WaterSupplyIdleRoom(Task* task);
static s32  _shelterB4WaterSupplyResolveWaterHoleVariant(RoomEventMsg* request, RoomEventMsg* reply);
static void _shelterB4WaterSupplyDrawXWaveStrips(Task* task);
static void _shelterB4WaterSupplyInitializeWater(Task* task);
static void _shelterB4WaterSupplyDrawWater(Task* task);

static void _shelterB4WaterSupplyUpperSewerDepartureTask(Task* task);
static s32  _shelterB4WaterSupplyRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _shelterB4WaterSupplyResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB4WaterSupplyIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32  _shelterB4WaterSupplyHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32  _shelterB4WaterSupplyHandleSound(Task* task, s32 messageId, s32 soundCommand, s32 unused);
static void _shelterB4WaterSupplyFirstValveDepartureTask(Task* task);
static void _shelterB4WaterSupplyWaterTask(Task* task);

/// Bodyless Water Hole departure recipe, borrowing the staged `gRoomDeparture`.
///
/// Both spawn arguments are unused. The room and staging must outlive the task;
/// the descriptor itself is read only during spawning.
static TaskDesc _gShelterB4WaterSupplyWaterHoleDepartureTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventDepartureTask, { .value = 0 } };

/// Synchronous message handlers borrowed by the initialized room receiver.
///
/// Resolve uses request/reply `RoomEventMsg` pointers; room action borrows a
/// `DirectionActionRequest`. Key-item, command and sound payloads are integer
/// words. The final record makes unsupported messages return zero. Keep the
/// room overlay loaded while the receiver holds this table.
static TaskMessageEntry _gShelterB4WaterSupplyRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB4WaterSupplyResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB4WaterSupplyRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB4WaterSupplyHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB4WaterSupplyIgnoreCommand },
    { ROOM_MESSAGE_SOUND, _shelterB4WaterSupplyHandleSound },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Indexed bodyless departure recipes: first valve use (0), upper sewer (1).
///
/// Slot 0 waits for the active valve CAP; slot 1 receives a CAP command ID in
/// the first spawn word and borrows the staged upper-sewer destination.
/// The second spawn word is unused. These are indexed entries with no terminator.
static TaskDesc _gShelterB4WaterSupplyDepartureTaskDescs[] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB4WaterSupplyFirstValveDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB4WaterSupplyUpperSewerDepartureTask, { .value = 0 } },
};

/// Undisplaced Y of both water rectangles in signed world units, published to the session.
static s16 _gShelterB4WaterSupplyWaterY = -2500;

/// Bodyless water-rendering recipe started when the room receiver initializes.
///
/// Spawn arguments are unused; the task borrows this room's water geometry and
/// an actor-load packet arena until room teardown.
static TaskDesc _gShelterB4WaterSupplyWaterTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_ACTOR } }, _shelterB4WaterSupplyWaterTask, { .value = 0 } };

/// The Z-running water rectangle, followed by the compact descriptor's list end.
static RoomCompactWaterSurface _gShelterB4WaterSupplyWaterWaveSurfaces[] = {
    { 9100, -0x364C, 1800, 0x2EE0, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// X-running water rectangle in world X/Z units, followed by the list end.
///
/// Its 8150-by-1800 extent is drawn as two strips of sixteen quads, sharing the
/// room's water Y. The sentinel is part of the complete two-record list.
static RoomCompactWaterSurface _gShelterB4WaterSupplyXWaterWaveSurfaces[] = {
    { 4400, -1900, 8150, 1800, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// World-space endpoints of the light beam drawn in mapped views 2, 3 and 4.
static SVECTOR _gShelterB4WaterSupplyViews234LightBeamEndpoints[] = {
    { 0x36C4, -5130, -1910, 0 },
    { 0x337C, -4540, -1910, 0 },
};

/// World-space endpoints of the light beam drawn in mapped views 5 and 6.
static SVECTOR _gShelterB4WaterSupplyViews56LightBeamEndpoints[] = {
    { 6520, -3790, -1910, 0 },
    { 5500, -3790, -1910, 0 },
};

/// Five world-space endpoint pairs selected by the mapped view's light beams.
///
/// Pairs 0/2 draw in views 4 and 8, pair 1 in views 6 and 7, pair 3 in views
/// 9..11 and pair 4 additionally in view 9. Each drawer borrows both points.
static SVECTOR _gShelterB4WaterSupplyLightBeamEndpoints[][2] = {
    { { 10480, -3840, -100, 0 }, { 9500, -3750, -100, 0 } },
    { { 1990, -5790, -80, 0 }, { 1000, -5790, -80, 0 } },
    { { 10920, -3790, -3560, 0 }, { 10920, -3790, -4550, 0 } },
    { { 10920, -3790, -11570, 0 }, { 10920, -3790, -12550, 0 } },
    { { 9130, -3790, -7950, 0 }, { 9130, -3790, -6970, 0 } },
};

/// View-space splash-position history for player-model coordinates 14 and 17.
///
/// XYZ narrow to signed halfwords. Initialization samples the existing composed
/// matrices; later samples refresh them and compare full-width translations.
/// Paused or out-of-water sampling retains these positions, so movement is
/// measured since the last sample. The vector's fourth halfword is unused.
static SVECTOR _gShelterB4WaterSupplyPlayerSplashPositionHistory[2] = { 0 };

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

u8 gShelterB4WaterSupplyStairSurfaceRows[4][16] = {
    { 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
    { 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0 },
    { 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 },
};

u8* gShelterB4WaterSupplyViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB4WaterSupplyViewCounts[1] = { 11 };

DirectionWarpEntry gShelterB4WaterSupplyWarpEntries[3] = {
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 640, -4000, -1024 },
      .companion      = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 16 }, 5350, -2000, -1600 },
      .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
      .departureSound = SOUND_SHELTER_B4_WATER_SUPPLY_EXIT_TRANSIT,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 7,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 9450, -2000, -13000 },
      .companion      = { { .word = 15 * ACTOR_TRANSFORM_ANGLE_TURN / 16 }, 10300, -2000, -13600 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 2),
      .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 1),
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 11,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 15600, -3600, -1000 },
      .companion      = { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 15600, -3600, -1000 },
      .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 4),
      .departureSound = DIRECTION_WARP_SOUND_NONE,
      .blockedSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, 6),
      .initialView    = 2,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = GAME_FLAG_MAP_MARK_WATER },
};

/// Collision-grid face normals in room space, with 4096 representing unit length.
static SVECTOR _gShelterB4WaterSupplyCollisionGridNormals[] = {
#include "assets/shelter_b4_water_supply_collision_0587C_normals.inc"
};

/// Collision-grid vertices in signed room-space game units.
static SVECTOR _gShelterB4WaterSupplyCollisionGridVerts[] = {
#include "assets/shelter_b4_water_supply_collision_0587C_verts.inc"
};

/// Collision-grid faces indexing this asset's vertex and normal pools.
static WorldCollisionGridFace _gShelterB4WaterSupplyCollisionGridFaces[] = {
#include "assets/shelter_b4_water_supply_collision_0587C_faces.inc"
};

/// Concatenated cell face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB4WaterSupplyCollisionGridCells[] = {
#include "assets/shelter_b4_water_supply_collision_0587C_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB4WaterSupplyCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..221
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B4_WATER_SUPPLY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB4WaterSupplyCollisionGridCells[(halfwordOffset)])
/// X-major 4-by-4 collision-cell directory; NULL entries contain no faces.
static s16* _gShelterB4WaterSupplyCollisionGridTable[] = {
#include "assets/shelter_b4_water_supply_collision_0587C_table.inc"
};
#undef SHELTER_B4_WATER_SUPPLY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB4WaterSupplyCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB4WaterSupplyCollisionGridNormals,
    .vertices    = _gShelterB4WaterSupplyCollisionGridVerts,
    .faces       = _gShelterB4WaterSupplyCollisionGridFaces,
    .cellFaceIds = _gShelterB4WaterSupplyCollisionGridTable,
    .xBias       = -400,
    .zBias       = 13900,
    .cellCountX  = 4,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB4WaterSupplyCollisionGridFaces),
};

ViewCamera gShelterB4WaterSupplyViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -6828, 0x7D00, 6785 } }, 447 },
    { { { { -1049, 0, -3959 }, { 841, 4002, -223 }, { 3868, -870, -1025 } }, { -0x2E41, 3890, 333 } }, 230 },
    { { { { -770, 0, -4022 }, { 44, 4095, -8 }, { 4022, -45, -770 } }, { -9469, 2988, 580 } }, 230 },
    { { { { -1205, 0, -3914 }, { -191, 4091, 58 }, { 3909, 200, -1204 } }, { -6858, 2988, 402 } }, 230 },
    { { { { -943, 0, 3985 }, { 65, 4095, 15 }, { -3985, 67, -943 } }, { -0x291C, 2925, 450 } }, 230 },
    { { { { -1040, 0, 3961 }, { -254, 4087, -66 }, { -3953, -263, -1038 } }, { -7900, 2925, 420 } }, 257 },
    { { { { -608, 0, -4050 }, { -1926, 3602, 289 }, { 3562, 1948, -535 } }, { 1769, 6119, 698 } }, 230 },
    { { { { 3988, 0, 933 }, { 42, 4091, -182 }, { -932, 187, 3983 } }, { -0x2898, 2969, 7455 } }, 230 },
    { { { { -4028, 0, 739 }, { 15, 4095, 83 }, { -739, 85, -4027 } }, { -0x28D4, 2936, 3528 } }, 230 },
    { { { { -4027, 0, 745 }, { 0, 4095, 1 }, { -745, 1, -4027 } }, { -0x28BD, 2952, 6769 } }, 230 },
    { { { { -3911, 0, 1216 }, { 140, 4068, 451 }, { -1208, 473, -3884 } }, { -0x2913, 3254, 9638 } }, 230 },
};

/// Empty sprite batch list for mapped view 1, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 2, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 3, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 34 foreground sprite rectangles for mapped view 4.
///
/// XY and dimensions are pixels relative to the view centre; depth is an
/// unscaled sorting value. Texture pages and CLUT are encoded VRAM addresses.
/// The room owns the mutable sources borrowed by its view and batch records.
static SpriteSource _gShelterB4WaterSupplyView4SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 72, 646, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, 112, 537, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, -72, 625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, -88, 624, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, -120, 647, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, -120, 601, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, -104, 628, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -56, 650, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -24, 650, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, 8, 650, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, 40, 650, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, 56, 650, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, 72, 650, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -120, 578, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -88, 650, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, -120, 545, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, -88, 575, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, -56, 600, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, -24, 600, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, 8, 600, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, 40, 600, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, 72, 600, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 104, -120, 575, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 104, -80, 575, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 104, -40, 575, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 104, 0, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 104, 40, 575, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 104, 80, 575, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, -120, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, -80, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, -40, 500, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, 0, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, 40, 500, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, 80, 500, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
};

/// Foreground range for mapped view 4, retaining the decoded background.
///
/// A zero-count background record precedes the complete source range and
/// `SPRITE_BATCH_END`. Ranges count source elements, not bytes.
static SpriteBatch _gShelterB4WaterSupplyView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4WaterSupplyView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 5, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 6, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 108 foreground sprite rectangles for mapped view 7.
///
/// XY and dimensions are pixels relative to the view centre; depth is an
/// unscaled sorting value. Texture pages and CLUT are encoded VRAM addresses.
/// The room owns the mutable sources borrowed by its view and batch records.
static SpriteSource _gShelterB4WaterSupplyView7SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, -8, 1200, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -56, -8, 1200, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, -8, 1200, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -24, -8, 1200, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -8, -8, 1212, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 8, -8, 1225, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 24, -8, 1225, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 0, 1200, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -48, 0, 1200, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, 0, 1200, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 0, 0, 1212, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 24, 0, 1212, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -72, 8, 1200, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -48, 8, 1200, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, 8, 1200, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 0, 8, 1200, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 24, 8, 1200, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -80, 16, 1175, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 16, 1177, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -32, 16, 1177, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -8, 16, 1177, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 16, 16, 1177, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, 16, 1177, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -80, 32, 1162, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 32, 1162, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -32, 32, 1162, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -8, 32, 1162, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 16, 32, 1162, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, 32, 1162, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, -56, 2517, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -56, 2500, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, -48, 2750, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -48, 2525, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, -40, 2800, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -40, 2500, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, -32, 2875, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -32, 2500, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, -24, 2565, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -24, 2500, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -8, -16, 2750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -16, 2500, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -8, 2500, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -8, -120, 2500, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 0, -120, 1225, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -8, -96, 2500, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 0, -96, 1268, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -8, -72, 1576, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 0, -72, 1503, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -120, 2125, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -96, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -72, 2250, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -48, 2250, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -24, 2325, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 24, -24, 1950, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, -56, 1875, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, -88, 1250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 24, -120, 1250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 40, -120, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 40, -88, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 40, -56, 1125, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 40, -24, 1125, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 72, -120, 850, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 72, -88, 850, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 72, -56, 850, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 72, -24, 850, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 104, -120, 750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 104, -88, 750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 104, -56, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 104, -24, 750, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -120, 675, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -88, 675, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -56, 675, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -40, -56, 1500, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -48, 2500, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -48, -64, 1656, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, -64, 1375, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -24, -64, 1375, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -8, -64, 2250, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, -64, 2250, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, -64, 1459, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, -48, 1524, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -64, -32, 1689, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, -80, 1281, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -48, -80, 1341, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, -80, 1495, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, -80, 1468, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -80, 1372, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 16, -80, 1256, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -64, -104, 1129, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -40, -104, 1225, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -16, -104, 1217, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -104, 1200, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, -48, 2500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, -32, 2500, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, -16, 2500, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -24, 48, 538, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, 56, 530, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, 72, 509, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 72, 675, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, 88, 506, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 16, 40, 550, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 16, 48, 575, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 16, 56, 575, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 56, 535, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 16, 72, 575, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 72, 523, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 8, 64, 575, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 88, 522, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

/// Source-element boundaries between the four view-7 foreground ranges.
enum {
    SHELTER_B4_WATER_SUPPLY_VIEW7_SECOND_BATCH_FIRST_SPRITE = 29,
    SHELTER_B4_WATER_SUPPLY_VIEW7_THIRD_BATCH_FIRST_SPRITE  = 72,
    SHELTER_B4_WATER_SUPPLY_VIEW7_FOURTH_BATCH_FIRST_SPRITE = 95
};

/// Four foreground ranges for mapped view 7, retaining the decoded background.
///
/// Ranges partition all 108 source elements after the zero-count background
/// record and end with `SPRITE_BATCH_END`. The two serialized bytes retain
/// their authored values; their meaning is unproven.
static SpriteBatch _gShelterB4WaterSupplyView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B4_WATER_SUPPLY_VIEW7_SECOND_BATCH_FIRST_SPRITE, 0, 0, { 3, 0 } },
    { SHELTER_B4_WATER_SUPPLY_VIEW7_SECOND_BATCH_FIRST_SPRITE, SHELTER_B4_WATER_SUPPLY_VIEW7_THIRD_BATCH_FIRST_SPRITE - SHELTER_B4_WATER_SUPPLY_VIEW7_SECOND_BATCH_FIRST_SPRITE, 0, 0, { 0, 0 } },
    { SHELTER_B4_WATER_SUPPLY_VIEW7_THIRD_BATCH_FIRST_SPRITE, SHELTER_B4_WATER_SUPPLY_VIEW7_FOURTH_BATCH_FIRST_SPRITE - SHELTER_B4_WATER_SUPPLY_VIEW7_THIRD_BATCH_FIRST_SPRITE, 0, 0, { 2, 0 } },
    { SHELTER_B4_WATER_SUPPLY_VIEW7_FOURTH_BATCH_FIRST_SPRITE, ARRAY_SIZE(_gShelterB4WaterSupplyView7SpriteSources) - SHELTER_B4_WATER_SUPPLY_VIEW7_FOURTH_BATCH_FIRST_SPRITE, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// The 47 foreground sprite rectangles for mapped view 8.
///
/// XY and dimensions are pixels relative to the view centre; depth is an
/// unscaled sorting value. Texture pages and CLUT are encoded VRAM addresses.
/// The room owns the mutable sources borrowed by its view and batch records.
static SpriteSource _gShelterB4WaterSupplyView8SpriteSources[] = {
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -16, -88, 1100, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -16, -56, 1308, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -16, -24, 1324, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -16, 8, 1125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, 24, 1283, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, -88, 903, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, -56, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, -24, 1050, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 80, 8, 1050, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 96, -88, 1000, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 96, -56, 1000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 96, -24, 1000, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 96, 8, 1000, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 96, 48, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 112, -88, 1000, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -88, 1000, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -56, 1000, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 112, -56, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 112, -24, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -24, 1000, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 112, 8, 1000, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 136, 8, 1000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 112, 48, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, 136, 48, 1000, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 112, 88, 1000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, 88, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -32, -88, 1125, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -32, -56, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -32, -24, 1125, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, 8, 1125, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, -88, 1000, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, -56, 1000, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, -24, 1000, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, 8, 1000, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, -56, 40, 1000, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -88, -88, 1000, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -88, -40, 1000, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -88, 8, 1000, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -88, 56, 1000, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -120, -88, 1000, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -120, -40, 1000, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -120, 8, 1000, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -120, 56, 1000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -160, -88, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -160, -40, 1000, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -160, 8, 1000, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 64 } }, -160, 56, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

/// Foreground range for mapped view 8, retaining the decoded background.
///
/// A zero-count background record precedes the complete source range and
/// `SPRITE_BATCH_END`. Ranges count source elements, not bytes.
static SpriteBatch _gShelterB4WaterSupplyView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterB4WaterSupplyView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 9, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 10, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped view 11, retaining the decoded background.
///
/// The zero-count first record is followed by `SPRITE_BATCH_END`; its retained
/// source-slot address is never dereferenced as a `SpriteSource`.
static SpriteBatch _gShelterB4WaterSupplyView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB4WaterSupplySpriteViews[11] = {
    { { .empty = _gShelterB4WaterSupplyView1SpriteBatches }, _gShelterB4WaterSupplyView1SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView2SpriteBatches }, _gShelterB4WaterSupplyView2SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView3SpriteBatches }, _gShelterB4WaterSupplyView3SpriteBatches, NULL },
    { { .elements = _gShelterB4WaterSupplyView4SpriteSources }, _gShelterB4WaterSupplyView4SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView5SpriteBatches }, _gShelterB4WaterSupplyView5SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView6SpriteBatches }, _gShelterB4WaterSupplyView6SpriteBatches, NULL },
    { { .elements = _gShelterB4WaterSupplyView7SpriteSources }, _gShelterB4WaterSupplyView7SpriteBatches, NULL },
    { { .elements = _gShelterB4WaterSupplyView8SpriteSources }, _gShelterB4WaterSupplyView8SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView9SpriteBatches }, _gShelterB4WaterSupplyView9SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView10SpriteBatches }, _gShelterB4WaterSupplyView10SpriteBatches, NULL },
    { { .empty = _gShelterB4WaterSupplyView11SpriteBatches }, _gShelterB4WaterSupplyView11SpriteBatches, NULL },
};

/// Ten room-owned point lights contributing in every logical view.
///
/// Positions and falloff radii use world units; RGB uses 4096 per unit.
/// Coordinate composition binds the view parent and shading overwrites
/// attenuation, so these records remain mutable while the room is loaded.
static WorldCoordPointLight _gShelterB4WaterSupplyPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2710, -3800, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x393A, -5245, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 1750, 2250 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1930, -7250, -1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 2785 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2710, -3800, -4000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9500, -3800, -7500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2904, -3800, -0x2EE0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3520, -4845, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6000, -3800, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1500, -5785, -500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2C24, -3800, -500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3358, 2949, 2539 }, { 0, 0 } }, 2000, 2500 },
};

WorldCoordRoomLights gShelterB4WaterSupplyRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB4WaterSupplyPointLights),
    .pointLights           = _gShelterB4WaterSupplyPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB4WaterSupplyViewBoundaryTriggers[18] = {
    { NULL, NULL, NULL, { 0x359F, -3328, -1152, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3541, -3392, -1089, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2DE0, -3361, -1056, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2D60, -3297, -992, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8608, -3457, -1024, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8449, -3328, -992, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5760, -3329, -1152, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5632, -3424, -1152, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3264, -3392, -992, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3104, -3393, -960, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2730, -3201, -2064, 0 }, { { -1232, -4160, 16, 0 }, { 1232, -4160, -16, 0 }, { -1232, 4160, 16, 0 }, { 1232, 4160, -16, 0 } }, { -54, 0, -4107, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 8, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2741, -3424, -2144, 0 }, { { 1200, -4160, -16, 0 }, { -1200, -4160, 16, 0 }, { 1200, 4160, -16, 0 }, { -1200, 4160, 16, 0 } }, { 54, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 4, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2711, -3424, -5617, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2720, -3552, -5488, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -3425, -8576, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9952, -3296, -8768, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -3617, -0x2EE0, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 10, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -3616, -0x2E40, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 11, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed stair and warp selectors used by this room's action triggers.
enum {
    SHELTER_B4_WATER_SUPPLY_STAIR_DESCEND       = 1 << 8,
    SHELTER_B4_WATER_SUPPLY_STAIR_FLIGHT_2      = 0x80 | (2 << 4) | 10, // Indexed surfaces, flight 2, ten steps
    SHELTER_B4_WATER_SUPPLY_STAIR_FLIGHT_3      = 0x80 | (3 << 4) | 8,  // Indexed surfaces, flight 3, eight steps
    SHELTER_B4_WATER_SUPPLY_STAIR_YAW_SHIFT     = 4,                    // Parameter1 uses sixteen angle units per step
    SHELTER_B4_WATER_SUPPLY_RESERVOIR_WARP      = (2 << 4) | 1,         // Departure warp 2, destination arrival 1
    SHELTER_B4_WATER_SUPPLY_UPPER_SEWER_WARP    = (1 << 4) | 3,         // Departure warp 1, destination arrival 3
    SHELTER_B4_WATER_SUPPLY_VALVE_TRAVEL_ACTION = 10,
};

WorldCollisionTrigger gShelterB4WaterSupplyActionTriggers[7] = {
    { NULL, NULL, NULL, { 2496, -4063, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING | SHELTER_B4_WATER_SUPPLY_STAIR_DESCEND, SHELTER_B4_WATER_SUPPLY_STAIR_FLIGHT_2, (ACTOR_TRANSFORM_ANGLE_TURN / 4) >> SHELTER_B4_WATER_SUPPLY_STAIR_YAW_SHIFT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4800, -2056, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4095, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING, SHELTER_B4_WATER_SUPPLY_STAIR_FLIGHT_2, (3 * ACTOR_TRANSFORM_ANGLE_TURN / 4) >> SHELTER_B4_WATER_SUPPLY_STAIR_YAW_SHIFT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2FA0, -2064, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4097, 0, -39, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING, SHELTER_B4_WATER_SUPPLY_STAIR_FLIGHT_3, (ACTOR_TRANSFORM_ANGLE_TURN / 4) >> SHELTER_B4_WATER_SUPPLY_STAIR_YAW_SHIFT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3640, -3647, -1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_FACING | SHELTER_B4_WATER_SUPPLY_STAIR_DESCEND, SHELTER_B4_WATER_SUPPLY_STAIR_FLIGHT_3, (3 * ACTOR_TRANSFORM_ANGLE_TURN / 4) >> SHELTER_B4_WATER_SUPPLY_STAIR_YAW_SHIFT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9536, -2072, -0x32D0, 0 }, { { -416, 0, -944, 0 }, { 416, 0, -944, 0 }, { -416, 0, 944, 0 }, { 416, 0, 944, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_RESERVOIR, SHELTER_B4_WATER_SUPPLY_RESERVOIR_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -4062, -1120, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B4_UPPER_SEWER, SHELTER_B4_WATER_SUPPLY_UPPER_SEWER_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3D40, -3645, -1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_B4_WATER_SUPPLY_VALVE_TRAVEL_ACTION, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource-entry keys used by this room's actor placements.
enum {
    SHELTER_B4_WATER_SUPPLY_BOG_DIVER_ENTRY_ID         = 4,
    SHELTER_B4_WATER_SUPPLY_AMOEBA_ENTRY_ID            = 24,
    SHELTER_B4_WATER_SUPPLY_SLOUCH_ENTRY_ID            = 70,
    SHELTER_B4_WATER_SUPPLY_CREEPING_STRANGER_ENTRY_ID = 72
};

/// Global-library decimal file numbers selected by this room's resource records.
enum {
    SHELTER_B4_WATER_SUPPLY_BOG_DIVER_FILE_NUMBER         = 4,
    SHELTER_B4_WATER_SUPPLY_AMOEBA_FILE_NUMBER            = 24,
    SHELTER_B4_WATER_SUPPLY_SLOUCH_FILE_NUMBER            = 70,
    SHELTER_B4_WATER_SUPPLY_CREEPING_STRANGER_FILE_NUMBER = 72
};

/// Bog Diver resource and body-task recipe for placement variant 1.
///
/// Entry IDs match the variant's placements; `AREA_PLACEMENT_END` terminates
/// the list. File 100400 supplies actor_100400; task index 0 starts its body.
static AreaResource _gShelterB4WaterSupplyVariant1Resources[] = {
    { SHELTER_B4_WATER_SUPPLY_BOG_DIVER_ENTRY_ID, SHELTER_B4_WATER_SUPPLY_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slouch and Creeping Stranger resources for placement variant 2.
///
/// Entry IDs 70 and 72 match the four placements. Files 107000 and 207200
/// supply the borrowed body-task recipes at index 0. `AREA_PLACEMENT_END`
/// terminates the list; resources and descriptors must stay loaded during spawning.
static AreaResource _gShelterB4WaterSupplyVariant2Resources[] = {
    { SHELTER_B4_WATER_SUPPLY_SLOUCH_ENTRY_ID, SHELTER_B4_WATER_SUPPLY_SLOUCH_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor107000SlouchTaskDescs },
    { SHELTER_B4_WATER_SUPPLY_CREEPING_STRANGER_ENTRY_ID, SHELTER_B4_WATER_SUPPLY_CREEPING_STRANGER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor207200CreepingStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Amoeba resource and body-task recipe for placement variant 3.
///
/// Entry 24 matches all three placements; file 102400 supplies actor_102400
/// and task index 0 starts its body. `AREA_PLACEMENT_END` terminates the list.
static AreaResource _gShelterB4WaterSupplyVariant3Resources[] = {
    { SHELTER_B4_WATER_SUPPLY_AMOEBA_ENTRY_ID, SHELTER_B4_WATER_SUPPLY_AMOEBA_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bog Diver resource and body-task recipe for placement variant 11.
///
/// Entry 4 matches both placements; file 100400 supplies actor_100400 and
/// task index 0 starts its body. `AREA_PLACEMENT_END` terminates the list.
static AreaResource _gShelterB4WaterSupplyVariant11Resources[] = {
    { SHELTER_B4_WATER_SUPPLY_BOG_DIVER_ENTRY_ID, SHELTER_B4_WATER_SUPPLY_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Initial Bog Diver placement for the default layout, variant 1.
///
/// Position is (7000, -2000, -1000) in world units and yaw is a quarter turn.
/// Loads the base resource with no texture-page offset and CLUT row offset 2.
/// The actor borrows this record while the room is loaded; the next record
/// terminates the list with `AREA_PLACEMENT_END`.
static AreaPlacement _gShelterB4WaterSupplyVariant1Placements[] = {
    { SHELTER_B4_WATER_SUPPLY_BOG_DIVER_ENTRY_ID, 0, 0, 7000, -2000, -1000, ONE / 4, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant-2 placements for two Creeping Strangers and two Slouches, then list end.
///
/// All use world-unit positions, 4096-unit yaw, base files and zero actor
/// arguments/parameter rows. Strangers shift texture pages by 2 and CLUT rows
/// by 3; Slouches shift only CLUT rows by 2. Actors borrow these records.
static AreaPlacement _gShelterB4WaterSupplyVariant2Placements[] = {
    { SHELTER_B4_WATER_SUPPLY_CREEPING_STRANGER_ENTRY_ID, 0, 0, 10000, -2000, -1900, 0, 0, 2, 3, 0 },
    { SHELTER_B4_WATER_SUPPLY_CREEPING_STRANGER_ENTRY_ID, 0, 0, 7800, -2000, -1000, 2600, 0, 2, 3, 0 },
    { SHELTER_B4_WATER_SUPPLY_SLOUCH_ENTRY_ID, 0, 0, 9500, -2000, -900, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B4_WATER_SUPPLY_SLOUCH_ENTRY_ID, 0, 0, 10200, -2000, -5400, -200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant-3 placements for three Amoebas along X 10000, followed by list end.
///
/// World-unit positions use Y -2000 and Z -1000/-7000/-4000, with zero yaw.
/// Base files have no page shift and CLUT row offset 2; actor arguments and
/// parameter rows are zero. Actors borrow these records while the room is loaded.
static AreaPlacement _gShelterB4WaterSupplyVariant3Placements[] = {
    { SHELTER_B4_WATER_SUPPLY_AMOEBA_ENTRY_ID, 0, 0, 10000, -2000, -1000, 0, 0, 0, 2, 0 },
    { SHELTER_B4_WATER_SUPPLY_AMOEBA_ENTRY_ID, 0, 0, 10000, -2000, -7000, 0, 0, 0, 2, 0 },
    { SHELTER_B4_WATER_SUPPLY_AMOEBA_ENTRY_ID, 0, 0, 10000, -2000, -4000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant-11 placements for two Bog Divers, followed by list end.
///
/// World positions are (10000, -2000, -1000/-6500), with half-turn/zero yaw.
/// Base files have no page shift and CLUT row offset 2; actor arguments and
/// parameter rows are zero. Actors borrow these records while the room is loaded.
static AreaPlacement _gShelterB4WaterSupplyVariant11Placements[] = {
    { SHELTER_B4_WATER_SUPPLY_BOG_DIVER_ENTRY_ID, 0, 0, 10000, -2000, -1000, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B4_WATER_SUPPLY_BOG_DIVER_ENTRY_ID, 0, 0, 10000, -2000, -6500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB4WaterSupplyPlacementVariants[12] = {
    { NULL, NULL },
    { _gShelterB4WaterSupplyVariant1Placements, _gShelterB4WaterSupplyVariant1Resources },
    { _gShelterB4WaterSupplyVariant2Placements, _gShelterB4WaterSupplyVariant2Resources },
    { _gShelterB4WaterSupplyVariant3Placements, _gShelterB4WaterSupplyVariant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gShelterB4WaterSupplyVariant11Placements, _gShelterB4WaterSupplyVariant11Resources },
};

WorldCollisionOccluder gShelterB4WaterSupplyOccluders[2] = {
    { NULL, NULL, { 4608, -3408, -7120, 0 }, { { -4256, 4336, -4880, 0 }, { 4256, 4336, 4880, 0 }, { -4256, -4336, -4880, 0 }, { 4256, -4336, 4880, 0 } }, { -3089, 0, 2693, 0 }, 7781, 1, 0 },
    { NULL, NULL, { 0x371F, -3712, -7649, 0 }, { { 3162, 4336, -5651, 0 }, { -3161, 4336, 5652, 0 }, { 3162, -4336, -5651, 0 }, { -3161, -4336, 5652, 0 } }, { -3578, 0, -2002, 0 }, 7781, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry gShelterB4WaterSupplyRoomAmbient[12] = {
    { .viewCount = ARRAY_SIZE(gShelterB4WaterSupplyRoomAmbient) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 520, 596, 596, 567 } },
    { .color = { 516, 557, 558, 541 } },
    { .color = { 513, 529, 544, 524 } },
    { .color = { 435, 519, 535, 489 } },
    { .color = { 520, 561, 556, 545 } },
    { .color = { 517, 555, 540, 538 } },
    { .color = { 519, 548, 538, 535 } },
    { .color = { 420, 480, 498, 459 } },
};

/// Type-1 cue-2 bases in the currently loaded bank; cue 1 adds one, companion cues add 100.
enum {
    SHELTER_B4_WATER_SUPPLY_SURFACE1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    SHELTER_B4_WATER_SUPPLY_SURFACE1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    SHELTER_B4_WATER_SUPPLY_SURFACE2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    SHELTER_B4_WATER_SUPPLY_SURFACE2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
    SHELTER_B4_WATER_SUPPLY_SURFACE2_JUMP_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x29,
};

/// Footstep cue bases shared by surface classes 1 and 3.
///
/// Walking and scripted jumps use the same pair; running uses the other pair.
/// Surface records borrow these type-1 requests for the room overlay's lifetime.
static WorldCollisionFootstepSounds _gShelterB4WaterSupplySurface1FootstepSounds = {
    .walk         = SHELTER_B4_WATER_SUPPLY_SURFACE1_WALK_CUE_BASE,
    .run          = SHELTER_B4_WATER_SUPPLY_SURFACE1_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_WATER_SUPPLY_SURFACE1_WALK_CUE_BASE,
};

/// Footstep cue bases for surface class 2, with distinct walk, run and scripted-jump pairs.
///
/// Surface records borrow these type-1 requests for the room overlay's lifetime.
static WorldCollisionFootstepSounds _gShelterB4WaterSupplySurface2FootstepSounds = {
    .walk         = SHELTER_B4_WATER_SUPPLY_SURFACE2_WALK_CUE_BASE,
    .run          = SHELTER_B4_WATER_SUPPLY_SURFACE2_RUN_CUE_BASE,
    .scriptedJump = SHELTER_B4_WATER_SUPPLY_SURFACE2_JUMP_CUE_BASE,
};

/// Silent blocking policy shared by surface classes 0 and 4..7.
static WorldCollisionSurfaceProperties _gShelterB4WaterSupplyDefaultSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class-1 policy: block probes, allow weapon impacts and apply pushback, with footsteps.
static WorldCollisionSurfaceProperties _gShelterB4WaterSupplySurface1 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4WaterSupplySurface1FootstepSounds,
};

/// Class-2 policy: block probes, allow weapon impacts and apply pushback, with its own footsteps.
static WorldCollisionSurfaceProperties _gShelterB4WaterSupplySurface2 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4WaterSupplySurface2FootstepSounds,
};

/// Class-3 policy: pass probes, ignore weapon impacts and apply pushback, using class-1 footsteps.
static WorldCollisionSurfaceProperties _gShelterB4WaterSupplySurface3 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB4WaterSupplySurface1FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB4WaterSupplySurfacePropertiesByClass[8] = {
    &_gShelterB4WaterSupplyDefaultSurface,
    &_gShelterB4WaterSupplySurface1,
    &_gShelterB4WaterSupplySurface2,
    &_gShelterB4WaterSupplySurface3,
    &_gShelterB4WaterSupplyDefaultSurface,
    &_gShelterB4WaterSupplyDefaultSurface,
    &_gShelterB4WaterSupplyDefaultSurface,
    &_gShelterB4WaterSupplyDefaultSurface,
};

/// Upper-sewer departure's subtractive fade, borrowed until session teardown.
///
/// The fade holds black after its thirty-update ramp. The containing storage's
/// trailing word has no established use and remains intact.
static RoomFadeStorage _gShelterB4WaterSupplyUpperSewerFade = { 0 };

/// Saved-location cell staging the CAP-gated upper-sewer destination.
///
/// Only `loc.area`, `loc.warp` and `loc.roomState` are written and later committed;
/// the other bytes retain their zero initialization. Keep this room-owned cell
/// stable until the deferred departure finishes its fade and transit sound.
static GameLocationKey _gShelterB4WaterSupplyUpperSewerDestination = { 0 };

RoomDeparture gRoomDeparture = { 0 };

#include "../../shared/room/room_event_departure_task.inc.c"

/// Room receiver states 0..2: initialize, idle and release the task.
///
/// The dispatcher copies this complete three-callback record to the stack;
/// the state must stay in range and the room code must remain loaded.
static const TaskFuncTable3 _gShelterB4WaterSupplyRoomStates = {
    {
        _shelterB4WaterSupplyInitializeRoom,
        _shelterB4WaterSupplyIdleRoom,
        taskKill,
    },
};

/// Pauses scene actors and starts the upper-sewer departure's fade to black.
///
/// Requests battle-escape results when the current battle permits them, then
/// starts a 30-update subtractive ramp and sets `departureTask->killCountdown`
/// to the same duration. The caller advances the departure state separately.
/// The live departure task and room-owned fade storage must remain writable;
/// the fade holds black until teardown and borrows the storage for its lifetime.
/// Spawn failure still leaves actors paused and the countdown initialized.
static inline void _shelterB4WaterSupplyStartUpperSewerFade(Task* departureTask)
{
    enum { FADE_FRAMES    = 30,
           FADE_TASK_BANK = 1,
           FADE_TASK_SLOT = 0x31,
           FADE_OT_AUTO   = 0 };
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
    sceneQueueBattleEscapeResult();
    _gShelterB4WaterSupplyUpperSewerFade.fade.blend      = SCREEN_FADE_SUBTRACT;
    _gShelterB4WaterSupplyUpperSewerFade.fade.phase      = SCREEN_FADE_RUNNING;
    _gShelterB4WaterSupplyUpperSewerFade.fade.rampFrames = FADE_FRAMES;
    taskSpawn(FADE_TASK_BANK, FADE_TASK_SLOT, FADE_OT_AUTO, &_gShelterB4WaterSupplyUpperSewerFade.fade);
    departureTask->killCountdown = FADE_FRAMES;
}

/// Runs the upper-sewer CAP choice, fade, transit sound and saved-location reload.
///
/// Start bodyless in state zero with a CAP command ID in `spawnArg1.value`
/// and the resolved destination staged in room-owned storage. Choice 10 accepts;
/// other choices resume actor/player control and rearm interaction for ten updates.
/// Acceptance fades for 30 ticks, waits for the transit sound, then publishes the
/// staged area/warp/room to the live save and starts reload. The room's fade and
/// destination storage must remain live and stable until the reload starts.
static void _shelterB4WaterSupplyUpperSewerDepartureTask(Task* task)
{
    enum {
        SHELTER_B4_WATER_SUPPLY_SEWER_START_CAP           = 0,
        SHELTER_B4_WATER_SUPPLY_SEWER_WAIT_CAP            = 1,
        SHELTER_B4_WATER_SUPPLY_SEWER_CHECK_CHOICE        = 2,
        SHELTER_B4_WATER_SUPPLY_SEWER_WAIT_FADE           = 3,
        SHELTER_B4_WATER_SUPPLY_SEWER_WAIT_SOUND          = 4,
        SHELTER_B4_WATER_SUPPLY_SEWER_RELOAD              = 5,
        SHELTER_B4_WATER_SUPPLY_SEWER_ACCEPTED_CHOICE     = 10,
        SHELTER_B4_WATER_SUPPLY_INTERACTION_REARM_UPDATES = 10,
        SHELTER_B4_WATER_SUPPLY_RELOAD_SPRITE_VARIANT     = 1,
    };
    switch (task->state) {
        case SHELTER_B4_WATER_SUPPLY_SEWER_START_CAP:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case SHELTER_B4_WATER_SUPPLY_SEWER_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_WATER_SUPPLY_SEWER_CHECK_CHOICE:
            if (capGetVariantKey() != SHELTER_B4_WATER_SUPPLY_SEWER_ACCEPTED_CHOICE) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl    = SCENE_COMBAT_ACTORS_RUNNING;
                gDirectionInteractionRearmUpdates = SHELTER_B4_WATER_SUPPLY_INTERACTION_REARM_UPDATES;
                break;
            }
            _shelterB4WaterSupplyStartUpperSewerFade(task);
            task->state++;
            break;
        case SHELTER_B4_WATER_SUPPLY_SEWER_WAIT_FADE:
            if (--task->killCountdown == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B4_WATER_SUPPLY_EXIT_TRANSIT, 0, 0);
                task->state++;
            }
            break;
        case SHELTER_B4_WATER_SUPPLY_SEWER_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B4_WATER_SUPPLY_EXIT_TRANSIT) == 0) {
                task->state++;
            }
            break;
        case SHELTER_B4_WATER_SUPPLY_SEWER_RELOAD:
            // Publish the staged selectors only after the fade and transit sound finish.
            gDisplayState.spriteVariant                                 = SHELTER_B4_WATER_SUPPLY_RELOAD_SPRITE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = _gShelterB4WaterSupplyUpperSewerDestination.area;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = _gShelterB4WaterSupplyUpperSewerDestination.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = _gShelterB4WaterSupplyUpperSewerDestination.roomState;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(task);
            break;
    }
}

/// Refuses key-item use with the inventory's zero reply.
///
/// The callback receives message 5105, an item ID and an unused word. All
/// arguments are unused; no room state changes or payload retention occur.
static s32 _shelterB4WaterSupplyRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return 0;
}

/// Resolves room exits and stages the CAP-gated upper-sewer departure.
///
/// Borrows complete eight-byte request/reply records, which may alias. Copies
/// before variant resolution. Upper-sewer requests return zero in query and
/// execute modes; execute mode snapshots only area/warp/room and starts CAP 4.
/// Other exits return one. Deferred work borrows the room's staging storage;
/// keep this package loaded and that storage stable through its departure.
static s32 _shelterB4WaterSupplyResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B4_WATER_SUPPLY_SEWER_DEPARTURE_DESCRIPTOR = 1,
           SHELTER_B4_WATER_SUPPLY_SEWER_DEPARTURE_CAP        = 4,
           SHELTER_B4_WATER_SUPPLY_DEPARTURE_DEFERRED         = 0 };
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B4_UPPER_SEWER) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            _gShelterB4WaterSupplyUpperSewerDestination.area      = reply->areaId;
            _gShelterB4WaterSupplyUpperSewerDestination.warp      = reply->warp;
            _gShelterB4WaterSupplyUpperSewerDestination.roomState = reply->roomState;
            taskSpawnFromTable(_gShelterB4WaterSupplyDepartureTaskDescs, SHELTER_B4_WATER_SUPPLY_SEWER_DEPARTURE_DESCRIPTOR, SHELTER_B4_WATER_SUPPLY_SEWER_DEPARTURE_CAP, 0);
        }
        return SHELTER_B4_WATER_SUPPLY_DEPARTURE_DEFERRED;
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Ignores room-command messages and returns zero without changing room state.
///
/// Retains the four-word callback ABI; both command words are unused.
static s32 _shelterB4WaterSupplyIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    return 0;
}

/// Handles the valve's directed travel action to the nighttime Water Hole.
///
/// Borrows a four-byte direction request during dispatch and ignores the
/// second payload. Action 10/argument 32 either reports a blocked route, starts
/// first-use CAP 3 and its deferred departure, or immediately stages repeat-use
/// travel. The first-use flag commits even if spawning fails. Returns zero for
/// every action; keep the room resources live through any spawned transition.
static s32 _shelterB4WaterSupplyHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { SHELTER_B4_WATER_SUPPLY_ACTION_TRAVEL                    = 10,
           SHELTER_B4_WATER_SUPPLY_VALVE_USED                       = 1,
           SHELTER_B4_WATER_SUPPLY_CAP_FIRST_VALVE_USE              = 3,
           SHELTER_B4_WATER_SUPPLY_CAP_ROUTE_BLOCKED                = 1,
           SHELTER_B4_WATER_SUPPLY_MAP_MARK_BLOCKED                 = 2,
           SHELTER_B4_WATER_SUPPLY_FIRST_VALVE_DEPARTURE_DESCRIPTOR = 0 };

    if (request->actionId == SHELTER_B4_WATER_SUPPLY_ACTION_TRAVEL) {
        if (request->argument == GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE) {
            if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) != 0) {
                if (gameFlagGetNibble(GAME_FLAG_WATER_SUPPLY_VALVE_FIRST_USE) != 0) {
                    _shelterB4WaterSupplyDepartToWaterHole();
                } else {
                    gameFlagSetNibble(GAME_FLAG_WATER_SUPPLY_VALVE_FIRST_USE, SHELTER_B4_WATER_SUPPLY_VALVE_USED);
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    capRunCommandWithTransition(SHELTER_B4_WATER_SUPPLY_CAP_FIRST_VALVE_USE);
                    taskSpawnFromTable(_gShelterB4WaterSupplyDepartureTaskDescs, SHELTER_B4_WATER_SUPPLY_FIRST_VALVE_DEPARTURE_DESCRIPTOR, 0, 0);
                }
            } else {
                capRunCommandWithTransition(SHELTER_B4_WATER_SUPPLY_CAP_ROUTE_BLOCKED);
                gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, SHELTER_B4_WATER_SUPPLY_MAP_MARK_BLOCKED);
            }
        }
    }
    return 0;
}

/// Starts the water-supply sound-bank entry selected by room sound command 6.
///
/// Other integer commands are ignored. Returns zero for every command; the
/// receiver, message ID and second argument word are unused.
static s32 _shelterB4WaterSupplyHandleSound(Task* task, s32 messageId, s32 soundCommand, s32 unused)
{
    enum { SHELTER_B4_WATER_SUPPLY_SOUND_COMMAND_6 = 6 };
    if (soundCommand == SHELTER_B4_WATER_SUPPLY_SOUND_COMMAND_6) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, SHELTER_B4_WATER_SUPPLY_SOUND_COMMAND_6), 0, 0);
    }
    return 0;
}

/// Resolves a staged departure's area/warp/room through an in-place execution request.
///
/// Borrows writable departure storage and a synchronous room-variant resolver.
/// Only area, warp, room and queryOnly are initialized; the resolver must read
/// only those fields. Other message bytes are indeterminate. Stage, sound and
/// facing remain intact; the resolved halfword area narrows back to a byte.
static inline void _shelterB4WaterSupplyResolveDeparture(RoomDeparture* departurePtr, RoomVariantResolver resolveDestination)
{
    RoomEventMsg destination;

    destination.areaId    = departurePtr->area;
    destination.warp      = departurePtr->warp;
    destination.roomState = departurePtr->roomState;
    destination.queryOnly = ROOM_EVENT_EXECUTE;
    resolveDestination(&destination, &destination);
    departurePtr->area      = destination.areaId;
    departurePtr->warp      = destination.warp;
    departurePtr->roomState = destination.roomState;
}

/// Starts the valve's repeat-use departure to the nighttime water hole.
///
/// Holds scripted player control, resolves the destination's progress-dependent
/// room with an in-place execution request, and copies the departure into global
/// staging before spawning its task. Warp 3/room 1 arrive facing a quarter turn
/// (4096 units per turn). An eligible live companion gets schedule 6 even if
/// spawning fails. The departure task borrows staging until the transition ends.
static void _shelterB4WaterSupplyDepartToWaterHole(void)
{
    enum { WATER_HOLE_ARRIVAL_WARP      = 3,
           WATER_HOLE_ARRIVAL_ROOM      = 1,
           COMPANION_FOLLOW_UP_SCHEDULE = 6,
           DEPARTURE_SOUND_SCRIPT       = 3 };
    RoomDeparture       departure;
    RoomVariantResolver resolveDestination = _shelterB4WaterSupplyResolveWaterHoleVariant;

    departure.stage     = GAME_STAGE_DRYFIELD_NIGHT;
    departure.area      = GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE;
    departure.warp      = WATER_HOLE_ARRIVAL_WARP;
    departure.roomState = WATER_HOLE_ARRIVAL_ROOM;
    departure.sndEvent  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, DEPARTURE_SOUND_SCRIPT);
    departure.facing    = ACTOR_TRANSFORM_ANGLE_TURN / 4;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    _shelterB4WaterSupplyResolveDeparture(&departure, resolveDestination);
    gRoomDeparture = departure;
    taskSpawnFromTable(&_gShelterB4WaterSupplyWaterHoleDepartureTaskDesc, 0, 0, 0);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL && gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_FOLLOW_UP_SCHEDULE);
    }
}

/// Waits for the valve's first-use CAP scene, then publishes Water Hole travel.
///
/// Start bodyless after CAP 3 and retain the room until CAP becomes idle.
/// Resolves the destination selectors synchronously, snapshots the departure,
/// starts its task and updates an eligible companion's schedule before killing
/// itself. Spawn failure still commits staging and schedule. Arrival uses warp
/// 3, base room 1 and a quarter-turn facing in 4096-unit angles.
static void _shelterB4WaterSupplyFirstValveDepartureTask(Task* task)
{
    enum { WATER_HOLE_ARRIVAL_WARP      = 3,
           WATER_HOLE_ARRIVAL_ROOM      = 1,
           COMPANION_FOLLOW_UP_SCHEDULE = 6,
           DEPARTURE_SOUND_SCRIPT       = 3 };
    RoomDeparture       departure;
    RoomEventMsg        destination;
    RoomVariantResolver resolveDestination;

/// Resolves the first valve departure's selectors without copying other message bytes.
///
/// departure and destinationMessage are distinct writable RoomDeparture and
/// RoomEventMsg lvalues, evaluated repeatedly; pass side-effect-free storage.
/// resolver is a RoomVariantResolver evaluated once and must accept an aliased
/// request/reply with only area/warp/room/queryOnly initialized. Copies the resolved
/// selectors back, narrowing area to a byte; other departure fields stay intact.
#define SHELTER_B4_WATER_SUPPLY_RESOLVE_FIRST_VALVE_DESTINATION(departure, destinationMessage, resolver) \
    {                                                                                                    \
        (destinationMessage).areaId    = (departure).area;                                               \
        (destinationMessage).warp      = (departure).warp;                                               \
        (destinationMessage).roomState = (departure).roomState;                                          \
        (destinationMessage).queryOnly = ROOM_EVENT_EXECUTE;                                             \
        (resolver)(&(destinationMessage), &(destinationMessage));                                        \
        (departure).area      = (destinationMessage).areaId;                                             \
        (departure).warp      = (destinationMessage).warp;                                               \
        (departure).roomState = (destinationMessage).roomState;                                          \
    }

    if (capIsBusy() == 0) {
        resolveDestination  = _shelterB4WaterSupplyResolveWaterHoleVariant;
        departure.stage     = GAME_STAGE_DRYFIELD_NIGHT;
        departure.area      = GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE;
        departure.warp      = WATER_HOLE_ARRIVAL_WARP;
        departure.roomState = WATER_HOLE_ARRIVAL_ROOM;
        departure.sndEvent  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_WATER_SUPPLY, DEPARTURE_SOUND_SCRIPT);
        departure.facing    = ACTOR_TRANSFORM_ANGLE_TURN / 4;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        SHELTER_B4_WATER_SUPPLY_RESOLVE_FIRST_VALVE_DESTINATION(departure, destination, resolveDestination);
        gRoomDeparture = departure;
        taskSpawnFromTable(&_gShelterB4WaterSupplyWaterHoleDepartureTaskDesc, 0, 0, 0);
        if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL && gameFlagGetNibble(GAME_FLAG_0CF) == 0) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_FOLLOW_UP_SCHEDULE);
        }
        taskKill(task);
    }
#undef SHELTER_B4_WATER_SUPPLY_RESOLVE_FIRST_VALVE_DESTINATION
}

/// Registers the room receiver, installs its messages and starts the water task.
///
/// Requires a new room task at state zero. Its borrowed message table remains
/// loaded while the receiver occupies `GAME_TASK_SLOT_ROOM`.
static void _shelterB4WaterSupplyInitializeRoom(Task* task)
{
    task->msgTable = _gShelterB4WaterSupplyRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(&_gShelterB4WaterSupplyWaterTaskDesc, 0, 0, 0);
    task->state = task->state + 1;
}

/// Keeps the initialized room receiver available for messages without per-frame work.
static void _shelterB4WaterSupplyIdleRoom(Task* task)
{
}

void shelterB4WaterSupplyRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB4WaterSupplyRoomStates;
    states.funcs[task->state](task);
}

/// Resolves the night Water Hole's arrival variant from the underpass progress flags.
///
/// Requests use Dryfield-night area IDs. Only execution requests for the
/// Water Hole modify `reply->roomState`: flag 0x51
/// selects 1 when set or 2 when clear, and flag 0x53 selects the second pair
/// (3 or 4). Other fields and queries remain untouched. The borrowed request
/// and writable reply may alias. Always returns 1; no record is retained.
static s32 _shelterB4WaterSupplyResolveWaterHoleVariant(RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B4_WATER_SUPPLY_WATER_HOLE_SWITCH_SET_VARIANT   = 1,
        SHELTER_B4_WATER_SUPPLY_WATER_HOLE_SWITCH_CLEAR_VARIANT = 2,
        SHELTER_B4_WATER_SUPPLY_WATER_HOLE_SECOND_PAIR_OFFSET   = 2
    };
    if (request->areaId == GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
            reply->roomState = SHELTER_B4_WATER_SUPPLY_WATER_HOLE_SWITCH_CLEAR_VARIANT;
        } else {
            reply->roomState = SHELTER_B4_WATER_SUPPLY_WATER_HOLE_SWITCH_SET_VARIANT;
        }
        if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
            reply->roomState = (u8)reply->roomState + SHELTER_B4_WATER_SUPPLY_WATER_HOLE_SECOND_PAIR_OFFSET;
        }
    }
    return 1;
}

/// Selects the compact descriptor type of the Z-running water rectangle list.
///
/// The shared drawer reads signed-halfword world X/Z geometry and advances
/// through 10-byte records. The matching `WATER_WAVE_STRIPS_IS_LIST_END`
/// binding tests `listMarker`, including the final -1 record, through a const
/// descriptor pointer. `RoomCompactWaterSurface` must already be visible;
/// this type binding is consumed and undefined by the shared strip include.
#define WATER_WAVE_STRIPS_SURFACE_T RoomCompactWaterSurface
/// Tests the list terminator in a `RoomCompactWaterSurface` descriptor.
///
/// Reads the signed 16-bit `listMarker` from a valid descriptor once and
/// returns an int (0 drawable entry, 1 list end), without reading geometry.
/// Captures no caller locals and adds no side effects. The shared strip
/// include consumes and undefines this override; see its predicate contract.
#define WATER_WAVE_STRIPS_IS_LIST_END(surface) ((surface)->listMarker == WATER_SURFACE_LIST_END)
/// Binds the readable Z-running rectangle list, including its in-bounds terminator.
///
/// Supplies a `RoomCompactWaterSurface*` without side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_SURFACES _gShelterB4WaterSupplyWaterWaveSurfaces
/// Binds the undisplaced water Y in signed world units.
///
/// Read once per draw from the room's `s16`; no side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_HEIGHT _gShelterB4WaterSupplyWaterY
/// Binds the writable byte cursor for mixed water quad and draw-mode packets.
///
/// A stable `u8*` lvalue, read and advanced repeatedly; requires word alignment
/// and a borrowed arena reserved until GPU consumption. The shared strip
/// include consumes and undefines this binding.
/// The caller resets it once before both drawers, which append in draw order.
#define WATER_WAVE_STRIPS_PACKET_CURSOR _gShelterB4WaterSupplyWaterPacketCursor
/// Scales the seam's sine displacement to -64..64 world-coordinate Y units.
///
/// Integer shift count for `water_wave_strips.inc.c`; see its configuration
/// contract. The include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 6
#include "../../shared/effects/water_wave_strips.inc.c"

/// Draws the X-running water rectangle as two strips sharing a moving wave crest.
///
/// The terminated descriptor list supplies signed world-unit geometry; each
/// strip has 16 quads, red outer edges and a grey seam displaced by -64..64 Y
/// units. The phase advances by -16 of 4096 angle units per display frame.
/// Appends subtractive Gouraud quads and draw modes to the word-aligned byte
/// arena, at most 0x600 bytes for this list. Requires the caller to reserve
/// that arena through GPU consumption and initialize the cursor beforehand.
/// Borrows one `WaterQuadScratch` block and overwrites GTE state. `task` is unused.
static void _shelterB4WaterSupplyDrawXWaveStrips(Task* task)
{
    enum {
        SHELTER_B4_WATER_SUPPLY_WAVE_SEGMENTS_PER_STRIP = 16,
        SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_STEP_SHIFT   = 9, // 512 angle units per segment
        SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_PER_FRAME    = 16,
        SHELTER_B4_WATER_SUPPLY_WAVE_AMPLITUDE_SHIFT    = 6  // Q12 sine to -64..64 world units
    };
    SVECTOR                  vertex0, vertex1, vertex2, vertex3;
    long                     screenXY0, screenXY1, screenXY2, screenXY3;
    long                     projectionScale, projectionFlags;
    s32                      wavePhase;
    RoomCompactWaterSurface* surface;
    WaterQuadScratch*        strip;
    WaterQuadScratch*        scratchTop;
    POLY_G4*                 quad;
    DR_MODE*                 drawMode;
    s32                      depth;
    s32                      segment;

    /// Queues the completed quad and its subtractive mode into one depth bucket.
    ///
    /// Captures `quad`, `depth`, `drawMode`, the byte cursor and display state.
    /// Takes no arguments; invoke only inside a braced block. Prepending the
    /// mode last makes the GPU consume it first. Undefined before leaving.
    /// quad is a completed POLY_G4 already reserved by the caller. The macro
    /// reserves a full 12-byte DR_MODE but submits one command word; the spare
    /// word is untouched. Both packets share the unbiased projected depth.
#define SHELTER_B4_WATER_SUPPLY_QUEUE_WATER_QUAD()                                               \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth), quad);                                     \
    drawMode                                = (DR_MODE*)_gShelterB4WaterSupplyWaterPacketCursor; \
    _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(drawMode + 1);                               \
    setDrawTPage(drawMode, 0, 0, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));                       \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(depth), drawMode)

    surface                    = _gShelterB4WaterSupplyXWaterWaveSurfaces;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchTop                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    wavePhase                  = -(gDisplayState.animFrame * SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_PER_FRAME);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchTop - 1;
    strip                                  = scratchTop - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    strip->y = _gShelterB4WaterSupplyWaterY;
    // Each rectangle has two strips with a shared sine-displaced seam.
    for (; surface->listMarker != WATER_SURFACE_LIST_END; surface++) {
        strip->dx = surface->width / SHELTER_B4_WATER_SUPPLY_WAVE_SEGMENTS_PER_STRIP;
        strip->dz = surface->depth / 2;
        strip->x  = surface->x;
        strip->z  = surface->z;
        // First strip: the flat outer edge to the raised seam.
        for (segment = 0; segment < SHELTER_B4_WATER_SUPPLY_WAVE_SEGMENTS_PER_STRIP; segment++) {
            vertex0.vx     = strip->x + strip->dx * segment;
            vertex0.vy     = strip->y;
            vertex0.vz     = strip->z;
            vertex1.vx     = strip->x + strip->dx * (segment + 1);
            vertex1.vy     = strip->y;
            vertex1.vz     = strip->z;
            strip->yOffset = (u32)rsin(wavePhase + (segment << SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_WATER_SUPPLY_WAVE_AMPLITUDE_SHIFT;
            vertex2.vx     = strip->x + strip->dx * segment;
            vertex2.vy     = strip->y + strip->yOffset;
            vertex2.vz     = strip->z + strip->dz;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_WATER_SUPPLY_WAVE_AMPLITUDE_SHIFT;
            vertex3.vx     = strip->x + strip->dx * (segment + 1);
            vertex3.vy     = strip->y + strip->yOffset;
            vertex3.vz     = strip->z + strip->dz;
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                                    = (POLY_G4*)_gShelterB4WaterSupplyWaterPacketCursor;
                _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r0                       = 0x80;
                quad->r1                       = 0x80;
                quad->g0                       = 0;
                quad->b0                       = 0;
                quad->g1                       = 0;
                quad->b1                       = 0;
                quad->r2                       = 0x20;
                quad->g2                       = 0x20;
                quad->b2                       = 0x20;
                quad->r3                       = 0x20;
                quad->g3                       = 0x20;
                quad->b3                       = 0x20;
                SHELTER_B4_WATER_SUPPLY_QUEUE_WATER_QUAD();
            }
        }
        // Second strip: the shared seam to the opposite flat edge.
        for (segment = 0; segment < SHELTER_B4_WATER_SUPPLY_WAVE_SEGMENTS_PER_STRIP; segment++) {
            strip->yOffset = (u32)rsin(wavePhase + (segment << SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_WATER_SUPPLY_WAVE_AMPLITUDE_SHIFT;
            vertex0.vx     = strip->x + strip->dx * segment;
            vertex0.vy     = strip->y + strip->yOffset;
            vertex0.vz     = strip->z + strip->dz;
            strip->yOffset = (u32)rsin(wavePhase + ((segment + 1) << SHELTER_B4_WATER_SUPPLY_WAVE_PHASE_STEP_SHIFT)) >> SHELTER_B4_WATER_SUPPLY_WAVE_AMPLITUDE_SHIFT;
            vertex1.vx     = strip->x + strip->dx * (segment + 1);
            vertex1.vy     = strip->y + strip->yOffset;
            vertex1.vz     = strip->z + strip->dz;
            vertex2.vx     = strip->x + strip->dx * segment;
            vertex2.vy     = strip->y;
            vertex2.vz     = strip->z + strip->dz * 2;
            vertex3.vx     = strip->x + strip->dx * (segment + 1);
            vertex3.vy     = strip->y;
            vertex3.vz     = strip->z + strip->dz * 2;
            depth          = RotTransPers4(&vertex0, &vertex1, &vertex2, &vertex3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &projectionScale, &projectionFlags);
            if (projectionFlags >= 0) {
                quad                                    = (POLY_G4*)_gShelterB4WaterSupplyWaterPacketCursor;
                _gShelterB4WaterSupplyWaterPacketCursor = (u8*)(quad + 1);
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                GPU_PRIMITIVE_XY_WORD(quad, 0) = screenXY0;
                GPU_PRIMITIVE_XY_WORD(quad, 1) = screenXY1;
                GPU_PRIMITIVE_XY_WORD(quad, 2) = screenXY2;
                GPU_PRIMITIVE_XY_WORD(quad, 3) = screenXY3;
                quad->r2                       = 0x80;
                quad->r3                       = 0x80;
                quad->g2                       = 0;
                quad->b2                       = 0;
                quad->g3                       = 0;
                quad->b3                       = 0;
                quad->r0                       = 0x20;
                quad->g0                       = 0x20;
                quad->b0                       = 0x20;
                quad->r1                       = 0x20;
                quad->g1                       = 0x20;
                quad->b1                       = 0x20;
                SHELTER_B4_WATER_SUPPLY_QUEUE_WATER_QUAD();
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
#undef SHELTER_B4_WATER_SUPPLY_QUEUE_WATER_QUAD
}

/// Runs water initialization or drawing and publishes the signed world-unit water Y.
///
/// `task->state` must be 0 (initialize) or 1 (draw). The room starts this
/// bodyless task at zero and keeps the package loaded for its lifetime.
static void _shelterB4WaterSupplyWaterTask(Task* task)
{
    TaskFunc states[] = { _shelterB4WaterSupplyInitializeWater, _shelterB4WaterSupplyDrawWater };

    states[task->state](task);
    gGameSession->waterY = _gShelterB4WaterSupplyWaterY;
}

/// Prepares the unused actor-load buffer for water packets and enters the drawing state.
///
/// A saved companion selects buffer 1; without one, buffer 2 is reused.
/// Clears its session marker, whose nonzero meaning is unproven. Requires
/// `task->state == 0`; this initialization does not draw.
static void _shelterB4WaterSupplyInitializeWater(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state = task->state + 1;
}

/// Selects the current display half of the borrowed actor-load packet arena.
///
/// Selects actor-load base 2 without a companion, otherwise base 1, and resets
/// the byte cursor to its `otBuffer` half (0 or 1), each 0xC000 bytes. Existing
/// actor data in that half must no longer be needed. The word-aligned half must
/// fit both append drawers and remain reserved through GPU consumption.
/// Reserves no storage and clears no packets; call once before the two drawers.
static inline void _shelterB4WaterSupplyResetWaterPackets(void)
{
    enum { SHELTER_B4_WATER_SUPPLY_COMPANION_NONE          = 0,
           SHELTER_B4_WATER_SUPPLY_WATER_PACKET_HALF_BYTES = 0xC000 };
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == SHELTER_B4_WATER_SUPPLY_COMPANION_NONE) {
        _gShelterB4WaterSupplyWaterPacketCursor = gFsActorBuffer2Base + gDisplayState.otBuffer * SHELTER_B4_WATER_SUPPLY_WATER_PACKET_HALF_BYTES;
    } else {
        _gShelterB4WaterSupplyWaterPacketCursor = gFsActorBuffer1Base + gDisplayState.otBuffer * SHELTER_B4_WATER_SUPPLY_WATER_PACKET_HALF_BYTES;
    }
}

/// Draws both water lists into one current-display packet half.
///
/// Requires initialized water state and actor storage reserved through GPU
/// consumption. Two rectangles emit at most 0xC00 bytes in a 0xC000-byte half.
/// `task` is forwarded only for the drawers' task-compatible signatures.
static void _shelterB4WaterSupplyDrawWater(Task* task)
{
    // Reset once: the X-running list appends after the Z-running list's packets.
    _shelterB4WaterSupplyResetWaterPackets();
    _waterDrawWaveStrips(task);
    _shelterB4WaterSupplyDrawXWaveStrips(task);
}

/// Samples a player part's movement to attempt a ripple and an upward spray.
///
/// `controllerWork` borrows the live controller work; its `angle` halfword
/// holds the narrowed odds and retains the final spray threshold on return.
/// `historyIndex` is 0 or 1, corresponding to player coordinate 14 or 17.
/// Composes the live part into view space and compares full-width XYZ against
/// the signed-halfword history. Odds retain the low signed 16 bits, with no
/// clamp: ripple adds 32 before its roll, then spray subtracts that bias.
/// Each test consumes one LCG draw, in ripple/spray order, against 0..511.
/// Records the new position even when neither spawn succeeds; paused sampling
/// leaves history frozen, so this measures movement since the last sample.
///
/// The part and view must share a composition root, with the view cache current.
/// Placement converts back to world coordinates and replaces Y with the live
/// water height. These effect IDs must select callbacks that use the spawner's
/// placement snapshot: the retained source pointer addresses stack storage.
static inline void _shelterB4WaterSupplyEmitPartSplashes(EffectWork* controllerWork, GfxCoord* trackedPart, s32 historyIndex)
{
    enum { SPLASH_ROLL_MASK   = 0x1FF,
           SPLASH_ROLL_SHIFT  = 16,
           RIPPLE_ODDS_BIAS   = 32,
           RIPPLE_HALF_SIDE   = 64,
           SPRAY_SIZE         = 384,
           SPRAY_CELL_UPDATES = 2,
           SPRAY_LAUNCH_SPEED = 32,
           SPRAY_UPWARD_BURST = 1,
           SPRAY_PERIOD_SHIFT = 12,
           SPRAY_SPEED_SHIFT  = 16,
           SPRAY_KIND_SHIFT   = 24,
           SPRAY_SPAWN_ARG    = SPRAY_SIZE | (SPRAY_CELL_UPDATES << SPRAY_PERIOD_SHIFT) |
                             (SPRAY_LAUNCH_SPEED << SPRAY_SPEED_SHIFT) | (SPRAY_UPWARD_BURST << SPRAY_KIND_SHIFT) };
    GfxCoord surfaceCoord;
    u32      randomRoll;
    actorRenderComposeCoord(trackedPart);
    // Odds narrow to s16 before each comparison; ripple gets the bias first.
    controllerWork->angle = ABS(_gShelterB4WaterSupplyPlayerSplashPositionHistory[historyIndex].vx - trackedPart->workm.t[0]) +
                            ABS(_gShelterB4WaterSupplyPlayerSplashPositionHistory[historyIndex].vy - trackedPart->workm.t[1]) +
                            ABS(_gShelterB4WaterSupplyPlayerSplashPositionHistory[historyIndex].vz - trackedPart->workm.t[2]) + RIPPLE_ODDS_BIAS;
    // Place both effects on the water plane in the view coordinate's local space.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &trackedPart->workm, &surfaceCoord.coord);
    surfaceCoord.parent       = &gGfxViewCoord;
    surfaceCoord.coord.t[1]   = gGameSession->waterY;
    surfaceCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&surfaceCoord);
    randomRoll = (RANDOM_LCG_ADVANCE());
    if ((s32)((randomRoll >> SPLASH_ROLL_SHIFT) & SPLASH_ROLL_MASK) < controllerWork->angle) {
        effectSpawn(gRoomEffectWaterRippleId, &surfaceCoord, RIPPLE_HALF_SIDE, NULL);
    }
    controllerWork->angle -= RIPPLE_ODDS_BIAS;
    randomRoll             = (RANDOM_LCG_ADVANCE());
    if ((s32)((randomRoll >> SPLASH_ROLL_SHIFT) & SPLASH_ROLL_MASK) < controllerWork->angle) {
        effectSpawn(gRoomEffectWaterSprayId, &surfaceCoord, SPRAY_SPAWN_ARG, NULL);
    }
    _gShelterB4WaterSupplyPlayerSplashPositionHistory[historyIndex].vx = trackedPart->workm.t[0];
    _gShelterB4WaterSupplyPlayerSplashPositionHistory[historyIndex].vy = trackedPart->workm.t[1];
    _gShelterB4WaterSupplyPlayerSplashPositionHistory[historyIndex].vz = trackedPart->workm.t[2];
}

void shelterB4WaterSupplySplashAndLightBeamsTask(Task* task)
{
    enum { SPLASH_INITIALIZE   = 0,
           SPLASH_ACTIVE       = 1,
           SPLASH_SAMPLE_COUNT = ARRAY_SIZE(_gShelterB4WaterSupplyPlayerSplashPositionHistory),
           SPLASH_FIRST_PART   = 14,
           SPLASH_PART_STRIDE  = 3,
           BEAM_RADIUS_SCALE   = 512 };
    Task*       playerTask;
    EffectWork* work;
    GfxCoord*   playerRoot;
    GfxCoord*   trackedPart;
    s32         sampleIndex;

    work       = task->spawnArg2.pointer;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerRoot = playerTask->extra.tmd->coords;
    // Seed cached samples without composing the player parts on the first tick.
    if (task->state == SPLASH_INITIALIZE) {
        gRoomEffectWaterRippleId  = EFFECT_SHELTER_B4_WATER_SUPPLY_WATER_RIPPLE;
        gRoomEffectWaterSprayId   = EFFECT_SHELTER_B4_WATER_SUPPLY_WATER_SPRAY;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B4_WATER_SUPPLY_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B4_WATER_SUPPLY_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B4_WATER_SUPPLY_ORANGE_BURST_2;
        task->state               = SPLASH_ACTIVE;
        for (sampleIndex = 0; sampleIndex < SPLASH_SAMPLE_COUNT; sampleIndex++) {
            trackedPart                                                       = &playerTask->extra.tmd->coords[SPLASH_FIRST_PART + sampleIndex * SPLASH_PART_STRIDE];
            _gShelterB4WaterSupplyPlayerSplashPositionHistory[sampleIndex].vx = trackedPart->workm.t[0];
            _gShelterB4WaterSupplyPlayerSplashPositionHistory[sampleIndex].vy = trackedPart->workm.t[1];
            _gShelterB4WaterSupplyPlayerSplashPositionHistory[sampleIndex].vz = trackedPart->workm.t[2];
        }
    } else if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && gGameSession->waterY < playerRoot->coord.t[1]) {
        sampleIndex = 0;
        for (; sampleIndex < SPLASH_SAMPLE_COUNT; sampleIndex++) {
            trackedPart = &playerTask->extra.tmd->coords[SPLASH_FIRST_PART + sampleIndex * SPLASH_PART_STRIDE];
            _shelterB4WaterSupplyEmitPartSplashes(work, trackedPart, sampleIndex);
        }
    }
    // View-specific beams keep drawing while splash sampling is paused.
    switch ((u8)viewGetMappedIndex()) {
        case 4:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[0], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[2], BEAM_RADIUS_SCALE, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            /* fallthrough */
        case 2:
        case 3:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyViews234LightBeamEndpoints, BEAM_RADIUS_SCALE, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            break;
        case 6:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[1], BEAM_RADIUS_SCALE, 0);
            /* fallthrough */
        case 5:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyViews56LightBeamEndpoints, BEAM_RADIUS_SCALE, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            break;
        case 7:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[1], BEAM_RADIUS_SCALE, 0);
            break;
        case 8:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[0], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[2], BEAM_RADIUS_SCALE, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            break;
        case 9:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[4], BEAM_RADIUS_SCALE, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            /* fallthrough */
        case 10:
        case 11:
            glowDrawDimGreyCapsule(_gShelterB4WaterSupplyLightBeamEndpoints[3], BEAM_RADIUS_SCALE, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            break;
    }
}

#include "../../shared/effects/water_ripple_task.inc.c"

void shelterB4WaterSupplyWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task.inc.c"

void shelterB4WaterSupplyWaterDriftTask(Task* task)
{
    _waterDriftTask(task);
}

#include "../../shared/effects/water_spin.inc.c"

#include "../../shared/effects/glow_draw_cone.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

void shelterB4WaterSupplyRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB4WaterSupplyRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void shelterB4WaterSupplyRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
