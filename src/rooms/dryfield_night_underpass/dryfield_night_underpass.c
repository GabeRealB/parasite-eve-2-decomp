#include "rooms/dryfield_night_underpass.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_01100.h"
#include "actors/actor_01500.h"
#include "actors/actor_01600.h"
#include "actors/actor_400500.h"
#include "actors/actor_402200.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"

static void _underpassSwitchTask(Task* task);
static s32  _underpassSwitchMsg(Task* unusedTask, s32 unusedMessageId, s32 switchId, s32 unusedSecondArg);

static s32 _underpassSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

static s32  _dryfieldNightUnderpassRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unused);
static s32  _dryfieldNightUnderpassIgnoreRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unused);
static void _dryfieldNightUnderpassIdleTask(Task* unusedTask);

static WorldCollisionGrid     _gDryfieldNightUnderpassCollisionGrid;
static WorldCollisionOccluder _gDryfieldNightUnderpassSightOccluders[3];
static WorldCollisionTrigger  _gDryfieldNightUnderpassViewBoundaryTriggers[16];
static WorldCollisionTrigger  _gDryfieldNightUnderpassActionTriggers[6];
static WorldCoordRoomLights   _gDryfieldNightUnderpassLampRoomLights;
static WorldCoordRoomLights   _gDryfieldNightUnderpassDimRoomLights;

/// Bodyless switch-prompt recipe at index zero, followed by the descriptor end marker.
///
/// The command handler supplies a flag nibble index and CAP command in the
/// two spawn words. Spawning reads the recipe synchronously; the room and CAP
/// resources must remain loaded until the prompt task completes.
static TaskDesc _gUnderpassSwitchTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _underpassSwitchTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Underpass receiver table for switch-dependent transitions, switch commands and sounds.
///
/// The room task borrows these ordered ID/handler records. The final
/// `TASK_MESSAGE_TABLE_END` makes unsupported messages return zero; keep
/// this overlay and its callback code loaded while the task receives messages.
static const TaskMessageEntry _gDryfieldNightUnderpassRoomMessages[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantUnderpassMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightUnderpassRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightUnderpassIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _underpassSwitchMsg },
    { ROOM_MESSAGE_SOUND, _underpassSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Eight ceiling-lamp flare centres in signed world-coordinate units.
///
/// Each element pairs with the same index in `_gDryfieldNightUnderpassFlareViewMasks`.
/// The fourth SVECTOR component is unused by projection.
static SVECTOR _gDryfieldNightUnderpassFlarePositions[8] = {
    { 0x4588, -3480, -1500, 0 },
    { 0x3B60, -3480, -6000, 0 },
    { 0x4588, -3480, -8700, 0 },
    { 0x34BC, -3480, -7200, 0 },
    { 9300, -3480, -9800, 0 },
    { 6200, -3480, -0x29CC, 0 },
    { 4850, -3420, -7200, 0 },
    { 500, -3480, -9700, 0 },
};

/// Logical-view visibility bits for the corresponding ceiling-lamp flare centres.
///
/// Bit n selects logical view n directly, before room-view mapping.
/// Signed halfword storage preserves the flare task's signed load and promotion.
static s16 _gDryfieldNightUnderpassFlareViewMasks[ARRAY_SIZE(_gDryfieldNightUnderpassFlarePositions)] = {
    (1 << 2) | (1 << 11),
    (1 << 3),
    (1 << 3) | (1 << 4) | (1 << 9),
    (1 << 4) | (1 << 9),
    (1 << 4),
    (1 << 6),
    (1 << 5),
    (1 << 5) | (1 << 7),
};

WorldCollisionRoomResources gDryfieldNightUnderpassRoomCollisionResources[6] = {
    {
        .grid                 = &_gDryfieldNightUnderpassCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightUnderpassViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightUnderpassActionTriggers,
        .occluders            = _gDryfieldNightUnderpassSightOccluders,
    },
    {
        .grid                 = &_gDryfieldNightUnderpassCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightUnderpassViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightUnderpassActionTriggers,
        .occluders            = _gDryfieldNightUnderpassSightOccluders,
    },
    {
        .grid                 = &_gDryfieldNightUnderpassCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightUnderpassViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightUnderpassActionTriggers,
        .occluders            = _gDryfieldNightUnderpassSightOccluders,
    },
    {
        .grid                 = &_gDryfieldNightUnderpassCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightUnderpassViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightUnderpassActionTriggers,
        .occluders            = _gDryfieldNightUnderpassSightOccluders,
    },
    {
        .grid                 = &_gDryfieldNightUnderpassCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightUnderpassViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightUnderpassActionTriggers,
        .occluders            = _gDryfieldNightUnderpassSightOccluders,
    },
    {
        .grid                 = &_gDryfieldNightUnderpassCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightUnderpassViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightUnderpassActionTriggers,
        .occluders            = _gDryfieldNightUnderpassSightOccluders,
    },
};

WorldCoordRoomLighting gDryfieldNightUnderpassRoomLighting[6] = {
    { &_gDryfieldNightUnderpassLampRoomLights, NULL },
    { &_gDryfieldNightUnderpassDimRoomLights, NULL },
    { &_gDryfieldNightUnderpassLampRoomLights, NULL },
    { &_gDryfieldNightUnderpassDimRoomLights, NULL },
    { &_gDryfieldNightUnderpassLampRoomLights, NULL },
    { &_gDryfieldNightUnderpassLampRoomLights, NULL },
};

/// Room 2's eleven logical-view indices, followed by a zero outside the live range.
///
/// Values select the area's 1-based camera/image/sprite slots. Reverse searches
/// use eleven entries; the stored zero does not terminate a lookup.
static u8 _gDryfieldNightUnderpassRoom2ViewMap[12] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

/// Room 3's eleven logical-view indices, followed by a zero outside the live range.
///
/// Values select the area's 1-based camera/image/sprite slots. Reverse searches
/// use eleven entries; the stored zero does not terminate a lookup.
static u8 _gDryfieldNightUnderpassRoom3ViewMap[12] = {
    1,
    11,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

/// Room 4's eleven logical-view indices, followed by a zero outside the live range.
///
/// Values select the area's 1-based camera/image/sprite slots. Reverse searches
/// use eleven entries; the stored zero does not terminate a lookup.
static u8 _gDryfieldNightUnderpassRoom4ViewMap[12] = {
    1,
    21,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    26,
    0,
};

/// Room 5's eleven logical-view indices, followed by a zero outside the live range.
///
/// Values select the area's 1-based camera/image/sprite slots. Reverse searches
/// use eleven entries; the stored zero does not terminate a lookup.
static u8 _gDryfieldNightUnderpassRoom5ViewMap[12] = {
    1,
    22,
    23,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

/// Room 6's eleven logical-view indices, followed by a zero outside the live range.
///
/// Values select the area's 1-based camera/image/sprite slots. Reverse searches
/// use eleven entries; the stored zero does not terminate a lookup.
static u8 _gDryfieldNightUnderpassRoom6ViewMap[12] = {
    1,
    24,
    25,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    26,
    0,
};

u8* gDryfieldNightUnderpassViewMaps[6] = {
    gViewIdentityMap,
    _gDryfieldNightUnderpassRoom2ViewMap,
    _gDryfieldNightUnderpassRoom3ViewMap,
    _gDryfieldNightUnderpassRoom4ViewMap,
    _gDryfieldNightUnderpassRoom5ViewMap,
    _gDryfieldNightUnderpassRoom6ViewMap,
};

// Logical entries searched in each room map, independent of its storage capacity.
enum {
    DRYFIELD_NIGHT_UNDERPASS_RESTRICTED_LOGICAL_VIEW_COUNT = 11,
};

ViewCount gDryfieldNightUnderpassViewCounts[6] = { DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT, DRYFIELD_NIGHT_UNDERPASS_RESTRICTED_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_UNDERPASS_RESTRICTED_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_UNDERPASS_RESTRICTED_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_UNDERPASS_RESTRICTED_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_UNDERPASS_RESTRICTED_LOGICAL_VIEW_COUNT };

// Optional door map-marker nibble IDs stored by these endpoints.
enum {
    DRYFIELD_NIGHT_UNDERPASS_CELLAR_MAP_FLAG     = 463,
    DRYFIELD_NIGHT_UNDERPASS_WATER_HOLE_MAP_FLAG = 462,
};

DirectionWarpEntry gDryfieldNightUnderpassWarpEntries[3] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 5303, .y = -997, .z = -0x2C94 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = 0 }, .x = 5303, .y = -997, .z = -0x2C94 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 8),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 7),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_UNDERPASS_CELLAR_MAP_FLAG,
    },
    {
        .player         = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN / 4) }, .x = 0x3D31, .y = -1000, .z = -4216 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = (ACTOR_TRANSFORM_ANGLE_TURN / 4) }, .x = 0x3D31, .y = -1000, .z = -4216 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 6),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 5),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_UNDERPASS_WATER_HOLE_MAP_FLAG,
    },
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 1212, .y = -1000, .z = -7543 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 1212, .y = -1000, .z = -7543 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 1),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 7,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Twelve room-space collision normals, with 4096 representing unit length.
static SVECTOR _gDryfieldNightUnderpassCollisionGridNormals[12] = {
#include "assets/dryfield_night_underpass_collision_01114_normals.inc"
};

