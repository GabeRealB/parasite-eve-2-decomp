#include "rooms/dryfield_water_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_03700.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
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

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/effects/water_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/water_hole.h"
#include "../../shared/room/room_variants.h"

static s32  _roomVariantResolveWaterHole(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _waterHoleWaterTask(Task* task);

/// The water-hole room's single cone light and adjacent uninterpreted image bytes.
///
/// The light stays writable for coordinate composition and attenuation queries;
/// its borrowed address is valid only while the room overlay is loaded. The
/// remaining bytes are outside the live light count; their original layout and
/// relationship to the light are unproven.
typedef struct {
    WorldCoordSpotLight coneLights[1];   // One mutable cone light, contributing in every view
    u8                  unknown_6C[540]; // Uninterpreted image bytes; purpose and internal boundaries unproven
} _DryfieldWaterHoleSpotLightStorage;
STATIC_ASSERT_SIZEOF(_DryfieldWaterHoleSpotLightStorage, 648);

static WorldCollisionGrid         _gDryfieldWaterHoleCollisionGrid;
static WorldCollisionOccluder     _gDryfieldWaterHoleOccluders[2];
static WorldCollisionTrigger      _gDryfieldWaterHoleViewBoundaryTriggers[14];
static WorldCollisionTrigger      _gDryfieldWaterHoleLitActionTriggers[7];
static WorldCollisionTrigger      _gDryfieldWaterHoleUnlitActionTriggers[6];
static WorldCoordRoomAmbientEntry _gDryfieldWaterHoleLitAmbientByView[9];
static WorldCoordRoomLights       _gDryfieldWaterHoleLitLights;
static WorldCoordRoomLights       _gDryfieldWaterHoleUnlitLights;

static s32 _dryfieldWaterHoleRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldWaterHoleCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldWaterHoleIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _dryfieldWaterHolePlaySoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Room receiver entries for transition, key-item, action, command and sound messages.
///
/// The final `TASK_MESSAGE_TABLE_END` entry ends dispatch. The initialized room
/// task borrows this table and its callbacks for its lifetime.
static TaskMessageEntry _gDryfieldWaterHoleRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveWaterHole },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldWaterHoleRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldWaterHoleIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldWaterHoleCommandMessage },
    { ROOM_MESSAGE_SOUND, _dryfieldWaterHolePlaySoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-192 recipe for the persistent daytime water-surface task.
///
/// Room initialization spawns this one recipe with zero arguments. The callback
/// and room-owned surface resources must remain loaded until the task exits.
static TaskDesc _gDryfieldWaterHoleWaterTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    _waterHoleWaterTask,
    { .value = 0 },
};

/// Two world-space water patches at the shared session height, followed by a Y=-1 sentinel.
///
/// Each frame draws 64 columns in each of two strips per patch. At most 256
/// accepted quads and their draw modes consume 0x3000 bytes of the borrowed actor arena.
static WaterHoleSurface _gWaterHoleSurfaces[3] = {
    { 4000, -2000, 8000, 2000, WATER_HOLE_SURFACE_HEIGHT },
    { 10000, -4000, 13000, 2000, WATER_HOLE_SURFACE_HEIGHT },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Six world-space beam endpoint pairs enabled by underpass switch 1.
///
/// Consecutive pairs form two beams for each view group: endpoints 0..3 for
/// views 3/4, 4..7 for views 4/6, and 8..11 for view 7. Coordinates use game
/// units. Call-site order preserves which endpoint receives the tapered end.
static SVECTOR _gDryfieldWaterHoleSwitchBeamEndpoints[12] = {
    { 9500, -1835, -100, 0 },
    { 10500, -1835, -100, 0 },
    { 9500, -1750, -75, 0 },
    { 10500, -1750, -75, 0 },
    { 14300, -1835, -3900, 0 },
    { 15300, -1835, -3900, 0 },
    { 14300, -1750, -3925, 0 },
    { 15300, -1750, -3925, 0 },
    { 18300, -1835, -2120, 0 },
    { 19300, -1835, -2120, 0 },
    { 18300, -1750, -2095, 0 },
    { 19300, -1750, -2095, 0 },
};

/// Previous emitting positions of player model parts 14 and 17.
///
/// Initialization samples their existing composed translations; active splash
/// updates replace XYZ after comparing motion. Coordinates narrow to signed
/// halfwords. Pause and out-of-water updates freeze history; each vector's
/// fourth halfword remains zero. The two-entry initialized storage is room-local.
static SVECTOR _gDryfieldWaterHolePreviousPartPositions[2] = { 0 };

WorldCollisionRoomResources gDryfieldWaterHoleRoomCollisionResources[4] = {
    { &_gDryfieldWaterHoleCollisionGrid, _gDryfieldWaterHoleViewBoundaryTriggers, _gDryfieldWaterHoleLitActionTriggers, _gDryfieldWaterHoleOccluders },
    { &_gDryfieldWaterHoleCollisionGrid, _gDryfieldWaterHoleViewBoundaryTriggers, _gDryfieldWaterHoleUnlitActionTriggers, _gDryfieldWaterHoleOccluders },
    { &_gDryfieldWaterHoleCollisionGrid, _gDryfieldWaterHoleViewBoundaryTriggers, _gDryfieldWaterHoleLitActionTriggers, _gDryfieldWaterHoleOccluders },
    { &_gDryfieldWaterHoleCollisionGrid, _gDryfieldWaterHoleViewBoundaryTriggers, _gDryfieldWaterHoleUnlitActionTriggers, _gDryfieldWaterHoleOccluders },
};

/// Room 2's eight logical-view resource indices with underpass switch 1 off.
///
/// Index by logical view - 1; view 1 shares the plan resource, views 2..8 use
/// the switch-off images, cameras and sprite lists. No terminator is stored.
static u8 _gDryfieldWaterHoleUnlitViewMap[8] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
};

/// Room 3's eight logical-view resource indices with switch 1 and flag 0x53 set.
///
/// Index by logical view - 1; views 7/8 select resources 10/9 instead of 7/8.
/// The other entries retain the switch-on mapping. No terminator is stored.
static u8 _gDryfieldWaterHoleAlternateLitViewMap[8] = {
    1,
    2,
    3,
    4,
    5,
    6,
    10,
    9,
};

/// Room 4's eight logical-view resource indices with switch 1 off and flag 0x53 set.
///
/// Index by logical view - 1; views 7/8 select resources 20/19 instead of 17/18.
/// The other entries retain the switch-off mapping. No terminator is stored.
static u8 _gDryfieldWaterHoleAlternateUnlitViewMap[8] = {
    1,
    12,
    13,
    14,
    15,
    16,
    20,
    19,
};

u8* gDryfieldWaterHoleRoomViewMaps[4] = {
    gViewIdentityMap,
    _gDryfieldWaterHoleUnlitViewMap,
    _gDryfieldWaterHoleAlternateLitViewMap,
    _gDryfieldWaterHoleAlternateUnlitViewMap,
};

enum { DRYFIELD_WATER_HOLE_LOGICAL_VIEW_COUNT = 8 };

ViewCount gDryfieldWaterHoleRoomViewCounts[4] = {
    DRYFIELD_WATER_HOLE_LOGICAL_VIEW_COUNT,
    DRYFIELD_WATER_HOLE_LOGICAL_VIEW_COUNT,
    DRYFIELD_WATER_HOLE_LOGICAL_VIEW_COUNT,
    DRYFIELD_WATER_HOLE_LOGICAL_VIEW_COUNT,
};

