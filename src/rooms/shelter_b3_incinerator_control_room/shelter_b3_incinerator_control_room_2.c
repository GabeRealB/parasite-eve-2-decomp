#include "rooms/shelter_b3_incinerator_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "shelter_b3_incinerator_control_room_private.h"

#include "actors/actor_142600.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

/// Two capsule endpoint pairs visible in mapped views 2 and 3.
///
/// Each row supplies two signed world-coordinate points, borrowed during drawing
/// from the loaded room overlay. Both views draw the rows in this order.
static SVECTOR _gShelterB3IncineratorControlRoomViews2And3GlowCapsules[2][2] = {
    { { -1543, -2132, -2753, 0 }, { -365, -2132, -2753, 0 } },
    { { -1543, -2042, -2677, 0 }, { -365, -2042, -2677, 0 } },
};

/// Fixed capsule endpoints and a cyan glow centre for mapped views 4, 5, 6 and 8.
///
/// Positions are signed world-coordinate units in room-overlay storage.
/// Capsule reads start at even endpoint indices 0 through 16 and consume two
/// points. The cyan centre supplies the view-4 diamond and view-8 disc.
static struct {
    SVECTOR capsuleEndpoints[18]; // Nine consecutive two-point capsule spans
    SVECTOR cyanCenter;           // Centre of the pulsing cyan diamond/disc
} _gShelterB3IncineratorControlRoomGlowPoints = {
    {
        { -3967, -562, -778, 0 },
        { -3967, -562, -70, 0 },
        { -3967, -562, 1570, 0 },
        { -3967, -562, 2278, 0 },
        { -3967, -680, -778, 0 },
        { -3967, -680, -70, 0 },
        { -3967, -680, 1570, 0 },
        { -3967, -680, 2278, 0 },
        { -7049, -562, 1570, 0 },
        { -7049, -562, 2278, 0 },
        { -7049, -680, 1570, 0 },
        { -7049, -680, 2278, 0 },
        { -7049, -562, -778, 0 },
        { -7049, -562, -70, 0 },
        { -7049, -680, -778, 0 },
        { -7049, -680, -70, 0 },
        { -5786, -2150, 3559, 0 },
        { -5228, -2150, 3559, 0 },
    },
    { -4439, -1161, -1751, 0 },
};

u8* gShelterB3IncineratorControlRoomViewMaps[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount gShelterB3IncineratorControlRoomViewCounts[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_COUNT] = {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT,
};

/// Endpoint sound cues from the loaded control-room sound bank.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_ARRIVAL_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 8),
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_HALL_ARRIVAL_SOUND        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 6),
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_HALL_DEPARTURE_SOUND      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 5),
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_HALL_BLOCKED_SOUND        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 7),
};

DirectionWarpEntry gShelterB3IncineratorControlRoomWarpEntries[SHELTER_B3_INCINERATOR_CONTROL_ROOM_WARP_COUNT] = {
    { .player         = { { .word = 2048 }, -5333, 0, 2984 },
      .companion      = { { .word = 2048 }, -5333, 0, 2984 },
      .arrivalSound   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_ARRIVAL_SOUND,
      .departureSound = DIRECTION_WARP_SOUND_NONE,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 5,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player         = { { .word = 3072 }, 820, 0, -2540 },
      .companion      = { { .word = 3072 }, 820, 0, -3100 },
      .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
      .departureSound = DIRECTION_WARP_SOUND_NONE,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 3,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = GAME_FLAG_MAP_MARK_RESERVOIR_1BF },
    { .player         = { { .word = 0 }, -6048, 0, -3020 },
      .companion      = { { .word = 256 }, -5200, 0, -3020 },
      .arrivalSound   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_HALL_ARRIVAL_SOUND,
      .departureSound = SHELTER_B3_INCINERATOR_CONTROL_ROOM_HALL_DEPARTURE_SOUND,
      .blockedSound   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_HALL_BLOCKED_SOUND,
      .initialView    = 4,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = GAME_FLAG_MAP_MARK_B3_INCINERATOR_CONTROL_DOOR },
    { .player         = { { .word = 2048 }, -5333, 0, 2984 },
      .companion      = { { .word = 2048 }, -5333, 0, 2984 },
      .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
      .departureSound = DIRECTION_WARP_SOUND_NONE,
      .blockedSound   = DIRECTION_WARP_SOUND_NONE,
      .initialView    = 6,
      .flags          = DIRECTION_WARP_FLAG_NONE,
      .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB3IncineratorControlRoomCollisionGridNormals[10] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_normals.inc"
};

/// Collision vertices in signed world-coordinate units.
static SVECTOR _gShelterB3IncineratorControlRoomCollisionGridVerts[27] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_verts.inc"
};

