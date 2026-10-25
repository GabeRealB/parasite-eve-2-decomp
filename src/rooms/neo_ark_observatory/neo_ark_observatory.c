#include "rooms/neo_ark_observatory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_450200.h"
#include "actors/actor_450200.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"
#include "../../shared/room/follow_collision.h"

static s32 _roomVariantResolveShelter(RoomEventMsg* request, RoomEventMsg* reply);

static WorldCollisionGrid _gNeoArkObservatoryCollisionGrid;

extern RoomDeparture gRoomDeparture;

/// Signed base grey intensity for the room's light beams, reset on the glow task's first tick.
///
/// The setter stores the low halfword without clamping. Drawing adds a
/// four-frame pulse of -4..4 and rejects negative results before narrowing RGB.
s16 _gNeoArkObservatoryLightBeamIntensity;

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
static void _neoArkObservatoryPlayerReflectionTask(Task* reflectionTask);
#include "../../shared/room/planar_reflection.h"

static void _neoArkObservatoryDrawLightBeam(const SVECTOR ringCenters[2], s32 outerRadius, s16 baseIntensity, s16 segmentCount);

/// Projects one beam segment's four world-space corners to screen pixels.
///
/// Borrows a live, word-aligned `quadScratch` block for this call. All four
/// vertices' XYZ components must be initialized as signed 16-bit world
/// coordinates in GPU quad strip order. Loads `gGfxViewCoord.workm`'s rotation;
/// the caller must load its translation and configure GTE projection first.
///
/// Writes all four `screenCorners` and the final RTPT `projectionFlags`, even
/// on rejection. Corner 0's RTPS flags are discarded; the caller rejects a
/// negative final FLAG. Leaves `vertices` and `depth` untouched, with corner
/// 3's depth in GTE SZ3 for reading before another depth-changing GTE command.
/// GTE state is not restored. Reserves no storage and retains no pointer.
static inline void _neoArkObservatoryProjectBeamSegment(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    // Save corner 0 before the triple transform replaces the screen FIFO.
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

static WorldCollisionOccluder _gNeoArkObservatorySightOccluders[4];
static WorldCollisionTrigger  _gNeoArkObservatoryViewBoundaryTriggers[18];
static WorldCollisionTrigger  _gNeoArkObservatoryActionTriggers[14];
static WorldCoordRoomLights   _gNeoArkObservatoryRoom1Lights;
static WorldCoordRoomLights   _gNeoArkObservatoryRoom2Lights;

static s32 _neoArkObservatoryRoomActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg);
static s32 _neoArkObservatoryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32 _neoArkObservatoryResolveRoomEventMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkObservatoryCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);

static void _neoArkObservatoryCapEventTask(Task* task);

#include "../../shared/room/planar_reflection_data.inc.c"

TaskDesc gNeoArkObservatoryReflectionTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, _neoArkObservatoryPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return gNeoArkObservatoryReflectionTaskDescs;
}

enum {
    NEO_ARK_OBSERVATORY_SCENE_CAP_COMMAND = 12,
};

/// Bodyless departure recipe consuming the room's staged destination at priority 32.
///
/// Spawning uses descriptor index 0 and no payload. The departure record and
/// room overlay must remain live until the departure task completes.
static TaskDesc _gNeoArkObservatoryDepartureTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventDepartureTask, { .value = 0 } };

static AnimationPackedPose _gNeoArkObservatoryAnimation03BC4Bank1[6] = {
#include "assets/neo_ark_observatory_animation_03BC4_bank1.inc"
};

static AnimationPackedRotation _gNeoArkObservatoryAnimation03BC4Bank4[64] = {
#include "assets/neo_ark_observatory_animation_03BC4_bank4.inc"
};

static AnimationRecord _gNeoArkObservatoryAnimation03BC4Records[141] = {
#include "assets/neo_ark_observatory_animation_03BC4_records.inc"
};

static u16 _gNeoArkObservatoryAnimation03BC4Indices[20] = {
#include "assets/neo_ark_observatory_animation_03BC4_indices.inc"
};

static AnimationSet _gNeoArkObservatoryAnimation03BC4 = {
    _gNeoArkObservatoryAnimation03BC4Records,
    _gNeoArkObservatoryAnimation03BC4Indices,
    { NULL, _gNeoArkObservatoryAnimation03BC4Bank1, NULL, NULL, _gNeoArkObservatoryAnimation03BC4Bank4, NULL, NULL, NULL },
};

/// Bodyless alternate-CAP playback recipe at priority 192.
///
/// Descriptor index 0 accepts the command index in spawn argument 1. Playback
/// uses the room's second CAP resource, then restores player control and the
/// default CAP resources. Keep the room loaded through completion.
static TaskDesc _gNeoArkObservatoryCapEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _neoArkObservatoryCapEventTask, { .value = 0 } };

/// Message routes installed by the room task, including the end marker.
///
/// Resolve borrows request/reply room-event records; room action borrows a
/// DirectionActionRequest; key-item and command payloads are integers.
/// The receiver borrows this table and its callbacks while the room is loaded.
static TaskMessageEntry _gNeoArkObservatoryRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkObservatoryResolveRoomEventMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkObservatoryRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkObservatoryRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _neoArkObservatoryCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Player animation-set table for the one-time observatory scene, installed at id 47.
///
/// The fixed 32-word bank copy reads this pointer, the following copy request
/// and play request, and the first four commands of the scene script: 31 words
/// beyond this table's end. Keep those objects in their stored order and retain
/// the original transfer length. Only id 47 is played; the copied request and
/// script words are not clips. The room overlay owns the borrowed clip data.
static AnimationSet* _gNeoArkObservatoryScenePlayerAnimationSets[1] = { &_gNeoArkObservatoryAnimation03BC4 };

/// Installs the scene's player clip and adjacent raw words into the 32-word bank extension.
///
/// The count is the bank capacity, not the playable set count. The source span
/// continues through the next play request and four scene commands; the player
/// consumes the request synchronously and borrows the clip through playback.
static AnimationBankCopyRequest _gNeoArkObservatoryScenePlayerAnimationBankCopyRequest = { _gNeoArkObservatoryScenePlayerAnimationSets, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Blends the player into scene clip 47 over 15 frames with world collision enabled.
///
/// The event interpreter resolves bank selector 1 for the equipped weapon.
/// The preceding bank copy must complete before this borrowed request is used.
static AnimationPlayRequest _gNeoArkObservatoryScenePlayerAnimationRequest = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_ENABLE };

/// One-time observatory scene after Power Plant 1 is cleared, terminated by END.
///
/// Starts CAP command 12, installs and plays player clip 47, starts the room
/// sound, waits for a CAP cue and three frames, then releases scripted control.
/// The first four commands also lie in the animation copy's raw source span.
/// The interpreter borrows the script and payloads through completion or skip.
static EvsCommand _gNeoArkObservatorySceneScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = NEO_ARK_OBSERVATORY_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gNeoArkObservatoryScenePlayerAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gNeoArkObservatoryScenePlayerAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 9) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip cleanup for the one-time observatory scene, terminated by END.
///
/// Fades out, waits eight frames, cleans up the scene, fades back in, waits
/// eight frames and releases player scripted control. The interpreter borrows
/// this script while the room overlay remains loaded.
static EvsCommand _gNeoArkObservatorySceneSkipScript[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Companion-obstacle displacement in room axes and signed game units.
///
/// X stays zero and Z stays -200. Y is zero while the companion blocks movement,
/// otherwise +10000 moves the obstacle out of the playable room. The rebuild
/// borrows XYZ for its call; the fourth halfword is unused.
static SVECTOR _gNeoArkObservatoryCompanionObstacleOffset = { 0, 0, -200, 0 };

/// Four model-local obstacle normals, with 4096 representing unit length.
static SVECTOR _gNeoArkObservatoryCompanionObstacleTemplateNormals[4] = {
#include "assets/neo_ark_observatory_collision_03E50_normals.inc"
};

/// Eight model-local obstacle corners in signed game-coordinate units.
static SVECTOR _gNeoArkObservatoryCompanionObstacleTemplateVerts[8] = {
#include "assets/neo_ark_observatory_collision_03E50_verts.inc"
};

/// Four side quads using vertices 0..7, normals 0..3 and room surface class 0.
static WorldCollisionGridFace _gNeoArkObservatoryCompanionObstacleTemplateFaces[4] = {
#include "assets/neo_ark_observatory_collision_03E50_faces.inc"
};

/// Single-cell face list (0..3, END) and the final retained zero halfword.
static s16 _gNeoArkObservatoryCompanionObstacleTemplateCells[6] = {
#include "assets/neo_ark_observatory_collision_03E50_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkObservatoryCompanionObstacleTemplateCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_OBSERVATORY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkObservatoryCompanionObstacleTemplateCells[(halfwordOffset)])
/// One-cell directory retained with the obstacle template; the rebuild uses only its mesh.
static s16* _gNeoArkObservatoryCompanionObstacleTemplateTable[1] = {
#include "assets/neo_ark_observatory_collision_03E50_table.inc"
};
#undef NEO_ARK_OBSERVATORY_CELL_FACE_LIST_AT_OFFSET

/// Model-local companion obstacle template restored into the room grid's leading slots.
///
/// Four normals, eight vertices and four quads remain unchanged by rebuilding.
/// The single 4000-unit cell has origin (-399, -500); its directory and lists
/// are retained asset data, unused by the rebuild. This template is never the
/// active room grid. The room overlay owns the record and all five pools.
static WorldCollisionGrid _gNeoArkObservatoryCompanionObstacleTemplate = { NULL, _gNeoArkObservatoryCompanionObstacleTemplateNormals, _gNeoArkObservatoryCompanionObstacleTemplateVerts, _gNeoArkObservatoryCompanionObstacleTemplateFaces, _gNeoArkObservatoryCompanionObstacleTemplateTable, 399, 500, 1, 1, 4000, ARRAY_SIZE(_gNeoArkObservatoryCompanionObstacleTemplateFaces) };

enum {
    NEO_ARK_OBSERVATORY_HIGH_GLOW_FIRST = 22,
    NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST  = 29,
};

/// World-space ring centres and glow points for the observatory's mapped views.
///
/// Elements 0..21 are eleven consecutive beam pairs, inner ring then outer
/// ring. Elements 22..28 are high glow points; 29..43 are low glow points.
/// View selections borrow interior pointers into this complete bank, so both
/// positive and negative element offsets stay within it. XYZ uses signed
/// 16-bit room coordinates; the fourth halfword is unused by the drawers.
static SVECTOR _gNeoArkObservatoryLightPositions[44] = {
    // Beam ring centres.
    { 512, -5120, 8192, 0 },
    { 512, -1024, 8192, 0 },
    { 1536, -5120, 8448, 0 },
    { 1536, -1024, 8448, 0 },
    { 2560, -5120, 8192, 0 },
    { 2560, -1024, 8192, 0 },
    { 3840, -5120, 9216, 0 },
    { 3840, -1024, 9216, 0 },
    { 4096, -5120, 8448, 0 },
    { 4096, -1024, 8448, 0 },
    { 3840, -5120, 7424, 0 },
    { 3840, -1024, 7424, 0 },
    { 768, -5120, 7936, 0 },
    { 768, -1024, 7936, 0 },
    { 1536, -5120, 7680, 0 },
    { 1536, -1024, 7680, 0 },
    { 2816, -5120, 7936, 0 },
    { 2816, -1024, 7936, 0 },
    { 4096, -3584, 1536, 0 },
    { 4096, 0, 1536, 0 },
    { 4096, -3584, 0x3600, 0 },
    { 4096, 0, 0x3600, 0 },
    // High glow points.
    { 0x2EE0, -3000, 0x2EE0, 0 },
    { 1000, -3000, 0x2EE0, 0 },
    { 8000, -3000, 0x2EE0, 0 },
    { 8000, -3000, 0x2710, 0 },
    { 8000, -3000, 8000, 0 },
    { 8000, -3000, 6000, 0 },
    { 8000, -3000, 4000, 0 },
    // Low glow points.
    { 5640, -210, 3030, 0 },
    { 5640, -210, 2580, 0 },
    { 5640, -210, 30, 0 },
    { 5640, -210, 0x325A, 0 },
    { 5640, -210, 0x341C, 0 },
    { 5640, -210, 0x3E12, 0 },
    { 4110, -70, 360, 0 },
    { 2380, -70, 1400, 0 },
    { 980, -70, 3160, 0 },
    { 60, -70, 5640, 0 },
    { -240, -70, 8000, 0 },
    { 70, -70, 0x292C, 0 },
    { 970, -70, 0x321E, 0 },
    { 2380, -70, 0x38FE, 0 },
    { 4120, -70, 0x3D0E, 0 },
};

WorldCollisionRoomResources gNeoArkObservatoryRoomCollisionResources[2] = {
    { &_gNeoArkObservatoryCollisionGrid, _gNeoArkObservatoryViewBoundaryTriggers, _gNeoArkObservatoryActionTriggers, _gNeoArkObservatorySightOccluders },
    { &_gNeoArkObservatoryCollisionGrid, _gNeoArkObservatoryViewBoundaryTriggers, _gNeoArkObservatoryActionTriggers, _gNeoArkObservatorySightOccluders },
};

WorldCoordRoomLighting gNeoArkObservatoryRoomLighting[2] = {
    { &_gNeoArkObservatoryRoom1Lights, NULL },
    { &_gNeoArkObservatoryRoom2Lights, NULL },
};

/// Logical-view map for room configuration 1, before the companion scene completes.
///
/// Views 4..6 select mapped views 16..18; the other logical views 1..21 retain
/// their indices. The final three stored zeros are outside the 21-view count.
static u8 _gNeoArkObservatoryRoom1ViewMap[24] = {
    1,
    2,
    3,
    16,
    17,
    18,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    0,
    0,
    0,
};

u8* gNeoArkObservatoryViewMaps[2] = {
    _gNeoArkObservatoryRoom1ViewMap,
    gViewIdentityMap,
};

ViewCount gNeoArkObservatoryViewCounts[2] = { ARRAY_SIZE(gNeoArkObservatoryViewCameras), ARRAY_SIZE(gNeoArkObservatoryViewCameras) };

DirectionWarpEntry gNeoArkObservatoryWarpEntries[3] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 0x34BC, 0, 0x2EE0 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 5 / 8 }, 0x2D82, 0, 0x316A }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 6), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 5), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 6700, 0, 0x37DC }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 6700, 0, 0x37DC }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 11, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 6490, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 6490, 0, 1700 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