WorldCoordRoomLighting gDryfieldWaterHoleRoomLighting[4] = {
    { &_gDryfieldWaterHoleLitLights, _gDryfieldWaterHoleLitAmbientByView },
    { &_gDryfieldWaterHoleUnlitLights, NULL },
    { &_gDryfieldWaterHoleLitLights, _gDryfieldWaterHoleLitAmbientByView },
    { &_gDryfieldWaterHoleUnlitLights, NULL },
};

enum { DRYFIELD_WATER_HOLE_UNDERPASS_ARRIVAL_MAP_FLAG = 462 };

DirectionWarpEntry gDryfieldWaterHoleWarpEntries[3] = {
    { { { .word = 0 }, 7400, 0, -1350 }, { 0, 0, 0, 0 }, { { .word = 0 }, 7400, 0, -1350 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 3), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 3), DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x57C0, -534, -3076 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x57C0, -534, -3076 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 2), SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 1), DIRECTION_WARP_SOUND_NONE, 8, DIRECTION_WARP_FLAG_NONE, DRYFIELD_WATER_HOLE_UNDERPASS_ARRIVAL_MAP_FLAG },
    { { { .word = 1024 }, 4680, 0, -954 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 4680, 0, -954 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_WATER },
};

/// Room-axis normals for the water-hole collision grid, with unit length 4096.
static SVECTOR _gDryfieldWaterHoleCollisionGridNormals[10] = {
#include "assets/dryfield_water_hole_collision_02CA0_normals.inc"
};

/// Room-space vertices for the water-hole collision grid, in signed game units.
static SVECTOR _gDryfieldWaterHoleCollisionGridVerts[54] = {
#include "assets/dryfield_water_hole_collision_02CA0_verts.inc"
};

/// Indexed collision faces carrying this room's surface classes 0, 2, 3 and 4.
static WorldCollisionGridFace _gDryfieldWaterHoleCollisionGridFaces[23] = {
#include "assets/dryfield_water_hole_collision_02CA0_faces.inc"
};

/// Packed signed face-index lists; each cell list ends at WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldWaterHoleCollisionGridCells[92] = {
#include "assets/dryfield_water_hole_collision_02CA0_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gDryfieldWaterHoleCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define DRYFIELD_WATER_HOLE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldWaterHoleCollisionGridCells[(halfwordOffset)])
/// Twelve cell-list pointers in X-major order, with two Z cells per X row.
static s16* _gDryfieldWaterHoleCollisionGridTable[12] = {
#include "assets/dryfield_water_hole_collision_02CA0_table.inc"
};
#undef DRYFIELD_WATER_HOLE_CELL_FACE_LIST_AT_OFFSET

/// The collision mesh and cell partition shared by all four water-hole room variants.
///
/// Six X cells by two Z cells, each 4000 game units wide, start at room-space
/// (4000, -5000). The view coordinate is bound on room setup; the record and
/// mesh pools remain writable and live only while this room overlay is loaded.
static WorldCollisionGrid _gDryfieldWaterHoleCollisionGrid = {
    NULL,
    _gDryfieldWaterHoleCollisionGridNormals,
    _gDryfieldWaterHoleCollisionGridVerts,
    _gDryfieldWaterHoleCollisionGridFaces,
    _gDryfieldWaterHoleCollisionGridTable,
    -4000,
    5000,
    6,
    2,
    4000,
    ARRAY_SIZE(_gDryfieldWaterHoleCollisionGridFaces),
};

ViewCamera gDryfieldWaterHoleViewCameras[26] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x34B7, 0x7530, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -0x34B7, 0x7530, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -3186, 0, -2573 }, { -25, 4095, 31 }, { 2573, 40, -3186 } }, { -8630, 1300, -20 } }, 246 },
    { { { { -2620, 0, 3148 }, { 219, 4086, 182 }, { -3140, 285, -2613 } }, { -9960, 1360, 430 } }, 246 },
    { { { { -2395, 0, -3322 }, { -237, 4085, 171 }, { 3313, 293, -2389 } }, { -9120, 1360, 430 } }, 246 },
    { { { { -3186, 0, -2573 }, { -25, 4095, 31 }, { 2573, 40, -3186 } }, { -8630, 1300, -20 } }, 246 },
    { { { { -2620, 0, 3148 }, { 219, 4086, 182 }, { -3140, 285, -2613 } }, { -9960, 1360, 430 } }, 246 },
    { { { { -2395, 0, -3322 }, { -237, 4085, 171 }, { 3313, 293, -2389 } }, { -9120, 1360, 430 } }, 246 },
};

/// Texture encodings and neutral RGB shared by mapped views 3 and 4's sprites.
enum {
    DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_DEPTH_8BIT = 1,
    DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960   = getTPage(DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_DEPTH_8BIT, 0, 960, 0),
    DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896   = getTPage(DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_DEPTH_8BIT, 0, 896, 0),
    DRYFIELD_WATER_HOLE_SPRITE_CLUT               = getClut(0, 255),
    DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR      = 128,
};

