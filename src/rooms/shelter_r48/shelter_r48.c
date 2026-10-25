#include "rooms/shelter_r48.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_503500.h"
#include "actors/actor_403600.h"
#include "actors/actor_503500.h"

#include "gameplay/display.h"
#include "gameplay/player_actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
/// Empty presence flag: this room supplies `_waterDrawSpinU16` and
/// `_waterDrawTileU16`.
///
/// Defined, with no replacement list, immediately before `water_effects.h`.
/// `defined()` is the only test. That header prototypes the shared drawers
/// only when `WATER_SHARED_U16_DRAWERS` is set
/// and this flag is not. This file prototypes and defines both drawers and
/// includes `water_drift_task_u16.inc.c` instead of the shared bodies. The
/// spin drawer takes a `u16` index and an `s16` scale and angle. The tile
/// drawer keeps `s32` parameters, uses texture page 0x2C and clut 0x4393,
/// and starts its cell grid at v 0.
#define WATER_OWN_U16_DRAWERS
#include "../../shared/effects/water_effects.h"
#include "../../shared/effects/effect_sprite.h"

static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTileU16(const GfxCoord* coord, s32 textureCell, s32 radiusScale);

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// Three ring-wall bands followed by three shockwave bands.
enum { SHELTER_R48_RING_BAND_COUNT = 6 };

static void _shelterR48RoomSetupState(Task* task);
static void _shelterR48RoomIdleState(Task* task);
static void _shelterR48DrawRingBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex);
static void _shelterR48DrawRoomGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
static void _shelterR48DrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _shelterR48DrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _shelterR48DrawGlowBeam(const GfxCoord* coord, s16 radiusScale, s32 yaw, s32 packedColor);

static WorldCollisionTrigger _gShelterR48ViewBoundaryTriggers[16];
static WorldCollisionTrigger _gShelterR48ActionTriggers[5];
static WorldCoordRoomLights  _gShelterR48RoomLights;
enum {
    SHELTER_R48_CARD_USE_READY   = 1,
    SHELTER_R48_CARD_USE_STARTED = 2
};

static s32 _shelterR48UseStaffCard(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterR48ResolveTransitionMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterR48IgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static s32 _shelterR48RoomActionMsg(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _shelterR48AdvanceBossSceneMsg(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static TmdBone _gShelterR48WaterRefractionSkeleton[1];
static void    _shelterR48WaterRefractionTask(Task* task);

/// Byte capacity of each of the water-refraction model's two packet-buffer halves.
enum { SHELTER_R48_REFRACTION_BUFFER_HALF_BYTES = 0x4000 };

/// Empty part and pre-transformed command groups for the water-refraction TMD body.
///
/// Three writable command words provide no geometry. The task instead fills
/// its TMD object's packet buffer directly; the stream ends after both groups.
static u32 _gShelterR48WaterRefractionStream[3] = {
    TMD_STREAM_GROUP_END,
    TMD_STREAM_GROUP_END,
    TMD_STREAM_END,
};

/// Synthetic one-part model providing the water-refraction task's owned packet buffer.
///
/// No vertices or normals are supplied: the callback writes framebuffer quads
/// directly into two 0x4000-byte halves. The descriptor skips automatic buffer
/// allocation; the first active draw allocates it. Runtime models borrow this
/// writable source and stream until teardown; the initial pose is copied.
static TmdSource _gShelterR48WaterRefractionSource = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = SHELTER_R48_REFRACTION_BUFFER_HALF_BYTES,
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gShelterR48WaterRefractionSkeleton),
    .partVertexCounts    = NULL,
    .verts               = NULL,
    .normals             = NULL,
    .skeleton            = _gShelterR48WaterRefractionSkeleton,
    .stream              = _gShelterR48WaterRefractionStream,
};

TaskDesc gShelterR48WaterRefractionTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _shelterR48WaterRefractionTask, { .model = &_gShelterR48WaterRefractionSource } };

/// Room-task handlers for transitions, staff cards, actions, commands and boss events.
///
/// The setup state installs this borrowed six-record table for the room task's
/// lifetime. Payloads are native pointers or integer words as each message
/// requires. The final marker makes unsupported message IDs return zero.
static TaskMessageEntry _gShelterR48RoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterR48ResolveTransitionMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterR48UseStaffCard },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterR48RoomActionMsg },
    { ROOM_MESSAGE_COMMAND, _shelterR48IgnoreRoomCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _shelterR48AdvanceBossSceneMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Radius, lift and spread offsets for six animated bands, in local game units.
///
/// Rows 0..2 form the ring wall; rows 3..5 form the shockwave, with separate
/// palettes. Each row is paired with the same row of the texture-phase table.
static EffectBandShape _gShelterR48RingBandShapes[SHELTER_R48_RING_BAND_COUNT] = {
    { .baseRadius = 512, .lift = 0, .spread = 3072 },
    { .baseRadius = 512, .lift = 1024, .spread = 1024 },
    { .baseRadius = 512, .lift = -1024, .spread = 1024 },
    { .baseRadius = 512, .lift = 2048, .spread = 3072 },
    { .baseRadius = 512, .lift = 4096, .spread = 2048 },
    { .baseRadius = 512, .lift = 6144, .spread = 1024 },
};

/// World-space centre of the progress-dependent orange or blue room glow, in game units.
static SVECTOR _gShelterR48GlowWorldPosition = { .vx = 8210, .vy = -2920, .vz = -370, .pad = 0 };

WorldCoordRoomLighting gShelterR48RoomLighting[1] = {
    { .lights = &_gShelterR48RoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gShelterR48RoomCollisionResources[SHELTER_R48_ROOM_COUNT] = {
    { .grid = &gShelterR48CollisionGrid, .viewBoundaryTriggers = _gShelterR48ViewBoundaryTriggers, .actionTriggers = _gShelterR48ActionTriggers, .occluders = NULL },
};

u8* gShelterR48ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterR48ViewCounts[SHELTER_R48_ROOM_COUNT] = { SHELTER_R48_VIEW_COUNT };

DirectionWarpEntry gShelterR48WarpEntries[SHELTER_R48_WARP_COUNT] = {
    { .player = { { .word = 1024 }, 1542, -1998, 7374 }, .unknown_10 = { 0 }, .companion = { { .word = 1024 }, 1542, -1998, 7374 }, .unknown_24 = { 0 }, .arrivalSound = DIRECTION_WARP_SOUND_NONE, .departureSound = DIRECTION_WARP_SOUND_NONE, .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY },
};

enum {
    SHELTER_R48_COLLISION_CELL_COUNT_X = 5,
    SHELTER_R48_COLLISION_CELL_COUNT_Z = 7,
    SHELTER_R48_COLLISION_CELL_SIZE    = 4000,
};

/// Room-space Q12 normals; the first four are rebuilt from the boss's body.
static SVECTOR _gShelterR48CollisionGridNormals[40] = {
#include "assets/shelter_r48_collision_0692C_normals.inc"
};

/// Room-space vertices in game units; the first eight track the boss's body.
static SVECTOR _gShelterR48CollisionGridVerts[136] = {
#include "assets/shelter_r48_collision_0692C_verts.inc"
};

/// Arena topology with four initially empty face slots for the boss's body.
static WorldCollisionGridFace _gShelterR48CollisionGridFaces[83] = {
#include "assets/shelter_r48_collision_0692C_faces.inc"
};

/// Face-index lists ending in `WORLD_COLLISION_GRID_CELL_END`, shared by grid cells.
static s16 _gShelterR48CollisionGridCells[584] = {
#include "assets/shelter_r48_collision_0692C_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterR48CollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..583
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_R48_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterR48CollisionGridCells[(halfwordOffset)])
/// X-major cell directory; each entry borrows a terminated face-index list.
static s16* _gShelterR48CollisionGridTable[SHELTER_R48_COLLISION_CELL_COUNT_X * SHELTER_R48_COLLISION_CELL_COUNT_Z] = {
#include "assets/shelter_r48_collision_0692C_table.inc"
};
#undef SHELTER_R48_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterR48CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterR48CollisionGridNormals,
    .vertices    = _gShelterR48CollisionGridVerts,
    .faces       = _gShelterR48CollisionGridFaces,
    .cellFaceIds = _gShelterR48CollisionGridTable,
    .xBias       = 145,
    .zBias       = 6073,
    .cellCountX  = SHELTER_R48_COLLISION_CELL_COUNT_X,
    .cellCountZ  = SHELTER_R48_COLLISION_CELL_COUNT_Z,
    .cellSize    = SHELTER_R48_COLLISION_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gShelterR48CollisionGridFaces),
};

ViewCamera gShelterR48ViewCameras[SHELTER_R48_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -7510, 0x61A8, -6980 } }, 329 },
    { { { { 3552, 0, -2038 }, { -271, 4059, -472 }, { 2020, 545, 3520 } }, { -1090, 3740, -2260 } }, 230 },
    { { { { -2135, 0, -3495 }, { -1970, 3383, 1203 }, { 2887, 2309, -1763 } }, { -228, 6620, -0x3546 } }, 207 },
    { { { { -3450, 0, -2206 }, { -862, 3770, 1348 }, { 2031, 1600, -3176 } }, { -5170, 5010, -0x3E8A } }, 230 },
    { { { { -3701, 0, 1754 }, { -150, 4080, -318 }, { -1747, -352, -3687 } }, { -0x3904, 830, -0x3D7C } }, 230 },
    { { { { -3530, 0, 2076 }, { 271, 4060, 461 }, { -2059, 534, -3500 } }, { -0x3A51, 3700, -0x2DA7 } }, 230 },
    { { { { 2122, 0, 3503 }, { 1951, 3401, -1181 }, { -2909, 2281, 1762 } }, { -0x3D7D, 6620, -358 } }, 207 },
    { { { { 3522, 0, 2090 }, { 928, 3669, -1564 }, { -1872, 1819, 3155 } }, { -0x297C, 5240, 1830 } }, 230 },
    { { { { 3679, 0, -1799 }, { 221, 4064, 453 }, { 1785, -505, 3651 } }, { -1409, 740, 1750 } }, 230 },
    { { { { 972, 0, 3978 }, { -1850, 3626, 452 }, { -3522, -1904, 860 } }, { -9250, -530, -5320 } }, 329 },
    { { { { -1452, 0, -3829 }, { 2603, 3004, -987 }, { 2809, -2784, -1065 } }, { -982, 9358, -9124 } }, 329 },
    { { { { -970, 0, -3979 }, { 326, 4082, -79 }, { 3966, -336, -966 } }, { 2690, 3160, -8680 } }, 329 },
    { { { { 959, 0, 3982 }, { -1734, 3687, 417 }, { -3584, -1783, 863 } }, { -6415, 1003, -6020 } }, 329 },
    { { { { 4030, 0, -730 }, { 316, 3690, 1747 }, { 658, -1776, 3631 } }, { -7440, 1530, -2760 } }, 289 },
    { { { { 2883, 0, 2909 }, { -1179, 3744, 1168 }, { -2659, -1660, 2635 } }, { -0x3070, 1700, -2500 } }, 329 },
    { { { { 2930, 0, 2861 }, { -662, 3984, 677 }, { -2784, -947, 2850 } }, { -0x2C56, 2700, -3550 } }, 329 },
    { { { { -970, 0, -3979 }, { 326, 4082, -79 }, { 3966, -336, -966 } }, { 2690, 3160, -8680 } }, 329 },
    { { { { 2122, 0, 3503 }, { 1951, 3401, -1181 }, { -2909, 2281, 1762 } }, { -0x3D7D, 6620, -358 } }, 207 },
};

/// Eight-bit, opaque-blend texture pages at VRAM Y=0 and palette rows at X=0.
enum {
    SHELTER_R48_SPRITE_TPAGE_832     = getTPage(1, 0, 832, 0),
    SHELTER_R48_SPRITE_TPAGE_896     = getTPage(1, 0, 896, 0),
    SHELTER_R48_SPRITE_TPAGE_960     = getTPage(1, 0, 960, 0),
    SHELTER_R48_SPRITE_CLUT_ROW_255  = getClut(0, 255),
    SHELTER_R48_SPRITE_CLUT_ROW_256  = getClut(0, 256),
    SHELTER_R48_SPRITE_CLUT_ROW_257  = getClut(0, 257),
    SHELTER_R48_SPRITE_CLUT_ROW_258  = getClut(0, 258),
    SHELTER_R48_SPRITE_NEUTRAL_COLOR = 128,
};

