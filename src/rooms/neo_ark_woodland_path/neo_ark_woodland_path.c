#include "rooms/neo_ark_woodland_path.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "common.h"

#include "neo_ark_woodland_path_private.h"

#include "actors/actor_401300.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/effects/water_effects.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

static s32 _neoArkWoodlandPathRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _neoArkWoodlandPathResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkWoodlandPathIgnoreRoomCommand(Task* task, s32 messageId, s32 commandIndex, s32 unused);
static s32 _neoArkWoodlandPathForwardRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static s32 _neoArkWoodlandPathForwardActorEvent(Task* task, s32 messageId, s32 hp, s32 unused);

enum { NEO_ARK_WOODLAND_PATH_ROAMER_ABSENT = -1 };

TaskDesc gNeoArkWoodlandPathWaterRefractionTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterRefractionTask, { .value = 0 } };

/// Retained no-body, priority-192 recipe for the framebuffer distortion task.
///
/// No spawn site selects this recipe; it is distinct from the active refraction
/// descriptor. The callback's room/view clipping determines whether it draws.
static TaskDesc _gNeoArkWoodlandPathWaterDistortBandTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterDistortBandTask, { .value = 0 } };

/// Synchronous message handlers installed on the woodland room receiver.
///
/// Resolves transitions, refuses key items, ignores CAP room commands, and
/// forwards room actions and enemy HP reports to pool A. The final END entry
/// makes unsupported IDs return zero. The table and callbacks live in the room
/// overlay and must outlive every dispatch to the receiver.
static TaskMessageEntry _gNeoArkWoodlandPathRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkWoodlandPathResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkWoodlandPathRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkWoodlandPathForwardRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkWoodlandPathIgnoreRoomCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _neoArkWoodlandPathForwardActorEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Borrowed pool-A controller receiving the room's spawn requests and HP reports.
///
/// NULL on overlay load or a failed spawn; room setup replaces it. Retirement
/// does not clear this handle, so forwarding requires the controller to remain
/// live. The task system owns and releases the task allocation.
static Task* _gNeoArkWoodlandPathRoamerPoolATask = NULL;

SVECTOR gNeoArkWoodlandPathPlayerWaterPartHistory[2] = { 0 };

u8* gNeoArkWoodlandPathViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkWoodlandPathViewCounts[1] = { ARRAY_SIZE(gNeoArkWoodlandPathViewCameras) };

enum {
    NEO_ARK_WOODLAND_PATH_WARP1_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 2),
    NEO_ARK_WOODLAND_PATH_WARP1_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 1),
    NEO_ARK_WOODLAND_PATH_WARP2_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 4),
    NEO_ARK_WOODLAND_PATH_WARP2_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 3),
};

DirectionWarpEntry gNeoArkWoodlandPathWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3895, 0, 9510 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, -3895, 0, 9510 }, { 0, 0, 0, 0 }, NEO_ARK_WOODLAND_PATH_WARP1_ARRIVAL_SOUND, NEO_ARK_WOODLAND_PATH_WARP1_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -5000, 0, -6680 }, { 0, 0, 0, 0 }, { { .word = 0 }, -5000, 0, -6680 }, { 0, 0, 0, 0 }, NEO_ARK_WOODLAND_PATH_WARP2_ARRIVAL_SOUND, NEO_ARK_WOODLAND_PATH_WARP2_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space face normals, with ONE representing unit length.
static SVECTOR _gNeoArkWoodlandPathCollisionGridNormals[17] = {
#include "assets/neo_ark_woodland_path_collision_0479C_normals.inc"
};

/// Collision vertex pool in signed room-coordinate units.
static SVECTOR _gNeoArkWoodlandPathCollisionGridVerts[75] = {
#include "assets/neo_ark_woodland_path_collision_0479C_verts.inc"
};

/// Indexed collision quads carrying room-local surface classes 0..4.
static WorldCollisionGridFace _gNeoArkWoodlandPathCollisionGridFaces[35] = {
#include "assets/neo_ark_woodland_path_collision_0479C_faces.inc"
};

/// Packed cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gNeoArkWoodlandPathCollisionGridCells[190] = {
#include "assets/neo_ark_woodland_path_collision_0479C_cells.inc"
};

/// Face-list address at a halfword offset in the woodland collision grid's pool.
///
/// Captures the 190-element s16 pool. `halfwordOffset` is evaluated once and
/// must select a list start in 0..189, not a cell-directory slot or byte offset.
/// Each list ends with WORLD_COLLISION_GRID_CELL_END. The generated table include
/// selects this binding through the grid's `cell_face_list_macro` configuration.
#define NEO_ARK_WOODLAND_PATH_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkWoodlandPathCollisionGridCells[(halfwordOffset)])
/// X-major directory of 4-by-5 collision cells; NULL marks an empty cell.
static s16* _gNeoArkWoodlandPathCollisionGridTable[20] = {
#include "assets/neo_ark_woodland_path_collision_0479C_table.inc"
};
#undef NEO_ARK_WOODLAND_PATH_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gNeoArkWoodlandPathCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkWoodlandPathCollisionGridNormals,
    .vertices    = _gNeoArkWoodlandPathCollisionGridVerts,
    .faces       = _gNeoArkWoodlandPathCollisionGridFaces,
    .cellFaceIds = _gNeoArkWoodlandPathCollisionGridTable,
    .xBias       = 7600,
    .zBias       = 7000,
    .cellCountX  = 4,
    .cellCountZ  = 5,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkWoodlandPathCollisionGridFaces),
};