/// Mapped view 1's empty sprite list, retaining decoded background strips.
///
/// The zero-count first record selects image strips; SPRITE_BATCH_END stops
/// the next record. The source slot borrows this list but selects no sprites.
static SpriteBatch _gDryfieldWaterHoleView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's empty sprite list, retaining decoded background strips.
///
/// The zero-count first record selects image strips; SPRITE_BATCH_END stops
/// the next record. The source slot borrows this list but selects no sprites.
static SpriteBatch _gDryfieldWaterHoleView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 3's 21 textured foreground rectangles at three sorting depths.
///
/// Positions and dimensions are pixels relative to the view origin, UVs are
/// texels, and depth is an unscaled OT sorting value. Neutral RGB and zero
/// command flags select opaque modulation. Room sprite packets borrow these
/// mutable sources while this overlay is loaded.
static SpriteSource _gDryfieldWaterHoleView3SpriteSources[21] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 64, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 40, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 88, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 64, 40 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 56, 160 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 200 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 48, 232 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 104, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 64, 120 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 64, 80 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 112, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 96, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 3's three sprite ranges following decoded background strips.
///
/// Ranges [0,8), [8,16) and [16,21) cover its source array, followed by
/// SPRITE_BATCH_END. All start visible and included in cached packet processing;
/// the final serialized byte pair in each record has an unproven role.
static SpriteBatch _gDryfieldWaterHoleView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 8, 0, 0, { 2, 0 } },
    { 16, ARRAY_SIZE(_gDryfieldWaterHoleView3SpriteSources) - 16, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 4's 33 textured foreground rectangles at two sorting depths.
///
/// Positions and dimensions are pixels relative to the view origin, UVs are
/// texels, and depth is an unscaled OT sorting value. Rectangles sample two
/// adjacent 8-bit texture pages with neutral RGB and opaque modulation. Room
/// sprite packets borrow these mutable sources while this overlay is loaded.
static SpriteSource _gDryfieldWaterHoleView4SpriteSources[33] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 32, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 88, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 88, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 120 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 88, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 32, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 72, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 48, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 64, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 40, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 24, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 120, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 112, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 112, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 64, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 88, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 56, 224 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 104, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 112, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 168 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 104, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 96, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 8, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 88, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 48, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 4's two sprite ranges following decoded background strips.
///
/// Ranges [0,15) and [15,33) cover its source array, followed by SPRITE_BATCH_END.
/// Both start visible and included in cached packet processing; the final
/// serialized byte pair in each record has an unproven role.
static SpriteBatch _gDryfieldWaterHoleView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, ARRAY_SIZE(_gDryfieldWaterHoleView4SpriteSources) - 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

enum { DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED = 0 };

/// Mapped view 5's eighteen sprite rectangles, all sorted at depth 625.
///
/// Positions and sizes are pixels relative to the drawing origin; texture
/// coordinates are texels. The room's sprite descriptor borrows this mutable
/// array while the overlay is loaded; sources contain no terminator.
static SpriteSource _gDryfieldWaterHoleView5SpriteSources[18] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 168 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 112, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 112, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 128 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 128 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 5's complete sprite range, preceded by a background-strip selector.
///
/// The zero-count first record retains decoded image strips. The next range
/// selects all eighteen sources, then SPRITE_BATCH_END ends the mutable list.
static SpriteBatch _gDryfieldWaterHoleView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterHoleView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's nineteen sprite rectangles in three sorting-depth groups.
///
/// Source ranges 0..8, 9..16 and 17..18 sort at depths 1375, 2000 and 3000.
/// Positions/sizes are pixels and texture coordinates are texels. The mutable
/// sources have no terminator and live with the loaded room overlay.
static SpriteSource _gDryfieldWaterHoleView6SpriteSources[19] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 72, 120 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 72, 160 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 72, 40 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 64, 80 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 64, 200 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 88, 184 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 104, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 88, 224 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 64, 224 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 96, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 88 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 88, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 80, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 6's background-strip selector and three complete source ranges.
///
/// Ranges cover 9, 8 and 2 source elements; the first zero count keeps image
/// strips and the final SPRITE_BATCH_END ends the mutable list. Serialized
/// trailing values 1/2 in the first two ranges retain their unproven meaning.
static SpriteBatch _gDryfieldWaterHoleView6SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 8, 0, 0, { 2, 0 } },
    { 17, ARRAY_SIZE(_gDryfieldWaterHoleView6SpriteSources) - 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 7's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. The source slot
/// borrows this mutable list's address but selects no source elements.
static SpriteBatch _gDryfieldWaterHoleView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 8's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. The source slot
/// borrows this mutable list's address but selects no source elements.
static SpriteBatch _gDryfieldWaterHoleView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 9's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Room 3's logical
/// view 8 selects this resource; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 10's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Room 3's logical
/// view 7 selects this resource; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped resource 11's stored empty sprite list, retaining background strips.
///
/// No daytime room map selects this slot. A zero-count first record precedes
/// SPRITE_BATCH_END; the source slot retains this list but supplies no sprites.
static SpriteBatch _gDryfieldWaterHoleView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 12's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Switch-off rooms
/// select it for logical view 2; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 13's twenty-one sprite rectangles in three sorting-depth groups.
///
/// Source ranges 0..7, 8..15 and 16..20 sort at depths 1375, 2000 and 875.
/// Positions/sizes are pixels and texture coordinates are texels. Switch-off
/// logical view 3 borrows these mutable sources while the overlay is loaded.
static SpriteSource _gDryfieldWaterHoleView13SpriteSources[21] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 40 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 104, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 88, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 56, 168 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 64, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 48, 128 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 104, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 56, 88 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 72, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 56, 72 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 88, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 13's background-strip selector and three complete source ranges.
///
/// Ranges cover 8, 8 and 5 source elements, followed by SPRITE_BATCH_END.
/// The zero first count keeps image strips. Serialized trailing values 2/1
/// retain their unproven meaning; the list stays mutable with the room overlay.
static SpriteBatch _gDryfieldWaterHoleView13SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 2, 0 } },
    { 8, 8, 0, 0, { 0, 0 } },
    { 16, ARRAY_SIZE(_gDryfieldWaterHoleView13SpriteSources) - 16, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 14's thirty-three sprite rectangles in two sorting-depth groups.
///
/// Sources 0..14 sort at depth 625 and 15..32 at 1250. Positions/sizes are
/// pixels and texture coordinates are texels. Switch-off logical view 4 borrows
/// these mutable sources while the overlay is loaded; no terminator is stored.
static SpriteSource _gDryfieldWaterHoleView14SpriteSources[33] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 80, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 112, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 72, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 56, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 40, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 40, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 32, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 32, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 16, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 16, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 112, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 16, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 0, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 112, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 96, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 72, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 96, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 80, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 120 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 64, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 48, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 48, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 0, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_896, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 168 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 14's background-strip selector and two complete source ranges.
///
/// Ranges cover 15 and 18 elements, followed by SPRITE_BATCH_END. The zero
/// first count keeps image strips. The first range's trailing 1 is uninterpreted.
static SpriteBatch _gDryfieldWaterHoleView14SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, ARRAY_SIZE(_gDryfieldWaterHoleView14SpriteSources) - 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 15's eighteen sprite rectangles, all sorted at depth 625.
///
/// Positions/sizes are pixels and texture coordinates are texels. Switch-off
/// logical view 5 borrows these mutable sources while the room overlay is
/// loaded; the source array has no terminator.
static SpriteSource _gDryfieldWaterHoleView15SpriteSources[18] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 80, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 80, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 128 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 168 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 96, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 96, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 168 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 15's complete source range and decoded-background-strip selector.
///
/// The zero-count first record keeps image strips. All eighteen sources are
/// selected by the next range, followed by SPRITE_BATCH_END.
static SpriteBatch _gDryfieldWaterHoleView15SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldWaterHoleView15SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 16's nineteen sprite rectangles in three sorting-depth groups.
///
/// Source ranges 0..7, 8..16 and 17..18 sort at depths 2000, 1375 and 3000.
/// Positions/sizes are pixels and texture coordinates are texels. Switch-off
/// logical view 6 borrows these mutable sources while the overlay is loaded.
static SpriteSource _gDryfieldWaterHoleView16SpriteSources[19] = {
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 72, 80 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 80, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 96, 224 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 72, 248 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 120 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 88, 80 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 64, 160 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 64, 120 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 192 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 104, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 48 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 80, 40 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 88, 184 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 96, 0 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 72, 224 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_WATER_HOLE_SPRITE_TEXTURE_PAGE_960, DRYFIELD_WATER_HOLE_SPRITE_CLUT, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 144 } }, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_NEUTRAL_COLOR, DRYFIELD_WATER_HOLE_SPRITE_OPAQUE_MODULATED },
};