enum { NEO_ARK_OBSERVATORY_COLLISION_CELLS_X = 4,
       NEO_ARK_OBSERVATORY_COLLISION_CELLS_Z = 5 };

/// Room-space collision normals; the first four are rebuilt for the companion obstacle.
static SVECTOR _gNeoArkObservatoryCollisionGridNormals[26] = {
#include "assets/neo_ark_observatory_collision_049E4_normals.inc"
};

/// Room-space collision vertices; the first eight are rebuilt for the companion obstacle.
static SVECTOR _gNeoArkObservatoryCollisionGridVerts[106] = {
#include "assets/neo_ark_observatory_collision_049E4_verts.inc"
};

/// Indexed room collision faces, with four leading slots reserved for the companion obstacle.
static WorldCollisionGridFace _gNeoArkObservatoryCollisionGridFaces[48] = {
#include "assets/neo_ark_observatory_collision_049E4_faces.inc"
};

/// Serialized face-index lists, each terminated by `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gNeoArkObservatoryCollisionGridCells[306] = {
#include "assets/neo_ark_observatory_collision_049E4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkObservatoryCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_OBSERVATORY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkObservatoryCollisionGridCells[(halfwordOffset)])
/// X-major cell directory; every cell includes the four companion-obstacle face slots.
static s16* _gNeoArkObservatoryCollisionGridTable[NEO_ARK_OBSERVATORY_COLLISION_CELLS_X * NEO_ARK_OBSERVATORY_COLLISION_CELLS_Z] = {
#include "assets/neo_ark_observatory_collision_049E4_table.inc"
};
#undef NEO_ARK_OBSERVATORY_CELL_FACE_LIST_AT_OFFSET

/// Complete room collision grid, including the mutable companion-obstacle slots.
///
/// The 4 by 5 XZ cells are 4000 game units wide, with room origin (0, 0).
/// The rebuild replaces only the first four normals/faces and eight vertices;
/// cell lists and the remaining room geometry stay intact. Activation borrows
/// the current view coordinate; the record and pools live with this overlay.
static WorldCollisionGrid _gNeoArkObservatoryCollisionGrid = { NULL, _gNeoArkObservatoryCollisionGridNormals, _gNeoArkObservatoryCollisionGridVerts, _gNeoArkObservatoryCollisionGridFaces, _gNeoArkObservatoryCollisionGridTable, 0, 0, NEO_ARK_OBSERVATORY_COLLISION_CELLS_X, NEO_ARK_OBSERVATORY_COLLISION_CELLS_Z, 4000, ARRAY_SIZE(_gNeoArkObservatoryCollisionGridFaces) };

ViewCamera gNeoArkObservatoryViewCameras[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x61A8, -8000 } }, 329 },
    { { { { -972, 0, -3978 }, { 543, 4057, -133 }, { 3941, -559, -963 } }, { -6480, 550, -0x305C } }, 289 },
    { { { { 3999, 0, -882 }, { -171, 4018, -775 }, { 865, 794, 3923 } }, { -7450, 2250, -5570 } }, 289 },
    { { { { -4077, 0, -389 }, { -13, 4093, 145 }, { 389, 146, -4074 } }, { -7450, 1570, -0x32B4 } }, 289 },
    { { { { -4072, 0, -434 }, { 55, 4062, -517 }, { 430, -520, -4039 } }, { -7450, 820, -9330 } }, 289 },
    { { { { -635, 0, -4046 }, { -426, 4073, 66 }, { 4023, 431, -632 } }, { -2310, 1470, -4610 } }, 289 },
    { { { { -3915, 0, -1202 }, { 195, 4041, -635 }, { 1186, -664, -3863 } }, { -660, 970, -0x2A9E } }, 289 },
    { { { { -200, 0, 4091 }, { 2927, 2861, 143 }, { -2858, 2930, -139 } }, { -5900, 5070, -8460 } }, 289 },
    { { { { 3816, 0, -1485 }, { 238, 4042, 613 }, { 1466, -658, 3767 } }, { -660, 970, -5150 } }, 289 },
    { { { { 912, 0, 3993 }, { 1882, 3612, -429 }, { -3521, 1930, 804 } }, { -0x2742, 2930, -450 } }, 289 },
    { { { { -821, 0, 4012 }, { 1903, 3605, 389 }, { -3532, 1942, -723 } }, { -0x2742, 2930, -0x3C6E } }, 289 },
    { { { { -481, 0, 4067 }, { -759, 4023, -89 }, { -3996, -764, -472 } }, { -0x3548, 550, -0x2FC6 } }, 289 },
    { { { { -1278, 0, -3891 }, { -2947, 2674, 967 }, { 2541, 3101, -834 } }, { -8640, 3230, -0x319C } }, 329 },
    { { { { 635, 0, 4046 }, { -751, 4024, 118 }, { -3975, -760, 624 } }, { -0x3548, 550, -0x2C4C } }, 289 },
    { { { { 1193, 0, -3918 }, { -3328, 2160, -1014 }, { 2066, 3479, 629 } }, { -8150, 3980, -0x2D32 } }, 329 },
    { { { { -4077, 0, -389 }, { -13, 4093, 145 }, { 389, 146, -4074 } }, { -7450, 1570, -0x32B4 } }, 289 },
    { { { { -4072, 0, -434 }, { 55, 4062, -517 }, { 430, -520, -4039 } }, { -7450, 820, -9330 } }, 289 },
    { { { { -635, 0, -4046 }, { -426, 4073, 66 }, { 4023, 431, -632 } }, { -2310, 1470, -4610 } }, 289 },
    { { { { -3915, 0, -1202 }, { 195, 4041, -635 }, { 1186, -664, -3863 } }, { -660, 970, -0x2A9E } }, 289 },
    { { { { -200, 0, 4091 }, { 2927, 2861, 143 }, { -2858, 2930, -139 } }, { -5900, 5070, -8460 } }, 289 },
    { { { { -803, 0, 4016 }, { 3001, 2721, 600 }, { -2668, 3061, -534 } }, { -3016, 3531, -7604 } }, 329 },
};

/// Neutral 8-bit sprite colour modulation, preserving sampled texture intensity.
enum { NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY = 128 };