ViewCamera gNeoArkWoodlandPathViewCameras[10] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 1610, 0x7530, -1510 } }, 329 },
    { { { { 4020, 0, 782 }, { -251, 3878, 1292 }, { -740, -1316, 3807 } }, { 3562, 291, -6263 } }, 230 },
    { { { { 3934, 0, 1140 }, { -229, 4012, 790 }, { -1116, -823, 3853 } }, { 3377, 524, -3609 } }, 230 },
    { { { { -745, 0, 4027 }, { -195, 4091, -36 }, { -4022, -199, -744 } }, { -2790, 1033, -5416 } }, 230 },
    { { { { -745, 0, 4027 }, { 45, 4095, 8 }, { -4027, 46, -745 } }, { -6861, 1215, -5486 } }, 230 },
    { { { { 4068, 0, -474 }, { -7, 4095, -66 }, { 474, 67, 4067 } }, { -4698, 771, 1395 } }, 230 },
    { { { { -4042, 0, -661 }, { -55, 4081, 341 }, { 659, 345, -4027 } }, { -4740, 927, -2874 } }, 230 },
    { { { { -985, 0, 3975 }, { 65, 4095, 16 }, { -3975, 67, -985 } }, { -7213, 1156, 3165 } }, 230 },
    { { { { -1144, 0, 3932 }, { -191, 4091, -55 }, { -3928, -199, -1142 } }, { -1787, 897, 3010 } }, 230 },
    { { { { -4024, 0, 764 }, { -94, 4064, -496 }, { -758, -505, -3993 } }, { 4210, 517, 1415 } }, 230 },
};

/// View 1 keeps the decoded background and selects no source sprites.
static SpriteBatch _gNeoArkWoodlandPathView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths over the decoded background in view 2.
static SpriteSource _gNeoArkWoodlandPathView2SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 72, 837, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -32, 88, 954, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 112, 575, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 232 } }, -160, -120, 820, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 224 } }, -96, -112, 858, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 200 } }, -80, -96, 880, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, -64, -88, 948, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -64, 16, 903, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -64, 32, 913, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -48, -80, 987, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -40, -56, 987, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, -32, 987, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -48, 56, 926, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -40, 80, 963, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 80, -120, 250, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 104, -120, 250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 64 } }, 120, -120, 233, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 32 } }, 128, -56, 222, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 32 } }, 136, -24, 215, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 144, 8, 211, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 72, 56, 750, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 88, 48, 717, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 56 } }, 104, 48, 610, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 64 } }, 136, 48, 514, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -128, 112, 800, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
};

/// View 2 draws one complete source range while retaining the decoded background.
static SpriteBatch _gNeoArkWoodlandPathView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkWoodlandPathView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths over the decoded background in view 3.
static SpriteSource _gNeoArkWoodlandPathView3SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 16, -88, 944, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, -72, 983, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 48, 814, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 16, -120, 918, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 40, -120, 933, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 72 } }, 72, -56, 855, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 64, -120, 914, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 80, 16, 796, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 112 } }, 96, -120, 717, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 112 } }, 96, 0, 712, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 232 } }, 120, -120, 648, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

/// View 3 draws one complete source range while retaining the decoded background.
static SpriteBatch _gNeoArkWoodlandPathView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkWoodlandPathView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths over the decoded background in view 4.
static SpriteSource _gNeoArkWoodlandPathView4SpriteSources[] = {
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 96, 1148, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 64 } }, 40, -120, 1075, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, 56, -120, 0x61A7, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 216 } }, 72, -120, 1076, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 64, -40, 1101, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 240 } }, 96, -120, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 192 } }, -40, -120, 1362, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 184 } }, -16, -120, 1412, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 88 } }, -8, -120, 1406, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 64 } }, 16, -120, 1425, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 128 } }, -72, -120, 1234, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 224 } }, -88, -120, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 200 } }, -104, -120, 975, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -136, -16, 947, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 104 } }, -136, -120, 947, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 48 } }, -152, 24, 947, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, 56, 947, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 72 } }, -160, -120, 947, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 224 } }, -136, -120, 825, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 40 } }, -120, 72, 825, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 24 } }, -160, -120, 825, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, -88, 576, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 104 } }, -144, -8, 825, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, 0, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -152, 72, 750, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, -160, 56, 700, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 80 } }, -144, 40, 700, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, 64, 700, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 96, 700, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 24 } }, -104, 56, 829, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 24 } }, -128, 56, 829, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -152, 64, 829, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 24 } }, -144, 56, 829, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, 56, 829, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 40, 858, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -80, 40, 853, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 24 } }, -72, 32, 925, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -56, 32, 925, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -48, 40, 925, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 200 } }, -160, -120, 917, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 192 } }, -144, -120, 940, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 184 } }, -128, -120, 965, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 160 } }, -112, -104, 996, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 104 } }, -88, -48, 1025, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 88 } }, -72, -40, 923, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -16, -64, 1225, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -80, -24, 1225, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -56, 8, 1225, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 56 } }, -24, -96, 1225, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 128 } }, -48, -120, 1225, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 128 } }, -72, -120, 1225, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 96 } }, -96, -120, 1225, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -96, -16, 1050, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 120 } }, -112, -120, 1225, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 8 } }, -16, 48, 975, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -64, 56, 975, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 24 } }, -24, 48, 975, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 56 } }, -32, -120, 975, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 24 } }, -24, -120, 975, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -56, 48, 975, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -48, 48, 975, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 56 } }, -40, 16, 975, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 80 } }, -40, -64, 975, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 80 } }, -32, -64, 975, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 56 } }, -32, 16, 975, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
};