/// Mapped view 16's background-strip selector and three complete source ranges.
///
/// Ranges cover 8, 9 and 2 source elements, followed by SPRITE_BATCH_END.
/// The zero first count keeps image strips. Serialized trailing values 1/2
/// retain their unproven meaning; the list stays mutable with the room overlay.
static SpriteBatch _gDryfieldWaterHoleView16SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 9, 0, 0, { 2, 0 } },
    { 17, ARRAY_SIZE(_gDryfieldWaterHoleView16SpriteSources) - 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 17's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Room 2's logical
/// view 7 selects this resource; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView17SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 18's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Room 2's logical
/// view 8 selects this resource; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView18SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 19's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Room 4's logical
/// view 8 selects this resource; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView19SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 20's empty sprite list, retaining decoded background strips.
///
/// A zero-count first record is followed by SPRITE_BATCH_END. Room 4's logical
/// view 7 selects this resource; its source slot supplies no source elements.
static SpriteBatch _gDryfieldWaterHoleView20SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldWaterHoleSpriteViews[20] = {
    { { .empty = _gDryfieldWaterHoleView1SpriteBatches }, _gDryfieldWaterHoleView1SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView2SpriteBatches }, _gDryfieldWaterHoleView2SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView3SpriteSources }, _gDryfieldWaterHoleView3SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView4SpriteSources }, _gDryfieldWaterHoleView4SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView5SpriteSources }, _gDryfieldWaterHoleView5SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView6SpriteSources }, _gDryfieldWaterHoleView6SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView7SpriteBatches }, _gDryfieldWaterHoleView7SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView8SpriteBatches }, _gDryfieldWaterHoleView8SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView9SpriteBatches }, _gDryfieldWaterHoleView9SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView10SpriteBatches }, _gDryfieldWaterHoleView10SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView11SpriteBatches }, _gDryfieldWaterHoleView11SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView12SpriteBatches }, _gDryfieldWaterHoleView12SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView13SpriteSources }, _gDryfieldWaterHoleView13SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView14SpriteSources }, _gDryfieldWaterHoleView14SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView15SpriteSources }, _gDryfieldWaterHoleView15SpriteBatches, NULL },
    { { .elements = _gDryfieldWaterHoleView16SpriteSources }, _gDryfieldWaterHoleView16SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView17SpriteBatches }, _gDryfieldWaterHoleView17SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView18SpriteBatches }, _gDryfieldWaterHoleView18SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView19SpriteBatches }, _gDryfieldWaterHoleView19SpriteBatches, NULL },
    { { .empty = _gDryfieldWaterHoleView20SpriteBatches }, _gDryfieldWaterHoleView20SpriteBatches, NULL },
};

