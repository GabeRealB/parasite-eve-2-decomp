#include "rooms/dryfield_night_motel_loft.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_motel_loft_private.h"

#include "gameplay/display.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

/// Scratch-stack block the room's falling triangle is drawn from.
///
/// Each corner is staged in `corners` as a point of the triangle's own plane
/// and replaced in place by its world position, narrowed to signed 16-bit
/// coordinate units. One RTPT then projects the three together; the screen
/// positions go straight into the packet, so the block keeps none of them.
///
/// Reserve the whole block and release it before the drawer returns.
typedef struct {
    s32     otz;             // SZ3 / 4 of the projection, a quarter of the last corner's depth; ordering-table and blend depth
    s32     projectionFlags; // GTE FLAG word of the projection; bit 31 set drops the triangle
    SVECTOR corners[3];      // Local corner workspace, then the world positions supplied to the projection
} _DryfieldNightMotelLoftTriScratch;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelLoftTriScratch, 0x20);

/// Ceiling-light centres and reusable falling-shard placement for the night loft.
///
/// The six flare centres are world-space inputs to projection. Shard XYZ are
/// overwritten for each spawn and transformed by the current GsWSMATRIX;
/// effects snapshot the position before the next overwrite. All coordinates
/// use signed whole game units; none of the fourth halfwords is consumed.
typedef struct {
    const SVECTOR flareAnchors[6]; // Fixed world-space flare centres
    SVECTOR       shardSpawn;      // Mutable GsWSMATRIX input; each spawn overwrites XYZ
} _DryfieldNightMotelLoftFlareAndShardPoints;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelLoftFlareAndShardPoints, 0x38);

static WorldCollisionGrid _gDryfieldNightMotelLoftCollisionGrid;

static void _dryfieldNightMotelLoftDrawShard(const GfxCoord* coord, s16 radius, s16 shade);

static TmdBone _gDryfieldNightMotelLoftActor135400Model071ACSkeleton[1] = {
#include "assets/actor_135400_model_071AC_skeleton.inc"
};

static u32 _gDryfieldNightMotelLoftActor135400Model071ACPartVerts[1] = {
#include "assets/actor_135400_model_071AC_partVerts.inc"
};

static SVECTOR _gDryfieldNightMotelLoftActor135400Model071ACVerts[20] = {
#include "assets/actor_135400_model_071AC_verts.inc"
};

static u32 _gDryfieldNightMotelLoftActor135400Model071ACStream[106] = {
#include "assets/actor_135400_model_071AC_stream.inc"
};

TmdSource gDryfieldNightMotelLoftActor135400Model071AC = {
    0,
    800,
    0,
    1,
    _gDryfieldNightMotelLoftActor135400Model071ACPartVerts,
    _gDryfieldNightMotelLoftActor135400Model071ACVerts,
    &_gDryfieldNightMotelLoftActor135400Model071ACVerts[20],
    _gDryfieldNightMotelLoftActor135400Model071ACSkeleton,
    _gDryfieldNightMotelLoftActor135400Model071ACStream,
};

TaskMessageEntry gDryfieldNightMotelLoftMessageHandlers[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantMotelBalconyMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldNightMotelLoftRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, dryfieldNightMotelLoftIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, dryfieldNightMotelLoftCommandMessage },
    { ROOM_MESSAGE_SOUND, dryfieldNightMotelLoftPlaySoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gDryfieldNightMotelLoftCaptionSceneTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = dryfieldNightMotelLoftCaptionSceneTask,
    .data     = { .value = 0 },
};

enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_IDLE_CLIP         = 1,
    DRYFIELD_NIGHT_MOTEL_LOFT_STALKER_DROP_COMMAND        = 0,
    DRYFIELD_NIGHT_MOTEL_LOFT_STALKER_FLOOR_START_COMMAND = 1,
    DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_CAP_COMMAND       = 4,
    DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_ROOM              = 2,
    DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_FIRST_SOUND       = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 6),
    DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_SECOND_SOUND      = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, 7),
};

/// Equipped-weapon idle request used before the encounter's first CAP cue.
///
/// The interpreter copies this record and replaces source.index with the
/// equipped bank before dispatch. Clip 1 resets with world collision enabled;
/// blendFrames is ignored. Keep the room payload live until that instruction.
static AnimationPlayRequest _gDryfieldNightMotelLoftEncounterIdleAnimation = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_IDLE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Command zero starts placement zero's scripted Stalker group entrance.
///
/// The receiver ignores the authored stage/area context and latches request 1.
/// Borrowed synchronously after the first CAP cue; no payload pointer is kept.
static ActorCommand _gDryfieldNightMotelLoftStalkerDropCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT } },
    .command = DRYFIELD_NIGHT_MOTEL_LOFT_STALKER_DROP_COMMAND,
};