/// Indexed collision quads using this room's surface classes 0, 1 and 3.
static WorldCollisionGridFace _gShelterB3IncineratorControlRoomCollisionGridFaces[19] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_faces.inc"
};

/// Signed face-index lists ending at -1, retaining all 68 authored halfwords.
static s16 _gShelterB3IncineratorControlRoomCollisionGridCells[68] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB3IncineratorControlRoomCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..67
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B3_INCINERATOR_CONTROL_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB3IncineratorControlRoomCollisionGridCells[(halfwordOffset)])
/// X-major 4-by-2 cell directory; NULL cells contain no faces.
static s16* _gShelterB3IncineratorControlRoomCollisionGridTable[8] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_table.inc"
};
#undef SHELTER_B3_INCINERATOR_CONTROL_ROOM_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB3IncineratorControlRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB3IncineratorControlRoomCollisionGridNormals,
    .vertices    = _gShelterB3IncineratorControlRoomCollisionGridVerts,
    .faces       = _gShelterB3IncineratorControlRoomCollisionGridFaces,
    .cellFaceIds = _gShelterB3IncineratorControlRoomCollisionGridTable,
    .xBias       = 6742,
    .zBias       = 3500,
    .cellCountX  = 4,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB3IncineratorControlRoomCollisionGridFaces),
};

ViewCamera gShelterB3IncineratorControlRoomViewCameras[SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 1000, 0x7530, 0 } }, 541 },
    { { { { 607, 0, 4050 }, { 1065, 3951, -159 }, { -3907, 1077, 586 } }, { -3319, 1397, 3178 } }, 212 },
    { { { { 607, 0, 4050 }, { 1065, 3951, -159 }, { -3907, 1077, 586 } }, { -3319, 1397, 3178 } }, 212 },
    { { { { -3911, 0, -1216 }, { -52, 4092, 170 }, { 1214, 178, -3907 } }, { 6542, 1367, -3406 } }, 230 },
    { { { { 3970, 0, -1006 }, { -3, 4095, -12 }, { 1006, 13, 3970 } }, { 6416, 1197, 2136 } }, 230 },
    { { { { -2634, 0, 3136 }, { 128, 4092, 108 }, { -3133, 168, -2632 } }, { 4213, 1411, -2685 } }, 498 },
    { { { { 4052, 0, -598 }, { -38, 4087, -263 }, { 597, 266, 4043 } }, { 6547, 1635, 2355 } }, 1104 },
    { { { { -1234, 0, -3905 }, { -938, 3975, 296 }, { 3791, 984, -1198 } }, { 5487, 1464, 1249 } }, 230 },
};

/// Empty mapped-view-1 batch list selecting decoded-image background strips.
///
/// The zero-count first record selects the background; the second ends the list.
/// The source slot retains this address without reading it as sprite elements.
static SpriteBatch _gShelterB3IncineratorControlRoomView1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// Empty mapped-view-2 batch list selecting decoded-image background strips.
///
/// The zero-count first record selects the background; the second ends the list.
/// The source slot retains this address without reading it as sprite elements.
static SpriteBatch _gShelterB3IncineratorControlRoomView2SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// GPU texture addresses and neutral RGB modulation for the room's view sprites.
///
/// The eight-bit texture page starts at VRAM word X 960, Y 0, with blend mode 0.
/// Its 256-colour CLUT starts at word X 0, Y 255; RGB 128 leaves colours unchanged.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE = getTPage(1, 0, 960, 0),
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT         = getClut(0, 255),
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB  = 128,
};