/// Seven opposing view-boundary pairs connecting logical views 2 through 9.
///
/// Each pair covers an adjacent view change; positions/radii use room game
/// units and normals use Q12. The final record carries LAST and is included.
/// Room setup borrows and mutates every record's coordinate, links, flags and
/// hit latch; storage must stay loaded until the boundary list is cleared.
static WorldCollisionTrigger _gDryfieldWaterHoleViewBoundaryTriggers[14] = {
    { NULL, NULL, NULL, { 6197, -1152, -1034, 0 }, { { 150, -2176, -1012, 0 }, { -150, -2176, 1012, 0 }, { 150, 2176, -1012, 0 }, { -150, 2176, 1012, 0 } }, { 4053, 0, 599, 0 }, { 0, 0, ONE, 0 }, 2401, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6304, -1104, -1024, 0 }, { { -154, -2128, 1004, 0 }, { 144, -2128, -1022, 0 }, { -154, 2128, 1004, 0 }, { 144, 2128, -1022, 0 } }, { -4056, 0, -597, 0 }, { 0, 0, ONE, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9467, -1248, -1028, 0 }, { { -345, -2272, -963, 0 }, { 345, -2272, 963, 0 }, { -345, 2272, -963, 0 }, { 345, 2272, 963, 0 } }, { 3869, 0, -1388, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9632, -1200, -1026, 0 }, { { 340, -2224, 960, 0 }, { -350, -2224, -966, 0 }, { 340, 2224, 960, 0 }, { -350, 2224, -966, 0 } }, { -3865, 0, 1383, 0 }, { 0, 0, ONE, 0 }, 2442, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A9F, -1088, -1922, 0 }, { { 919, -2112, 432, 0 }, { -929, -2112, -443, 0 }, { 919, 2112, 432, 0 }, { -929, 2112, -443, 0 } }, { -1765, 0, 3723, 0 }, { 0, 0, ONE, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A7E, -1168, -1858, 0 }, { { -927, -2192, -439, 0 }, { 923, -2192, 434, 0 }, { -927, 2192, -439, 0 }, { 923, 2192, 434, 0 } }, { 1748, 0, -3710, 0 }, { 0, 0, ONE, 0 }, 2415, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x319F, -1248, -2945, 0 }, { { 435, -2272, -931, 0 }, { -441, -2272, 919, 0 }, { 435, 2272, -931, 0 }, { -441, 2272, 919, 0 } }, { 3717, 0, 1758, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x323F, -1136, -2976, 0 }, { { -441, -2160, 919, 0 }, { 435, -2160, -931, 0 }, { -441, 2160, 919, 0 }, { 435, 2160, -931, 0 } }, { -3706, 0, -1755, 0 }, { 0, 0, ONE, 0 }, 2387, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x415F, -1200, -2912, 0 }, { { 198, -2224, 1001, 0 }, { -202, -2224, -1006, 0 }, { 198, 2224, 1001, 0 }, { -202, 2224, -1006, 0 } }, { -4027, 0, 801, 0 }, { 0, 0, ONE, 0 }, 2442, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40FF, -1328, -2977, 0 }, { { -205, -2352, -1008, 0 }, { 195, -2352, 999, 0 }, { -205, 2352, -1008, 0 }, { 195, 2352, 999, 0 } }, { 4021, 0, -803, 0 }, { 0, 0, ONE, 0 }, 2560, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x531F, -1376, -2978, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1023, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1023, 0 } }, { -4116, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2598, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x523F, -1408, -2978, 0 }, { { 51, -2432, -1022, 0 }, { -50, -2432, 1022, 0 }, { 51, 2432, -1022, 0 }, { -50, 2432, 1022, 0 } }, { 4093, 0, 200, 0 }, { 0, 0, ONE, 0 }, 2635, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x5800, -1216, -6144, 0 }, { { 0, -2176, -1024, 0 }, { 0, -2176, 1024, 0 }, { 0, 2176, -1024, 0 }, { 0, 2176, 1024, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2401, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x58C0, -1344, -6144, 0 }, { { 0, -2176, 1024, 0 }, { 0, -2176, -1024, 0 }, { 0, 2176, 1024, 0 }, { 0, 2176, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2401, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Action selectors and packed parameters for exits, stairs and interactions.
///
/// Warp bytes pair the current area's departure slot with destination arrival
/// slot 2. Stair parameters select surface row 1 plus a low-nibble step count;
/// yaw bytes store 16-angle-unit steps, with 4096 angle units per turn.
enum {
    DRYFIELD_WATER_HOLE_DRIVEWAY_WARP_SLOTS    = (1 << 4) | 2,
    DRYFIELD_WATER_HOLE_UNDERPASS_WARP_SLOTS   = (2 << 4) | 2,
    DRYFIELD_WATER_HOLE_STAIR_DESCEND          = 0x100,
    DRYFIELD_WATER_HOLE_STAIR_DESCENT_STEPS    = 0x80 | (1 << 4) | 2,
    DRYFIELD_WATER_HOLE_STAIR_ASCENT_STEPS     = 0x80 | (1 << 4) | 5,
    DRYFIELD_WATER_HOLE_STAIR_DESCENT_YAW      = 3072 / 16,
    DRYFIELD_WATER_HOLE_STAIR_ASCENT_YAW       = 1024 / 16,
    DRYFIELD_WATER_HOLE_EXAMINE_COMMAND        = 2,
    DRYFIELD_WATER_HOLE_LIT_REGION_CAP_COMMAND = 6,
};

/// Switch-on rooms 1/3's driveway/underpass exits, stairs and CAP interactions.
///
/// Includes water examination through room command 2 and CAP command 6's
/// placement-entry-37 tally interaction. LAST ends setup after the final record.
/// Geometry uses room game units and Q12 normals; coordinate/link/flag/hit
/// fields are mutable and must stay loaded until the action list is cleared.
static WorldCollisionTrigger _gDryfieldWaterHoleLitActionTriggers[7] = {
    { NULL, NULL, NULL, { 7776, -48, -1584, 0 }, { { -671, 0, -336, 0 }, { 672, 0, -336, 0 }, { -671, 0, 336, 0 }, { 672, 0, 336, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 749, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DRIVEWAY, DRYFIELD_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5890, -1504, -2992, 0 }, { { 0, 1408, -1199, 0 }, { 0, 1408, 1200, 0 }, { 0, -1408, -1199, 0 }, { 0, -1408, 1200, 0 } }, { -4098, 0, 0, 0 }, { -ONE, 0, 0, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_DRYFIELD_UNDERPASS, DRYFIELD_WATER_HOLE_UNDERPASS_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5840, -416, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | DRYFIELD_WATER_HOLE_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_WATER_HOLE_STAIR_DESCENT_STEPS, DRYFIELD_WATER_HOLE_STAIR_DESCENT_YAW, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, DRYFIELD_WATER_HOLE_STAIR_ASCENT_STEPS, DRYFIELD_WATER_HOLE_STAIR_ASCENT_YAW, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_WATER_HOLE_EXAMINE_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7776, -64, -2208, 0 }, { { -1055, 0, 80, 0 }, { 1056, 0, 80, 0 }, { -1055, 0, 880, 0 }, { 1056, 0, 880, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, ONE, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DRIVEWAY, DRYFIELD_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4000, -64, -4352, 0 }, { { 960, 0, 225, 0 }, { 960, 0, 1312, 0 }, { -1152, 0, 225, 0 }, { -1152, 0, 1312, 0 } }, { 0, 4110, 0, 0 }, { -ONE, 0, 0, 0 }, 1745, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_WATER_HOLE_LIT_REGION_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Switch-off rooms 2/4's driveway/underpass exits, stairs and water examination.
///
/// Omits the switch-on CAP-6 region and uses a different near-driveway quad.
/// The final driveway record carries LAST and is included. Geometry uses room
/// game units and Q12 normals; runtime coordinate/link/flag/hit fields remain
/// mutable, with storage kept loaded until the action list is cleared.
static WorldCollisionTrigger _gDryfieldWaterHoleUnlitActionTriggers[6] = {
    { NULL, NULL, NULL, { 7776, -48, -1584, 0 }, { { -671, 0, -336, 0 }, { 672, 0, -336, 0 }, { -671, 0, 336, 0 }, { 672, 0, 336, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 749, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DRIVEWAY, DRYFIELD_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5890, -1504, -2992, 0 }, { { 0, 1408, -1199, 0 }, { 0, 1408, 1200, 0 }, { 0, -1408, -1199, 0 }, { 0, -1408, 1200, 0 } }, { -4098, 0, 0, 0 }, { -ONE, 0, 0, 0 }, 1846, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_DRYFIELD_UNDERPASS, DRYFIELD_WATER_HOLE_UNDERPASS_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5840, -416, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING | DRYFIELD_WATER_HOLE_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_WATER_HOLE_STAIR_DESCENT_STEPS, DRYFIELD_WATER_HOLE_STAIR_DESCENT_YAW, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 729, WORLD_COLLISION_TRIGGER_ACTION_FACING, DRYFIELD_WATER_HOLE_STAIR_ASCENT_STEPS, DRYFIELD_WATER_HOLE_STAIR_ASCENT_YAW, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_WATER_HOLE_EXAMINE_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7776, -64, -2208, 0 }, { { -1055, 0, 176, 0 }, { 1056, 0, 176, 0 }, { -1055, 0, 1040, 0 }, { 1056, 0, 1040, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 1481, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_DRIVEWAY, DRYFIELD_WATER_HOLE_DRIVEWAY_WARP_SLOTS, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space sight-blocking quads shared by all four water-hole rooms.
///
/// Positions/radii use game units and normals use Q12. LAST includes the
/// second quad before ending setup. Runtime links and enable flags remain
/// mutable; both records must stay loaded until their sight list is cleared.
static WorldCollisionOccluder _gDryfieldWaterHoleOccluders[2] = {
    { NULL, NULL, { 8544, -1520, -3664, 0 }, { { -1536, -2224, -1840, 0 }, { -1536, 2224, -1840, 0 }, { 1536, -2224, 1840, 0 }, { 1536, 2224, 1840, 0 } }, { -3150, 0, 2629, 0 }, 3268, 1, 0 },
    { NULL, NULL, { 0x34E0, -1568, -176, 0 }, { { -1504, -2224, -1936, 0 }, { -1504, 2224, -1936, 0 }, { 1504, -2224, 1936, 0 }, { 1504, 2224, 1936, 0 } }, { -3237, 0, 2514, 0 }, 3308, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Six switch-on point lights shared by room variants 1 and 3 in every view.
///
/// Positions and falloff distances are room game units; RGB uses 12 fractional
/// bits. Coordinate composition and attenuation queries mutate the entries;
/// their borrowed addresses expire with this overlay.
static WorldCoordPointLight _gDryfieldWaterHoleLitPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9250, -1650, -600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x374A, -1650, -3400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, 1664, 3367 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4A12, -1650, -2600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3622, 3423, 3247 }, { 0, 0 } }, 1518, 3158 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x57C0, -4125, -2264 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -1290, -663 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2B64, -1277, -2296 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
};

/// Switch-on cone-light storage shared by room variants 1 and 3.
///
/// Only the leading cone light is published. Its position and falloff distances
/// are room game units, RGB and the axis are Q12, and the full opening is 113
/// in 4096 units per turn. Composition and attenuation mutate the light; its
/// address expires with this overlay. The following 540 bytes have no established
/// runtime consumer, internal layout or containing-object relationship.
static _DryfieldWaterHoleSpotLightStorage _gDryfieldWaterHoleLitSpotLightStorage = {
    {
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -ONE, 0, 0 }, { 0, 0, ONE }, { 0, ONE, 0 } }, { 7394, -3968, -1496 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3604, 2867 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 3000, 6000, 113 },
    },
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x06,
        0x00,
        0x00,
        0x00,
        0x08,
        0x9A,
        0x18,
        0x80,
        0x01,
        0x00,
        0x00,
        0x00,
        0x48,
        0x9C,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x06,
        0xBC,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x06,
        0x00,
        0x00,
        0x99,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xA2,
        0x48,
        0x00,
        0x00,
        0x8E,
        0xF9,
        0xFF,
        0xFF,
        0x48,
        0xF4,
        0xFF,
        0xFF,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x06,
        0xBC,
        0x08,
        0x99,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xE1,
        0x5F,
        0x00,
        0x00,
        0x92,
        0xEF,
        0xFF,
        0xFF,
        0xD8,
        0xF6,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xCC,
        0x0C,
        0x3D,
        0x0A,
        0x66,
        0x06,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8E,
        0xF9,
        0xFF,
        0xFF,
        0x15,
        0xFC,
        0xFF,
        0xFF,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x9C,
        0x2A,
        0x00,
        0x00,
        0x03,
        0xFB,
        0xFF,
        0xFF,
        0x08,
        0xF7,
        0xFF,
        0xFF,
    },
};

/// Switch-on model-light collection: six points, one cone and no directional lights.
///
/// Rooms 1 and 3 borrow this single descriptor and its mutable light arrays.
/// The opaque tail of the cone-light storage is outside the published count.
static WorldCoordRoomLights _gDryfieldWaterHoleLitLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldWaterHoleLitPointLights),
    _gDryfieldWaterHoleLitPointLights,
    ARRAY_SIZE(_gDryfieldWaterHoleLitSpotLightStorage.coneLights),
    _gDryfieldWaterHoleLitSpotLightStorage.coneLights,
};

/// Seven switch-off point lights shared by room variants 2 and 4 in every view.
///
/// Positions and falloff distances are room game units; RGB uses 12 fractional
/// bits. All inner radii are zero. Composition and attenuation mutate these
/// entries; their borrowed addresses expire with this overlay.
static WorldCoordPointLight _gDryfieldWaterHoleUnlitPointLights[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8650, -1650, -1100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3A06, -1650, -3100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x48A2, -1650, -3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x5FE1, -4206, -2344 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2621, 1638 }, { 0, 0 } }, 0, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5500, -1650, -1003 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2A9C, -1277, -2296 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x5208, -1277, -2996 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
};

/// The switch-off cone light shared by room variants 2 and 4 in every view.
///
/// Position and falloff distances are room game units, RGB and the axis are
/// Q12, and the full opening is 113 in 4096 units per turn. The counted light
/// remains writable for composition and attenuation while the overlay is loaded.
static WorldCoordSpotLight _gDryfieldWaterHoleUnlitSpotLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7444, -3447, -1496 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3014, 2621 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 2500, 5000, 113 },
};

/// Switch-off model-light collection: seven points, one cone and no directional lights.
///
/// Rooms 2 and 4 borrow this single descriptor and its mutable light arrays.
static WorldCoordRoomLights _gDryfieldWaterHoleUnlitLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldWaterHoleUnlitPointLights),
    _gDryfieldWaterHoleUnlitPointLights,
    ARRAY_SIZE(_gDryfieldWaterHoleUnlitSpotLights),
    _gDryfieldWaterHoleUnlitSpotLights,
};