/// Unreferenced command-1/2 records in the loft's authored Stalker namespace.
///
/// Command 1 selects the group entrance's floor start; command 2 has no branch
/// there. No current consumer establishes these records' intended use.
static ActorCommand _gDryfieldNightMotelLoftUnusedStalkerCommands[2] = {
    { .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT } }, .command = DRYFIELD_NIGHT_MOTEL_LOFT_STALKER_FLOOR_START_COMMAND },
    { .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT } }, .command = 2 },
};

EvsCommand gDryfieldNightMotelLoftStalkerEncounterScript[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightMotelLoftEncounterIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = dryfieldNightMotelLoftSetCurrentRoom }, { .value = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_ROOM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gDryfieldNightMotelLoftStalkerDropCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_FIRST_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_NIGHT_MOTEL_LOFT_ENCOUNTER_SECOND_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static SVECTOR _gDryfieldNightMotelLoftJerryCanBarrierTemplateNormals[1] = {
#include "assets/dryfield_night_motel_loft_collision_01794_normals.inc"
};

static SVECTOR _gDryfieldNightMotelLoftJerryCanBarrierTemplateVerts[4] = {
#include "assets/dryfield_night_motel_loft_collision_01794_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelLoftJerryCanBarrierTemplateFaces[1] = {
#include "assets/dryfield_night_motel_loft_collision_01794_faces.inc"
};

static s16 _gDryfieldNightMotelLoftJerryCanBarrierTemplateCells[4] = {
#include "assets/dryfield_night_motel_loft_collision_01794_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelLoftJerryCanBarrierTemplateCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..3 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_LOFT_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelLoftJerryCanBarrierTemplateCells[(halfwordOffset)])
static s16* _gDryfieldNightMotelLoftJerryCanBarrierTemplateTable[2] = {
#include "assets/dryfield_night_motel_loft_collision_01794_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_LOFT_CELL_FACE_LIST_AT_OFFSET

/// One-face wall template beside the Jerry Can, copied into the live room mesh.
///
/// Rebuilding copies normal 0 and vertices 0..3's XYZ and the complete face;
/// collection shifts the restored vertices below the floor. The stored cell
/// index is retained with the asset, but this template is never activated.
/// Geometry uses integer room units and normals use ONE for unit length.
static WorldCollisionGrid _gDryfieldNightMotelLoftJerryCanBarrierTemplate = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelLoftJerryCanBarrierTemplateNormals,
    .vertices    = _gDryfieldNightMotelLoftJerryCanBarrierTemplateVerts,
    .faces       = _gDryfieldNightMotelLoftJerryCanBarrierTemplateFaces,
    .cellFaceIds = _gDryfieldNightMotelLoftJerryCanBarrierTemplateTable,
    .xBias       = 5872,
    .zBias       = 2740,
    .cellCountX  = 1,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelLoftJerryCanBarrierTemplateFaces),
};

/// Six ceiling-light flare anchors and one reusable shard-spawn point.
///
/// Flares read only the anchors; the two bursts overwrite shardSpawn.XYZ.
/// Both roles use signed halfword coordinates and retain each fourth halfword.
/// The loaded room owns the storage; shard setup snapshots its spawn position.
static _DryfieldNightMotelLoftFlareAndShardPoints _gDryfieldNightMotelLoftFlareAndShardPoints = {
    .flareAnchors = {
        { 5200, -2110, -1240, 0 },
        { -10, -2110, -1240, 0 },
        { -4600, -2110, -1240, 0 },
        { -4600, -2110, 1500, 0 },
        { -10, -2110, 1500, 0 },
        { 5200, -2110, 1500, 0 },
    },
    .shardSpawn = { 3000, -3420, 0, 0 },
};

WorldCoordRoomLighting gDryfieldNightMotelLoftRoomLighting[2] = {
    { .lights = &gDryfieldNightMotelLoftRoomLights, .ambientTable = NULL },
    { .lights = &gDryfieldNightMotelLoftRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightMotelLoftRoomCollisionResources[2] = {
    { .grid = &_gDryfieldNightMotelLoftCollisionGrid, .viewBoundaryTriggers = gDryfieldNightMotelLoftViewBoundaryTriggers, .actionTriggers = gDryfieldNightMotelLoftActionTriggers, .occluders = NULL },
    { .grid = &_gDryfieldNightMotelLoftCollisionGrid, .viewBoundaryTriggers = gDryfieldNightMotelLoftViewBoundaryTriggers, .actionTriggers = gDryfieldNightMotelLoftActionTriggers, .occluders = NULL },
};

enum { DRYFIELD_NIGHT_MOTEL_LOFT_LOGICAL_VIEW_COUNT = 14 };

/// Room 2's logical-to-mapped view indices after the encounter switches rooms.
///
/// Logical slots 2, 3 and 7 select mapped views 9, 10 and 11; the remaining
/// active slots retain their indices. Fourteen unsigned bytes are searched;
/// the two trailing stored zeros are outside that domain, not terminators.
/// The room directory borrows this mutable array while the overlay is loaded.
static u8 _gDryfieldNightMotelLoftRoom2ViewMap[16] = {
    1,
    9,
    10,
    4,
    5,
    6,
    11,
    8,
    2,
    3,
    7,
    12,
    13,
    14,
    0,
    0,
};

u8* gDryfieldNightMotelLoftViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldNightMotelLoftRoom2ViewMap,
};

ViewCount gDryfieldNightMotelLoftViewCounts[2] = {
    DRYFIELD_NIGHT_MOTEL_LOFT_LOGICAL_VIEW_COUNT,
    DRYFIELD_NIGHT_MOTEL_LOFT_LOGICAL_VIEW_COUNT,
};

enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_WARP_ARRIVAL_SOUND_ENTRY   = 2,
    DRYFIELD_NIGHT_MOTEL_LOFT_WARP_DEPARTURE_SOUND_ENTRY = 1,
    DRYFIELD_NIGHT_MOTEL_LOFT_WARP_INITIAL_VIEW          = 2,
    DRYFIELD_NIGHT_MOTEL_LOFT_WARP_MAP_FLAG              = 466,
};

DirectionWarpEntry gDryfieldNightMotelLoftWarpEntries[1] = {
    {
        .player         = { { .word = 0 }, 3953, 0, -2156 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 3953, 0, -2156 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, DRYFIELD_NIGHT_MOTEL_LOFT_WARP_ARRIVAL_SOUND_ENTRY),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, DRYFIELD_NIGHT_MOTEL_LOFT_WARP_DEPARTURE_SOUND_ENTRY),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_NIGHT_MOTEL_LOFT_WARP_INITIAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_MOTEL_LOFT_WARP_MAP_FLAG,
    },
};