/// Drawing ranges for mapped view 1, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled. No
/// source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Drawing ranges for mapped view 2, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled. No
/// source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 3.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView3SpriteSources[21] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1450, { .fields = { 48, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1450, { .fields = { 48, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1450, { .fields = { 48, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1450, { .fields = { 48, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1450, { .fields = { 24, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1450, { .fields = { 0, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1450, { .fields = { 0, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1450, { .fields = { 0, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -120, 1450, { .fields = { 16, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1450, { .fields = { 72, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 1450, { .fields = { 112, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -72, 1450, { .fields = { 112, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1450, { .fields = { 96, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 24, 1450, { .fields = { 96, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 24, 1450, { .fields = { 112, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 48, 1500, { .fields = { 16, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -24, 1450, { .fields = { 96, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -24, 1500, { .fields = { 96, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -72, 1500, { .fields = { 104, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -48, 1500, { .fields = { 16, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -120, 1500, { .fields = { 104, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Drawing ranges for mapped view 3, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkObservatoryView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 4.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView4SpriteSources[19] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 16, 1950, { .fields = { 80, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -32, 1950, { .fields = { 80, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -80, 1950, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, -120, 1950, { .fields = { 8, 88 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 64, -120, 1950, { .fields = { 8, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -80, 1950, { .fields = { 80, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -32, 1950, { .fields = { 72, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 16, 1950, { .fields = { 32, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 16, 1950, { .fields = { 64, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -32, 1950, { .fields = { 56, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -80, 1950, { .fields = { 64, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 1950, { .fields = { 40, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -120, 1950, { .fields = { 56, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -80, 1950, { .fields = { 72, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -32, 1950, { .fields = { 64, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 16, 1950, { .fields = { 120, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, 16, 2075, { .fields = { 56, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -32, 2075, { .fields = { 56, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -80, 2075, { .fields = { 56, 232 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Drawing ranges for mapped view 4, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkObservatoryView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 5.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView5SpriteSources[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -88, 1200, { .fields = { 120, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 56, 1125, { .fields = { 80, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1125, { .fields = { 80, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1125, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1125, { .fields = { 80, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1125, { .fields = { 120, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1125, { .fields = { 0, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1125, { .fields = { 48, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -40, 1125, { .fields = { 40, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 8, 1125, { .fields = { 88, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 56, 1125, { .fields = { 40, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 56, 1125, { .fields = { 48, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 8, 1125, { .fields = { 48, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -40, 1125, { .fields = { 64, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -88, 1125, { .fields = { 32, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1125, { .fields = { 24, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -120, 1200, { .fields = { 24, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -48, 1200, { .fields = { 112, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -40, 1200, { .fields = { 64, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 1200, { .fields = { 32, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 56, 1200, { .fields = { 40, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Drawing ranges for mapped view 5, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkObservatoryView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 6.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView6SpriteSources[82] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -80, 975, { .fields = { 24, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 987, { .fields = { 120, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 1275, { .fields = { 120, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 1000, { .fields = { 120, 56 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 912, { .fields = { 48, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -120, 931, { .fields = { 48, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 912, { .fields = { 40, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 931, { .fields = { 24, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 912, { .fields = { 32, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 931, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 912, { .fields = { 88, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 931, { .fields = { 72, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, 72, 912, { .fields = { 56, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, 72, 931, { .fields = { 8, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -160, 88, 912, { .fields = { 112, 72 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -120, 937, { .fields = { 16, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -72, 1000, { .fields = { 24, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -24, 1000, { .fields = { 16, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 24, 1000, { .fields = { 8, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -72, 1150, { .fields = { 8, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -24, 1150, { .fields = { 32, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1150, { .fields = { 40, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 1275, { .fields = { 0, 120 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, -16, 1275, { .fields = { 8, 232 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -88, 950, { .fields = { 104, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -88, 950, { .fields = { 112, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 1025, { .fields = { 80, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -120, 1025, { .fields = { 80, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 40, -120, 1037, { .fields = { 80, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -72, 1025, { .fields = { 88, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -24, 1025, { .fields = { 88, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 1025, { .fields = { 40, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 24, 1037, { .fields = { 24, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1025, { .fields = { 104, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, -120, 1025, { .fields = { 0, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, -120, 848, { .fields = { 0, 152 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, -120, 1125, { .fields = { 16, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 862, { .fields = { 16, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 831, { .fields = { 120, 136 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 1100, { .fields = { 120, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -120, 825, { .fields = { 24, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -72, 825, { .fields = { 32, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -24, 825, { .fields = { 16, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 104, 24, 825, { .fields = { 0, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 104, 72, 825, { .fields = { 96, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -120, 950, { .fields = { 112, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -72, 950, { .fields = { 112, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -24, 950, { .fields = { 32, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, 24, 950, { .fields = { 120, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, 72, 72, 950, { .fields = { 104, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1100, { .fields = { 8, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -24, 1100, { .fields = { 16, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -72, 1100, { .fields = { 64, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1100, { .fields = { 64, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -120, 1125, { .fields = { 72, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -72, 1125, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -24, 1125, { .fields = { 80, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 24, 1125, { .fields = { 8, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1125, { .fields = { 120, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1325, { .fields = { 120, 120 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 1187, { .fields = { 120, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 1187, { .fields = { 8, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -32, 1187, { .fields = { 8, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, 16, 1187, { .fields = { 16, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, 16, 1312, { .fields = { 32, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -32, 1312, { .fields = { 96, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -80, 1312, { .fields = { 96, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 64, 1175, { .fields = { 48, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 56, 1435, { .fields = { 32, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 0, 48, 1551, { .fields = { 88, 176 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, 40, 1633, { .fields = { 88, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 32, 1712, { .fields = { 104, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 64, 1303, { .fields = { 88, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 56, 1419, { .fields = { 88, 216 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 48, 1599, { .fields = { 88, 232 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 40, 1633, { .fields = { 88, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 32, 1712, { .fields = { 88, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 1303, { .fields = { 24, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 56, 1419, { .fields = { 96, 184 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 48, 1599, { .fields = { 96, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 40, 1633, { .fields = { 104, 208 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 1712, { .fields = { 104, 200 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
};

