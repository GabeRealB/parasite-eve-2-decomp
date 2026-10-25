#include "rooms/neo_ark_south_promenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_02000.h"
#include "actors/actor_02300.h"
#include "actors/actor_03800.h"
#include "actors/actor_05600.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/room/room_variants.h"

static void _neoArkSouthPromenadeInitRoomTask(Task* task);
static void _neoArkSouthPromenadeIdleRoomTask(Task* task);

enum {
    NEO_ARK_SOUTH_PROMENADE_ROOM_STATE_REGISTER_MESSAGES = 0,
    NEO_ARK_SOUTH_PROMENADE_ROOM_STATE_IDLE              = 1,
    NEO_ARK_SOUTH_PROMENADE_ROOM_STATE_KILL              = 2,
};

/// Room controller handlers indexed by `Task::state` 0..2.
///
/// Registration installs the borrowed message table and enters idle; state 2
/// kills the task. The dispatcher copies all three pointers before calling one.
/// Keep this overlay loaded while the room controller can run.
static const TaskFuncTable3 _gNeoArkSouthPromenadeRoomTaskStates = {
    .funcs = {
        [NEO_ARK_SOUTH_PROMENADE_ROOM_STATE_REGISTER_MESSAGES] = _neoArkSouthPromenadeInitRoomTask,
        [NEO_ARK_SOUTH_PROMENADE_ROOM_STATE_IDLE]              = _neoArkSouthPromenadeIdleRoomTask,
        [NEO_ARK_SOUTH_PROMENADE_ROOM_STATE_KILL]              = taskKill,
    },
};

static s32 _neoArkSouthPromenadeRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _neoArkSouthPromenadeResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkSouthPromenadeIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _neoArkSouthPromenadeIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

static WorldCollisionGrid     _gNeoArkSouthPromenadeCollisionGrid;
static WorldCollisionOccluder _gNeoArkSouthPromenadeOccluders[1];
static WorldCollisionTrigger  _gNeoArkSouthPromenadeViewBoundaryTriggers[6];
static WorldCollisionTrigger  _gNeoArkSouthPromenadeActionTriggers[6];
static WorldCoordRoomLights   _gNeoArkSouthPromenadeRoomLights;

/// Synchronous room messages: resolve transitions, reject items and ignore actions/commands.
///
/// The four handlers use the task-message word ABI and return signed results;
/// pointer payloads are borrowed through dispatch. The final ID terminates lookup.
/// The registered room task borrows this table while this overlay is loaded.
static TaskMessageEntry _gNeoArkSouthPromenadeRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkSouthPromenadeResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkSouthPromenadeRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkSouthPromenadeIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _neoArkSouthPromenadeIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting gNeoArkSouthPromenadeRoomLighting[1] = {
    { &_gNeoArkSouthPromenadeRoomLights, NULL },
};

WorldCollisionRoomResources gNeoArkSouthPromenadeCollisionResources[1] = {
    {
        .grid                 = &_gNeoArkSouthPromenadeCollisionGrid,
        .viewBoundaryTriggers = _gNeoArkSouthPromenadeViewBoundaryTriggers,
        .actionTriggers       = _gNeoArkSouthPromenadeActionTriggers,
        .occluders            = _gNeoArkSouthPromenadeOccluders,
    },
};

u8* gNeoArkSouthPromenadeViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkSouthPromenadeViewCounts[1] = { NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT };

enum {
    NEO_ARK_SOUTH_PROMENADE_WARP1_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 2),
    NEO_ARK_SOUTH_PROMENADE_WARP1_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, 1),
};

DirectionWarpEntry gNeoArkSouthPromenadeWarpEntries[2] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 1137, 0, 2500 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 1137, 0, 2500 },
        .arrivalSound   = NEO_ARK_SOUTH_PROMENADE_WARP1_ARRIVAL_SOUND,
        .departureSound = NEO_ARK_SOUTH_PROMENADE_WARP1_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 10800, 0, 12800 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 10800, 0, 12800 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space face normals for the promenade collision grid, with ONE per unit.