static SVECTOR _gDryfieldNightMotelLoftCollisionGridNormals[9] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_normals.inc"
};

static SVECTOR _gDryfieldNightMotelLoftCollisionGridVerts[34] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelLoftCollisionGridFaces[18] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_faces.inc"
};

static s16 _gDryfieldNightMotelLoftCollisionGridCells[82] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelLoftCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..81 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_LOFT_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelLoftCollisionGridCells[(halfwordOffset)])
static s16* _gDryfieldNightMotelLoftCollisionGridTable[6] = {
#include "assets/dryfield_night_motel_loft_collision_01B60_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_LOFT_CELL_FACE_LIST_AT_OFFSET

/// Full room collision mesh shared by the two loft room selectors.
///
/// Normal 0, face 0 and vertices 0..3 reserve the movable Jerry Can wall;
/// rebuilding that prefix leaves the other geometry and cell lists intact.
/// Activation binds the view coordinate. Positions and the 4000-unit cells
/// use integer room units; normals use ONE for unit length. The overlay owns
/// all mutable storage and must remain loaded while this grid is active.
static WorldCollisionGrid _gDryfieldNightMotelLoftCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelLoftCollisionGridNormals,
    .vertices    = _gDryfieldNightMotelLoftCollisionGridVerts,
    .faces       = _gDryfieldNightMotelLoftCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelLoftCollisionGridTable,
    .xBias       = 6000,
    .zBias       = 2500,
    .cellCountX  = 3,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelLoftCollisionGridFaces),
};

