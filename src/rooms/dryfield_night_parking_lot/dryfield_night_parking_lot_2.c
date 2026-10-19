#include "rooms/dryfield_night_parking_lot.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_parking_lot_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room/room_events.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/parking_lot.h"

static void _dryfieldNightParkingLotDrawGreyCapsule(const SVECTOR* startWorldPoint, const SVECTOR* endWorldPoint, s32 radiusScale);

static WorldCollisionGrid     _gDryfieldNightParkingLotCollisionGrid;
static WorldCollisionOccluder _gDryfieldNightParkingLotOccluders[2];
static WorldCollisionTrigger  _gDryfieldNightParkingLotViewBoundaryTriggers[10];
static WorldCollisionTrigger  _gDryfieldNightParkingLotActionTriggers[12];
static WorldCoordRoomLights   _gDryfieldNightParkingLotRoomLights;

static AnimationPackedPose _gDryfieldNightParkingLotAnimation0166CBank1[10] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightParkingLotAnimation0166CBank4[95] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_bank4.inc"
};

static AnimationRecord _gDryfieldNightParkingLotAnimation0166CRecords[139] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_records.inc"
};

static u16 _gDryfieldNightParkingLotAnimation0166CIndices[20] = {
#include "assets/dryfield_night_parking_lot_animation_0166C_indices.inc"
};

static AnimationSet _gDryfieldNightParkingLotAnimation0166C = {
    _gDryfieldNightParkingLotAnimation0166CRecords,
    _gDryfieldNightParkingLotAnimation0166CIndices,
    { NULL, _gDryfieldNightParkingLotAnimation0166CBank1, NULL, NULL, _gDryfieldNightParkingLotAnimation0166CBank4, NULL, NULL, NULL },
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, ROOM_EVENT_TASK_PRIORITY } }, roomEventTask, { .value = 0 } };