/// Seventy mesh vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightUnderpassCollisionGridVerts[70] = {
#include "assets/dryfield_night_underpass_collision_01114_verts.inc"
};

/// Forty-six collision faces using surface classes 0, 2, 3 and 4.
static WorldCollisionGridFace _gDryfieldNightUnderpassCollisionGridFaces[46] = {
#include "assets/dryfield_night_underpass_collision_01114_faces.inc"
};

/// Twenty-four signed face-index lists, each ending in WORLD_COLLISION_GRID_CELL_END.
///
/// The 346-halfword asset span also retains unused zeros between lists.
static s16 _gDryfieldNightUnderpassCollisionGridCells[346] = {
#include "assets/dryfield_night_underpass_collision_01114_cells.inc"
};

// The generated table supplies halfword offsets into this grid's cell-list pool.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightUnderpassCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..345 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_UNDERPASS_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightUnderpassCollisionGridCells[(halfwordOffset)])
/// The six-by-four X-major directory of cell face-index lists.
static s16* _gDryfieldNightUnderpassCollisionGridTable[24] = {
#include "assets/dryfield_night_underpass_collision_01114_table.inc"
};
#undef DRYFIELD_NIGHT_UNDERPASS_CELL_FACE_LIST_AT_OFFSET

/// The collision mesh shared by nighttime underpass rooms 1..6.
///
/// The XZ grid starts at (-3000, -13000), with six-by-four 4000-unit cells.
/// Activation binds the mutable descriptor to the live view coordinate;
/// descriptor and pools remain valid only while this overlay is loaded.
static WorldCollisionGrid _gDryfieldNightUnderpassCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightUnderpassCollisionGridNormals,
    .vertices    = _gDryfieldNightUnderpassCollisionGridVerts,
    .faces       = _gDryfieldNightUnderpassCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightUnderpassCollisionGridTable,
    .xBias       = 3000,
    .zBias       = 13000,
    .cellCountX  = 6,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightUnderpassCollisionGridFaces),
};