static SVECTOR _gNeoArkSouthPromenadeCollisionGridNormals[] = {
#include "assets/neo_ark_south_promenade_collision_027CC_normals.inc"
};

/// Room-space collision vertices in signed game coordinates.
static SVECTOR _gNeoArkSouthPromenadeCollisionGridVerts[] = {
#include "assets/neo_ark_south_promenade_collision_027CC_verts.inc"
};

/// Indexed collision quads selecting the room's surface classes 0, 1 and 2.
static WorldCollisionGridFace _gNeoArkSouthPromenadeCollisionGridFaces[] = {
#include "assets/neo_ark_south_promenade_collision_027CC_faces.inc"
};

/// Serialized signed face-index lists ending at WORLD_COLLISION_GRID_CELL_END.
///
/// Retained words between lists remain outside the lists selected by the cell table.
static s16 _gNeoArkSouthPromenadeCollisionGridCells[] = {
#include "assets/neo_ark_south_promenade_collision_027CC_cells.inc"
};

/// XZ dimensions of the collision asset's X-major cell table.
enum {
    NEO_ARK_SOUTH_PROMENADE_GRID_CELL_COUNT_X = 4,
    NEO_ARK_SOUTH_PROMENADE_GRID_CELL_COUNT_Z = 4,
};

// The generated table supplies constant element offsets into the face-index pool.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gNeoArkSouthPromenadeCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..157,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define NEO_ARK_SOUTH_PROMENADE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkSouthPromenadeCollisionGridCells[(halfwordOffset)])
/// X-major cell directory; NULL entries are empty cells.
static s16* _gNeoArkSouthPromenadeCollisionGridTable[] = {
#include "assets/neo_ark_south_promenade_collision_027CC_table.inc"
};
#undef NEO_ARK_SOUTH_PROMENADE_CELL_FACE_LIST_AT_OFFSET

STATIC_ASSERT_SIZEOF(_gNeoArkSouthPromenadeCollisionGridTable,
                     NEO_ARK_SOUTH_PROMENADE_GRID_CELL_COUNT_X * NEO_ARK_SOUTH_PROMENADE_GRID_CELL_COUNT_Z * sizeof(s16*));

/// Promenade collision mesh indexed by 4000-unit XZ cells from origin (700, 500).
///
/// Room setup binds the borrowed view coordinate. Mesh pools and cell lists stay
/// live with this overlay while the grid is active; the asset contains 35 quads.
static WorldCollisionGrid _gNeoArkSouthPromenadeCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkSouthPromenadeCollisionGridNormals,
    .vertices    = _gNeoArkSouthPromenadeCollisionGridVerts,
    .faces       = _gNeoArkSouthPromenadeCollisionGridFaces,
    .cellFaceIds = _gNeoArkSouthPromenadeCollisionGridTable,
    .xBias       = -700,
    .zBias       = -500,
    .cellCountX  = NEO_ARK_SOUTH_PROMENADE_GRID_CELL_COUNT_X,
    .cellCountZ  = NEO_ARK_SOUTH_PROMENADE_GRID_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkSouthPromenadeCollisionGridFaces),
};

ViewCamera gNeoArkSouthPromenadeViewCameras[NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -7000, 0x58D6, -7000 } }, 289 },
    { { { { 4002, 0, 871 }, { 35, 4092, -163 }, { -871, 167, 3998 } }, { -0x3070, 1502, -3900 } }, 257 },
    { { { { -3965, 0, 1025 }, { 54, 4090, 210 }, { -1023, 217, -3960 } }, { -0x31E2, 1650, -0x2BC0 } }, 257 },
    { { { { -954, 0, 3983 }, { 197, 4090, 47 }, { -3978, 203, -953 } }, { -0x3070, 1690, -3200 } }, 207 },
    { { { { -870, 0, 4002 }, { 776, 4018, 168 }, { -3926, 794, -853 } }, { -8000, 2042, -3300 } }, 207 },
};