/// Empty view-1 sprite list: retains decoded background strips and an end record.
static SpriteBatch _gShelterR48View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 2, selected in source elements.
static SpriteSource _gShelterR48View2SpriteSources[70] = {
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 112, 2666, { .fields = { 88, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 64, 72, 3580, { .fields = { 88, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 88, 72, 3437, { .fields = { 88, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 88, 80, 3438, { .fields = { 88, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 112, 80, 3116, { .fields = { 80, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 104, 2677, { .fields = { 88, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 96, 2711, { .fields = { 80, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 48, 80, 3461, { .fields = { 88, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 24, 80, 2989, { .fields = { 104, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 24, 96, 2992, { .fields = { 8, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, 8, 96, 2882, { .fields = { 112, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -16, 96, 931, { .fields = { 8, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -16, 72, 2929, { .fields = { 8, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -48, 24, 3904, { .fields = { 40, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -16, 24, 3187, { .fields = { 0, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -16, 48, 2887, { .fields = { 8, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 24, 56, 2992, { .fields = { 32, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 24, 32, 3246, { .fields = { 32, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, 32, 3497, { .fields = { 24, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 64, 56, 3534, { .fields = { 88, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 112, 24, 3444, { .fields = { 48, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 136, 24, 3180, { .fields = { 48, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 112, 48, 3448, { .fields = { 112, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 136, 48, 2803, { .fields = { 112, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 112, 64, 3105, { .fields = { 88, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 120, 88, 3074, { .fields = { 96, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 136, 64, 2650, { .fields = { 88, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 136, 80, 2662, { .fields = { 88, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -24, 104, 974, { .fields = { 80, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -72, 104, 979, { .fields = { 72, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -88, 72, 1282, { .fields = { 104, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -40, 72, 1282, { .fields = { 120, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -48, 64, 1375, { .fields = { 64, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, 64, 1407, { .fields = { 40, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, 56, 1532, { .fields = { 40, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -48, 56, 1532, { .fields = { 48, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -56, 48, 1607, { .fields = { 40, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -104, 48, 1657, { .fields = { 40, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -96, 40, 1782, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -64, 40, 1732, { .fields = { 64, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -72, 32, 1882, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -104, 32, 1857, { .fields = { 72, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, 24, 2032, { .fields = { 48, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, 16, 2157, { .fields = { 56, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, 8, 2232, { .fields = { 56, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, 0, 2282, { .fields = { 88, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -96, -8, 2307, { .fields = { 112, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -96, -16, 2332, { .fields = { 72, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -104, -24, 2357, { .fields = { 72, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -80, 88, 1157, { .fields = { 112, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -32, 88, 1157, { .fields = { 112, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -136, 72, 1250, { .fields = { 104, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -160, 72, 1250, { .fields = { 0, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -160, 88, 1125, { .fields = { 112, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -120, 88, 1125, { .fields = { 112, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -120, 104, 975, { .fields = { 112, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -160, 104, 975, { .fields = { 32, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, -160, 56, 1450, { .fields = { 88, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, 114, 90, 7500, { .fields = { 72, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, 114, 58, 7500, { .fields = { 72, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, 58, 58, 7500, { .fields = { 72, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, 58, 90, 7500, { .fields = { 72, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, 2, 90, 7500, { .fields = { 72, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, 2, 58, 7500, { .fields = { 72, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, -54, 58, 7500, { .fields = { 24, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, -54, 90, 7500, { .fields = { 24, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 32 } }, -102, 90, 7500, { .fields = { 24, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 32 } }, -102, 58, 7500, { .fields = { 80, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 32 } }, -126, 58, 7500, { .fields = { 104, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 32 } }, -126, 90, 7500, { .fields = { 80, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-2 source ranges with a zero-count background record and an end marker.
///
/// Ranges cover all 70 sources; the boss scenes hide/show batches 1 and 3.
static SpriteBatch _gShelterR48View2SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 2, 0 } },
    { 28, 30, 0, 0, { 1, 0 } },
    { 58, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 3, selected in source elements.
static SpriteSource _gShelterR48View3SpriteSources[125] = {
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -80, -16, 3250, { .fields = { 80, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -80, -40, 3460, { .fields = { 72, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -40, -16, 3996, { .fields = { 80, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -8, -16, 4421, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -40, -40, 3847, { .fields = { 8, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -8, -40, 4146, { .fields = { 32, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 16, -24, 4686, { .fields = { 56, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 16, -48, 4383, { .fields = { 56, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 48, -48, 4145, { .fields = { 40, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 48, -24, 3667, { .fields = { 120, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 56, 0, 3757, { .fields = { 112, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 88, 0, 3500, { .fields = { 104, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 128, 0, 2750, { .fields = { 112, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 128, -24, 2942, { .fields = { 88, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 88, -24, 3423, { .fields = { 96, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 88, -48, 3289, { .fields = { 120, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -64, 72, 1875, { .fields = { 112, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -16, 72, 1875, { .fields = { 104, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 32, 72, 1875, { .fields = { 104, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -112, 56, 1750, { .fields = { 16, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -72, 56, 1750, { .fields = { 8, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -24, 56, 1875, { .fields = { 8, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 24, 56, 1875, { .fields = { 8, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 72, 56, 1975, { .fields = { 8, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -112, 24, 2125, { .fields = { 16, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -112, 40, 2125, { .fields = { 32, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -72, 40, 1875, { .fields = { 24, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -24, 40, 1875, { .fields = { 32, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -24, 24, 1875, { .fields = { 16, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 24, 24, 1875, { .fields = { 16, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 24, 40, 1875, { .fields = { 16, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 72, 40, 1975, { .fields = { 104, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 120, 48, 1950, { .fields = { 96, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 72, 32, 1947, { .fields = { 104, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 72, 24, 1900, { .fields = { 120, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 16, 1900, { .fields = { 104, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 32, 8, 1900, { .fields = { 104, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -8, 16, 1875, { .fields = { 96, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -48, 16, 1850, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -8, 8, 1875, { .fields = { 96, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -8, 0, 1800, { .fields = { 96, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, -8, 2000, { .fields = { 40, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, -8, 2375, { .fields = { 104, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, -16, 2557, { .fields = { 40, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -80, -24, 2586, { .fields = { 8, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -112, -16, 2401, { .fields = { 48, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, -8, 2588, { .fields = { 120, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, 0, 2250, { .fields = { 80, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 0, 2378, { .fields = { 104, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 16, 2125, { .fields = { 104, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 8, 2263, { .fields = { 112, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 8, 2250, { .fields = { 56, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 16, 2250, { .fields = { 8, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 48, 2000, { .fields = { 104, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 120, 40, 2000, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 104, 32, 2125, { .fields = { 104, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 88, 24, 2000, { .fields = { 104, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 72, 16, 1900, { .fields = { 104, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 64, 8, 1900, { .fields = { 104, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 96, 16, 2157, { .fields = { 96, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 112, 24, 2128, { .fields = { 96, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 32, 2179, { .fields = { 88, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 40, 2125, { .fields = { 104, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 24, 2200, { .fields = { 112, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 120, 16, 2250, { .fields = { 112, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, 8, 2250, { .fields = { 104, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 112, 0, 2250, { .fields = { 112, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 8, 2250, { .fields = { 112, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 16, 2250, { .fields = { 64, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -24, 0, 1750, { .fields = { 24, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, 0, 1800, { .fields = { 112, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 32, 96, 1750, { .fields = { 112, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -16, 96, 1750, { .fields = { 64, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -64, 96, 1750, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 48 } }, -112, 72, 1750, { .fields = { 80, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -32, 8, 1825, { .fields = { 112, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 8, 1900, { .fields = { 80, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 80, 96, 1500, { .fields = { 56, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 80, 72, 1550, { .fields = { 56, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 128, 72, 1500, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 56, 1400, { .fields = { 40, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -72, 24, 1950, { .fields = { 24, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -56, 24, 1875, { .fields = { 120, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 8 } }, 104, 64, 565, { .fields = { 104, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -152, -24, 716, { .fields = { 88, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -136, -16, 3253, { .fields = { 88, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -128, 8, 1858, { .fields = { 32, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -152, 8, 711, { .fields = { 56, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -152, 24, 545, { .fields = { 56, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -128, 32, 527, { .fields = { 32, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -136, 40, 662, { .fields = { 104, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -128, 48, 624, { .fields = { 72, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -104, 40, 536, { .fields = { 72, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -112, 56, 781, { .fields = { 24, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -96, 56, 567, { .fields = { 16, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -88, 80, 710, { .fields = { 16, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -80, 64, 552, { .fields = { 56, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -48, 72, 533, { .fields = { 48, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -72, 88, 667, { .fields = { 88, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -8, 64, 500, { .fields = { 64, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 24, 64, 451, { .fields = { 48, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 56, 64, 447, { .fields = { 8, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 80, 56, 463, { .fields = { 32, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 96, 40, 493, { .fields = { 24, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 112, 32, 525, { .fields = { 8, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, 136, 16, 567, { .fields = { 32, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, 48, 779, { .fields = { 0, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, 112, 48, 649, { .fields = { 16, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 96, 72, 742, { .fields = { 24, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 80, 88, 672, { .fields = { 24, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, 80, 80, 577, { .fields = { 88, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, 104, 650, { .fields = { 0, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 56, 88, 544, { .fields = { 120, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 40, 104, 522, { .fields = { 112, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 8, 96, 500, { .fields = { 120, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -8, 80, 500, { .fields = { 112, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 32 } }, 53, 20, 7500, { .fields = { 80, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, 53, -4, 7500, { .fields = { 80, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, 53, -28, 7500, { .fields = { 40, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, 13, -28, 7500, { .fields = { 40, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, -35, -28, 7500, { .fields = { 32, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, -35, -4, 7500, { .fields = { 32, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 32 } }, -35, 20, 7500, { .fields = { 80, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, 13, -4, 7500, { .fields = { 40, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 32 } }, 13, 20, 7500, { .fields = { 88, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-3 source ranges with a zero-count background record and an end marker.
///
/// Ranges cover all 125 sources; the boss scenes hide/show batches 1 and 4.
static SpriteBatch _gShelterR48View3SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 2, 0 } },
    { 16, 67, 0, 0, { 3, 0 } },
    { 83, 33, 0, 0, { 1, 0 } },
    { 116, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 4, selected in source elements.
static SpriteSource _gShelterR48View4SpriteSources[106] = {
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -112, 24, 2558, { .fields = { 104, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -112, 0, 2750, { .fields = { 120, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -64, 24, 3000, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -64, 0, 3068, { .fields = { 96, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -16, 0, 4138, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 32, -8, 3825, { .fields = { 16, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 32, 16, 3970, { .fields = { 16, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 80, 16, 3750, { .fields = { 24, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 80, -8, 3354, { .fields = { 16, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 112, -8, 3750, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, 8, 3625, { .fields = { 8, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -16, 24, 4016, { .fields = { 80, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 72, 40, 3750, { .fields = { 48, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, 8, 1277, { .fields = { 64, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 112, 16, 1325, { .fields = { 32, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 24, 1277, { .fields = { 16, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 40, 1277, { .fields = { 8, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 40, 32, 1500, { .fields = { 16, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 40, 1325, { .fields = { 16, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 32, 40, 1325, { .fields = { 40, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 0, 40, 1500, { .fields = { 32, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -16, 32, 1423, { .fields = { 32, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -16, 24, 1469, { .fields = { 40, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -32, 16, 1800, { .fields = { 120, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -40, 24, 1565, { .fields = { 40, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -72, 24, 2000, { .fields = { 24, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -48, 32, 1625, { .fields = { 24, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -128, 40, 2383, { .fields = { 24, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -96, 40, 1950, { .fields = { 32, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -64, 40, 1550, { .fields = { 24, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -32, 40, 1383, { .fields = { 32, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -144, 48, 2328, { .fields = { 48, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -112, 48, 1950, { .fields = { 40, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 56, 2256, { .fields = { 80, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -128, 56, 1981, { .fields = { 88, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 32, 56, 1350, { .fields = { 104, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -128, 96, 1500, { .fields = { 96, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -152, 88, 1750, { .fields = { 64, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 80, 1729, { .fields = { 16, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -104, 56, 1550, { .fields = { 0, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 48, 1517, { .fields = { 8, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -72, 56, 1500, { .fields = { 8, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -40, 56, 1500, { .fields = { 0, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -48, 48, 1408, { .fields = { 0, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 8, 56, 1500, { .fields = { 8, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -8, 56, 1500, { .fields = { 16, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -16, 48, 1339, { .fields = { 24, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 8, 48, 1375, { .fields = { 16, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, 32, 1800, { .fields = { 32, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -88, 32, 1950, { .fields = { 24, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, 96, 96, 1250, { .fields = { 64, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, 32, 96, 1250, { .fields = { 64, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -32, 96, 1250, { .fields = { 64, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -96, 96, 1500, { .fields = { 64, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, 96, 80, 1250, { .fields = { 8, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, 32, 80, 1250, { .fields = { 8, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, 80, 1250, { .fields = { 24, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, -80, 80, 1500, { .fields = { 8, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -128, 80, 1500, { .fields = { 24, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 64, 32 } }, 96, 48, 1202, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 48, 64, 1250, { .fields = { 72, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 0, 64, 1250, { .fields = { 72, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -40, 64, 1500, { .fields = { 96, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -72, 64, 1500, { .fields = { 80, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 64, 48, 1277, { .fields = { 80, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -112, 64, 1500, { .fields = { 72, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -160, 64, 1500, { .fields = { 64, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 40, 48, 1325, { .fields = { 16, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, 32, 1277, { .fields = { 112, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 96, 24, 1277, { .fields = { 112, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 64, 24, 1325, { .fields = { 16, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -8, 347, { .fields = { 112, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 104, 0, 377, { .fields = { 104, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 72, 8, 417, { .fields = { 112, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 40, 16, 465, { .fields = { 112, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 8, 24, 523, { .fields = { 80, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -24, 32, 598, { .fields = { 48, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -64, 40, 701, { .fields = { 56, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -80, 40, 784, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -72, 72, 840, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -64, 88, 883, { .fields = { 120, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, -24, 80, 672, { .fields = { 16, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, -56, 80, 759, { .fields = { 0, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 0, 72, 597, { .fields = { 48, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 32, 72, 526, { .fields = { 40, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, 72, 72, 470, { .fields = { 112, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 96, 64, 432, { .fields = { 48, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 128, 64, 393, { .fields = { 72, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -96, 48, 938, { .fields = { 112, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -96, 72, 983, { .fields = { 120, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -112, 48, 967, { .fields = { 104, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -112, 24, 919, { .fields = { 56, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -128, 16, 2426, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -144, 16, 1026, { .fields = { 80, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -160, 24, 1167, { .fields = { 80, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -136, 40, 2036, { .fields = { 72, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -160, 48, 1259, { .fields = { 72, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 56, 24 } }, 28, 64, 7500, { .fields = { 16, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, -20, 64, 7500, { .fields = { 16, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 56, 24 } }, -76, 64, 7500, { .fields = { 8, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 56, 24 } }, -76, 40, 7500, { .fields = { 8, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 56, 24 } }, -76, 16, 7500, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, -20, 40, 7500, { .fields = { 72, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, -20, 16, 7500, { .fields = { 64, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 56, 24 } }, 28, 16, 7500, { .fields = { 40, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 56, 24 } }, 28, 40, 7500, { .fields = { 48, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-4 source ranges with a zero-count background record and an end marker.
///
/// Ranges cover all 106 sources; the boss scenes hide/show batches 1 and 4.
static SpriteBatch _gShelterR48View4SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 2, 0 } },
    { 13, 58, 0, 0, { 3, 0 } },
    { 71, 26, 0, 0, { 1, 0 } },
    { 97, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 5, selected in source elements.
static SpriteSource _gShelterR48View5SpriteSources[151] = {
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -136, -24, 1841, { .fields = { 80, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -96, -24, 1962, { .fields = { 80, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -96, -16, 1913, { .fields = { 64, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -136, -16, 1718, { .fields = { 72, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -160, -16, 1790, { .fields = { 96, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -88, -8, 1829, { .fields = { 80, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, -8, 1678, { .fields = { 88, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, -8, 745, { .fields = { 88, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -88, 0, 1720, { .fields = { 80, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 0, 1579, { .fields = { 72, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 0, 1777, { .fields = { 80, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -72, 8, 1601, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 8, 1538, { .fields = { 56, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 8, 1824, { .fields = { 72, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -72, 16, 1509, { .fields = { 72, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 16, 1456, { .fields = { 64, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 16, 1820, { .fields = { 72, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 24, 1816, { .fields = { 72, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 24, 1384, { .fields = { 80, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -72, 24, 1465, { .fields = { 104, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 32, 1390, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 32, 1323, { .fields = { 96, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 40, 1331, { .fields = { 96, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 40, 1260, { .fields = { 96, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 40, 1328, { .fields = { 112, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 32, 1344, { .fields = { 112, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 48, 724, { .fields = { 104, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 48, 1186, { .fields = { 96, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 48, 1256, { .fields = { 88, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 56, 1166, { .fields = { 96, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 56, 1170, { .fields = { 80, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -24, 56, 1117, { .fields = { 88, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 56, 1062, { .fields = { 88, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 24, 56, 1100, { .fields = { 112, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 32, 48, 1152, { .fields = { 112, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 40, 40, 1174, { .fields = { 104, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 48, 32, 1212, { .fields = { 104, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 24, 1173, { .fields = { 72, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 16, 1297, { .fields = { 40, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 72, 8, 1346, { .fields = { 40, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 0, 1344, { .fields = { 40, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, -8, 1335, { .fields = { 40, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, -16, 1335, { .fields = { 40, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, -24, 1452, { .fields = { 40, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, -40, 1473, { .fields = { 64, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, -16, 1344, { .fields = { 56, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, -8, 1280, { .fields = { 40, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 120, 0, 1267, { .fields = { 48, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 0, 1112, { .fields = { 48, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, 8, 1253, { .fields = { 32, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 8, 1062, { .fields = { 40, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 16, 1195, { .fields = { 32, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 16, 1020, { .fields = { 40, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 24, 1180, { .fields = { 40, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 120, 24, 1064, { .fields = { 48, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 24, 946, { .fields = { 56, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 80, 32, 1137, { .fields = { 40, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 112, 32, 1021, { .fields = { 72, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 32, 914, { .fields = { 72, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 72, 40, 1122, { .fields = { 64, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, 40, 964, { .fields = { 64, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 40, 875, { .fields = { 72, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 48, 1066, { .fields = { 72, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 48, 839, { .fields = { 72, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 48, 849, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 56, 964, { .fields = { 64, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 56, 941, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 120, 56, 841, { .fields = { 56, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 56, 818, { .fields = { 64, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 64, 969, { .fields = { 48, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 64, 854, { .fields = { 48, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 64, 840, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 72, 72, 815, { .fields = { 48, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 112, 72, 833, { .fields = { 64, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 72, 845, { .fields = { 64, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 80, 803, { .fields = { 56, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 120, 80, 844, { .fields = { 24, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 80, 851, { .fields = { 112, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, 88, 789, { .fields = { 16, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 88, 840, { .fields = { 24, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 96, 1125, { .fields = { 16, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 104, 907, { .fields = { 32, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 112, 859, { .fields = { 32, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -160, 104, 613, { .fields = { 48, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -112, 104, 576, { .fields = { 48, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -64, 104, 580, { .fields = { 48, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, 104, 564, { .fields = { 48, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, 80, 104, 613, { .fields = { 40, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 32, 104, 566, { .fields = { 48, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 72, 88, 695, { .fields = { 16, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 72, 80, 768, { .fields = { 112, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 72, 96, 668, { .fields = { 16, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 24, 96, 635, { .fields = { 16, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -24, 96, 635, { .fields = { 16, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 96, 636, { .fields = { 8, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 96, 636, { .fields = { 16, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 96, 636, { .fields = { 24, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 88, 697, { .fields = { 16, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 80, 772, { .fields = { 120, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 72, 870, { .fields = { 120, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 64, 1159, { .fields = { 120, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 88, 697, { .fields = { 0, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 80, 772, { .fields = { 120, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 72, 892, { .fields = { 112, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 64, 993, { .fields = { 112, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -64, 64, 1026, { .fields = { 104, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -64, 72, 890, { .fields = { 112, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -64, 80, 790, { .fields = { 112, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -64, 88, 696, { .fields = { 112, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 88, 697, { .fields = { 0, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 80, 792, { .fields = { 0, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 72, 904, { .fields = { 0, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 64, 1022, { .fields = { 0, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 32, 64, 964, { .fields = { 16, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 72, 865, { .fields = { 8, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 80, 768, { .fields = { 8, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 88, 696, { .fields = { 8, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, -160, -8, 500, { .fields = { 48, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -160, 8, 702, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -160, 40, 523, { .fields = { 112, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 48 } }, -152, 72, 537, { .fields = { 120, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -160, 88, 505, { .fields = { 104, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -128, 8, 471, { .fields = { 104, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, -144, 48, 607, { .fields = { 32, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -128, 40, 531, { .fields = { 112, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -128, 72, 532, { .fields = { 96, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -120, 96, 504, { .fields = { 96, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -112, 24, 518, { .fields = { 40, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -80, 24, 484, { .fields = { 32, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -40, 48, 494, { .fields = { 56, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 8 } }, 0, 56, 464, { .fields = { 8, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 40, 48, 479, { .fields = { 72, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 64, 32, 511, { .fields = { 112, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 64, 0, 514, { .fields = { 112, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 48, -16, 459, { .fields = { 48, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 8, -16, 425, { .fields = { 48, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -24, -16, 464, { .fields = { 56, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -40, -8, 451, { .fields = { 88, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -40, 16, 464, { .fields = { 120, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -40, 64, 508, { .fields = { 88, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -40, 88, 490, { .fields = { 112, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 64, 64, 507, { .fields = { 96, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, 64, 96, 506, { .fields = { 96, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 80, 48, 512, { .fields = { 64, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 112, 32, 534, { .fields = { 96, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 128, 16, 534, { .fields = { 96, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 144, 0, 553, { .fields = { 96, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 8 } }, 88, -8, 513, { .fields = { 8, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 112, -24, 532, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 128, -40, 506, { .fields = { 72, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, 152, -48, 542, { .fields = { 104, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-5 source ranges covering 151 elements, with background and end records.
static SpriteBatch _gShelterR48View5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 117, 0, 0, { 1, 0 } },
    { 117, 34, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 6, selected in source elements.
static SpriteSource _gShelterR48View6SpriteSources[86] = {
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -64, 104, 985, { .fields = { 88, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -24, 104, 992, { .fields = { 80, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -40, 88, 1050, { .fields = { 80, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -88, 88, 1050, { .fields = { 80, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -72, 80, 1125, { .fields = { 80, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -32, 80, 1125, { .fields = { 88, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -40, 72, 1200, { .fields = { 80, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -80, 72, 1200, { .fields = { 88, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, 64, 1300, { .fields = { 88, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -48, 64, 1300, { .fields = { 72, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -48, 56, 1375, { .fields = { 80, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, 56, 1375, { .fields = { 80, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -80, 48, 1500, { .fields = { 80, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -40, 48, 1500, { .fields = { 88, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -40, 40, 1575, { .fields = { 16, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -64, 40, 1625, { .fields = { 48, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, 32, 1494, { .fields = { 104, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -104, 40, 1625, { .fields = { 88, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 32, 1596, { .fields = { 0, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 24, 1675, { .fields = { 112, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -112, -40, 2377, { .fields = { 32, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -88, -40, 2375, { .fields = { 72, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -88, -32, 2325, { .fields = { 64, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -88, -24, 2275, { .fields = { 64, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -80, -16, 2200, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -80, -8, 2125, { .fields = { 64, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 0, 2025, { .fields = { 48, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 8, 1925, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 16, 1812, { .fields = { 56, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -120, 104, 975, { .fields = { 80, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -160, 104, 975, { .fields = { 96, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -128, 88, 1050, { .fields = { 112, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -160, 88, 1050, { .fields = { 120, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 80, 1125, { .fields = { 72, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 80, 1125, { .fields = { 80, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 72, 1200, { .fields = { 72, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 72, 1200, { .fields = { 56, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 64, 1300, { .fields = { 56, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 64, 1300, { .fields = { 72, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -160, 48, 1437, { .fields = { 24, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -112, 48, 1500, { .fields = { 80, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -112, 56, 1375, { .fields = { 80, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 0, 96, 2876, { .fields = { 56, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 24, 80, 2975, { .fields = { 104, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -8, 80, 2884, { .fields = { 48, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -16, 64, 2907, { .fields = { 32, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -16, 48, 2858, { .fields = { 32, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -48, 32, 3148, { .fields = { 72, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -24, 40, 3239, { .fields = { 80, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -24, 24, 3290, { .fields = { 72, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -8, 32, 2985, { .fields = { 48, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 8, 40, 3081, { .fields = { 72, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 112, 24, 3347, { .fields = { 24, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 40, 3076, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 72, 32, 3460, { .fields = { 48, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 24, 32, 3421, { .fields = { 32, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 24, 48, 3271, { .fields = { 32, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 72, 48, 3500, { .fields = { 40, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 112, 40, 3322, { .fields = { 120, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 56, 2711, { .fields = { 32, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 72, 2742, { .fields = { 120, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 88, 2736, { .fields = { 120, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, 104, 2708, { .fields = { 0, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 88, 64, 3353, { .fields = { 0, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 88, 80, 3346, { .fields = { 0, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 56, 64, 3560, { .fields = { 0, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 24, 64, 2971, { .fields = { 0, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 48, 80, 3496, { .fields = { 104, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, 114, 75, 7500, { .fields = { 80, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, 114, 99, 7500, { .fields = { 80, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 114, 59, 7500, { .fields = { 0, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 66, 59, 7500, { .fields = { 8, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 18, 59, 7500, { .fields = { 8, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, -30, 59, 7500, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, -78, 59, 7500, { .fields = { 120, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 16 } }, -134, 59, 7500, { .fields = { 120, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, 66, 75, 7500, { .fields = { 80, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, 66, 99, 7500, { .fields = { 80, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, 18, 75, 7500, { .fields = { 80, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, 18, 99, 7500, { .fields = { 56, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, -30, 75, 7500, { .fields = { 32, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, -30, 99, 7500, { .fields = { 32, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, -78, 75, 7500, { .fields = { 72, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 24 } }, -78, 99, 7500, { .fields = { 80, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 24 } }, -134, 75, 7500, { .fields = { 72, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 56, 24 } }, -134, 99, 7500, { .fields = { 72, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-6 source ranges with a zero-count background record and an end marker.
///
/// Ranges cover all 86 sources; the boss scenes hide/show batches 2 and 3.
static SpriteBatch _gShelterR48View6SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 42, 0, 0, { 2, 0 } },
    { 42, 26, 0, 0, { 1, 0 } },
    { 68, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 7, selected in source elements.
static SpriteSource _gShelterR48View7SpriteSources[141] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, -16, 3268, { .fields = { 96, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, 8, 3000, { .fields = { 96, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 56, 8, 3000, { .fields = { 88, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 56, -16, 3592, { .fields = { 48, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 16, -16, 4651, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -8, -16, 4412, { .fields = { 104, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -24, -24, 3784, { .fields = { 64, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -24, 0, 3625, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -72, -16, 3692, { .fields = { 40, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -56, 0, 3575, { .fields = { 88, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, -24, 3714, { .fields = { 8, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 112, 0, 2239, { .fields = { 16, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 104, 8, 2160, { .fields = { 8, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 128, 8, 2055, { .fields = { 8, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 120, 16, 2125, { .fields = { 0, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 96, 16, 2000, { .fields = { 16, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 112, 24, 2000, { .fields = { 16, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 24, 2125, { .fields = { 64, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 32, 2000, { .fields = { 32, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 40, 2000, { .fields = { 40, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 48, 2000, { .fields = { 64, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 56, 1875, { .fields = { 32, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 16, 1925, { .fields = { 56, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 80, 24, 1925, { .fields = { 56, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 32, 1950, { .fields = { 120, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 40, 1875, { .fields = { 120, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, 48, 1875, { .fields = { 0, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 112, 56, 1750, { .fields = { 16, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 64, 1875, { .fields = { 16, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 8, 1900, { .fields = { 8, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -8, 0, 1750, { .fields = { 8, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -72, -24, 2437, { .fields = { 8, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -88, -16, 2459, { .fields = { 0, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, -8, 2295, { .fields = { 0, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -120, -8, 2375, { .fields = { 120, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 0, 2250, { .fields = { 0, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -112, 0, 2303, { .fields = { 120, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -144, 0, 2294, { .fields = { 120, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -104, 8, 2375, { .fields = { 40, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -128, 8, 2184, { .fields = { 40, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 8, 2250, { .fields = { 40, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -128, 16, 2375, { .fields = { 40, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -96, 16, 2125, { .fields = { 32, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 24, 2200, { .fields = { 16, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 32, 2250, { .fields = { 0, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 40, 2250, { .fields = { 24, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -128, 88, 1875, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -80, 88, 1875, { .fields = { 96, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, 88, 1875, { .fields = { 96, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 16, 88, 1875, { .fields = { 96, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 64, 88, 1875, { .fields = { 96, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 112, 88, 1864, { .fields = { 96, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 112, 72, 1875, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 64, 72, 1875, { .fields = { 96, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 16, 72, 1875, { .fields = { 72, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, 72, 1875, { .fields = { 72, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -80, 72, 1875, { .fields = { 80, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -120, 72, 1875, { .fields = { 88, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -128, 40, 1900, { .fields = { 24, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -112, 32, 2125, { .fields = { 40, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 40, 48, 1750, { .fields = { 32, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 72, 48, 1800, { .fields = { 40, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 88, 56, 1775, { .fields = { 40, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 56, 40, 1825, { .fields = { 24, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 32, 1850, { .fields = { 48, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 48, 24, 1875, { .fields = { 16, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 48, 16, 1900, { .fields = { 80, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -80, 40, 2000, { .fields = { 72, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -40, 24, 1750, { .fields = { 32, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 8, 24, 1750, { .fields = { 56, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 88, 64, 1750, { .fields = { 96, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 8, 1750, { .fields = { 16, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -24, 16, 1750, { .fields = { 32, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 0, 2000, { .fields = { 48, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -32, 0, 2000, { .fields = { 40, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -8, 48, 1875, { .fields = { 16, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -56, 48, 1875, { .fields = { 16, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, 32, 1750, { .fields = { 48, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 40, 56, 1750, { .fields = { 48, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -104, 48, 1750, { .fields = { 80, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -152, 48, 1875, { .fields = { 80, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -40, 8, 1975, { .fields = { 72, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -64, 8, 1987, { .fields = { 96, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -48, 16, 2100, { .fields = { 72, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -72, 16, 2125, { .fields = { 16, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -56, 24, 1825, { .fields = { 24, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -88, 24, 2125, { .fields = { 16, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -64, 32, 1812, { .fields = { 16, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -80, 32, 2025, { .fields = { 32, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 32, 16, 1875, { .fields = { 32, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 8, 16, 1750, { .fields = { 24, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -160, 16, 2250, { .fields = { 64, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 16, 32, 1750, { .fields = { 96, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 112, 104, 1780, { .fields = { 88, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 64, 104, 1750, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 16, 104, 1750, { .fields = { 48, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, 104, 1750, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -80, 104, 1750, { .fields = { 48, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -128, 104, 1750, { .fields = { 48, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -120, 40, 425, { .fields = { 120, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -120, 56, 425, { .fields = { 16, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -104, 72, 520, { .fields = { 112, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -96, 48, 425, { .fields = { 48, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -88, 72, 538, { .fields = { 40, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -80, 64, 425, { .fields = { 32, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -64, 72, 425, { .fields = { 112, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -80, 88, 497, { .fields = { 0, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -72, 104, 499, { .fields = { 0, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 16, 96, 425, { .fields = { 8, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 32, 104, 425, { .fields = { 0, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 56, 104, 425, { .fields = { 16, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 72, 96, 425, { .fields = { 16, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 16 } }, 88, 96, 500, { .fields = { 88, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, 0, 64, 425, { .fields = { 120, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 80, 64, 437, { .fields = { 112, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 96, 72, 501, { .fields = { 16, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 96, 48, 425, { .fields = { 32, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 112, 64, 500, { .fields = { 24, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 112, 40, 425, { .fields = { 16, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 128, 32, 425, { .fields = { 24, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 144, 16, 425, { .fields = { 40, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 8 } }, 40, 72, 441, { .fields = { 0, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, 48, 80, 440, { .fields = { 16, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 8, 64, 425, { .fields = { 8, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, -32, 80, 425, { .fields = { 8, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, -24, 88, 484, { .fields = { 16, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 0, 96, 425, { .fields = { 32, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 128, 56, 425, { .fields = { 48, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, -152, 32, 425, { .fields = { 8, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, -144, 40, 534, { .fields = { 24, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -136, 48, 425, { .fields = { 64, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -128, 48, 525, { .fields = { 120, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, 46, -19, 7500, { .fields = { 120, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, 6, -19, 7500, { .fields = { 72, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, -34, -19, 7500, { .fields = { 56, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, -34, 5, 7500, { .fields = { 64, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, 6, 5, 7500, { .fields = { 40, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, 46, 5, 7500, { .fields = { 32, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 48, 24 } }, 46, 29, 7500, { .fields = { 48, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, 6, 29, 7500, { .fields = { 48, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 40, 24 } }, -34, 29, 7500, { .fields = { 80, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-7 source ranges with a zero-count background record and an end marker.
///
/// Ranges cover all 141 sources; the boss scenes hide/show batches 1 and 4.
static SpriteBatch _gShelterR48View7SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 2, 0 } },
    { 11, 88, 0, 0, { 3, 0 } },
    { 99, 33, 0, 0, { 1, 0 } },
    { 132, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 8, selected in source elements.
static SpriteSource _gShelterR48View8SpriteSources[114] = {
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -56, 16, 2875, { .fields = { 112, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -56, 0, 2994, { .fields = { 80, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -8, 0, 4010, { .fields = { 72, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 0, 0, 4238, { .fields = { 48, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 40, 0, 3610, { .fields = { 72, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 80, 0, 3475, { .fields = { 96, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 40, 16, 3693, { .fields = { 32, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 48, 24, 3698, { .fields = { 32, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 80, 16, 3604, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, -16, 3770, { .fields = { 80, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 16, -16, 4087, { .fields = { 80, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 56, -16, 3441, { .fields = { 72, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -56, -16, 2992, { .fields = { 96, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -80, 0, 3250, { .fields = { 96, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -80, 16, 3250, { .fields = { 56, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 120, 0, 3243, { .fields = { 96, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, 112, 16, 3500, { .fields = { 80, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 56, 1268, { .fields = { 120, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 40, 48, 1268, { .fields = { 112, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 48, 40, 1293, { .fields = { 8, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 56, 32, 1293, { .fields = { 112, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 88, 24, 1293, { .fields = { 120, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, 16, 1293, { .fields = { 80, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 104, 32, 1293, { .fields = { 8, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 88, 40, 1293, { .fields = { 120, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 48, 1268, { .fields = { 120, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 72, 56, 1268, { .fields = { 120, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 104, 56, 1268, { .fields = { 0, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 48, 1268, { .fields = { 0, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, 56, 1268, { .fields = { 120, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -152, 48, 2319, { .fields = { 24, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -136, 40, 2384, { .fields = { 24, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -128, 48, 2216, { .fields = { 16, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -112, 40, 2250, { .fields = { 24, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -104, 32, 2073, { .fields = { 32, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -80, 24, 1817, { .fields = { 32, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -80, 32, 1605, { .fields = { 32, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -56, 24, 1541, { .fields = { 64, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -160, 56, 2242, { .fields = { 24, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -136, 56, 2027, { .fields = { 32, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 56, 1625, { .fields = { 16, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -104, 48, 1875, { .fields = { 16, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -88, 40, 1580, { .fields = { 8, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -56, 32, 1441, { .fields = { 24, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -16, 32, 1367, { .fields = { 40, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -40, 24, 1458, { .fields = { 0, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -40, 16, 1557, { .fields = { 48, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 32, 32, 1325, { .fields = { 88, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 8, 40, 1375, { .fields = { 120, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 0, 48, 1500, { .fields = { 120, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_832, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -8, 56, 1500, { .fields = { 120, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -40, 48, 1500, { .fields = { 24, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -40, 40, 1484, { .fields = { 40, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -64, 48, 1500, { .fields = { 88, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -64, 56, 1500, { .fields = { 0, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -32, 56, 1500, { .fields = { 40, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -96, 88, 1550, { .fields = { 72, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, 112, 88, 1218, { .fields = { 72, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, 64, 88, 1218, { .fields = { 80, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, 16, 88, 1250, { .fields = { 80, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, -32, 88, 1250, { .fields = { 80, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -48, 88, 1500, { .fields = { 112, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -64, 104, 1500, { .fields = { 96, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, 64, 1243, { .fields = { 0, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, 64, 1243, { .fields = { 8, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 16, 64, 1243, { .fields = { 120, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -32, 64, 1500, { .fields = { 32, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -80, 64, 1500, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -112, 64, 1500, { .fields = { 80, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -160, 64, 1625, { .fields = { 48, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, 32, 1293, { .fields = { 64, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 128, 24, 405, { .fields = { 64, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 80, 32, 447, { .fields = { 64, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 56, 40, 498, { .fields = { 88, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 24, 40, 537, { .fields = { 64, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 0, 48, 593, { .fields = { 72, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -32, 48, 652, { .fields = { 40, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 8 } }, -80, 56, 675, { .fields = { 16, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -80, 64, 675, { .fields = { 112, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -64, 88, 675, { .fields = { 120, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -56, 88, 675, { .fields = { 112, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -16, 88, 677, { .fields = { 120, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 24, 88, 594, { .fields = { 120, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 64, 88, 523, { .fields = { 104, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 112, 88, 463, { .fields = { 112, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -96, 56, 675, { .fields = { 56, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -104, 80, 675, { .fields = { 40, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -112, 56, 675, { .fields = { 48, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -136, 48, 675, { .fields = { 56, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -160, 48, 675, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 16 } }, -112, 40, 675, { .fields = { 80, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 16 } }, -120, 32, 675, { .fields = { 24, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 16 } }, -128, 24, 675, { .fields = { 24, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, -152, 24, 675, { .fields = { 24, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, -160, 32, 675, { .fields = { 24, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 32, 16 } }, -104, 48, 926, { .fields = { 48, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 16, 40 } }, -120, 48, 879, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 32, 24 } }, -104, 64, 842, { .fields = { 40, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 83, 2, 7500, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 35, 2, 7500, { .fields = { 120, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 40, 24 } }, -5, 2, 7500, { .fields = { 32, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, -53, 2, 7500, { .fields = { 32, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 83, 26, 7500, { .fields = { 32, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 35, 26, 7500, { .fields = { 32, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 40, 24 } }, -5, 26, 7500, { .fields = { 72, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, -53, 26, 7500, { .fields = { 32, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 16 } }, -53, 50, 7500, { .fields = { 32, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 40, 16 } }, -5, 50, 7500, { .fields = { 120, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 16 } }, 35, 50, 7500, { .fields = { 112, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 16 } }, 83, 50, 7500, { .fields = { 24, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 16 } }, 83, 66, 7500, { .fields = { 24, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 16 } }, 35, 66, 7500, { .fields = { 16, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 40, 16 } }, -5, 66, 7500, { .fields = { 32, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 16 } }, -53, 66, 7500, { .fields = { 56, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-8 source ranges with a zero-count background record and an end marker.
///
/// Ranges cover all 114 sources; the boss scenes hide/show batches 1 and 5.
static SpriteBatch _gShelterR48View8SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 3, 0 } },
    { 17, 54, 0, 0, { 4, 0 } },
    { 71, 24, 0, 0, { 2, 0 } },
    { 95, 3, 0, 0, { 1, 0 } },
    { 98, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 9, selected in source elements.
static SpriteSource _gShelterR48View9SpriteSources[131] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, -32, 1463, { .fields = { 72, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, -24, 1465, { .fields = { 112, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 112, -16, 1391, { .fields = { 120, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 144, -16, 1283, { .fields = { 88, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, -8, 1234, { .fields = { 8, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 96, -8, 1353, { .fields = { 112, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 0, 1175, { .fields = { 0, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 88, 0, 1303, { .fields = { 112, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 8, 1111, { .fields = { 16, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 80, 8, 1270, { .fields = { 16, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 64, 16, 1243, { .fields = { 48, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 104, 16, 1135, { .fields = { 24, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 16, 1012, { .fields = { 0, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 24, 914, { .fields = { 120, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 24, 1096, { .fields = { 120, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 56, 24, 1196, { .fields = { 56, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 48, 32, 1087, { .fields = { 56, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 32, 1062, { .fields = { 64, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 32, 877, { .fields = { 56, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 112, 40, 859, { .fields = { 32, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 72, 40, 1046, { .fields = { 40, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 40, 40, 1131, { .fields = { 48, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 24, 48, 1111, { .fields = { 56, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 24, 56, 925, { .fields = { 56, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 48, 1050, { .fields = { 56, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 48, 1000, { .fields = { 96, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 48, 832, { .fields = { 56, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 56, 56, 912, { .fields = { 64, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 88, 56, 881, { .fields = { 56, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 56, 905, { .fields = { 48, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 112, 64, 916, { .fields = { 96, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 72, 64, 958, { .fields = { 112, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 64, 1042, { .fields = { 112, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -160, 104, 614, { .fields = { 24, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -112, 104, 597, { .fields = { 24, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -64, 104, 571, { .fields = { 16, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, 104, 571, { .fields = { 8, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 32, 104, 579, { .fields = { 8, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 80, 104, 622, { .fields = { 24, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 104, 792, { .fields = { 16, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 56, 72, 836, { .fields = { 104, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 80, 80, 743, { .fields = { 96, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 96, 88, 740, { .fields = { 96, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 96, 784, { .fields = { 96, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 88, 772, { .fields = { 120, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 80, 852, { .fields = { 104, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 96, 72, 819, { .fields = { 112, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 128, 72, 848, { .fields = { 112, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 96, 96, 653, { .fields = { 0, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 48, 88, 680, { .fields = { 16, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 0, 88, 697, { .fields = { 32, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -48, 88, 681, { .fields = { 40, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -96, 88, 682, { .fields = { 24, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -136, 88, 682, { .fields = { 16, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -160, 88, 682, { .fields = { 40, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 80, 791, { .fields = { 96, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 80, 791, { .fields = { 96, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -64, 80, 811, { .fields = { 0, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -24, 80, 820, { .fields = { 8, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 48, 80, 775, { .fields = { 8, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 16, 80, 820, { .fields = { 8, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 16, 72, 903, { .fields = { 24, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -32, 72, 938, { .fields = { 8, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -80, 72, 934, { .fields = { 120, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 72, 941, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 72, 908, { .fields = { 96, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -160, 64, 1041, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 64, 1118, { .fields = { 88, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -64, 64, 1025, { .fields = { 96, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 64, 1041, { .fields = { 88, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 56, 1198, { .fields = { 88, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 56, 1126, { .fields = { 96, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 56, 1144, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 48, 1150, { .fields = { 56, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 40, 1150, { .fields = { 56, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 32, 1178, { .fields = { 56, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 24, 1786, { .fields = { 88, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 16, 1807, { .fields = { 88, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 8, 1773, { .fields = { 80, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 0, 1762, { .fields = { 80, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, -8, 1750, { .fields = { 56, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -136, -16, 1780, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -96, -16, 1943, { .fields = { 72, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -120, -8, 1751, { .fields = { 80, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -88, -8, 1845, { .fields = { 72, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 0, 1720, { .fields = { 80, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 0, 1661, { .fields = { 72, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 8, 1557, { .fields = { 48, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -80, 8, 1605, { .fields = { 48, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 16, 1462, { .fields = { 40, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -80, 16, 1544, { .fields = { 40, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -120, 24, 1386, { .fields = { 72, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -80, 24, 1449, { .fields = { 72, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 32, 1328, { .fields = { 80, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -72, 32, 1353, { .fields = { 72, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -120, 40, 1249, { .fields = { 88, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -88, 40, 1312, { .fields = { 88, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -56, 40, 1300, { .fields = { 80, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -120, 48, 1174, { .fields = { 40, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -88, 48, 1214, { .fields = { 56, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -56, 48, 1200, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 40 } }, -160, 80, 500, { .fields = { 120, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 48 } }, -160, 32, 500, { .fields = { 120, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 40 } }, -160, -8, 500, { .fields = { 120, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -152, -8, 500, { .fields = { 64, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -136, 8, 500, { .fields = { 104, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, -152, 48, 604, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -136, 40, 500, { .fields = { 104, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -128, 80, 500, { .fields = { 112, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -120, 24, 500, { .fields = { 56, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -88, 24, 535, { .fields = { 56, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -48, -8, 518, { .fields = { 112, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -48, 32, 509, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, -48, 64, 505, { .fields = { 96, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -40, 96, 516, { .fields = { 96, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, -32, 48, 484, { .fields = { 56, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 8, 48, 462, { .fields = { 56, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 48, 48, 501, { .fields = { 56, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 88, 48, 500, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 112, 32, 500, { .fields = { 88, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 128, 16, 500, { .fields = { 96, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 144, 0, 500, { .fields = { 96, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -48, 575, { .fields = { 72, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 120, -32, 550, { .fields = { 64, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 88, -16, 550, { .fields = { 56, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 48, -16, 521, { .fields = { 64, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 16 } }, 8, -16, 484, { .fields = { 64, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 24 } }, -32, -16, 506, { .fields = { 72, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 48 } }, 64, 0, 514, { .fields = { 112, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, 56, 64, 506, { .fields = { 80, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 56, 88, 515, { .fields = { 104, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View-9 source ranges covering 131 elements, with background and end records.
static SpriteBatch _gShelterR48View9SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 101, 0, 0, { 1, 0 } },
    { 101, 30, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 10, selected in source elements.
static SpriteSource _gShelterR48View10SpriteSources[7] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 48 } }, -56, 72, 1473, { .fields = { 88, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, -16, 72, 1477, { .fields = { 96, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 16, 72, 1495, { .fields = { 96, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 16, 24, 1611, { .fields = { 96, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, -16, 24, 1551, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -56, 56, 1564, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -48, 24, 1578, { .fields = { 64, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 10 ranges 0..6, retaining decoded background strips.
///
/// Ranges count source elements; the final record ends the list. Visibility
/// and cached-packet controls are mutable while the room is loaded.
static SpriteBatch _gShelterR48View10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterR48View10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-11 sprite list: retains decoded background strips and an end record.
static SpriteBatch _gShelterR48View11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 12, selected in source elements.
static SpriteSource _gShelterR48View12SpriteSources[44] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 112, 104, 1500, { .fields = { 48, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 72, 104, 1436, { .fields = { 32, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 32, 104, 1372, { .fields = { 48, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, 104, 1372, { .fields = { 40, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -64, 104, 879, { .fields = { 32, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 72, 96, 1420, { .fields = { 32, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 48, 88, 1452, { .fields = { 24, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 24, 96, 1388, { .fields = { 24, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 0, 88, 1436, { .fields = { 24, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 24, 80, 1428, { .fields = { 40, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -16, 80, 1403, { .fields = { 24, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -24, 96, 1388, { .fields = { 16, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -48, 88, 1337, { .fields = { 16, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -64, 96, 1105, { .fields = { 24, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -104, 96, 874, { .fields = { 72, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -144, 96, 836, { .fields = { 72, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -160, 72, 824, { .fields = { 112, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -160, 24, 866, { .fields = { 112, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -144, 32, 1212, { .fields = { 96, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -144, 64, 845, { .fields = { 96, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -128, 40, 1250, { .fields = { 96, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -128, 64, 843, { .fields = { 112, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -112, 48, 1263, { .fields = { 56, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -96, 56, 1287, { .fields = { 112, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -80, 64, 1322, { .fields = { 72, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -64, 72, 1312, { .fields = { 88, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -80, 88, 961, { .fields = { 32, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -112, 88, 855, { .fields = { 32, 8 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -112, 80, 876, { .fields = { 16, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -112, 72, 923, { .fields = { 32, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -112, 64, 1177, { .fields = { 48, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, -8, 112, 882, { .fields = { 48, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 88 } }, -16, 24, 500, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -40, 64, 500, { .fields = { 88, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -64, 48, 863, { .fields = { 64, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 24 } }, -96, 32, 849, { .fields = { 56, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -120, 16, 835, { .fields = { 64, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 32 } }, -160, -8, 822, { .fields = { 88, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -160, -72, 829, { .fields = { 88, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -136, -56, 839, { .fields = { 80, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -120, -48, 848, { .fields = { 88, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 24 } }, -96, -32, 861, { .fields = { 64, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -64, -16, 500, { .fields = { 72, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 24 } }, -40, 0, 500, { .fields = { 64, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 12 ranges 0..30, 31..43, retaining decoded background strips.
///
/// Ranges count source elements; the final record ends the list. Visibility
/// and cached-packet controls are mutable while the room is loaded.
static SpriteBatch _gShelterR48View12SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 1, 0 } },
    { 31, ARRAY_SIZE(_gShelterR48View12SpriteSources) - 31, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 13, selected in source elements.
static SpriteSource _gShelterR48View13SpriteSources[12] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -88, 88, 783, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -80, 48, 742, { .fields = { 96, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, -48, 40, 726, { .fields = { 96, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -48, 88, 698, { .fields = { 64, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -16, 88, 679, { .fields = { 64, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, -16, 40, 742, { .fields = { 96, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 16, 40, 766, { .fields = { 96, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, 16, 88, 704, { .fields = { 64, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 48, 40, 827, { .fields = { 96, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, 48, 88, 779, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, 80, 64, 902, { .fields = { 72, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 80, 96, 1057, { .fields = { 104, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 13 ranges 0..11, retaining decoded background strips.
///
/// Ranges count source elements; the final record ends the list. Visibility
/// and cached-packet controls are mutable while the room is loaded.
static SpriteBatch _gShelterR48View13SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelterR48View13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-14 sprite list: retains decoded background strips and an end record.
static SpriteBatch _gShelterR48View14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-15 sprite list: retains decoded background strips and an end record.
static SpriteBatch _gShelterR48View15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-16 sprite list: retains decoded background strips and an end record.
static SpriteBatch _gShelterR48View16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 17, selected in source elements.
static SpriteSource _gShelterR48View17SpriteSources[89] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, -160, 64, 902, { .fields = { 120, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -152, 88, 904, { .fields = { 112, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -136, 32, 1304, { .fields = { 64, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -136, 48, 859, { .fields = { 112, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -136, 72, 938, { .fields = { 112, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -136, 96, 912, { .fields = { 104, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -144, 32, 1301, { .fields = { 64, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -144, 48, 836, { .fields = { 112, 152 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -144, 72, 927, { .fields = { 120, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -144, 96, 907, { .fields = { 104, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -152, 24, 1297, { .fields = { 64, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -152, 40, 924, { .fields = { 104, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -152, 64, 909, { .fields = { 104, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -160, 16, 1356, { .fields = { 72, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -160, 32, 901, { .fields = { 120, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -128, 40, 1312, { .fields = { 72, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -120, 40, 1351, { .fields = { 72, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -112, 48, 1355, { .fields = { 88, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -104, 48, 1386, { .fields = { 72, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -96, 56, 1335, { .fields = { 80, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -88, 56, 1389, { .fields = { 80, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -80, 64, 1347, { .fields = { 80, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -72, 72, 1351, { .fields = { 64, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -64, 72, 1359, { .fields = { 64, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -56, 80, 1298, { .fields = { 96, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -48, 80, 1352, { .fields = { 104, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -40, 88, 1292, { .fields = { 80, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -32, 88, 1274, { .fields = { 80, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -24, 88, 1414, { .fields = { 80, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -16, 80, 1515, { .fields = { 40, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 8, 80, 1546, { .fields = { 32, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 40, 80, 1540, { .fields = { 72, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -16, 88, 1471, { .fields = { 32, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 16, 88, 1482, { .fields = { 32, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 48, 88, 1420, { .fields = { 16, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 72, 96, 1317, { .fields = { 24, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 96, 1424, { .fields = { 24, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -16, 96, 1379, { .fields = { 16, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -8, 104, 1284, { .fields = { 24, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 32, 104, 1285, { .fields = { 16, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 80, 104, 1195, { .fields = { 16, 56 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 88, 112, 1147, { .fields = { 16, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 40, 112, 1131, { .fields = { 16, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -8, 112, 1124, { .fields = { 16, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -40, 104, 1143, { .fields = { 32, 40 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -40, 112, 1020, { .fields = { 32, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -48, 96, 984, { .fields = { 96, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -56, 96, 1010, { .fields = { 112, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -64, 88, 1012, { .fields = { 64, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -64, 104, 988, { .fields = { 64, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -72, 88, 966, { .fields = { 72, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -72, 104, 947, { .fields = { 64, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -80, 80, 993, { .fields = { 64, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -80, 96, 943, { .fields = { 112, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -88, 72, 985, { .fields = { 112, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -88, 96, 938, { .fields = { 120, 184 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -96, 72, 940, { .fields = { 120, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -96, 96, 934, { .fields = { 120, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -104, 64, 1016, { .fields = { 56, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -104, 80, 948, { .fields = { 64, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -104, 96, 944, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -112, 64, 946, { .fields = { 64, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -112, 80, 939, { .fields = { 72, 200 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -112, 96, 925, { .fields = { 88, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -120, 56, 998, { .fields = { 64, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -120, 72, 935, { .fields = { 88, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -120, 96, 931, { .fields = { 104, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -128, 56, 882, { .fields = { 64, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -128, 72, 931, { .fields = { 88, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -128, 96, 916, { .fields = { 96, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, -8, 112, 625, { .fields = { 72, 136 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -16, 72, 625, { .fields = { 112, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 40 } }, -16, 32, 625, { .fields = { 112, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -16, 16, 625, { .fields = { 72, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -40, 0, 625, { .fields = { 88, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -64, -16, 625, { .fields = { 88, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -80, -24, 625, { .fields = { 88, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -96, -32, 625, { .fields = { 72, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -120, -48, 625, { .fields = { 80, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -136, -56, 625, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -160, -72, 625, { .fields = { 72, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -160, -8, 625, { .fields = { 72, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -136, 8, 625, { .fields = { 72, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 24 } }, -120, 16, 625, { .fields = { 88, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -96, 32, 625, { .fields = { 56, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -80, 40, 625, { .fields = { 56, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -64, 48, 625, { .fields = { 72, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -48, 56, 625, { .fields = { 96, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -32, 64, 625, { .fields = { 96, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 17 ranges 0..69, 70..88, retaining decoded background strips.
///
/// Ranges count source elements; the final record ends the list. Visibility
/// and cached-packet controls are mutable while the room is loaded.
static SpriteBatch _gShelterR48View17SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 70, 0, 0, { 1, 0 } },
    { 70, ARRAY_SIZE(_gShelterR48View17SpriteSources) - 70, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted texture rectangles for mapped view 18, selected in source elements.
static SpriteSource _gShelterR48View18SpriteSources[99] = {
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 120, 112, 1603, { .fields = { 64, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 128, 112, 1596, { .fields = { 72, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 136, 112, 1752, { .fields = { 48, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 144, 112, 1693, { .fields = { 104, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 8, 8 } }, 152, 112, 1632, { .fields = { 56, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 104, 32, 1533, { .fields = { 48, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, -40, 3526, { .fields = { 8, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, -16, 3268, { .fields = { 16, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 96, 8, 1802, { .fields = { 32, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 56, 8, 1348, { .fields = { 8, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 56, -16, 3592, { .fields = { 40, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 56, -40, 3964, { .fields = { 64, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 16, -40, 4455, { .fields = { 48, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 16, -16, 4651, { .fields = { 40, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -8, -16, 4412, { .fields = { 88, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -8, -40, 4093, { .fields = { 80, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -64, -24, 3910, { .fields = { 0, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -24, -40, 3924, { .fields = { 48, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -24, -24, 3784, { .fields = { 96, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -24, 0, 1113, { .fields = { 80, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -72, -16, 3692, { .fields = { 64, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -56, 0, 1085, { .fields = { 48, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -64, -40, 3899, { .fields = { 72, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 32, 8, 500, { .fields = { 0, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 16, 104, 500, { .fields = { 64, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, 104, 500, { .fields = { 48, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -80, 104, 500, { .fields = { 80, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -128, 104, 795, { .fields = { 64, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -128, 88, 837, { .fields = { 0, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 48, 24, 1085, { .fields = { 8, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 48, 16, 1224, { .fields = { 80, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -88, 24, 500, { .fields = { 0, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 16, 500, { .fields = { 0, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -24, 16, 875, { .fields = { 80, 208 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -32, 32, 750, { .fields = { 16, 240 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -80, 32, 750, { .fields = { 16, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 16, 32, 750, { .fields = { 24, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -8, 48, 625, { .fields = { 96, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -56, 48, 625, { .fields = { 96, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -104, 48, 625, { .fields = { 96, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, -32, 72, 375, { .fields = { 80, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, -80, 72, 500, { .fields = { 80, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, 16, 72, 500, { .fields = { 80, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 8, 16, 712, { .fields = { 88, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -40, 24, 500, { .fields = { 64, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -128, 32, 908, { .fields = { 48, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 40, 56, 816, { .fields = { 48, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 40, 48, 865, { .fields = { 96, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 56, 40, 861, { .fields = { 40, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 56, 32, 990, { .fields = { 104, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 64, 16, 1471, { .fields = { 8, 80 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -136, 72, 743, { .fields = { 32, 176 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, 0, 625, { .fields = { 40, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -48, 0, 625, { .fields = { 56, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 24, 48 } }, 64, 72, 692, { .fields = { 104, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 80, 24, 1431, { .fields = { 32, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, -120, 40, 525, { .fields = { 40, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -120, 56, 502, { .fields = { 112, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -104, 72, 520, { .fields = { 48, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -96, 48, 496, { .fields = { 112, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -88, 72, 538, { .fields = { 0, 216 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, -80, 64, 501, { .fields = { 0, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, -64, 72, 507, { .fields = { 24, 128 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -80, 88, 497, { .fields = { 32, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -72, 104, 499, { .fields = { 32, 32 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 16, 96, 503, { .fields = { 32, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 32, 104, 521, { .fields = { 64, 232 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 56, 104, 518, { .fields = { 64, 64 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 72, 96, 534, { .fields = { 0, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 16 } }, 88, 96, 500, { .fields = { 64, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, 0, 64, 487, { .fields = { 112, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 32 } }, 80, 64, 500, { .fields = { 112, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 96, 72, 501, { .fields = { 112, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 96, 48, 476, { .fields = { 16, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 112, 64, 500, { .fields = { 16, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 112, 40, 498, { .fields = { 16, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 128, 32, 500, { .fields = { 112, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 144, 16, 519, { .fields = { 16, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 40, 8 } }, 40, 72, 441, { .fields = { 64, 248 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, 48, 80, 440, { .fields = { 8, 104 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 8, 64, 454, { .fields = { 32, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, -32, 80, 508, { .fields = { 16, 88 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, -24, 88, 484, { .fields = { 88, 112 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 16, 24 } }, 0, 96, 488, { .fields = { 0, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 128, 56, 500, { .fields = { 64, 16 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, -152, 32, 525, { .fields = { 8, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, -144, 40, 534, { .fields = { 88, 160 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -136, 48, 525, { .fields = { 8, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_256, { .fields = { 8, 32 } }, -128, 48, 525, { .fields = { 120, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_257, { .fields = { 64, 48 } }, 16, -120, 3912, { .fields = { 64, 0 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 9, -35, 7500, { .fields = { 88, 168 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 57, -35, 7500, { .fields = { 0, 96 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, -39, -35, 7500, { .fields = { 16, 48 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, -39, -11, 7500, { .fields = { 16, 24 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 9, -11, 7500, { .fields = { 32, 144 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_896, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 57, -11, 7500, { .fields = { 88, 120 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 57, 13, 7500, { .fields = { 32, 72 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, 9, 13, 7500, { .fields = { 48, 224 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_R48_SPRITE_TPAGE_960, SHELTER_R48_SPRITE_CLUT_ROW_258, { .fields = { 48, 24 } }, -39, 13, 7500, { .fields = { 32, 192 } }, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, SHELTER_R48_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 18 ranges 0..22, 23..55, 56..88, 89..89, 90..98, retaining decoded background strips.
///
/// Ranges count source elements; the final record ends the list. Visibility
/// and cached-packet controls are mutable while the room is loaded.
static SpriteBatch _gShelterR48View18SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 33, 0, 0, { 4, 0 } },
    { 56, 33, 0, 0, { 2, 0 } },
    { 89, 1, 0, 0, { 1, 0 } },
    { 90, ARRAY_SIZE(_gShelterR48View18SpriteSources) - 90, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterR48SpriteViews[SHELTER_R48_VIEW_COUNT] = {
    { { .empty = _gShelterR48View1SpriteBatches }, _gShelterR48View1SpriteBatches, NULL },
    { { .elements = _gShelterR48View2SpriteSources }, _gShelterR48View2SpriteBatches, NULL },
    { { .elements = _gShelterR48View3SpriteSources }, _gShelterR48View3SpriteBatches, NULL },
    { { .elements = _gShelterR48View4SpriteSources }, _gShelterR48View4SpriteBatches, NULL },
    { { .elements = _gShelterR48View5SpriteSources }, _gShelterR48View5SpriteBatches, NULL },
    { { .elements = _gShelterR48View6SpriteSources }, _gShelterR48View6SpriteBatches, NULL },
    { { .elements = _gShelterR48View7SpriteSources }, _gShelterR48View7SpriteBatches, NULL },
    { { .elements = _gShelterR48View8SpriteSources }, _gShelterR48View8SpriteBatches, NULL },
    { { .elements = _gShelterR48View9SpriteSources }, _gShelterR48View9SpriteBatches, NULL },
    { { .elements = _gShelterR48View10SpriteSources }, _gShelterR48View10SpriteBatches, NULL },
    { { .empty = _gShelterR48View11SpriteBatches }, _gShelterR48View11SpriteBatches, NULL },
    { { .elements = _gShelterR48View12SpriteSources }, _gShelterR48View12SpriteBatches, NULL },
    { { .elements = _gShelterR48View13SpriteSources }, _gShelterR48View13SpriteBatches, NULL },
    { { .empty = _gShelterR48View14SpriteBatches }, _gShelterR48View14SpriteBatches, NULL },
    { { .empty = _gShelterR48View15SpriteBatches }, _gShelterR48View15SpriteBatches, NULL },
    { { .empty = _gShelterR48View16SpriteBatches }, _gShelterR48View16SpriteBatches, NULL },
    { { .elements = _gShelterR48View17SpriteSources }, _gShelterR48View17SpriteBatches, NULL },
    { { .elements = _gShelterR48View18SpriteSources }, _gShelterR48View18SpriteBatches, NULL },
};

/// Authored room-space point lights admitted in every view.
///
/// Positions and falloff radii use game units; intensities use `ONE` for 1.0.
/// Composition and per-query attenuation remain writable while the room is loaded.
static WorldCoordPointLight _gShelterR48PointLights[57] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8004, -981, 0x28FC } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5803, -2722, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2B5A, -682, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x28AF, -1083, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9789, -1452, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2D76, -371, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -1535, 8627 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -452, 0x28FA } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -804, 9888 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -1179, 9235 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -3109, 4111 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -2735, 9235 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -2735, 4765 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -2383, 5366 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36AE, -3466, 3503 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -364, 3360 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -1445, 5223 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -1102, 4622 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -3115, 9888 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -722, 3968 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CCD, -3457, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9620, -2381, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2806, -2744, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2AB1, -3144, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5693, -1140, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5010, -740, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4384, -377, 999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6233, -1453, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -3458, 0x28FA } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x366F, -1935, 7157 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1677, -3804, 0x2FA1 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x327A, -3804, 825 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2007, -2377, 8627 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x375A, -3804, 2093 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2834, 165, 1041 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2033, 165, 1791 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x33A7, 165, 0x3296 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x367F, 165, 0x2FA6 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8016, -981, 3492 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1912 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -2530, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -3523, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2792, -3804, 0x33F8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, 3266 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -1589, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -4548, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3D86, -5485, 7117 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2940, 2940, 2344 }, { 0, 0 } }, 1000, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8129, -1935, 0x328E } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7905, -1935, 1059 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2017, -1935, 6456 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2085, 2085, 1663 }, { 0, 0 } }, 500, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6347, -2377, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4498, -3458, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5124, -3111, 0x32C8 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4095, ONE, 3266 }, { 0, 0 } }, 100, 900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7995, 0x3A4D, 6998 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1005, 879, 752 }, { 0, 0 } }, 10, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -291, -3647, 6898 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 461, 527, 478 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5327, -981, 8386 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2941, -981, 9449 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2C54, -981, 0x2C54 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x297D, -981, 2932 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3543, 2364, 1913 }, { 0, 0 } }, 6000, 0x2710 },
};

/// The room's downward cone light at (8000,-18600,7000), admitted in every view.
///
/// The Q12 axis points along +Y. Falloff spans 10..20000 game units; the full
/// opening is 568 angle units (4096 per turn). Composition and attenuation are mutable.
static WorldCoordSpotLight _gShelterR48ConeLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8000, -0x48A8, 7000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3618, 2633, 2264 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 10, 0x4E20, 568 },
};

/// Room-1's mutable collection of point and cone lights, with no directional lights.
static WorldCoordRoomLights _gShelterR48RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterR48PointLights),
    .pointLights           = _gShelterR48PointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelterR48ConeLights),
    .coneLights            = _gShelterR48ConeLights,
};

/// Sixteen mutable directed boundaries joining views 2..9 in both directions.
///
/// Origins, corner offsets and radii use room game units; normals are Q12.
/// The final included record carries LAST. Room activation borrows and links
/// these records to the view-boundary list until clearing them at transition.
static WorldCollisionTrigger _gShelterR48ViewBoundaryTriggers[16] = {
    { NULL, NULL, NULL, { 2001, -4784, 5392, 0 }, { { -2160, -6160, 304, 0 }, { 2160, -6160, -304, 0 }, { -2160, 6160, 304, 0 }, { 2160, 6160, -304, 0 } }, { -572, 0, -4058, 0 }, { 0, 0, ONE, 0 }, 6516, 0, 9, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2096, -5152, 5248, 0 }, { { 2208, -6160, -336, 0 }, { -2208, -6160, 336, 0 }, { 2208, 6160, -336, 0 }, { -2208, 6160, 336, 0 } }, { 616, 0, 4050, 0 }, { 0, 0, ONE, 0 }, 6536, 0, 2, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2113, -5216, 9568, 0 }, { { 2161, -6160, -553, 0 }, { -2167, -6160, 547, 0 }, { 2161, 6160, -553, 0 }, { -2167, 6160, 547, 0 } }, { 1008, 0, 3969, 0 }, { 0, 0, ONE, 0 }, 6536, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1984, -5184, 9728, 0 }, { { -2121, -6160, 513, 0 }, { 2119, -6160, -515, 0 }, { -2121, 6160, 513, 0 }, { 2119, 6160, -515, 0 } }, { -966, 0, -3983, 0 }, { 0, 0, ONE, 0 }, 6516, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6017, -4960, 0x32F1, 0 }, { { 592, -6160, 1792, 0 }, { -592, -6160, -1792, 0 }, { 592, 6160, 1792, 0 }, { -592, 6160, -1792, 0 } }, { -3904, 0, 1289, 0 }, { 0, 0, ONE, 0 }, 6435, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5904, -4897, 0x32E0, 0 }, { { -608, -6160, -1792, 0 }, { 608, -6160, 1792, 0 }, { -608, 6160, -1792, 0 }, { 608, 6160, 1792, 0 } }, { 3887, 0, -1320, 0 }, { 0, 0, ONE, 0 }, 6435, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2AE0, -4928, 0x3280, 0 }, { { -846, -6160, -1603, 0 }, { 822, -6160, 1585, 0 }, { -846, 6160, -1603, 0 }, { 822, 6160, 1585, 0 } }, { 3639, 0, -1906, 0 }, { 0, 0, ONE, 0 }, 6415, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2BE0, -4896, 0x32E0, 0 }, { { 815, -6160, 1596, 0 }, { -823, -6160, -1603, 0 }, { 815, 6160, 1596, 0 }, { -823, 6160, -1603, 0 } }, { -3653, 0, 1870, 0 }, { 0, 0, ONE, 0 }, 6415, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x37A0, -4576, 8608, 0 }, { { -2160, -6160, 304, 0 }, { 2160, -6160, -304, 0 }, { -2160, 6160, 304, 0 }, { 2160, 6160, -304, 0 } }, { -572, 0, -4058, 0 }, { 0, 0, ONE, 0 }, 6516, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3780, -4576, 8448, 0 }, { { 2208, -6160, -336, 0 }, { -2208, -6160, 336, 0 }, { 2208, 6160, -336, 0 }, { -2208, 6160, 336, 0 } }, { 616, 0, 4050, 0 }, { 0, 0, ONE, 0 }, 6536, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3800, -4800, 3872, 0 }, { { 2208, -6160, -336, 0 }, { -2208, -6160, 336, 0 }, { 2208, 6160, -336, 0 }, { -2208, 6160, 336, 0 } }, { 616, 0, 4050, 0 }, { 0, 0, ONE, 0 }, 6536, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x37A0, -4801, 4032, 0 }, { { -2160, -6160, 304, 0 }, { 2160, -6160, -304, 0 }, { -2160, 6160, 304, 0 }, { 2160, 6160, -304, 0 } }, { -572, 0, -4058, 0 }, { 0, 0, ONE, 0 }, 6516, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x27C0, -4801, 1088, 0 }, { { 570, -6160, 1741, 0 }, { -578, -6160, -1748, 0 }, { 570, 6160, 1741, 0 }, { -578, 6160, -1748, 0 } }, { -3898, 0, 1281, 0 }, { 0, 0, ONE, 0 }, 6415, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9952, -4800, 1024, 0 }, { { -646, -6160, -1719, 0 }, { 639, -6160, 1712, 0 }, { -646, 6160, -1719, 0 }, { 639, 6160, 1712, 0 } }, { 3848, 0, -1442, 0 }, { 0, 0, ONE, 0 }, 6415, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4736, -4800, 1088, 0 }, { { -571, -6160, -1718, 0 }, { 548, -6160, 1695, 0 }, { -571, 6160, -1718, 0 }, { 548, 6160, 1695, 0 } }, { 3896, 0, -1278, 0 }, { 0, 0, ONE, 0 }, 6415, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4943, -4864, 1056, 0 }, { { 545, -6160, 1696, 0 }, { -572, -6160, -1719, 0 }, { 545, 6160, 1696, 0 }, { -572, 6160, -1719, 0 } }, { -3900, 0, 1275, 0 }, { 0, 0, ONE, 0 }, 6415, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource-entry IDs and their corresponding actor-library file numbers.
enum {
    SHELTER_R48_RESOURCE_ACTOR_503500 = 35,
    SHELTER_R48_RESOURCE_TENTACLE     = 45,
    SHELTER_R48_RESOURCE_ACTOR_403600 = 36,
};

/// Variant 1's actor_503500 boss and tentacle resources, followed by the end record.
///
/// Entry 45 requests uncataloged base file 604500; its tentacle spawn recipe
/// still comes from actor_503500, independently of that empty file request.
static AreaResource _gShelterR48Variant1Resources[3] = {
    { SHELTER_R48_RESOURCE_ACTOR_503500, SHELTER_R48_RESOURCE_ACTOR_503500, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, gActor503500EnemyTaskDescs },
    { SHELTER_R48_RESOURCE_TENTACLE, SHELTER_R48_RESOURCE_TENTACLE, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &gActor503500TentacleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's separate boss and tentacle resource list, identical to variant 1.
///
/// Retains the same uncataloged tentacle file request and live actor_503500 recipe.
static AreaResource _gShelterR48Variant2Resources[3] = {
    { SHELTER_R48_RESOURCE_ACTOR_503500, SHELTER_R48_RESOURCE_ACTOR_503500, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, gActor503500EnemyTaskDescs },
    { SHELTER_R48_RESOURCE_TENTACLE, SHELTER_R48_RESOURCE_TENTACLE, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &gActor503500TentacleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's actor_403600 boss resource, followed by the end record.
static AreaResource _gShelterR48Variant3Resources[2] = {
    { SHELTER_R48_RESOURCE_ACTOR_403600, SHELTER_R48_RESOURCE_ACTOR_403600, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, gActor403600TaskTable },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 1 places boss and tentacle at (8000,1000,7000) in world game units.
///
/// Both use spawn mode/variant zero, base files and CLUT row offset +2; the
/// boss also has texture-page offset -1. Includes the placement end record.
static AreaPlacement _gShelterR48Variant1Placements[3] = {
    { SHELTER_R48_RESOURCE_ACTOR_503500, 0, 0, 8000, 1000, 7000, 0, 0, -1, 2, 0 },
    { SHELTER_R48_RESOURCE_TENTACLE, 0, 0, 8000, 1000, 7000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 2's separate boss and tentacle placements, identical to variant 1.
static AreaPlacement _gShelterR48Variant2Placements[3] = {
    { SHELTER_R48_RESOURCE_ACTOR_503500, 0, 0, 8000, 1000, 7000, 0, 0, -1, 2, 0 },
    { SHELTER_R48_RESOURCE_TENTACLE, 0, 0, 8000, 1000, 7000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 3 places actor_403600 at (8000,500,6400) in world game units.
///
/// Yaw is -1024 (4096 per turn); mode, actor variant and file low index are
/// zero, with texture-page offset zero and CLUT row offset +2. Includes the end record.
static AreaPlacement _gShelterR48Variant3Placements[2] = {
    { SHELTER_R48_RESOURCE_ACTOR_403600, 0, 0, 8000, 500, 6400, -1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterR48AreaVariants[SHELTER_R48_AREA_VARIANT_COUNT] = {
    { NULL, NULL },
    { _gShelterR48Variant1Placements, _gShelterR48Variant1Resources },
    { _gShelterR48Variant2Placements, _gShelterR48Variant2Resources },
    { _gShelterR48Variant3Placements, _gShelterR48Variant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Payloads authored into the room's interaction and departure triggers.
enum {
    SHELTER_R48_EXIT_WARP_ENDPOINTS       = (1 << 4) | 2,
    SHELTER_R48_TRIGGER_CAP_COMMAND       = 1,
    SHELTER_R48_TRIGGER_PROGRESS_RESPONSE = 1,
};

/// Five mutable facing quads for departure, CAP, progress response and room events.
///
/// Positions and radii use room game units; normals are Q12. The warp requests
/// Pod Access Tunnel with departure slot 1 and arrival slot 2. The last two
/// quads use the deferred room-event sentinel; the final included quad carries LAST.
/// Activation links the records to the action list until room transition clears it.
static WorldCollisionTrigger _gShelterR48ActionTriggers[5] = {
    { NULL, NULL, NULL, { -960, -2080, 6848, 0 }, { { -576, 0, -1024, 0 }, { 576, 0, -1024, 0 }, { -576, 0, 1024, 0 }, { 576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, SHELTER_R48_EXIT_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1536, -2080, 6912, 0 }, { { -576, 0, -1024, 0 }, { 576, 0, -1024, 0 }, { -576, 0, 1024, 0 }, { 576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_R48_TRIGGER_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7760, -2080, 336, 0 }, { { -1136, 0, -592, 0 }, { 1136, 0, -592, 0 }, { -1136, 0, 592, 0 }, { 1136, 0, 592, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_R48_TRIGGER_PROGRESS_RESPONSE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7824, -2080, -32, 0 }, { { -1952, 0, -464, 0 }, { 1536, 0, -464, 0 }, { -1952, 0, 1488, 0 }, { 1536, 0, 1488, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, ONE, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7632, -2080, 416, 0 }, { { -1392, 0, -976, 0 }, { 1392, 0, -976, 0 }, { -1392, 0, 976, 0 }, { 1392, 0, 976, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 1698, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Type-1 requests in the loaded sound bank; cue 1 selects the following script.
enum {
    SHELTER_R48_CLASS1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_R48_CLASS1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
};

/// Class-1 footstep cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gShelterR48Class1FootstepSounds = {
    .walk         = SHELTER_R48_CLASS1_WALK_SOUND,
    .run          = SHELTER_R48_CLASS1_RUN_SOUND,
    .scriptedJump = SHELTER_R48_CLASS1_WALK_SOUND,
};

/// Classes 0 and 2..7 block probes, allow weapon impacts and apply pushback, without cues.
static WorldCollisionSurfaceProperties _gShelterR48SolidSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1 passes probes and ignores weapon impacts, retaining pushback and footstep cues.
static WorldCollisionSurfaceProperties _gShelterR48ProbePassSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR48Class1FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterR48SurfacePropertiesByClass[8] = {
    &_gShelterR48SolidSurface,
    &_gShelterR48ProbePassSurface,
    &_gShelterR48SolidSurface,
    &_gShelterR48SolidSurface,
    &_gShelterR48SolidSurface,
    &_gShelterR48SolidSurface,
    &_gShelterR48SolidSurface,
    &_gShelterR48SolidSurface,
};

/// Zeroed initial pose of the water-refraction model's single self-parented root.
///
/// Creation copies the all-zero matrix and parent index 0 into owned runtime
/// coordinates. This source pose is read only and remains local to the room.
static TmdBone _gShelterR48WaterRefractionSkeleton[1] = { 0 };

/// Random texture-animation phase bytes for six bands of sixteen segments.
///
/// Seeded once per room effect initialization from the low byte of LCG bits
/// 16..31, in band/segment order. Drawing adds signed frame age, takes signed
/// remainder modulo six, then narrows the texture-cell index to a halfword.
static u8 _gShelterR48RingTexturePhases[SHELTER_R48_RING_BAND_COUNT][EFFECT_BAND_SEGMENT_COUNT] = { 0 };

/// Prepares the screen-row rays and depth dividend for a horizontal water plane.
///
/// Borrows a writable scratch block and the current display state. `planeY` is
/// the plane's world Y in game coordinate units; `display->screenDistance` is
/// in pixels and narrows to a signed halfword in `screenRow.vz`.
/// View translation first narrows to signed halfwords, then rotates through the
/// transposed Q12 view rotation with GTE saturation. The resulting Y plus
/// `planeY`, multiplied by screen distance, is the ray-intersection dividend.
/// The caller must set `screenRow.vy` to a centred pixel row before rotating it
/// and dividing by its positive world-space Y component. No scratch allocation
/// occurs here; GTE rotation and arithmetic state are overwritten.
static inline void _shelterR48PrepareWaterProjection(WaterRefractionScratch* scratch, const DisplayState* display, s32 planeY)
{
    // Express the view translation and screen rays in the same world-space basis.
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->transposedView);
    scratch->viewTranslation.vx = gGfxViewCoord.workm.t[0];
    scratch->viewTranslation.vy = gGfxViewCoord.workm.t[1];
    scratch->viewTranslation.vz = gGfxViewCoord.workm.t[2];
    _gfxRotateSv(&scratch->transposedView, &scratch->viewTranslation);
    scratch->depth        = scratch->viewTranslation.vy + planeY;
    scratch->depth       *= display->screenDistance;
    scratch->screenRow.vx = 0;
    scratch->screenRow.vz = display->screenDistance;
    gte_SetRotMatrix(&scratch->transposedView);
}

/// Redraws the visible water surface as displaced scanlines of the current framebuffer.
///
/// Requires the room's TMD-body task and its source's two 0x4000-byte packet
/// halves. GAME_FLAG_100 must be nonzero, and only views 2, 3, 4, 6, 7, 8 and
/// 18 draw. At most 48 rows split at screen X=0 consume 96 POLY_FT4 packets
/// (3840 bytes per half). The initial active update allocates the buffer;
/// allocation must succeed. Task teardown releases the TMD and its buffer.
/// `killCountdown` holds a wrapping 16-bit wave phase, advanced by 32 only
/// while actor control is running. Screen positions are pixels centred on
/// (160,120); phases use 4096 units per turn. Requires scratch-stack space
/// and the current framebuffer and ordering table; overwrites GTE state.
static void _shelterR48WaterRefractionTask(Task* task)
{
    enum {
        SHELTER_R48_REFRACTION_PLANE_Y           = 0xD02,
        SHELTER_R48_REFRACTION_PHASE_STEP        = 0x20,
        SHELTER_R48_REFRACTION_HALF_WIDTH        = 160,
        SHELTER_R48_REFRACTION_HALF_HEIGHT       = 120,
        SHELTER_R48_REFRACTION_FADE_ROWS         = 8,
        SHELTER_R48_REFRACTION_TRIG_SHIFT        = 12,
        SHELTER_R48_REFRACTION_QUAD_WORDS        = 9,
        SHELTER_R48_REFRACTION_RAW_QUAD_CODE     = 0x2D,
        SHELTER_R48_REFRACTION_RIGHT_PAGE_U_BIAS = 32,
        SHELTER_R48_REFRACTION_MAX_DEPTH         = 0x3FFF,
        SHELTER_R48_REFRACTION_SORT_DEPTH_MASK   = 0x3FFF,
        SHELTER_R48_REFRACTION_STATE_NEW         = 0
    };
    DisplayState*           display;
    TmdObject*              model;
    WaterRefractionScratch* scratch;
    POLY_FT4*               quad;
    u8*                     bufferBytes;
    s32                     displayBuffer;
    s32                     viewId;
    s32                     clipMode;
    s32                     wrapShift;
    s32                     primaryPhase;
    s32                     secondaryPhase;
    s32                     row;
    s32                     centredRow;
    s32                     leftX;
    s32                     rightX;
    s32                     spanCount;
    s32                     clipRow;
    s32                     sortBias;
    s32                     waveWeight;
    s32                     waveScale;
    s32                     screenLeft;
    s32                     waveOffset;
    s32                     sampleOffset;
    s32                     firstRow;
    s32                     endRow;
    s32                     rowsFromStart;
    s32                     rowDepth;
    s32                     sortIndex;
    s32                     spanIndex;
    s32                     wrapFadeRow;
    s32                     fadeRows;
    s32                     viewLeftX;
    s32                     viewRightX;
    s32                     clipLeftX;
    s32                     clipRightX;
    s32                     rightPageLeftX;
    s32                     leftPageRightX;
    s32                     sampleRow;
    s32                     wrappedEdge;
    s32                     sine;
    s32                     cosine;

    model         = task->extra.tmd;
    displayBuffer = gDisplayState.otBuffer;
    viewId        = gGameSession->location.view;
    if (gameFlagGetNibble(GAME_FLAG_100) == 0) {
        return;
    }
    wrapShift = 0;
    switch (viewId) {
        case 2:
            firstRow   = 0xC3;
            endRow     = 0xF0;
            viewLeftX  = 1;
            viewRightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
            break;
        case 3:
            firstRow   = 0x68;
            endRow     = 0x88;
            viewLeftX  = 0;
            viewRightX = 0x44;
            break;
        case 4:
            firstRow   = 0x84;
            endRow     = 0xAA;
            viewLeftX  = -0xB;
            viewRightX = 0x3E;
            break;
        case 6:
            firstRow   = 0xC2;
            endRow     = 0xF0;
            viewLeftX  = 4;
            viewRightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
            break;
        case 7:
        case 18:
            firstRow   = 0x6A;
            endRow     = 0x8A;
            viewLeftX  = -5;
            viewRightX = 0x46;
            break;
        case 8:
            firstRow   = 0x7B;
            endRow     = 0xAB;
            viewLeftX  = -0x11;
            viewRightX = 0x78;
            break;
        default:
            return;
    }
    clipMode    = 0;
    clipRow     = 0;
    waveScale   = ONE;
    waveWeight  = ONE;
    sortBias    = 0;
    wrapFadeRow = 0;
    fadeRows    = SHELTER_R48_REFRACTION_FADE_ROWS;
    clipLeftX   = -SHELTER_R48_REFRACTION_HALF_WIDTH;
    clipRightX  = SHELTER_R48_REFRACTION_HALF_WIDTH;
    if (task->state == SHELTER_R48_REFRACTION_STATE_NEW) {
        model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(model);
        task->state++;
    }
    display     = &gDisplayState;
    bufferBytes = model->buffer;
    if (display->otBuffer != 0) {
        bufferBytes += SHELTER_R48_REFRACTION_BUFFER_HALF_BYTES;
    }
    // Preincrement packet traversal is retained to preserve the instruction schedule.
    quad = (POLY_FT4*)bufferBytes - 1;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        task->killCountdown = (u16)task->killCountdown + SHELTER_R48_REFRACTION_PHASE_STEP;
    }
    primaryPhase   = task->killCountdown * 2;
    secondaryPhase = task->killCountdown;
    SCRATCH_STACK_RESERVE_BLOCK(WaterRefractionScratch);
    scratch = SCRATCH_STACK_CURSOR(WaterRefractionScratch);
    // Back-project each scanline onto the horizontal water plane for sorting.
    _shelterR48PrepareWaterProjection(scratch, display, SHELTER_R48_REFRACTION_PLANE_Y);
    for (row = firstRow; row < endRow; row++) {
        centredRow            = row - SHELTER_R48_REFRACTION_HALF_HEIGHT;
        scratch->screenRow.vy = centredRow;
        gte_ldv0(&scratch->screenRow);
        gte_rtv0();
        leftX     = viewLeftX;
        rightX    = viewRightX;
        spanCount = 1;
        if (clipRow > 0) {
            if (row < clipRow + 8) {
                leftX = clipLeftX;
                if (wrapShift > 0) {
                    rightX = leftX + wrapShift;
                } else {
                    rightX = clipRightX;
                    leftX  = rightX + wrapShift;
                }
                if (clipRow < row) {
                    spanCount = 2;
                }
            }
        } else if ((clipRow < 0) && (-clipRow < row)) {
            leftX = clipLeftX;
            if (wrapShift > 0) {
                rightX = leftX + wrapShift;
            } else {
                rightX = clipRightX;
                leftX  = rightX + wrapShift;
            }
        }
        sine           = rsin(primaryPhase);
        cosine         = rcos(secondaryPhase + 0x134);
        sine          += 0x2000;
        sampleOffset   = cosine + sine;
        sampleOffset >>= 9;
        if (waveWeight == 0) {
            sampleOffset = (sampleOffset * waveScale) >> SHELTER_R48_REFRACTION_TRIG_SHIFT;
        }
        waveOffset   = sampleOffset;
        sampleOffset = waveOffset + 1;
        if (firstRow != 1) {
            rowsFromStart = row - firstRow;
            if (rowsFromStart < fadeRows) {
                do {
                    do {
                        do {
                            do {
                                sampleOffset  = waveOffset >> ((fadeRows - rowsFromStart) >> 1);
                                sampleOffset += 1;
                            } while (0);
                        } while (0);
                    } while (0);
                } while (0);
            }
        }
        gte_stsv(&scratch->rotatedRow);
        if (scratch->rotatedRow.vy > 0) {
            sortIndex   = scratch->depth / scratch->rotatedRow.vy;
            sortIndex >>= 2;
        } else {
            sortIndex = SHELTER_R48_REFRACTION_MAX_DEPTH;
        }
        sampleRow  = centredRow + SHELTER_R48_REFRACTION_HALF_HEIGHT + sampleOffset;
        rowDepth   = sortIndex;
        sortIndex  = ((rowDepth << gDisplayState.otDepthShift) & SHELTER_R48_REFRACTION_SORT_DEPTH_MASK) >> 4;
        sortIndex += sortBias;
        if (sampleRow >= 0xEF) {
            sampleRow = 0x1DC - sampleRow;
        }
        if (clipMode == 1) {
            screenLeft = -SHELTER_R48_REFRACTION_HALF_WIDTH;
            if (row < 0x7D) {
                leftX  = screenLeft;
                rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
            } else {
                leftX = screenLeft;
                if (row < 0xB3) {
                    spanCount = 2;
                }
                rightX = -0x59;
            }
        } else if (clipMode == 2) {
            if (row < 0x83) {
                leftX  = screenLeft;
                rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
            } else {
                if (row < 0xB7) {
                    spanCount = 2;
                    leftX     = 0x57;
                } else {
                    leftX = 0x57;
                }
                rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
            }
        } else if (clipMode == 3) {
            spanCount = 1;
            if (row < 0x43) {
                leftX  = screenLeft;
                rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
            } else {
                spanCount = 2;
            }
        }
        spanIndex = 0;
        if (spanCount != 0) {
            do {
                if (clipMode == 1) {
                    if (spanIndex != 0) {
                        leftX  = 0x3C;
                        rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
                    }
                } else if (clipMode == 2) {
                    if (spanIndex == 1) {
                        leftX  = screenLeft;
                        rightX = -0x69;
                    }
                } else if (clipMode == 3) {
                    if (spanIndex == 0) {
                        if (row < 0x43) {
                            leftX  = screenLeft;
                            rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
                        } else {
                            leftX  = screenLeft;
                            rightX = -0x57;
                        }
                    } else {
                        if (row < 0xC1) {
                            leftX  = 0x5D;
                            rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
                        } else {
                            leftX  = 0x2A;
                            rightX = SHELTER_R48_REFRACTION_HALF_WIDTH;
                        }
                    }
                } else if (spanIndex == 1) {
                    if (row - wrapFadeRow < fadeRows) {
                        sampleOffset = waveOffset >> ((fadeRows - (row - wrapFadeRow)) >> 1);
                        sampleRow    = centredRow + 0x79 + sampleOffset;
                        if (sampleRow >= 0xEF) {
                            sampleRow = 0x1DC - sampleRow;
                        }
                    }
                    wrappedEdge = wrapShift - 0x140;
                    if (wrapShift <= 0) {
                        wrappedEdge = wrapShift + 0x140;
                    }
                    leftX = clipLeftX;
                    if (wrappedEdge > 0) {
                        rightX = wrappedEdge + leftX;
                    } else {
                        rightX = clipRightX;
                        leftX  = wrappedEdge + rightX;
                    }
                }
                // Split at X=0 because the framebuffer spans two texture pages.
                if (rightX > 0) {
                    quad++;
                    quad->y1       = centredRow;
                    quad->y0       = centredRow;
                    quad->y3       = centredRow + 1;
                    quad->y2       = centredRow + 1;
                    quad->tpage    = getTPage(2, 0, 0x80, displayBuffer << 8);
                    rightPageLeftX = leftX;
                    if (leftX < 0) {
                        rightPageLeftX = 0;
                    }
                    quad->x2 = rightPageLeftX;
                    quad->x0 = rightPageLeftX;
                    quad->u2 = rightPageLeftX + SHELTER_R48_REFRACTION_RIGHT_PAGE_U_BIAS;
                    quad->u0 = rightPageLeftX + SHELTER_R48_REFRACTION_RIGHT_PAGE_U_BIAS;
                    quad->x3 = rightX;
                    quad->x1 = rightX;
                    quad->u3 = rightX + SHELTER_R48_REFRACTION_RIGHT_PAGE_U_BIAS;
                    quad->u1 = rightX + SHELTER_R48_REFRACTION_RIGHT_PAGE_U_BIAS;
                    quad->v1 = sampleRow + (displayBuffer << 4);
                    quad->v0 = sampleRow + (displayBuffer << 4);
                    quad->v3 = sampleRow + (displayBuffer << 4) + 1;
                    quad->v2 = sampleRow + (displayBuffer << 4) + 1;
                    setlen(quad, SHELTER_R48_REFRACTION_QUAD_WORDS);
                    setcode(quad, SHELTER_R48_REFRACTION_RAW_QUAD_CODE);
                    addPrim(&gGpuCurrentOt[sortIndex], quad);
                }
                if (leftX <= 0) {
                    quad++;
                    quad->y1       = centredRow;
                    quad->y0       = centredRow;
                    quad->y2       = (quad->y3 = centredRow + 1);
                    quad->tpage    = getTPage(2, 0, 0, displayBuffer << 8);
                    leftPageRightX = rightX;
                    if (rightX > 0) {
                        leftPageRightX = 0;
                    }
                    quad->u2 = leftX - 0x60;
                    quad->u0 = leftX - 0x60;
                    quad->u3 = (leftX - 0x60) + (leftPageRightX - leftX);
                    quad->u1 = (leftX - 0x60) + (leftPageRightX - leftX);
                    quad->x2 = leftX;
                    quad->x0 = leftX;
                    quad->x3 = leftPageRightX;
                    quad->x1 = leftPageRightX;
                    quad->v1 = sampleRow + (displayBuffer << 4);
                    quad->v0 = sampleRow + (displayBuffer << 4);
                    quad->v3 = sampleRow + (displayBuffer << 4) + 1;
                    quad->v2 = sampleRow + (displayBuffer << 4) + 1;
                    setlen(quad, SHELTER_R48_REFRACTION_QUAD_WORDS);
                    setcode(quad, SHELTER_R48_REFRACTION_RAW_QUAD_CODE);
                    addPrim(&gGpuCurrentOt[sortIndex], quad);
                }
                spanIndex += 1;
            } while (spanIndex < spanCount);
        }
        primaryPhase += 0x1F;
        if (rowDepth >= 0x301) {
            secondaryPhase += 0xC5 + (rowDepth - 0x300) / 4;
        } else {
            secondaryPhase += 0xC5;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterRefractionScratch);
}

/// Tests whether the live action-trigger list contains a latched room event.
///
/// Returns 1 for a room action with event ID 255 and a nonzero hit, otherwise
/// 0, including an empty list. Borrows live, NULL-terminated records without
/// changing the links, consuming the hit or retaining a pointer.
static inline s32 _shelterR48HasRoomEventTriggerHit(void)
{
    const WorldCollisionTrigger* trigger;
    s32                          triggerHit;

    trigger    = gWorldCollisionActionTriggerListHead;
    triggerHit = 1;
    while (trigger != NULL) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            goto triggerChecked;
        }
        trigger = trigger->next;
    }
    triggerHit = 0;
triggerChecked:
    return triggerHit;
}

/// Starts the room's card-use scene with Bowman's Card or Yoshida's Card.
///
/// Handles the integer item ID in message 0x13F1; the receiver, message ID
/// and second payload word are unused. Requires progress nibble 0x100 to be
/// 1 and a hit room-action trigger with the room-event parameter to accept
/// the card. On acceptance,
/// holds scene handling and player updates, spawns the actor overlay's scene
/// launcher, sets progress to 2 and room scene state to 4, and returns 1.
/// Otherwise returns 0. Requires the trigger list live through dispatch and
/// the room and actor overlays loaded for the resulting scene. Retains no
/// payload storage.
static s32 _shelterR48UseStaffCard(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum {
        SHELTER_R48_ITEM_BOWMANS_CARD      = 0x121,
        SHELTER_R48_ITEM_YOSHIDAS_CARD     = 0x122,
        SHELTER_R48_SCENE_CARD_USE_STARTED = 4,
        SHELTER_R48_EVENT_HOLD             = 1,
        SHELTER_R48_PLAYER_UPDATE_HOLD     = 1,
        SHELTER_R48_ITEM_USE_REJECTED      = 0,
        SHELTER_R48_ITEM_USE_ACCEPTED      = 1
    };
    s32 accepted;

    accepted = SHELTER_R48_ITEM_USE_REJECTED;
    if (itemId == SHELTER_R48_ITEM_BOWMANS_CARD || itemId == SHELTER_R48_ITEM_YOSHIDAS_CARD) {
        if (gameFlagGetNibble(GAME_FLAG_100) == SHELTER_R48_CARD_USE_READY) {
            // Card use is accepted only at an active room-event trigger.
            if (_shelterR48HasRoomEventTriggerHit() != 0) {
                // Hold gameplay before queuing the card-use scene.
                gGameSession->eventState  = SHELTER_R48_EVENT_HOLD;
                gPlayerActorStateTickHeld = SHELTER_R48_PLAYER_UPDATE_HOLD;
                taskSpawnFromTableOnDefaultList(&gActor503500StaffCardSceneTaskDesc, 0, 0, 0);
                gameFlagSetNibble(GAME_FLAG_100, SHELTER_R48_CARD_USE_STARTED);
                gameFlagSetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE, SHELTER_R48_SCENE_CARD_USE_STARTED);
                accepted = SHELTER_R48_ITEM_USE_ACCEPTED;
            }
        }
    }
    return accepted;
}

/// Room-controller callbacks indexed by state: 0 setup, 1 idle, 2 release.
///
/// Dispatch copies all three pointers; state must be 0..2. There is no
/// terminator or bounds check; keep the room's callback code loaded.
static const TaskFuncTable3 _gShelterR48RoomTaskStates = {
    { _shelterR48RoomSetupState, _shelterR48RoomIdleState, taskKill }
};

/// Allows a room transition after resolving its Mine/Shelter destination room.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with a readable eight-byte request and
/// writable reply, borrowed for this call; they may alias. Copies the complete
/// record, then resolves its room from story progress only on execution.
/// Queries preserve the requested destination. Receiver and message ID are
/// unused, no payload is retained, and the result is always 1 (allowed).
static s32 _shelterR48ResolveTransitionMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_R48_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_R48_TRANSITION_ALLOWED;
}

/// Ignores the room-command message and returns zero.
///
/// Installed for ROOM_MESSAGE_COMMAND on the room task. Neither payload word
/// is inspected, and the receiver and message ID are unused.
static s32 _shelterR48IgnoreRoomCommand(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// Plays the progress-selected CAP response to the room's action 1.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request.
/// Progress 0 selects command 6, progress 1 command 7; other progress values
/// and action IDs do nothing. Ignores control, argument, receiver, message ID
/// and the zero second payload word; retains no storage and returns zero.
static s32 _shelterR48RoomActionMsg(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        SHELTER_R48_ACTION_PROGRESS_RESPONSE = 1,
        SHELTER_R48_CARD_USE_UNAVAILABLE     = 0,
        SHELTER_R48_CAP_BEFORE_CARD_READY    = 6,
        SHELTER_R48_CAP_CARD_READY           = 7
    };
    if (request->actionId == SHELTER_R48_ACTION_PROGRESS_RESPONSE) {
        switch (gameFlagGetNibble(GAME_FLAG_100)) {
            case SHELTER_R48_CARD_USE_UNAVAILABLE:
                capRunCommandWithTransition(SHELTER_R48_CAP_BEFORE_CARD_READY);
                break;
            case SHELTER_R48_CARD_USE_READY:
                capRunCommandWithTransition(SHELTER_R48_CAP_CARD_READY);
                break;
        }
    }
    return 0;
}

/// Advances the boss encounter to its interruption or defeat scene.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT`; the receiver, message ID and both
/// integer payload words are unused. Scene state 1 starts the interruption
/// script and becomes 2. Every other state starts the defeat script with its
/// skip target, spawns drift emitters at position indices 0, 1 and 3, and becomes 3.
/// Requires the room and actor overlay loaded; returns zero.
static s32 _shelterR48AdvanceBossSceneMsg(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum {
        SHELTER_R48_SCENE_BATTLE      = 1,
        SHELTER_R48_SCENE_INTERRUPTED = 2,
        SHELTER_R48_SCENE_DEFEATED    = 3
    };
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE) == SHELTER_R48_SCENE_BATTLE) {
        evsStartScript(gActor503500BossPhaseTransitionScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        gameFlagSetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE, SHELTER_R48_SCENE_INTERRUPTED);
    } else {
        // Queue the scene before the drift emitters at its three fixed positions.
        evsStartScriptWithSkip(gActor503500BossDefeatScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor503500BossDefeatSkipScript);
        taskSpawnFromTable(gActor503500SceneEffectTaskDescs, ACTOR_503500_SCENE_TASK_DRIFT_EMITTER, 0, 0);
        taskSpawnFromTable(gActor503500SceneEffectTaskDescs, ACTOR_503500_SCENE_TASK_DRIFT_EMITTER, 1, 0);
        taskSpawnFromTable(gActor503500SceneEffectTaskDescs, ACTOR_503500_SCENE_TASK_DRIFT_EMITTER, 3, 0);
        gameFlagSetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE, SHELTER_R48_SCENE_DEFEATED);
    }
    return 0;
}

/// Registers the room receiver and starts the skippable boss-entry scene.
///
/// Requires the room task in setup state 0 and its actor overlay loaded.
/// Resets the actor's fade handle, sets scene state 1 and advances to idle.
static void _shelterR48RoomSetupState(Task* task)
{
    enum { SHELTER_R48_SCENE_BATTLE = 1 };
    task->msgTable = _gShelterR48RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    actor503500ClearFadeFromBlackHandle(0);
    evsStartScriptWithSkip(gActor503500BossIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor503500BossIntroSkipScript);
    gameFlagSetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE, SHELTER_R48_SCENE_BATTLE);
    task->state += 1;
}

/// Leaves the initialized room task idle until another state is selected.
static void _shelterR48RoomIdleState(Task* task)
{
    char stackReservation[0x10]; // Unused storage retained for the callback's stack adjustment.
}

void shelterR48RoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gShelterR48RoomTaskStates;
    handlers.funcs[task->state](task);
}

void shelterR48SetBackgroundSpritesVisible(u8 visible)
{
    enum {
        SHELTER_R48_BACKGROUND_SPRITES_HIDE = 0,
        SHELTER_R48_BACKGROUND_SPRITES_SHOW = 1
    };
    GameLocationKey* location = &gGameSession->location;
    SpriteView*      views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    SpriteBatch*     batches;

    if (visible == SHELTER_R48_BACKGROUND_SPRITES_HIDE) {
        batches           = views[1].batches;
        batches[1].hidden = 1;
        batches[3].hidden = 1;
        batches           = views[2].batches;
        batches[1].hidden = 1;
        batches[4].hidden = 1;
        batches           = views[3].batches;
        batches[1].hidden = 1;
        batches[4].hidden = 1;
        batches           = views[5].batches;
        batches[2].hidden = 1;
        batches[3].hidden = 1;
        batches           = views[6].batches;
        batches[1].hidden = 1;
        batches[4].hidden = 1;
        batches           = views[7].batches;
        batches[1].hidden = 1;
        batches[5].hidden = 1;
        batches           = views[17].batches;
        batches[1].hidden = 1;
        batches[5].hidden = 1;
    } else if (visible == SHELTER_R48_BACKGROUND_SPRITES_SHOW) {
        batches           = views[1].batches;
        batches[1].hidden = 0;
        batches[3].hidden = 0;
        batches           = views[2].batches;
        batches[1].hidden = 0;
        batches[4].hidden = 0;
        batches           = views[3].batches;
        batches[1].hidden = 0;
        batches[4].hidden = 0;
        batches           = views[5].batches;
        batches[2].hidden = 0;
        batches[3].hidden = 0;
        batches           = views[6].batches;
        batches[1].hidden = 0;
        batches[4].hidden = 0;
        batches           = views[7].batches;
        batches[1].hidden = 0;
        batches[5].hidden = 0;
        batches           = views[17].batches;
        batches[1].hidden = 0;
        batches[5].hidden = 0;
    }
}

void shelterR48InitRingsAndDrawGlowTask(Task* task)
{
    enum {
        SHELTER_R48_GLOW_STATE_NEW     = 0,
        SHELTER_R48_GLOW_STATE_RUNNING = 1,
        SHELTER_R48_GLOW_VIEW_MASK     = (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 18),
        SHELTER_R48_GLOW_RADIUS_SCALE  = 256,
        SHELTER_R48_GLOW_ORANGE        = 0x5C40,
        SHELTER_R48_GLOW_BLUE          = 0x504C
    };
    s32 viewMask;
    s32 bandIndex;
    s32 segmentIndex;

    viewMask = 1 << viewGetMappedIndex();
    if (task->state == SHELTER_R48_GLOW_STATE_NEW) {
        // Seed every band segment's animation phase once for this room visit.
        gRoomEffectState->groundTraceEnabled = false;
        for (bandIndex = 0; bandIndex < ARRAY_SIZE(_gShelterR48RingTexturePhases); bandIndex++) {
            for (segmentIndex = 0; segmentIndex < ARRAY_SIZE(_gShelterR48RingTexturePhases[bandIndex]); segmentIndex++) {
                _gShelterR48RingTexturePhases[bandIndex][segmentIndex] = (RANDOM_LCG_ADVANCE()) >> 16;
            }
        }
        task->state = SHELTER_R48_GLOW_STATE_RUNNING;
    }
    // Keep drawing tied to the mapped camera and the room's card-use progress.
    if (viewMask & SHELTER_R48_GLOW_VIEW_MASK) {
        if (gameFlagGetNibble(GAME_FLAG_100) == SHELTER_R48_CARD_USE_READY) {
            _shelterR48DrawRoomGlow(&_gShelterR48GlowWorldPosition, SHELTER_R48_GLOW_RADIUS_SCALE, SHELTER_R48_GLOW_ORANGE);
        } else if (gameFlagGetNibble(GAME_FLAG_100) == SHELTER_R48_CARD_USE_STARTED) {
            _shelterR48DrawRoomGlow(&_gShelterR48GlowWorldPosition, SHELTER_R48_GLOW_RADIUS_SCALE, SHELTER_R48_GLOW_BLUE);
        }
    }
}

/// Places an effect at its parent's origin and refreshes its composed transform.
///
/// `coord` must be a live writable node, disjoint from its borrowed `parent`.
/// A non-NULL parent and all ancestors must remain live in an acyclic chain
/// until the effect detaches; NULL makes a root node. Replaces the nine Q12
/// rotation coefficients and all three translation words, preserving matrix
/// alignment bytes and `param` storage.
/// Clears the composition stamp before composing the full chain, which may
/// refresh ancestors and overwrite GTE state. Allocates and releases nothing.
static inline void _shelterR48AttachEffectCoord(GfxCoord* coord, GfxCoord* parent)
{
    coord->parent = parent;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[2]   = 0;
    coord->coord.t[1]   = 0;
    coord->coord.t[0]   = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void shelterR48SmallOrbTask(Task* task)
{
    enum {
        SHELTER_R48_SMALL_ORB_STATE_NEW         = 0,
        SHELTER_R48_SMALL_ORB_STATE_ATTACHED    = 1,
        SHELTER_R48_SMALL_ORB_MODE_NEW          = 0,
        SHELTER_R48_SMALL_ORB_MODE_TRAIL        = 1,
        SHELTER_R48_SMALL_ORB_MODE_BURST        = 2,
        SHELTER_R48_SMALL_ORB_MODE_FINISHED     = 3,
        SHELTER_R48_SMALL_ORB_TILE_RADIUS_SCALE = 0x380,
        SHELTER_R48_SMALL_ORB_BURST_PAIR_COUNT  = 4,
        SHELTER_R48_SMALL_ORB_LAUNCH_SPRAY_ARG  = 0x14002400,
        SHELTER_R48_SMALL_ORB_TRAIL_SPRAY_ARG   = 0x1001400,
        SHELTER_R48_SMALL_ORB_BURST_CORE_ARG    = 0x10002380,
        SHELTER_R48_SMALL_ORB_BURST_SPRAY_ARG   = 0x2002400,
        SHELTER_R48_SMALL_ORB_BURST_DRIFT_ARG   = 0x2202300
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         burstPairIndex;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // Suspension redraws the cached sprite before cancellation retires the effect.
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _waterDrawTileU16(coord, (u16)(work->age / 2), SHELTER_R48_SMALL_ORB_TILE_RADIUS_SCALE);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    if (task->state == SHELTER_R48_SMALL_ORB_STATE_NEW) {
        _shelterR48AttachEffectCoord(coord, work->parent);
        task->state = SHELTER_R48_SMALL_ORB_STATE_ATTACHED;
    }
    work->age += 1;
    switch (task->spawnArg1.value) {
        case SHELTER_R48_SMALL_ORB_MODE_NEW:
            effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_SMALL_ORB_LAUNCH_SPRAY_ARG, NULL);
            task->spawnArg1.value = SHELTER_R48_SMALL_ORB_MODE_TRAIL;
            return;
        case SHELTER_R48_SMALL_ORB_MODE_TRAIL:
            _waterDrawTileU16(coord, (u16)(work->age / 2), SHELTER_R48_SMALL_ORB_TILE_RADIUS_SCALE);
            if (!(work->age & 1)) {
                effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_SMALL_ORB_TRAIL_SPRAY_ARG, NULL);
            }
            // Trail mode retains its second age increment after drawing and emission.
            work->age += 1;
            return;
        case SHELTER_R48_SMALL_ORB_MODE_BURST:
            // Emit the stationary core before the four spray/drift pairs, then retire next tick.
            effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_SMALL_ORB_BURST_CORE_ARG, NULL);
            for (burstPairIndex = 0; burstPairIndex < SHELTER_R48_SMALL_ORB_BURST_PAIR_COUNT; burstPairIndex++) {
                effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_SMALL_ORB_BURST_SPRAY_ARG, NULL);
                effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, SHELTER_R48_SMALL_ORB_BURST_DRIFT_ARG, NULL);
            }
            task->spawnArg1.value = SHELTER_R48_SMALL_ORB_MODE_FINISHED;
            return;
        case SHELTER_R48_SMALL_ORB_MODE_FINISHED:
            effectKillTask(work, task);
            return;
    }
}

void shelterR48ProjectileTask(Task* task)
{
    enum {
        SHELTER_R48_PROJECTILE_STATE_NEW           = 0,
        SHELTER_R48_PROJECTILE_STATE_ATTACHED      = 1,
        SHELTER_R48_PROJECTILE_MODE_NEW            = 0,
        SHELTER_R48_PROJECTILE_MODE_TRAIL          = 1,
        SHELTER_R48_PROJECTILE_MODE_PARTICLES      = 2,
        SHELTER_R48_PROJECTILE_SPRITE_FRAME_COUNT  = 12,
        SHELTER_R48_PROJECTILE_SPRITE_RADIUS_SCALE = 0x800,
        SHELTER_R48_PROJECTILE_DRIFT_PERIOD        = 6,
        SHELTER_R48_PROJECTILE_LAUNCH_SPRAY_ARG    = 0x14002800,
        SHELTER_R48_PROJECTILE_TRAIL_SPRAY_ARG     = 0x12801800,
        SHELTER_R48_PROJECTILE_LINGER_DRIFT_ARG    = 0x2802800,
        SHELTER_R48_PROJECTILE_LINGER_SPRAY_ARG    = 0x12803800
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // Suspension redraws the cached sprite before cancellation retires the effect.
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _shelterR48DrawBankedDriftSprite(coord, ((work->age / 2) % SHELTER_R48_PROJECTILE_SPRITE_FRAME_COUNT), SHELTER_R48_PROJECTILE_SPRITE_RADIUS_SCALE, 0);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    if (task->state == SHELTER_R48_PROJECTILE_STATE_NEW) {
        _shelterR48AttachEffectCoord(coord, work->parent);
        task->state = SHELTER_R48_PROJECTILE_STATE_ATTACHED;
    }
    work->age += 1;
    switch (task->spawnArg1.value) {
        case SHELTER_R48_PROJECTILE_MODE_NEW:
            effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_PROJECTILE_LAUNCH_SPRAY_ARG, NULL);
            task->spawnArg1.value = SHELTER_R48_PROJECTILE_MODE_TRAIL;
            return;
        case SHELTER_R48_PROJECTILE_MODE_TRAIL:
            _shelterR48DrawBankedDriftSprite(coord, ((work->age / 2) % SHELTER_R48_PROJECTILE_SPRITE_FRAME_COUNT), SHELTER_R48_PROJECTILE_SPRITE_RADIUS_SCALE, 0);
            if (!(work->age & 1)) {
                effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_PROJECTILE_TRAIL_SPRAY_ARG, NULL);
            }
            // Trail mode retains its second age increment after drawing and emission.
            work->age += 1;
            return;
        case SHELTER_R48_PROJECTILE_MODE_PARTICLES:
            // Keep emitting without a core until the shot owner tears down this effect.
            if ((work->age % SHELTER_R48_PROJECTILE_DRIFT_PERIOD) == 0) {
                effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, SHELTER_R48_PROJECTILE_LINGER_DRIFT_ARG, NULL);
            }
            if (!(work->age & 1)) {
                effectSpawn(EFFECT_SHELTER_R48_SPRAY, coord, SHELTER_R48_PROJECTILE_LINGER_SPRAY_ARG, NULL);
            }
            return;
    }
}

void shelterR48LargeOrbTask(Task* task)
{
    enum {
        SHELTER_R48_LARGE_ORB_STATE_NEW              = 0,
        SHELTER_R48_LARGE_ORB_STATE_ATTACHED         = 1,
        SHELTER_R48_LARGE_ORB_MODE_NEW               = 0,
        SHELTER_R48_LARGE_ORB_MODE_TRAIL             = 1,
        SHELTER_R48_LARGE_ORB_MODE_PARTICLES         = 2,
        SHELTER_R48_LARGE_ORB_SPRITE_FRAME_COUNT     = 12,
        SHELTER_R48_LARGE_ORB_TRAIL_RADIUS_SCALE     = 0x800,
        SHELTER_R48_LARGE_ORB_SUSPENDED_RADIUS_SCALE = 0xa00,
        SHELTER_R48_LARGE_ORB_ALTERNATE_PALETTE      = 0x1000,
        SHELTER_R48_LARGE_ORB_LAUNCH_DRIFT_ARG       = 0x94002a00,
        SHELTER_R48_LARGE_ORB_TRAIL_DRIFT_ARG        = 0x92801800,
        SHELTER_R48_LARGE_ORB_LINGER_DRIFT_ARG       = 0x92603c00
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // Suspension redraws the cached sprite before cancellation retires the effect.
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _shelterR48DrawBankedDriftSprite(coord, ((work->age / 2) % SHELTER_R48_LARGE_ORB_SPRITE_FRAME_COUNT | SHELTER_R48_LARGE_ORB_ALTERNATE_PALETTE), SHELTER_R48_LARGE_ORB_SUSPENDED_RADIUS_SCALE, 0);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    if (task->state == SHELTER_R48_LARGE_ORB_STATE_NEW) {
        _shelterR48AttachEffectCoord(coord, work->parent);
        task->state = SHELTER_R48_LARGE_ORB_STATE_ATTACHED;
    }
    work->age += 1;
    switch (task->spawnArg1.value) {
        case SHELTER_R48_LARGE_ORB_MODE_NEW:
            effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, SHELTER_R48_LARGE_ORB_LAUNCH_DRIFT_ARG, NULL);
            task->spawnArg1.value = SHELTER_R48_LARGE_ORB_MODE_TRAIL;
            return;
        case SHELTER_R48_LARGE_ORB_MODE_TRAIL:
            _shelterR48DrawBankedDriftSprite(coord, ((work->age / 2) % SHELTER_R48_LARGE_ORB_SPRITE_FRAME_COUNT | SHELTER_R48_LARGE_ORB_ALTERNATE_PALETTE), SHELTER_R48_LARGE_ORB_TRAIL_RADIUS_SCALE, 0);
            if (!(work->age & 1)) {
                effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, SHELTER_R48_LARGE_ORB_TRAIL_DRIFT_ARG, NULL);
            }
            // Trail mode retains its second age increment after drawing and emission.
            work->age += 1;
            return;
        case SHELTER_R48_LARGE_ORB_MODE_PARTICLES:
            // Keep emitting without a core until the shot owner tears down this effect.
            if (!(work->age & 1)) {
                effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, SHELTER_R48_LARGE_ORB_LINGER_DRIFT_ARG, NULL);
            }
            return;
    }
}

void shelterR48YellowRingFlashTask(Task* task)
{
    /// Draws additive yellow discs at the saved radius and twice its low halfword.
    ///
    /// coord is a composed GfxCoord*, work a live EffectWork* and rgb a writable
    /// three-byte buffer disjoint from both. Arguments are evaluated repeatedly;
    /// pass stable pointers without side effects. scale is brightness and angle
    /// stores a radius in coordinate units: tint is (scale,scale,scale/4), narrowed
    /// to bytes after the signed shift. Doubling wraps through u16 then s16;
    /// both draws use signed-halfword radii. Leaves rgb for the caller's tint/band
    /// drawing. Requires scratch and frame arena space for both disc packet sets.
#define SHELTER_R48_DRAW_YELLOW_FLASH_DISCS(coord, work, rgb)                 \
    {                                                                         \
        (rgb)[0] = (work)->scale;                                             \
        (rgb)[1] = (work)->scale;                                             \
        (rgb)[2] = (work)->scale >> 2;                                        \
        effectDrawGouraudDisc((coord), (work)->angle, (rgb));                 \
        effectDrawGouraudDisc((coord), (s16)((u16)(work)->angle * 2), (rgb)); \
    }
    enum {
        SHELTER_R48_YELLOW_FLASH_STATE_NEW           = 0,
        SHELTER_R48_YELLOW_FLASH_STATE_CHARGING      = 1,
        SHELTER_R48_YELLOW_FLASH_STATE_FADING        = 2,
        SHELTER_R48_YELLOW_FLASH_CHARGE_UPDATES      = 90,
        SHELTER_R48_YELLOW_FLASH_BRIGHTNESS_RAMP     = 256,
        SHELTER_R48_YELLOW_FLASH_INITIAL_RADIUS      = 256,
        SHELTER_R48_YELLOW_FLASH_RADIUS_STEP         = 24,
        SHELTER_R48_YELLOW_FLASH_BAND_MIN_BRIGHTNESS = 97,
        SHELTER_R48_YELLOW_FLASH_BAND_RADIUS_SHIFT   = 8,
        SHELTER_R48_YELLOW_FLASH_BAND_WIDTH          = 512,
        SHELTER_R48_YELLOW_FLASH_FULL_BRIGHTNESS     = 255,
        SHELTER_R48_YELLOW_FLASH_FADE_STEP           = 16,
        SHELTER_R48_YELLOW_FLASH_FADE_RADIUS_STEP    = 96
    };
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        // Age counts even suspended visits; charge and fade advance only while running.
        work->age++;
        switch (task->state) {
            case SHELTER_R48_YELLOW_FLASH_STATE_NEW:
                _shelterR48AttachEffectCoord(coord, work->parent);
                task->spawnArg1.value = SHELTER_R48_YELLOW_FLASH_CHARGE_UPDATES;
                task->state           = SHELTER_R48_YELLOW_FLASH_STATE_CHARGING;
                work->scale           = 0;
                work->angle           = SHELTER_R48_YELLOW_FLASH_INITIAL_RADIUS;
                work->step            = SHELTER_R48_YELLOW_FLASH_BRIGHTNESS_RAMP / task->spawnArg1.value;
                // Fall through to draw the first charge tick after initialization.
            case SHELTER_R48_YELLOW_FLASH_STATE_CHARGING:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    SHELTER_R48_DRAW_YELLOW_FLASH_DISCS(coord, work, rgb);
                    if (work->scale >= SHELTER_R48_YELLOW_FLASH_BAND_MIN_BRIGHTNESS) {
                        rgb[0] = work->period;
                        rgb[1] = work->period;
                        rgb[2] = work->period >> 2;
                        effectDrawOuterGlowBand(coord, (s16)(task->spawnArg1.value << SHELTER_R48_YELLOW_FLASH_BAND_RADIUS_SHIFT), SHELTER_R48_YELLOW_FLASH_BAND_WIDTH, rgb);
                    }
                    return;
                }
                work->scale += work->step;
                work->angle += SHELTER_R48_YELLOW_FLASH_RADIUS_STEP;
                task->spawnArg1.value--;
                SHELTER_R48_DRAW_YELLOW_FLASH_DISCS(coord, work, rgb);
                if (work->scale >= SHELTER_R48_YELLOW_FLASH_BAND_MIN_BRIGHTNESS) {
                    work->period += (u16)work->step * 2;
                    rgb[0]        = work->period;
                    rgb[1]        = work->period;
                    rgb[2]        = work->period >> 2;
                    effectDrawOuterGlowBand(coord, (s16)(task->spawnArg1.value << SHELTER_R48_YELLOW_FLASH_BAND_RADIUS_SHIFT), SHELTER_R48_YELLOW_FLASH_BAND_WIDTH, rgb);
                }
                if (task->spawnArg1.value == 0) {
                    work->scale = SHELTER_R48_YELLOW_FLASH_FULL_BRIGHTNESS;
                    task->state = SHELTER_R48_YELLOW_FLASH_STATE_FADING;
                    effectSpawn(EFFECT_SHELTER_R48_RING_WALL, coord, 0, NULL);
                }
                return;
            case SHELTER_R48_YELLOW_FLASH_STATE_FADING:
                // Draw the saved brightness before applying the active-tick fade.
                if (work->scale >= SHELTER_R48_YELLOW_FLASH_FADE_STEP + 1) {
                    SHELTER_R48_DRAW_YELLOW_FLASH_DISCS(coord, work, rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= SHELTER_R48_YELLOW_FLASH_FADE_STEP;
                        work->angle -= SHELTER_R48_YELLOW_FLASH_FADE_RADIUS_STEP;
                    }
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
#undef SHELTER_R48_DRAW_YELLOW_FLASH_DISCS
}

void shelterR48RingWallTask(Task* task)
{
    enum {
        SHELTER_R48_RING_WALL_STATE_NEW          = 0,
        SHELTER_R48_RING_WALL_STATE_FADING       = 1,
        SHELTER_R48_RING_WALL_INITIAL_BRIGHTNESS = 128,
        SHELTER_R48_RING_WALL_FADE_STEP          = 8,
        SHELTER_R48_RING_WALL_RADIUS_STEP        = 512,
        SHELTER_R48_RING_WALL_SPREAD_STEP        = 256
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _shelterR48DrawRingBand(work, coord, 0);
        _shelterR48DrawRingBand(work, coord, 1);
        _shelterR48DrawRingBand(work, coord, 2);
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        work->age++;
        if (task->state == SHELTER_R48_RING_WALL_STATE_NEW) {
            work->age   = 1;
            work->scale = SHELTER_R48_RING_WALL_INITIAL_BRIGHTNESS;
            task->state = SHELTER_R48_RING_WALL_STATE_FADING;
        }
        if (work->scale >= SHELTER_R48_RING_WALL_FADE_STEP + 1) {
            work->scale -= SHELTER_R48_RING_WALL_FADE_STEP;
            work->angle += SHELTER_R48_RING_WALL_RADIUS_STEP;
            work->step  += SHELTER_R48_RING_WALL_SPREAD_STEP;
            _shelterR48DrawRingBand(work, coord, 0);
            _shelterR48DrawRingBand(work, coord, 1);
            _shelterR48DrawRingBand(work, coord, 2);
            return;
        }
    }
    effectKillTask(work, task);
}

/// Projects one of sixteen ring segments and selects its animated texture cell.
///
/// Borrows a writable scratch block containing two initialized sixteen-vertex
/// rings of signed-halfword world positions. `bandIndex` is 0..5 in the room's
/// initialized phase table and `segmentIndex` is 0..15, wrapping at the seam.
/// Requires the current projection parameters and `GsWSMATRIX` rotation and
/// translation already loaded. Writes `sxy0`..`sxy3` in centred screen pixels.
/// `projectionFlags` contains only the final three-vertex projection's FLAG;
/// the first vertex's flags are not combined with it. Leaves that final vertex's
/// SZ3 in the GTE for the caller to read as SZ3/4, without writing `otz`.
///
/// Adds the phase byte to signed frame age, takes signed remainder modulo six,
/// then narrows to `u16`. Nonnegative ages select cells 0..5; negative remainders
/// wrap on narrowing. The lookup remains between RTPT and the FIFO reads.
/// Retains neither pointer and overwrites GTE vector, projection and flag state.
static inline u16 _shelterR48ProjectRingSegment(EffectBandScratch* scratch, const EffectWork* work, s32 bandIndex, s32 segmentIndex)
{
    enum { SHELTER_R48_RING_FRAME_COUNT = 6 };
    s32 nextSegmentIndex;
    u16 textureFrame;

    // Save the first corner before RTPT replaces the screen-coordinate FIFO.
    gte_ldv0(&scratch->topRing[segmentIndex]);
    gte_rtps();
    gte_stsxy(&scratch->sxy0);
    nextSegmentIndex = (segmentIndex + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
    gte_ldv3(&scratch->topRing[nextSegmentIndex], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex]);
    gte_rtpt();
    textureFrame = (_gShelterR48RingTexturePhases[bandIndex][segmentIndex] + work->age) % SHELTER_R48_RING_FRAME_COUNT;
    gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
    gte_stflg(&scratch->projectionFlags);
    return textureFrame;
}

/// Draws one animated textured band between a base rim and a raised rim.
///
/// `bandIndex` is 0..5 in both the shape and per-segment phase tables. Each
/// rim has 16 vertices: angle/step give base radius/spread and period gives
/// lift, all in local coordinate units with halfword wrapping. `coord` is
/// borrowed with workm composed for GsWSMATRIX. Age selects one of six
/// 40-texel texture cells; scale's low byte modulates RGB. Bands 0..2 and
/// 3..5 use different palettes. The caller initializes the phase table.
/// Queues at most 16 semitransparent quads, rejecting negative final GTE
/// flags. Requires scratch space and primitive capacity; retains no pointer.
static void _shelterR48DrawRingBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex)
{
    enum {
        SHELTER_R48_RING_FRAME_TEXELS  = 40,
        SHELTER_R48_RING_TOP_V         = 96,
        SHELTER_R48_RING_BOTTOM_V      = 135,
        SHELTER_R48_RING_PALETTE_SPLIT = 3,
        SHELTER_R48_RING_WALL_CLUT     = 0x42C6,
        SHELTER_R48_SHOCKWAVE_CLUT     = 0x4282,
        SHELTER_R48_RING_TEXTURE_PAGE  = 0x2A,
        SHELTER_R48_RING_QUAD_CODE     = 0x2E,
        SHELTER_R48_RING_QUAD_WORDS    = 9,
        SHELTER_R48_RING_TRIG_SHIFT    = 12
    };
    EffectBandScratch* scratch;
    SVECTOR*           baseVertex;
    POLY_FT4*          quad;
    EffectBandShape*   shape;
    s32                segmentIndex;
    s32                angle;
    s32                leftU;
    u16                textureFrame;
    s16                raisedRadius;
    s16                baseRadius;
    u16                lift;
    u16                animatedLift;

    shape        = &_gShelterR48RingBandShapes[bandIndex];
    animatedLift = work->period;
    baseRadius   = work->angle;
    lift         = animatedLift + shape->lift;
    baseRadius  += shape->baseRadius;
    raisedRadius = baseRadius + work->step + shape->spread;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Build the two rims in local space, then retain their wrapped world positions.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                             = segmentIndex << 8;
        scratch->topRing[segmentIndex].vx = (rsin(angle) * raisedRadius) >> SHELTER_R48_RING_TRIG_SHIFT;
        scratch->topRing[segmentIndex].vy = -lift;
        scratch->topRing[segmentIndex].vz = (rcos(angle) * raisedRadius) >> SHELTER_R48_RING_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx    = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy    = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz    = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(angle) * baseRadius) >> SHELTER_R48_RING_TRIG_SHIFT;
        baseVertex                           = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        baseVertex->vy                       = 0;
        baseVertex->vz                       = (rcos(angle) * baseRadius) >> SHELTER_R48_RING_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx = (u16)scratch->bottomRing[segmentIndex].vx + (u16)coord->workm.t[0];
        baseVertex->vy                       = (u16)baseVertex->vy + (u16)coord->workm.t[1];
        baseVertex->vz                       = (u16)baseVertex->vz + (u16)coord->workm.t[2];
    }
    // Projection rejection consumes no primitive packet.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        textureFrame = _shelterR48ProjectRingSegment(scratch, work, bandIndex, segmentIndex);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, SHELTER_R48_RING_QUAD_WORDS);
            quad->code = SHELTER_R48_RING_QUAD_CODE;
            setRGB0(quad, *(const u8*)&work->scale, *(const u8*)&work->scale, *(const u8*)&work->scale);
            quad->tpage = SHELTER_R48_RING_TEXTURE_PAGE;
            quad->clut  = bandIndex < SHELTER_R48_RING_PALETTE_SPLIT ? SHELTER_R48_RING_WALL_CLUT : SHELTER_R48_SHOCKWAVE_CLUT;
            leftU       = textureFrame * SHELTER_R48_RING_FRAME_TEXELS;
            setUV4(quad, leftU, SHELTER_R48_RING_TOP_V, leftU + SHELTER_R48_RING_FRAME_TEXELS - 1, SHELTER_R48_RING_TOP_V, leftU, SHELTER_R48_RING_BOTTOM_V, leftU + SHELTER_R48_RING_FRAME_TEXELS - 1, SHELTER_R48_RING_BOTTOM_V);
            quad->x0 = (u16)scratch->sxy0.vx;
            quad->y0 = (u16)scratch->sxy0.vy;
            quad->x1 = (u16)scratch->sxy1.vx;
            quad->y1 = (u16)scratch->sxy1.vy;
            quad->x2 = (u16)scratch->sxy2.vx;
            quad->y2 = (u16)scratch->sxy2.vy;
            quad->x3 = (u16)scratch->sxy3.vx;
            quad->y3 = (u16)scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

#include "../../shared/effects/water_drift_task_u16.inc.c"

void shelterR48WaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/effects/water_spin_u16_corner.inc.c"

/// Draws one rotated, camera-facing cell of the eight-frame water-drift strip.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate.
/// `textureColumn` is unsigned. Frame 0..7 selects a 32-texel square at
/// V=224..255; UVs wrap to bytes without a bounds check. The 4-bit texture page
/// is at VRAM (704, 0), with its 16-colour palette at (304, 271).
///
/// `radiusScale * 31 / depth` gives the signed half-diagonal in pixels before
/// rotation. Drift tasks supply scale 0..4095. `spinAngle` uses 4096 units per
/// turn. With positive scale, zero puts the first corner above the centre,
/// a quarter turn to its right. Signed division truncates toward zero; the Q12 products round down
/// and must fit s32. Depth is SZ3/4, biased by +1 for sizing and sorting.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`; the primitive cursor must have space for the complete packet.
/// One word-aligned `EffectShapeScratch` is reserved on the initialized scratch
/// stack and released on every path. No pointer is retained; GTE state changes.
static void _waterDrawSpinU16(const GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    enum {
        WATER_SPIN_U16_DEPTH_BIAS      = 1,
        WATER_SPIN_U16_CELL_SHIFT      = 5,
        WATER_SPIN_U16_UV_SPAN_TEXELS  = 31,
        WATER_SPIN_U16_FIRST_TEXEL_ROW = 224,
        WATER_SPIN_U16_LAST_TEXEL_ROW  = 255,
        WATER_SPIN_U16_QUARTER_TURN    = 0x400
    };
    void**              scratchCursor;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    SVECTOR*            projectionPoint;
    s32                 cellU;
    s32                 lastU;
    s32                 perpendicularAngle;
    u16                 centreZ;

    // Project the composed centre before constructing the screen-space quad.
    scratchCursor                   = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd                      = *scratchCursor;
    (scratchEnd - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    projection                      = scratchEnd - 1;
    projection->worldPoint.vy       = (u16)coord->workm.t[1];
    centreZ                         = (u16)coord->workm.t[2];
    projection->worldPoint.vz       = centreZ;
    *scratchCursor                  = projection;
    projectionPoint                 = &projection->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(projectionPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        // Keep the room variant's one-unit bias for sizing and sorting.
        projection->depth += WATER_SPIN_U16_DEPTH_BIAS;
        quad               = gGpuPrimCursor;
        gGpuPrimCursor     = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setShadeTex(quad, 1);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(304, 271);
        cellU       = textureColumn << WATER_SPIN_U16_CELL_SHIFT;
        lastU       = cellU + WATER_SPIN_U16_UV_SPAN_TEXELS;
        setUV4(quad, cellU, WATER_SPIN_U16_FIRST_TEXEL_ROW, lastU, WATER_SPIN_U16_FIRST_TEXEL_ROW, cellU, WATER_SPIN_U16_LAST_TEXEL_ROW, lastU, WATER_SPIN_U16_LAST_TEXEL_ROW);
        // Opposite corners share an offset; the second pair is a quarter turn away.
        _waterComputeSpriteCornerOffset(projection, radiusScale, spinAngle);
        quad->x0           = projection->screenX + (u16)projection->extent.corner.x;
        quad->x3           = projection->screenX - (u16)projection->extent.corner.x;
        quad->y0           = projection->screenY - (u16)projection->extent.corner.y;
        quad->y3           = projection->screenY + (u16)projection->extent.corner.y;
        perpendicularAngle = spinAngle + WATER_SPIN_U16_QUARTER_TURN;
        _waterComputeSpriteCornerOffset(projection, radiusScale, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
        quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
        quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
        quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK_AT(scratchCursor, EffectShapeScratch);
}

#include "../../shared/effects/water_tile_u16_corners.inc.c"

/// Draws one upright, camera-facing cell of the eight-frame water-spray grid.
///
/// `coord` is borrowed with `workm` already composed in the input space of
/// `GsWSMATRIX`; drift tasks normally supply view-space translations. Each
/// translation keeps its low 16 bits as a signed GTE coordinate. `textureCell`
/// keeps its low 16 bits. Its low three bits choose a 56-texel square from four
/// columns and two rows, starting at V=0. The 4-bit texture page is at VRAM
/// (768, 0), with its 16-colour palette at (304, 270).
///
/// `(s16)radiusScale * 55 / depth` gives the signed horizontal half-width `r`
/// in pixels. Drift tasks supply scale 0..4095. Signed division truncates
/// toward zero. The top edge is centre Y - r - (r >> 1), the bottom centre Y +
/// (r >> 1), placing the centre about a quarter of the height above the bottom
/// edge. Depth is SZ3/4, biased by +1 for sizing and sorting.
///
/// A nonnegative GTE FLAG queues one raw-texture, additive semi-transparent
/// `POLY_FT4`; the primitive cursor must have space for the complete packet.
/// One word-aligned `EffectCentreScratch` is reserved on the initialized
/// scratch stack and released on every path. No pointer is retained; GTE state
/// changes.
static void _waterDrawTileU16(const GfxCoord* coord, s32 textureCell, s32 radiusScale)
{
    enum {
        WATER_TILE_U16_DEPTH_BIAS        = 1,
        WATER_TILE_U16_COLUMNS           = 4,
        WATER_TILE_U16_CELLS             = 8,
        WATER_TILE_U16_ROW_SHIFT         = 2,
        WATER_TILE_U16_CELL_TEXELS       = 56,
        WATER_TILE_U16_UV_SPAN_TEXELS    = WATER_TILE_U16_CELL_TEXELS - 1,
        WATER_TILE_U16_FIRST_TEXEL_ROW   = 0,
        WATER_TILE_U16_PERSPECTIVE_SCALE = 55,
        WATER_TILE_U16_PACKET_CODE       = 0x2F
    };
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    u16                  frameIndex;
    u32                  cellIndex;
    s32                  rowTexelOffset;
    u8                   firstU;
    u8                   lastU;
    u8                   firstV;
    u8                   lastV;

    // Project the composed centre before constructing the screen-space quad.
    frameIndex                = textureCell;
    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        // Keep the room variant's one-unit bias for sizing and sorting.
        projection->depth += WATER_TILE_U16_DEPTH_BIAS;
        quad               = gGpuPrimCursor;
        gGpuPrimCursor     = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, WATER_TILE_U16_PACKET_CODE);
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 768, 0);
        quad->clut  = getClut(304, 270);
        // Wrap the animation frame over the eight-cell texture grid.
        cellIndex      = frameIndex;
        firstU         = (cellIndex & (WATER_TILE_U16_COLUMNS - 1)) * WATER_TILE_U16_CELL_TEXELS;
        rowTexelOffset = ((cellIndex & (WATER_TILE_U16_CELLS - 1)) >> WATER_TILE_U16_ROW_SHIFT) * WATER_TILE_U16_CELL_TEXELS;
        firstV         = rowTexelOffset + WATER_TILE_U16_FIRST_TEXEL_ROW;
        lastV          = rowTexelOffset + WATER_TILE_U16_FIRST_TEXEL_ROW + WATER_TILE_U16_UV_SPAN_TEXELS;
        lastU          = firstU + WATER_TILE_U16_UV_SPAN_TEXELS;
        setUV4(quad, firstU, firstV, lastU, firstV, firstU, lastV, lastU, lastV);
        projection->screenExtent = ((s16)radiusScale * WATER_TILE_U16_PERSPECTIVE_SCALE) / projection->depth;
        _waterSetUprightSpriteCorners(quad, projection);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Writes the signed pixel offset to one rotated drift-sprite corner.
///
/// `projection` borrows a live workspace with positive `depth`; only
/// `extent.corner` changes and no pointer is retained. The signed half-diagonal
/// is `sizeFactor * 47 / depth` pixels, truncated toward zero before rotation.
/// Products must fit s32; the Q12 arithmetic shift rounds negative products down.
///
/// `cornerAngle` is absolute, in 4096 units per turn. X points right and Y up:
/// with a positive half-diagonal, zero points up and a quarter turn points right.
/// The drawer subtracts Y from the centre and uses opposite signs for each pair
/// of corners, repeating a quarter turn later for the other pair.
static inline void _shelterR48RotateDriftCorner(EffectShapeScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        SHELTER_R48_DRIFT_PERSPECTIVE_SCALE  = 47, // Multiplier in the depth-divided half-diagonal numerator
        SHELTER_R48_DRIFT_TRIG_FRACTION_BITS = 12  // Fractional bits in rsin/rcos samples; 4096 represents 1.0
    };
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                  = rsin(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * SHELTER_R48_DRIFT_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * trigSample) >> SHELTER_R48_DRIFT_TRIG_FRACTION_BITS;
    trigSample                  = rcos(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * SHELTER_R48_DRIFT_PERSPECTIVE_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * trigSample) >> SHELTER_R48_DRIFT_TRIG_FRACTION_BITS;
}

/// Binds Shelter R48's exported `void (Task*)` callback for this drift instance.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Used only as the definition name; no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK shelterR48SpriteDriftTask
/// Binds the twelve-frame drift drawer: `(const GfxCoord*, u16 frameAndPalette, s16 size, s16 angle)`.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_BANKED _shelterR48DrawBankedDriftSprite
/// Binds the ten-frame drift drawer with the same coordinate, packed-frame, size and angle contract.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE _shelterR48DrawAlternateDriftSprite
/// Selects the drift sprite's palette bank from the spawn argument's sign bit.
///
/// Define as integer 1 before including `effect_sprite_drift.inc.c`, as Shelter
/// R48 does; 0 or undefined selects the default encoding (bit 31 chooses the
/// drawer, bits 28..30 choose the palette bank). With 1, initialization chooses
/// the banked drawer when bits 28..31 are all zero, otherwise the alternate drawer. Bit 31 alone
/// sets `EffectWork::pos.vx` to 0 or 0x1000, ORed with the animation cell index
/// in the drawer's u16 argument. Both R48 drawers use bank 1 for CLUT 0x428F
/// and bank 0 for their ordinary palettes.
///
/// Suspended updates choose A for a nonnegative spawn argument and B for a
/// negative one, regardless of the running drawer state, retaining the current
/// cell and bank. The bank is initialized only on the first running update.
/// `EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED` overrides suspended selection.
/// The fragment tests this configuration with #if/#elif and undefines it
/// after inclusion.
#define EFFECT_SPRITE_DRIFT_SIGN_BANK 1
/// Applies fixed negative-Y acceleration to every moving drift sprite.
///
/// Define as integer 1 before including `effect_sprite_drift.inc.c`, as Shelter
/// R48 does; 0 or undefined retains movement kind 7's age-dependent positive-Y
/// acceleration. Kind is bits 24..27 of `Task::spawnArg1.value`. With 1, both
/// drawer states ignore kind: state 1 subtracts 2 from Y velocity per moving
/// update, and state 2 subtracts 1 (coordinate units per update squared).
/// Position advances with the current velocity before this signed-halfword
/// velocity update. Initialization, suspension and sprites with `EffectWork::step`
/// zero do not accelerate. The fragment tests this switch with #if and undefines
/// it after inclusion.
#define EFFECT_SPRITE_DRIFT_FIXED_NEGATIVE_Y_ACCELERATION 1
#include "../../shared/effects/effect_sprite_drift.inc.c"

/// Draws a rotating twelve-cell sprite with a per-cell or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; translation is narrowed
/// to s16. Bits 0..11 of `frameAndPalette` hold cell 0..11; any selector in
/// bits 12..15 uses CLUT 0x428F instead of the cell palette on VRAM row 270.
/// The five-column sheet has 48-texel cells at V=112 on texture page 0x2C.
///
/// `size * 47 / (SZ3/4 + 1)` is the signed pixel half-diagonal before Q12
/// rotation; `angle` uses 4096 units per turn. A nonnegative GTE FLAG queues
/// an additive raw-texture FT4, also sorted with the biased depth. One scratch
/// block is borrowed and released on every path; the initialized scratch stack
/// and primitive cursor must provide capacity. No pointer is retained.
static void _shelterR48DrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        SHELTER_R48_DRIFT_FIRST_TEXEL_ROW   = 112,
        SHELTER_R48_DRIFT_LAST_TEXEL_ROW    = -97,
        SHELTER_R48_DRIFT_FRAME_MASK        = 0xFFF,
        SHELTER_R48_DRIFT_PALETTE_SHIFT     = 12,
        SHELTER_R48_DRIFT_CELLS_PER_ROW     = 5,
        SHELTER_R48_DRIFT_CELL_PITCH_TEXELS = 48,
        SHELTER_R48_DRIFT_UV_SPAN_TEXELS    = 47,
        SHELTER_R48_DRIFT_QUARTER_TURN      = 0x400,
        SHELTER_R48_DRIFT_PACKET_WORDS      = sizeof(POLY_FT4) / sizeof(u32) - 1,
        SHELTER_R48_DRIFT_PACKET_CODE       = 0x2F,
        SHELTER_R48_DRIFT_TEXTURE_PAGE      = 0x2C,
        SHELTER_R48_DRIFT_ALTERNATE_CLUT    = 0x428F,
        SHELTER_R48_DRIFT_FIRST_FRAME_CLUT  = 0x4380,
        SHELTER_R48_DRIFT_CLUT_COLUMN_MASK  = 0x3F
    };
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    u16                 cellColumn;
    u16                 cellRow;
    s32                 cellU;
    s32                 cellV;
    s32                 cornerAngle;
    s32                 perpendicularAngle;
    s32                 paletteSelector;
    s32                 frameIndex;

    scratchEnd                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (scratchEnd - 1)->worldPoint.vx = coord->workm.t[0];
    SCRATCH_STACK_CURSOR(void)      = scratchEnd - 1;
    projection                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    projection->worldPoint.vy       = coord->workm.t[1];
    projection->worldPoint.vz       = coord->workm.t[2];
    frameIndex                      = frameAndPalette & SHELTER_R48_DRIFT_FRAME_MASK;
    paletteSelector                 = frameAndPalette >> SHELTER_R48_DRIFT_PALETTE_SHIFT;
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        projection->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, SHELTER_R48_DRIFT_PACKET_WORDS);
        setcode(quad, SHELTER_R48_DRIFT_PACKET_CODE);
        quad->tpage = SHELTER_R48_DRIFT_TEXTURE_PAGE;
        if (paletteSelector != 0) {
            quad->clut = SHELTER_R48_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = (frameIndex & SHELTER_R48_DRIFT_CLUT_COLUMN_MASK) | SHELTER_R48_DRIFT_FIRST_FRAME_CLUT;
        }
        cellColumn  = (u16)frameIndex % SHELTER_R48_DRIFT_CELLS_PER_ROW;
        cellRow     = (u16)frameIndex / SHELTER_R48_DRIFT_CELLS_PER_ROW;
        cornerAngle = angle;
        cellU       = cellColumn * SHELTER_R48_DRIFT_CELL_PITCH_TEXELS;
        cellV       = cellRow * SHELTER_R48_DRIFT_CELL_PITCH_TEXELS;
        setUV4(quad, cellU, cellV + SHELTER_R48_DRIFT_FIRST_TEXEL_ROW, cellU + SHELTER_R48_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_R48_DRIFT_FIRST_TEXEL_ROW, cellU, cellV + SHELTER_R48_DRIFT_LAST_TEXEL_ROW, cellU + SHELTER_R48_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_R48_DRIFT_LAST_TEXEL_ROW);
        _shelterR48RotateDriftCorner(projection, size, cornerAngle);
        quad->x0           = projection->screenX + (u16)projection->extent.corner.x;
        quad->x3           = projection->screenX - (u16)projection->extent.corner.x;
        quad->y0           = projection->screenY - (u16)projection->extent.corner.y;
        quad->y3           = projection->screenY + (u16)projection->extent.corner.y;
        perpendicularAngle = cornerAngle + SHELTER_R48_DRIFT_QUARTER_TURN;
        _shelterR48RotateDriftCorner(projection, size, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
        quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
        quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
        quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws a rotating ten-cell sprite with a base or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; translation is narrowed
/// to s16. Bits 0..11 of `frameAndPalette` hold cell 0..9; any nonzero selector
/// in bits 12..15 uses CLUT 0x428F instead of 0x43D0. The five-column sheet
/// has 48-texel cells at V=128 on texture page 0x2B.
///
/// `size * 47 / (SZ3/4 + 1)` is the signed pixel half-diagonal before Q12
/// rotation; `angle` uses 4096 units per turn. A nonnegative GTE FLAG queues
/// an additive raw-texture FT4, sorted with biased depth. One scratch block
/// is borrowed and released on every path; the initialized scratch stack and
/// primitive cursor must provide capacity. No pointer is retained.
static void _shelterR48DrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        SHELTER_R48_DRIFT_FIRST_TEXEL_ROW   = -128,
        SHELTER_R48_DRIFT_LAST_TEXEL_ROW    = -81,
        SHELTER_R48_DRIFT_FRAME_MASK        = 0xFFF,
        SHELTER_R48_DRIFT_PALETTE_SHIFT     = 12,
        SHELTER_R48_DRIFT_CELLS_PER_ROW     = 5,
        SHELTER_R48_DRIFT_CELL_PITCH_TEXELS = 48,
        SHELTER_R48_DRIFT_UV_SPAN_TEXELS    = 47,
        SHELTER_R48_DRIFT_QUARTER_TURN      = 0x400,
        SHELTER_R48_DRIFT_PACKET_WORDS      = sizeof(POLY_FT4) / sizeof(u32) - 1,
        SHELTER_R48_DRIFT_PACKET_CODE       = 0x2F,
        SHELTER_R48_DRIFT_TEXTURE_PAGE      = 0x2B,
        SHELTER_R48_DRIFT_ALTERNATE_CLUT    = 0x428F,
        SHELTER_R48_DRIFT_BASE_CLUT         = 0x43D0
    };
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    u16                 cellColumn;
    u16                 cellRow;
    s32                 cellU;
    s32                 cellV;
    s32                 cornerAngle;
    s32                 perpendicularAngle;
    s32                 paletteSelector;
    u16                 frameIndex;

    scratchEnd                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (scratchEnd - 1)->worldPoint.vx = coord->workm.t[0];
    SCRATCH_STACK_CURSOR(void)      = scratchEnd - 1;
    projection                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    projection->worldPoint.vy       = coord->workm.t[1];
    projection->worldPoint.vz       = coord->workm.t[2];
    frameIndex                      = frameAndPalette & SHELTER_R48_DRIFT_FRAME_MASK;
    paletteSelector                 = frameAndPalette >> SHELTER_R48_DRIFT_PALETTE_SHIFT;
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        projection->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, SHELTER_R48_DRIFT_PACKET_WORDS);
        setcode(quad, SHELTER_R48_DRIFT_PACKET_CODE);
        quad->tpage = SHELTER_R48_DRIFT_TEXTURE_PAGE;
        if (paletteSelector != 0) {
            quad->clut = SHELTER_R48_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = SHELTER_R48_DRIFT_BASE_CLUT;
        }
        cellColumn  = (u16)frameIndex % SHELTER_R48_DRIFT_CELLS_PER_ROW;
        cellRow     = (u16)frameIndex / SHELTER_R48_DRIFT_CELLS_PER_ROW;
        cornerAngle = angle;
        cellU       = cellColumn * SHELTER_R48_DRIFT_CELL_PITCH_TEXELS;
        cellV       = cellRow * SHELTER_R48_DRIFT_CELL_PITCH_TEXELS;
        setUV4(quad, cellU, cellV + SHELTER_R48_DRIFT_FIRST_TEXEL_ROW, cellU + SHELTER_R48_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_R48_DRIFT_FIRST_TEXEL_ROW, cellU, cellV + SHELTER_R48_DRIFT_LAST_TEXEL_ROW, cellU + SHELTER_R48_DRIFT_UV_SPAN_TEXELS, cellV + SHELTER_R48_DRIFT_LAST_TEXEL_ROW);
        _shelterR48RotateDriftCorner(projection, size, cornerAngle);
        quad->x0           = projection->screenX + (u16)projection->extent.corner.x;
        quad->x3           = projection->screenX - (u16)projection->extent.corner.x;
        quad->y0           = projection->screenY - (u16)projection->extent.corner.y;
        quad->y3           = projection->screenY + (u16)projection->extent.corner.y;
        perpendicularAngle = cornerAngle + SHELTER_R48_DRIFT_QUARTER_TURN;
        _shelterR48RotateDriftCorner(projection, size, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
        quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
        quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
        quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void shelterR48OrangeRingFlashTask(Task* task)
{
    /// Draws additive orange discs at the saved radius and twice its low halfword.
    ///
    /// coord is a composed GfxCoord*, work a live EffectWork* and rgb a writable
    /// three-byte buffer disjoint from both. Arguments are evaluated repeatedly;
    /// pass stable pointers without side effects. scale is brightness and angle
    /// stores a radius in coordinate units: tint is (scale,scale/2,scale/4), narrowed
    /// to bytes after signed shifts. Doubling wraps through u16 then s16;
    /// both draws use signed-halfword radii. Leaves rgb for the caller's tint/band
    /// drawing. Requires scratch and frame arena space for both disc packet sets.
#define SHELTER_R48_DRAW_ORANGE_FLASH_DISCS(coord, work, rgb)                 \
    {                                                                         \
        (rgb)[0] = (work)->scale;                                             \
        (rgb)[1] = (work)->scale >> 1;                                        \
        (rgb)[2] = (work)->scale >> 2;                                        \
        effectDrawGouraudDisc((coord), (work)->angle, (rgb));                 \
        effectDrawGouraudDisc((coord), (s16)((u16)(work)->angle * 2), (rgb)); \
    }
    enum {
        SHELTER_R48_ORANGE_FLASH_STATE_NEW         = 0,
        SHELTER_R48_ORANGE_FLASH_STATE_CHARGING    = 1,
        SHELTER_R48_ORANGE_FLASH_STATE_FADING      = 2,
        SHELTER_R48_ORANGE_FLASH_ATTACH_PITCH      = -1024,
        SHELTER_R48_ORANGE_FLASH_BRIGHTNESS_RAMP   = 256,
        SHELTER_R48_ORANGE_FLASH_INITIAL_RADIUS    = 256,
        SHELTER_R48_ORANGE_FLASH_BAND_PULSE_PERIOD = 10,
        SHELTER_R48_ORANGE_FLASH_BAND_WIDTH        = 256,
        SHELTER_R48_ORANGE_FLASH_FULL_BRIGHTNESS   = 255,
        SHELTER_R48_ORANGE_FLASH_FADE_STEP         = 4
    };
    EffectWork* work;
    GfxCoord*   coord;
    EffectWork* shockwaveWork;
    u8          rgb[3];
    s32         brightnessStep;
    s16         previousBrightness;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        // Age counts even suspended visits; charge and fade advance only while running.
        work->age++;
        switch (task->state) {
            case SHELTER_R48_ORANGE_FLASH_STATE_NEW:
                coord->parent = work->parent;
                gfxRotMatrixX(&coord->coord, SHELTER_R48_ORANGE_FLASH_ATTACH_PITCH, GRAPHICS_ROTATION_REPLACE);
                coord->coord.t[2]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[0]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                task->state = SHELTER_R48_ORANGE_FLASH_STATE_CHARGING;
                work->scale = 0;
                work->angle = SHELTER_R48_ORANGE_FLASH_INITIAL_RADIUS;
                work->step  = SHELTER_R48_ORANGE_FLASH_BRIGHTNESS_RAMP / task->spawnArg1.value;
                // Fall through to draw the first charge tick after initialization.
            case SHELTER_R48_ORANGE_FLASH_STATE_CHARGING:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    SHELTER_R48_DRAW_ORANGE_FLASH_DISCS(coord, work, rgb);
                    effectDrawOuterGlowBand(coord, (s16)((task->spawnArg1.value % SHELTER_R48_ORANGE_FLASH_BAND_PULSE_PERIOD) * (work->scale << 2)), SHELTER_R48_ORANGE_FLASH_BAND_WIDTH, rgb);
                    return;
                }
                previousBrightness = work->scale;
                brightnessStep     = (u16)work->step;
                work->scale        = previousBrightness + brightnessStep;
                work->angle       += (u16)work->step * 8;
                task->spawnArg1.value--;
                SHELTER_R48_DRAW_ORANGE_FLASH_DISCS(coord, work, rgb);
                effectDrawOuterGlowBand(coord, (s16)((task->spawnArg1.value % SHELTER_R48_ORANGE_FLASH_BAND_PULSE_PERIOD) * (work->scale << 2)), SHELTER_R48_ORANGE_FLASH_BAND_WIDTH, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale   = SHELTER_R48_ORANGE_FLASH_FULL_BRIGHTNESS;
                    task->state   = SHELTER_R48_ORANGE_FLASH_STATE_FADING;
                    shockwaveWork = effectSpawn(EFFECT_SHELTER_R48_SHOCKWAVE_RINGS, coord, 0, NULL);
                    if (shockwaveWork != NULL) {
                        taskReparent(task, shockwaveWork->task);
                    }
                }
                return;
            case SHELTER_R48_ORANGE_FLASH_STATE_FADING:
                // Draw the saved brightness before applying the active-tick fade.
                if (work->scale >= SHELTER_R48_ORANGE_FLASH_FADE_STEP + 1) {
                    SHELTER_R48_DRAW_ORANGE_FLASH_DISCS(coord, work, rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= SHELTER_R48_ORANGE_FLASH_FADE_STEP;
                    }
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
#undef SHELTER_R48_DRAW_ORANGE_FLASH_DISCS
}

void shelterR48ShockwaveRingsTask(Task* task)
{
    enum {
        SHELTER_R48_SHOCKWAVE_STATE_NEW          = 0,
        SHELTER_R48_SHOCKWAVE_STATE_EXPANDING    = 1,
        SHELTER_R48_SHOCKWAVE_STATE_FADING       = 2,
        SHELTER_R48_SHOCKWAVE_INITIAL_BRIGHTNESS = 128,
        SHELTER_R48_SHOCKWAVE_FADE_AGE           = 49,
        SHELTER_R48_SHOCKWAVE_GROWTH_STEP        = 16,
        SHELTER_R48_SHOCKWAVE_FADE_STEP          = 16
    };
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _shelterR48DrawRingBand(work, coord, 3);
        _shelterR48DrawRingBand(work, coord, 4);
        _shelterR48DrawRingBand(work, coord, 5);
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        work->age++;
        // Attach the effect to its parent before starting the expansion.
        switch (task->state) {
            case SHELTER_R48_SHOCKWAVE_STATE_NEW:
                _shelterR48AttachEffectCoord(coord, work->parent);
                work->age   = 1;
                work->scale = SHELTER_R48_SHOCKWAVE_INITIAL_BRIGHTNESS;
                task->state = SHELTER_R48_SHOCKWAVE_STATE_EXPANDING;
                return;
            case SHELTER_R48_SHOCKWAVE_STATE_EXPANDING:
                _shelterR48DrawRingBand(work, coord, 3);
                _shelterR48DrawRingBand(work, coord, 4);
                _shelterR48DrawRingBand(work, coord, 5);
                work->angle  += SHELTER_R48_SHOCKWAVE_GROWTH_STEP;
                work->period += SHELTER_R48_SHOCKWAVE_GROWTH_STEP;
                work->step   += SHELTER_R48_SHOCKWAVE_GROWTH_STEP;
                rgb[0]        = work->scale;
                rgb[1]        = work->scale >> 1;
                rgb[2]        = work->scale >> 2;
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                if (work->age >= SHELTER_R48_SHOCKWAVE_FADE_AGE) {
                    task->state = SHELTER_R48_SHOCKWAVE_STATE_FADING;
                }
                return;
            case SHELTER_R48_SHOCKWAVE_STATE_FADING:
                if (work->scale >= SHELTER_R48_SHOCKWAVE_FADE_STEP + 1) {
                    work->scale  -= SHELTER_R48_SHOCKWAVE_FADE_STEP;
                    work->angle  += SHELTER_R48_SHOCKWAVE_GROWTH_STEP;
                    work->period += SHELTER_R48_SHOCKWAVE_GROWTH_STEP;
                    work->step   += SHELTER_R48_SHOCKWAVE_GROWTH_STEP;
                    _shelterR48DrawRingBand(work, coord, 3);
                    _shelterR48DrawRingBand(work, coord, 4);
                    _shelterR48DrawRingBand(work, coord, 5);
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 1;
                    rgb[2] = work->scale >> 2;
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

void shelterR48PinkRingFlashTask(Task* task)
{
    /// Draws both charging discs and bands, halving RGB for the outer band.
    ///
    /// Arguments must be live initialized object pointers and a writable
    /// three-byte RGB array. They are expanded repeatedly; pass simple
    /// variables without side effects. Requires composed drawing coordinates
    /// and the charge geometry constants below. Expands to a compound statement.
#define SHELTER_R48_DRAW_PINK_CHARGE(work, coord, task, rgb)                                                                  \
    {                                                                                                                         \
        (rgb)[0] = (work)->scale;                                                                                             \
        (rgb)[1] = (work)->scale >> 2;                                                                                        \
        (rgb)[2] = (work)->scale >> 1;                                                                                        \
        effectDrawGouraudDisc((coord), (work)->angle, (rgb));                                                                 \
        effectDrawGouraudDisc((coord), (s16)((u16)(work)->angle * 2), (rgb));                                                 \
        effectDrawOuterGlowBand((coord), (s16)((u16)(task)->spawnArg1.value * 16 + SHELTER_R48_PINK_FLASH_INNER_BAND_RADIUS), \
                                SHELTER_R48_PINK_FLASH_INNER_BAND_WIDTH, (rgb));                                              \
        (rgb)[0] >>= 1;                                                                                                       \
        (rgb)[1] >>= 1;                                                                                                       \
        (rgb)[2] >>= 1;                                                                                                       \
        effectDrawOuterGlowBand((coord), (s16)((u16)(task)->spawnArg1.value * 32 + SHELTER_R48_PINK_FLASH_OUTER_BAND_RADIUS), \
                                SHELTER_R48_PINK_FLASH_OUTER_BAND_WIDTH, (rgb));                                              \
    }
    enum {
        SHELTER_R48_PINK_FLASH_STATE_NEW           = 0,
        SHELTER_R48_PINK_FLASH_STATE_CHARGING      = 1,
        SHELTER_R48_PINK_FLASH_STATE_FLASH         = 2,
        SHELTER_R48_PINK_FLASH_STATE_SPREADING     = 3,
        SHELTER_R48_PINK_FLASH_STATE_SHRINKING     = 4,
        SHELTER_R48_PINK_FLASH_CHARGE_UPDATES      = 30,
        SHELTER_R48_PINK_FLASH_FULL_BRIGHTNESS     = 255,
        SHELTER_R48_PINK_FLASH_FADE_STEP           = 16,
        SHELTER_R48_PINK_FLASH_BEAM_RADIUS         = 1536,
        SHELTER_R48_PINK_FLASH_BEAM_YAW_LIMIT      = 512,
        SHELTER_R48_PINK_FLASH_BEAM_YAW_STEP       = 16,
        SHELTER_R48_PINK_FLASH_BEAM_COLOR          = 0xC36,
        SHELTER_R48_PINK_FLASH_INITIAL_DISC_RADIUS = 256,
        SHELTER_R48_PINK_FLASH_BRIGHTNESS_RAMP     = 256,
        SHELTER_R48_PINK_FLASH_DISC_SHRINK_STEP    = 96,
        SHELTER_R48_PINK_FLASH_BEAM_SPREAD_SHRINK  = 24,
        SHELTER_R48_PINK_FLASH_BEAM_FINAL_SHRINK   = 48,
        SHELTER_R48_PINK_FLASH_INNER_BAND_RADIUS   = 2048,
        SHELTER_R48_PINK_FLASH_INNER_BAND_WIDTH    = 256,
        SHELTER_R48_PINK_FLASH_OUTER_BAND_RADIUS   = 3072,
        SHELTER_R48_PINK_FLASH_OUTER_BAND_WIDTH    = 192
    };
    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];
    s32         brightnessStep;
    s16         brightness;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case SHELTER_R48_PINK_FLASH_STATE_NEW:
                _shelterR48AttachEffectCoord(coord, work->parent);
                task->state           = SHELTER_R48_PINK_FLASH_STATE_CHARGING;
                task->spawnArg1.value = SHELTER_R48_PINK_FLASH_CHARGE_UPDATES;
                work->scale           = 0;
                work->angle           = SHELTER_R48_PINK_FLASH_INITIAL_DISC_RADIUS;
                work->step            = SHELTER_R48_PINK_FLASH_BRIGHTNESS_RAMP / task->spawnArg1.value;
            case SHELTER_R48_PINK_FLASH_STATE_CHARGING:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    SHELTER_R48_DRAW_PINK_CHARGE(work, coord, task, rgb);
                    return;
                }
                brightness     = work->scale;
                brightnessStep = (u16)work->step;
                work->scale    = brightness + brightnessStep;
                work->angle   += (u16)work->step * 8;
                task->spawnArg1.value--;
                SHELTER_R48_DRAW_PINK_CHARGE(work, coord, task, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale  = SHELTER_R48_PINK_FLASH_FULL_BRIGHTNESS;
                    task->state  = SHELTER_R48_PINK_FLASH_STATE_FLASH;
                    work->period = SHELTER_R48_PINK_FLASH_BEAM_RADIUS;
                    work->step   = 0;
                }
                return;
            // The screen flash fades before the two capsules separate and shrink.
            case SHELTER_R48_PINK_FLASH_STATE_FLASH:
                if (work->scale >= SHELTER_R48_PINK_FLASH_FADE_STEP + 1) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    effectDrawGouraudDisc(coord, work->angle, rgb);
                    effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->scale -= SHELTER_R48_PINK_FLASH_FADE_STEP;
                        work->angle -= SHELTER_R48_PINK_FLASH_DISC_SHRINK_STEP;
                    }
                    effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                } else {
                    task->state = SHELTER_R48_PINK_FLASH_STATE_SPREADING;
                }
                _shelterR48DrawGlowBeam(coord, work->period, work->step, SHELTER_R48_PINK_FLASH_BEAM_COLOR);
                _shelterR48DrawGlowBeam(coord, work->period, (s16)-work->step, SHELTER_R48_PINK_FLASH_BEAM_COLOR);
                return;
            case SHELTER_R48_PINK_FLASH_STATE_SPREADING:
                _shelterR48DrawGlowBeam(coord, work->period, work->step, SHELTER_R48_PINK_FLASH_BEAM_COLOR);
                _shelterR48DrawGlowBeam(coord, work->period, (s16)-work->step, SHELTER_R48_PINK_FLASH_BEAM_COLOR);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    if (work->step < SHELTER_R48_PINK_FLASH_BEAM_YAW_LIMIT) {
                        work->period -= SHELTER_R48_PINK_FLASH_BEAM_SPREAD_SHRINK;
                        work->step   += SHELTER_R48_PINK_FLASH_BEAM_YAW_STEP;
                    } else {
                        task->state = SHELTER_R48_PINK_FLASH_STATE_SHRINKING;
                    }
                }
                return;
            case SHELTER_R48_PINK_FLASH_STATE_SHRINKING:
                if (work->period > 0) {
                    _shelterR48DrawGlowBeam(coord, work->period, work->step, SHELTER_R48_PINK_FLASH_BEAM_COLOR);
                    _shelterR48DrawGlowBeam(coord, work->period, (s16)-work->step, SHELTER_R48_PINK_FLASH_BEAM_COLOR);
                    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                        work->period -= SHELTER_R48_PINK_FLASH_BEAM_FINAL_SHRINK;
                    }
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(work, task);
#undef SHELTER_R48_DRAW_PINK_CHARGE
}

/// Initializes a Gouraud quad with one coloured centre vertex and a black rim.
///
/// Borrows one writable `POLY_G4` packet. Sets the eight-word packet length,
/// opaque Gouraud-quad command and all four RGB triples; vertex 2 receives the
/// supplied colour bytes, and vertices 0, 1 and 3 are black. Wider caller values
/// narrow to their low bytes on entry.
///
/// The caller supplies geometry with vertex 2 at the centre and the others on
/// the rim, then sets blend mode and ordering-table linkage. Coordinates and
/// the tag's link bits are preserved; no primitive is allocated or queued here.
static inline void _shelterR48InitGlowWedge(POLY_G4* quad, u8 red, u8 green, u8 blue)
{
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, red, green, blue);
    setRGB3(quad, 0, 0, 0);
}

/// Draws a two-layer glow capsule from an effect origin to a yawed local tip.
///
/// Borrows composed `coord`; the tip starts at (0,2048,5120), rotates by the
/// signed low halfword of `yaw` (4096 units per turn), then by workm. Both
/// endpoints narrow to signed halfword positions before GsWSMATRIX projection.
/// `radiusScale` sets the two pixel radii as scale * 64/depth and * 128/depth;
/// both accepted depths (camera Z/4) must be nonzero. `packedColor` is 0xRGB,
/// with each nibble scaled by 16 and odd frames adding 16 to each colour byte.
/// Negative GTE flags reject either endpoint. Queues twelve additive quads
/// plus blend commands; joining quads sort at the tip depth. Requires scratch
/// and primitive space, overwrites GTE state, and retains no pointer.
static void _shelterR48DrawGlowBeam(const GfxCoord* coord, s16 radiusScale, s32 yaw, s32 packedColor)
{
    enum {
        SHELTER_R48_GLOW_TRIG_SHIFT         = 12,
        SHELTER_R48_GLOW_HALF_TURN          = 2048,
        SHELTER_R48_GLOW_QUARTER_TURN       = 1024,
        SHELTER_R48_GLOW_EIGHTH_TURN        = 512,
        SHELTER_R48_BEAM_LOCAL_TIP_Y        = 2048,
        SHELTER_R48_BEAM_LOCAL_TIP_Z        = 5120,
        SHELTER_R48_BEAM_RADIUS_SCALE_SHIFT = 6
    };
    MATRIX           yawMatrix;
    RoomBeamScratch* scratch;
    POLY_G4*         quad;
    s32              radiusPass;
    u8               red;
    u8               green;
    u8               blue;
    s32              sweepLimit;
    s32              sweepStart;
    s32              radiusNumerator;
    s32              sweepAngle;
    s32              nextSweepAngle;
    s32              sideAngle;
    s32              flicker;
    s32              redBase;
    s32              greenBase;

    // Form both world endpoints before deriving their projected capsule radii.
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    scratch->point1.vy = SHELTER_R48_BEAM_LOCAL_TIP_Y;
    scratch->point1.vx = 0;
    scratch->point1.vz = SHELTER_R48_BEAM_LOCAL_TIP_Z;
    gfxRotMatrixY(&yawMatrix, (s16)yaw, 1);
    sweepAngle = packedColor;
    gte_SetRotMatrix(&yawMatrix);
    gte_ldv0(&scratch->point1);
    gte_rtv0();
    gte_stsv(&scratch->point1);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->point1);
    gte_rtv0();
    gte_stsv(&scratch->point1);
    scratch->point0.vx  = coord->workm.t[0];
    scratch->point0.vy  = coord->workm.t[1];
    scratch->point0.vz  = coord->workm.t[2];
    scratch->point1.vx += scratch->point0.vx;
    scratch->point1.vy += scratch->point0.vy;
    scratch->point1.vz += scratch->point0.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->point0);
    gte_rtps();
    gte_stsxy(&scratch->pair.sx0);
    gte_stflg(&scratch->pair.flag);
    if (scratch->pair.flag >= 0) {
        gte_stszotz(&scratch->pair.otz0);
        gte_ldv0(&scratch->point1);
        gte_rtps();
        gte_stsxy(&scratch->pair.sx1);
        gte_stflg(&scratch->pair.flag);
        gte_stszotz(&scratch->pair.otz1);
        if (scratch->pair.flag >= 0) {
            redBase   = (((u16)sweepAngle >> 8) & 0xF) << 4;
            greenBase = (((u16)sweepAngle >> 4) & 0xF) << 4;
            flicker   = ((u8)gDisplayState.animFrame & 1) << 4;
            red       = redBase + flicker;
            green     = greenBase + flicker;
            // Preserve the packed-colour read before reusing its register as an angle.
            sweepAngle = 0;
            blue       = ((packedColor & 0xF) << 4) + flicker;
            for (radiusPass = 1; radiusPass < 3; radiusPass++) {
                radiusNumerator       = radiusScale * (radiusPass << SHELTER_R48_BEAM_RADIUS_SCALE_SHIFT);
                scratch->pair.radius0 = radiusNumerator / scratch->pair.otz0;
                scratch->pair.radius1 = radiusNumerator / scratch->pair.otz1;
                sweepAngle            = (s16)ratan2((s16)scratch->pair.sy1 - (s16)scratch->pair.sy0, (s16)scratch->pair.sx0 - (s16)scratch->pair.sx1);
                if (sweepAngle < sweepAngle + SHELTER_R48_GLOW_HALF_TURN) {
                    sweepStart = sweepAngle;
                    sweepLimit = sweepAngle + SHELTER_R48_GLOW_HALF_TURN;
                    do {
                        quad           = gGpuPrimCursor;
                        gGpuPrimCursor = quad + 1;
                        _shelterR48InitGlowWedge(quad, red, green, blue);
                        quad->x0 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + SHELTER_R48_GLOW_HALF_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y0 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + SHELTER_R48_GLOW_HALF_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + SHELTER_R48_GLOW_HALF_TURN + SHELTER_R48_GLOW_EIGHTH_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + SHELTER_R48_GLOW_HALF_TURN + SHELTER_R48_GLOW_EIGHTH_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->x2 = scratch->pair.sx1;
                        quad->y2 = scratch->pair.sy1;
                        quad->x3 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + SHELTER_R48_GLOW_HALF_TURN + SHELTER_R48_GLOW_QUARTER_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y3 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + SHELTER_R48_GLOW_HALF_TURN + SHELTER_R48_GLOW_QUARTER_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->pair.otz1),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);

                        quad           = gGpuPrimCursor;
                        gGpuPrimCursor = quad + 1;
                        _shelterR48InitGlowWedge(quad, red, green, blue);
                        quad->x0       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y0       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->x1       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle + SHELTER_R48_GLOW_EIGHTH_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y1       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle + SHELTER_R48_GLOW_EIGHTH_TURN)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        nextSweepAngle = sweepAngle + SHELTER_R48_GLOW_QUARTER_TURN;
                        quad->x2       = scratch->pair.sx0;
                        quad->y2       = scratch->pair.sy0;
                        quad->x3       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(nextSweepAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y3       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(nextSweepAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->pair.otz0),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz0);

                        quad           = gGpuPrimCursor;
                        sideAngle      = sweepStart + (sweepAngle - sweepStart) * 2;
                        gGpuPrimCursor = quad + 1;
                        setPolyG4(quad);
                        setRGB0(quad, 0, 0, 0);
                        setRGB1(quad, 0, 0, 0);
                        setRGB2(quad, red, green, blue);
                        setRGB3(quad, red, green, blue);
                        quad->x0 = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sideAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y0 = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sideAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sideAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sideAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
                        quad->x2 = scratch->pair.sx0;
                        quad->y2 = scratch->pair.sy0;
                        quad->x3 = scratch->pair.sx1;
                        quad->y3 = scratch->pair.sy1;
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->pair.otz1),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);
                        sweepAngle = nextSweepAngle;
                    } while (sweepAngle < sweepLimit);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBeamScratch);
}

/// Draws a tinted disc and four glow blades around a fixed room point.
///
/// Borrows `worldPoint` in world coordinates. After view projection, camera
/// Z/4 is reduced by 192 and clamped to 16 for sizing and sorting. The signed
/// low halfword of `radiusScale` gives outer/inner radii of scale * 64/depth
/// and scale * 8/depth. `packedColor` contains RGB nibbles in bits 8..11,
/// 4..7 and 0..3, scaled by 16; bits 12..15 give the odd-frame flicker shift
/// (0..7 required). Colour bytes wrap before halving. Negative GTE flags
/// reject the point. Queues twenty additive quads plus blend commands, using
/// scratch space and the frame's primitive allocator; retains no pointer.
static void _shelterR48DrawRoomGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        SHELTER_R48_ROOM_GLOW_DEPTH_PULL = 192,
        SHELTER_R48_ROOM_GLOW_MIN_DEPTH  = 16
    };
    enum {
        SHELTER_R48_GLOW_TRIG_SHIFT         = 12,
        SHELTER_R48_GLOW_FULL_TURN          = 4096,
        SHELTER_R48_GLOW_HALF_TURN          = 2048,
        SHELTER_R48_GLOW_QUARTER_TURN       = 1024,
        SHELTER_R48_GLOW_EIGHTH_TURN        = 512,
        SHELTER_R48_GLOW_SIXTEENTH_TURN     = 256,
        SHELTER_R48_GLOW_RADIUS_SCALE       = 64,
        SHELTER_R48_GLOW_INNER_RADIUS_SCALE = 8
    };
    GlowCentreRadiiScratch* scratch;
    POLY_G4*                quad;
    s32                     angle;
    s32                     halfStepAngle;
    s32                     nextAngle;
    s32                     previousShoulderAngle;
    s32                     nextShoulderAngle;
    s32                     oppositeAngle;
    s32                     frameCounter;
    s32                     shiftedColor;
    s32                     flicker;
    s32                     red;
    s32                     green;
    s32                     blue;
    s32                     outerRadius;
    s32                     innerRadius;
    s32                     halfRed;
    s32                     halfGreen;
    s32                     halfBlue;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->sx);
    gte_stflg(&scratch->flag);
    if (scratch->flag >= 0) {
        gte_stszotz(&scratch->otz);
        scratch->otz -= SHELTER_R48_ROOM_GLOW_DEPTH_PULL;
        if (scratch->otz < SHELTER_R48_ROOM_GLOW_MIN_DEPTH) {
            scratch->otz = SHELTER_R48_ROOM_GLOW_MIN_DEPTH;
        }
        radiusScale        <<= 16;
        radiusScale        >>= 16;
        outerRadius          = (radiusScale * SHELTER_R48_GLOW_RADIUS_SCALE) / scratch->otz;
        frameCounter         = gDisplayState.animFrame;
        scratch->outerRadius = outerRadius;
        innerRadius          = (radiusScale * SHELTER_R48_GLOW_INNER_RADIUS_SCALE) / scratch->otz;
        angle                = 0;
        shiftedColor         = packedColor << 16;
        flicker              = (frameCounter & 1) << (shiftedColor >> 28);
        red                  = flicker + ((shiftedColor >> 20) & 0xF0);
        green                = flicker + ((shiftedColor >> 16) & 0xF0);
        blue                 = flicker + ((packedColor & 0xF) << 4);
        scratch->innerRadius = innerRadius;
        // Layer eight full-radius wedges with bright half-radius copies.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            halfRed = (u8)red >> 1;
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            halfGreen = (u8)green >> 1;
            halfBlue  = (u8)blue >> 1;
            setRGB2(quad, halfRed, halfGreen, halfBlue);
            setRGB3(quad, 0, 0, 0);
            quad->x0      = scratch->sx + ((scratch->outerRadius * rsin(angle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + SHELTER_R48_GLOW_SIXTEENTH_TURN;
            quad->y0      = scratch->sy + ((scratch->outerRadius * rcos(angle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->x1      = scratch->sx + ((scratch->outerRadius * rsin(halfStepAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->y1      = scratch->sy + ((scratch->outerRadius * rcos(halfStepAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            nextAngle     = angle + SHELTER_R48_GLOW_EIGHTH_TURN;
            quad->x2      = scratch->sx;
            quad->y2      = scratch->sy;
            quad->x3      = scratch->sx + ((scratch->outerRadius * rsin(nextAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->y3      = scratch->sy + ((scratch->outerRadius * rcos(nextAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _shelterR48InitGlowWedge(quad, red, green, blue);
            quad->x0 = scratch->sx + ((scratch->outerRadius * rsin(angle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->y0 = scratch->sy + ((scratch->outerRadius * rcos(angle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->x1 = scratch->sx + ((scratch->outerRadius * rsin(halfStepAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->y1 = scratch->sy + ((scratch->outerRadius * rcos(halfStepAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->x2 = scratch->sx;
            quad->y2 = scratch->sy;
            quad->x3 = scratch->sx + ((scratch->outerRadius * rsin(nextAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->y3 = scratch->sy + ((scratch->outerRadius * rcos(nextAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        } while (angle < SHELTER_R48_GLOW_FULL_TURN);

        // Add four alternating blades with black rims.
        angle = SHELTER_R48_GLOW_EIGHTH_TURN;
        red   = (u8)halfRed;
        green = (u8)halfGreen;
        blue  = (u8)halfBlue;
        do {
            previousShoulderAngle = angle - SHELTER_R48_GLOW_QUARTER_TURN;
            quad                  = gGpuPrimCursor;
            gGpuPrimCursor        = quad + 1;
            _shelterR48InitGlowWedge(quad, red, green, blue);
            quad->x0          = scratch->sx + ((scratch->innerRadius * rsin(previousShoulderAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->y0          = scratch->sy + ((scratch->innerRadius * rcos(previousShoulderAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->x1          = scratch->sx + ((scratch->outerRadius * rsin(angle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->y1          = scratch->sy + ((scratch->outerRadius * rcos(angle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            nextShoulderAngle = angle + SHELTER_R48_GLOW_QUARTER_TURN;
            quad->x2          = scratch->sx;
            quad->y2          = scratch->sy;
            quad->x3          = scratch->sx + ((scratch->innerRadius * rsin(nextShoulderAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            quad->y3          = scratch->sy + ((scratch->innerRadius * rcos(nextShoulderAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _shelterR48InitGlowWedge(quad, red, green, blue);
            quad->x0      = scratch->sx + ((scratch->innerRadius * rsin(angle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->y0      = scratch->sy + ((scratch->innerRadius * rcos(angle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->x1      = scratch->sx + ((scratch->outerRadius * rsin(nextShoulderAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT - 1));
            quad->y1      = scratch->sy + ((scratch->outerRadius * rcos(nextShoulderAngle)) >> (SHELTER_R48_GLOW_TRIG_SHIFT - 1));
            oppositeAngle = angle + SHELTER_R48_GLOW_HALF_TURN;
            quad->x2      = scratch->sx;
            quad->y2      = scratch->sy;
            quad->x3      = scratch->sx + ((scratch->innerRadius * rsin(oppositeAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            quad->y3      = scratch->sy + ((scratch->innerRadius * rcos(oppositeAngle)) >> SHELTER_R48_GLOW_TRIG_SHIFT);
            angle         = oppositeAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        } while (angle < SHELTER_R48_GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
}