ViewCamera gDryfieldNightUnderpassViewCameras[DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -7864, 0x6671, 5100 } }, 329 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { -4007, 0, 848 }, { -132, 4045, -626 }, { -837, -639, -3958 } }, { -0x42CB, 1690, 3166 } }, 230 },
    { { { { 998, 0, -3972 }, { 996, 3964, 250 }, { 3845, -1027, 966 } }, { -6656, 1455, 9182 } }, 230 },
    { { { { 994, 0, 3973 }, { -803, 4011, 201 }, { -3891, -828, 974 } }, { -0x2AAE, 1498, 9252 } }, 230 },
    { { { { -3946, 0, 1096 }, { -240, 3996, -865 }, { -1069, -898, -3850 } }, { -6279, 1492, 7705 } }, 230 },
    { { { { 951, 0, 3983 }, { -763, 4020, 182 }, { -3910, -784, 934 } }, { -5505, 1532, 9129 } }, 230 },
    { { { { 1678, 0, 3736 }, { 2611, 2928, -1173 }, { -2671, 2863, 1200 } }, { -444, 3936, 9337 } }, 230 },
    { { { { 1312, 0, -3879 }, { 1085, 3932, 367 }, { 3724, -1146, 1260 } }, { -0x293A, 1292, 9364 } }, 230 },
    { { { { 3773, 0, 1592 }, { 1116, 2920, -2645 }, { -1135, 2871, 2690 } }, { -0x43B0, 3973, 2327 } }, 230 },
    { { { { 3875, 0, 1325 }, { -149, 4069, 437 }, { -1316, -461, 3851 } }, { -0x4463, 1654, 7373 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { 4042, 0, 659 }, { 359, 3434, -2202 }, { -553, 2231, 3389 } }, { -0x421F, 3970, 8993 } }, 230 },
    { { { { 4075, 0, 405 }, { 186, 3640, -1868 }, { -360, 1877, 3622 } }, { -0x4162, 3823, 5117 } }, 230 },
    { { { { -4047, 0, 631 }, { -451, 2858, -2898 }, { -440, -2933, -2824 } }, { -0x4229, 2000, 6853 } }, 348 },
};