const TaskMessageEntry gDryfieldNightParkingLotRoomMessages[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, parkingLotResolveMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldNightParkingLotRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, dryfieldNightParkingLotStartScavengerEncounterMessage },
    { ROOM_MESSAGE_COMMAND, dryfieldNightParkingLotCommandMessage },
    { ROOM_MESSAGE_SOUND, parkingLotSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Player-bank extension for the scavenger entrance: one clip, then a cleared slot.
///
/// Both words are copied, including NULL; this is not a terminated clip list.
/// The clip remains borrowed from this overlay while the player plays it.
static AnimationSet* _gDryfieldNightParkingLotScavengerEntranceAnimationSets[2] = {
    &_gDryfieldNightParkingLotAnimation0166C,
    NULL,
};

/// Installs the entrance clip at the equipped player bank's first extension slot.
///
/// Synchronous dispatch copies two words and clears the following slot.
static AnimationBankCopyRequest _gDryfieldNightParkingLotScavengerEntranceBankCopy = {
    .sets  = _gDryfieldNightParkingLotScavengerEntranceAnimationSets,
    .count = ARRAY_SIZE(_gDryfieldNightParkingLotScavengerEntranceAnimationSets),
};

/// Plays the installed scavenger-entrance clip with a reset pose and grid collision.
///
/// The event runner copies the request and replaces source.index with the
/// equipped weapon bank before dispatch; the stored selector is not used.
static AnimationPlayRequest _gDryfieldNightParkingLotScavengerEntranceAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Scavenger activation wave requested as the encounter's entrance begins.
enum { DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_ENTRANCE_WAVE = 1 };

enum { DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_ENTRANCE_CAP_COMMAND = 7,
       DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_RELEASE_WAIT_FRAMES  = 3 };

EvsCommand gDryfieldNightParkingLotScavengerEntranceScript[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_ENTRANCE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gDryfieldNightParkingLotScavengerEntranceBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = dryfieldNightParkingLotSetScavengerWave }, { .value = DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_ENTRANCE_WAVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightParkingLotScavengerEntranceAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_RELEASE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Ambient-effect gates for mapped camera indices 1..8, indexed by index - 1.
///
/// Enables ambient effects in mapped views 2..5; the other entries disable them.
/// The glow task copies the selected unsigned halfword into the room-effect state.
static u16 _gDryfieldNightParkingLotAmbientEffectGates[8] = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
};

/// World-space centres and endpoints shared by the parking lot's light glows.
///
/// XYZ are signed game-coordinate units. Logical views select overlapping
/// ranges of this catalog: 0..3 and 6..8 are flare centres, and 4..5 are
/// capsule endpoints. All nine entries are used across logical views 2..6.
static SVECTOR _gDryfieldNightParkingLotGlowPoints[9] = {
    { -7290, -2310, -110, 0 },
    { -6280, -2310, -110, 0 },
    { 1900, -1730, 3680, 0 },
    { 4100, -1730, 3680, 0 },
    { -1030, -2020, 150, 0 },
    { -570, -2020, 150, 0 },
    { -2950, -2480, 760, 0 },
    { 130, -2480, 2660, 0 },
    { 3930, -2480, 1380, 0 },
};

WorldCoordRoomLighting gDryfieldNightParkingLotRoomLighting[2] = {
    { .lights = &_gDryfieldNightParkingLotRoomLights, .ambientTable = NULL },
    { .lights = &_gDryfieldNightParkingLotRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightParkingLotRoomCollisionResources[2] = {
    { &_gDryfieldNightParkingLotCollisionGrid, _gDryfieldNightParkingLotViewBoundaryTriggers, _gDryfieldNightParkingLotActionTriggers, _gDryfieldNightParkingLotOccluders },
    { &_gDryfieldNightParkingLotCollisionGrid, _gDryfieldNightParkingLotViewBoundaryTriggers, _gDryfieldNightParkingLotActionTriggers, _gDryfieldNightParkingLotOccluders },
};

/// Room 2's seven logical views mapped to camera, image and sprite indices.
///
/// Views 1..7 index the first seven bytes; the retained eighth zero byte is
/// outside the view count and is not a terminator.
static u8 _gDryfieldNightParkingLotRoom2ViewMap[8] = {
    1,
    8,
    3,
    4,
    9,
    10,
    11,
    0,
};

u8* gDryfieldNightParkingLotViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldNightParkingLotRoom2ViewMap,
};

ViewCount gDryfieldNightParkingLotViewCounts[2] = { 7, 7 };

DirectionWarpEntry gDryfieldNightParkingLotWarpEntries[5] = {
    { { { .word = 3072 }, 0x2995, 0, 3077 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x2995, 0, 3077 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 2), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 1), DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 486 },
    { { { .word = 2048 }, 3200, 0, 3472 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 3200, 0, 3472 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 8), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 7), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 10), 5, DIRECTION_WARP_FLAG_NONE, 481 },
    { { { .word = 0 }, -285, 0, 351 }, { 0, 0, 0, 0 }, { { .word = 0 }, -285, 0, 351 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 6), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 5), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 10), 4, DIRECTION_WARP_FLAG_NONE, 480 },
    { { { .word = 2048 }, -6840, 0, -450 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -6840, 0, -450 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 8), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 7), SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_PARKING_LOT, 10), 2, DIRECTION_WARP_FLAG_NONE, 479 },
    { { { .word = 0 }, 4771, -450, 1414 }, { 0, 0, 0, 0 }, { { .word = 0 }, 4491, 0, 2215 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-axis mesh normals, with 4096 representing unit length.
static SVECTOR _gDryfieldNightParkingLotCollisionGridNormals[18] = {
#include "assets/dryfield_night_parking_lot_collision_02510_normals.inc"
};

/// Room-space collision vertices in signed game-coordinate units.
static SVECTOR _gDryfieldNightParkingLotCollisionGridVerts[132] = {
#include "assets/dryfield_night_parking_lot_collision_02510_verts.inc"
};

/// Collision faces indexing the grid's vertex/normal pools and surface classes 0..7.
static WorldCollisionGridFace _gDryfieldNightParkingLotCollisionGridFaces[55] = {
#include "assets/dryfield_night_parking_lot_collision_02510_faces.inc"
};

/// Packed signed face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
static s16 _gDryfieldNightParkingLotCollisionGridCells[436] = {
#include "assets/dryfield_night_parking_lot_collision_02510_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightParkingLotCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..435 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_PARKING_LOT_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightParkingLotCollisionGridCells[(halfwordOffset)])
/// X-major 9-by-5 directory into the grid's packed cell face-index lists.
static s16* _gDryfieldNightParkingLotCollisionGridTable[45] = {
#include "assets/dryfield_night_parking_lot_collision_02510_table.inc"
};
#undef DRYFIELD_NIGHT_PARKING_LOT_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by both logical rooms, with 4000-unit square XZ cells.
///
/// Activation binds the mutable descriptor to the view coordinate. Its pools
/// and cell directory belong to this overlay and must stay loaded while active.
static WorldCollisionGrid _gDryfieldNightParkingLotCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightParkingLotCollisionGridNormals,
    .vertices    = _gDryfieldNightParkingLotCollisionGridVerts,
    .faces       = _gDryfieldNightParkingLotCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightParkingLotCollisionGridTable,
    .xBias       = 17800,
    .zBias       = 8900,
    .cellCountX  = 9,
    .cellCountZ  = 5,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightParkingLotCollisionGridFaces),
};

ViewCamera gDryfieldNightParkingLotViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 30000, 0 } }, 380 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 10233, 2236, 6567 } }, 230 },
    { { { { 2646, 0, -3126 }, { -3085, 656, -2612 }, { 501, 4043, 424 } }, { 3012, 5975, 537 } }, 230 },
    { { { { 456, 0, 4070 }, { -262, 4087, 29 }, { -4062, -264, 455 } }, { -3566, 1036, -1232 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 10233, 2236, 6567 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
};

/// Eight-bit sprite pages at (896, 0)/(960, 0), palette rows and neutral RGB.
enum {
    DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE  = getTPage(1, 0, 896, 0),
    DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE = getTPage(1, 0, 960, 0),
    DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT        = getClut(0, 255),
    DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT    = getClut(0, 256),
    DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB = 128,
};

/// Mapped view 1's empty batch list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Ten depth-sorted sprite rectangles for room 1's logical view 2 (mapped view 2).
///
/// Positions are pixels relative to the view centre; depth is an unscaled
/// sorting value. The batch list selects all ten elements without a source
/// terminator. The loaded overlay owns these writable source records.
static SpriteSource _gDryfieldNightParkingLotView2SpriteSources[10] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, -160, 32, 500, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 80 } }, -104, 40, 500, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 104 } }, -80, 16, 500, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, -56, 32, 500, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 96 } }, 0, 24, 500, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 112 } }, 40, 8, 625, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 120 } }, 72, 0, 650, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 80 } }, 0, -32, 2750, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, 40, -32, 3000, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 80 } }, 96, -24, 6250, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 2's two sprite ranges, after the zero-count background record.
///
/// Ranges cover elements 0..6 and 7..9; the final record ends the list.
static SpriteBatch _gDryfieldNightParkingLotView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, ARRAY_SIZE(_gDryfieldNightParkingLotView2SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's pixel clip (136, 0, 182, 239), restored at sorting depth 2250.
///
/// Coordinates are relative to the draw buffer, before its VRAM Y offset.
/// The second record terminates the list and emits no drawing command.
static SpriteDrawArea _gDryfieldNightParkingLotView2SpriteDrawAreas[2] = {
    { { 136, 0, 182, 239 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Twenty-four depth-sorted sprite rectangles for logical view 3 in both rooms.
///
/// Mapped view 3 uses all elements through its batch list. Positions are
/// pixels relative to the view centre; depths are unscaled sorting values.
/// The loaded overlay owns the writable records; sources have no terminator.
static SpriteSource _gDryfieldNightParkingLotView3SpriteSources[24] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 120, 8 } }, -128, 72, 625, { .fields = { 40, 16 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 136, 8 } }, -40, 80, 625, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 88, 8 } }, 72, 88, 625, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 16, 1000, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 56 } }, -96, -40, 750, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -120, 625, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 32 } }, -112, -120, 625, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 24 } }, -64, -104, 625, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 24 } }, -32, -88, 625, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 24 } }, 0, -80, 625, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 24 } }, 16, -80, 925, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, -96, 925, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 16 } }, 0, -56, 925, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 8 } }, 8, -40, 1125, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 24 } }, 8, -32, 1350, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, -40, 925, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 96 } }, 40, -112, 925, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 56 } }, 56, -120, 925, { .fields = { 24, 56 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 56 } }, 56, -64, 925, { .fields = { 24, 112 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 56 } }, 112, -120, 925, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 56 } }, 112, -64, 925, { .fields = { 80, 152 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 24 } }, 64, -8, 925, { .fields = { 120, 24 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 24 } }, 112, -8, 925, { .fields = { 24, 200 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 40 } }, 104, 16, 925, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 3's complete sprite range, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightParkingLotView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sixteen depth-sorted sprite rectangles for logical view 4 in both rooms.
///
/// Mapped view 4 uses all elements through four batch ranges. The last six
/// elements use the alternate palette. Positions are view-centred pixels;
/// depths are unscaled sorting values. These writable sources have no terminator.
static SpriteSource _gDryfieldNightParkingLotView4SpriteSources[16] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 16 } }, 48, -88, 925, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 8 } }, 80, -80, 925, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 24 } }, 112, -72, 925, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 48 } }, -160, 72, 550, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 240 } }, -160, -120, 1000, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 240 } }, -144, -120, 1000, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 224 } }, -120, -120, 1000, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 176 } }, -96, -96, 1125, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 8, 136 } }, -80, -72, 1250, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 24 } }, -72, -56, 1250, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT, { .fields = { 16, 96 } }, -88, -48, 2000, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT, { .fields = { 24, 168 } }, -72, -112, 2000, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT, { .fields = { 8, 48 } }, 32, 8, 1657, { .fields = { 24, 168 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT, { .fields = { 8, 48 } }, 40, 8, 1470, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT, { .fields = { 16, 56 } }, 48, 0, 1407, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_ALT_CLUT, { .fields = { 16, 64 } }, 64, -8, 1419, { .fields = { 48, 176 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 4's four contiguous sprite ranges and terminal record.
///
/// The leading zero-count record retains decoded background strips. Ranges
/// select elements 0..3, 4..9, 10..11 and 12..15 without gaps or overlap.
static SpriteBatch _gDryfieldNightParkingLotView4SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 6, 0, 0, { 0, 0 } },
    { 10, 2, 0, 0, { 2, 0 } },
    { 12, ARRAY_SIZE(_gDryfieldNightParkingLotView4SpriteSources) - 12, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seven depth-sorted sprite rectangles for room 1's logical view 5 (mapped view 5).
///
/// The batch list selects all elements. Positions are view-centred pixels;
/// depths are unscaled sorting values. The loaded overlay owns the writable
/// sources, which have no terminator.
static SpriteSource _gDryfieldNightParkingLotView5SpriteSources[7] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 56 } }, -144, -104, 650, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 24 } }, -128, 88, 750, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 5's complete sprite range, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightParkingLotView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Twenty-one depth-sorted sprite rectangles for room 1's logical view 6.
///
/// Mapped view 6 selects all elements through two batches. Positions are
/// view-centred pixels; depths are unscaled sorting values. The loaded
/// overlay owns the writable sources, which have no terminator.
static SpriteSource _gDryfieldNightParkingLotView6SpriteSources[21] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, 64, 500, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, 64, 500, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, 24, 500, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, 24, 500, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, -16, 525, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, -16, 500, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, -56, 500, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, -56, 500, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, -80, 500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, -120, 1012, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, -72, 1012, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, -24, 1012, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, 24, 1012, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, 24, 1012, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -24, 1012, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -72, 1012, { .fields = { 32, 192 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -120, 1012, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -120, 1012, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -72, 1012, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -24, 1012, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 1012, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 6's ranges 0..8 and 9..20, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView6SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldNightParkingLotView6SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Room 1 logical view 7's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Ten depth-sorted sprite rectangles for room 2's logical view 2 (mapped view 8).
///
/// The batch list selects all ten elements. Positions are view-centred pixels;
/// depths are unscaled sorting values. This is separate writable storage from
/// mapped view 2's sources, even though the initial rectangle values agree.
static SpriteSource _gDryfieldNightParkingLotView8SpriteSources[10] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, -160, 32, 500, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 80 } }, -104, 40, 500, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 104 } }, -80, 16, 500, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, -56, 32, 500, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 96 } }, 0, 24, 500, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 112 } }, 40, 8, 625, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 120 } }, 72, 0, 650, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 80 } }, 0, -32, 2750, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, 40, -32, 3000, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_LEFT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 80 } }, 96, -24, 6250, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 8's ranges 0..6 and 7..9, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView8SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, ARRAY_SIZE(_gDryfieldNightParkingLotView8SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 8's pixel clip (136, 0, 182, 239), restored at sorting depth 2250.
///
/// Coordinates are relative to the draw buffer, before its VRAM Y offset.
/// The second record terminates the list and emits no drawing command.
static SpriteDrawArea _gDryfieldNightParkingLotView8SpriteDrawAreas[2] = {
    { { 136, 0, 182, 239 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Seven depth-sorted sprite rectangles for room 2's logical view 5 (mapped view 9).
///
/// The batch list selects all elements. Positions are view-centred pixels;
/// depths are unscaled sorting values. This is separate writable storage from
/// mapped view 5's sources, even though the initial rectangle values agree.
static SpriteSource _gDryfieldNightParkingLotView9SpriteSources[7] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 56 } }, -144, -104, 650, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 24 } }, -128, 88, 750, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 9's complete source range, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightParkingLotView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Six sprite rectangles for room 2's logical view 6 (mapped view 10).
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// All six elements are selected by the view's batch list.
static SpriteSource _gDryfieldNightParkingLotView10SpriteSources[6] = {
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 64 } }, 136, -80, 500, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 64 } }, 136, -16, 500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 56 } }, 112, -64, 500, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 72 } }, 112, -8, 500, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 72 } }, 80, -48, 500, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_PARKING_LOT_SPRITE_RIGHT_TPAGE, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 72 } }, 80, 24, 500, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_PARKING_LOT_SPRITE_NEUTRAL_RGB, 0 },
};