enum {
    DRYFIELD_WATER_HOLE_BAT_PLACEMENT_ENTRY = 37,
    DRYFIELD_WATER_HOLE_BAT_FILE_NUMBER     = 37,
};

/// Default placement variant's bat resource recipe, followed by AREA_PLACEMENT_END.
///
/// Entry 37 selects decimal file 37 at file-group base 10 (base file 103700)
/// and task descriptor 0. The eight placement records reuse this one recipe.
/// The variant borrows the resource list and bat overlay's task descriptor;
/// both must stay loaded during loading, spawning and model-flag updates.
static AreaResource _gDryfieldWaterHoleAreaResources[2] = {
    { DRYFIELD_WATER_HOLE_BAT_PLACEMENT_ENTRY, DRYFIELD_WATER_HOLE_BAT_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldWaterHoleAreaVariants[DRYFIELD_WATER_HOLE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldWaterHoleBatPlacements, .resources = _gDryfieldWaterHoleAreaResources },
};

/// Minimum ambient colours for logical views 1..8 in switch-on rooms 1 and 3.
///
/// Entry zero counts the following view entries. RGB levels use 16 units per
/// 8-bit colour level; luma stores (3*r + 4*g + b)/8. The brighter minima in
/// views 6 and 7 are selected by logical view, independently of resource mapping.
static WorldCoordRoomAmbientEntry _gDryfieldWaterHoleLitAmbientByView[9] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldWaterHoleLitAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 509, 508, 509, 508 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Cue-2 bases in the loaded type-1 bank; cue 1 selects each base plus one.
enum {
    DRYFIELD_WATER_HOLE_SURFACE_4_WALK_REQUEST = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    DRYFIELD_WATER_HOLE_SURFACE_4_RUN_REQUEST  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
    DRYFIELD_WATER_HOLE_SURFACE_4_JUMP_REQUEST = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x29,
    DRYFIELD_WATER_HOLE_SURFACE_6_WALK_REQUEST = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    DRYFIELD_WATER_HOLE_SURFACE_6_RUN_REQUEST  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
};

/// Footstep pairs for class 4's grid floors and lower scripted stair steps.
///
/// Walk, run and scripted-jump bases select distinct type-1 script pairs;
/// cue 2 uses the base and cue 1 uses base + 1. The surface profile borrows
/// this room-owned record for the overlay's loaded lifetime.
static WorldCollisionFootstepSounds _gDryfieldWaterHoleSurface4FootstepSounds = {
    .walk         = DRYFIELD_WATER_HOLE_SURFACE_4_WALK_REQUEST,
    .run          = DRYFIELD_WATER_HOLE_SURFACE_4_RUN_REQUEST,
    .scriptedJump = DRYFIELD_WATER_HOLE_SURFACE_4_JUMP_REQUEST,
};

/// Footstep pairs for class 6, selected by the upper scripted stair steps.
///
/// Scripted jumps share the walk pair; running has a distinct type-1 pair.
/// Cue 2 uses the base and cue 1 uses base + 1. No grid face carries class 6;
/// the surface profile borrows this record while the overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldWaterHoleSurface6FootstepSounds = {
    .walk         = DRYFIELD_WATER_HOLE_SURFACE_6_WALK_REQUEST,
    .run          = DRYFIELD_WATER_HOLE_SURFACE_6_RUN_REQUEST,
    .scriptedJump = DRYFIELD_WATER_HOLE_SURFACE_6_WALK_REQUEST,
};

/// Silent blocking profile shared by surface classes 0, 1, 5 and 7.
///
/// Allows weapon impacts and applies grid pushback; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldWaterHoleDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 4's blocking profile with footstep cues, used by floors and lower stair steps.
///
/// Allows weapon impacts and applies grid pushback. Grid walls also carry this
/// class; the first byte's role is unproven. The footstep record is borrowed.
static WorldCollisionSurfaceProperties _gDryfieldWaterHoleSurface4Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldWaterHoleSurface4FootstepSounds,
};

/// Class 2's silent blocking profile, selected by a room grid wall face.
///
/// Allows weapon impacts and applies grid pushback; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldWaterHoleSurface2Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 3's silent blocking profile, selected by a room grid wall face.
///
/// Allows weapon impacts and applies grid pushback; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldWaterHoleSurface3Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 6's blocking profile with footstep cues for the upper scripted stair steps.
///
/// Allows weapon impacts and applies grid pushback; no grid face carries this
/// class. The first byte's role is unproven. The footstep record is borrowed.
static WorldCollisionSurfaceProperties _gDryfieldWaterHoleSurface6Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldWaterHoleSurface6FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldWaterHoleSurfacePropertiesByClass[8] = {
    &_gDryfieldWaterHoleDefaultSurfaceProperties,
    &_gDryfieldWaterHoleDefaultSurfaceProperties,
    &_gDryfieldWaterHoleSurface2Properties,
    &_gDryfieldWaterHoleSurface3Properties,
    &_gDryfieldWaterHoleSurface4Properties,
    &_gDryfieldWaterHoleDefaultSurfaceProperties,
    &_gDryfieldWaterHoleSurface6Properties,
    &_gDryfieldWaterHoleDefaultSurfaceProperties,
};