/// Mapped view 1's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// GPU encodings and neutral modulation shared by these room sprite sources.
///
/// Pages use 8-bit textures at VRAM Y 0 with average blending; the palette
/// starts at (0, 255). Code flags remain zero for opaque, modulated sprites.
enum {
    DRYFIELD_NIGHT_UNDERPASS_SPRITE_TEXTURE_8BIT  = 1,
    DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896     = getTPage(DRYFIELD_NIGHT_UNDERPASS_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960     = getTPage(DRYFIELD_NIGHT_UNDERPASS_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR = 128,
};

/// Sixteen mutable texture rectangles and sorting depths for mapped view 3.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
static SpriteSource _gDryfieldNightUnderpassView3SpriteSources[16] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 3's complete source range after a decoded-background selector.
///
/// The zero-count first record retains background strips; the end marker
/// follows the single range covering every source element.
static SpriteBatch _gDryfieldNightUnderpassView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Eleven mutable texture rectangles and sorting depths for mapped view 4.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
static SpriteSource _gDryfieldNightUnderpassView4SpriteSources[11] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 4's complete source range after a decoded-background selector.
///
/// The zero-count first record retains background strips; the end marker
/// follows the single range covering every source element.
static SpriteBatch _gDryfieldNightUnderpassView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Ten mutable texture rectangles and sorting depths for mapped view 5.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
static SpriteSource _gDryfieldNightUnderpassView5SpriteSources[10] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 5's complete source range and empty trailing range.
///
/// The zero-count first record retains background strips; the trailing range
/// begins one past the sources and has no elements. Byte `field_6[0]` is 1
/// on the populated range; its role is unproven and the stored value is retained.
static SpriteBatch _gDryfieldNightUnderpassView5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView5SpriteSources), 0, 0, { 1, 0 } },
    { ARRAY_SIZE(_gDryfieldNightUnderpassView5SpriteSources), 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 7's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 8's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Eleven mutable texture rectangles and sorting depths for mapped view 9.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
static SpriteSource _gDryfieldNightUnderpassView9SpriteSources[11] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 9's complete source range after two empty records.
///
/// The zero-count first record retains background strips. The second has
/// `field_6[0]` set to 1; its role is unproven and the stored value is retained.
/// The third record covers every source element before the end marker.
static SpriteBatch _gDryfieldNightUnderpassView9SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 10's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 11's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 12's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 16 texture rectangles and sorting depths for mapped view 13.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
/// Storage remains writable for the room-sprite interface.
static SpriteSource _gDryfieldNightUnderpassView13SpriteSources[16] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, 48, -120, 1016, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 40 } }, 72, 80, 964, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, 88, -120, 1000, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 56 } }, 128, -120, 1000, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, 88, 24, 975, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 56 } }, 128, 24, 975, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 48 } }, 88, -24, 975, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, -24, 975, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 128, -64, 975, { .fields = { 64, 216 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 40 } }, 88, -64, 975, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 96, 80, 975, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 128, 80, 975, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 48 } }, 64, 32, 993, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, 56, -64, 1005, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 32 } }, 56, -24, 1032, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 24 } }, 56, 8, 1014, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 13's complete source range after a decoded-background selector.
///
/// The zero-count first record retains background strips; the end marker
/// follows the single range covering every source element.
static SpriteBatch _gDryfieldNightUnderpassView13SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 11 texture rectangles and sorting depths for mapped view 14.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
/// Storage remains writable for the room-sprite interface.
static SpriteSource _gDryfieldNightUnderpassView14SpriteSources[11] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 8, 8 } }, -96, 96, 2125, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 40 } }, -16, -24, 2129, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 24 } }, 0, 56, 1860, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 32 } }, -16, 48, 2000, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 32 } }, -16, 16, 2058, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 88, 48 } }, -96, -72, 2125, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 48 } }, -96, -120, 2125, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 80, 120 } }, -96, -24, 2125, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 64, 80 } }, -160, 40, 2125, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_896, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 64, 80 } }, -160, -120, 2125, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 64, 80 } }, -160, -40, 2125, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 14's complete source range after a decoded-background selector.
///
/// The zero-count first record retains background strips; the end marker
/// follows the single range covering every source element.
static SpriteBatch _gDryfieldNightUnderpassView14SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView14SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 10 texture rectangles and sorting depths for mapped view 15.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
/// Storage remains writable for the room-sprite interface.
static SpriteSource _gDryfieldNightUnderpassView15SpriteSources[10] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 8, 24 } }, -104, -120, 910, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 56 } }, -112, -48, 913, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 48 } }, -104, -96, 995, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 48 } }, -120, 8, 901, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 16, 40 } }, -128, 56, 896, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 56, 72 } }, -160, -120, 897, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 56 } }, -160, -48, 897, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 48 } }, -160, 8, 897, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, -160, 56, 897, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 24 } }, -160, 96, 897, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 15's complete source range and empty trailing range.
///
/// The zero-count first record retains background strips; the trailing range
/// begins one past the sources and has no elements. Byte `field_6[0]` is 1
/// on the populated range; its role is unproven and the stored value is retained.
static SpriteBatch _gDryfieldNightUnderpassView15SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView15SpriteSources), 0, 0, { 1, 0 } },
    { ARRAY_SIZE(_gDryfieldNightUnderpassView15SpriteSources), 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 16's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 17's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView17SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 18's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView18SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 11 texture rectangles and sorting depths for mapped view 19.
///
/// Positions are relative to the view centre and sizes are pixels; UV is in texels.
/// Depth is the unscaled value quantized into the frame's ordering table.
/// Storage remains writable for the room-sprite interface.
static SpriteSource _gDryfieldNightUnderpassView19SpriteSources[11] = {
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 48 } }, -72, -112, 1325, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 56 } }, -120, -120, 1325, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 40, 56 } }, -160, -120, 1325, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 40 } }, -56, -16, 1197, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 32, 48 } }, -56, -64, 1250, { .fields = { 8, 192 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 32 } }, -56, 24, 1162, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 24, 32 } }, -56, 56, 1133, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 56, 80 } }, -112, -64, 1175, { .fields = { 72, 176 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 80 } }, -160, -64, 1175, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 56, 88 } }, -112, 16, 1125, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_UNDERPASS_SPRITE_TPAGE_960, DRYFIELD_NIGHT_UNDERPASS_SPRITE_CLUT, { .fields = { 48, 88 } }, -160, 16, 1125, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_UNDERPASS_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 19's complete source range after two empty records.
///
/// The zero-count first record retains background strips. The second has
/// `field_6[0]` set to 1; its role is unproven and the stored value is retained.
/// The third record covers every source element before the end marker.
static SpriteBatch _gDryfieldNightUnderpassView19SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightUnderpassView19SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 20's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView20SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 21's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView21SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 22's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView22SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 23's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView23SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 24's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView24SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 25's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView25SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 26's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightUnderpassView26SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightUnderpassSpriteViews[DRYFIELD_NIGHT_UNDERPASS_MAPPED_VIEW_COUNT] = {
    { { .empty = _gDryfieldNightUnderpassView1SpriteBatches }, _gDryfieldNightUnderpassView1SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView2SpriteBatches }, _gDryfieldNightUnderpassView2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView3SpriteSources }, _gDryfieldNightUnderpassView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView4SpriteSources }, _gDryfieldNightUnderpassView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView5SpriteSources }, _gDryfieldNightUnderpassView5SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView6SpriteBatches }, _gDryfieldNightUnderpassView6SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView7SpriteBatches }, _gDryfieldNightUnderpassView7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView8SpriteBatches }, _gDryfieldNightUnderpassView8SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView9SpriteSources }, _gDryfieldNightUnderpassView9SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView10SpriteBatches }, _gDryfieldNightUnderpassView10SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView11SpriteBatches }, _gDryfieldNightUnderpassView11SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView12SpriteBatches }, _gDryfieldNightUnderpassView12SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView13SpriteSources }, _gDryfieldNightUnderpassView13SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView14SpriteSources }, _gDryfieldNightUnderpassView14SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView15SpriteSources }, _gDryfieldNightUnderpassView15SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView16SpriteBatches }, _gDryfieldNightUnderpassView16SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView17SpriteBatches }, _gDryfieldNightUnderpassView17SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView18SpriteBatches }, _gDryfieldNightUnderpassView18SpriteBatches, NULL },
    { { .elements = _gDryfieldNightUnderpassView19SpriteSources }, _gDryfieldNightUnderpassView19SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView20SpriteBatches }, _gDryfieldNightUnderpassView20SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView21SpriteBatches }, _gDryfieldNightUnderpassView21SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView22SpriteBatches }, _gDryfieldNightUnderpassView22SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView23SpriteBatches }, _gDryfieldNightUnderpassView23SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView24SpriteBatches }, _gDryfieldNightUnderpassView24SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView25SpriteBatches }, _gDryfieldNightUnderpassView25SpriteBatches, NULL },
    { { .empty = _gDryfieldNightUnderpassView26SpriteBatches }, _gDryfieldNightUnderpassView26SpriteBatches, NULL },
};