/// Mapped view 10's complete source range, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightParkingLotView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 11's empty sprite list, retaining decoded background strips.
static SpriteBatch _gDryfieldNightParkingLotView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightParkingLotSpriteViews[11] = {
    { { .empty = _gDryfieldNightParkingLotView1SpriteBatches }, _gDryfieldNightParkingLotView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightParkingLotView2SpriteSources }, _gDryfieldNightParkingLotView2SpriteBatches, _gDryfieldNightParkingLotView2SpriteDrawAreas },
    { { .elements = _gDryfieldNightParkingLotView3SpriteSources }, _gDryfieldNightParkingLotView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightParkingLotView4SpriteSources }, _gDryfieldNightParkingLotView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightParkingLotView5SpriteSources }, _gDryfieldNightParkingLotView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightParkingLotView6SpriteSources }, _gDryfieldNightParkingLotView6SpriteBatches, NULL },
    { { .empty = _gDryfieldNightParkingLotView7SpriteBatches }, _gDryfieldNightParkingLotView7SpriteBatches, NULL },
    { { .elements = _gDryfieldNightParkingLotView8SpriteSources }, _gDryfieldNightParkingLotView8SpriteBatches, _gDryfieldNightParkingLotView8SpriteDrawAreas },
    { { .elements = _gDryfieldNightParkingLotView9SpriteSources }, _gDryfieldNightParkingLotView9SpriteBatches, NULL },
    { { .elements = _gDryfieldNightParkingLotView10SpriteSources }, _gDryfieldNightParkingLotView10SpriteBatches, NULL },
    { { .empty = _gDryfieldNightParkingLotView11SpriteBatches }, _gDryfieldNightParkingLotView11SpriteBatches, NULL },
};