ViewCamera gDryfieldNightMotelLoftViewCameras[11] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x6978, 0 } }, 541 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { 388, 0, 4077 }, { -370, 4079, 35 }, { -4060, -372, 386 } }, { -600, 740, 1510 } }, 257 },
    { { { { -3971, 0, -1003 }, { -512, 3522, 2027 }, { 863, 2090, -3414 } }, { 5710, 2360, -2340 } }, 257 },
    { { { { -800, 0, 4016 }, { 475, 4067, 94 }, { -3988, 484, -795 } }, { -960, 1500, -1900 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
    { { { { 2074, 0, 3531 }, { -2306, 3102, 1354 }, { -2674, -2674, 1571 } }, { -4300, 1910, 790 } }, 207 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
};

/// GPU addresses and neutral modulation shared by this TU's room-view sprites.
///
/// Pages use eight-bit textures at Y=0; palettes start at X=0 in rows 255..258.
enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896     = 142,
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960     = 143,
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255  = 0x3FC0,
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_256  = 0x4000,
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_257  = 0x4040,
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_258  = 0x4080,
    DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR = 128,
};

SpriteBatch gDryfieldNightMotelLoftView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch gDryfieldNightMotelLoftView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldNightMotelLoftView3SpriteSources[8] = {
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -48, -48, 2450, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 96 } }, -8, -56, 1325, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, 16, 8, 1300, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, 16, -56, 1425, { .fields = { 104, 184 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 104 } }, -16, -56, 1375, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 112 } }, -24, -64, 1600, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 88 } }, -32, -56, 1800, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 80 } }, -40, -56, 1975, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightMotelLoftView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(gDryfieldNightMotelLoftView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldNightMotelLoftView4SpriteSources[13] = {
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 120 } }, 24, -48, 1275, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 120 } }, 80, -120, 875, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 120 } }, 80, 0, 625, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 112 } }, 56, -112, 1150, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 104 } }, 56, 0, 825, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 80 } }, 40, -80, 1200, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 88 } }, 40, 0, 1025, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, 32, -56, 1250, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, 32, 0, 1250, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 120 } }, 112, -120, 700, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 120 } }, 112, 0, 525, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 120 } }, 136, -120, 625, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 120 } }, 136, 0, 475, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightMotelLoftView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(gDryfieldNightMotelLoftView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldNightMotelLoftView5SpriteSources[20] = {
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 80 } }, -160, -120, 600, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 104 } }, -160, -40, 750, { .fields = { 120, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 96 } }, -152, -120, 575, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 96 } }, -152, -24, 700, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 40, 104 } }, -120, -24, 675, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 40, 96 } }, -120, -120, 562, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, -80, -120, 625, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 72 } }, -80, -56, 700, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 72 } }, -80, 16, 750, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, -64, 16, 725, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, -64, -56, 750, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -64, -120, 675, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -56, -120, 700, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, -56, -56, 775, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -56, 16, 775, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -48, 0, 825, { .fields = { 24, 72 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -48, -64, 750, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -48, -120, 725, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -40, -64, 825, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, -40, 0, 900, { .fields = { 120, 208 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightMotelLoftView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(gDryfieldNightMotelLoftView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldNightMotelLoftView6SpriteSources[13] = {
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 96 } }, -16, -56, 1400, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 64 } }, -40, -64, 1400, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 64 } }, -40, 0, 1300, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 72 } }, -64, -72, 1050, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 88 } }, -64, 0, 1000, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 80 } }, -88, -80, 875, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 120 } }, -88, 0, 825, { .fields = { 104, 128 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 104 } }, -104, -88, 800, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 104 } }, -104, 16, 675, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 112 } }, -128, -96, 700, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 104 } }, -128, 16, 550, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 128 } }, -160, -112, 625, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 104 } }, -160, 16, 525, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightMotelLoftView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(gDryfieldNightMotelLoftView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldNightMotelLoftView7SpriteSources[25] = {
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -8, 8, 1500, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 0, -56, 1412, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, 0, -8, 1450, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, 8, -64, 1250, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, 8, -8, 1450, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, 16, -64, 1100, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 16, 72 } }, 16, -8, 1237, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, 32, -72, 1075, { .fields = { 64, 184 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 104 } }, 32, -8, 925, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 96 } }, 64, -80, 825, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 104 } }, 88, -88, 850, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 104 } }, 88, 16, 675, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 112 } }, 112, -96, 750, { .fields = { 104, 120 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 104 } }, 112, 16, 600, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 120 } }, 136, -104, 500, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 104 } }, 136, 16, 525, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 48 } }, 64, 16, 725, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, 64, 64, 700, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_256, { .fields = { 16, 104 } }, -72, 16, 400, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_256, { .fields = { 16, 104 } }, -88, 16, 387, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_256, { .fields = { 16, 104 } }, -104, 16, 375, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_256, { .fields = { 16, 104 } }, -120, 16, 387, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_256, { .fields = { 16, 104 } }, -136, 16, 400, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_257, { .fields = { 80, 96 } }, -120, 24, 625, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_896, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_258, { .fields = { 40, 88 } }, -96, -16, 1000, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
};