/// View 4 partitions every source into nine ranges over the decoded background.
static SpriteBatch _gNeoArkWoodlandPathView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 7, 0 } },
    { 6, 12, 0, 0, { 2, 0 } },
    { 18, 7, 0, 0, { 5, 0 } },
    { 25, 4, 0, 0, { 1, 0 } },
    { 29, 5, 0, 0, { 6, 0 } },
    { 34, 5, 0, 0, { 4, 0 } },
    { 39, 6, 0, 0, { 8, 0 } },
    { 45, 9, 0, 0, { 3, 0 } },
    { 54, ARRAY_SIZE(_gNeoArkWoodlandPathView4SpriteSources) - 54, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths over the decoded background in view 5.
static SpriteSource _gNeoArkWoodlandPathView5SpriteSources[] = {
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 72 } }, 16, -120, 0x61A7, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 120 } }, 40, -120, 1321, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 176 } }, 56, -120, 1354, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 88, 208 } }, 72, -120, 1300, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 8 } }, 112, 88, 1300, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 16, 1000, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 72, 1000, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 88 } }, 128, 24, 1000, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 64, 96 } }, 64, 16, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 64, 1000, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 72, 750, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 64, 750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 40, 875, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, 32, 875, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 96, 16, 875, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 88 } }, 128, 24, 750, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 88, 64, 750, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 40 } }, 64, 64, 750, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 8 } }, -48, 112, 305, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 32 } }, 24, 88, 125, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 32 } }, 64, 88, 125, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, 96, 125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 16 } }, 112, 104, 125, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 128, 88, 125, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 40 } }, 144, 80, 125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, 104, 302, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 104, 125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 104, 125, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 8, 16 } }, 16, 104, 125, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 96 } }, 96, -104, 1050, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 32, 32 } }, 96, -8, 1001, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 104, 24, 1050, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 16, -72, 1925, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -48, 40, 1444, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 8 } }, -104, 72, 1353, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 24, 72 } }, -144, -120, 1399, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -120, -120, 1395, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 40, 192 } }, -104, -120, 1383, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 184 } }, -64, -120, 1444, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -48, -120, 1510, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -32, -120, 1371, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 80 } }, -16, -120, 1947, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 64 } }, 0, -120, 1925, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 24 } }, 8, -88, 1925, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 32 } }, -112, 48, 1383, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -56, -80, 1694, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -48, -40, 1694, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 136 } }, -40, -88, 1694, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 8, 32 } }, -24, 16, 1889, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 16 } }, -16, 24, 1889, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 8 } }, -80, -48, 1275, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 8 } }, -128, -24, 1250, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 56 } }, -120, 8, 1250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 104 } }, -120, -96, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 80 } }, -112, -88, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 32 } }, -104, -56, 1294, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -96, -72, 1275, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 40 } }, -88, -80, 1275, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -144, -64, 1250, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 8 } }, -152, -32, 1250, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 8 } }, -136, -32, 1250, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 8 } }, -64, 24, 1183, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 32 } }, -160, 40, 228, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 8, 24 } }, -144, 48, 1125, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 24, 32 } }, -136, 40, 1125, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 40, 32 } }, -112, 32, 1125, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 24 } }, -72, 32, 1125, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 16 } }, -56, 40, 1125, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 8 } }, -104, 64, 1125, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 8 } }, -96, 24, 1125, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 8 } }, -80, 24, 1125, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -160, -64, 1525, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -144, -64, 1525, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -128, -64, 1550, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -112, -64, 1575, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -96, -64, 1632, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -80, -64, 1755, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 112 } }, -64, -64, 1942, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
};

/// View 5 partitions every source into ten ranges over the decoded background.
static SpriteBatch _gNeoArkWoodlandPathView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 5, 0, 0, { 1, 0 } },
    { 10, 8, 0, 0, { 8, 0 } },
    { 18, 11, 0, 0, { 0, 0 } },
    { 29, 3, 0, 0, { 5, 0 } },
    { 32, 13, 0, 0, { 2, 0 } },
    { 45, 5, 0, 0, { 9, 0 } },
    { 50, 11, 0, 0, { 4, 0 } },
    { 61, 10, 0, 0, { 7, 0 } },
    { 71, ARRAY_SIZE(_gNeoArkWoodlandPathView5SpriteSources) - 71, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths over the decoded background in view 6.
static SpriteSource _gNeoArkWoodlandPathView6SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -88, -120, 1063, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, 24, 995, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 176 } }, -160, -120, 1058, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 168 } }, -120, -120, 1070, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -88, -16, 1052, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -80, 0, 1061, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, 0, 1053, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 24, 8, 1219, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 48 } }, 40, -8, 1062, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 64, -16, 1211, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 88, -8, 1056, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 112, -8, 1133, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 168 } }, 72, -120, 1224, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 176 } }, 88, -120, 1054, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 168 } }, 112, -120, 1218, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 160 } }, 144, -120, 1408, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 184 } }, 56, -120, 1316, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 168 } }, 40, -120, 1498, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, 16, -16, 1559, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 0, -8, 1798, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -16, -8, 1885, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 24, -120, 1535, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 16, -104, 1550, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -104, 1729, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -8, 1257, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 160 } }, 80, -120, 1304, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 16, -104, 1546, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 120 } }, 32, -120, 1448, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 120 } }, 56, -120, 1448, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 112 } }, 72, -120, 1448, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -120, 1324, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -80, 1302, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 32 } }, 120, -40, 1307, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, -144, 64, 537, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 56, 8 } }, -160, 72, 525, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 56, 8 } }, -160, 80, 500, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 48, 8 } }, -160, 88, 475, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, -152, 96, 450, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, 112, 48, 650, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 48, 8 } }, 104, 56, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 56, 8 } }, 104, 64, 600, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 24, 8 } }, 136, 72, 600, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 128, 80, 612, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 136, 80, 605, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 144, 80, 605, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 40, 32 } }, -160, 80, 450, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 24, 16 } }, 128, 64, 575, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
};