/// 16 directed camera boundaries in room-local game units.
///
/// Source/destination views are 1-based: 3->2, 2->3, 3->9, 9->3, 5->4, 4->5, 5->6, 6->5, 5->7,
/// 7->5, 8->7, 7->8, 4->9, 9->4, 10->2, 2->10.
/// Room binding installs the view coordinate and enables all records through LAST.
/// Normals use 4096 per unit; keep the writable list live until it is unlinked.
static WorldCollisionTrigger _gDryfieldNightUnderpassViewBoundaryTriggers[16] = {
    { NULL, NULL, NULL, { 0x4070, -2560, -5280, 0 }, { { -1616, -1888, 0, 0 }, { 1616, -1888, 0, 0 }, { -1616, 1888, 0, 0 }, { 1616, 1888, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2544, -5408, 0 }, { { 1616, -1840, 0, 0 }, { -1616, -1840, 0, 0 }, { 1616, 1840, 0, 0 }, { -1616, 1840, 0, 0 } }, { 0, 0, 4105, 0 }, { 0, 0, ONE, 0 }, 2442, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3A3F, -2720, -8402, 0 }, { { 65, -1824, -1727, 0 }, { -64, -1824, 1728, 0 }, { 65, 1824, -1727, 0 }, { -64, 1824, 1728, 0 } }, { 4101, 0, 151, 0 }, { 0, 0, ONE, 0 }, 2508, 0, 3, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3ABE, -2736, -8307, 0 }, { { -81, -1808, 1758, 0 }, { 81, -1808, -1757, 0 }, { -81, 1808, 1758, 0 }, { 81, 1808, -1757, 0 } }, { -ONE, 0, -190, 0 }, { 0, 0, ONE, 0 }, 2521, 0, 9, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9023, -2688, -8449, 0 }, { { 0, -1824, 1615, 0 }, { 1, -1824, -1616, 0 }, { 0, 1824, 1615, 0 }, { 1, 1824, -1616, 0 } }, { -4102, 0, -2, 0 }, { 0, 0, ONE, 0 }, 2428, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8799, -2736, -8577, 0 }, { { -79, -1808, -1614, 0 }, { 78, -1808, 1613, 0 }, { -79, 1808, -1614, 0 }, { 78, 1808, 1613, 0 } }, { 4091, 0, -200, 0 }, { 0, 0, ONE, 0 }, 2415, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5214, -2656, -0x27A2, 0 }, { { 1616, -1824, 1, 0 }, { -1615, -1824, 0, 0 }, { 1616, 1824, 1, 0 }, { -1615, 1824, 0, 0 } }, { -2, 0, 4100, 0 }, { 0, 0, ONE, 0 }, 2428, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5150, -2624, -9954, 0 }, { { -1615, -1888, 0, 0 }, { 1616, -1888, 1, 0 }, { -1615, 1888, 0, 0 }, { 1616, 1888, 1, 0 } }, { 0, 0, -4102, 0 }, { 0, 0, ONE, 0 }, 2482, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3230, -2624, -8482, 0 }, { { -207, -1792, -2412, 0 }, { 207, -1792, 2413, 0 }, { -207, 1792, -2412, 0 }, { 207, 1792, 2413, 0 } }, { 4094, 0, -353, 0 }, { 0, 0, ONE, 0 }, 3007, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3454, -2624, -8513, 0 }, { { 159, -1856, 2416, 0 }, { -159, -1856, -2415, 0 }, { 159, 1856, 2416, 0 }, { -159, 1856, -2415, 0 } }, { -4100, 0, 269, 0 }, { 0, 0, ONE, 0 }, 3050, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -960, -2592, -8577, 0 }, { { 306, -1856, 1579, 0 }, { -322, -1856, -1589, 0 }, { 306, 1856, 1579, 0 }, { -322, 1856, -1589, 0 } }, { -4028, 0, 798, 0 }, { 0, 0, ONE, 0 }, 2455, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1056, -2528, -8385, 0 }, { { -322, -1856, -1592, 0 }, { 308, -1856, 1578, 0 }, { -322, 1856, -1592, 0 }, { 308, 1856, 1578, 0 } }, { 4028, 0, -802, 0 }, { 0, 0, ONE, 0 }, 2455, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3130, -2592, -8513, 0 }, { { 17, -1824, 1905, 0 }, { -16, -1824, -1904, 0 }, { 17, 1824, 1905, 0 }, { -16, 1824, -1904, 0 } }, { -4104, 0, 35, 0 }, { 0, 0, ONE, 0 }, 2635, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3061, -2528, -8577, 0 }, { { 1, -1824, -1615, 0 }, { 0, -1824, 1616, 0 }, { 1, 1824, -1615, 0 }, { 0, 1824, 1616, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2428, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40C0, -2624, -1029, 0 }, { { 1577, -1840, 182, 0 }, { -1630, -1840, -237, 0 }, { 1577, 1840, 182, 0 }, { -1630, 1840, -237, 0 } }, { -534, 0, 4073, 0 }, { 0, 0, ONE, 0 }, 2442, 0, 10, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40F1, -2624, -962, 0 }, { { -1854, -1840, -209, 0 }, { 1834, -1840, 184, 0 }, { -1854, 1840, -209, 0 }, { 1834, 1840, 184, 0 } }, { 434, 0, -4078, 0 }, { 0, 0, ONE, 0 }, 2610, 0, 2, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_CELLAR        = (1 << 4) | 1,
    DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_GENERAL_STORE = (3 << 4) | 3,
    DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_WATER_HOLE    = (2 << 4) | 2,
    DRYFIELD_NIGHT_UNDERPASS_TRIGGER_COMMAND_SECOND_SWITCH = 2,
    DRYFIELD_NIGHT_UNDERPASS_TRIGGER_COMMAND_FIRST_SWITCH  = 1,
};