/// View 1's empty sprite list, retaining the decoded image-strip background.
static SpriteBatch _gNeoArkSouthPromenadeView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's source rectangles: 16 opaque sprites followed by nine translucent sprites.
///
/// Positions and sizes are pixels relative to the view centre; depths are OT
/// sorting values. Texture pages and CLUTs are encoded GPU addresses.
static SpriteSource _gNeoArkSouthPromenadeView2SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 24, 912, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 912, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 56, 900, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 16, 900, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -48, 1856, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1837, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1837, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1837, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, -112, 1837, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -88, 1837, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -40, 1837, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 8, 1837, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, 8, 1856, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -40, 1856, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -16, -16, 1856, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -16, 8, 1856, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 136, -64, 1905, { .fields = { 120, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 112, -48, 1905, { .fields = { 112, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, 0, -8, 1905, { .fields = { 24, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 16, -24, 1905, { .fields = { 40, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 40, -40, 1905, { .fields = { 8, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 64, -56, 1905, { .fields = { 8, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 88, -72, 1905, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 112, -88, 1905, { .fields = { 24, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, 136, -104, 1905, { .fields = { 0, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View 2's background selector and source ranges [0,4), [4,16) and [16,25).
///
/// The zero-count first record retains image strips; the terminal record selects
/// no sources. Serialized field_6 bytes remain uninterpreted.
static SpriteBatch _gNeoArkSouthPromenadeView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 12, 0, 0, { 2, 0 } },
    { 16, ARRAY_SIZE(_gNeoArkSouthPromenadeView2SpriteSources) - 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's source rectangles: 26 opaque sprites followed by 13 translucent sprites.
///
/// Positions and sizes are pixels relative to the view centre; depths are OT
/// sorting values. Texture pages and CLUTs are encoded GPU addresses.
static SpriteSource _gNeoArkSouthPromenadeView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 1862, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -120, 1625, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 1837, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 8, 1837, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -24, 1625, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 80, -72, 1625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -88, 1625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -72, 1837, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 48, -120, 1837, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 2306, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1862, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, 8, 1862, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 8, 1862, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -40, 1862, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1862, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1862, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1862, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1862, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1862, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 64, -88, 1862, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -120, 2175, { .fields = { 32, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 56, 8, 1862, { .fields = { 56, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -16, 1625, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -64, 1625, { .fields = { 32, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -112, 1625, { .fields = { 8, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, 112, -120, 1625, { .fields = { 56, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -16, 1812, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -64, 1812, { .fields = { 40, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, -64, 1862, { .fields = { 64, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -112, 1812, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 8 } }, 64, -120, 1750, { .fields = { 16, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 24 } }, 48, -112, 1862, { .fields = { 64, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, 48, -120, 1862, { .fields = { 40, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// View 3's background selector and source ranges [0,14), [14,26) and [26,39).
///
/// The zero-count first record retains image strips; the terminal record selects
/// no sources. Serialized field_6 bytes remain uninterpreted.
static SpriteBatch _gNeoArkSouthPromenadeView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 12, 0, 0, { 2, 0 } },
    { 26, ARRAY_SIZE(_gNeoArkSouthPromenadeView3SpriteSources) - 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list, retaining the decoded image-strip background.
static SpriteBatch _gNeoArkSouthPromenadeView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list, retaining the decoded image-strip background.
static SpriteBatch _gNeoArkSouthPromenadeView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkSouthPromenadeSpriteViews[NEO_ARK_SOUTH_PROMENADE_VIEW_COUNT] = {
    { { .empty = _gNeoArkSouthPromenadeView1SpriteBatches }, _gNeoArkSouthPromenadeView1SpriteBatches, NULL },
    { { .elements = _gNeoArkSouthPromenadeView2SpriteSources }, _gNeoArkSouthPromenadeView2SpriteBatches, NULL },
    { { .elements = _gNeoArkSouthPromenadeView3SpriteSources }, _gNeoArkSouthPromenadeView3SpriteBatches, NULL },
    { { .empty = _gNeoArkSouthPromenadeView4SpriteBatches }, _gNeoArkSouthPromenadeView4SpriteBatches, NULL },
    { { .empty = _gNeoArkSouthPromenadeView5SpriteBatches }, _gNeoArkSouthPromenadeView5SpriteBatches, NULL },
};

/// Two directional lights admitted in every promenade view, with Q12 RGB intensity.
///
/// Local translations supply light directions, not point-light positions. Room
/// lighting updates parent and compose the mutable coordinates for model shading.
static WorldCoordLight _gNeoArkSouthPromenadeDirectionalLights[] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 392, -740, 260 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4997, 4915, 4894 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -332, 260, -220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1310, 1290, 1228 }, { 0, 0 } },
};

/// Promenade light collection: two directional lights, no point or cone lights.
static WorldCoordRoomLights _gNeoArkSouthPromenadeRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gNeoArkSouthPromenadeDirectionalLights),
    .directionalLights     = _gNeoArkSouthPromenadeDirectionalLights,
    .pointLightCount       = 0,
    .pointLights           = NULL,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Six directed view boundaries: 4 to/from 5, 4 to/from 3 and 2 to/from 3.
///
/// Room setup binds and enables the mutable nodes; the sixth is marked LAST.
/// Origins and corner offsets use game coordinates, normals use ONE per unit.
static WorldCollisionTrigger _gNeoArkSouthPromenadeViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 5005, -1568, 2682, 0 }, { { 44, -1904, -2937, 0 }, { -44, -1904, 2938, 0 }, { 44, 1904, -2937, 0 }, { -44, 1904, 2938, 0 } }, { 4109, 0, 60, 0 }, { 0, 0, ONE, 0 }, 3500, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5272, -1600, 2635, 0 }, { { -17, -1904, 3156, 0 }, { 17, -1904, -3155, 0 }, { -17, 1904, 3156, 0 }, { 17, 1904, -3155, 0 } }, { -4101, 0, -23, 0 }, { 0, 0, ONE, 0 }, 3683, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9087, -1441, 2401, 0 }, { { 309, -1904, 2737, 0 }, { -308, -1904, -2736, 0 }, { 309, 1904, 2737, 0 }, { -308, 1904, -2736, 0 } }, { -4071, 0, 458, 0 }, { 0, 0, ONE, 0 }, 3347, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8784, -1601, 2400, 0 }, { { -323, -1904, -2719, 0 }, { 322, -1904, 2718, 0 }, { -323, 1904, -2719, 0 }, { 322, 1904, 2718, 0 } }, { 4084, 0, -485, 0 }, { 0, 0, ONE, 0 }, 3328, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2C11, -1440, 7472, 0 }, { { 3787, -1904, 557, 0 }, { -3788, -1904, -558, 0 }, { 3787, 1904, 557, 0 }, { -3788, 1904, -558, 0 } }, { -597, 0, 4053, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2BD1, -1504, 7632, 0 }, { { -3440, -1904, -505, 0 }, { 3439, -1904, 504, 0 }, { -3440, 1904, -505, 0 }, { 3439, 1904, 504, 0 } }, { 594, 0, -4057, 0 }, { 0, 0, ONE, 0 }, 3957, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed action parameters for the promenade's warps, stair climb and CAP commands.
enum {
    NEO_ARK_SOUTH_PROMENADE_SAVANNA_WARP        = (1 << 4) | 2, // Departure slot 1, destination arrival slot 2.
    NEO_ARK_SOUTH_PROMENADE_OBSERVATORY_WARP    = (2 << 4) | 3, // Departure slot 2, destination arrival slot 3.
    NEO_ARK_SOUTH_PROMENADE_STAIR_SURFACE_STEPS = (1 << 4) | 8, // Surface class 1 throughout eight steps.
    NEO_ARK_SOUTH_PROMENADE_STAIR_YAW           = 1024 >> 4,    // Quarter-turn yaw in 16-angle-unit steps.
    NEO_ARK_SOUTH_PROMENADE_CAP_INTERACTION_1   = 1,
    NEO_ARK_SOUTH_PROMENADE_CAP_INTERACTION_2   = 2,
};

/// Two departure warps, a stair action and three CAP interaction quads.
///
/// Warps request the Savanna Zone and Observatory. Coordinates are in game units;
/// facing normals use ONE per unit. Keep nodes live until unlinked; the sixth is LAST.
static WorldCollisionTrigger _gNeoArkSouthPromenadeActionTriggers[6] = {
    { NULL, NULL, NULL, { 1104, -48, 2416, 0 }, { { -496, 0, -1104, 0 }, { 496, 0, -1104, 0 }, { -496, 0, 1104, 0 }, { 496, 0, 1104, 0 } }, { 0, 4113, 0, 0 }, { ONE, 0, 0, 0 }, 1207, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SAVANNA_ZONE, NEO_ARK_SOUTH_PROMENADE_SAVANNA_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2D20, -1248, 0x3070, 0 }, { { 0, 1280, -1072, 0 }, { 0, -1280, -1072, 0 }, { 0, 1280, 1072, 0 }, { 0, -1280, 1072, 0 } }, { 4106, 0, 0, 0 }, { -ONE, 0, 0, 0 }, 1668, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_OBSERVATORY, NEO_ARK_SOUTH_PROMENADE_OBSERVATORY_WARP, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2A30, -64, 0x30A0, 0 }, { { -288, 0, -592, 0 }, { 288, 0, -592, 0 }, { -288, 0, 592, 0 }, { 288, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { -ONE, 0, 0, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_FACING, NEO_ARK_SOUTH_PROMENADE_STAIR_SURFACE_STEPS, NEO_ARK_SOUTH_PROMENADE_STAIR_YAW, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6464, -64, 912, 0 }, { { -5648, 0, -480, 0 }, { 5648, 0, -480, 0 }, { -5648, 0, 480, 0 }, { 5648, 0, 480, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, ONE, 0 }, 5655, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SOUTH_PROMENADE_CAP_INTERACTION_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4720, -64, 3936, 0 }, { { -3872, 0, -480, 0 }, { 3872, 0, -480, 0 }, { -3872, 0, 480, 0 }, { 3872, 0, 480, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, -ONE, 0 }, 3899, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SOUTH_PROMENADE_CAP_INTERACTION_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B2F, -64, 1375, 0 }, { { -1874, 0, -1634, 0 }, { 2368, 0, 812, 0 }, { -2367, 0, -811, 0 }, { 1875, 0, 1635, 0 } }, { 0, 4099, 0, 0 }, { -2276, 0, 3406, 0 }, 2495, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SOUTH_PROMENADE_CAP_INTERACTION_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 2's empty resource list, paired with an empty placement list.
static AreaResource _gNeoArkSouthPromenadeVariant2Resources[] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's empty resource list, paired with an empty placement list.
static AreaResource _gNeoArkSouthPromenadeVariant3Resources[] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Placement-entry keys and matching base-file numbers used by the encounter variants.
enum {
    NEO_ARK_SOUTH_PROMENADE_BLACK_BEETLE_RESOURCE = 38,
    NEO_ARK_SOUTH_PROMENADE_PAWN105600_RESOURCE   = 56,
    NEO_ARK_SOUTH_PROMENADE_PAWN102000_RESOURCE   = 20,
    NEO_ARK_SOUTH_PROMENADE_ROOK202300_RESOURCE   = 23,
};

/// Variant 4's Black Beetle resource recipe, terminated by AREA_PLACEMENT_END.
///
/// Six placements share entry 38; base-10 file 38 selects actor_103800, descriptor 0.
/// The recipe and borrowed actor descriptor must stay loaded through loading/spawning.
static AreaResource _gNeoArkSouthPromenadeVariant4Resources[] = {
    { NEO_ARK_SOUTH_PROMENADE_BLACK_BEETLE_RESOURCE, NEO_ARK_SOUTH_PROMENADE_BLACK_BEETLE_RESOURCE, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 5's Pawn GOLEM resource recipe, terminated by AREA_PLACEMENT_END.
///
/// Two placements share entry 56; base-10 file 56 selects actor_105600, descriptor 0.
/// The recipe and borrowed actor table must stay loaded through loading/spawning.
static AreaResource _gNeoArkSouthPromenadeVariant5Resources[] = {
    { NEO_ARK_SOUTH_PROMENADE_PAWN105600_RESOURCE, NEO_ARK_SOUTH_PROMENADE_PAWN105600_RESOURCE, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor105600PawnGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's Pawn and Rook GOLEM recipes, terminated by AREA_PLACEMENT_END.
///
/// Two entry-20 placements use base-10 file 20 and actor_102000; entry 23 uses
/// base-20 file 23 and actor_202300. Both select descriptor 0 in borrowed tables.
/// Keep the recipe and actor tables loaded through loading/spawning.
static AreaResource _gNeoArkSouthPromenadeVariant11Resources[] = {
    { NEO_ARK_SOUTH_PROMENADE_PAWN102000_RESOURCE, NEO_ARK_SOUTH_PROMENADE_PAWN102000_RESOURCE, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102000PawnGolemTasks },
    { NEO_ARK_SOUTH_PROMENADE_ROOK202300_RESOURCE, NEO_ARK_SOUTH_PROMENADE_ROOK202300_RESOURCE, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gNeoArkSouthPromenadeAreaVariants[NEO_ARK_SOUTH_PROMENADE_AREA_VARIANT_COUNT] = {
    [2]  = { .placements = gMapNeoArkSouthPromenadeVariant2Placements, .resources = _gNeoArkSouthPromenadeVariant2Resources },
    [3]  = { .placements = gMapNeoArkSouthPromenadeVariant3Placements, .resources = _gNeoArkSouthPromenadeVariant3Resources },
    [4]  = { .placements = gMapNeoArkSouthPromenadeVariant4Placements, .resources = _gNeoArkSouthPromenadeVariant4Resources },
    [5]  = { .placements = gMapNeoArkSouthPromenadeVariant5Placements, .resources = _gNeoArkSouthPromenadeVariant5Resources },
    [11] = { .placements = gMapNeoArkSouthPromenadeVariant11Placements, .resources = _gNeoArkSouthPromenadeVariant11Resources },
};

/// Single room-space sight-blocking quad, linked and enabled during room setup.
///
/// Its corners are relative to the origin in game units; the normal uses 4096
/// per unit. LAST includes this record before ending the source-array scan.
/// List links and transient flags are mutable. Keep this overlay loaded until
/// unlinked. Persistent flag bit 0 is retained, but its role is unproven.
static WorldCollisionOccluder _gNeoArkSouthPromenadeOccluders[1] = {
    {
        .next     = NULL,
        .prevLink = NULL,
        .origin   = { 4912, -1568, 7344, 0 },
        .vertices = {
            { -4560, 2496, 2832, 0 },
            { 4560, 2496, -2832, 0 },
            { -4560, -2496, 2832, 0 },
            { 4560, -2496, -2832, 0 },
        },
        .normal     = { 2162, 0, 3481, 0 },
        .radius     = 5904,
        .flags      = 0x01 | WORLD_COLLISION_OCCLUDER_LAST,
        .unknown_3B = 0,
    },
};

/// Cue-2 request bases in the loaded type-1 sound bank; cue 1 uses the next entry.
enum {
    NEO_ARK_SOUTH_PROMENADE_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    NEO_ARK_SOUTH_PROMENADE_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
};

/// Walking, running and scripted-jump footstep bases for surface classes 1 and 2.
///
/// Walking and scripted jumps share a cue pair; running uses the following pair.
/// Consumers borrow the cues while this overlay and its type-1 bank are loaded.
static WorldCollisionFootstepSounds _gNeoArkSouthPromenadeFootstepSounds = {
    .walk         = NEO_ARK_SOUTH_PROMENADE_FOOTSTEP_WALK,
    .run          = NEO_ARK_SOUTH_PROMENADE_FOOTSTEP_RUN,
    .scriptedJump = NEO_ARK_SOUTH_PROMENADE_FOOTSTEP_WALK,
};

/// Classes 0 and 3..7: probe blocking, weapon impacts and pushback without footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSouthPromenadeSolidSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1: probe blocking, weapon impacts, pushback and the room's footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSouthPromenadeSolidFootstepSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkSouthPromenadeFootstepSounds,
};

/// Class 2: probe passage and no weapon impacts, retaining pushback and footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSouthPromenadeProbePassSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkSouthPromenadeFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkSouthPromenadeSurfaceProperties[NEO_ARK_SOUTH_PROMENADE_SURFACE_CLASS_COUNT] = {
    &_gNeoArkSouthPromenadeSolidSurface,
    &_gNeoArkSouthPromenadeSolidFootstepSurface,
    &_gNeoArkSouthPromenadeProbePassSurface,
    &_gNeoArkSouthPromenadeSolidSurface,
    &_gNeoArkSouthPromenadeSolidSurface,
    &_gNeoArkSouthPromenadeSolidSurface,
    &_gNeoArkSouthPromenadeSolidSurface,
    &_gNeoArkSouthPromenadeSolidSurface,
};

/// Refuses every key-item use request in this room, returning zero to the item menu.
///
/// `itemId` is the selected inventory item; all arguments are ignored.
static s32 _neoArkSouthPromenadeRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a room-transition destination from Neo Ark progress and permits departure.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; the task and message ID are unused.
/// Request and reply borrow complete eight-byte records and may be the same object.
/// The full record is copied before the stage resolver updates its room selector;
/// queries preserve the copied destination. Area and arrival selectors must be
/// valid in the destination stage. Neither pointer is retained. Always returns
/// `ROOM_VARIANT_TRANSITION_DIRECT`, even when the destination is unchanged.
static s32 _neoArkSouthPromenadeResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Ignores room commands from CAP and facing triggers, returning zero.
///
/// The command selector and its argument are unused in this room.
static s32 _neoArkSouthPromenadeIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room actions from direction triggers, returning zero.
///
/// `request` is borrowed for synchronous dispatch and is neither read nor retained.
static s32 _neoArkSouthPromenadeIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Called in state 0 with a live task. The loaded room's table is borrowed
/// for subsequent synchronous messages while the registered task remains alive.
static void _neoArkSouthPromenadeInitRoomTask(Task* task)
{
    task->msgTable = _gNeoArkSouthPromenadeRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the room task idle in state 1 while its message table remains available.
static void _neoArkSouthPromenadeIdleRoomTask(Task* task)
{
}

void neoArkSouthPromenadeRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkSouthPromenadeRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void neoArkSouthPromenadeRegisterGolemEffectsTask(Task* task)
{
    enum { REGISTER_EFFECTS,
           EFFECTS_REGISTERED };

    if (task->state == REGISTER_EFFECTS) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_SOUTH_PROMENADE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_SOUTH_PROMENADE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_SOUTH_PROMENADE_SPARK_BURST;
        task->state             = EFFECTS_REGISTERED;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkSouthPromenadeRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkSouthPromenadeRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkSouthPromenadeRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