/// Seventeen point lights admitted in every logical view of both parking-lot rooms.
///
/// Local translations and falloff radii use world-coordinate units; RGB uses
/// 12 fractional bits. Coordinate setup parents the writable transforms to the
/// current view, and lighting queries overwrite attenuation. The room-light
/// collection counts the whole array, with no sentinel or spare entries.
static WorldCoordPointLight _gDryfieldNightParkingLotPointLights[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2680, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2211, 2457, 2703 }, { 0, 0 } }, 100000, 100000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8450, -1990, -5560 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1802, 1884, 1966 }, { 0, 0 } }, 3000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2970, -2190, 780 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3440, 3194 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3220, -2190, -4630 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3604, 3522, 3276 }, { 0, 0 } }, 2500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 130, -2190, 2660 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3440, 3194 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3930, -2190, 1380 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3440, 3194 }, { 0, 0 } }, 1000, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -730, -2010, 440 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2949, 2211 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3010, -1970, 2800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3850, 3440 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7290, -2010, 2980 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 2949, 2539 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2760, -2010, 2980 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3031, 2949, 2539 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7480, -1990, -1610 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3522, 3031 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 990, -1970, 2800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3850, 3440 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4990, -1970, 2800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3850, 3440 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7210, -1420, 3730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3850, 3440 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9250, -1420, 3730 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3850, 3440 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 640, -1520, 350 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2949, 2211 }, { 0, 0 } }, 500, 800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1500, -1520, 350 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2949, 2211 }, { 0, 0 } }, 500, 800 },
};