/// 6 interaction quads: 4 warps, 2 room commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gDryfieldNightUnderpassActionTriggers[6] = {
    { NULL, NULL, NULL, { 5088, -1088, -0x2C70, 0 }, { { -1024, 0, -432, 0 }, { 1024, 0, -432, 0 }, { -1024, 0, 432, 0 }, { 1024, 0, 432, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, ONE, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_CELLAR, DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_CELLAR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1600, -1056, -7424, 0 }, { { -832, 0, -592, 0 }, { 768, 0, -592, 0 }, { -832, 0, 432, 0 }, { 768, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 1019, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_GENERAL_STORE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3B40, -1088, -3904, 0 }, { { 432, 0, -1024, 0 }, { 432, 0, 1024, 0 }, { -432, 0, -1024, 0 }, { -432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_WATER_HOLE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -1056, -8464, 0 }, { { -432, 0, -960, 0 }, { 432, 0, -960, 0 }, { -432, 0, 960, 0 }, { 432, 0, 960, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_UNDERPASS_TRIGGER_COMMAND_SECOND_SWITCH, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x40E0, -1056, -576, 0 }, { { -1024, 0, -304, 0 }, { 1024, 0, -304, 0 }, { -1024, 0, 304, 0 }, { 1024, 0, 304, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -ONE, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_UNDERPASS_TRIGGER_COMMAND_FIRST_SWITCH, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1648, -1056, -7184, 0 }, { { -1008, 0, -544, 0 }, { 1008, 0, -544, 0 }, { -1008, 0, 544, 0 }, { 1008, 0, 544, 0 } }, { 0, 4121, 0, 0 }, { 0, 0, -ONE, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE, DRYFIELD_NIGHT_UNDERPASS_TRIGGER_WARP_TO_GENERAL_STORE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// 3 sight-blocking room quads.
///
/// Origin and corner offsets use room-local game units; normals use 4096 per unit.
/// Room binding links and enables every record through LAST; storage stays live
/// until unlinking or room/view rebinding. The authored low flag bit is unproven.
static WorldCollisionOccluder _gDryfieldNightUnderpassSightOccluders[3] = {
    { NULL, NULL, { 1184, -2304, -0x2840, 0 }, { { -2912, -3328, 0, 0 }, { 2912, -3328, 0, 0 }, { -2912, 3328, 0, 0 }, { 2912, 3328, 0, 0 } }, { 0, 0, -4106, 0 }, 4404, 1, 0 },
    { NULL, NULL, { 0x2FB0, -2240, -0x2820, 0 }, { { -5840, -3264, 0, 0 }, { 5840, -3264, 0, 0 }, { -5840, 3264, 0, 0 }, { 5840, 3264, 0, 0 } }, { 0, 0, -4111, 0 }, 6675, 1, 0 },
    { NULL, NULL, { 8992, -3328, -4416, 0 }, { { -5840, -2848, 2496, 0 }, { 5840, -2848, -2496, 0 }, { -5840, 2848, 2496, 0 }, { 5840, 2848, -2496, 0 } }, { -1614, 0, -3777, 0 }, 6945, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Eight warm ceiling-lamp lights and one blue fill light for rooms 1, 3, 5 and 6.
///
/// Positions and falloff radii are world units; RGB intensities use Q12.
/// Every view admits each light. Composition and attenuation queries update
/// the transforms and contribution scales while this overlay is loaded.
static WorldCoordPointLight _gDryfieldNightUnderpassLampPointLights[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3214, -1973, -3920 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1966, 2621, 3276 }, { 0, 0 } }, 2500, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x44C0, -3483, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3C28, -3483, -6000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x44C0, -3483, -8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9300, -3483, -9600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x34BC, -3483, -7400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6200, -3483, -0x2904 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 500, -3483, -9500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4850, -3423, -7400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3440, 2457 }, { 0, 0 } }, 2000, 4000 },
};

/// Rooms 1, 3, 5 and 6's nine point lights, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightUnderpassLampRoomLights = {
    .pointLightCount = ARRAY_SIZE(_gDryfieldNightUnderpassLampPointLights),
    .pointLights     = _gDryfieldNightUnderpassLampPointLights,
};

/// Six cool point lights for the dimmer room variants 2 and 4.
///
/// Positions and falloff radii are world units; RGB intensities use Q12.
/// Every view admits each light. Full strength ends at the origin, then each
/// contribution fades toward its outer radius. Transforms and scales are mutable.
static WorldCoordPointLight _gDryfieldNightUnderpassDimPointLights[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4268, -2483, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 500, -2483, -8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3F48, -2483, -8000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2AF8, -2483, -8300 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5850, -2423, -9100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2088, 2457 }, { 0, 0 } }, 0, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x314C, -1973, -3920 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, ONE, ONE }, { 0, 0 } }, 0, 8000 },
};

