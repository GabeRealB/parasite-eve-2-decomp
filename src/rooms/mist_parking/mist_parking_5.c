#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "mist_parking_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"

/// World-coordinate capsule endpoints and the separate pulsing point-glow centre.
///
/// Positions use signed whole world-coordinate units and are borrowed only
/// during drawing. Row drawing takes three consecutive capsule pairs. Every
/// vector includes the fourth halfword read alongside Z by the GTE.
typedef struct {
    SVECTOR capsuleEndpoints[13][2]; // Thirteen capsule pairs; pair 1 is retained but unused
    SVECTOR pointCenter;             // Centre shared by the cyan diamond and disc glows
} _MistParkingGlowWorldPoints;
STATIC_ASSERT_SIZEOF(_MistParkingGlowWorldPoints, 0xD8);

/// XZ cell-row stride of the parking grid's authored cell index.
enum { MIST_PARKING_COLLISION_CELL_COUNT_Z = 4 };

static WorldCollisionTrigger      _gMistParkingViewBoundaryTriggers[12];
static WorldCollisionTrigger      _gMistParkingEarlyChapterActionTriggers[13];
static WorldCollisionTrigger      _gMistParkingLaterChapterActionTriggers[14];
static WorldCoordRoomAmbientEntry _gMistParkingRoom2AmbientByView[MIST_PARKING_VIEW_COUNT + 1];
static WorldCoordRoomLights       _gMistParkingRoom1Lights;
static WorldCoordRoomLights       _gMistParkingRoom2Lights;

/// Scene target, MIST stock profile and script delays for the variant-1 shop poses.
enum {
    MIST_PARKING_JODIE_SCENE_PLACEMENT   = 1,
    MIST_PARKING_SHOP_STOCK_PROFILE      = 0x10,
    MIST_PARKING_SHOP_POSE_DELAY_UPDATES = 20,
    MIST_PARKING_SHOP_REPLY_END_UPDATES  = 30
};

EvsCommand gMistParkingJodieShopTalkScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingShopPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gMistParkingPlayerShopTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieIdleAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_POSE_DELAY_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieGreetingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieIdleAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingJodieOpenShopScript[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingShopPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = MIST_PARKING_SHOP_STOCK_PROFILE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_POSE_DELAY_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gMistParkingPlayerShopTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieReplyEndAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_REPLY_END_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieShopExitAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingJodieShopReplyScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingShopPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gMistParkingPlayerShopTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieReplyEndAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_REPLY_END_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieIdleAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingJodieShopFarewellScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingShopPlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &gMistParkingPlayerShopTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieReplyEndAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_REPLY_END_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &gMistParkingJodieShopExitAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Authored parking glow positions in signed world-coordinate units.
///
/// Capsule drawers borrow one endpoint pair; row drawers borrow three
/// consecutive pairs. The separate centre serves
/// all point glows. Keep the unused second pair and all vector fourth halfwords.
static _MistParkingGlowWorldPoints _gMistParkingGlowWorldPoints = {
    .capsuleEndpoints = {
        { { 11430, -2690, -4750, 0 }, { 10230, -2690, -4750, 0 } },
        { { 8070, -2690, -4750, 0 }, { 6860, -2690, -4750, 0 } },
        { { 3870, -3190, -4840, 0 }, { 3870, -3190, -3690, 0 } },
        { { 3870, -3190, -1600, 0 }, { 3870, -3190, -450, 0 } },
        { { 3870, -3190, 1020, 0 }, { 3870, -3190, 2180, 0 } },
        { { -2000, -3190, -4840, 0 }, { -2000, -3190, -3690, 0 } },
        { { -2000, -3190, -1600, 0 }, { -2000, -3190, -450, 0 } },
        { { -2000, -3190, 1020, 0 }, { -2000, -3190, 2180, 0 } },
        { { -5870, -3190, -4840, 0 }, { -5870, -3190, -3690, 0 } },
        { { -5870, -3190, -1600, 0 }, { -5870, -3190, -450, 0 } },
        { { -5870, -3190, 1020, 0 }, { -5870, -3190, 2180, 0 } },
        { { -7780, -3190, -2000, 0 }, { -8940, -3190, -2000, 0 } },
        { { -7780, -3190, -5000, 0 }, { -8940, -3190, -5000, 0 } },
    },
    .pointCenter = { -7412, -1345, 773, 0 },
};

WorldCollisionRoomResources gMistParkingRoomCollisionResources[MIST_PARKING_ROOM_VARIANT_COUNT] = {
    { &gMistParkingCollisionGrid, _gMistParkingViewBoundaryTriggers, _gMistParkingEarlyChapterActionTriggers, NULL },
    { &gMistParkingCollisionGrid, _gMistParkingViewBoundaryTriggers, _gMistParkingEarlyChapterActionTriggers, NULL },
    { &gMistParkingCollisionGrid, _gMistParkingViewBoundaryTriggers, _gMistParkingLaterChapterActionTriggers, NULL },
    { &gMistParkingCollisionGrid, _gMistParkingViewBoundaryTriggers, _gMistParkingLaterChapterActionTriggers, NULL },
};

/// Early-chapter logical-view permutation for room variants 1 and 2.
///
/// Twenty bytes indexed by logical view - 1, each selecting a 1-based camera,
/// image and sprite slot. Later-chapter variants use the gameplay identity map.
static u8 _gMistParkingEarlyChapterViewMap[MIST_PARKING_VIEW_COUNT] = {
    1,
    2,
    3,
    4,
    18,
    19,
    7,
    20,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    5,
    6,
    8,
};

u8* gMistParkingViewMaps[MIST_PARKING_ROOM_VARIANT_COUNT] = {
    _gMistParkingEarlyChapterViewMap,
    _gMistParkingEarlyChapterViewMap,
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount gMistParkingViewCounts[MIST_PARKING_ROOM_VARIANT_COUNT] = {
    MIST_PARKING_VIEW_COUNT, MIST_PARKING_VIEW_COUNT, MIST_PARKING_VIEW_COUNT, MIST_PARKING_VIEW_COUNT
};

WorldCoordRoomLighting gMistParkingRoomLightingTable[MIST_PARKING_ROOM_VARIANT_COUNT] = {
    { &_gMistParkingRoom1Lights, NULL },
    { &_gMistParkingRoom2Lights, _gMistParkingRoom2AmbientByView },
    { &_gMistParkingRoom1Lights, NULL },
    { &_gMistParkingRoom2Lights, _gMistParkingRoom2AmbientByView },
};

DirectionWarpEntry gMistParkingWarpEntries[MIST_PARKING_WARP_COUNT] = {
    { { { .word = ONE * 3 / 4 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, { { .word = ONE * 3 / 4 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_PARKING, 0x15), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_PARKING, 0x14), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, 8448, 0, -3014 }, { 0, 0, 0, 0 }, { { .word = 0 }, 8448, 0, -3014 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ONE * 3 / 4 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, { { .word = ONE * 3 / 4 }, 0x2EEC, 0, -5053 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ONE / 2 }, 5022, 0, -5260 }, { 0, 0, 0, 0 }, { { .word = ONE / 2 }, 5022, 0, -5260 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space grid normals in 1/4096 units; the first two are reserved for Pierce's patch.
static SVECTOR _gMistParkingCollisionGridNormals[] = {
#include "assets/mist_parking_collision_14C44_normals.inc"
};

/// Grid vertices in signed world units; the first six are rewritten for Pierce's patch.
static SVECTOR _gMistParkingCollisionGridVerts[] = {
#include "assets/mist_parking_collision_14C44_verts.inc"
};

/// Indexed grid faces, including the two initially disabled Pierce-patch faces.
static WorldCollisionGridFace _gMistParkingCollisionGridFaces[] = {
#include "assets/mist_parking_collision_14C44_faces.inc"
};

/// Packed signed face-index lists; each cell's list ends with WORLD_COLLISION_GRID_CELL_END.
static s16 _gMistParkingCollisionGridCells[] = {
#include "assets/mist_parking_collision_14C44_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMistParkingCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MIST_PARKING_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMistParkingCollisionGridCells[(halfwordOffset)])
/// X-major 7-by-4 grid cell pointers into the packed face-index lists.
static s16* _gMistParkingCollisionGridTable[] = {
#include "assets/mist_parking_collision_14C44_table.inc"
};
#undef MIST_PARKING_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gMistParkingCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gMistParkingCollisionGridNormals,
    .vertices    = _gMistParkingCollisionGridVerts,
    .faces       = _gMistParkingCollisionGridFaces,
    .cellFaceIds = _gMistParkingCollisionGridTable,
    .xBias       = 11000,
    .zBias       = 7000,
    .cellCountX  = ARRAY_SIZE(_gMistParkingCollisionGridTable) / MIST_PARKING_COLLISION_CELL_COUNT_Z,
    .cellCountZ  = MIST_PARKING_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gMistParkingCollisionGridFaces),
};

ViewCamera gMistParkingViewCameras[MIST_PARKING_VIEW_COUNT] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6978, 0 } }, 257 },
    { { { { 1185, 0, -3920 }, { -1131, 3921, -341 }, { 3754, 1181, 1134 } }, { -4650, 1900, 5340 } }, 257 },
    { { { { -3872, 0, -1335 }, { -882, 3074, 2558 }, { 1002, 2706, -2906 } }, { -7880, 2620, 660 } }, 207 },
    { { { { 3928, 0, -1157 }, { -680, 3313, -2310 }, { 936, 2408, 3177 } }, { -7810, 2620, 3170 } }, 207 },
    { { { { 1525, 0, -3801 }, { -1441, 3789, -578 }, { 3517, 1553, 1411 } }, { 2220, 3050, 5330 } }, 257 },
    { { { { 1940, 0, 3607 }, { 848, 3981, -456 }, { -3506, 963, 1885 } }, { -4330, 2770, 5440 } }, 257 },
    { { { { 3908, 0, 1223 }, { 316, 3956, -1011 }, { -1182, 1059, 3775 } }, { 2600, 2770, 4330 } }, 257 },
    { { { { 3944, 0, -1103 }, { -319, 3920, -1143 }, { 1055, 1187, 3775 } }, { -2890, 2770, 4790 } }, 257 },
    { { { { 1474, 0, 3821 }, { 1397, 3812, -539 }, { -3556, 1497, 1372 } }, { 6830, 1450, -490 } }, 257 },
    { { { { 2493, 0, -3249 }, { 2259, 2943, 1733 }, { 2335, -2847, 1791 } }, { -2570, 630, 4110 } }, 257 },
    { { { { 4040, 0, -670 }, { -557, 2276, -3359 }, { 372, 3405, 2245 } }, { -3370, 3940, 5780 } }, 257 },
    { { { { -1614, 0, -3764 }, { -140, 4093, 60 }, { 3761, 152, -1613 } }, { -1730, 1590, 2750 } }, 680 },
    { { { { 2089, 0, -3522 }, { -459, 4060, -272 }, { 3492, 534, 2072 } }, { -2680, 1720, 5510 } }, 541 },
    { { { { 4024, 0, 759 }, { -97, 4061, 518 }, { -753, -527, 3991 } }, { -5100, 1320, 5320 } }, 257 },
    { { { { -812, 0, -4014 }, { -303, 4084, 61 }, { 4003, 309, -810 } }, { -3440, 1640, 3930 } }, 541 },
    { { { { 958, 0, 3982 }, { 680, 4035, -163 }, { -3923, 700, 944 } }, { -6690, 1740, 5270 } }, 257 },
    { { { { 2152, 0, -3484 }, { -38, 4095, -23 }, { 3484, 45, 2152 } }, { 2390, 510, 4370 } }, 329 },
    { { { { 1525, 0, -3801 }, { -1441, 3789, -578 }, { 3517, 1553, 1411 } }, { 2220, 3050, 5330 } }, 257 },
    { { { { 1940, 0, 3607 }, { 848, 3981, -456 }, { -3506, 963, 1885 } }, { -4330, 2770, 5440 } }, 257 },
    { { { { 3944, 0, -1103 }, { -319, 3920, -1143 }, { 1055, 1187, 3775 } }, { -2890, 2770, 4790 } }, 257 },
};

/// Texture depth, neutral modulation and opaque code flags for parking sprites.
enum {
    MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT = 1,
    MIST_PARKING_SPRITE_NEUTRAL_RGB        = 128,
    MIST_PARKING_SPRITE_OPAQUE             = 0
};

/// Background-only batch list for mapped parking view 1.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 2.
///
/// Contains 21 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView2SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 128 } }, -160, -120, 725, { .fields = { 80, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 112 } }, -160, 8, 725, { .fields = { 32, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 128 } }, -112, -120, 725, { .fields = { 104, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 112 } }, -112, 8, 725, { .fields = { 80, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -88, -120, 725, { .fields = { 64, 184 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -88, -48, 725, { .fields = { 32, 168 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -88, 8, 725, { .fields = { 32, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -88, 64, 725, { .fields = { 16, 56 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -72, -120, 725, { .fields = { 72, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, -48, 725, { .fields = { 24, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, 8, 725, { .fields = { 48, 168 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, 64, 725, { .fields = { 56, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -64, -112, 775, { .fields = { 64, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -64, -48, 775, { .fields = { 48, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -64, 8, 775, { .fields = { 56, 168 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -64, 64, 775, { .fields = { 24, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -56, 8, 825, { .fields = { 24, 160 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -56, 56, 825, { .fields = { 16, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 24, 875, { .fields = { 48, 224 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 56, 875, { .fields = { 56, 224 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -40, 32, 925, { .fields = { 24, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 2.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMistParkingView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 3.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 4.
///
/// Contains 26 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView4SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 24 } }, -120, -8, 720, { .fields = { 120, 184 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -56, -8, 802, { .fields = { 8, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -16, -8, 799, { .fields = { 120, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 32 } }, -16, -120, 650, { .fields = { 48, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 32 } }, -88, -120, 637, { .fields = { 32, 152 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 32 } }, -160, -120, 625, { .fields = { 56, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -152, -88, 637, { .fields = { 120, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 16 } }, -88, -88, 650, { .fields = { 104, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 16 } }, -16, -88, 662, { .fields = { 112, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 24 } }, -144, -72, 650, { .fields = { 24, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 24 } }, -80, -72, 662, { .fields = { 24, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, -16, -72, 675, { .fields = { 0, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 16 } }, -136, -48, 614, { .fields = { 104, 152 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -80, -48, 671, { .fields = { 96, 32 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, -16, -48, 731, { .fields = { 112, 16 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, -16, -32, 766, { .fields = { 40, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, -72, -32, 715, { .fields = { 32, 24 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, -128, -32, 663, { .fields = { 40, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -8, -24, 799, { .fields = { 88, 80 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -48, -16, 769, { .fields = { 88, 40 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -80, -8, 750, { .fields = { 96, 184 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -112, -8, 723, { .fields = { 96, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 16, 840, { .fields = { 112, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 0, 16, 925, { .fields = { 104, 152 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, 16, 905, { .fields = { 120, 152 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, -48, 24, 875, { .fields = { 56, 184 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 4.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 18, 0, 0, { 2, 0 } },
    { 18, ARRAY_SIZE(_gMistParkingView4SpriteSources) - 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 5.
///
/// Contains 39 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView5SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -160, -56, 1050, { .fields = { 72, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -152, -48, 1075, { .fields = { 64, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -144, -40, 1125, { .fields = { 0, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, -32, 1150, { .fields = { 80, 224 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -56, 16, 1375, { .fields = { 104, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 112 } }, -160, -40, 1050, { .fields = { 80, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -136, -40, 1150, { .fields = { 32, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -136, 16, 1100, { .fields = { 48, 160 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -120, -48, 1200, { .fields = { 32, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -120, 0, 1150, { .fields = { 32, 152 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -104, -48, 1250, { .fields = { 16, 192 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -104, 0, 1200, { .fields = { 16, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -88, -48, 1250, { .fields = { 8, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -88, 0, 1250, { .fields = { 16, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -80, -48, 1375, { .fields = { 24, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, 0, 1375, { .fields = { 88, 224 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, -24, 1375, { .fields = { 120, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, 0, 1375, { .fields = { 0, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, 0, 1375, { .fields = { 120, 176 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 96 } }, -160, 24, 1050, { .fields = { 24, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 112 } }, -104, 8, 1050, { .fields = { 80, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 120 } }, -72, 0, 1050, { .fields = { 104, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, -48, 0, 1050, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, -16, 1375, { .fields = { 120, 240 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -24, -32, 1375, { .fields = { 120, 40 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -8, -32, 1350, { .fields = { 120, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 8, -24, 1350, { .fields = { 120, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 24, -24, 1350, { .fields = { 8, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, -24, 1350, { .fields = { 112, 240 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, 0, 1100, { .fields = { 8, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -16, 0, 1150, { .fields = { 8, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 0, 0, 1200, { .fields = { 0, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 16, 0, 1275, { .fields = { 0, 192 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 32, 8, 1350, { .fields = { 96, 224 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 8, 1350, { .fields = { 120, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 40, 1275, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 0, 48, 1200, { .fields = { 48, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -16, 48, 1150, { .fields = { 48, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -32, 48, 1100, { .fields = { 64, 176 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 5.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 15, 0, 0, { 2, 0 } },
    { 19, ARRAY_SIZE(_gMistParkingView5SpriteSources) - 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 6.
///
/// Contains 37 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView6SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 144, -24, 1250, { .fields = { 8, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 128, -24, 1250, { .fields = { 16, 200 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 48 } }, 104, -16, 1250, { .fields = { 104, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 48 } }, 72, -16, 1250, { .fields = { 96, 160 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 8, 1250, { .fields = { 80, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 48 } }, 72, -16, 1250, { .fields = { 104, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 72, 32, 1250, { .fields = { 56, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 48 } }, 80, -16, 1250, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, 80, 32, 1250, { .fields = { 96, 88 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 88, -16, 1200, { .fields = { 0, 200 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, 88, 40, 1200, { .fields = { 120, 56 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 56 } }, 104, -16, 1150, { .fields = { 120, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 104, 40, 1150, { .fields = { 32, 200 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 128, -8, 1050, { .fields = { 24, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 128, 56, 1050, { .fields = { 24, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 24 } }, 16, -120, 1250, { .fields = { 24, 88 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 72 } }, 24, -96, 1250, { .fields = { 72, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 72 } }, 16, -24, 1250, { .fields = { 72, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -64, 16, 1625, { .fields = { 0, 160 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -56, 16, 1500, { .fields = { 0, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, -48, 8, 1500, { .fields = { 88, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 48, 1500, { .fields = { 96, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, -40, 8, 1500, { .fields = { 88, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -40, 48, 1500, { .fields = { 8, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, -24, 8, 1500, { .fields = { 88, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -8, 8, 1500, { .fields = { 112, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 8, 8, 1450, { .fields = { 96, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 24, 8, 1400, { .fields = { 64, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -24, 48, 1500, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -8, 48, 1500, { .fields = { 96, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 8, 48, 1450, { .fields = { 56, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 24, 48, 1400, { .fields = { 56, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 40, 24, 1350, { .fields = { 112, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 40, 72, 1350, { .fields = { 96, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 56, 32, 1300, { .fields = { 80, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 48 } }, 56, 72, 1300, { .fields = { 104, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 72 } }, 72, 48, 1250, { .fields = { 16, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 6.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 1, 0, 0, { 3, 0 } },
    { 5, 10, 0, 0, { 2, 0 } },
    { 15, 3, 0, 0, { 4, 0 } },
    { 18, ARRAY_SIZE(_gMistParkingView6SpriteSources) - 18, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 7.
///
/// Contains 9 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView7SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -160, -120, 1425, { .fields = { 112, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -160, -56, 1325, { .fields = { 112, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -160, 8, 1412, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -144, 8, 1412, { .fields = { 104, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -144, -56, 1350, { .fields = { 104, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -144, -112, 1437, { .fields = { 120, 200 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -136, -56, 1375, { .fields = { 104, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -136, 8, 1425, { .fields = { 112, 200 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -128, 8, 1425, { .fields = { 104, 192 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 7.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMistParkingView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 8.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 9.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 10.
///
/// Contains 25 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView10SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -160, -120, 125, { .fields = { 96, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -160, -56, 125, { .fields = { 96, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, -120, 112, { .fields = { 96, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -128, -64, 112, { .fields = { 64, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -96, -120, 100, { .fields = { 40, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -96, -80, 100, { .fields = { 40, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -64, -120, 87, { .fields = { 48, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -48, -120, 75, { .fields = { 48, 240 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 136, -120, 50, { .fields = { 72, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, 136, -72, 50, { .fields = { 72, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 112, -120, 50, { .fields = { 40, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 120, -88, 50, { .fields = { 112, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, -120, 50, { .fields = { 96, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 128, 72, 42, { .fields = { 96, 184 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 128, 24, 42, { .fields = { 64, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 96, 80, 45, { .fields = { 64, 192 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 96, 40, 45, { .fields = { 40, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 64, 88, 50, { .fields = { 32, 176 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 64, 56, 50, { .fields = { 16, 32 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 32, 96, 55, { .fields = { 64, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 32, 72, 55, { .fields = { 16, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 88, 57, { .fields = { 48, 32 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -8, 96, 62, { .fields = { 24, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, 104, 67, { .fields = { 24, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, 112, 75, { .fields = { 24, 16 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 10.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMistParkingView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 11.
///
/// Contains 4 source elements, without a terminator. XY and dimensions are
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Uses opaque 8-bit textures and neutral RGB. The batch ranges select these
/// overlay-owned records while the view's cached packets are allocated/drawn.
static SpriteSource _gMistParkingView11SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -88, -64, 675, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -104, -56, 675, { .fields = { 112, 80 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -112, -32, 700, { .fields = { 120, 160 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -120, 8, 725, { .fields = { 120, 224 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 11.
///
/// The first zero-count record keeps decoded background image strips. Ranges
/// index source elements; the terminal record contributes no sprites. The two
/// serialized tail bytes are preserved; their role is unproven.
static SpriteBatch _gMistParkingView11SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMistParkingView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 12.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView12SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 13.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView13SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 14.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView14SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 15.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView15SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 16.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView16SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 17.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView17SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 18.
///
/// Contains 35 source elements with no terminator. XY and dimensions use
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Opaque 8-bit textures use neutral RGB and the view's palette at (0, 255).
/// Batch ranges borrow these records while cached packets are allocated/drawn.
static SpriteSource _gMistParkingView18SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -160, -56, 1050, { .fields = { 64, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -152, -48, 1075, { .fields = { 56, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -144, -40, 1125, { .fields = { 32, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, -32, 115, { .fields = { 88, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -56, 16, 936, { .fields = { 120, 240 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 112 } }, -160, -40, 1050, { .fields = { 88, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -136, -40, 1150, { .fields = { 16, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -136, 16, 1100, { .fields = { 16, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -120, -48, 1200, { .fields = { 56, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -120, 0, 1150, { .fields = { 16, 176 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -104, -48, 1250, { .fields = { 72, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -104, 0, 1200, { .fields = { 0, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -88, -48, 1250, { .fields = { 32, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -88, 0, 1250, { .fields = { 48, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -80, -48, 1375, { .fields = { 40, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, 0, 1375, { .fields = { 64, 80 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, -24, 1375, { .fields = { 16, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, 0, 1375, { .fields = { 24, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, 0, 1375, { .fields = { 104, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 8, 1350, { .fields = { 96, 232 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 32, 0, 1350, { .fields = { 56, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 96 } }, -160, 24, 1050, { .fields = { 32, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 112 } }, -104, 8, 1050, { .fields = { 72, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 120 } }, -72, 0, 1050, { .fields = { 104, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, -48, 0, 1050, { .fields = { 112, 120 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -32, 0, 1100, { .fields = { 0, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -32, 48, 1100, { .fields = { 40, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -16, 48, 1150, { .fields = { 24, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -16, 0, 1150, { .fields = { 8, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 0, 0, 1200, { .fields = { 0, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 0, 48, 1200, { .fields = { 40, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 40, 1275, { .fields = { 120, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 16, 0, 1275, { .fields = { 0, 192 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -32, -8, 1375, { .fields = { 88, 112 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -16, -8, 1453, { .fields = { 112, 248 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 18, with decoded background image strips.
///
/// The zero-count header keeps the background; subsequent ranges index source
/// elements and the terminal record contributes none. Serialized tail bytes
/// are preserved, including their nonzero values; their role is unproven.
static SpriteBatch _gMistParkingView18SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 15, 0, 0, { 2, 0 } },
    { 19, ARRAY_SIZE(_gMistParkingView18SpriteSources) - 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped parking view 19.
///
/// Contains 43 source elements with no terminator. XY and dimensions use
/// pixels relative to the view centre; depths are unscaled OT sorting values.
/// Opaque 8-bit textures use neutral RGB and the view's palette at (0, 255).
/// Batch ranges borrow these records while cached packets are allocated/drawn.
static SpriteSource _gMistParkingView19SpriteSources[] = {
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 144, -24, 1250, { .fields = { 32, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, 128, -24, 1250, { .fields = { 8, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 48 } }, 72, -16, 1250, { .fields = { 80, 48 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 48 } }, 104, -16, 1250, { .fields = { 96, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 8, 1995, { .fields = { 80, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 128, -8, 1050, { .fields = { 0, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 128, 56, 1098, { .fields = { 24, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 56 } }, 104, -16, 1150, { .fields = { 112, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 56 } }, 104, 40, 1150, { .fields = { 112, 184 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, 88, -16, 1200, { .fields = { 112, 56 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, 88, 40, 1200, { .fields = { 120, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 80, -16, 1250, { .fields = { 16, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 72, -16, 1250, { .fields = { 8, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, 72, 32, 1250, { .fields = { 104, 176 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, 80, 32, 1250, { .fields = { 96, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 24, 40, 949, { .fields = { 40, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 40, 938, { .fields = { 40, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 40, 40, 947, { .fields = { 48, 64 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 40, 958, { .fields = { 32, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 40, 978, { .fields = { 24, 128 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 40, 1775, { .fields = { 32, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 24 } }, 16, -120, 1341, { .fields = { 24, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 72 } }, 24, -96, 1250, { .fields = { 56, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 64 } }, 16, -24, 1250, { .fields = { 8, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, -64, 16, 1414, { .fields = { 104, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -56, 16, 1525, { .fields = { 72, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, -48, 8, 1500, { .fields = { 104, 96 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, 48, 1500, { .fields = { 88, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -40, 8, 1500, { .fields = { 96, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -40, 48, 1500, { .fields = { 64, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -24, 8, 1500, { .fields = { 112, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -24, 48, 1500, { .fields = { 96, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -8, 8, 1500, { .fields = { 80, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -8, 48, 1500, { .fields = { 80, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 8, 8, 1450, { .fields = { 88, 136 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 24, 8, 1400, { .fields = { 88, 176 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 24, 48, 1400, { .fields = { 112, 144 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, 8, 48, 1450, { .fields = { 112, 0 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 40, 24, 1350, { .fields = { 56, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 40, 72, 1350, { .fields = { 40, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 56, 32, 1300, { .fields = { 96, 216 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 56, 72, 1300, { .fields = { 24, 208 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
    { getTPage(MIST_PARKING_SPRITE_TEXTURE_DEPTH_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 80, 72 } }, 72, 48, 1250, { .fields = { 48, 72 } }, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_NEUTRAL_RGB, MIST_PARKING_SPRITE_OPAQUE },
};

/// Drawing ranges for mapped parking view 19, with decoded background image strips.
///
/// The zero-count header keeps the background; subsequent ranges index source
/// elements and the terminal record contributes none. Serialized tail bytes
/// are preserved, including their nonzero values; their role is unproven.
static SpriteBatch _gMistParkingView19SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 4, 1, 0, 0, { 3, 0 } },
    { 5, 10, 0, 0, { 2, 0 } },
    { 15, 9, 0, 0, { 4, 0 } },
    { 24, ARRAY_SIZE(_gMistParkingView19SpriteSources) - 24, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-only batch list for mapped parking view 20.
///
/// The zero-count header keeps decoded image strips; the next record ends the
/// list. Its retained `SpriteView.sources.empty` address supplies no sprites.
static SpriteBatch _gMistParkingView20SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gMistParkingSpriteViews[MIST_PARKING_VIEW_COUNT] = {
    { { .empty = _gMistParkingView1SpriteBatches }, _gMistParkingView1SpriteBatches, NULL },
    { { .elements = _gMistParkingView2SpriteSources }, _gMistParkingView2SpriteBatches, NULL },
    { { .empty = _gMistParkingView3SpriteBatches }, _gMistParkingView3SpriteBatches, NULL },
    { { .elements = _gMistParkingView4SpriteSources }, _gMistParkingView4SpriteBatches, NULL },
    { { .elements = _gMistParkingView5SpriteSources }, _gMistParkingView5SpriteBatches, NULL },
    { { .elements = _gMistParkingView6SpriteSources }, _gMistParkingView6SpriteBatches, NULL },
    { { .elements = _gMistParkingView7SpriteSources }, _gMistParkingView7SpriteBatches, NULL },
    { { .empty = _gMistParkingView8SpriteBatches }, _gMistParkingView8SpriteBatches, NULL },
    { { .empty = _gMistParkingView9SpriteBatches }, _gMistParkingView9SpriteBatches, NULL },
    { { .elements = _gMistParkingView10SpriteSources }, _gMistParkingView10SpriteBatches, NULL },
    { { .elements = _gMistParkingView11SpriteSources }, _gMistParkingView11SpriteBatches, NULL },
    { { .empty = _gMistParkingView12SpriteBatches }, _gMistParkingView12SpriteBatches, NULL },
    { { .empty = _gMistParkingView13SpriteBatches }, _gMistParkingView13SpriteBatches, NULL },
    { { .empty = _gMistParkingView14SpriteBatches }, _gMistParkingView14SpriteBatches, NULL },
    { { .empty = _gMistParkingView15SpriteBatches }, _gMistParkingView15SpriteBatches, NULL },
    { { .empty = _gMistParkingView16SpriteBatches }, _gMistParkingView16SpriteBatches, NULL },
    { { .empty = _gMistParkingView17SpriteBatches }, _gMistParkingView17SpriteBatches, NULL },
    { { .elements = _gMistParkingView18SpriteSources }, _gMistParkingView18SpriteBatches, NULL },
    { { .elements = _gMistParkingView19SpriteSources }, _gMistParkingView19SpriteBatches, NULL },
    { { .empty = _gMistParkingView20SpriteBatches }, _gMistParkingView20SpriteBatches, NULL },
};

/// CAP command slots and room actions selected by the parking interaction quads.
///
/// Numbered CAP entries identify the default dialogue resource's authored slots.
/// Room-message entries instead invoke the room script or view-9 cutscene.
enum {
    MIST_PARKING_TRIGGER_CAP_COMMAND_5          = 5,
    MIST_PARKING_TRIGGER_CAP_COMMAND_6          = 6,
    MIST_PARKING_TRIGGER_CAP_COMMAND_7          = 7,
    MIST_PARKING_TRIGGER_CAP_COMMAND_9          = 9,
    MIST_PARKING_TRIGGER_CAP_COMMAND_11         = 11,
    MIST_PARKING_TRIGGER_CAP_COMMAND_12         = 12,
    MIST_PARKING_TRIGGER_CAP_COMMAND_21         = 21,
    MIST_PARKING_TRIGGER_DEPARTURE_MENU         = 15,
    MIST_PARKING_TRIGGER_SHOP_TALK              = 18,
    MIST_PARKING_TRIGGER_ROOM_SCRIPT            = 1,
    MIST_PARKING_TRIGGER_ROOM_CUTSCENE          = 8,
    MIST_PARKING_TRIGGER_SELECT_ROOM_2          = 1,
    MIST_PARKING_TRIGGER_SELECT_ROOM_1          = 2,
    MIST_PARKING_TRIGGER_SHOOTING_GALLERY_WARPS = (1 << 4) | 1 // Current-area endpoint 1; destination arrival 1
};

/// Twelve directed logical-view boundaries shared by every parking room variant.
///
/// Source/destination pairs connect views 2/5, 2/3, 3/4, 5/6, 5/8 and 6/7.
/// Room setup binds the view coordinate and links/enables all records, including
/// the final LAST-marked boundary. Origins/corners/radii use world units;
/// normals use 4096 per unit. Link and hit state remain writable while active.
static WorldCollisionTrigger _gMistParkingViewBoundaryTriggers[12] = {
    { NULL, NULL, NULL, { 6687, -2240, -4768, 0 }, { { -114, -3264, -1385, 0 }, { 113, -3264, 1384, 0 }, { -114, 3264, -1385, 0 }, { 113, 3264, 1384, 0 } }, { 4103, 0, -337, 0 }, { 0, 0, 4096, 0 }, 3547, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6800, -1648, -4864, 0 }, { { 171, -2672, 1615, 0 }, { -172, -2672, -1616, 0 }, { 171, 2672, 1615, 0 }, { -172, 2672, -1616, 0 } }, { -4090, 0, 433, 0 }, { 0, 0, 4096, 0 }, 3124, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8543, -2160, -3041, 0 }, { { -1624, -3184, 13, 0 }, { 1625, -3184, -12, 0 }, { -1624, 3184, 13, 0 }, { 1625, 3184, -12, 0 } }, { -32, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8542, -1984, -3298, 0 }, { { 1625, -3008, -12, 0 }, { -1624, -3008, 13, 0 }, { 1625, 3008, -12, 0 }, { -1624, 3008, 13, 0 } }, { 31, 0, 4115, 0 }, { 0, 0, 4096, 0 }, 3415, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8542, -1792, -1922, 0 }, { { -1611, -2816, -229, 0 }, { 1605, -2816, 223, 0 }, { -1611, 2816, -229, 0 }, { 1605, 2816, 223, 0 } }, { 569, 0, -4058, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8543, -1648, -1953, 0 }, { { 1606, -2672, 224, 0 }, { -1610, -2672, -228, 0 }, { 1606, 2672, 224, 0 }, { -1610, 2672, -228, 0 } }, { -573, 0, 4069, 0 }, { 0, 0, 4096, 0 }, 3124, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 703, -1616, -5185, 0 }, { { 12, -2640, 1624, 0 }, { -13, -2640, -1625, 0 }, { 12, 2640, 1624, 0 }, { -13, 2640, -1625, 0 } }, { -4125, 0, 31, 0 }, { 0, 0, 4096, 0 }, 3093, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 543, -1712, -5217, 0 }, { { -12, -2736, -1624, 0 }, { 13, -2736, 1625, 0 }, { -12, 2736, -1624, 0 }, { 13, 2736, 1625, 0 } }, { 4119, 0, -33, 0 }, { 0, 0, 4096, 0 }, 3176, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4223, -1680, -1154, 0 }, { { -1624, -2704, 13, 0 }, { 1625, -2704, -12, 0 }, { -1624, 2704, 13, 0 }, { 1625, 2704, -12, 0 } }, { -33, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 5, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4223, -1728, -1474, 0 }, { { 1625, -2752, -12, 0 }, { -1624, -2752, 13, 0 }, { 1625, 2752, -12, 0 }, { -1624, 2752, 13, 0 } }, { 31, 0, 4114, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 8, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4257, -1936, -1089, 0 }, { { 2937, -2960, -204, 0 }, { -2936, -2960, 205, 0 }, { 2937, 2960, -204, 0 }, { -2936, 2960, 205, 0 } }, { 284, 0, 4086, 0 }, { 0, 0, 4096, 0 }, 4159, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4321, -2208, -801, 0 }, { { -2936, -3232, 205, 0 }, { 2937, -3232, -204, 0 }, { -2936, 3232, 205, 0 }, { 2937, 3232, -204, 0 } }, { -286, 0, -4094, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and departure quads for parking room variants 1 and 2.
///
/// Dispatch CAP commands, room actions 1/2 and the shooting-gallery warp.
/// Room setup binds and links this writable array through its LAST-marked
/// record. Origins/corners/radii use world units; normals use 4096 per unit.
/// CAP command 8 is sent to the room handler; other CAP entries run directly.
static WorldCollisionTrigger _gMistParkingEarlyChapterActionTriggers[13] = {
    { NULL, NULL, NULL, { 4960, -64, 2664, 0 }, { { 925, 0, 1264, 0 }, { -964, 0, 1259, 0 }, { 917, 0, -1274, 0 }, { -876, 0, -1247, 0 } }, { 0, 4108, 0, 0 }, { -4051, 0, -600, 0 }, 1583, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 480, -64, -4193, 0 }, { { -2310, 0, 1056, 0 }, { -2511, 0, -726, 0 }, { 2512, 0, 727, 0 }, { 2311, 0, -1055, 0 } }, { 0, 4099, 0, 0 }, { -601, 0, -4051, 0 }, 2610, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -64, 2047, 0 }, { { -2977, 0, 1622, 0 }, { -2953, 0, -1643, 0 }, { 2985, 0, 1676, 0 }, { 2945, 0, -1654, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4095, 0 }, 3415, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6976, -64, 479, 0 }, { { -865, 0, 918, 0 }, { -841, 0, -875, 0 }, { 841, 0, 876, 0 }, { 865, 0, -918, 0 } }, { 0, 4108, 0, 0 }, { 4095, 0, 0, 0 }, 1260, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_ROOM_CUTSCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9736, -96, -3553, 0 }, { { -761, 0, 2422, 0 }, { -769, 0, -2443, 0 }, { 753, 0, 2444, 0 }, { 777, 0, -2422, 0 } }, { 0, 4098, 0, 0 }, { 4075, 0, -401, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4513, -96, 3775, 0 }, { { 2427, 0, 747, 0 }, { -2438, 0, 784, 0 }, { 2440, 0, -767, 0 }, { -2426, 0, -762, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, -4075, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8639, -48, -3854, 0 }, { { 634, 0, -368, 0 }, { 623, 0, 369, 0 }, { -623, 0, -369, 0 }, { -634, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { -202, 0, -4092, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_ROOM, MIST_PARKING_TRIGGER_SELECT_ROOM_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8448, -32, -2560, 0 }, { { 634, 0, -368, 0 }, { 623, 0, 369, 0 }, { -623, 0, -369, 0 }, { -634, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { -200, 0, 4091, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_ROOM, MIST_PARKING_TRIGGER_SELECT_ROOM_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -64, -5905, 0 }, { { -1219, 0, 1639, 0 }, { -1477, 0, -281, 0 }, { 1253, 0, 1561, 0 }, { 1444, 0, -167, 0 } }, { 0, 4109, 0, 0 }, { 4051, 0, -601, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_DEPARTURE_MENU, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8432, -64, -1505, 0 }, { { -1070, 0, 252, 0 }, { -1051, 0, -453, 0 }, { 1052, 0, 455, 0 }, { 1071, 0, -251, 0 } }, { 0, 4107, 0, 0 }, { -201, 0, -4091, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_SHOP_TALK, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x30C0, -64, -4656, 0 }, { { -878, 0, 1036, 0 }, { -859, 0, -1045, 0 }, { 860, 0, 1047, 0 }, { 879, 0, -1035, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MIST_SHOOTING_GALLERY, MIST_PARKING_TRIGGER_SHOOTING_GALLERY_WARPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, -3296, 0 }, { { -3238, 0, 1632, 0 }, { -3631, 0, -1526, 0 }, { 3472, 0, 1143, 0 }, { 3079, 0, -2015, 0 } }, { 0, 4113, 0, 0 }, { -601, 0, -4051, 0 }, 3932, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8464, -32, -1408, 0 }, { { 1386, 32, -368, 0 }, { 1375, 32, 369, 0 }, { -1375, -32, -369, 0 }, { -1386, -32, 368, 0 } }, { -100, 4097, -9, 0 }, { -202, 0, -4092, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and departure quads for parking room variants 3 and 4.
///
/// Adds room CAP command 1 and uses direct command 21 at the shared location.
/// Room setup binds and links this writable array through its LAST-marked
/// record. Origins/corners/radii use world units; normals use 4096 per unit.
/// Room actions 1/2 and the shooting-gallery warp remain available.
static WorldCollisionTrigger _gMistParkingLaterChapterActionTriggers[14] = {
    { NULL, NULL, NULL, { 2879, -64, -3537, 0 }, { { -1055, 0, -1286, 0 }, { 727, 0, -1487, 0 }, { -726, 0, 1488, 0 }, { 1056, 0, 1287, 0 } }, { 0, 4099, 0, 0 }, { 4051, 0, -601, 0 }, 1664, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_ROOM_SCRIPT, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4960, -64, 2664, 0 }, { { 925, 0, 1264, 0 }, { -964, 0, 1259, 0 }, { 917, 0, -1274, 0 }, { -876, 0, -1247, 0 } }, { 0, 4108, 0, 0 }, { -4051, 0, -600, 0 }, 1583, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -16, -64, -4193, 0 }, { { -1782, 0, 1056, 0 }, { -1983, 0, -726, 0 }, { 1984, 0, 727, 0 }, { 1783, 0, -1055, 0 } }, { 0, 4107, 0, 0 }, { -601, 0, -4051, 0 }, 2111, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 800, -64, 2047, 0 }, { { -2977, 0, 1622, 0 }, { -2953, 0, -1643, 0 }, { 2985, 0, 1676, 0 }, { 2945, 0, -1654, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4095, 0 }, 3415, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6976, -64, 479, 0 }, { { -865, 0, 918, 0 }, { -841, 0, -875, 0 }, { 841, 0, 876, 0 }, { 865, 0, -918, 0 } }, { 0, 4108, 0, 0 }, { 4095, 0, 0, 0 }, 1260, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_ROOM_CUTSCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9736, -96, -3553, 0 }, { { -761, 0, 2422, 0 }, { -769, 0, -2443, 0 }, { 753, 0, 2444, 0 }, { 777, 0, -2422, 0 } }, { 0, 4098, 0, 0 }, { 4075, 0, -401, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4513, -96, 3775, 0 }, { { 2427, 0, 747, 0 }, { -2438, 0, 784, 0 }, { 2440, 0, -767, 0 }, { -2426, 0, -762, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, -4075, 0 }, 2560, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8751, -48, -3694, 0 }, { { 682, 0, -368, 0 }, { 671, 0, 369, 0 }, { -671, 0, -369, 0 }, { -682, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { -202, 0, -4092, 0 }, 773, WORLD_COLLISION_TRIGGER_ACTION_ROOM, MIST_PARKING_TRIGGER_SELECT_ROOM_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8480, -32, -2656, 0 }, { { 634, 0, -368, 0 }, { 623, 0, 369, 0 }, { -623, 0, -369, 0 }, { -634, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { -200, 0, 4091, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_ROOM, MIST_PARKING_TRIGGER_SELECT_ROOM_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -64, -5905, 0 }, { { -1219, 0, 1639, 0 }, { -1477, 0, -281, 0 }, { 1253, 0, 1561, 0 }, { 1444, 0, -167, 0 } }, { 0, 4109, 0, 0 }, { 4051, 0, -601, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_DEPARTURE_MENU, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8464, -64, -1425, 0 }, { { -1038, 0, 236, 0 }, { -1019, 0, -469, 0 }, { 1020, 0, 471, 0 }, { 1039, 0, -235, 0 } }, { 0, 4096, 0, 0 }, { -201, 0, -4091, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_SHOP_TALK, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x30C0, -64, -4656, 0 }, { { -878, 0, 1036, 0 }, { -859, 0, -1045, 0 }, { 860, 0, 1047, 0 }, { 879, 0, -1035, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MIST_SHOOTING_GALLERY, MIST_PARKING_TRIGGER_SHOOTING_GALLERY_WARPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -3232, 0 }, { { -2934, 0, 1184, 0 }, { -3263, 0, -1590, 0 }, { 2400, 0, 631, 0 }, { 2071, 0, -2143, 0 } }, { 0, 4104, 0, 0 }, { -601, 0, -4051, 0 }, 3629, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8784, -64, -1328, 0 }, { { 1530, 0, -544, 0 }, { 1519, 0, 545, 0 }, { -1519, 0, -545, 0 }, { -1530, 0, 544, 0 } }, { 0, 4097, 0, 0 }, { -202, 0, -4092, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, MIST_PARKING_TRIGGER_CAP_COMMAND_9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Authored point lights for parking room variants 1 and 3.
///
/// Contains 28 mutable records, all admitted in every view. Positions and
/// falloff radii use world units; RGB uses Q12 intensity. Room-light updates
/// parent and compose the transforms, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gMistParkingRoom1PointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x4FB0, -4600, -1460 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x4FB0, -4600, -5530 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x5F64, -5400, -5530 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x5F64, -5400, -1460 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x6F04, -6200, -1460 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x6F04, -6200, -5530 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3870, -3000, 1630 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3870, -3000, -990 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3870, -3000, -4270 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -3000, -4270 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -3000, -990 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -3000, 1630 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5870, -3000, 1630 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5870, -3000, -990 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5870, -3000, -4270 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8370, -3000, -5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8370, -3000, -2000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5143, -2876, -4752 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3428, 4033, 3661 }, { 0, 0 } }, 3221, 4203 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8510, -2320, -590 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7518, -2500, -4749 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 2048, 2457 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4257, -3000, 6007 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 4000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x289F, -2500, -4746 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 2048, 2457 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x7404, -6000, -3500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3174, 3788, 3788 }, { 0, 0 } }, 7000, 0x4650 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3052, -3000, -1460 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3052, -3000, -5530 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x4024, -3800, -5530 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x4024, -3800, -1460 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 3768, 3768 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 869, -3000, -5187 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3276, 3276 }, { 0, 0 } }, 3000, 4000 },
};

/// Point-light collection for parking room variants 1 and 3.
///
/// Borrows the complete mutable point-light array; no directional or cone lights.
static WorldCoordRoomLights _gMistParkingRoom1Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gMistParkingRoom1PointLights),
    .pointLights           = _gMistParkingRoom1PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Authored point lights for parking room variants 2 and 4.
///
/// Contains 2 mutable records, all admitted in every view. Positions and
/// falloff radii use world units; RGB uses Q12 intensity. Room-light updates
/// parent and compose the transforms, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gMistParkingRoom2PointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8510, -2000, 209 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2252, 2048 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8510, -2000, -1857 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2252, 2048 }, { 0, 0 } }, 1500, 2500 },
};

/// Point-light collection for parking room variants 2 and 4.
///
/// Borrows the complete mutable point-light array; no directional or cone lights.
static WorldCoordRoomLights _gMistParkingRoom2Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gMistParkingRoom2PointLights),
    .pointLights           = _gMistParkingRoom2PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Pierce Carradine and Jodie Bouquet resources for parking placement variants 1 and 2.
///
/// Entry IDs 143/115 match the map-owned placements; both use descriptor 0.
/// File 131 in group bases 10/20 selects the two actor packages. The final
/// AREA_PLACEMENT_END record terminates lookup. Loading/spawning borrows the
/// table and actor descriptors while their overlays remain loaded.
static AreaResource _gMistParkingActorResources[] = {
    { 143, 131, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor113100PierceCarradineTaskDescriptors },
    { 115, 131, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor213100JodieBouquetTaskDescriptors },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gMistParkingAreaVariants[MIST_PARKING_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapAkropolisMistParkingPlacements, .resources = _gMistParkingActorResources },
    [2]                    = { .placements = gMapAkropolisMistParkingPlacements, .resources = _gMistParkingActorResources },
};

/// Minimum ambient RGB levels by logical view for parking room variants 2 and 4.
///
/// Entry zero holds the count; entries 1..20 use 16 units per 8-bit colour level.
/// Views 3/4 have elevated minima (1000/1500); other views use 16. The fourth
/// colour halfword stores the equal-channel brightness and is not used by shading.
static WorldCoordRoomAmbientEntry _gMistParkingRoom2AmbientByView[MIST_PARKING_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(_gMistParkingRoom2AmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1000, 1000, 1000, 1000 } },
    { .color = { 1500, 1500, 1500, 1500 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Type-1 loaded-bank request bases for paired parking footstep cues.
enum {
    MIST_PARKING_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 17,
    MIST_PARKING_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 19
};

/// Walking, running and scripted-jump cue bases for parking surface classes 1/3.
///
/// Cue 2 uses the base, cue 1 adds one and companions add 100. Scripted jumps
/// reuse the walking pair. Surface policies borrow this record while loaded.
static WorldCollisionFootstepSounds _gMistParkingFootstepSounds = {
    .walk         = MIST_PARKING_FOOTSTEP_WALK,
    .run          = MIST_PARKING_FOOTSTEP_RUN,
    .scriptedJump = MIST_PARKING_FOOTSTEP_WALK,
};

/// Parking surface policy for classes 0 and 4..7.
///
/// Blocks probes, allows weapon impacts and applies pushback. No footstep cues.
/// The leading zero byte is retained; its role is unproven.
static WorldCollisionSurfaceProperties _gMistParkingDefaultSurfaceProperties = {
    0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL
};

/// Parking surface policy for class 1.
///
/// Blocks probes, allows weapon impacts and applies pushback. Borrows the parking footstep cues.
/// The leading zero byte is retained; its role is unproven.
static WorldCollisionSurfaceProperties _gMistParkingSurfaceClass1Properties = {
    0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gMistParkingFootstepSounds
};

/// Parking surface policy for class 2.
///
/// Blocks probes, allows weapon impacts and applies pushback. No footstep cues.
/// The leading zero byte is retained; its role is unproven.
static WorldCollisionSurfaceProperties _gMistParkingSurfaceClass2Properties = {
    0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL
};

/// Parking surface policy for class 3.
///
/// Blocks probes, allows weapon impacts and applies pushback. Borrows the parking footstep cues.
/// The leading zero byte is retained; its role is unproven.
static WorldCollisionSurfaceProperties _gMistParkingSurfaceClass3Properties = {
    0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gMistParkingFootstepSounds
};

WorldCollisionSurfaceProperties* gMistParkingSurfacePropertiesByClass[MIST_PARKING_SURFACE_CLASS_COUNT] = {
    &_gMistParkingDefaultSurfaceProperties,
    &_gMistParkingSurfaceClass1Properties,
    &_gMistParkingSurfaceClass2Properties,
    &_gMistParkingSurfaceClass3Properties,
    &_gMistParkingDefaultSurfaceProperties,
    &_gMistParkingDefaultSurfaceProperties,
    &_gMistParkingDefaultSurfaceProperties,
    &_gMistParkingDefaultSurfaceProperties,
};

s32 gShopRefillDisplayQuantityFixed = 0;

const EquipmentWeaponSupply* gShopRefillSupply = NULL;

Task* gRoomCutsceneSoundTask = NULL;

s32 D_mist_parking_8019531C = 0;

Task* gMistParkingCutsceneModelTask = NULL;

Task* gMistParkingPlayerHeadAimTask = NULL;

MistParkingPrizeAnnouncementState gMistParkingPrizeAnnouncement = { 0 };

MistParkingHeadAimHandle gMistParkingShopPlayerHeadAimHandle = { 0 };

MistParkingShopTalkState gMistParkingShopTalk = { 0, 0, 0, 0 };

RoomCutsceneRec gMistParkingCommandCutscene = { 0 };

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

/// Draws one parking-area row of three capsule glows.
///
/// Borrows three contiguous, word-aligned `worldEndpointPairs`, drawing pairs
/// 0, 1 and 2 in order. A negative GTE flag at either endpoint skips
/// that capsule; the other pairs are still drawn. The signed low halfword of
/// `radiusScale` gives each endpoint's pixel radius as that value times 64
/// divided by camera Z / 4, which must be nonzero for accepted projections.
/// `rgbNibbles` bits 8..11, 4..7 and 0..3 supply RGB channels scaled by 16;
/// odd animation frames set intensity bit 3 in each channel.
///
/// Requires the current view, an initialized scratch stack with room for one
/// `OverlayPointPairScratch`, and a current ordering table and packet arena.
/// Queues up to eighteen additive Gouraud quads plus their blend commands.
/// Retains no endpoint pointer; packets use the frame arena until GPU completion.
static inline void _mistParkingDrawGlowRow(const SVECTOR worldEndpointPairs[3][2], s32 radiusScale, s32 rgbNibbles)
{
    _glowDrawCapsule(worldEndpointPairs[0], radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpointPairs[1], radiusScale, rgbNibbles);
    _glowDrawCapsule(worldEndpointPairs[2], radiusScale, rgbNibbles);
}

void mistParkingDrawGlowsTask(Task* unused)
{
    enum {
        MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE       = 0x200, // Pixel radius = scale * 64 / (camera Z / 4)
        MIST_PARKING_GLOW_GREY_RGB_NIBBLES           = 0x444, // Each channel is 64, with bit 3 set on odd animation frames
        MIST_PARKING_GLOW_POINT_PULSE_RATE           = 0x60,  // 4096 angle units per turn, per animation frame
        MIST_PARKING_GLOW_DIAMOND_RADIUS_SCALE       = 0x100, // Pixel half-extent = scale * 32 / (camera Z / 4)
        MIST_PARKING_GLOW_VIEW7_DIAMOND_RADIUS_SCALE = 0xE0,
        MIST_PARKING_GLOW_DISC_RADIUS_SCALE          = 0x40   // Outer pixel radius = scale * 64 / (camera Z / 4)
    };

    u8 mappedViewIndex;

    // Select by mapped camera index; logical views can use a different ordering.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[0], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 18:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 7:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[6], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[7], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[9], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[10], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawDiamond(&_gMistParkingGlowWorldPoints.pointCenter, MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_VIEW7_DIAMOND_RADIUS_SCALE);
            break;
        case 20:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 9:
            _glowDrawPulsingDisc(&_gMistParkingGlowWorldPoints.pointCenter, MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_DISC_RADIUS_SCALE);
            break;
        case 10:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 11:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 14:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[7], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 15:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[0], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 16:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[5], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[9], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[11], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[12], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 5:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[2], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
        case 6:
        case 19:
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[5], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _mistParkingDrawGlowRow(&_gMistParkingGlowWorldPoints.capsuleEndpoints[8], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[11], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[12], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawDiamond(&_gMistParkingGlowWorldPoints.pointCenter, MIST_PARKING_GLOW_POINT_PULSE_RATE, MIST_PARKING_GLOW_DIAMOND_RADIUS_SCALE);
            break;
        case 8:
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[3], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            _glowDrawCapsule(_gMistParkingGlowWorldPoints.capsuleEndpoints[4], MIST_PARKING_GLOW_CAPSULE_RADIUS_SCALE, MIST_PARKING_GLOW_GREY_RGB_NIBBLES);
            break;
    }
}

#include "../../shared/effects/glow_draw_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"

#include "../../shared/effects/glow_draw_capsule.inc.c"