/// Drawing ranges for mapped view 6, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView6SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 26, 10, 0, 0, { 3, 0 } },
    { 36, 23, 0, 0, { 2, 0 } },
    { 59, 8, 0, 0, { 4, 0 } },
    { 67, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 7.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView7SpriteSources[83] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -160, 64, 1550, { .fields = { 16, 104 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 1550, { .fields = { 0, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -8, 1550, { .fields = { 56, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -48, 1550, { .fields = { 64, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -88, 1550, { .fields = { 56, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -128, -48, 1875, { .fields = { 104, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 1875, { .fields = { 0, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -72, 1875, { .fields = { 8, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -48, 2150, { .fields = { 88, 88 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -104, -8, 2150, { .fields = { 0, 136 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -104, 16, 2150, { .fields = { 32, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 2150, { .fields = { 32, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 1875, { .fields = { 104, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -112, 48, 1875, { .fields = { 24, 136 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, -48, 2325, { .fields = { 64, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -56, 2150, { .fields = { 8, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 80, 2375, { .fields = { 112, 88 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 32, 2375, { .fields = { 104, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, 32, 2375, { .fields = { 88, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 80, 2375, { .fields = { 8, 32 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 32, 2375, { .fields = { 104, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -16, 2375, { .fields = { 56, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -16, 2375, { .fields = { 56, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -16, 2375, { .fields = { 112, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -24, 2375, { .fields = { 88, 184 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -112, -24, 2375, { .fields = { 104, 216 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -24, 2375, { .fields = { 104, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 1250, { .fields = { 88, 136 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 1250, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 72, 1250, { .fields = { 104, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 1250, { .fields = { 48, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 40, 1250, { .fields = { 24, 72 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 1250, { .fields = { 0, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1250, { .fields = { 24, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -80, -8, 1250, { .fields = { 120, 120 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 1250, { .fields = { 112, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1250, { .fields = { 8, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 1250, { .fields = { 88, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -112, -88, 1250, { .fields = { 104, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 80, 812, { .fields = { 64, 200 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 96, 775, { .fields = { 0, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 40, 812, { .fields = { 64, 120 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 775, { .fields = { 112, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 800, { .fields = { 104, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 24, 850, { .fields = { 56, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 862, { .fields = { 96, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1062, { .fields = { 88, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 1300, { .fields = { 56, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -152, 32, 1300, { .fields = { 88, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 1200, { .fields = { 32, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -136, 32, 1200, { .fields = { 96, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, 32, 1062, { .fields = { 72, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 1062, { .fields = { 24, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -72, 2554, { .fields = { 88, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -48, 2350, { .fields = { 24, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, -48, 2350, { .fields = { 0, 40 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, -48, 2350, { .fields = { 24, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, -48, 2350, { .fields = { 56, 200 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -152, -64, 2425, { .fields = { 96, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, -64, 2425, { .fields = { 96, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, -64, 2425, { .fields = { 8, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -64, 2425, { .fields = { 0, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -64, 2562, { .fields = { 40, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, -72, 2500, { .fields = { 96, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -72, 2575, { .fields = { 40, 112 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 32, 987, { .fields = { 120, 88 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 72, 987, { .fields = { 56, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 72, 925, { .fields = { 40, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 88, 40, 987, { .fields = { 32, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 40, 925, { .fields = { 40, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 925, { .fields = { 80, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, 16, 1000, { .fields = { 112, 24 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 128, 16, 1000, { .fields = { 88, 232 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 1000, { .fields = { 8, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1096, { .fields = { 96, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, 48, 796, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 48, 24 } }, -160, 96, 789, { .fields = { 104, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -112, 48, 960, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -112, 96, 956, { .fields = { 56, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 48, 1106, { .fields = { 96, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 40 } }, -152, 48, 1437, { .fields = { 48, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, -136, 48, 1437, { .fields = { 40, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 48 } }, -112, 48, 1300, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Drawing ranges for mapped view 7, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView7SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 5, 0 } },
    { 27, 12, 0, 0, { 4, 0 } },
    { 39, 7, 0, 0, { 6, 0 } },
    { 46, 7, 0, 0, { 1, 0 } },
    { 53, 12, 0, 0, { 7, 0 } },
    { 65, 9, 0, 0, { 3, 0 } },
    { 74, 6, 0, 0, { 8, 0 } },
    { 80, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 8.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView8SpriteSources[94] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, 64, 812, { .fields = { 104, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 875, { .fields = { 120, 184 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 750, { .fields = { 120, 176 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 72, 750, { .fields = { 16, 32 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 56, 750, { .fields = { 16, 56 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 56, 750, { .fields = { 56, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 48, 750, { .fields = { 24, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 40, 750, { .fields = { 32, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 750, { .fields = { 72, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 32, 750, { .fields = { 64, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 32, 750, { .fields = { 64, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 32, 750, { .fields = { 40, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, 32, 750, { .fields = { 80, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, 40, 750, { .fields = { 80, 216 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 40, 750, { .fields = { 56, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, 48, 750, { .fields = { 64, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 64, 750, { .fields = { 16, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 750, { .fields = { 104, 56 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 80, 750, { .fields = { 48, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 96, 750, { .fields = { 96, 24 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, 104, 875, { .fields = { 72, 200 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, 96, 875, { .fields = { 96, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -112, 88, 875, { .fields = { 16, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 88, 875, { .fields = { 24, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 88, 875, { .fields = { 8, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, 88, 875, { .fields = { 96, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 88, 875, { .fields = { 96, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, 88, 875, { .fields = { 72, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 88, 875, { .fields = { 80, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 96, 875, { .fields = { 104, 176 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 72, 875, { .fields = { 88, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, 72, 875, { .fields = { 80, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 72, 875, { .fields = { 72, 184 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 72, 875, { .fields = { 72, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 72, 734, { .fields = { 64, 16 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, 72, 875, { .fields = { 72, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 56, 750, { .fields = { 96, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 812, { .fields = { 56, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 64, 875, { .fields = { 56, 176 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 64, 1236, { .fields = { 64, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 32, 625, { .fields = { 64, 200 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, 48, 625, { .fields = { 64, 208 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 625, { .fields = { 104, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 625, { .fields = { 64, 32 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 88, 625, { .fields = { 96, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 88, 625, { .fields = { 96, 32 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 88, 625, { .fields = { 96, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, 88, 625, { .fields = { 96, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 88, 625, { .fields = { 96, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 88, 625, { .fields = { 96, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 88, 625, { .fields = { 40, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 625, { .fields = { 40, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 625, { .fields = { 32, 32 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 56, 625, { .fields = { 32, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 56, 625, { .fields = { 32, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 56, 625, { .fields = { 48, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, 64, 625, { .fields = { 0, 104 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, 64, 625, { .fields = { 0, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 56, 625, { .fields = { 64, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 56, 625, { .fields = { 64, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 56, 625, { .fields = { 72, 160 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 56, 943, { .fields = { 64, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 56, 625, { .fields = { 64, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 128, 48, 625, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, 40, 625, { .fields = { 48, 216 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, 40, 625, { .fields = { 56, 232 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 40, 625, { .fields = { 0, 176 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 32, 625, { .fields = { 120, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 32, 625, { .fields = { 96, 104 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 80, 80, 800, { .fields = { 16, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, 104, 800, { .fields = { 24, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 8 } }, -136, 112, 800, { .fields = { 96, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -48, 48, 800, { .fields = { 88, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, 48, 800, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, 64, 800, { .fields = { 64, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -80, 48, 800, { .fields = { 64, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 16, 48, 800, { .fields = { 64, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, 16, 64, 800, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -80, 64, 800, { .fields = { 96, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -48, 64, 800, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -104, 56, 800, { .fields = { 48, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -112, 64, 800, { .fields = { 112, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, 48, 64, 800, { .fields = { 112, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 48, 88, 800, { .fields = { 72, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, 88, 800, { .fields = { 16, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -112, 88, 800, { .fields = { 80, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -136, 64, 800, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 24 } }, -136, 88, 800, { .fields = { 120, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 24 } }, -160, 88, 800, { .fields = { 120, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -160, 112, 800, { .fields = { 112, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -152, 80, 800, { .fields = { 24, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 80, 88, 800, { .fields = { 40, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 80, 104, 800, { .fields = { 40, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 16 } }, 112, 104, 800, { .fields = { 80, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Drawing ranges for mapped view 8, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView8SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 29, 0, 0, { 2, 0 } },
    { 69, 25, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 9.
///
/// Screen positions and dimensions are pixels relative to the view centre;
/// depth is the unscaled ordering value. Batches select source elements.
static SpriteSource _gNeoArkObservatoryView9SpriteSources[69] = {
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 32, 32, 2250, { .fields = { 112, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 80, 32, 2250, { .fields = { 88, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 32, 2250, { .fields = { 32, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 32, -16, 2250, { .fields = { 8, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 80, -16, 2250, { .fields = { 16, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -16, 2250, { .fields = { 40, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 32, -24, 2284, { .fields = { 120, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, -24, 1302, { .fields = { 0, 56 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 1200, { .fields = { 120, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1200, { .fields = { 112, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1200, { .fields = { 32, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1200, { .fields = { 80, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -88, 1200, { .fields = { 16, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -112, 1200, { .fields = { 88, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -72, 1200, { .fields = { 104, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, -80, 1200, { .fields = { 96, 248 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -24, 1200, { .fields = { 104, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 24, 1200, { .fields = { 80, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 72, 1200, { .fields = { 56, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 72, 1200, { .fields = { 64, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -24, 1200, { .fields = { 72, 96 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, -64, 1200, { .fields = { 96, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -16, 1200, { .fields = { 96, 208 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -8, 1200, { .fields = { 40, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -8, 1200, { .fields = { 112, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, -48, 1200, { .fields = { 120, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 24, 750, { .fields = { 32, 224 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 24, 737, { .fields = { 24, 208 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 16, 700, { .fields = { 24, 120 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 128, 16, 650, { .fields = { 40, 192 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 40, 700, { .fields = { 24, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 64, 700, { .fields = { 80, 208 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 96, 700, { .fields = { 24, 24 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 96, 700, { .fields = { 112, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 48, 700, { .fields = { 112, 128 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 1400, { .fields = { 40, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, 72, 1400, { .fields = { 16, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, 96, 1400, { .fields = { 56, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 112, 48, 1400, { .fields = { 64, 72 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, 32, 1400, { .fields = { 80, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 72, 1350, { .fields = { 56, 0 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 96, 1350, { .fields = { 56, 24 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 96, 1250, { .fields = { 56, 120 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 72, 1250, { .fields = { 48, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 48, 1250, { .fields = { 40, 72 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 48, 1350, { .fields = { 48, 232 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 80, 32, 1350, { .fields = { 16, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 1250, { .fields = { 56, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 32, -56, 2309, { .fields = { 80, 88 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, -56, 1985, { .fields = { 88, 48 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 112, -56, 1251, { .fields = { 72, 168 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1496, { .fields = { 16, 80 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 112, -16, 1283, { .fields = { 16, 72 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -24, 1216, { .fields = { 16, 64 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -16, 1604, { .fields = { 16, 88 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 32, -72, 2505, { .fields = { 48, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -64, 2772, { .fields = { 0, 152 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -72, 2578, { .fields = { 0, 240 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -56, 2763, { .fields = { 24, 144 } }, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, NEO_ARK_OBSERVATORY_SPRITE_NEUTRAL_INTENSITY, 0 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, 48, 650, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 24 } }, 112, 96, 650, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 48 } }, 80, 48, 700, { .fields = { 96, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, 80, 96, 700, { .fields = { 48, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 48 } }, 64, 48, 750, { .fields = { 0, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, 64, 96, 750, { .fields = { 96, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 56, 48, 800, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 48 } }, 56, 48, 1300, { .fields = { 88, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 24, 48 } }, 72, 48, 1300, { .fields = { 72, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 40 } }, 96, 48, 1350, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Drawing ranges for mapped view 9, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges index this view's source array in elements.
static SpriteBatch _gNeoArkObservatoryView9SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 18, 0, 0, { 4, 0 } },
    { 26, 9, 0, 0, { 1, 0 } },
    { 35, 13, 0, 0, { 5, 0 } },
    { 48, 11, 0, 0, { 0, 0 } },
    { 59, 7, 0, 0, { 6, 0 } },
    { 66, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 10.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView10SpriteSources[76] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 72, 1150, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 24, 1150, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -24, 1150, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -72, 1150, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -120, 1150, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 72, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 24, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -24, 1250, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -72, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -120, 1250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -120, 1325, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -72, 1325, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -40, 1395, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -24, 1325, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -24, 1395, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 24, 1325, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 72, 1325, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 72, 1395, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 24, 1395, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, -80, 1550, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 975, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 975, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 975, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 975, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 975, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -120, 1175, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -72, 1175, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -24, 1175, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 24, 1175, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 72, 1175, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, -120, 1350, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, -72, 1350, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 48, -72, 1550, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, -24, 1550, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, -24, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, 24, 1350, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 1350, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1225, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -8, 104, 1237, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 1137, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 96, 1150, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 88, 1150, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 1112, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 88, 1162, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 80, 1112, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 72, 1050, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, 104, 1287, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 104, 1300, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 96, 1212, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 88, 1150, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 80, 1100, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 40, 72, 1062, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 40, 64, 1062, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 96, 1162, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 88, 1137, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 80, 1087, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 72, 1075, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 64, 1050, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 104, 1137, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 112, 1137, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, -24, 1025, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, -24, 1025, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, 8, 1037, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, 8, 1037, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, 40, 1087, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, 80, 1100, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -40, 80, 1125, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 112, 1125, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 112, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 40, 1112, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, 40, 1087, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, 8, 1050, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -88, 80, 1175, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 1175, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -72, 48, 1187, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -72, 88, 1200, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
};

/// Drawing ranges for mapped view 10, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView10SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 18, 0, 0, { 0, 0 } },
    { 37, 21, 0, 0, { 5, 0 } },
    { 58, 14, 0, 0, { 1, 0 } },
    { 72, 2, 0, 0, { 4, 0 } },
    { 74, ARRAY_SIZE(_gNeoArkObservatoryView10SpriteSources) - 74, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 11.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView11SpriteSources[78] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, -80, 1450, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 72, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 24, 1200, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 1200, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -72, 1200, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -120, 1200, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -120, 1275, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -72, 1275, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -24, 1275, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, 24, 1275, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, 72, 1275, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 72, 1350, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 1350, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -24, 1350, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -72, 1350, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -120, 1350, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -72, 1450, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -24, 1450, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 24, 1450, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 72, 1450, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 1037, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 1037, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 1037, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 1037, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 1037, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, -120, 1212, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1212, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1212, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, 24, 1212, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 1370, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 1212, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -24, 1370, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -72, 1370, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -120, 1370, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -96, 1450, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 1500, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 1500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, 24, 1370, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 1036, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 104, 1275, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -40, 104, 1287, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -88, 104, 1287, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 104, 1287, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 96, 1275, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -88, 96, 1275, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 96, 1145, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 96, 1145, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 88, 1125, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 80, 1075, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 88, 1132, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 80, 1121, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 72, 1075, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -88, 88, 1145, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 80, 1132, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 72, 1120, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 64, 1075, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 88, 1145, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 1132, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 72, 1120, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 1075, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, 48, 1075, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, -24, 1025, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -24, 1025, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 0, 1037, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 0, 1037, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 24, 1050, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 48, 1075, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 72, 1087, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 96, 1100, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 96, 1075, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 72, 1075, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 1062, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 24, 1050, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 24, 1050, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 24, 80, 1250, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 1250, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 32, 80, 1200, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 32, 40, 1187, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

/// Drawing ranges for mapped view 11, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView11SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 18, 0, 0, { 0, 0 } },
    { 38, 22, 0, 0, { 5, 0 } },
    { 60, 14, 0, 0, { 1, 0 } },
    { 74, 2, 0, 0, { 4, 0 } },
    { 76, ARRAY_SIZE(_gNeoArkObservatoryView11SpriteSources) - 76, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 12.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView12SpriteSources[23] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -80, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 64, 1125, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 16, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -32, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -80, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, -104, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -112, -104, 1125, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -80, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -32, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 16, 1125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 64, 1125, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 64, 1125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 16, 1125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -32, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 16, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -32, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -32, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -80, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -80, 1125, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -104, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -104, 1125, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -104, 1125, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 64, 1125, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
};

/// Drawing ranges for mapped view 12, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView12SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkObservatoryView12SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Drawing ranges for mapped view 13, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// No source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 14.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView14SpriteSources[14] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 64, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 64, 1000, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 80, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 16, 1000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 16, 1000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -32, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -32, 1000, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -32, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -80, 1000, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -80, 1000, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -80, 1000, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, -120, 1000, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -112, -120, 1000, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -120, 1000, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
};

/// Drawing ranges for mapped view 14, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView14SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkObservatoryView14SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Drawing ranges for mapped view 15, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// No source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Drawing ranges for mapped view 16, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// No source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 17.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView17SpriteSources[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -88, 1200, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 56, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1125, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -40, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 8, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 56, 1125, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 56, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 8, 1125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -40, 1125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -88, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1125, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -120, 1200, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -48, 1200, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -40, 1200, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 1200, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 56, 1200, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
};

/// Drawing ranges for mapped view 17, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView17SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkObservatoryView17SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 18.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView18SpriteSources[82] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -80, 975, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 987, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 1275, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 1000, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 912, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -120, 931, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 912, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 931, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 912, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 931, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 912, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 931, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, 72, 912, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, 72, 931, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -160, 88, 912, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -120, 937, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -72, 1000, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -24, 1000, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 24, 1000, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -72, 1150, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -24, 1150, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1150, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 1275, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, -16, 1275, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -88, 950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -88, 950, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 1025, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -120, 1025, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 40, -120, 1037, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -72, 1025, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -24, 1025, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 1025, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 24, 1037, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1025, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, -120, 1025, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, -120, 848, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, -120, 1125, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 862, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 831, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 1100, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -120, 825, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -72, 825, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -24, 825, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 104, 24, 825, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 104, 72, 825, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -120, 950, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -72, 950, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -24, 950, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, 24, 950, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, 72, 72, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -24, 1100, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -72, 1100, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1100, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -120, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -72, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -24, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 24, 1125, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1325, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 1187, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 1187, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -32, 1187, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, 16, 1187, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, 16, 1312, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -32, 1312, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -80, 1312, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 64, 1175, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 56, 1435, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 0, 48, 1551, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, 40, 1633, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 32, 1712, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 64, 1303, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 56, 1419, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 48, 1599, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 40, 1633, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 32, 1712, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 1303, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 56, 1419, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 48, 1599, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 40, 1633, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 1712, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
};

/// Drawing ranges for mapped view 18, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView18SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 26, 10, 0, 0, { 3, 0 } },
    { 36, 23, 0, 0, { 2, 0 } },
    { 59, 8, 0, 0, { 4, 0 } },
    { 67, ARRAY_SIZE(_gNeoArkObservatoryView18SpriteSources) - 67, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for mapped view 19.
///
/// Positions are pixels relative to the view's drawing origin; sizes are
/// pixels, UVs are texels, and depth is the unscaled ordering value. This view's
/// batches select source ranges in elements while the room overlay is loaded.
static SpriteSource _gNeoArkObservatoryView19SpriteSources[74] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -160, 64, 1550, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 1550, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -8, 1550, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -48, 1550, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -88, 1550, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -128, -48, 1875, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 1875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -72, 1875, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -48, 2150, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -104, -8, 2150, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -104, 16, 2150, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 2150, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 1875, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -112, 48, 1875, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, -48, 2325, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -56, 2150, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 80, 2375, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 32, 2375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 32, 2375, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 80, 2375, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 32, 2375, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -16, 2375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -16, 2375, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -16, 2375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -24, 2375, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -112, -24, 2375, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -24, 2375, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 72, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 1250, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 40, 1250, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -80, -8, 1250, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -88, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 80, 812, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 96, 775, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 40, 812, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 775, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 800, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 24, 850, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 862, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1062, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 1300, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -152, 32, 1300, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 1200, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -136, 32, 1200, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, 32, 1062, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 1062, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -72, 2554, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -48, 2350, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, -48, 2350, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, -48, 2350, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, -48, 2350, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -152, -64, 2425, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -120, -64, 2425, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, -64, 2425, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -64, 2425, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -64, 2562, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, -72, 2500, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -72, 2575, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1096, { .fields = { 48, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, 48, 796, { .fields = { 32, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 48, 24 } }, -160, 96, 789, { .fields = { 120, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -112, 48, 960, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -112, 96, 956, { .fields = { 8, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 48, 1106, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 40 } }, -152, 48, 1437, { .fields = { 104, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, -136, 48, 1437, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 48 } }, -112, 48, 1300, { .fields = { 64, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Drawing ranges for mapped view 19, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// Remaining ranges cover this view's complete source array in elements.
static SpriteBatch _gNeoArkObservatoryView19SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 4, 0 } },
    { 27, 12, 0, 0, { 3, 0 } },
    { 39, 7, 0, 0, { 5, 0 } },
    { 46, 7, 0, 0, { 1, 0 } },
    { 53, 12, 0, 0, { 6, 0 } },
    { 65, 6, 0, 0, { 7, 0 } },
    { 71, ARRAY_SIZE(_gNeoArkObservatoryView19SpriteSources) - 71, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Drawing ranges for mapped view 20, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// No source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView20SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Drawing ranges for mapped view 21, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded background strips enabled.
/// No source sprites are selected; the source slot retains this list address.
static SpriteBatch _gNeoArkObservatoryView21SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkObservatorySpriteViews[21] = {
    { { .empty = _gNeoArkObservatoryView1SpriteBatches }, _gNeoArkObservatoryView1SpriteBatches, NULL },
    { { .empty = _gNeoArkObservatoryView2SpriteBatches }, _gNeoArkObservatoryView2SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView3SpriteSources }, _gNeoArkObservatoryView3SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView4SpriteSources }, _gNeoArkObservatoryView4SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView5SpriteSources }, _gNeoArkObservatoryView5SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView6SpriteSources }, _gNeoArkObservatoryView6SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView7SpriteSources }, _gNeoArkObservatoryView7SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView8SpriteSources }, _gNeoArkObservatoryView8SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView9SpriteSources }, _gNeoArkObservatoryView9SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView10SpriteSources }, _gNeoArkObservatoryView10SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView11SpriteSources }, _gNeoArkObservatoryView11SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView12SpriteSources }, _gNeoArkObservatoryView12SpriteBatches, NULL },
    { { .empty = _gNeoArkObservatoryView13SpriteBatches }, _gNeoArkObservatoryView13SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView14SpriteSources }, _gNeoArkObservatoryView14SpriteBatches, NULL },
    { { .empty = _gNeoArkObservatoryView15SpriteBatches }, _gNeoArkObservatoryView15SpriteBatches, NULL },
    { { .empty = _gNeoArkObservatoryView16SpriteBatches }, _gNeoArkObservatoryView16SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView17SpriteSources }, _gNeoArkObservatoryView17SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView18SpriteSources }, _gNeoArkObservatoryView18SpriteBatches, NULL },
    { { .elements = _gNeoArkObservatoryView19SpriteSources }, _gNeoArkObservatoryView19SpriteBatches, NULL },
    { { .empty = _gNeoArkObservatoryView20SpriteBatches }, _gNeoArkObservatoryView20SpriteBatches, NULL },
    { { .empty = _gNeoArkObservatoryView21SpriteBatches }, _gNeoArkObservatoryView21SpriteBatches, NULL },
};

/// The 17 point lights for room configuration 1.
///
/// Positions and falloff radii use room units; RGB uses 12 fractional bits.
/// All views admit every light. Composition and queries update the records.
/// The two configurations retain their authored order for equal-rank selection.
static WorldCoordPointLight _gNeoArkObservatoryRoom1PointLights[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CF2, -2500, 0x2EC5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9184, -2500, 0x2EC6 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8002, -2500, 9640 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8002, -2500, 7413 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8002, -2500, 5559 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6982, -2500, 3913 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3998, -2500, 6335 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3998, -2500, 9409 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7441, -2000, 0x37E0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -802, -2500, 8240 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CD5, -2337, 0x2BA7 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 906, 669 }, { 0, 0 } }, 100, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -70, -2500, 0x2F1B } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -70, -2500, 3999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7441, -2000, 1447 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3103, -2500, 0x2EC5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1155, -2500, 1837 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1155, -2500, 0x36FE } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
};

/// Point-light collection for room configuration 1; no directional or cone lights.
static WorldCoordRoomLights _gNeoArkObservatoryRoom1Lights = { 0, NULL, ARRAY_SIZE(_gNeoArkObservatoryRoom1PointLights), _gNeoArkObservatoryRoom1PointLights, 0, NULL };

/// The 17 point lights for room configuration 2.
///
/// Positions and falloff radii use room units; RGB uses 12 fractional bits.
/// All views admit every light. Composition and queries update the records.
/// The two configurations retain their authored order for equal-rank selection.
static WorldCoordPointLight _gNeoArkObservatoryRoom2PointLights[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CD5, -2337, 0x2BA7 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 906, 669 }, { 0, 0 } }, 100, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2CF2, -2500, 0x2EC5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9184, -2500, 0x2EC6 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8002, -2500, 9640 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8002, -2500, 7413 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8002, -2500, 5559 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6982, -2500, 3913 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3998, -2500, 6335 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3998, -2500, 9409 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7441, -2000, 0x37E0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -802, -2500, 8240 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -70, -2500, 0x2F1B } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -70, -2500, 3999 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7441, -2000, 1447 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3103, -2500, 0x2EC5 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1155, -2500, 1837 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1155, -2500, 0x36FE } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
};

/// Point-light collection for room configuration 2; no directional or cone lights.
static WorldCoordRoomLights _gNeoArkObservatoryRoom2Lights = { 0, NULL, ARRAY_SIZE(_gNeoArkObservatoryRoom2PointLights), _gNeoArkObservatoryRoom2PointLights, 0, NULL };

/// Eighteen directed view boundaries, shared by both room configurations.
///
/// Parameters name source/destination logical views 2..11. The final boundary
/// carries LAST and is included in the scan. Room setup binds coordinates and
/// links/enables the mutable records; unlink them before unloading the overlay.
static WorldCollisionTrigger _gNeoArkObservatoryViewBoundaryTriggers[18] = {
    { NULL, NULL, NULL, { 8851, -1408, 0x2EEF, 0 }, { { 506, -1872, -1859, 0 }, { -506, -1872, 1859, 0 }, { 506, 1872, -1859, 0 }, { -506, 1872, 1859, 0 } }, { 3953, 0, 1076, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8955, -1440, 0x2F0C, 0 }, { { -524, -1840, 1842, 0 }, { 524, -1840, -1841, 0 }, { -524, 1840, 1842, 0 }, { 524, 1840, -1841, 0 } }, { -3946, 0, -1124, 0 }, { 0, 0, 4096, 0 }, 2648, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8047, -1440, 9136, 0 }, { { -1596, -1840, 162, 0 }, { 1596, -1840, -161, 0 }, { -1596, 1840, 162, 0 }, { 1596, 1840, -161, 0 } }, { -415, 0, -4088, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8032, -1472, 8993, 0 }, { { 1668, -1840, -158, 0 }, { -1668, -1840, 159, 0 }, { 1668, 1840, -158, 0 }, { -1668, 1840, 159, 0 } }, { 386, 0, 4081, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7904, -1505, 6720, 0 }, { { 1732, -1840, 2, 0 }, { -1732, -1840, -1, 0 }, { 1732, 1840, 2, 0 }, { -1732, 1840, -1, 0 } }, { -4, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7952, -1441, 6861, 0 }, { { -1900, -1840, 21, 0 }, { 1900, -1840, -21, 0 }, { -1900, 1840, 21, 0 }, { 1900, 1840, -21, 0 } }, { -46, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8128, -1536, 3984, 0 }, { { -1900, -1840, 1573, 0 }, { 1900, -1840, -1573, 0 }, { -1900, 1840, 1573, 0 }, { 1900, 1840, -1573, 0 } }, { -2629, 0, -3176, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7970, -1536, 3937, 0 }, { { 1908, -1840, -1595, 0 }, { -1908, -1840, 1595, 0 }, { 1908, 1840, -1595, 0 }, { -1908, 1840, 1595, 0 } }, { 2629, 0, 3145, 0 }, { 0, 0, 4096, 0 }, 3093, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5121, -1600, 5120, 0 }, { { -12, -1840, -2459, 0 }, { 12, -1840, 2459, 0 }, { -12, 1840, -2459, 0 }, { 12, 1840, 2459, 0 } }, { 4108, 0, -21, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5297, -1568, 4705, 0 }, { { 4, -1840, 1829, 0 }, { -4, -1840, -1829, 0 }, { 4, 1840, 1829, 0 }, { -4, 1840, -1829, 0 } }, { -4106, 0, 8, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4289, -1568, 1072, 0 }, { { 1060, -1840, 1717, 0 }, { -1060, -1840, -1717, 0 }, { 1060, 1840, 1717, 0 }, { -1060, 1840, -1717, 0 } }, { -3492, 0, 2154, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 7, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4224, -1633, 1297, 0 }, { { -988, -1840, -1563, 0 }, { 989, -1840, 1563, 0 }, { -988, 1840, -1563, 0 }, { 989, 1840, 1563, 0 } }, { 3466, 0, -2194, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 10, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1776, -1600, 6129, 0 }, { { -2540, -1840, -843, 0 }, { 2541, -1840, 843, 0 }, { -2540, 1840, -843, 0 }, { 2541, 1840, 843, 0 } }, { 1291, 0, -3893, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1921, -1569, 5986, 0 }, { { 2420, -1840, 853, 0 }, { -2420, -1840, -853, 0 }, { 2420, 1840, 853, 0 }, { -2420, 1840, -853, 0 } }, { -1363, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 3156, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1888, -1505, 0x2831, 0 }, { { 2356, -1840, -1051, 0 }, { -2356, -1840, 1051, 0 }, { 2356, 1840, -1051, 0 }, { -2356, 1840, 1051, 0 } }, { 1677, 0, 3761, 0 }, { 0, 0, 4096, 0 }, 3166, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1936, -1409, 0x28C1, 0 }, { { -2524, -1840, 1093, 0 }, { 2524, -1840, -1093, 0 }, { -2524, 1840, 1093, 0 }, { 2524, 1840, -1093, 0 } }, { -1629, 0, -3762, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4400, -1440, 0x3A80, 0 }, { { 836, -1840, -1787, 0 }, { -836, -1840, 1787, 0 }, { 836, 1840, -1787, 0 }, { -836, 1840, 1787, 0 } }, { 3717, 0, 1738, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 11, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4576, -1440, 0x3AE1, 0 }, { { -924, -1840, 1861, 0 }, { 924, -1840, -1861, 0 }, { -924, 1840, 1861, 0 }, { 924, 1840, -1861, 0 } }, { -3674, 0, -1825, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 9, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room-action IDs interpreted by this room's direction-message receiver.
enum {
    NEO_ARK_OBSERVATORY_TRIGGER_COMPANION_ARRIVAL      = 1,
    NEO_ARK_OBSERVATORY_TRIGGER_COMPANION_SECOND_SCENE = 2,
    NEO_ARK_OBSERVATORY_TRIGGER_COMPANION_TALK         = 3,
    NEO_ARK_OBSERVATORY_TRIGGER_SCENE                  = 4,
    NEO_ARK_OBSERVATORY_TRIGGER_SHELTER_DEPARTURE      = 10,
};

/// Packed warp slots and stair settings stored in the interaction triggers.
enum {
    NEO_ARK_OBSERVATORY_NORTH_PROMENADE_WARP    = (2 << 4) | 1,
    NEO_ARK_OBSERVATORY_SOUTH_PROMENADE_WARP    = (3 << 4) | 2,
    NEO_ARK_OBSERVATORY_STAIR_DESCEND           = 0x100,
    NEO_ARK_OBSERVATORY_STAIR_SURFACE_AND_STEPS = (2 << 4) | 8,
    NEO_ARK_OBSERVATORY_STAIR_YAW_STEPS         = (ACTOR_TRANSFORM_ANGLE_TURN / 4) / 16,
};

/// Fourteen interaction quads shared by both room configurations, ending with LAST.
///
/// Includes Shelter departure, promenade warps, automatic story gates, CAP
/// interactions and two eight-step descents on surface class 2. Positions and
/// radii use room units; normals use 4096 per unit. Room setup binds and links
/// the writable records, whose storage must remain loaded until unlinked.
static WorldCollisionTrigger _gNeoArkObservatoryActionTriggers[14] = {
    { NULL, NULL, NULL, { 0x3540, -48, 0x2EC0, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_ROOM, NEO_ARK_OBSERVATORY_TRIGGER_SHELTER_DEPARTURE, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7584, -256, 0x3780, 0 }, { { 0, 1232, -960, 0 }, { 0, -1232, -960, 0 }, { 0, 1232, 960, 0 }, { 0, -1232, 960, 0 } }, { 4105, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1557, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_NORTH_PROMENADE, NEO_ARK_OBSERVATORY_NORTH_PROMENADE_WARP, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7552, -272, 1728, 0 }, { { 0, 1184, -1280, 0 }, { 0, -1184, -1280, 0 }, { 0, 1184, 1280, 0 }, { 0, -1184, 1280, 0 } }, { 4106, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1740, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, NEO_ARK_OBSERVATORY_SOUTH_PROMENADE_WARP, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B60, -64, 0x2EE0, 0 }, { { 0, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { 0, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, 201, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_OBSERVATORY_TRIGGER_COMPANION_ARRIVAL, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 5504, -64, 4096, 0 }, { { -544, 0, -1504, 0 }, { 544, 0, -1504, 0 }, { -544, 0, 1504, 0 }, { 544, 0, 1504, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_OBSERVATORY_TRIGGER_COMPANION_SECOND_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 512, -64, 7008, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2495, -64, 0x34AF, 0 }, { { -2561, 0, -1893, 0 }, { -1033, 0, -2838, 0 }, { 1034, 0, 2839, 0 }, { 2562, 0, 1894, 0 } }, { 0, 4108, 0, 0 }, { 2896, 0, -2896, 0 }, 3176, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2175, -64, 3008, 0 }, { { 920, 0, -3050, 0 }, { 2325, 0, -1929, 0 }, { -2324, 0, 1930, 0 }, { -920, 0, 3050, 0 } }, { 0, 4112, 0, 0 }, { 3702, 0, 1751, 0 }, 3176, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5600, -64, 7968, 0 }, { { -3488, 0, -2080, 0 }, { -384, 0, -2080, 0 }, { -3488, 0, 2400, 0 }, { -384, 0, 2400, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 4222, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5568, -64, 7968, 0 }, { { -2208, 0, -2912, 0 }, { 1056, 0, -2912, 0 }, { -2208, 0, 3584, 0 }, { 1056, 0, 3584, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 4190, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6864, -64, 0x37B0, 0 }, { { -208, 0, -624, 0 }, { 208, 0, -624, 0 }, { -208, 0, 624, 0 }, { 208, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_FACING | NEO_ARK_OBSERVATORY_STAIR_DESCEND, NEO_ARK_OBSERVATORY_STAIR_SURFACE_AND_STEPS, NEO_ARK_OBSERVATORY_STAIR_YAW_STEPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6880, -64, 1760, 0 }, { { -224, 0, -608, 0 }, { 224, 0, -608, 0 }, { -224, 0, 608, 0 }, { 224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_FACING | NEO_ARK_OBSERVATORY_STAIR_DESCEND, NEO_ARK_OBSERVATORY_STAIR_SURFACE_AND_STEPS, NEO_ARK_OBSERVATORY_STAIR_YAW_STEPS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9824, -64, 0x2BC0, 0 }, { { -448, 0, -192, 0 }, { 1056, 0, -192, 0 }, { -1024, 0, 1728, 0 }, { 1056, 0, 1728, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 2023, WORLD_COLLISION_TRIGGER_ACTION_ROOM, NEO_ARK_OBSERVATORY_TRIGGER_COMPANION_TALK, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5728, -64, 1152, 0 }, { { -768, 0, -1504, 0 }, { 768, 0, -1504, 0 }, { -768, 0, 1504, 0 }, { 768, 0, 1504, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 1688, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_OBSERVATORY_TRIGGER_SCENE, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum { NEO_ARK_OBSERVATORY_RESOURCE_SCENE_STOP   = 101,
       NEO_ARK_OBSERVATORY_SCENE_STOP_TASK_INDEX = 0 };

/// Variant 1's scene-stop resource recipe, followed by the placement END marker.
///
/// Entry 101 loads actor450200 via base group 40 and file number 502, then
/// selects descriptor 0: a bodyless task that kills itself. The paired
/// placements belong to the Neo Ark map; the task descriptors are borrowed
/// from the loaded scene-actor overlay.
static AreaResource _gNeoArkObservatoryVariant1Resources[2] = {
    { NEO_ARK_OBSERVATORY_RESOURCE_SCENE_STOP, 502, AREA_RESOURCE_FILE_GROUP_BASE_40, NEO_ARK_OBSERVATORY_SCENE_STOP_TASK_INDEX, { 0, 0 }, gActor450200SceneTaskDescs },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gNeoArkObservatoryAreaVariants[13] = {
    { NULL, NULL },
    { gMapNeoArkObservatoryDefaultPlacements, _gNeoArkObservatoryVariant1Resources },
    { NULL, NULL },
    { NULL, NULL },
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

/// Four room-space sight-blocking quads shared by both room configurations.
///
/// The final quad carries LAST and is included in the scan. Linking and
/// enabling mutate these records; unlink them before unloading the room.
/// The stored low flag bit is retained without an established interpretation.
static WorldCollisionOccluder _gNeoArkObservatorySightOccluders[4] = {
    { NULL, NULL, { 6336, -1664, 9200, 0 }, { { 32, 2688, 4016, 0 }, { -32, 2688, -4016, 0 }, { 32, -2688, 4016, 0 }, { -32, -2688, -4016, 0 } }, { 4114, 0, -33, 0 }, 4830, 1, 0 },
    { NULL, NULL, { 9248, -1440, 6720, 0 }, { { 32, 2688, 4016, 0 }, { -32, 2688, -4016, 0 }, { 32, -2688, 4016, 0 }, { -32, -2688, -4016, 0 } }, { 4114, 0, -33, 0 }, 4830, 1, 0 },
    { NULL, NULL, { 9312, -1376, 2656, 0 }, { { 4016, 2688, -32, 0 }, { -4016, 2688, 32, 0 }, { 4016, -2688, -32, 0 }, { -4016, -2688, 32, 0 } }, { -33, 0, -4115, 0 }, 4830, 1, 0 },
    { NULL, NULL, { 0x291F, -1504, 0x349F, 0 }, { { 4013, 2688, 166, 0 }, { -4012, 2688, -165, 0 }, { 4013, -2688, 166, 0 }, { -4012, -2688, -165, 0 } }, { 169, 0, -4111, 0 }, 4830, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Walking, running and scripted-jump cue bases for surface class 1.
///
/// Type-1 requests select the loaded room bank; cue 1 uses the next entry.
static WorldCollisionFootstepSounds _gNeoArkObservatorySurface1FootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

/// Walking, running and scripted-jump cue bases for surface class 2.
///
/// Type-1 requests select the loaded room bank; cue 1 uses the next entry.
static WorldCollisionFootstepSounds _gNeoArkObservatorySurface2FootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x55,
};

/// Walking, running and scripted-jump cue bases for surface class 3.
///
/// Type-1 requests select the loaded room bank; cue 1 uses the next entry.
static WorldCollisionFootstepSounds _gNeoArkObservatorySurface3FootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
    SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
};

/// Blocking, impact-enabled surface with pushback and no footstep cues (classes 0, 4..7).
static WorldCollisionSurfaceProperties _gNeoArkObservatorySilentSurface = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

/// Blocking, impact-enabled surface with pushback and class-1 footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkObservatorySurface1 = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gNeoArkObservatorySurface1FootstepSounds };

/// Blocking, impact-enabled surface with pushback and class-2 footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkObservatorySurface2 = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gNeoArkObservatorySurface2FootstepSounds };

/// Blocking, impact-enabled surface with pushback and class-3 footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkObservatorySurface3 = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gNeoArkObservatorySurface3FootstepSounds };

WorldCollisionSurfaceProperties* gNeoArkObservatorySurfaceProperties[8] = {
    &_gNeoArkObservatorySilentSurface,
    &_gNeoArkObservatorySurface1,
    &_gNeoArkObservatorySurface2,
    &_gNeoArkObservatorySurface3,
    &_gNeoArkObservatorySilentSurface,
    &_gNeoArkObservatorySilentSurface,
    &_gNeoArkObservatorySilentSurface,
    &_gNeoArkObservatorySilentSurface,
};

/// Saved-area changes committed when the companion departs for either promenade.
///
/// In every save mode, selects observatory placement variant 2, removes its
/// saved enemy poses and clears its map mark. AREA_APPLY_END terminates the
/// synchronously borrowed list; variant 2 is an absent placement slot.
static AreaApplyRec _gNeoArkObservatoryCompanionDepartureAreaUpdates[2] = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 2, AREA_APPLY_MODE_ALWAYS },
    { AREA_APPLY_END, 0, 0, 0 },
};

RoomDeparture gRoomDeparture;

static inline void _neoArkObservatoryResolveDeparture(RoomDeparture* departure, RoomVariantResolver resolve);
static void        _neoArkObservatoryInitializeRoom(Task* task);
static void        _neoArkObservatoryUpdateCompanionVisibility(Task* unusedTask);

#include "../../shared/room/planar_reflection.inc.c"

/// Runs this room's player reflection, including its attachment reflections.
///
/// Starts bodyless in state 0 with a live player model; spawnArg1.value selects
/// a floor reflection (0) or the room's mirror plane (1). State 1 updates the
/// cloned model and reflected view. The task owns its clone and mirror work
/// and is torn down with the player; dispatch requires state 0 or 1.
static void _neoArkObservatoryPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

#include "../../shared/room/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room/room_event_departure_task.inc.c"

/// Resolves a prepared departure's area, warp and room through an execute request.
///
/// Borrows a writable departure with valid 1-based selectors and a resolver for
/// its stage. The resolver must support an aliased request/reply and read only
/// area, warp, room and queryOnly: other request fields are uninitialized. Its
/// result is ignored; the resolved area narrows back to a byte. Stage, sound and
/// facing stay intact, and neither the helper nor the resolver retains storage.
static inline void _neoArkObservatoryResolveDeparture(RoomDeparture* departure, RoomVariantResolver resolve)
{
    RoomEventMsg request;

    request.areaId    = departure->area;
    request.warp      = departure->warp;
    request.roomState = departure->roomState;
    request.queryOnly = ROOM_EVENT_EXECUTE;
    resolve(&request, &request);
    departure->area      = request.areaId;
    departure->warp      = request.warp;
    departure->roomState = request.roomState;
}

/// Resolves and publishes a prepared Shelter departure after holding player control.
///
/// Borrows the initialized departure for this call; its resolver follows
/// `_neoArkObservatoryResolveDeparture`'s selector-only, aliased-message contract.
/// Copies the result into the room's persistent departure record before queuing
/// its bodyless task. Keep the room loaded and do not overwrite that record
/// while the task runs. Failed allocation still leaves control held and the
/// destination committed to the record.
static inline void _neoArkObservatoryQueueDeparture(RoomDeparture* departure, RoomVariantResolver resolve)
{
    enum { DEPARTURE_TASK = 0 };

    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    _neoArkObservatoryResolveDeparture(departure, resolve);
    gRoomDeparture = *departure;
    taskSpawnFromTable(&_gNeoArkObservatoryDepartureTaskDesc, DEPARTURE_TASK, 0, 0);
}

/// Handles the observatory's room-action triggers and their one-time story scenes.
///
/// Borrows a four-byte DirectionActionRequest during synchronous dispatch.
/// Action 10 departs for Mine/Shelter using the argument as the destination area,
/// except the first cleared-power-plant return selects Control Room warp 3.
/// Actions 1/2 start gated companion scenes, 3 starts companion talk in view 2,
/// and 4 starts the observatory scene after Power Plant 1 is cleared. Unknown
/// actions do nothing. Returns zero; other callback arguments are unused.
/// The room, companion scene resources and relevant map overlay must stay loaded
/// through the queued work. Progress changes survive failed spawns/start attempts.
static s32 _neoArkObservatoryRoomActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg)
{
    enum { ACTION_SHELTER_DEPARTURE          = 10,
           ACTION_COMPANION_ARRIVAL_SCENE    = 1,
           ACTION_COMPANION_SECOND_SCENE     = 2,
           ACTION_COMPANION_TALK             = 3,
           ACTION_OBSERVATORY_SCENE          = 4,
           COMPANION_ROUTE_BEFORE_ARRIVAL    = 2,
           COMPANION_ROUTE_AT_OBSERVATORY    = 3,
           COMPANION_RETURN_SCHEDULE         = 8,
           POWER_PLANT_2_RETURN_READY        = 1,
           CONTROL_ROOM_RETURN_WARP          = 3,
           REQUESTED_SHELTER_WARP            = 4,
           DEFAULT_DESTINATION_ROOM          = 1,
           DEPARTURE_FACING                  = ACTOR_TRANSFORM_ANGLE_TURN / 4,
           DEPARTURE_SOUND                   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_OBSERVATORY, 5),
           COMPANION_FIRST_BRANCH_OBJECTIVE  = 0x2C,
           COMPANION_SECOND_BRANCH_OBJECTIVE = 0x2D,
           SECOND_SCENE_DIALOGUE_INDEX       = 6,
           SECOND_SCENE_EVENT                = 0x15,
           COMPANION_TALK_VIEW               = 2 };
    RoomDeparture       departure;
    RoomVariantResolver resolve;
    s32                 powerPlant2Cleared;

    // Prepare and resolve the Shelter destination before publishing it to its task.
    if (actionRequest->actionId == ACTION_SHELTER_DEPARTURE) {
        if (gameFlagGetNibble(GAME_FLAG_0D1) == COMPANION_ROUTE_BEFORE_ARRIVAL) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_RETURN_SCHEDULE);
        }
        if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) == 0) {
            powerPlant2Cleared = gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED);
            if (powerPlant2Cleared == POWER_PLANT_2_RETURN_READY) {
                RoomVariantResolver resolve;

                gameFlagSetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN, true);
                departure.stage     = GAME_STAGE_MINE_SHELTER;
                departure.area      = GAME_AREA_SHELTER_B1_CONTROL_ROOM;
                departure.warp      = CONTROL_ROOM_RETURN_WARP;
                departure.roomState = powerPlant2Cleared;
                departure.sndEvent  = DEPARTURE_SOUND;
                departure.facing    = DEPARTURE_FACING;
                resolve             = _roomVariantResolveShelter;
                _neoArkObservatoryQueueDeparture(&departure, resolve);
                return 0;
            }
        }
        departure.stage     = GAME_STAGE_MINE_SHELTER;
        departure.area      = actionRequest->argument;
        departure.roomState = DEFAULT_DESTINATION_ROOM;
        departure.warp      = REQUESTED_SHELTER_WARP;
        departure.sndEvent  = DEPARTURE_SOUND;
        departure.facing    = DEPARTURE_FACING;
        resolve             = _roomVariantResolveShelter;
        _neoArkObservatoryQueueDeparture(&departure, resolve);
    }
    // Commit each one-shot gate before requesting its scene.
    if (actionRequest->actionId == ACTION_COMPANION_ARRIVAL_SCENE && gameFlagGetNibble(GAME_FLAG_0D7) == 0) {
        gameFlagSetNibble(GAME_FLAG_0D7, true);
        if (gameFlagGetNibble(GAME_FLAG_083) != 0) {
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, COMPANION_FIRST_BRANCH_OBJECTIVE);
            evsStartScriptWithSkip(gActor450200SoloSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor450200SoloSceneSkipScript);
        } else {
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, COMPANION_SECOND_BRANCH_OBJECTIVE);
            gameFlagSetNibble(GAME_FLAG_0D1, COMPANION_ROUTE_AT_OBSERVATORY);
            evsStartScriptWithSkip(gActor450200CompanionSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor450200CompanionSceneSkipScript);
        }
    }
    if (actionRequest->actionId == ACTION_COMPANION_SECOND_SCENE) {
        if (gameFlagGetNibble(GAME_FLAG_0E1) == 0) {
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, SECOND_SCENE_DIALOGUE_INDEX);
            gameFlagSetNibble(GAME_FLAG_0E1, true);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = SECOND_SCENE_EVENT;
            evsStartScriptWithSkip(gActor450200RoomVariantSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor450200RoomVariantSceneSkipScript);
        }
    }
    if (actionRequest->actionId == ACTION_COMPANION_TALK && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL && gGameSession->location.view == COMPANION_TALK_VIEW) {
        actor450200StartCompanionTalk();
    }
    if (actionRequest->actionId == ACTION_OBSERVATORY_SCENE && gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) != 0 && gameFlagGetNibble(GAME_FLAG_OBSERVATORY_EVENT_SEEN) == 0) {
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EVENT_SEEN, true);
        evsStartScriptWithSkip(_gNeoArkObservatorySceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gNeoArkObservatorySceneSkipScript);
    }
    return 0;
}

void neoArkObservatoryUpdateCompanionObstacle(s32 unusedEventArg)
{
    enum { NEO_ARK_OBSERVATORY_DISABLED_OBSTACLE_Y_OFFSET = 10000 };

    Task* obstacleActorTask;
    Task* companionTask;

    obstacleActorTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    companionTask     = obstacleActorTask;
    if (obstacleActorTask == NULL) {
        obstacleActorTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    // Move disabled companion collision out of the playable room.
    if (companionTask != NULL && gameFlagGetNibble(GAME_FLAG_0D7) != 0) {
        _gNeoArkObservatoryCompanionObstacleOffset.vy = 0;
    } else {
        _gNeoArkObservatoryCompanionObstacleOffset.vy = NEO_ARK_OBSERVATORY_DISABLED_OBSTACLE_Y_OFFSET;
    }
    _followCollisionRebuildObstacle(obstacleActorTask->extra.tmd->coords, &_gNeoArkObservatoryCompanionObstacleOffset);
}

/// Plays a command from the observatory's second loaded CAP data resource.
///
/// State 0 selects resource ordinal 1 and VRAM origin (768,0), then attempts the
/// command in spawnArg1.value with no event flags. State 1 waits for CAP to be
/// idle; state 2 resumes player control, restores default CAP resources and kills
/// this bodyless task. Failed starts still reach cleanup; unknown states do
/// nothing. Requires the room's CAP files/textures through playback and a valid
/// command index for resource 1. The command handler holds control before spawn.
static void _neoArkObservatoryCapEventTask(Task* task)
{
    enum { CAP_EVENT_START      = 0,
           CAP_EVENT_WAIT       = 1,
           CAP_EVENT_RESTORE    = 2,
           CAP_RESOURCE_ORDINAL = 1,
           CAP_TEXTURE_VRAM_X   = 0x300,
           CAP_TEXTURE_VRAM_Y   = 0 };

    switch (task->state) {
        case CAP_EVENT_START:
            gCapFile = NULL;
            capSelectLoadedFile(CAP_RESOURCE_ORDINAL);
            capSetTexturePage(CAP_TEXTURE_VRAM_X, CAP_TEXTURE_VRAM_Y);
            capSpawnEventIfIdle(task->spawnArg1.value, CAP_EVENT_NO_FLAGS);
            task->state++;
            break;
        case CAP_EVENT_WAIT:
            if (capIsBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case CAP_EVENT_RESTORE:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

/// Rejects key-item use in this room with result 0 and no side effects.
///
/// All four callback arguments are unused. The key-item menu interprets zero
/// as an unavailable use and leaves the item in inventory.
static s32 _neoArkObservatoryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return 0;
}

/// Resolves a Neo Ark departure and commits the companion's promenade progression.
///
/// Borrows a complete readable request and writable reply, which may alias.
/// Copies all eight bytes before map resolution. An executed north/south
/// promenade request at route state 3 and companion schedule 9 clears that
/// schedule and applies the saved-area updates. Queries suppress those effects.
/// Always returns 1; receiver and message ID are unused. Requires the map overlay,
/// room update list and live saved state. Neither pointer is retained.
static s32 _neoArkObservatoryResolveRoomEventMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { COMPANION_ROUTE_AT_OBSERVATORY = 3,
           COMPANION_PROMENADE_SCHEDULE   = 9,
           COMPANION_SCHEDULE_CLEARED     = 0,
           TRANSITION_ALLOWED             = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if ((gameFlagGetNibble(GAME_FLAG_0D1) == COMPANION_ROUTE_AT_OBSERVATORY) && (gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE) == COMPANION_PROMENADE_SCHEDULE) &&
        ((request->areaId == GAME_AREA_NEO_ARK_NORTH_PROMENADE) || (request->areaId == GAME_AREA_NEO_ARK_SOUTH_PROMENADE)) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_SCHEDULE_CLEARED);
        areaApplySavedUpdates(_gNeoArkObservatoryCompanionDepartureAreaUpdates);
    }
    return TRANSITION_ALLOWED;
}

/// Holds player control and queues the room's alternate CAP command 1.
///
/// Handles ROOM_MESSAGE_COMMAND index 1; other indices do nothing. Returns zero
/// and ignores the other arguments. The room and CAP resources must remain loaded
/// through the spawned playback task. A failed spawn leaves the player held.
static s32 _neoArkObservatoryCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum { ALTERNATE_CAP_COMMAND = 1,
           CAP_EVENT_TASK        = 0 };

    if (commandIndex == ALTERNATE_CAP_COMMAND) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(&_gNeoArkObservatoryCapEventTaskDesc, CAP_EVENT_TASK, ALTERNATE_CAP_COMMAND, 0);
    }
    return 0;
}

/// Installs the room receiver, companion setup and saved light-beam intensity.
///
/// A live companion in variant 1 receives the observatory entry setup;
/// otherwise its obstacle is rebuilt from current presence and story flags.
/// The second companion scene's saved flag restores grey beams at level 160.
/// Advances to the visibility state. The room slot borrows the live task.
static void _neoArkObservatoryInitializeRoom(Task* task)
{
    enum { COMPANION_ENTRY_VARIANT    = 1,
           SAVED_SCENE_BEAM_INTENSITY = 160 };

    task->msgTable = _gNeoArkObservatoryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) && (gGameSession->location.variant == COMPANION_ENTRY_VARIANT)) {
        actor450200InitializeObservatoryCompanion();
    } else {
        neoArkObservatoryUpdateCompanionObstacle(0);
    }
    if (gameFlagGetNibble(GAME_FLAG_0E1) != 0) {
        neoArkObservatorySetLightBeamIntensity(SAVED_SCENE_BEAM_INTENSITY);
    }
    task->state = task->state + 1;
}

/// Updates the companion's visibility for the observatory's saved view and event state.
///
/// The idle room shows the companion only in saved view 2. During an event,
/// every saved view except 3 shows it. Hidden modes release model buffers;
/// visible modes use automatic buffering. The task argument is unused and a
/// missing companion is harmless.
static void _neoArkObservatoryUpdateCompanionVisibility(Task* unusedTask)
{
    enum { NEO_ARK_OBSERVATORY_COMPANION_IDLE_VIEW         = 2,
           NEO_ARK_OBSERVATORY_COMPANION_HIDDEN_EVENT_VIEW = 3 };
    s32 drawMode;

    if (gGameSession->eventState == 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != NEO_ARK_OBSERVATORY_COMPANION_IDLE_VIEW) {
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE);
            return;
        }
    }
    drawMode = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view == NEO_ARK_OBSERVATORY_COMPANION_HIDDEN_EVENT_VIEW) {
        drawMode = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE;
    }
    companionSetDrawMode(drawMode);
}

/// Room-task dispatch value for states 0 initialize, 1 update companion visibility, 2 kill.
///
/// Copied as three callback pointers before dispatch. The live task's state
/// must be in 0..2; there is no terminator or bounds check. Keep the room
/// overlay loaded through the selected callback.
static const TaskFuncTable3 _gNeoArkObservatoryRoomTaskStates = {
    {
        _neoArkObservatoryInitializeRoom,
        _neoArkObservatoryUpdateCompanionVisibility,
        taskKill,
    },
};

void neoArkObservatoryRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gNeoArkObservatoryRoomTaskStates;
    handlers.funcs[task->state](task);
}

/// Selects the observatory's mutable room grid receiving the companion obstacle.
///
/// Binds a `WorldCollisionGrid` object lvalue; the fragment takes its address
/// once per rebuild. Writable, word-aligned mesh pools reserve normals/faces
/// 0..3 and vertices 0..7 for the obstacle, disjoint from the source pools.
/// Normals use 4096 for unit length; vertices use signed room-coordinate units.
/// Every cell already references those four faces, so moving the obstacle
/// preserves the cell lists, grid descriptor and remaining room geometry.
/// The record and pools live with the room overlay through rebuilding and
/// active collision queries. Define before the rebuild fragment include and
/// undefine immediately after it.
#define FOLLOW_COLLISION_ROOM_GRID _gNeoArkObservatoryCollisionGrid

/// Selects the observatory's model-local companion-obstacle template.
///
/// Binds a `WorldCollisionGrid` object lvalue, whose address the fragment takes
/// once per rebuild. Its four quad faces, four normals and eight vertices are
/// read without modifying the source. Normals use 4096 for unit length; vertices
/// use signed game-coordinate units. The mesh pools live with the overlay and
/// must be disjoint from the room grid's pools. Cell metadata is unused.
/// Define before the rebuild fragment include and undefine immediately after it.
#define FOLLOW_COLLISION_OBSTACLE_SOURCE _gNeoArkObservatoryCompanionObstacleTemplate
#include "../../shared/room/follow_collision_rebuild.inc.c"
#undef FOLLOW_COLLISION_OBSTACLE_SOURCE
#undef FOLLOW_COLLISION_ROOM_GRID

void neoArkObservatoryGlowTask(Task* effectTask)
{
    enum {
        NEO_ARK_OBSERVATORY_GLOW_STATE_INIT   = 0,
        NEO_ARK_OBSERVATORY_GLOW_STATE_DRAW   = 1,
        NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT  = 0x444, // Packed RGB nibbles
        NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM  = 0x333,
        NEO_ARK_OBSERVATORY_GLOW_DISC_DIM     = 0x222,
        NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS  = 0x280, // Projected radius numerator
        NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS   = 0x200,
        NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS = 0x400, // World-coordinate units
        NEO_ARK_OBSERVATORY_BEAM_LARGE_RADIUS = 0x600,
    };
    u8 mappedView;

    if (effectTask->state == NEO_ARK_OBSERVATORY_GLOW_STATE_INIT) {
        _gNeoArkObservatoryLightBeamIntensity = 0;
        effectTask->state                     = NEO_ARK_OBSERVATORY_GLOW_STATE_DRAW;
    }

    // Select the lights visible in the mapped camera, including alternate room views.
    // Interior pointers select overlapping glow groups and beam pairs in the same bank.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2:
            glowDrawDisc(&_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_HIGH_GLOW_FIRST], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        case 3: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_HIGH_GLOW_FIRST + 2];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 4:
        case 16: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_HIGH_GLOW_FIRST + 3];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[3], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            break;
        }
        case 5:
        case 17: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_HIGH_GLOW_FIRST + 5];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 6:
        case 18:
            glowDrawDisc(&_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        case 7: {
            const SVECTOR* beamCenters;
            beamCenters = _gNeoArkObservatoryLightPositions;
            _neoArkObservatoryDrawLightBeam(&beamCenters[0], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
            _neoArkObservatoryDrawLightBeam(&beamCenters[2], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 0xC);
            _neoArkObservatoryDrawLightBeam(&beamCenters[4], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
        }
            // View 7 also draws the glow discs used by mapped view 19.
            /* fallthrough */
        case 19: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[6], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[7], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[8], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[9], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 8:
        case 20: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST + 9];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-32], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-30], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-28], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
            break;
        }
        case 9: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST + 11];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[3], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[-6], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[-8], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-28], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-26], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 0xC);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-24], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 8);
            break;
        }
        case 10: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST + 1];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[5], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[6], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[7], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-12], NEO_ARK_OBSERVATORY_BEAM_LARGE_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 0x10);
            break;
        }
        case 11: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST + 12];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[-7], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[-8], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-21], NEO_ARK_OBSERVATORY_BEAM_LARGE_RADIUS, _gNeoArkObservatoryLightBeamIntensity, 0x10);
            break;
        }
        case 12:
        case 14: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_HIGH_GLOW_FIRST];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 21: {
            const SVECTOR* glowPoints;
            glowPoints = &_gNeoArkObservatoryLightPositions[NEO_ARK_OBSERVATORY_LOW_GLOW_FIRST + 9];
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
    }
}

/// Draws an additive grey light beam between two horizontal world-space rings.
///
/// Borrows two centres for this call: ring 0 has half the signed low-halfword
/// outerRadius, ring 1 has the full radius, both in world-coordinate units.
/// segmentCount must be 1..4096; room callers use 8, 12 or 16. Angles use 4096
/// units per turn, advancing one unit per animation frame. Integer division
/// truncates the angle step when the count does not divide a turn exactly.
///
/// A four-frame sine pulse adds -4..4 to baseIntensity, narrowed to s16.
/// Negative results draw nothing; nonnegative intensities narrow to RGB bytes.
/// The first ring fades from full to half intensity per segment, and the
/// second ring is black. Each accepted projection queues one quad and an
/// additive blend command, at the last corner's camera depth / 4 plus one.
/// Requires the composed view matrix, scratch stack and current frame arena.
static void _neoArkObservatoryDrawLightBeam(const SVECTOR ringCenters[2], s32 outerRadius, s16 baseIntensity, s16 segmentCount)
{
    enum {
        NEO_ARK_OBSERVATORY_BEAM_PULSE_ANGLE_SHIFT = 10, // Quarter-turn per animation frame
        NEO_ARK_OBSERVATORY_BEAM_PULSE_LEVEL_SHIFT = 10, // Q12 sine to -4..4 intensity
    };
    EffectQuadScratch*  quadScratch;
    POLY_G4*            quad;
    const SVECTOR*      outerCenter;
    const DisplayState* display;
    s16                 startAngle;
    s16                 angleStep;
    s32                 angle;
    s32                 nextAngle;
    s16                 innerRadius;
    s16                 intensity;

    angleStep   = GLOW_FULL_TURN / segmentCount;
    outerCenter = ringCenters + 1;
    innerRadius = (s16)outerRadius >> 1;
    startAngle  = gDisplayState.animFrame & (GLOW_FULL_TURN - 1);
    intensity   = baseIntensity + (rsin(gDisplayState.animFrame << NEO_ARK_OBSERVATORY_BEAM_PULSE_ANGLE_SHIFT) >> NEO_ARK_OBSERVATORY_BEAM_PULSE_LEVEL_SHIFT);
    if (intensity >= 0) {
        SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
        quadScratch = SCRATCH_STACK_CURSOR(EffectQuadScratch);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        // Build adjoining XZ ring edges, then project each strip segment through the view.
        for (angle = startAngle; angle < startAngle + angleStep * segmentCount; angle = nextAngle) {
            quadScratch->vertices[0].vx = ringCenters->vx + ((rsin(angle) * innerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[0].vy = ringCenters->vy;
            quadScratch->vertices[0].vz = ringCenters->vz + ((rcos(angle) * innerRadius) >> GLOW_TRIG_SHIFT);
            nextAngle                   = angle + angleStep;
            quadScratch->vertices[1].vx = ringCenters->vx + ((rsin(nextAngle) * innerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[1].vy = ringCenters->vy;
            quadScratch->vertices[1].vz = ringCenters->vz + ((rcos(nextAngle) * innerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[2].vx = outerCenter->vx + ((rsin(angle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[2].vy = outerCenter->vy;
            quadScratch->vertices[2].vz = outerCenter->vz + ((rcos(angle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[3].vx = outerCenter->vx + ((rsin(nextAngle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[3].vy = outerCenter->vy;
            quadScratch->vertices[3].vz = outerCenter->vz + ((rcos(nextAngle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            _neoArkObservatoryProjectBeamSegment(quadScratch);
            if (quadScratch->projectionFlags >= 0) {
                // Use the final projected corner for ordering, with a one-entry bias.
                gte_stszotz(&quadScratch->depth);
                quadScratch->depth++;
                display        = &gDisplayState;
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, intensity, intensity, intensity);
                setRGB1(quad, intensity >> 1, intensity >> 1, intensity >> 1);
                setRGB2(quad, 0, 0, 0);
                setRGB3(quad, 0, 0, 0);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(quadScratch->depth, display->otDepthShift),
                        quad);
                quad->x0 = quadScratch->screenCorners[0].vx;
                quad->y0 = quadScratch->screenCorners[0].vy;
                quad->x1 = quadScratch->screenCorners[1].vx;
                quad->y1 = quadScratch->screenCorners[1].vy;
                quad->x2 = quadScratch->screenCorners[2].vx;
                quad->y2 = quadScratch->screenCorners[2].vy;
                quad->x3 = quadScratch->screenCorners[3].vx;
                quad->y3 = quadScratch->screenCorners[3].vy;
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, quadScratch->depth);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
    }
}

#include "../../shared/effects/glow_draw_disc.inc.c"

void neoArkObservatorySetLightBeamIntensity(s32 intensity)
{
    _gNeoArkObservatoryLightBeamIntensity = intensity;
}