enum {
    DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FIRST_RANGE_COUNT  = 18,
    DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_SECOND_RANGE_COUNT = 5,
    DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_THIRD_RANGE_COUNT  = 1,
    DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_THIRD_RANGE_START  = DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FIRST_RANGE_COUNT + DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_SECOND_RANGE_COUNT,
    DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FOURTH_RANGE_START = DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_THIRD_RANGE_START + DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_THIRD_RANGE_COUNT,
};

SpriteBatch gDryfieldNightMotelLoftView7SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FIRST_RANGE_COUNT, 0, 0, { 3, 0 } },
    { DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FIRST_RANGE_COUNT, DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_SECOND_RANGE_COUNT, 0, 0, { 0, 0 } },
    { DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_THIRD_RANGE_START, DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_THIRD_RANGE_COUNT, 0, 0, { 2, 0 } },
    { DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FOURTH_RANGE_START, ARRAY_SIZE(gDryfieldNightMotelLoftView7SpriteSources) - DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_7_FOURTH_RANGE_START, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource gDryfieldNightMotelLoftView8SpriteSources[22] = {
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 40, 64 } }, -160, -80, 375, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -160, -16, 400, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -120, -64, 400, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -120, -24, 387, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -88, -48, 375, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -88, -16, 362, { .fields = { 32, 136 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -56, -32, 350, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -56, 8, 337, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -24, -8, 325, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -24, 24, 325, { .fields = { 32, 40 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, -160, -120, 475, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, -128, -120, 425, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -128, -56, 400, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, -96, -120, 412, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -96, -56, 400, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, -64, -120, 400, { .fields = { 96, 136 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 64 } }, -32, -120, 375, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 72 } }, -8, -120, 362, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -40, -56, 350, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -48, -40, 350, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -64, -24, 350, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_TPAGE_960, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -72, -8, 350, { .fields = { 0, 40 } }, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_MOTEL_LOFT_SPRITE_NEUTRAL_COLOR, 0 },
};

enum { DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_8_FIRST_RANGE_COUNT = 10 };