/// Rooms 2 and 4's six point lights, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightUnderpassDimRoomLights = {
    .pointLightCount = ARRAY_SIZE(_gDryfieldNightUnderpassDimPointLights),
    .pointLights     = _gDryfieldNightUnderpassDimPointLights,
};

/// Placement keys and decimal file numbers for the first two encounter variants.
enum {
    DRYFIELD_NIGHT_UNDERPASS_GRAY_STALKER_RESOURCE_ID = 5,
    DRYFIELD_NIGHT_UNDERPASS_GRAY_STALKER_FILE_NUMBER = 5,
    DRYFIELD_NIGHT_UNDERPASS_SCAVENGER_RESOURCE_ID    = 16,
    DRYFIELD_NIGHT_UNDERPASS_SCAVENGER_FILE_NUMBER    = 16,
};

/// Variant 1's Gray Stalker load and spawn recipe, followed by the resource terminator.
///
/// Placement key 5 selects actor_400500 (stage-zero file 400500) and descriptor 0.
/// The recipe borrows this room overlay; its task descriptor and model borrow
/// the actor overlay, which must stay loaded throughout the spawned actor's life.
static AreaResource _gDryfieldNightUnderpassGrayStalkerResources[2] = {
    {
        .entryId        = DRYFIELD_NIGHT_UNDERPASS_GRAY_STALKER_RESOURCE_ID,
        .fileNumber     = DRYFIELD_NIGHT_UNDERPASS_GRAY_STALKER_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .taskTable      = &gActor400500TaskDesc,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Scavenger load and spawn recipe, followed by the resource terminator.
///
/// Placement key 16 selects actor_101600 (stage-zero file 101600) and descriptor 0.
/// All five authored Scavenger placements share this entry. Keep this room
/// loaded while using the recipe and the actor overlay loaded while actors live.
static AreaResource _gDryfieldNightUnderpassScavengerResources[2] = {
    {
        .entryId        = DRYFIELD_NIGHT_UNDERPASS_SCAVENGER_RESOURCE_ID,
        .fileNumber     = DRYFIELD_NIGHT_UNDERPASS_SCAVENGER_FILE_NUMBER,
        .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
        .taskIndex      = 0,
        .taskTable      = &gActor101600ScavengerTaskDesc,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    DRYFIELD_NIGHT_UNDERPASS_RESOURCE_MOSSBACK     = 11,
    DRYFIELD_NIGHT_UNDERPASS_FILE_MOSSBACK         = 11,
    DRYFIELD_NIGHT_UNDERPASS_RESOURCE_KNIGHT_GOLEM = 22,
    DRYFIELD_NIGHT_UNDERPASS_FILE_KNIGHT_GOLEM     = 22,
    DRYFIELD_NIGHT_UNDERPASS_RESOURCE_MIND_SUCKLER = 15,
    DRYFIELD_NIGHT_UNDERPASS_FILE_MIND_SUCKLER     = 15,
};

/// Mossback resource recipes for placement variant 3.
///
/// Keys match `gMapDryfieldFullUnderpassVariant3Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightUnderpassVariant3Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_UNDERPASS_RESOURCE_MOSSBACK, .fileNumber = DRYFIELD_NIGHT_UNDERPASS_FILE_MOSSBACK, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor101100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Knight Golem resource recipes for placement variant 7.
///
/// Keys match `gMapDryfieldFullUnderpassVariant7Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightUnderpassVariant7Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_UNDERPASS_RESOURCE_KNIGHT_GOLEM, .fileNumber = DRYFIELD_NIGHT_UNDERPASS_FILE_KNIGHT_GOLEM, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor402200GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Mind Suckler resource recipes for placement variant 11.
///
/// Keys match `gMapDryfieldFullUnderpassVariant11Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightUnderpassVariant11Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_UNDERPASS_RESOURCE_MIND_SUCKLER, .fileNumber = DRYFIELD_NIGHT_UNDERPASS_FILE_MIND_SUCKLER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor01500MindSucklerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightUnderpassAreaVariants[DRYFIELD_NIGHT_UNDERPASS_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldFullUnderpassVariant1Placements, .resources = _gDryfieldNightUnderpassGrayStalkerResources },
    [2]                    = { .placements = gMapDryfieldFullUnderpassVariant2Placements, .resources = _gDryfieldNightUnderpassScavengerResources },
    [3]                    = { .placements = gMapDryfieldFullUnderpassVariant3Placements, .resources = _gDryfieldNightUnderpassVariant3Resources },
    [7]                    = { .placements = gMapDryfieldFullUnderpassVariant7Placements, .resources = _gDryfieldNightUnderpassVariant7Resources },
    [11]                   = { .placements = gMapDryfieldFullUnderpassVariant11Placements, .resources = _gDryfieldNightUnderpassVariant11Resources },
};

/// Cue-2 bases in the currently loaded type-1 bank; cue 1 uses the next entry.
enum {
    DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_2_AND_4_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
    DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_2_AND_4_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3B,
    DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_3_WALK       = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_3_RUN        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
};

/// Footstep script bases shared by collision surface classes 2 and 4.
///
/// Walk and scripted jumps use entry 0x39; running uses 0x3B. Cue 1 adds one
/// to the cue-2 base. Requests select the loaded type-1 bank, not a fixed bank.
static WorldCollisionFootstepSounds _gDryfieldNightUnderpassSurfaceClass2And4FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_2_AND_4_WALK,
    .run          = DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_2_AND_4_RUN,
    .scriptedJump = DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_2_AND_4_WALK,
};

/// Footstep script bases for collision surface class 3.
///
/// Walk and scripted jumps use entry 0x35; running uses 0x37. Cue 1 adds one
/// to the cue-2 base. Requests select the loaded type-1 bank, not a fixed bank.
static WorldCollisionFootstepSounds _gDryfieldNightUnderpassSurfaceClass3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_3_WALK,
    .run          = DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_3_RUN,
    .scriptedJump = DRYFIELD_NIGHT_UNDERPASS_FOOTSTEP_CLASS_3_WALK,
};