/// View 6 partitions every source into five ranges over the decoded background.
static SpriteBatch _gNeoArkWoodlandPathView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { 12, 12, 0, 0, { 3, 0 } },
    { 24, 9, 0, 0, { 2, 0 } },
    { 33, 9, 0, 0, { 4, 0 } },
    { 42, ARRAY_SIZE(_gNeoArkWoodlandPathView6SpriteSources) - 42, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths over the decoded background in view 7.
static SpriteSource _gNeoArkWoodlandPathView7SpriteSources[] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 40, -120, 1100, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -96, -40, 882, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -40, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -16, 0, 1946, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 160 } }, -160, -120, 808, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 80 } }, -152, -120, 981, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 56 } }, -152, -16, 928, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 160 } }, -120, -120, 856, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 80 } }, -104, -120, 823, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -64, -40, 919, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -104, -16, 1175, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 80 } }, -88, -40, 988, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -64, -16, 1099, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -48, -16, 1466, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, -16, 1913, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 72 } }, -40, -120, 1027, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 24, 32 } }, -24, -120, 1055, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 32 } }, -24, -88, 1053, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 96 } }, 120, -120, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 56 } }, 136, -16, 907, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 64 } }, 120, -24, 750, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 80 } }, 72, -120, 1100, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 80 } }, 96, -40, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 48 } }, 112, -8, 750, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, 8, 1100, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 72, 16, 1100, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 64 } }, 48, -120, 1100, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 144 } }, -160, -120, 1234, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 64, 136 } }, -112, -120, 1313, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, -48, -104, 1250, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -104, 16, 1279, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -32, 536, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 88 } }, 96, -120, 525, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 48, 48 } }, 48, -120, 516, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 255), { .fields = { 16, 48 } }, 120, -32, 533, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 48, 32 } }, 112, 16, 1042, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -160, 96, 475, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 24, 8 } }, -128, 48, 731, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 40, 8 } }, -144, 56, 646, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 40, 8 } }, -152, 64, 622, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 72, 589, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 16, 8 } }, -160, 80, 550, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -160, 88, 500, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, 112, 56, 622, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 832, 0), getClut(0, 256), { .fields = { 48, 8 } }, 112, 64, 572, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 112, 72, 575, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, 120, 80, 525, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 128, 88, 500, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, 136, 96, 475, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -144, 88, 588, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -136, 88, 583, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -128, 88, 593, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 32, 24 } }, -144, 64, 564, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 128, 72, 565, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 128, 96, 575, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
};

/// View 7 partitions every source into five ranges over the decoded background.
static SpriteBatch _gNeoArkWoodlandPathView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { 27, 4, 0, 0, { 3, 0 } },
    { 31, 5, 0, 0, { 2, 0 } },
    { 36, 13, 0, 0, { 4, 0 } },
    { 49, ARRAY_SIZE(_gNeoArkWoodlandPathView7SpriteSources) - 49, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8 texture rectangles in centred pixels, with unscaled sorting depths.
///
/// Four batch ranges cover all 32 elements; there is no source terminator.
static SpriteSource _gNeoArkWoodlandPathView8SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 48 } }, -160, 72, 250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -128, 96, 250, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 96, 112 } }, 64, -120, 1749, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 40, -104, 1664, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 16 } }, 80, -8, 1741, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 88, 40 } }, 72, 8, 1539, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -32, 1125, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, 0, 1514, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 0, 1182, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, 112, 56, 1125, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 128, 72, 1125, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 40 } }, 72, -120, 1125, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -120, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, 80, -80, 1125, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 120, -80, 1125, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, 80, 16, 1265, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 48 } }, 104, 8, 1265, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 16, -104, 2750, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, -64, 2750, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, -48, 2750, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 160 } }, -40, -120, 1975, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 160 } }, -104, -120, 1925, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 160 } }, -72, -120, 1950, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 160 } }, -8, -120, 2000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 160 } }, 0, -120, 2250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 144 } }, 8, -112, 2750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 16, -80, 2750, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 24, -80, 2750, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 16, 0, 2750, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 160 } }, -136, -120, 1900, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, -160, -120, 1875, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 72 } }, -160, -32, 1875, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
};

/// View 8 retains its decoded background and draws four contiguous source ranges.
///
/// Ranges [0,2), [2,6), [6,17) and [17,32) are visible and included in cached
/// packets. END terminates the list; the two serialized trailing bytes are
/// retained without assigning an unproven meaning.
static SpriteBatch _gNeoArkWoodlandPathView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 4, 0, 0, { 0, 0 } },
    { 6, 11, 0, 0, { 2, 0 } },
    { 17, ARRAY_SIZE(_gNeoArkWoodlandPathView8SpriteSources) - 17, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9 texture rectangles in centred pixels, with unscaled sorting depths.
///
/// Four batch ranges cover all 26 elements; there is no source terminator.
static SpriteSource _gNeoArkWoodlandPathView9SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 40 } }, 112, 80, 250, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -32, 48, 1425, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 192 } }, -160, -120, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 192 } }, -112, -120, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 184 } }, -64, -120, 1250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 104 } }, -32, -120, 1425, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, -8, -120, 1425, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, -32, 8, 1438, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 0, -96, 1500, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 40, 694, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 56, 692, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 120 } }, -88, -120, 1125, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 128 } }, -96, -120, 1000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, -88, 0, 1004, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -88, 48, 1005, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, 0, 1125, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -48, 32, 1125, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -56, 48, 1125, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, 64, 1009, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 112 } }, -40, -120, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, -96, 1385, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -8, -88, 1500, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -24, -56, 1375, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -160, -120, 560, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 64 } }, -128, -120, 658, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, -120, 941, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
};