SpriteBatch gDryfieldNightMotelLoftView8SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_8_FIRST_RANGE_COUNT, 0, 0, { 1, 0 } },
    { DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_8_FIRST_RANGE_COUNT, ARRAY_SIZE(gDryfieldNightMotelLoftView8SpriteSources) - DRYFIELD_NIGHT_MOTEL_LOFT_VIEW_8_FIRST_RANGE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

void dryfieldNightMotelLoftRebuildJerryCanCollision(s32 collected)
{
    enum { DRYFIELD_NIGHT_MOTEL_LOFT_COLLECTED_BARRIER_Y_OFFSET = 3000 };
    WorldCollisionGrid*       liveGrid;
    const WorldCollisionGrid* templateGrid;
    SVECTOR                   displacement;
    s32                       geometryIndex;

    liveGrid     = &_gDryfieldNightMotelLoftCollisionGrid;
    templateGrid = &_gDryfieldNightMotelLoftJerryCanBarrierTemplate;

    // Restore only the template's obstacle prefix, leaving the room mesh intact.
    for (geometryIndex = 0; geometryIndex < (s32)ARRAY_SIZE(_gDryfieldNightMotelLoftJerryCanBarrierTemplateFaces); geometryIndex++) {
        liveGrid->normals[geometryIndex].vx = templateGrid->normals[geometryIndex].vx;
        liveGrid->normals[geometryIndex].vy = templateGrid->normals[geometryIndex].vy;
        liveGrid->normals[geometryIndex].vz = templateGrid->normals[geometryIndex].vz;
        liveGrid->faces[geometryIndex]      = templateGrid->faces[geometryIndex];
    }

    for (geometryIndex = 0; geometryIndex < (s32)ARRAY_SIZE(_gDryfieldNightMotelLoftJerryCanBarrierTemplateVerts); geometryIndex++) {
        liveGrid->vertices[geometryIndex].vx = templateGrid->vertices[geometryIndex].vx;
        liveGrid->vertices[geometryIndex].vy = templateGrid->vertices[geometryIndex].vy;
        liveGrid->vertices[geometryIndex].vz = templateGrid->vertices[geometryIndex].vz;
    }

    if (collected == 0) {
        displacement.vx = 0;
        displacement.vy = 0;
    } else {
        displacement.vx = 0;
        displacement.vy = DRYFIELD_NIGHT_MOTEL_LOFT_COLLECTED_BARRIER_Y_OFFSET;
    }
    displacement.vz = 0;

    // Move the wall below the floor once the pickup's saved state is removed.
    for (geometryIndex = 0; geometryIndex < (s32)ARRAY_SIZE(_gDryfieldNightMotelLoftJerryCanBarrierTemplateVerts); geometryIndex++) {
        liveGrid->vertices[geometryIndex].vx += displacement.vx;
        liveGrid->vertices[geometryIndex].vy += displacement.vy;
        liveGrid->vertices[geometryIndex].vz += displacement.vz;
    }
}

/// Attempts one falling-shard spawn using the room's reusable shard point.
///
/// `points` borrows the room's complete point storage; only shardSpawn's XYZ
/// changes. Four LCG draws select matrix-input XYZ in [3001,4024], [-3420,-2909],
/// [-1023,1024] and a radius of 16..79 coordinate units. Placement uses the
/// current GsWSMATRIX. The spawner snapshots XYZ and retains the overlay-owned
/// pointer, which this shard never reads again. Failure still consumes all draws.
static inline void _dryfieldNightMotelLoftSpawnShard(_DryfieldNightMotelLoftFlareAndShardPoints* points)
{
    enum { DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_MIN_RADIUS = 16,
           SHARD_RADIUS_JITTER_MASK                   = 0x3F };

    RANDOM_LCG_ADVANCE();
    points->shardSpawn.vx = 0xFB8 - (RANDOM_LCG_DRAW() & 0x3FF);
    RANDOM_LCG_ADVANCE();
    points->shardSpawn.vy = (RANDOM_LCG_DRAW() & 0x1FF) - 0xD5C;
    RANDOM_LCG_ADVANCE();
    points->shardSpawn.vz = 0x400 - (RANDOM_LCG_DRAW() & 0x7FF);
    RANDOM_LCG_ADVANCE();
    effectSpawn(EFFECT_NIGHT_MOTEL_LOFT_FALLING_SHARD, NULL, (RANDOM_LCG_DRAW() & SHARD_RADIUS_JITTER_MASK) + DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_MIN_RADIUS, &points->shardSpawn);
}

void dryfieldNightMotelLoftFlaresAndShardsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_WAIT_VIEW_8        = 0,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_WAIT_VIEWS_3_OR_10 = 1,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_DONE               = 2,
        DRYFIELD_NIGHT_MOTEL_LOFT_FIRST_SHARD_BURST_COUNT   = 48,
        DRYFIELD_NIGHT_MOTEL_LOFT_SECOND_SHARD_BURST_COUNT  = 32,
        DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE             = 0,
        DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE        = 0x300
    };
    s32 shardIndex;

    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
        case 9:
            glowDrawFlareClipped(&_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[0], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[5], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            break;
        case 3:
        case 10: {
            const SVECTOR* viewFlarePoints = &_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[1];

            glowDrawFlareClipped(&viewFlarePoints[0], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&viewFlarePoints[1], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&viewFlarePoints[3], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            // This second burst waits until the first burst has run in view 8.
            if (task->state == DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_WAIT_VIEWS_3_OR_10) {
                _DryfieldNightMotelLoftFlareAndShardPoints* spawnPoints;

                shardIndex  = 0;
                spawnPoints = PARENT_OF(&viewFlarePoints[-1], _DryfieldNightMotelLoftFlareAndShardPoints, flareAnchors);
                do {
                    _dryfieldNightMotelLoftSpawnShard(spawnPoints);
                    shardIndex++;
                } while (shardIndex < DRYFIELD_NIGHT_MOTEL_LOFT_SECOND_SHARD_BURST_COUNT);
                task->state = DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_DONE;
            }
            break;
        }
        case 4:
            glowDrawFlareClipped(&_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[2], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            break;
        case 6:
            glowDrawFlareClipped(&_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[3], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            break;
        case 7:
        case 11:
            glowDrawFlareClipped(&_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[4], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&_gDryfieldNightMotelLoftFlareAndShardPoints.flareAnchors[5], DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_LOFT_FLARE_RADIUS_SCALE);
            break;
        case 8:
            // Advance even if some of the 48 effect allocations fail.
            if (task->state == DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_WAIT_VIEW_8) {
                _DryfieldNightMotelLoftFlareAndShardPoints* spawnPoints;

                shardIndex  = 0;
                spawnPoints = &_gDryfieldNightMotelLoftFlareAndShardPoints;
                do {
                    _dryfieldNightMotelLoftSpawnShard(spawnPoints);
                    shardIndex++;
                } while (shardIndex < DRYFIELD_NIGHT_MOTEL_LOFT_FIRST_SHARD_BURST_COUNT);
                task->state = DRYFIELD_NIGHT_MOTEL_LOFT_SHARDS_WAIT_VIEWS_3_OR_10;
            }
            break;
    }
}

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

void dryfieldNightMotelLoftFallingShardTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_INITIALIZE  = 0,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FALLING     = 1,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADING      = 2,
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_RADIUS_MASK = 0xFFF, // Low twelve spawn-argument bits, in coordinate units
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_GRAVITY_Q12 = 0x180, // Added to Y velocity each airborne frame; ONE = 1.0 times speed
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADE_STEP   = 4,     // Grey levels lost per active frame after the first bounce
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_MIN_SHADE   = 5      // Release before drawing a dimmer shard
    };
    EffectWork* work          = task->spawnArg2.pointer;
    s16         effectControl = gRoomEffectState->effectControl;
    GfxCoord*   coord         = task->extra.coordBody->coord;
    SVECTOR     displacement;

    /// Composes spin, advances the shard, and draws its existing cached transform.
    ///
    /// Borrows distinct writable EffectWork, GfxCoord and SVECTOR pointers.
    /// Spin uses 4096 units per turn; speed scales the Q12 movement vector
    /// into parent-coordinate units. The output remains available for floor
    /// rollback. Drawing uses the existing composed cache after marking it
    /// dirty; no composition is performed here. Arguments are evaluated
    /// repeatedly and must have no side effects. Captures no caller locals.
    /// Expands to statements; invoke only in braced switch cases.
#define DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD(effectWork, coordNode, displacementOut) \
    gfxRotMatrixXYZ(&(coordNode)->coord, &(effectWork)->pos, GRAPHICS_ROTATION_COMPOSE);      \
    MatrixNormal(&(coordNode)->coord, &(coordNode)->coord);                                   \
    gte_lddp((u16)(effectWork)->scale);                                                       \
    gte_ldsv(&(effectWork)->move);                                                            \
    gte_gpf12();                                                                              \
    gte_stsv((displacementOut));                                                              \
    (coordNode)->coord.t[0]  += (displacementOut)->vx;                                        \
    (coordNode)->coord.t[1]  += (displacementOut)->vy;                                        \
    (coordNode)->coord.t[2]  += (displacementOut)->vz;                                        \
    (coordNode)->composeStamp = GRAPHICS_COORD_DIRTY;                                         \
    _dryfieldNightMotelLoftDrawShard((coordNode), (effectWork)->angle, (effectWork)->period);

    if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
            switch (task->state) {
                case DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_INITIALIZE:
                    // Reuse the spawn offset as per-frame Euler spin (4096 units/turn).
                    // move is a ONE-normalized velocity, scaled by scale's speed;
                    // gravity and bounces change its length. angle holds the
                    // radius in coordinate units, and period holds the grey level.
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = RANDOM_LCG_DRAW() & 0x7F;
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->scale = (RANDOM_LCG_DRAW() & 0x3F) + 0x40;
                    work->angle = task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_RADIUS_MASK;
                    RANDOM_LCG_ADVANCE();
                    work->period = (RANDOM_LCG_DRAW() & 0x7F) + 0x40;
                    VectorNormalSS(&work->move, &work->move);
                    RANDOM_LCG_ADVANCE();
                    work->pos.vx = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
                    RANDOM_LCG_ADVANCE();
                    work->pos.vy = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
                    RANDOM_LCG_ADVANCE();
                    work->pos.vz        = 0x100 - (RANDOM_LCG_DRAW() & 0x1FF);
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    task->state         = DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FALLING;
                    break;
                case DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FALLING:
                    DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD(work, coord, &displacement);
                    // Drawing precedes rollback; the first floor crossing starts fading.
                    if (coord->coord.t[1] > 0) {
                        coord->coord.t[0] -= displacement.vx;
                        coord->coord.t[1] -= displacement.vy;
                        coord->coord.t[2] -= displacement.vz;
                        work->move.vx      = work->move.vx >> 1;
                        work->move.vy      = -(work->move.vy >> 1);
                        work->move.vz      = work->move.vz >> 1;
                        task->state        = DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADING;
                    } else {
                        work->move.vy += DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_GRAVITY_Q12;
                    }
                    break;
                case DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADING:
                    work->period -= DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_FADE_STEP;
                    if (work->period < DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_MIN_SHADE) {
                        effectKillTask(work, task);
                        break;
                    }
                    DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD(work, coord, &displacement);
                    // The fading shard can still bounce and lose half its velocity.
                    if (coord->coord.t[1] > 0) {
                        coord->coord.t[0] -= displacement.vx;
                        coord->coord.t[1] -= displacement.vy;
                        coord->coord.t[2] -= displacement.vz;
                        work->move.vx      = work->move.vx >> 1;
                        work->move.vy      = -(work->move.vy >> 1);
                        work->move.vz      = work->move.vz >> 1;
                    } else {
                        work->move.vy += DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_GRAVITY_Q12;
                    }
                    break;
            }
        }
    } else {
        effectKillTask(work, task);
    }
}