/// Thirteen depth-sorted texture rectangles drawn over mapped view 3's background.
///
/// XY and dimensions are pixels relative to the view centre; UV is in texels.
/// Depth is an unscaled OT sorting value. The room owns this writable array;
/// its batch list selects the full extent without a source sentinel.
static SpriteSource _gShelterB3IncineratorControlRoomView3SpriteSources[13] = {
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 72 } }, 8, -80, 1558, { .fields = { 120, 144 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 32 } }, 0, -40, 1788, { .fields = { 120, 216 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 32 } }, -8, -40, 1863, { .fields = { 112, 216 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 32 } }, -16, -40, 1707, { .fields = { 104, 216 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -16, -64, 1794, { .fields = { 80, 0 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -8, -64, 1783, { .fields = { 88, 216 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, 0, -64, 1517, { .fields = { 96, 216 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, 0, -80, 1828, { .fields = { 96, 240 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -8, -80, 1703, { .fields = { 88, 240 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -16, -80, 1729, { .fields = { 80, 24 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 48, 72 } }, 16, -80, 1725, { .fields = { 72, 144 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 40, 72 } }, 64, -80, 1725, { .fields = { 88, 0 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 56, 72 } }, 104, -80, 1725, { .fields = { 72, 72 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
};

/// Background selector, full sprite range and terminator for mapped view 3.
///
/// The zero-count first record requests image strips. The next record draws all
/// thirteen source elements. The list remains mutable in room-overlay storage.
static SpriteBatch _gShelterB3IncineratorControlRoomView3SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB3IncineratorControlRoomView3SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// Eighteen depth-sorted texture rectangles drawn over mapped view 4's background.
///
/// XY and dimensions are pixels relative to the view centre; UV is in texels.
/// The three nonempty batches select ranges [0,14), [14,16) and [16,18).
/// Depth is an unscaled OT sorting value; the room owns this writable array.
static SpriteSource _gShelterB3IncineratorControlRoomView4SpriteSources[18] = {
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 64, 72 } }, -160, -32, 1586, { .fields = { 48, 160 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 16, 72 } }, -32, -32, 1586, { .fields = { 112, 160 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 24, 1586, { .fields = { 64, 120 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, -32, 1586, { .fields = { 72, 120 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -40, -16, 1339, { .fields = { 120, 232 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, 8, 1344, { .fields = { 72, 136 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 40, 72 } }, -96, -32, 1586, { .fields = { 48, 0 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, -32, 1258, { .fields = { 88, 232 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, -16, 1333, { .fields = { 112, 232 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, -16, 1249, { .fields = { 104, 232 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 8, 1339, { .fields = { 80, 120 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 8, 1254, { .fields = { 80, 232 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -48, 24, 1310, { .fields = { 80, 136 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 24, 1256, { .fields = { 72, 232 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 16, 48 } }, -32, -24, 1399, { .fields = { 64, 72 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 8, 48 } }, -40, -24, 1339, { .fields = { 80, 72 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 40, 80 } }, 88, -64, 706, { .fields = { 88, 0 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 40, 80 } }, 88, 16, 713, { .fields = { 88, 80 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
};

/// Background selector, three sprite ranges and terminator for mapped view 4.
///
/// Ranges cover fourteen, two and two source elements after a zero-count record
/// selecting image strips. Serialized trailing bytes 1 and 2 have no established
/// reader and remain uninterpreted. The room owns this writable list.
static SpriteBatch _gShelterB3IncineratorControlRoomView4SpriteBatches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 14, .field_6 = { 1, 0 } },
    { .firstSprite = 14, .spriteCount = 2, .field_6 = { 2, 0 } },
    { .firstSprite = 16, .spriteCount = ARRAY_SIZE(_gShelterB3IncineratorControlRoomView4SpriteSources) - 16 },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// Two vertically adjoining texture rectangles drawn over mapped view 5's background.
///
/// Each is 32 by 72 pixels, at X -120 and Y 16 or -56 relative to the view centre.
/// Both use unscaled OT depth 753. The room owns this writable source array.
static SpriteSource _gShelterB3IncineratorControlRoomView5SpriteSources[2] = {
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 32, 72 } }, -120, 16, 753, { .fields = { 96, 72 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
    { SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_CLUT, { .fields = { 32, 72 } }, -120, -56, 753, { .fields = { 96, 0 } }, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SPRITE_NEUTRAL_RGB, 0 },
};

/// Background selector, full sprite range and terminator for mapped view 5.
///
/// The zero-count first record requests image strips; the next selects both
/// source elements. The list remains mutable in room-overlay storage.
static SpriteBatch _gShelterB3IncineratorControlRoomView5SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB3IncineratorControlRoomView5SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// Empty mapped-view-6 batch list selecting decoded-image background strips.
///
/// The zero-count first record selects the background; the second ends the list.
/// The source slot retains this address without reading it as sprite elements.
static SpriteBatch _gShelterB3IncineratorControlRoomView6SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// Empty mapped-view-7 batch list selecting decoded-image background strips.
///
/// The zero-count first record selects the background; the second ends the list.
/// The source slot retains this address without reading it as sprite elements.
static SpriteBatch _gShelterB3IncineratorControlRoomView7SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

/// Empty mapped-view-8 batch list selecting decoded-image background strips.
///
/// The zero-count first record selects the background; the second ends the list.
/// The source slot retains this address without reading it as sprite elements.
static SpriteBatch _gShelterB3IncineratorControlRoomView8SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0 },
};

SpriteView gShelterB3IncineratorControlRoomSpriteViews[SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_COUNT] = {
    { { .empty = _gShelterB3IncineratorControlRoomView1SpriteBatches }, _gShelterB3IncineratorControlRoomView1SpriteBatches, NULL },
    { { .empty = _gShelterB3IncineratorControlRoomView2SpriteBatches }, _gShelterB3IncineratorControlRoomView2SpriteBatches, NULL },
    { { .elements = _gShelterB3IncineratorControlRoomView3SpriteSources }, _gShelterB3IncineratorControlRoomView3SpriteBatches, NULL },
    { { .elements = _gShelterB3IncineratorControlRoomView4SpriteSources }, _gShelterB3IncineratorControlRoomView4SpriteBatches, NULL },
    { { .elements = _gShelterB3IncineratorControlRoomView5SpriteSources }, _gShelterB3IncineratorControlRoomView5SpriteBatches, NULL },
    { { .empty = _gShelterB3IncineratorControlRoomView6SpriteBatches }, _gShelterB3IncineratorControlRoomView6SpriteBatches, NULL },
    { { .empty = _gShelterB3IncineratorControlRoomView7SpriteBatches }, _gShelterB3IncineratorControlRoomView7SpriteBatches, NULL },
    { { .empty = _gShelterB3IncineratorControlRoomView8SpriteBatches }, _gShelterB3IncineratorControlRoomView8SpriteBatches, NULL },
};

/// Eight mutable point lights contributing in every control-room view.
///
/// Positions and inner/outer falloff distances use world-coordinate units; RGB
/// intensities have 12 fractional bits. Coordinate parenting, composition and
/// query attenuation are updated in place for the loaded room's lifetime.
static WorldCoordPointLight _gShelterB3IncineratorControlRoomPointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -591, -1844, -2881 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3295, -3263, 141 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3734, 3791, 3811 }, { 0, 0 } }, 1000, 0x2D03 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6412, -2764, -1784 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2370, 2309, 2426 }, { 0, 0 } }, 1000, 5940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4412, -2882, 2029 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2128, 2126, 2128 }, { 0, 0 } }, 1000, 2477 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6584, 160, 1519 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2553, 2457, 2457 }, { 0, 0 } }, 1000, 1297 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4412, -321, 1461 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 3981 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6679, 0, -121 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 2100 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4352, 0, -121 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1000, 2100 },
};

WorldCoordRoomLights gShelterB3IncineratorControlRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB3IncineratorControlRoomPointLights),
    .pointLights           = _gShelterB3IncineratorControlRoomPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB3IncineratorControlRoomViewBoundaryTriggers[SHELTER_B3_INCINERATOR_CONTROL_ROOM_VIEW_BOUNDARY_COUNT] = {
    { .origin       = { -5537, -1536, 687, 0 },
      .vertices     = { { -2101, -2016, -224, 0 }, { 2099, -2016, 222, 0 }, { -2101, 2016, -224, 0 }, { 2099, 2016, 222, 0 } },
      .normal       = { 435, 0, -4103, 0 },
      .facingNormal = { 0, 0, ONE, 0 },
      .radius       = 2918,
      .control      = 0,
      .parameter0   = 4,
      .parameter1   = 5,
      .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin       = { -5456, -1536, 592, 0 },
      .vertices     = { { 2067, -2016, 219, 0 }, { -2069, -2016, -221, 0 }, { 2067, 2016, 219, 0 }, { -2069, 2016, -221, 0 } },
      .normal       = { -435, 0, 4078, 0 },
      .facingNormal = { 0, 0, ONE, 0 },
      .radius       = 2896,
      .control      = 0,
      .parameter0   = 5,
      .parameter1   = 4,
      .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin       = { -4305, -1248, -3633, 0 },
      .vertices     = { { -318, -2016, -2024, 0 }, { 315, -2016, 2021, 0 }, { -318, 2016, -2024, 0 }, { 315, 2016, 2021, 0 } },
      .normal       = { 4050, 0, -635, 0 },
      .facingNormal = { 0, 0, ONE, 0 },
      .radius       = 2873,
      .control      = 0,
      .parameter0   = 3,
      .parameter1   = 4,
      .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY },
    { .origin       = { -4145, -1280, -3648, 0 },
      .vertices     = { { 298, -2016, 1911, 0 }, { -301, -2016, -1914, 0 }, { 298, 2016, 1911, 0 }, { -301, 2016, -1914, 0 } },
      .normal       = { -4053, 0, 633, 0 },
      .facingNormal = { 0, 0, ONE, 0 },
      .radius       = 2792,
      .control      = 0,
      .parameter0   = 4,
      .parameter1   = 3,
      .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST },
};

/// Placement key and decimal file number for base scene file 142600.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SCENE_ENTRY_ID    = 101,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SCENE_FILE_NUMBER = 426,
};

/// Base actor-142600 scene resource for placement variant 1, followed by END.
///
/// Group base 10 and decimal file number 426 select file 142600 at low index 0.
/// The loader queues it even though the placement list is empty; no placed actor
/// is spawned. Its descriptor table belongs to the separately loaded actor package.
static AreaResource _gShelterB3IncineratorControlRoomArrivalSceneResources[2] = {
    { .entryId        = SHELTER_B3_INCINERATOR_CONTROL_ROOM_SCENE_ENTRY_ID,
      .fileNumber     = SHELTER_B3_INCINERATOR_CONTROL_ROOM_SCENE_FILE_NUMBER,
      .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
      .taskIndex      = 0,
      .taskTable      = gActor142600TaskDescriptors },
    { .entryId = AREA_PLACEMENT_END },
};

/// Sentinel-only actor placement list for area layout variant 1.
///
/// Both placement spawning and additional-file loading stop on the first record.
/// The variant's separate resource list can still load its base scene resource.
static AreaPlacement _gShelterB3IncineratorControlRoomEmptyPlacements[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB3IncineratorControlRoomAreaVariants[SHELTER_B3_INCINERATOR_CONTROL_ROOM_AREA_VARIANT_COUNT] = {
    { NULL, NULL },
    { _gShelterB3IncineratorControlRoomEmptyPlacements, _gShelterB3IncineratorControlRoomArrivalSceneResources },
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

/// Packed stair selectors and room-local CAP commands carried by action triggers.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_DESCEND           = 0x100,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_SURFACE_CLASS     = 2,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_STEPS             = 6,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_CAP_COMMAND       = 4,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_CAP_COMMAND = 5,
};

WorldCollisionTrigger gShelterB3IncineratorControlRoomRoom1ActionTriggers[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_1_ACTION_COUNT] = {
    { .origin       = { 1201, -48, -2720, 0 },
      .vertices     = { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } },
      .normal       = { 0, 4098, 0, 0 },
      .facingNormal = { -ONE, 0, 0, 0 },
      .radius       = 1093,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_FACING | SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_DESCEND,
      .parameter0   = (SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_SURFACE_CLASS << 4) | SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_STEPS,
      .parameter1   = (1024 >> 4),
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { 1792, -304, -2704, 0 },
      .vertices     = { { 0, -2560, -1328, 0 }, { 0, 2560, -1328, 0 }, { 0, -2560, 1328, 0 }, { 0, 2560, 1328, 0 } },
      .normal       = { -4099, 0, 0, 0 },
      .facingNormal = { -ONE, 0, 0, 0 },
      .radius       = 2873,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC,
      .parameter0   = GAME_AREA_SHELTER_B4_LOWER_SEWER,
      .parameter1   = (2 << 4) | 1,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -5903, -48, -3136, 0 },
      .vertices     = { { 752, 0, -400, 0 }, { 752, 0, 400, 0 }, { -752, 0, -400, 0 }, { -752, 0, 400, 0 } },
      .normal       = { 0, 4115, 0, 0 },
      .facingNormal = { 0, 0, ONE, 0 },
      .radius       = 851,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
      .parameter0   = GAME_AREA_SHELTER_B3_ELEVATOR_HALL,
      .parameter1   = (3 << 4) | 1,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -5440, -64, 3136, 0 },
      .vertices     = { { -1424, 0, -384, 0 }, { 1424, 0, -384, 0 }, { -1424, 0, 384, 0 }, { 1424, 0, 384, 0 } },
      .normal       = { 0, 4113, 0, 0 },
      .facingNormal = { 0, 0, -ONE, 0 },
      .radius       = 1470,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
      .parameter0   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_CAP_COMMAND,
      .parameter1   = 0,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -4576, -64, -1584, 0 },
      .vertices     = { { -400, 0, -752, 0 }, { 400, 0, -752, 0 }, { -400, 0, 752, 0 }, { 400, 0, 752, 0 } },
      .normal       = { 0, 4115, 0, 0 },
      .facingNormal = { -ONE, 0, 0, 0 },
      .radius       = 851,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
      .parameter0   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_COMMAND,
      .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST },
};

WorldCollisionTrigger gShelterB3IncineratorControlRoomRoom2ActionTriggers[SHELTER_B3_INCINERATOR_CONTROL_ROOM_ROOM_2_ACTION_COUNT] = {
    { .origin       = { 720, -48, -2720, 0 },
      .vertices     = { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } },
      .normal       = { 0, 4098, 0, 0 },
      .facingNormal = { -ONE, 0, 0, 0 },
      .radius       = 1093,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
      .parameter0   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_STAIR_CAP_COMMAND,
      .parameter1   = 0,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { 1792, -304, -2704, 0 },
      .vertices     = { { 0, -2560, -1328, 0 }, { 0, 2560, -1328, 0 }, { 0, -2560, 1328, 0 }, { 0, 2560, 1328, 0 } },
      .normal       = { -4099, 0, 0, 0 },
      .facingNormal = { -ONE, 0, 0, 0 },
      .radius       = 2873,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC,
      .parameter0   = GAME_AREA_SHELTER_B4_LOWER_SEWER,
      .parameter1   = (2 << 4) | 1,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -5600, -48, -3136, 0 },
      .vertices     = { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } },
      .normal       = { 0, 4098, 0, 0 },
      .facingNormal = { 0, 0, ONE, 0 },
      .radius       = 1093,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
      .parameter0   = GAME_AREA_SHELTER_B3_ELEVATOR_HALL,
      .parameter1   = (3 << 4) | 1,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -4608, -64, -1504, 0 },
      .vertices     = { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } },
      .normal       = { 0, 4098, 0, 0 },
      .facingNormal = { -ONE, 0, 0, 0 },
      .radius       = 1093,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
      .parameter0   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_COMMAND,
      .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -5504, -64, 3168, 0 },
      .vertices     = { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } },
      .normal       = { 0, 4098, 0, 0 },
      .facingNormal = { 0, 0, -ONE, 0 },
      .radius       = 1093,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
      .parameter0   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_CAP_COMMAND,
      .parameter1   = 0,
      .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin       = { -4288, -64, -2080, 0 },
      .vertices     = { { 832, 0, -1008, 0 }, { 832, 0, 1008, 0 }, { -832, 0, -1008, 0 }, { -832, 0, 1008, 0 } },
      .normal       = { 0, 4104, 0, 0 },
      .facingNormal = { 0, 0, -ONE, 0 },
      .radius       = 1305,
      .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
      .parameter0   = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_COMMAND,
      .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
      .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST },
};

WorldCollisionOccluder gShelterB3IncineratorControlRoomOccluders[SHELTER_B3_INCINERATOR_CONTROL_ROOM_OCCLUDER_COUNT] = {
    { .origin   = { -1312, -95, 368, 0 },
      .vertices = { { -2752, 4576, -2416, 0 }, { 2752, 4576, 2416, 0 }, { -2752, -4576, -2416, 0 }, { 2752, -4576, 2416, 0 } },
      .normal   = { -2705, 0, 3080, 0 },
      .radius   = 5860,
      .flags    = 1 | WORLD_COLLISION_OCCLUDER_LAST }, // Authored low bit is retained; the sight query does not interpret it.
};

/// Cue-2 bases in the loaded type-1 footstep bank.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SHARED_WALK_BASE     = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SHARED_RUN_BASE      = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SURFACE_2_WALK_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SURFACE_2_RUN_BASE   = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SURFACE_2_STAIR_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

/// Walk, run and stair-climb footstep cues shared by surface classes 1 and 3.
///
/// Stair climbing reuses the walk base. Animation cue 1 adds one; companion cues
/// add another 100. Surface records borrow this room-owned record while loaded.
static WorldCollisionFootstepSounds _gShelterB3IncineratorControlRoomSharedFootstepSounds = {
    .walk         = SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SHARED_WALK_BASE,
    .run          = SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SHARED_RUN_BASE,
    .scriptedJump = SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SHARED_WALK_BASE,
};

/// Distinct walk, run and stair-climb footstep cues for surface class 2.
///
/// Animation cue 1 adds one to its type-1 base; companion cues add another 100.
/// The class-2 surface record borrows this room-owned record while loaded.
static WorldCollisionFootstepSounds _gShelterB3IncineratorControlRoomSurface2FootstepSounds = {
    .walk         = SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SURFACE_2_WALK_BASE,
    .run          = SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SURFACE_2_RUN_BASE,
    .scriptedJump = SHELTER_B3_INCINERATOR_CONTROL_ROOM_FOOTSTEP_SURFACE_2_STAIR_BASE,
};

/// Probe-blocking, pushback and weapon-impact policy for classes 0 and 4..7.
///
/// No footstep cues are selected. The leading byte's role is unproven and its
/// stored zero is retained. The class directory borrows this room-owned record.
static WorldCollisionSurfaceProperties _gShelterB3IncineratorControlRoomSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Probe-blocking, pushback, weapon-impact and shared footstep policy for class 1.
///
/// The leading byte's role is unproven and its stored zero is retained.
/// The class directory and footstep pointer require the room to stay loaded.
static WorldCollisionSurfaceProperties _gShelterB3IncineratorControlRoomSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB3IncineratorControlRoomSharedFootstepSounds,
};

/// Probe-blocking, pushback, weapon-impact and distinct footstep policy for class 2.
///
/// The leading byte's role is unproven and its stored zero is retained.
/// The class directory and footstep pointer require the room to stay loaded.
static WorldCollisionSurfaceProperties _gShelterB3IncineratorControlRoomSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB3IncineratorControlRoomSurface2FootstepSounds,
};

/// Probe-blocking, pushback and shared footstep policy without weapon impacts for class 3.
///
/// The leading byte's role is unproven and its stored zero is retained.
/// The class directory and footstep pointer require the room to stay loaded.
static WorldCollisionSurfaceProperties _gShelterB3IncineratorControlRoomSurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB3IncineratorControlRoomSharedFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB3IncineratorControlRoomSurfacePropertiesByClass[SHELTER_B3_INCINERATOR_CONTROL_ROOM_SURFACE_CLASS_COUNT] = {
    &_gShelterB3IncineratorControlRoomSilentSurfaceProperties,
    &_gShelterB3IncineratorControlRoomSurface1Properties,
    &_gShelterB3IncineratorControlRoomSurface2Properties,
    &_gShelterB3IncineratorControlRoomSurface3Properties,
    &_gShelterB3IncineratorControlRoomSilentSurfaceProperties,
    &_gShelterB3IncineratorControlRoomSilentSurfaceProperties,
    &_gShelterB3IncineratorControlRoomSilentSurfaceProperties,
    &_gShelterB3IncineratorControlRoomSilentSurfaceProperties,
};

/// Placement variants and map-mark policy applied by the incinerator arrival.
enum {
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_ARRIVAL_VARIANT                = 2,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_SEWER_BOUNTY_NIGHTMARE_VARIANT = 7,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_MAP_MARK_CLEAR                 = 0,
    SHELTER_B3_INCINERATOR_CONTROL_ROOM_MAP_MARK_SET                   = 1,
};

AreaApplyRec gShelterB3IncineratorControlRoomArrivalSavedUpdates[5] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_ARRIVAL_VARIANT, AREA_APPLY_MODE_ALWAYS | SHELTER_B3_INCINERATOR_CONTROL_ROOM_MAP_MARK_CLEAR },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, SHELTER_B3_INCINERATOR_CONTROL_ROOM_ARRIVAL_VARIANT, AREA_APPLY_MODE_ALWAYS | SHELTER_B3_INCINERATOR_CONTROL_ROOM_MAP_MARK_CLEAR },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SEWER_BOUNTY_NIGHTMARE_VARIANT, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B3_INCINERATOR_CONTROL_ROOM_MAP_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SEWER_BOUNTY_NIGHTMARE_VARIANT, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_B3_INCINERATOR_CONTROL_ROOM_MAP_MARK_SET },
    { AREA_APPLY_END, 0, 0, 0 },
};

Task* gRoomCutsceneSoundTask = NULL;

RoomCutsceneRec gShelterB3IncineratorControlRoomTelephoneCutscene;

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

/// Draws the fixed pair of vertically stacked capsule glows shared by views 4, 5 and 8.
///
/// Both capsules use the signed low halfword of `radiusScale`; each endpoint's
/// pixel radius is that scale times 64 divided by camera Z / 4. `rgbNibbles`
/// packs red, green and blue in bits 8..11, 4..7 and 0..3, scaled by 16;
/// odd animation frames add 8 to each channel. Negative projection flags
/// reject a capsule; accepted endpoint depths must be nonzero.
///
/// Requires a composed view matrix, an initialized scratch stack and the current
/// frame's ordering table and packet arena. Queues the lower capsule before the
/// upper one; additive packets remain in that arena until GPU completion.
static inline void _shelterB3IncineratorControlRoomDrawStackedGlows(s32 radiusScale, s32 rgbNibbles)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_STACKED_GLOW_LOWER_PAIR_START = 0,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_STACKED_GLOW_UPPER_PAIR_START = 4,
    };

    _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[SHELTER_B3_INCINERATOR_CONTROL_ROOM_STACKED_GLOW_LOWER_PAIR_START], radiusScale, rgbNibbles);
    _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[SHELTER_B3_INCINERATOR_CONTROL_ROOM_STACKED_GLOW_UPPER_PAIR_START], radiusScale, rgbNibbles);
}