/// Blocking, impact-enabled, pushback surface for classes 0, 1 and 5..7, with no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightUnderpassDefaultSurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4's blocking, impact-enabled, pushback surface with the shared class-2/4 cues.
static WorldCollisionSurfaceProperties _gDryfieldNightUnderpassSurfaceClass4Properties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightUnderpassSurfaceClass2And4FootstepSounds,
};

/// Class 2's blocking, impact-enabled, pushback surface with the shared class-2/4 cues.
static WorldCollisionSurfaceProperties _gDryfieldNightUnderpassSurfaceClass2Properties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightUnderpassSurfaceClass2And4FootstepSounds,
};

/// Class 3's blocking, impact-enabled, pushback surface with its own footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightUnderpassSurfaceClass3Properties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightUnderpassSurfaceClass3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightUnderpassSurfacePropertiesByClass[8] = {
    &_gDryfieldNightUnderpassDefaultSurfaceProperties,
    &_gDryfieldNightUnderpassDefaultSurfaceProperties,
    &_gDryfieldNightUnderpassSurfaceClass2Properties,
    &_gDryfieldNightUnderpassSurfaceClass3Properties,
    &_gDryfieldNightUnderpassSurfaceClass4Properties,
    &_gDryfieldNightUnderpassDefaultSurfaceProperties,
    &_gDryfieldNightUnderpassDefaultSurfaceProperties,
    &_gDryfieldNightUnderpassDefaultSurfaceProperties,
};

static void _dryfieldNightUnderpassInitializeRoom(Task* task);

#include "../../shared/underpass_switches_task.inc.c"

#include "../../shared/room/room_variants_underpass.inc.c"

#include "../../shared/underpass_switches_msg.inc.c"

#include "../../shared/underpass_sound_msg.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Rejects every key-item use request in the nighttime underpass.
///
/// Ignores all arguments and returns zero, selecting the item menu's
/// "no use now" response. No inventory or room state changes.
static s32 _dryfieldNightUnderpassRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unused)
{
    return 0;
}

/// Ignores room-action trigger requests in the nighttime underpass.
///
/// The borrowed `DirectionActionRequest` is neither read nor retained.
/// Returns zero; the direction dispatcher discards the result.
static s32 _dryfieldNightUnderpassIgnoreRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return 0;
}

/// Registers the night underpass's room-message receiver and enters its idle state.
///
/// Requires the live room task in state 0. The message table and room slot
/// borrow the loaded overlay and task for subsequent synchronous dispatch.
static void _dryfieldNightUnderpassInitializeRoom(Task* task)
{
    task->msgTable = _gDryfieldNightUnderpassRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized nighttime underpass room task alive and idle.
static void _dryfieldNightUnderpassIdleTask(Task* unusedTask)
{
}

/// Room-task handlers indexed by state 0..2.
///
/// 0 initializes the receiver, 1 waits for messages, 2 tears down.
/// Dispatch copies all three pointers to the stack; there is no bounds check
/// or terminator. Keep this overlay loaded until the task exits.
static const TaskFuncTable3 _gDryfieldNightUnderpassRoomStates = {
    { _dryfieldNightUnderpassInitializeRoom, _dryfieldNightUnderpassIdleTask, taskKill },
};

void dryfieldNightUnderpassRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightUnderpassRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare.inc.c"

void dryfieldNightUnderpassDrawFlaresTask(Task* unusedTask)
{
    enum {
        DRYFIELD_NIGHT_UNDERPASS_FLARE_TEXTURE_INDEX = 0,
        DRYFIELD_NIGHT_UNDERPASS_FLARE_RADIUS_SCALE  = 640,
    };

    s32            viewMask;
    s32            flareIndex;
    const SVECTOR* flarePosition;
    const s16*     flareViewMasks;

    viewMask = 1 << gGameSession->location.view;
    if (gameFlagGetNibble(GAME_FLAG_053) == 0) {
        // Each position's mask uses the view ID directly as its bit index.
        flareIndex     = 0;
        flarePosition  = _gDryfieldNightUnderpassFlarePositions;
        flareViewMasks = _gDryfieldNightUnderpassFlareViewMasks;
        for (; flareIndex < (s32)ARRAY_SIZE(_gDryfieldNightUnderpassFlarePositions); flarePosition++, flareIndex++, flareViewMasks++) {
            if (viewMask & *flareViewMasks) {
                _glowDrawFlare(flarePosition, DRYFIELD_NIGHT_UNDERPASS_FLARE_TEXTURE_INDEX, DRYFIELD_NIGHT_UNDERPASS_FLARE_RADIUS_SCALE);
            }
        }
    }
}