/// The point-light collection shared by both logical parking-lot rooms.
///
/// No directional or cone lights are supplied. The overlay owns the writable
/// point lights; coordinate setup and shading update their transform caches
/// and attenuation while the room is loaded.
static WorldCoordRoomLights _gDryfieldNightParkingLotRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightParkingLotPointLights),
    .pointLights           = _gDryfieldNightParkingLotPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Ten directed view boundaries shared by both logical rooms, ending with LAST.
///
/// Pairs connect logical views 2/3, 3/4, 4/5, 6/7 and 5/6. Room setup binds
/// and links every writable quad to the view-boundary list until the room retires.
static WorldCollisionTrigger _gDryfieldNightParkingLotViewBoundaryTriggers[10] = {
    { NULL, NULL, NULL, { -2698, -2736, -1594, 0 }, { { -2084, -3856, 2134, 0 }, { 2085, -3856, -2134, 0 }, { -2084, 3856, 2134, 0 }, { 2085, 3856, -2134, 0 } }, { -2935, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2816, -2592, -1600, 0 }, { { 2078, -3616, -2142, 0 }, { -2092, -3616, 2127, 0 }, { 2078, 3616, -2142, 0 }, { -2092, 3616, 2127, 0 } }, { 2941, 0, 2873, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2560, -2656, 1664, 0 }, { { 1854, -3680, -2342, 0 }, { -1877, -3680, 2312, 0 }, { 1854, 3680, -2342, 0 }, { -1877, 3680, 2312, 0 } }, { 3203, 0, 2568, 0 }, { 0, 0, 4096, 0 }, 4720, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2528, -2720, 1760, 0 }, { { -1873, -3744, 2314, 0 }, { 1857, -3744, -2338, 0 }, { -1873, 3744, 2314, 0 }, { 1857, 3744, -2338, 0 } }, { -3200, 0, -2566, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 831, -2528, 2015, 0 }, { { -694, -3552, 2899, 0 }, { 690, -3552, -2903, 0 }, { -694, 3552, 2899, 0 }, { 690, 3552, -2903, 0 } }, { -3985, 0, -951, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 798, -2416, 2046, 0 }, { { 690, -3440, -2903, 0 }, { -694, -3440, 2899, 0 }, { 690, 3440, -2903, 0 }, { -694, 3440, 2899, 0 } }, { 3998, 0, 953, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8191, -2320, 2527, 0 }, { { -34, -3344, -2982, 0 }, { 35, -3344, 2983, 0 }, { -34, 3344, -2982, 0 }, { 35, 3344, 2983, 0 } }, { 4106, 0, -48, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8223, -2256, 2399, 0 }, { { 35, -3280, 2983, 0 }, { -34, -3280, -2982, 0 }, { 35, 3280, 2983, 0 }, { -34, 3280, -2982, 0 } }, { -4098, 0, 47, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4048, -2912, 3200, 0 }, { { -500, -3856, 3222, 0 }, { 501, -3856, -3222, 0 }, { -500, 3856, 3222, 0 }, { 501, 3856, -3222, 0 } }, { -4059, 0, -631, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3968, -2848, 2864, 0 }, { { 396, -3856, -2554, 0 }, { -395, -3856, 2554, 0 }, { 396, 3856, -2554, 0 }, { -395, 3856, 2554, 0 } }, { 4054, 0, 627, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Descent option in the control word of this room's stair trigger.
enum { DRYFIELD_NIGHT_PARKING_LOT_STAIR_DESCEND = 0x100 };

/// Twelve interaction/transition quads shared by both logical rooms, ending with LAST.
///
/// Includes five exit warps, two stair traversals, the scavenger-entrance room
/// action and four CAP/room-command interactions. Setup binds and links the
/// mutable records; keep this overlay loaded until they are unlinked.
static WorldCollisionTrigger _gDryfieldNightParkingLotActionTriggers[12] = {
    { NULL, NULL, NULL, { 10352, -48, 2976, 0 }, { { -560, 0, -1024, 0 }, { 560, 0, -1024, 0 }, { -560, 0, 1024, 0 }, { 560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MAIN_STREET, 0x13, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3040, -48, 3744, 0 }, { { 832, 0, -560, 0 }, { 832, 0, 560, 0 }, { -832, 0, -560, 0 }, { -832, 0, 560, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1001, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_LOBBY, 0x21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -800, -48, 192, 0 }, { { 704, 0, -560, 0 }, { 704, 0, 560, 0 }, { -704, 0, -560, 0 }, { -704, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 898, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_TOILET, 0x31, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6736, -48, -640, 0 }, { { 656, 0, -560, 0 }, { 656, 0, 560, 0 }, { -656, 0, -560, 0 }, { -656, 0, 560, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 861, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_SALOON_G_R, 0x41, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -48, 1936, 0 }, { { 368, 0, -224, 0 }, { 368, 0, 224, 0 }, { -368, 0, -224, 0 }, { -368, 0, 224, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 430, WORLD_COLLISION_TRIGGER_ACTION_FACING, 52, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4640, -992, 1632, 0 }, { { -560, 1024, 0, 0 }, { 560, 1024, 0, 0 }, { -560, -1024, 0, 0 }, { 560, -1024, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 0x51, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -880, -64, 3184, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4076, 0, 401, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4672, -480, 1408, 0 }, { { 512, 0, -112, 0 }, { 512, 0, 112, 0 }, { -512, 0, -112, 0 }, { -512, 0, 112, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_FACING | DRYFIELD_NIGHT_PARKING_LOT_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, 50, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 18, -64, 2210, 0 }, { { 0, 0, -2160, 0 }, { 736, 0, -2160, 0 }, { -736, 0, 2160, 0 }, { 0, 0, 2160, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2275, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2080, -64, 2592, 0 }, { { 1232, 0, -416, 0 }, { 1232, 0, 416, 0 }, { -1232, 0, -416, 0 }, { -1232, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6992, -64, -1936, 0 }, { { 1472, 0, -192, 0 }, { 1472, 0, 928, 0 }, { -1472, 0, -928, 0 }, { -1472, 0, 192, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, 4091, 0 }, 1736, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5624, -64, -3000, 0 }, { { -200, 0, -1816, 0 }, { 920, 0, -1816, 0 }, { -904, 0, 1704, 0 }, { 184, 0, 1928, 0 } }, { 0, 4103, 0, 0 }, { 3857, 0, 1380, 0 }, 2031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space sight-blocking quads shared by both rooms, ending with LAST.
///
/// Linking changes these records in place; keep them loaded until unlinked.
static WorldCollisionOccluder _gDryfieldNightParkingLotOccluders[2] = {
    { NULL, NULL, { -160, -1872, -2944, 0 }, { { 1508, 2896, -2822, 0 }, { 1508, -2896, -2822, 0 }, { -1509, 2896, 2821, 0 }, { -1509, -2896, 2821, 0 } }, { 3613, 0, 1931, 0 }, 4314, 1, 0 },
    { NULL, NULL, { -6018, -1792, 3071, 0 }, { { 1509, 2896, -2821, 0 }, { 1509, -2896, -2821, 0 }, { -1508, 2896, 2822, 0 }, { -1508, -2896, 2822, 0 } }, { 3613, 0, 1931, 0 }, 4314, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Variant 2's Desert Chaser and Moth file/task recipes, keyed by placement ID.
///
/// The final entry is AREA_PLACEMENT_END; actor descriptors are borrowed from
/// their loaded packages during resource loading and placement spawning.
static AreaResource _gDryfieldNightParkingLotVariant2Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Scavenger file/task recipe, followed by AREA_PLACEMENT_END.
///
/// The descriptor is borrowed from the loaded actor package for the entrance
/// and battle placements; the resource entryId matches their placement ID.
static AreaResource _gDryfieldNightParkingLotVariant3Resources[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4's Desert Chaser and Scorpion recipes, followed by AREA_PLACEMENT_END.
///
/// Placement IDs select the file/task recipes; actor descriptors remain
/// borrowed from their loaded packages while loading and spawning placements.
static AreaResource _gDryfieldNightParkingLotVariant4Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, gActor02500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 7's GOLEM file/task recipe, followed by AREA_PLACEMENT_END.
///
/// Placement ID 39 selects the borrowed actor-package descriptor.
static AreaResource _gDryfieldNightParkingLotVariant7Resources[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor403900GolemTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 21's Bat file/task recipe, followed by AREA_PLACEMENT_END.
///
/// Placement ID 37 selects the borrowed actor-package descriptor.
static AreaResource _gDryfieldNightParkingLotVariant21Resources[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightParkingLotAreaVariants[22] = {
    [2]  = { .placements = gMapDryfieldFullParkingLotVariant2Placements, .resources = _gDryfieldNightParkingLotVariant2Resources },
    [3]  = { .placements = gMapDryfieldFullParkingLotVariant3Placements, .resources = _gDryfieldNightParkingLotVariant3Resources },
    [4]  = { .placements = gMapDryfieldFullParkingLotVariant4Placements, .resources = _gDryfieldNightParkingLotVariant4Resources },
    [7]  = { .placements = gMapDryfieldFullParkingLotVariant7Placements, .resources = _gDryfieldNightParkingLotVariant7Resources },
    [21] = { .placements = gMapDryfieldFullParkingLotVariant21Placements, .resources = _gDryfieldNightParkingLotVariant21Resources },
};

/// Cue-2 script bases in the loaded type-1 bank; cue 1 selects base + 1.
enum {
    DRYFIELD_NIGHT_PARKING_LOT_CLASS5_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_NIGHT_PARKING_LOT_CLASS5_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    DRYFIELD_NIGHT_PARKING_LOT_CLASS2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    DRYFIELD_NIGHT_PARKING_LOT_CLASS2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    DRYFIELD_NIGHT_PARKING_LOT_CLASS3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_NIGHT_PARKING_LOT_CLASS3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
    DRYFIELD_NIGHT_PARKING_LOT_CLASS3_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x55,
};

/// Walk/run footstep pairs for classes 5 and 6; scripted jumps use the walk pair.
///
/// Cue 1 adds one to the cue-2 base; companion playback adds 100. Surface
/// records borrow these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightParkingLotClass5FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_PARKING_LOT_CLASS5_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_PARKING_LOT_CLASS5_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_PARKING_LOT_CLASS5_WALK_SOUND,
};

/// Class 2's walk/run footstep pairs; scripted jumps use the walk pair.
///
/// Cue 1 adds one to the cue-2 base; companion playback adds 100. Surface
/// records borrow these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightParkingLotClass2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_PARKING_LOT_CLASS2_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_PARKING_LOT_CLASS2_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_PARKING_LOT_CLASS2_WALK_SOUND,
};

/// Class 3's separate walk, run and scripted-jump footstep pairs.
///
/// Cue 1 adds one to the cue-2 base; companion playback adds 100. Surface
/// records borrow these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldNightParkingLotClass3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_PARKING_LOT_CLASS3_WALK_SOUND,
    .run          = DRYFIELD_NIGHT_PARKING_LOT_CLASS3_RUN_SOUND,
    .scriptedJump = DRYFIELD_NIGHT_PARKING_LOT_CLASS3_JUMP_SOUND,
};

/// Blocking policy shared by classes 0, 4 and 7, without surface footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightParkingLotSilentBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 5's blocking policy and borrowed walk/run footstep pairs.
static WorldCollisionSurfaceProperties _gDryfieldNightParkingLotClass5Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightParkingLotClass5FootstepSounds,
};

/// Class 2's blocking policy and borrowed walk/run footstep pairs.
static WorldCollisionSurfaceProperties _gDryfieldNightParkingLotClass2Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightParkingLotClass2FootstepSounds,
};

/// Class 3's blocking policy and borrowed walk/run/scripted-jump footstep pairs.
static WorldCollisionSurfaceProperties _gDryfieldNightParkingLotClass3Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightParkingLotClass3FootstepSounds,
};

/// Class 1's probe-pass policy, with pushback but no weapon impacts or footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightParkingLotProbePassThroughSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 6's blocking policy, borrowing the same footstep pairs as class 5.
static WorldCollisionSurfaceProperties _gDryfieldNightParkingLotClass6Surface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightParkingLotClass5FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightParkingLotSurfacePropertiesByClass[8] = {
    &_gDryfieldNightParkingLotSilentBlockingSurface,
    &_gDryfieldNightParkingLotProbePassThroughSurface,
    &_gDryfieldNightParkingLotClass2Surface,
    &_gDryfieldNightParkingLotClass3Surface,
    &_gDryfieldNightParkingLotSilentBlockingSurface,
    &_gDryfieldNightParkingLotClass5Surface,
    &_gDryfieldNightParkingLotClass6Surface,
    &_gDryfieldNightParkingLotSilentBlockingSurface,
};

void dryfieldNightParkingLotDrawGlowsTask(Task* task)
{
    u8 mappedViewIndex;

    // The effect gate follows the mapped camera; light selection follows the logical view.
    mappedViewIndex                  = viewGetMappedIndex();
    gRoomEffectState->roomEffectMode = _gDryfieldNightParkingLotAmbientEffectGates[mappedViewIndex - 1];
    switch (gGameSession->location.view) {
        case 2: {
            const SVECTOR* viewPoints = _gDryfieldNightParkingLotGlowPoints;
            glowDrawFlareClipped(&viewPoints[0], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[1], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[2], 0, 0x330);
            glowDrawFlareClipped(&viewPoints[6], 1, 0x380);
            glowDrawFlareClipped(&viewPoints[7], 1, 0x380);
            break;
        }
        case 3: {
            const SVECTOR* viewPoints = &_gDryfieldNightParkingLotGlowPoints[6];
            glowDrawFlareClipped(&viewPoints[0], 1, 0x380);
            break;
        }
        case 4: {
            const SVECTOR* viewPoints = &_gDryfieldNightParkingLotGlowPoints[4];
            _dryfieldNightParkingLotDrawGreyCapsule(&viewPoints[0], &viewPoints[1], 0x180);
            glowDrawFlareClipped(&viewPoints[2], 1, 0x380);
            glowDrawFlareClipped(&viewPoints[3], 1, 0x380);
            break;
        }
        case 5: {
            const SVECTOR* viewPoints = &_gDryfieldNightParkingLotGlowPoints[2];
            glowDrawFlareClipped(&viewPoints[0], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[1], 0, 0x300);
            glowDrawFlareClipped(&viewPoints[5], 1, 0x380);
            glowDrawFlareClipped(&viewPoints[6], 1, 0x380);
            break;
        }
        case 6: {
            const SVECTOR* viewPoints = &_gDryfieldNightParkingLotGlowPoints[3];
            glowDrawFlareClipped(&viewPoints[0], 0, 0x300);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

/// Prepares the GPU header and colours of one grey capsule-cap quad.
///
/// Borrows one writable `POLY_G4` through `wedge` for this call. Vertex 2 receives
/// `centreIntensity` (0..255) in each RGB byte; vertices 0, 1 and 3 are black.
/// The packet has eight payload words and an opaque, untextured Gouraud-quad
/// command. Before submission, the caller fills all four screen positions,
/// supplies the ordering-table link, enables semitransparency and queues the
/// additive blend mode. Packet storage belongs to the caller's frame arena.
static inline void _dryfieldNightParkingLotInitCapsuleWedge(POLY_G4* wedge, u8 centreIntensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, centreIntensity, centreIntensity, centreIntensity);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws an additive grey capsule between separate world endpoints.
///
/// Borrows the endpoints for view projection. The second camera Z / 4 depth
/// must be at least 17; the first is clamped to 16. Projection flags are not
/// tested. Each pixel radius is the signed low halfword of `radiusScale`
/// times 64 divided by that endpoint's depth. Trigonometry uses Q12 values
/// and 4096 angle units per turn, with zero down the screen; caps have a fixed
/// screen orientation rather than following the projected endpoint direction.
///
/// Lit vertices alternate grey 32/48 with frame parity and rims are black.
/// Queues six Gouraud quads and their additive blend commands in the current
/// frame arena; joining bands sort at the first endpoint's depth. Requires
/// the composed view matrix, 24 scratch bytes and sufficient packet space.
static void _dryfieldNightParkingLotDrawGreyCapsule(const SVECTOR* startWorldPoint, const SVECTOR* endWorldPoint, s32 radiusScale)
{
    GlowPointPairScratch* projection;
    POLY_G4*              quad;
    s32                   angle;
    s32                   rimAngle;
    s32                   endRimAngle;
    s32                   nextAngle;
    s32                   endRimAngleForSin;
    s32                   brightness;
    s32                   scaledRadius;
    s32                   startRadius;
    s32                   endRadius;

    projection = SCRATCH_STACK_RESERVE_BLOCK(GlowPointPairScratch);

    // Project both endpoints; only the second depth rejects the capsule.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(startWorldPoint);
    gte_rtps();
    gte_stsxy(&projection->sx0);
    gte_stszotz(&projection->otz0);
    gte_ldv0(endWorldPoint);
    gte_rtps();
    gte_stsxy(&projection->sx1);
    gte_stszotz(&projection->otz1);
    if (projection->otz1 >= GLOW_MIN_DEPTH) {
        if (projection->otz0 < GLOW_NEAR_DEPTH_CLAMP) {
            projection->otz0 = GLOW_NEAR_DEPTH_CLAMP;
        }
        scaledRadius        = (s16)radiusScale * GLOW_RADIUS_SCALE;
        startRadius         = scaledRadius / projection->otz0;
        endRadius           = scaledRadius / projection->otz1;
        angle               = 0;
        brightness          = (((u8)gDisplayState.animFrame & 1) * (1 << GLOW_BRIGHT_FLICKER_SHIFT)) | GLOW_FLICKER_BASE_INTENSITY;
        projection->radius0 = startRadius;
        projection->radius1 = endRadius;
        // Join opposing half-disc caps with two bands at the first endpoint depth.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _dryfieldNightParkingLotInitCapsuleWedge(quad, brightness);
            quad->x0  = projection->sx0 + ((projection->radius0 * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y0  = projection->sy0 + ((projection->radius0 * rcos(angle)) >> GLOW_TRIG_SHIFT);
            rimAngle  = angle + GLOW_EIGHTH_TURN;
            quad->x1  = projection->sx0 + ((projection->radius0 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1  = projection->sy0 + ((projection->radius0 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle = angle + GLOW_QUARTER_TURN;
            quad->x2  = projection->sx0;
            quad->y2  = projection->sy0;
            quad->x3  = projection->sx0 + ((projection->radius0 * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3  = projection->sy0 + ((projection->radius0 * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz0);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, brightness, brightness, brightness);
            setRGB3(quad, brightness, brightness, brightness);
            quad->x0 = projection->sx0 + ((projection->radius0 * rsin((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->y0 = projection->sy0 + ((projection->radius0 * rcos((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->x1 = projection->sx1 + ((projection->radius1 * rsin((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->y1 = projection->sy1 + ((projection->radius1 * rcos((angle * 2))) >> GLOW_TRIG_SHIFT);
            quad->x2 = projection->sx0;
            quad->y2 = projection->sy0;
            quad->x3 = projection->sx1;
            quad->y3 = projection->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz0);

            endRimAngleForSin = GLOW_FULL_TURN - angle;
            quad              = gGpuPrimCursor;
            endRimAngle       = GLOW_FULL_TURN - angle;
            gGpuPrimCursor    = quad + 1;
            _dryfieldNightParkingLotInitCapsuleWedge(quad, brightness);
            quad->x0     = projection->sx1 + ((projection->radius1 * rsin(endRimAngleForSin)) >> GLOW_TRIG_SHIFT);
            quad->y0     = projection->sy1 + ((projection->radius1 * rcos(endRimAngle)) >> GLOW_TRIG_SHIFT);
            endRimAngle  = GLOW_FULL_TURN - GLOW_EIGHTH_TURN;
            endRimAngle -= angle;
            quad->x1     = projection->sx1 + ((projection->radius1 * rsin(endRimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1     = projection->sy1 + ((projection->radius1 * rcos(endRimAngle)) >> GLOW_TRIG_SHIFT);
            endRimAngle  = GLOW_FULL_TURN - GLOW_QUARTER_TURN;
            endRimAngle -= angle;
            quad->x2     = projection->sx1;
            quad->y2     = projection->sy1;
            quad->x3     = projection->sx1 + ((projection->radius1 * rsin(endRimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3     = projection->sy1 + ((projection->radius1 * rcos(endRimAngle)) >> GLOW_TRIG_SHIFT);
            angle        = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, projection->otz1);
        } while (angle < GLOW_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowPointPairScratch);
}