/// Byte cursor for interleaved POLY_G4 and DR_MODE packets in the current actor-arena half.
///
/// Reset on every draw to buffer 2 without a companion, otherwise buffer 1,
/// offset by the current ordering-table half (0 or 1) times 0xC000 bytes.
/// Packet casts select views of word-aligned storage; GPU use outlives this cursor.
static u8* _gWaterHolePrimCursor = NULL;

/// Signed-halfword running-actor tick counter controlling the water seam's phase.
///
/// Paused actors retain it. Each increment scrolls -16 angle units (4096 per
/// turn); narrowing retains the low 16 bits, so the sine repeats every 256 ticks.
static s16 _gWaterHoleWaveScroll;

/// Refuses every key-item use in this room without consuming the item.
///
/// `itemId` is the inventory item ID carried by
/// `ROOM_MESSAGE_USE_KEY_ITEM`; all inputs are unused.
/// Returns zero so the item menu displays its refusal notice.
static s32 _dryfieldWaterHoleRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/water_hole_door_msg.inc.c"

/// Examines the water hole in response to CAP room command 2.
///
/// Starts CAP playback with a display transition, shows the water map marker
/// and queues the locked-route sound, even if CAP playback cannot start.
/// Other commands do nothing. Requires the room CAP resources; returns zero.
static s32 _dryfieldWaterHoleCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_WATER_HOLE_COMMAND_EXAMINE = 2,
        DRYFIELD_WATER_HOLE_MAP_MARK_SHOWN  = 2,
    };

    if (commandId == DRYFIELD_WATER_HOLE_COMMAND_EXAMINE) {
        capRunCommandWithTransition(DRYFIELD_WATER_HOLE_COMMAND_EXAMINE);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, DRYFIELD_WATER_HOLE_MAP_MARK_SHOWN);
        sndEvtRequestScriptStart(SOUND_WATER_HOLE_LOCKED, 0, 0);
    }
    return 0;
}

/// Ignores trigger requests for a room-specific action.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` supplies a borrowed `request` and a zero
/// second payload word. All inputs are unused; returns zero, which the
/// direction sender ignores.
static s32 _dryfieldWaterHoleIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Plays the water-hole room's sound for CAP cue 4 or 5.
///
/// `ROOM_MESSAGE_SOUND` carries an integer cue key and an unused second word.
/// Cue 4 selects the locked sound and 5 selects area sound 5; other keys do
/// nothing. Sound requests are queued without per-task state. Always returns 0.
static s32 _dryfieldWaterHolePlaySoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_WATER_HOLE_SOUND_CUE_LOCKED = 4,
        DRYFIELD_WATER_HOLE_SOUND_CUE_5      = 5,
    };

    switch (cueKey) {
        case DRYFIELD_WATER_HOLE_SOUND_CUE_LOCKED:
            sndEvtRequestScriptStart(SOUND_WATER_HOLE_LOCKED, 0, 0);
            break;
        case DRYFIELD_WATER_HOLE_SOUND_CUE_5:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_HOLE, 5), 0, 0);
            break;
    }
    return 0;
}

/// Registers the water-hole message receiver and starts its water-surface task.
///
/// Enter at state 0 with the room resources loaded. Publishes the live task in
/// `GAME_TASK_SLOT_ROOM` and advances to idle state 1 even if the water-task
/// spawn fails. The room overlay must remain loaded while either task lives.
static void _dryfieldWaterHoleInitializeRoomTask(Task* task)
{
    task->msgTable = _gDryfieldWaterHoleRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(&_gDryfieldWaterHoleWaterTaskDesc, 0, 0, 0);
    task->state = task->state + 1;
}

/// Keeps the initialized room task idle without changing its state.
///
/// State 1 has no per-frame work; the task's installed message table remains
/// available for dispatch. The task argument is unused.
static void _dryfieldWaterHoleRoomIdle(Task* task)
{
}

/// Room-task callbacks indexed by state: 0 initialize, 1 idle, 2 release.
///
/// The dispatcher copies all three callbacks to its stack; state must be 0..2.
static const TaskFuncTable3 _gDryfieldWaterHoleRoomTaskStates = {
    { _dryfieldWaterHoleInitializeRoomTask, _dryfieldWaterHoleRoomIdle, taskKill },
};

void dryfieldWaterHoleRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldWaterHoleRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/water_hole_draw_surfaces.inc.c"

#include "../../shared/water_hole_water_task.inc.c"

#include "../../shared/water_hole_water_start.inc.c"