#undef DRYFIELD_NIGHT_MOTEL_LOFT_STEP_AND_DRAW_SHARD

/// Translates a rotated shard corner into the cached transform's output space.
///
/// `corner` supplies a live, writable vector already rotated by `coord->workm`.
/// Its XYZ and the cache's initialized translation use integer game-coordinate
/// units. The cache must correspond to that rotation; this helper uses its
/// existing translation even when `composeStamp` marks it stale.
///
/// Each component sum keeps its low 16 bits as a signed coordinate, wrapping
/// rather than saturating. The unsigned halfword conversions bound each sum
/// to 0..131070 before narrowing, avoiding signed 32-bit translation overflow.
/// Writes only XYZ, leaving the vector's fourth halfword unchanged. Both
/// objects are borrowed for this call and must be disjoint; no pointer is
/// retained, and neither the coordinate nor GTE state is changed.
static inline void _dryfieldNightMotelLoftTranslateShardCorner(SVECTOR* corner, const GfxCoord* coord)
{
    corner->vx = (u16)corner->vx + (u16)coord->workm.t[0];
    corner->vy = (u16)corner->vy + (u16)coord->workm.t[1];
    corner->vz = (u16)corner->vz + (u16)coord->workm.t[2];
}

/// Draws a semi-transparent grey triangular shard in the coordinate's local YZ plane.
///
/// `radius` is in coordinate units (0..4095 for this task), and `shade` is an
/// RGB byte level (5..191 while the task draws). `coord->workm` must contain
/// the cached transform to apply; this drawer does not refresh it. Three
/// corners spaced by truncated thirds of a turn are scaled, transformed,
/// narrowed to signed 16-bit coordinates, and projected through `GsWSMATRIX`.
/// A projection with GTE FLAG bit 31 set consumes a packet but queues none.
/// Each queued triangle chooses blend mode 0 or 1 from the LCG and consumes
/// a `DR_TPAGE` command too. The frame arena must have room for both packets;
/// it retains them until drawing completes. The scratch reservation is
/// released on return.
static void _dryfieldNightMotelLoftDrawShard(const GfxCoord* coord, s16 radius, s16 shade)
{
    enum {
        // Truncated third of a turn, in 4096 angle units per turn.
        DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_CORNER_ANGLE_STEP = 0x555
    };
    _DryfieldNightMotelLoftTriScratch* scratch;
    POLY_F3*                           primitive;
    s32                                cornerIndex;

    SCRATCH_STACK_RESERVE_BLOCK(_DryfieldNightMotelLoftTriScratch);
    scratch = SCRATCH_STACK_CURSOR(_DryfieldNightMotelLoftTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (cornerIndex = 0; cornerIndex < (s32)ARRAY_SIZE(scratch->corners); cornerIndex++) {
        // Stage the corner on the unit circle of the local YZ plane, scale
        // it, then rotate it by the coordinate's cached matrix.
        scratch->corners[cornerIndex].vx = 0;
        scratch->corners[cornerIndex].vy = rsin(cornerIndex * DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_CORNER_ANGLE_STEP);
        scratch->corners[cornerIndex].vz = rcos(cornerIndex * DRYFIELD_NIGHT_MOTEL_LOFT_SHARD_CORNER_ANGLE_STEP);
        gte_lddp(radius);
        gte_ldsv(&scratch->corners[cornerIndex]);
        gte_gpf12();
        gte_stsv(&scratch->corners[cornerIndex]);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->corners[cornerIndex]);
        gte_rtv0();
        gte_stsv(&scratch->corners[cornerIndex]);
        // Translate in cached-transform space, preserving the low 16 bits.
        _dryfieldNightMotelLoftTranslateShardCorner(&scratch->corners[cornerIndex], coord);
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2]);
    gte_rtpt();
    primitive      = gGpuPrimCursor;
    gGpuPrimCursor = primitive + 1;
    setPolyF3(primitive);
    gte_stsxy3(&primitive->x0, &primitive->x1, &primitive->x2);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->otz);
        setRGB0(primitive, shade, shade, shade);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                primitive);
        RANDOM_LCG_ADVANCE();
        gpuSetPrimitiveBlendMode(primitive, RANDOM_LCG_DRAW() & 1, scratch->otz);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_DryfieldNightMotelLoftTriScratch);
}