/// View 9 retains its decoded background and draws four contiguous source ranges.
///
/// Ranges [0,1), [1,8), [8,23) and [23,26) are visible and included in cached
/// packets. END terminates the list; serialized trailing bytes are retained.
static SpriteBatch _gNeoArkWoodlandPathView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 7, 0, 0, { 0, 0 } },
    { 8, 15, 0, 0, { 2, 0 } },
    { 23, ARRAY_SIZE(_gNeoArkWoodlandPathView9SpriteSources) - 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10 texture rectangles in centred pixels, with unscaled sorting depths.
///
/// Three batch ranges cover all 14 elements; there is no source terminator.
static SpriteSource _gNeoArkWoodlandPathView10SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 80, 104, 100, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 104, 96, 125, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 120, 88, 150, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 136, -16, 150, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 104, 8, 150, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 128, 32, 150, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, 112, 48, 150, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 120, 72, 150, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 128, 96, 150, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -32, -88, 1425, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 128, 40 } }, -160, -120, 1008, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 104, 56 } }, -160, -80, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 88, 48 } }, -160, -24, 1125, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 112, 40 } }, -160, 24, 1113, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
};

/// View 10 retains its decoded background and draws three contiguous source ranges.
///
/// Ranges [0,3), [3,9) and [9,14) are visible and included in cached packets.
/// END terminates the list; serialized trailing bytes are retained.
static SpriteBatch _gNeoArkWoodlandPathView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 6, 0, 0, { 2, 0 } },
    { 9, ARRAY_SIZE(_gNeoArkWoodlandPathView10SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkWoodlandPathSpriteViews[10] = {
    { { .empty = _gNeoArkWoodlandPathView1SpriteBatches }, _gNeoArkWoodlandPathView1SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView2SpriteSources }, _gNeoArkWoodlandPathView2SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView3SpriteSources }, _gNeoArkWoodlandPathView3SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView4SpriteSources }, _gNeoArkWoodlandPathView4SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView5SpriteSources }, _gNeoArkWoodlandPathView5SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView6SpriteSources }, _gNeoArkWoodlandPathView6SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView7SpriteSources }, _gNeoArkWoodlandPathView7SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView8SpriteSources }, _gNeoArkWoodlandPathView8SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView9SpriteSources }, _gNeoArkWoodlandPathView9SpriteBatches, NULL },
    { { .elements = _gNeoArkWoodlandPathView10SpriteSources }, _gNeoArkWoodlandPathView10SpriteBatches, NULL },
};

/// Six local point lights and one broad full-strength point light for the path.
static WorldCoordPointLight _gNeoArkWoodlandPathPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5800, -1500, -5200 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6300, -1500, -2400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3700, -1500, -5200 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2800, -1500, -5200 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1000, -1500, -5200 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6300, -1500, 1400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -5000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 0x186A0, 0x186A0 },
};