void shelterB3IncineratorControlRoomDrawViewGlowsTask(Task* task)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE = 0x180, // Pixel radius = scale * 64 / (camera Z / 4).
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY             = 0x111, // RGB nibbles; channel intensities 16 before flicker.
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_WARM                 = 0x421, // RGB nibbles; channel intensities 64, 32, 16 before flicker.
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CYAN_PULSE_RATE      = 0x60,  // 4096 angle units per turn, per animation frame.
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CYAN_RADIUS_SCALE    = 0x80,  // Diamond half-extent uses scale * 32 / depth; disc radius uses scale * 64 / depth.
    };
    u8 mappedViewIndex;

    // Select the visible world-point sets while preserving their packet order.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2:
        case 3:
            _glowDrawCapsule(_gShelterB3IncineratorControlRoomViews2And3GlowCapsules[0], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(_gShelterB3IncineratorControlRoomViews2And3GlowCapsules[1], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            break;
        case 4:
            _shelterB3IncineratorControlRoomDrawStackedGlows(SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[8], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[10], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawDiamond(&_gShelterB3IncineratorControlRoomGlowPoints.cyanCenter, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CYAN_PULSE_RATE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CYAN_RADIUS_SCALE);
            break;
        case 5:
            _shelterB3IncineratorControlRoomDrawStackedGlows(SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[2], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[6], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[12], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[14], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[16], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_WARM);
            break;
        case 6:
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[12], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawCapsule(&_gShelterB3IncineratorControlRoomGlowPoints.capsuleEndpoints[14], SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            break;
        case 8:
            _shelterB3IncineratorControlRoomDrawStackedGlows(SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CAPSULE_RADIUS_SCALE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_DIM_GREY);
            _glowDrawPulsingDisc(&_gShelterB3IncineratorControlRoomGlowPoints.cyanCenter, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CYAN_PULSE_RATE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_GLOW_CYAN_RADIUS_SCALE);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"