void dryfieldWaterHoleRoomEffectsTask(Task* task)
{
    enum {
        DRYFIELD_WATER_HOLE_EFFECTS_INITIALIZE  = 0,
        DRYFIELD_WATER_HOLE_EFFECTS_ACTIVE      = 1,
        DRYFIELD_WATER_HOLE_SPLASH_SAMPLE_COUNT = ARRAY_SIZE(_gDryfieldWaterHolePreviousPartPositions),
        DRYFIELD_WATER_HOLE_SPLASH_FIRST_PART   = 14,
        DRYFIELD_WATER_HOLE_SPLASH_PART_STRIDE  = 3,
        DRYFIELD_WATER_HOLE_SPLASH_ROLL_MASK    = 0x1FF,
        DRYFIELD_WATER_HOLE_RIPPLE_ODDS_BIAS    = 32,
        DRYFIELD_WATER_HOLE_RIPPLE_HALF_SIDE    = 64,
        DRYFIELD_WATER_HOLE_SPRAY_SIZE          = 384,
        DRYFIELD_WATER_HOLE_SPRAY_CELL_UPDATES  = 2,
        DRYFIELD_WATER_HOLE_SPRAY_LAUNCH_SPEED  = 32,
        DRYFIELD_WATER_HOLE_SPRAY_UPWARD_BURST  = 1,
        DRYFIELD_WATER_HOLE_SPRAY_SPAWN_ARG     = DRYFIELD_WATER_HOLE_SPRAY_SIZE |
                                              (DRYFIELD_WATER_HOLE_SPRAY_CELL_UPDATES << 12) |
                                              (DRYFIELD_WATER_HOLE_SPRAY_LAUNCH_SPEED << 16) |
                                              (DRYFIELD_WATER_HOLE_SPRAY_UPWARD_BURST << 24),
        DRYFIELD_WATER_HOLE_BEAMS_ENABLED     = 1,
        DRYFIELD_WATER_HOLE_BEAM_VIEWS_3_4    = (1 << 3) | (1 << 4),
        DRYFIELD_WATER_HOLE_BEAM_VIEWS_4_6    = (1 << 4) | (1 << 6),
        DRYFIELD_WATER_HOLE_BEAM_VIEW_7       = 1 << 7,
        DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE = 256,
    };
    Task*       playerTask;
    s32         viewMask;
    EffectWork* work;
    GfxCoord*   effectCoord;
    GfxCoord*   playerRoot;
    GfxCoord*   trackedPart;
    GfxCoord*   viewCoord;
    GfxCoord    surfaceCoord;
    s32         sampleIndex;
    u32         randomValue;

/// Emits one tracked part's ripple/spray pair and updates its position sample.
///
/// Captures work, trackedPart, viewCoord, surfaceCoord, randomValue and this
/// function's effect constants. sampleIndex is a stable, side-effect-free 0..1
/// index, evaluated repeatedly. The temporary coordinate is copied by spawn;
/// two LCG draws occur in ripple/spray order. Use inside a braced loop body.
#define DRYFIELD_WATER_HOLE_EMIT_PART_SPLASHES(sampleIndex)                                                                                             \
    {                                                                                                                                                   \
        actorRenderComposeCoord(trackedPart);                                                                                                           \
        /* Odds narrow to s16; sample history also retains only the low halfwords. */                                                                   \
        work->angle = ABS(_gDryfieldWaterHolePreviousPartPositions[(sampleIndex)].vx - trackedPart->workm.t[0]) +                                       \
                      ABS(_gDryfieldWaterHolePreviousPartPositions[(sampleIndex)].vy - trackedPart->workm.t[1]) +                                       \
                      ABS(_gDryfieldWaterHolePreviousPartPositions[(sampleIndex)].vz - trackedPart->workm.t[2]) + DRYFIELD_WATER_HOLE_RIPPLE_ODDS_BIAS; \
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &trackedPart->workm, &surfaceCoord.coord);                                                       \
        surfaceCoord.parent       = viewCoord;                                                                                                          \
        surfaceCoord.coord.t[1]   = gGameSession->waterY;                                                                                               \
        surfaceCoord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                                               \
        actorRenderComposeCoord(&surfaceCoord);                                                                                                         \
        randomValue = (RANDOM_LCG_ADVANCE());                                                                                                           \
        if ((s32)((randomValue >> 16) & DRYFIELD_WATER_HOLE_SPLASH_ROLL_MASK) < work->angle) {                                                          \
            effectSpawn(gRoomEffectWaterRippleId, &surfaceCoord, DRYFIELD_WATER_HOLE_RIPPLE_HALF_SIDE, 0);                                              \
        }                                                                                                                                               \
        work->angle -= DRYFIELD_WATER_HOLE_RIPPLE_ODDS_BIAS;                                                                                            \
        randomValue  = (RANDOM_LCG_ADVANCE());                                                                                                          \
        if ((s32)((randomValue >> 16) & DRYFIELD_WATER_HOLE_SPLASH_ROLL_MASK) < work->angle) {                                                          \
            effectSpawn(gRoomEffectWaterSprayId, &surfaceCoord, DRYFIELD_WATER_HOLE_SPRAY_SPAWN_ARG, 0);                                                \
        }                                                                                                                                               \
        _gDryfieldWaterHolePreviousPartPositions[(sampleIndex)].vx = trackedPart->workm.t[0];                                                           \
        _gDryfieldWaterHolePreviousPartPositions[(sampleIndex)].vy = trackedPart->workm.t[1];                                                           \
        _gDryfieldWaterHolePreviousPartPositions[(sampleIndex)].vz = trackedPart->workm.t[2];                                                           \
    }

    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    viewMask    = 1 << gGameSession->location.view;
    work        = task->spawnArg2.pointer;
    effectCoord = task->extra.coordBody->coord;
    playerRoot  = playerTask->extra.tmd->coords;
    switch (task->state) {
        case DRYFIELD_WATER_HOLE_EFFECTS_INITIALIZE:
            gRoomEffectWaterRippleId = EFFECT_DRYFIELD_WATER_HOLE_WATER_RIPPLE;
            gRoomEffectWaterSprayId  = EFFECT_DRYFIELD_WATER_HOLE_WATER_SPRAY;
            task->state              = DRYFIELD_WATER_HOLE_EFFECTS_ACTIVE;
            // Seed history from the current composed positions, without composing again.
            for (sampleIndex = 0; sampleIndex < DRYFIELD_WATER_HOLE_SPLASH_SAMPLE_COUNT; sampleIndex++) {
                trackedPart                                              = &playerTask->extra.tmd->coords[DRYFIELD_WATER_HOLE_SPLASH_FIRST_PART + sampleIndex * DRYFIELD_WATER_HOLE_SPLASH_PART_STRIDE];
                _gDryfieldWaterHolePreviousPartPositions[sampleIndex].vx = trackedPart->workm.t[0];
                _gDryfieldWaterHolePreviousPartPositions[sampleIndex].vy = trackedPart->workm.t[1];
                _gDryfieldWaterHolePreviousPartPositions[sampleIndex].vz = trackedPart->workm.t[2];
            }
            break;
        case DRYFIELD_WATER_HOLE_EFFECTS_ACTIVE:
            // Paused or out-of-water updates retain the last emitting positions.
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && gGameSession->waterY < playerRoot->coord.t[1]) {
                viewCoord = &gGfxViewCoord;
                for (sampleIndex = 0; sampleIndex < DRYFIELD_WATER_HOLE_SPLASH_SAMPLE_COUNT; sampleIndex++) {
                    trackedPart = &playerTask->extra.tmd->coords[DRYFIELD_WATER_HOLE_SPLASH_FIRST_PART + sampleIndex * DRYFIELD_WATER_HOLE_SPLASH_PART_STRIDE];
                    DRYFIELD_WATER_HOLE_EMIT_PART_SPLASHES(sampleIndex);
                }
            }
            // Beam drawing is independent of the splash pause and water-height gates.
            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == DRYFIELD_WATER_HOLE_BEAMS_ENABLED) {
                if (viewMask & DRYFIELD_WATER_HOLE_BEAM_VIEWS_3_4) {
                    _glowDrawTaperedBeam(effectCoord, &_gDryfieldWaterHoleSwitchBeamEndpoints[1], &_gDryfieldWaterHoleSwitchBeamEndpoints[0], DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE);
                    _glowDrawTaperedBeam(effectCoord, &_gDryfieldWaterHoleSwitchBeamEndpoints[3], &_gDryfieldWaterHoleSwitchBeamEndpoints[2], DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE);
                }
                if (viewMask & DRYFIELD_WATER_HOLE_BEAM_VIEWS_4_6) {
                    _glowDrawTaperedBeam(effectCoord, &_gDryfieldWaterHoleSwitchBeamEndpoints[4], &_gDryfieldWaterHoleSwitchBeamEndpoints[5], DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE);
                    _glowDrawTaperedBeam(effectCoord, &_gDryfieldWaterHoleSwitchBeamEndpoints[6], &_gDryfieldWaterHoleSwitchBeamEndpoints[7], DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE);
                }
                if (viewMask & DRYFIELD_WATER_HOLE_BEAM_VIEW_7) {
                    _glowDrawTaperedBeam(effectCoord, &_gDryfieldWaterHoleSwitchBeamEndpoints[9], &_gDryfieldWaterHoleSwitchBeamEndpoints[8], DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE);
                    _glowDrawTaperedBeam(effectCoord, &_gDryfieldWaterHoleSwitchBeamEndpoints[11], &_gDryfieldWaterHoleSwitchBeamEndpoints[10], DRYFIELD_WATER_HOLE_BEAM_RADIUS_SCALE);
                }
            }
            break;
    }
#undef DRYFIELD_WATER_HOLE_EMIT_PART_SPLASHES
}

#include "../../shared/effects/glow_draw_tapered_beam.inc.c"

#include "../../shared/effects/water_ripple_task.inc.c"

void dryfieldWaterHoleWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"