WorldCoordRoomLights gNeoArkWoodlandPathRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkWoodlandPathPointLights),
    .pointLights           = _gNeoArkWoodlandPathPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gNeoArkWoodlandPathViewBoundaryTriggers[16] = {
    { NULL, NULL, NULL, { -3969, -2048, 7647, 0 }, { { 3476, -3712, -1, 0 }, { -3475, -3712, 2, 0 }, { 3476, 3712, -1, 0 }, { -3475, 3712, 2, 0 } }, { 1, 0, 4099, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4001, -1952, 7871, 0 }, { { -3475, -3712, 2, 0 }, { 3476, -3712, -1, 0 }, { -3475, 3712, 2, 0 }, { 3476, 3712, -1, 0 } }, { -2, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4387, -1952, 5758, 0 }, { { -3326, -3712, -1007, 0 }, { 3327, -3712, 1007, 0 }, { -3326, 3712, -1007, 0 }, { 3327, 3712, 1007, 0 } }, { 1187, 0, -3925, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4482, -2015, 5568, 0 }, { { 3306, -3712, 1073, 0 }, { -3306, -3712, -1074, 0 }, { 3306, 3712, 1073, 0 }, { -3306, 3712, -1074, 0 } }, { -1267, 0, 3900, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 320, -2049, 4608, 0 }, { { -2, -3712, -3476, 0 }, { 2, -3712, 3476, 0 }, { -2, 3712, -3476, 0 }, { 2, 3712, 3476, 0 } }, { 4100, 0, -3, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 512, -2080, 4576, 0 }, { { 2, -3712, 3476, 0 }, { -2, -3712, -3476, 0 }, { 2, 3712, 3476, 0 }, { -2, 3712, -3476, 0 } }, { -4101, 0, 2, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4190, -2048, 5055, 0 }, { { -1010, -3712, -3326, 0 }, { 1011, -3712, 3326, 0 }, { -1010, 3712, -3326, 0 }, { 1011, 3712, 3326, 0 } }, { 3923, 0, -1193, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4253, -2048, 4830, 0 }, { { 1011, -3712, 3326, 0 }, { -1010, -3712, -3326, 0 }, { 1011, 3712, 3326, 0 }, { -1010, 3712, -3326, 0 } }, { -3924, 0, 1192, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5087, -2175, 798, 0 }, { { 3476, -3712, 9, 0 }, { -3476, -3712, -8, 0 }, { 3476, 3712, 9, 0 }, { -3476, 3712, -8, 0 } }, { -11, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5024, -2113, 926, 0 }, { { -3476, -3712, -8, 0 }, { 3476, -3712, 9, 0 }, { -3476, 3712, -8, 0 }, { 3476, 3712, 9, 0 } }, { 9, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4607, -2368, -4034, 0 }, { { -1387, -3712, 3187, 0 }, { 1388, -3712, -3187, 0 }, { -1387, 3712, 3187, 0 }, { 1388, 3712, -3187, 0 } }, { -3760, 0, -1637, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4415, -2272, -4034, 0 }, { { 1388, -3712, -3187, 0 }, { -1387, -3712, 3187, 0 }, { 1388, 3712, -3187, 0 }, { -1387, 3712, 3187, 0 } }, { 3759, 0, 1636, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -224, -2336, -4226, 0 }, { { 512, -3712, -3438, 0 }, { -515, -3712, 3436, 0 }, { 512, 3712, -3438, 0 }, { -515, 3712, 3436, 0 } }, { 4054, 0, 605, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -97, -2400, -4290, 0 }, { { -503, -3712, 3438, 0 }, { 501, -3712, -3441, 0 }, { -503, 3712, 3438, 0 }, { 501, 3712, -3441, 0 } }, { -4058, 0, -593, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4961, -2368, -4417, 0 }, { { -2885, -3712, 1939, 0 }, { 2886, -3712, -1938, 0 }, { -2885, 3712, 1939, 0 }, { 2886, 3712, -1938, 0 } }, { -2288, 0, -3405, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4994, -2368, -4546, 0 }, { { 2886, -3712, -1938, 0 }, { -2885, -3712, 1939, 0 }, { 2886, 3712, -1938, 0 }, { -2885, 3712, 1939, 0 } }, { 2286, 0, 3403, 0 }, { 0, 0, 4096, 0 }, 5068, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

// Warp selector bytes pack the departure slot high and the arrival slot low.
enum {
    NEO_ARK_WOODLAND_PATH_FOREST_WARP_SLOTS  = (1 << 4) | 3,
    NEO_ARK_WOODLAND_PATH_PYRAMID_WARP_SLOTS = (2 << 4) | 1,
    NEO_ARK_WOODLAND_PATH_ROAMER_POINT_1     = 1,
    NEO_ARK_WOODLAND_PATH_ROAMER_POINT_2     = 2,
    NEO_ARK_WOODLAND_PATH_ROAMER_POINT_3     = 3,
    NEO_ARK_WOODLAND_PATH_ROAMER_POINT_4     = 4,
    NEO_ARK_WOODLAND_PATH_ROAMER_POINT_5     = 5,
    NEO_ARK_WOODLAND_PATH_ROAMER_POINT_6     = 6,
};

WorldCollisionTrigger gNeoArkWoodlandPathActionTriggers[8] = {
    { NULL, NULL, NULL, { -4000, -48, 9600, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_FOREST_ZONE, NEO_ARK_WOODLAND_PATH_FOREST_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4997, -48, -6720, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_PYRAMID, NEO_ARK_WOODLAND_PATH_PYRAMID_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3968, -64, 8056, 0 }, { { -1024, 0, -168, 0 }, { 1024, 0, -168, 0 }, { -1024, 0, 168, 0 }, { 1024, 0, 168, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_WOODLAND_PATH_ROAMER_POINT_1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3438, -64, 4768, 0 }, { { -2000, 0, -1152, 0 }, { 2032, 0, -1152, 0 }, { -2064, 0, 480, 0 }, { 2032, 0, 1824, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2721, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_WOODLAND_PATH_ROAMER_POINT_2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1696, -64, 4992, 0 }, { { -1184, 0, -1664, 0 }, { 1184, 0, -1664, 0 }, { -1184, 0, 1664, 0 }, { 1184, 0, 1664, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_WOODLAND_PATH_ROAMER_POINT_3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1392, -64, -4064, 0 }, { { -1232, 0, -2016, 0 }, { 1520, 0, -2016, 0 }, { -1808, 0, 2016, 0 }, { 1520, 0, 2016, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 2697, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_WOODLAND_PATH_ROAMER_POINT_4, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3864, -64, -4080, 0 }, { { 984, 0, -1712, 0 }, { 1560, 0, -1712, 0 }, { -4104, 0, 1712, 0 }, { 1560, 0, 1712, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 4434, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_WOODLAND_PATH_ROAMER_POINT_5, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5592, -64, -4720, 0 }, { { -808, 0, -816, 0 }, { 2072, 0, -816, 0 }, { -808, 0, 752, 0 }, { -456, 0, 880, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_WOODLAND_PATH_ROAMER_POINT_6, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_ENTRY_ID    = 13,
    NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_FILE_NUMBER = 13,
};

/// Variant 1's entry-13 Horned Stranger resource, followed by the list terminator.
///
/// Two placements share file 401300 and descriptor 0; loading and spawning
/// borrow the list through the room's area-variant directory while loaded.
static AreaResource _gNeoArkWoodlandPathVariant1ActorResources[] = {
    { NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_ENTRY_ID, NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401300HornedStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's entry-13 Horned Stranger resource, followed by the list terminator.
///
/// Two placements share file 401300 and descriptor 0; storage stays room-owned
/// and is borrowed through the area-variant directory while loaded.
static AreaResource _gNeoArkWoodlandPathVariant2ActorResources[] = {
    { NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_ENTRY_ID, NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401300HornedStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's entry-13 Horned Stranger resource, followed by the list terminator.
///
/// Two placements share file 401300 and descriptor 0; storage stays room-owned
/// and is borrowed through the area-variant directory while loaded.
static AreaResource _gNeoArkWoodlandPathVariant3ActorResources[] = {
    { NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_ENTRY_ID, NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401300HornedStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4's entry-13 Horned Stranger resource, followed by the list terminator.
///
/// Four placements share file 401300 and descriptor 0; storage stays room-owned
/// and is borrowed through the area-variant directory while loaded.
static AreaResource _gNeoArkWoodlandPathVariant4ActorResources[] = {
    { NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_ENTRY_ID, NEO_ARK_WOODLAND_PATH_HORNED_STRANGER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor401300HornedStrangerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gNeoArkWoodlandPathAreaVariants[NEO_ARK_WOODLAND_PATH_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkWoodlandPathVariant1Placements, .resources = _gNeoArkWoodlandPathVariant1ActorResources },
    [2]                    = { .placements = gMapNeoArkWoodlandPathVariant2Placements, .resources = _gNeoArkWoodlandPathVariant2ActorResources },
    [3]                    = { .placements = gMapNeoArkWoodlandPathVariant3Placements, .resources = _gNeoArkWoodlandPathVariant3ActorResources },
    [4]                    = { .placements = gMapNeoArkWoodlandPathVariant4Placements, .resources = _gNeoArkWoodlandPathVariant4ActorResources },
};

WorldCoordRoomAmbientEntry gNeoArkWoodlandPathRoomAmbientByView[11] = {
    { .viewCount = ARRAY_SIZE(gNeoArkWoodlandPathRoomAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 413, 412, 413, 412 } },
    { .color = { 512, 512, 512, 512 } },
    { .color = { 512, 516, 514, 514 } },
    { .color = { 512, 512, 516, 512 } },
    { .color = { 410, 410, 410, 410 } },
    { .color = { 410, 410, 410, 410 } },
    { .color = { 410, 410, 410, 410 } },
};

WorldCollisionOccluder gNeoArkWoodlandPathSightOccluders[4] = {
    { NULL, NULL, { -2896, -1440, 1968, 0 }, { { -6800, 3616, -1968, 0 }, { 6800, 3616, 1968, 0 }, { -6800, -3616, -1968, 0 }, { 6800, -3616, 1968, 0 } }, { -1143, 0, 3946, 0 }, 7948, 1, 0 },
    { NULL, NULL, { -2768, -1504, -176, 0 }, { { -6656, 3616, 2240, 0 }, { 6656, 3616, -2240, 0 }, { -6656, -3616, 2240, 0 }, { 6656, -3616, -2240, 0 } }, { 1308, 0, 3889, 0 }, 7898, 1, 0 },
    { NULL, NULL, { 3696, -1632, 7328, 0 }, { { -6672, 3616, -672, 0 }, { 6672, 3616, 672, 0 }, { -6672, -3616, -672, 0 }, { 6672, -3616, 672, 0 } }, { -412, 0, 4085, 0 }, 7611, 1, 0 },
    { NULL, NULL, { 3136, -1568, -7872, 0 }, { { -6656, 3616, 2240, 0 }, { 6656, 3616, -2240, 0 }, { -6656, -3616, 2240, 0 }, { 6656, -3616, -2240, 0 } }, { 1308, 0, 3889, 0 }, 7898, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

// Cue-2 bases in the loaded sound bank; cue 1 uses the following script.
enum {
    NEO_ARK_WOODLAND_PATH_CLASS2_WALK_SOUND = 0x10000035,
    NEO_ARK_WOODLAND_PATH_CLASS2_RUN_SOUND  = 0x10000037,
    NEO_ARK_WOODLAND_PATH_CLASS3_WALK_SOUND = 0x10000025,
    NEO_ARK_WOODLAND_PATH_CLASS3_RUN_SOUND  = 0x10000027,
    NEO_ARK_WOODLAND_PATH_CLASS3_JUMP_SOUND = 0x10000029,
    NEO_ARK_WOODLAND_PATH_CLASS4_WALK_SOUND = 0x10000039,
    NEO_ARK_WOODLAND_PATH_CLASS4_RUN_SOUND  = 0x1000003B,
};

/// Class 4 walk and run cue bases; scripted jumps reuse walking cues.
static WorldCollisionFootstepSounds _gNeoArkWoodlandPathClass4FootstepSounds = {
    .walk         = NEO_ARK_WOODLAND_PATH_CLASS4_WALK_SOUND,
    .run          = NEO_ARK_WOODLAND_PATH_CLASS4_RUN_SOUND,
    .scriptedJump = NEO_ARK_WOODLAND_PATH_CLASS4_WALK_SOUND,
};

/// Class 2 walk and run cue bases; scripted jumps reuse walking cues.
static WorldCollisionFootstepSounds _gNeoArkWoodlandPathClass2FootstepSounds = {
    .walk         = NEO_ARK_WOODLAND_PATH_CLASS2_WALK_SOUND,
    .run          = NEO_ARK_WOODLAND_PATH_CLASS2_RUN_SOUND,
    .scriptedJump = NEO_ARK_WOODLAND_PATH_CLASS2_WALK_SOUND,
};

/// Class 3 has separate walk, run and scripted-jump cue bases.
static WorldCollisionFootstepSounds _gNeoArkWoodlandPathClass3FootstepSounds = {
    .walk         = NEO_ARK_WOODLAND_PATH_CLASS3_WALK_SOUND,
    .run          = NEO_ARK_WOODLAND_PATH_CLASS3_RUN_SOUND,
    .scriptedJump = NEO_ARK_WOODLAND_PATH_CLASS3_JUMP_SOUND,
};

/// Blocking surface without footstep cues, shared by classes 0 and 5..7.
static WorldCollisionSurfaceProperties _gNeoArkWoodlandPathDefaultSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's distinct blocking record has no footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkWoodlandPathSurfaceClass1 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 2 blocks probes and uses its walk/run cues for footsteps.
static WorldCollisionSurfaceProperties _gNeoArkWoodlandPathSurfaceClass2 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkWoodlandPathClass2FootstepSounds,
};

/// Class 3 blocks probes and selects three distinct footstep cue bases.
static WorldCollisionSurfaceProperties _gNeoArkWoodlandPathSurfaceClass3 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkWoodlandPathClass3FootstepSounds,
};

/// Class 4 blocks probes and uses its walk/run cues for footsteps.
static WorldCollisionSurfaceProperties _gNeoArkWoodlandPathSurfaceClass4 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkWoodlandPathClass4FootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkWoodlandPathSurfaceProperties[8] = {
    &_gNeoArkWoodlandPathDefaultSurface,
    &_gNeoArkWoodlandPathSurfaceClass1,
    &_gNeoArkWoodlandPathSurfaceClass2,
    &_gNeoArkWoodlandPathSurfaceClass3,
    &_gNeoArkWoodlandPathSurfaceClass4,
    &_gNeoArkWoodlandPathDefaultSurface,
    &_gNeoArkWoodlandPathDefaultSurface,
    &_gNeoArkWoodlandPathDefaultSurface,
};

static void _neoArkWoodlandPathInitRoomTask(Task* task);
static void _neoArkWoodlandPathRoomIdleState(Task* task);

#include "../../shared/effects/water.inc.c"

/// Refuses every key-item use with the item menu's cannot-use reply.
///
/// `itemId` is the selected inventory item ID; all arguments are ignored.
static s32 _neoArkWoodlandPathRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a Neo Ark room transition after resolving its destination variant.
///
/// Borrows a complete request and writable reply through synchronous dispatch;
/// they may alias. Copies all eight bytes before resolving the reply's room.
/// Query mode preserves the requested selectors. Neither pointer is retained;
/// the map overlay must be loaded. Always returns 1; the task and ID are unused.
static s32 _neoArkWoodlandPathResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_WOODLAND_PATH_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_WOODLAND_PATH_TRANSITION_ALLOWED;
}

/// Ignores CAP room commands and returns zero without starting an event.
///
/// `commandIndex` is the CAP command selector; all arguments are unused.
static s32 _neoArkWoodlandPathIgnoreRoomCommand(Task* task, s32 messageId, s32 commandIndex, s32 unused)
{
    return 0;
}

/// Forwards a room action to the woodland path's pool-A roamer controller.
///
/// `request` borrows a non-null four-byte record until dispatch returns. Action
/// IDs 1..6 select spawn points; zero clears the pending selector. Neither
/// pointer is retained here. Returns -1 while the controller is absent, zero
/// without its installed messages, otherwise the latch's reply 1. The receiver
/// is unused; the message ID and final payload are forwarded unchanged.
static s32 _neoArkWoodlandPathForwardRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    s32 result;

    if (_gNeoArkWoodlandPathRoamerPoolATask == NULL) {
        result = NEO_ARK_WOODLAND_PATH_ROAMER_ABSENT;
    } else {
        result = TASK_MESSAGE_DISPATCH_POINTER(_gNeoArkWoodlandPathRoamerPoolATask, messageId, request, unused);
    }
    return result;
}

/// Forwards a retreating enemy's HP report to the pool-A roamer controller.
///
/// `hp` is a signed hit-point count; positive values may refill a reserve slot,
/// while nonpositive reports only extend the cooldown. The receiver is unused;
/// the message ID and both payloads are forwarded unchanged. Returns -1 while
/// the controller is absent, otherwise forwards its dispatch word. The pool's
/// retreat handler defines no result, so senders must discard this word.
static s32 _neoArkWoodlandPathForwardActorEvent(Task* task, s32 messageId, s32 hp, s32 unused)
{
    s32 result;

    if (_gNeoArkWoodlandPathRoamerPoolATask == NULL) {
        result = NEO_ARK_WOODLAND_PATH_ROAMER_ABSENT;
    } else {
        result = taskMessageDispatch(_gNeoArkWoodlandPathRoamerPoolATask, messageId, hp, unused);
    }
    return result;
}

/// Registers the woodland receiver and starts its pool-A controller.
///
/// Start in state 0 with this overlay loaded. Publishes the borrowed room task
/// and the spawned controller, which may be null on allocation failure, then
/// advances to idle state 1. The controller services forwarded room actions.
static void _neoArkWoodlandPathInitRoomTask(Task* task)
{
    enum { NEO_ARK_WOODLAND_PATH_POOL_A_CONTROLLER_TASK = 1 };

    task->msgTable = _gNeoArkWoodlandPathRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gNeoArkWoodlandPathRoamerPoolATask = taskSpawnFromTable(gNeoArkWoodlandPathRoamerControllerTaskDescs, NEO_ARK_WOODLAND_PATH_POOL_A_CONTROLLER_TASK, 0, 0);
    task->state                         = task->state + 1;
}

/// Keeps the woodland path room task available for messages in state 1.
///
/// Leaves its state and message table intact; room teardown is owned by the caller.
static void _neoArkWoodlandPathRoomIdleState(Task* task)
{
}

/// Room task states 0..2: install messages and spawn pool A, idle, then retire.
///
/// Dispatch copies all three handlers before indexing by the task's state;
/// the state must remain in range and the room overlay must remain loaded.
static const TaskFuncTable3 _gNeoArkWoodlandPathRoomTaskStates = {
    { _neoArkWoodlandPathInitRoomTask, _neoArkWoodlandPathRoomIdleState, taskKill }
};

void neoArkWoodlandPathRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gNeoArkWoodlandPathRoomTaskStates;
    handlers.funcs[task->state](task);
}
