#include "rooms/mine_forked_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_01500.h"
#include "actors/actor_01600.h"
#include "actors/actor_02500.h"
#include "actors/actor_03700.h"
#include "actors/actor_300700.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of the room's area object, the model the switch event sends
/// rolling down the tunnel slope; allocated zeroed and kept at `Task::work`.
///
/// The two matrices are the storage the object's `TmdObject::lightMtx` and
/// `colorMtx` point at, which its child part borrows as well.
typedef struct {
    MATRIX light;         // the model's light matrix
    MATRIX color;         // the model's colour matrix
    Task*  child;         // the part parented onto the object's coordinate and tilted separately; NULL when its spawn failed
    s32    freeCountdown; // frames until the hidden model's primitive buffers are freed, releasing them when it reaches 0 (-1 idle)
} _MineForkedTunnelAreaObjectWork;
STATIC_ASSERT_SIZEOF(_MineForkedTunnelAreaObjectWork, 0x48);

static void _mineForkedTunnelInitAreaObject(Task* task);
static void _mineForkedTunnelTickAreaObject(Task* task);
static s32  _mineForkedTunnelApplyAreaObjectCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused);
static void _mineForkedTunnelTiltAreaObjectChild(Task* task);
static void _mineForkedTunnelAreaObjectChildTask(Task* task);
static void _mineForkedTunnelExitAreaObject(Task* task);
static void _mineForkedTunnelBindAreaObjectLighting(Task* task);
static s32  _mineForkedTunnelSetAreaObjectDrawMode(Task* task, s32 messageId, s32 drawMode, s32 unused);
static void _mineForkedTunnelAttachAreaObjectChild(Task* task);
static void _mineForkedTunnelUpdateAreaObjectCollision(s32 switchUsed);
static void _mineForkedTunnelInitRoomTask(Task* task);
static void _mineForkedTunnelIdleRoomTask(Task* unusedTask);
static void _mineForkedTunnelSetSpriteBatchesHidden(u8 hidden);
static s32  _mineForkedTunnelHandleSwitchAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unused);
static void _mineForkedTunnelRestoreAreaObjectSoundMix(void);

/// Script commands accepted by the switch-controlled area object.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_RESET      = 0,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_TILT_CHILD = 1,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_START_PATH = 2,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_PARK       = 3,
};

/// Motion gates stored in the area object and child's first spawn argument.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE    = 0,
    MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING = 1,
};

/// Reserved collision-box prefix, smaller than the complete room grid pools.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT   = 3,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT = 8,
};

/// Restores the box's faces and XYZ components in the room's reserved prefix.
///
/// Grid pointers must be side-effect-free expressions with live, disjoint pools;
/// they are evaluated repeatedly. The room pools are writable and contain three
/// normals/faces and eight vertices. Vector pad halfwords and later entries are
/// retained; each face copies its full twelve bytes, including surface flags.
/// The index argument is a writable s32 cursor, left at eight. Invoke inside
/// braces; the subsequent caller loop applies the selected room-axis offset.
#define MINE_FORKED_TUNNEL_COPY_AREA_OBJECT_COLLISION_BOX(roomGrid, templateGrid, entryIndex)                          \
    {                                                                                                                  \
        for ((entryIndex) = 0; (entryIndex) < MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT; (entryIndex)++) {   \
            (roomGrid)->normals[entryIndex].vx = (templateGrid)->normals[entryIndex].vx;                               \
            (roomGrid)->normals[entryIndex].vy = (templateGrid)->normals[entryIndex].vy;                               \
            (roomGrid)->normals[entryIndex].vz = (templateGrid)->normals[entryIndex].vz;                               \
            (roomGrid)->faces[entryIndex]      = (templateGrid)->faces[entryIndex];                                    \
        }                                                                                                              \
        for ((entryIndex) = 0; (entryIndex) < MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT; (entryIndex)++) { \
            (roomGrid)->vertices[entryIndex].vx = (templateGrid)->vertices[entryIndex].vx;                             \
            (roomGrid)->vertices[entryIndex].vy = (templateGrid)->vertices[entryIndex].vy;                             \
            (roomGrid)->vertices[entryIndex].vz = (templateGrid)->vertices[entryIndex].vz;                             \
        }                                                                                                              \
    }

/// Draw modes carried by the area object's ACTOR_MESSAGE_SET_MODEL_DRAW payload.
enum {
    MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_AUTO    = 0,
    MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_AUTO    = 1,
    MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_RELEASE = 2,
    MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_MANUAL  = 3,
};

/// Setup, motion/drawing and teardown handlers for the switch-controlled area object.
///
/// `Task::state` selects 0..2 without a bounds check. The dispatcher copies
/// all three callbacks and runs them only while scene actor control is active.
/// Teardown may release the task; callbacks require this room overlay loaded.
static const TaskFuncTable3 _gMineForkedTunnelAreaObjectStateHandlers = {
    { _mineForkedTunnelInitAreaObject, _mineForkedTunnelTickAreaObject, _mineForkedTunnelExitAreaObject },
};

/// Attach, tilt and teardown handlers for the area object's child model.
///
/// `Task::state` selects 0..2 without a bounds check. The dispatcher copies
/// all three callbacks and runs them only while scene actor control is active.
/// Attachment borrows the live parent; teardown releases the child task.
static const TaskFuncTable3 _gMineForkedTunnelAreaObjectChildStateHandlers = {
    { _mineForkedTunnelAttachAreaObjectChild, _mineForkedTunnelTiltAreaObjectChild, taskKill },
};

/// Tunnel room callbacks: 0 register/music/board visibility, 1 idle, 2 release.
///
/// All three pointers are copied for unchecked `Task::state` dispatch. There is
/// no terminator; keep this room's callback code loaded until the task exits.
static const TaskFuncTable3 _gMineForkedTunnelRoomTaskStates = {
    { _mineForkedTunnelInitRoomTask, _mineForkedTunnelIdleRoomTask, taskKill },
};

static u32     _gMineForkedTunnelModel03340PartVerts[1];
static SVECTOR _gMineForkedTunnelModel03340Verts[20];
static SVECTOR _gMineForkedTunnelModel03340Normals[12];
static TmdBone _gMineForkedTunnelModel03340Skeleton[1];
static u32     _gMineForkedTunnelModel03340Stream[104];

static s32 _mineForkedTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unused);
static s32 _mineForkedTunnelResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _mineForkedTunnelBoardCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

static void _mineForkedTunnelSwitchInteractionTask(Task* task);
static void _mineForkedTunnelOakBoardPromptTask(Task* task);

static WorldCollisionGrid         _gMineForkedTunnelCollisionGrid;
static WorldCollisionTrigger      _gMineForkedTunnelViewBoundaryTriggers[6];
static WorldCollisionTrigger      _gMineForkedTunnelActionTriggers[6];
static WorldCoordRoomAmbientEntry _gMineForkedTunnelViewAmbientTable[8];
static WorldCoordRoomLights       _gMineForkedTunnelRoomLights;

static TmdBone _gMineForkedTunnelModel01B48Skeleton[1] = {
#include "assets/mine_forked_tunnel_model_01B48_skeleton.inc"
};

static u32 _gMineForkedTunnelModel01B48PartVerts[1] = {
#include "assets/mine_forked_tunnel_model_01B48_partVerts.inc"
};

static SVECTOR _gMineForkedTunnelModel01B48Verts[223] = {
#include "assets/mine_forked_tunnel_model_01B48_verts.inc"
};

static SVECTOR _gMineForkedTunnelModel01B48Normals[56] = {
#include "assets/mine_forked_tunnel_model_01B48_normals.inc"
};

static u32 _gMineForkedTunnelModel01B48Stream[1451] = {
#include "assets/mine_forked_tunnel_model_01B48_stream.inc"
};

TmdSource gMineForkedTunnelModel01B48 = {
    0,
    10460,
    0,
    1,
    _gMineForkedTunnelModel01B48PartVerts,
    _gMineForkedTunnelModel01B48Verts,
    _gMineForkedTunnelModel01B48Normals,
    _gMineForkedTunnelModel01B48Skeleton,
    _gMineForkedTunnelModel01B48Stream,
};

static TmdBone _gMineForkedTunnelModel03340Skeleton[1] = {
#include "assets/mine_forked_tunnel_model_03340_skeleton.inc"
};

static u32 _gMineForkedTunnelModel03340PartVerts[1] = {
#include "assets/mine_forked_tunnel_model_03340_partVerts.inc"
};

static SVECTOR _gMineForkedTunnelModel03340Verts[20] = {
#include "assets/mine_forked_tunnel_model_03340_verts.inc"
};

static SVECTOR _gMineForkedTunnelModel03340Normals[12] = {
#include "assets/mine_forked_tunnel_model_03340_normals.inc"
};

static u32 _gMineForkedTunnelModel03340Stream[104] = {
#include "assets/mine_forked_tunnel_model_03340_stream.inc"
};

static TmdSource _gMineForkedTunnelModel03340 = {
    0,
    728,
    0,
    1,
    _gMineForkedTunnelModel03340PartVerts,
    _gMineForkedTunnelModel03340Verts,
    _gMineForkedTunnelModel03340Normals,
    _gMineForkedTunnelModel03340Skeleton,
    _gMineForkedTunnelModel03340Stream,
};

/// Authored Euler rotations paired with the switch-controlled area object's position path.
///
/// Angles use 4096 units per turn and ZYX placement order. Playback samples
/// entries 0..109, one per updating frame; setup and reset use entry zero.
/// The full 240-entry track is retained. Fourth components are unused.
static SVECTOR _gMineForkedTunnelAreaObjectRotationPath[240] = {
#include "assets/mine_forked_tunnel_motion_03504.inc"
};

/// Authored translations paired with the switch-controlled area object's rotation path.
///
/// XYZ uses integer game units in the model root's parent frame. Playback
/// samples entries 0..109, one per updating frame; setup and reset use entry
/// zero. The full 240-entry track is retained. Fourth components are unused.
static SVECTOR _gMineForkedTunnelAreaObjectPositionPath[240] = {
#include "assets/mine_forked_tunnel_motion_03C84.inc"
};

/// Parent-relative Euler rotations for the child model's 54-frame tilt.
///
/// Angles use 4096 units per turn; only pitch varies and fourth components
/// are unused. A nonnegative task cursor samples every entry once while
/// motion is enabled, then holds the last placement. Translation stays fixed.
static SVECTOR _gMineForkedTunnelAreaObjectChildRotationPath[54] = {
#include "assets/mine_forked_tunnel_motion_04404.inc"
};

enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_TASK_PARENT = 0,
    MINE_FORKED_TUNNEL_AREA_OBJECT_TASK_CHILD  = 1,
};

/// TMD recipes for the rolling area-object parent (0) and its tilting child (1).
///
/// Both run at priority `TASK_PRIORITY_CONTROL`; there is no terminator. The child's spawnArg2
/// borrows the live parent task, whose coordinate and lighting it shares.
/// Spawning copies a recipe; model resources and callbacks remain room-owned.
static TaskDesc _gMineForkedTunnelAreaObjectTaskDescs[2] = {
    [MINE_FORKED_TUNNEL_AREA_OBJECT_TASK_PARENT] = { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, mineForkedTunnelAreaObjectTask, { .model = &gMineForkedTunnelModel01B48 } },
    [MINE_FORKED_TUNNEL_AREA_OBJECT_TASK_CHILD]  = { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _mineForkedTunnelAreaObjectChildTask, { .model = &_gMineForkedTunnelModel03340 } },
};

/// Draw-mode and motion-command handlers for the rolling area object, then END.
///
/// The parent task borrows these three records and callbacks until teardown.
/// Draw requests carry an integer mode; commands borrow an ActorCommand whose
/// context is ignored and whose command selects reset, child tilt, path or park.
static const TaskMessageEntry _gMineForkedTunnelAreaObjectMessages[3] SECTION(".data") = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _mineForkedTunnelSetAreaObjectDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _mineForkedTunnelApplyAreaObjectCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Initial child-model placement relative to the area object's root coordinate.
///
/// XYZ uses integer game units and Euler angles use 4096 units per turn.
/// Attachment uses the whole transform; tilt playback retains its translation
/// and supplies rotations from the child path. Fourth components are unused.
static ActorTransform _gMineForkedTunnelAreaObjectChildPlacement = {
    .pos = { 180, -235, -780, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Final placement of the area object after the switch has been used.
///
/// Setup with a used switch and the park command apply this placement in the
/// root's parent frame. XYZ uses integer game units; the Y angle is 2047 of
/// 4096 units per turn. Fourth components are unused.
static ActorTransform _gMineForkedTunnelAreaObjectParkedPlacement = {
    .pos = { 2253, 964, 1131, 0 },
    .rot = { 0, 2047, 0, 0 },
};

/// Authored cell dimensions of the area-object collision template, in game units.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_COUNT_X = 1,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_COUNT_Z = 1,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_SIZE    = 4000,
};

/// Room-space normals for the collision box's three exposed faces, at 4096 per unit.
static SVECTOR _gMineForkedTunnelAreaObjectCollisionBoxNormals[MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT] = {
#include "assets/mine_forked_tunnel_collision_0469C_normals.inc"
};

/// Eight box corners in signed integer room-coordinate units, before the runtime offset.
static SVECTOR _gMineForkedTunnelAreaObjectCollisionBoxVerts[MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT] = {
#include "assets/mine_forked_tunnel_collision_0469C_verts.inc"
};

/// Three side quads indexing the template pools, all using room surface class zero.
static WorldCollisionGridFace _gMineForkedTunnelAreaObjectCollisionBoxFaces[MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT] = {
#include "assets/mine_forked_tunnel_collision_0469C_faces.inc"
};

/// One packed cell list containing all three face IDs followed by the -1 terminator.
static s16 _gMineForkedTunnelAreaObjectCollisionBoxCells[MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT + 1] = {
#include "assets/mine_forked_tunnel_collision_0469C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMineForkedTunnelAreaObjectCollisionBoxCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MINE_FORKED_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMineForkedTunnelAreaObjectCollisionBoxCells[(halfwordOffset)])
/// Single-cell directory retained with the authored collision-box grid.
static s16* _gMineForkedTunnelAreaObjectCollisionBoxTable[MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_COUNT_X * MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_COUNT_Z] = {
#include "assets/mine_forked_tunnel_collision_0469C_table.inc"
};
#undef MINE_FORKED_TUNNEL_CELL_FACE_LIST_AT_OFFSET

/// Collision-box template for the switch-controlled area object.
///
/// Rebuilding copies three normals/faces and eight vertices into the room
/// grid's reserved prefix, then offsets those vertices. This template is never
/// activated or bound to a view coordinate; its authored cell directory remains
/// part of the asset. Its pools stay live with this room overlay.
static WorldCollisionGrid _gMineForkedTunnelAreaObjectCollisionBox = {
    .viewCoord   = NULL,
    .normals     = _gMineForkedTunnelAreaObjectCollisionBoxNormals,
    .vertices    = _gMineForkedTunnelAreaObjectCollisionBoxVerts,
    .faces       = _gMineForkedTunnelAreaObjectCollisionBoxFaces,
    .cellFaceIds = _gMineForkedTunnelAreaObjectCollisionBoxTable,
    .xBias       = -1747,
    .zBias       = -7643,
    .cellCountX  = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_COUNT_X,
    .cellCountZ  = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_COUNT_Z,
    .cellSize    = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gMineForkedTunnelAreaObjectCollisionBoxFaces),
};

/// Tunnel transition, key-item refusal, switch-action and board-command handlers, then END.
///
/// The room receiver borrows these five ordered records and callback code
/// while loaded. TASK_MESSAGE_TABLE_END returns zero for unsupported IDs.
static const TaskMessageEntry _gMineForkedTunnelRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mineForkedTunnelResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mineForkedTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineForkedTunnelHandleSwitchAction },
    { ROOM_MESSAGE_COMMAND, _mineForkedTunnelBoardCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPackedPose _gMineForkedTunnelAnimation049C4Bank1[6] = {
#include "assets/mine_forked_tunnel_animation_049C4_bank1.inc"
};

static AnimationPackedRotation _gMineForkedTunnelAnimation049C4Bank4[46] = {
#include "assets/mine_forked_tunnel_animation_049C4_bank4.inc"
};

static AnimationRecord _gMineForkedTunnelAnimation049C4Records[109] = {
#include "assets/mine_forked_tunnel_animation_049C4_records.inc"
};

static u16 _gMineForkedTunnelAnimation049C4Indices[20] = {
#include "assets/mine_forked_tunnel_animation_049C4_indices.inc"
};

static AnimationSet _gMineForkedTunnelAnimation049C4 = {
    _gMineForkedTunnelAnimation049C4Records,
    _gMineForkedTunnelAnimation049C4Indices,
    { NULL, _gMineForkedTunnelAnimation049C4Bank1, NULL, NULL, _gMineForkedTunnelAnimation049C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineForkedTunnelAnimation04C50Bank1[2] = {
#include "assets/mine_forked_tunnel_animation_04C50_bank1.inc"
};

static AnimationPackedRotation _gMineForkedTunnelAnimation04C50Bank4[47] = {
#include "assets/mine_forked_tunnel_animation_04C50_bank4.inc"
};

static AnimationRecord _gMineForkedTunnelAnimation04C50Records[90] = {
#include "assets/mine_forked_tunnel_animation_04C50_records.inc"
};

static u16 _gMineForkedTunnelAnimation04C50Indices[20] = {
#include "assets/mine_forked_tunnel_animation_04C50_indices.inc"
};

static AnimationSet _gMineForkedTunnelAnimation04C50 = {
    _gMineForkedTunnelAnimation04C50Records,
    _gMineForkedTunnelAnimation04C50Indices,
    { NULL, _gMineForkedTunnelAnimation04C50Bank1, NULL, NULL, _gMineForkedTunnelAnimation04C50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineForkedTunnelAnimation05B1CBank1[33] = {
#include "assets/mine_forked_tunnel_animation_05B1C_bank1.inc"
};

static AnimationPackedRotation _gMineForkedTunnelAnimation05B1CBank4[381] = {
#include "assets/mine_forked_tunnel_animation_05B1C_bank4.inc"
};

static AnimationRecord _gMineForkedTunnelAnimation05B1CRecords[447] = {
#include "assets/mine_forked_tunnel_animation_05B1C_records.inc"
};

static u16 _gMineForkedTunnelAnimation05B1CIndices[20] = {
#include "assets/mine_forked_tunnel_animation_05B1C_indices.inc"
};

static AnimationSet _gMineForkedTunnelAnimation05B1C = {
    _gMineForkedTunnelAnimation05B1CRecords,
    _gMineForkedTunnelAnimation05B1CIndices,
    { NULL, _gMineForkedTunnelAnimation05B1CBank1, NULL, NULL, _gMineForkedTunnelAnimation05B1CBank4, NULL, NULL, NULL },
};

/// Bodyless interaction tasks: slot 0 operates the switch, slot 1 prompts for the oak board.
///
/// Both run at priority `TASK_PRIORITY_CONTROL` with zero descriptor payloads. Spawning borrows
/// callbacks from this room overlay; there is no terminator.
static TaskDesc _gMineForkedTunnelInteractionTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mineForkedTunnelSwitchInteractionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mineForkedTunnelOakBoardPromptTask, { .value = 0 } },
};

/// Player animation-bank extension for the switch scene, occupying set slots 47..50.
///
/// Slot 47 is NULL; slots 48..50 borrow the three room-owned animation sets.
/// The player copies these pointers, so their descriptors and packed data must
/// stay loaded through playback. There is no terminator.
static AnimationSet* _gMineForkedTunnelSwitchSceneAnimationSets[4] = {
    NULL,
    &_gMineForkedTunnelAnimation049C4,
    &_gMineForkedTunnelAnimation04C50,
    &_gMineForkedTunnelAnimation05B1C,
};

/// Request to copy the switch scene's four set pointers into the player's bank extension.
///
/// The event script borrows this request until dispatch. The source includes
/// a NULL slot and covers exactly the set table, without adjacent payload data.
static AnimationBankCopyRequest _gMineForkedTunnelSwitchSceneAnimationBankCopy = {
    .sets  = _gMineForkedTunnelSwitchSceneAnimationSets,
    .count = ARRAY_SIZE(_gMineForkedTunnelSwitchSceneAnimationSets),
};

enum {
    MINE_FORKED_TUNNEL_UNUSED_REQUEST_BANK = 1,
    MINE_FORKED_TUNNEL_UNUSED_REQUEST_CLIP = 1,
};

/// Retained, unused bank-1 clip-1 animation reset request.
///
/// Stores zero blend frames and enabled grid collision. No script selects
/// this record, so its intended receiver and scene use are unproven.
static const AnimationPlayRequest _gMineForkedTunnelUnusedBank1Clip1ResetRequest SECTION(".data") = {
    .source               = { .index = MINE_FORKED_TUNNEL_UNUSED_REQUEST_BANK },
    .animationId          = MINE_FORKED_TUNNEL_UNUSED_REQUEST_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum {
    MINE_FORKED_TUNNEL_SWITCH_REQUEST_BANK       = 1,
    MINE_FORKED_TUNNEL_SWITCH_PLAYER_CLIP_48     = ANIMATION_BANK_BASE_SET_COUNT + 1,
    MINE_FORKED_TUNNEL_SWITCH_PLAYER_CLIP_49     = ANIMATION_BANK_BASE_SET_COUNT + 2,
    MINE_FORKED_TUNNEL_SWITCH_PLAYER_CLIP_50     = ANIMATION_BANK_BASE_SET_COUNT + 3,
    MINE_FORKED_TUNNEL_SWITCH_FINAL_BLEND_FRAMES = 30,
};

/// Final player clip request for the normal switch scene, interpolated over thirty frames.
///
/// Clip 48 occupies extension slot 1. The script copies this request and resolves
/// the equipped weapon bank before dispatch; its stored bank seed is not used.
/// Grid collision stays disabled. The extension's room data must remain loaded.
static AnimationPlayRequest _gMineForkedTunnelSwitchScenePlayerClip48Request = {
    { .index = MINE_FORKED_TUNNEL_SWITCH_REQUEST_BANK },
    MINE_FORKED_TUNNEL_SWITCH_PLAYER_CLIP_48,
    ANIMATION_BLEND_INTERPOLATE,
    MINE_FORKED_TUNNEL_SWITCH_FINAL_BLEND_FRAMES,
    ANIMATION_WORLD_COLLISION_DISABLE,
};

/// First player clip request for the normal switch scene, resetting extension clip 49.
///
/// Zero blend frames; grid collision stays disabled. The script resolves the
/// equipped bank in a copy, ignoring the seed; room clip data remains borrowed.
static AnimationPlayRequest _gMineForkedTunnelSwitchScenePlayerClip49Request = {
    { .index = MINE_FORKED_TUNNEL_SWITCH_REQUEST_BANK },
    MINE_FORKED_TUNNEL_SWITCH_PLAYER_CLIP_49,
    ANIMATION_BLEND_RESET,
    0,
    ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Middle player clip request for the normal switch scene, resetting extension clip 50.
///
/// Zero blend frames; grid collision stays disabled. The script resolves the
/// equipped bank in a copy, ignoring the seed; room clip data remains borrowed.
static AnimationPlayRequest _gMineForkedTunnelSwitchScenePlayerClip50Request = {
    { .index = MINE_FORKED_TUNNEL_SWITCH_REQUEST_BANK },
    MINE_FORKED_TUNNEL_SWITCH_PLAYER_CLIP_50,
    ANIMATION_BLEND_RESET,
    0,
    ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Player's room-space placement after the switch scene's first clip starts.
///
/// XYZ use world game units; yaw is 2047 of 4096 units per turn. The normal
/// event script borrows the record through synchronous placement dispatch.
static ActorTransform _gMineForkedTunnelSwitchScenePlayerTransform = { { 2251, 0, 9707, 0 }, { 0, 2047, 0, 0 } };

/// Stops the area object and its child and restores the parent's first path pose.
///
/// The normal switch script borrows this command through dispatch. The receiver
/// ignores context and resets both motion cursors, leaving the child's pose intact.
static ActorCommand _gMineForkedTunnelAreaObjectResetCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL } },
    .command = MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_RESET,
};

/// Starts the area object's child tilt when the child task exists.
///
/// The normal switch script borrows this command through dispatch. The receiver
/// ignores context and preserves the parent's motion and both path cursors.
static ActorCommand _gMineForkedTunnelAreaObjectTiltChildCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL } },
    .command = MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_TILT_CHILD,
};

/// Starts the area object's parent path motion without rewinding its cursor.
///
/// The normal switch script borrows this command through dispatch. The receiver
/// ignores context and leaves the child's motion unchanged.
static ActorCommand _gMineForkedTunnelAreaObjectStartPathCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL } },
    .command = MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_START_PATH,
};

/// Parks and stops the area object at the end of either normal or skipped switch playback.
///
/// The receiver also rebuilds its collision box from the switch flag, leaving
/// the child and both motion cursors unchanged. Scripts borrow this four-byte
/// payload until dispatch; the receiver ignores its stage/area context.
static ActorCommand _gMineForkedTunnelAreaObjectParkCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL } },
    .command = MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_PARK,
};

/// Scene selectors and timing shared by the normal and skipped switch motion.
enum {
    MINE_FORKED_TUNNEL_SWITCH_SCENE_OBJECT_ID             = 55,
    MINE_FORKED_TUNNEL_SWITCH_SCENE_CHILD_TILT_VIEW       = 6,
    MINE_FORKED_TUNNEL_SWITCH_SCENE_PATH_START_VIEW       = 7,
    MINE_FORKED_TUNNEL_SWITCH_SCENE_PATH_END_VIEW         = 2,
    MINE_FORKED_TUNNEL_SWITCH_SCENE_RETURN_VIEW           = 3,
    MINE_FORKED_TUNNEL_SWITCH_SCENE_SKIP_FADE_WAIT_FRAMES = 8,
};

/// Switch-motion scene: animate the player, tilt/roll/park the prop and restore presentation.
///
/// Installs the player's extension clips and sends four area-object commands
/// in order. View cuts and frame waits synchronize sound and motion; END follows
/// control/equipment restoration. All 35 commands, payloads and model/animation
/// resources are borrowed through normal or skipped execution.
static EvsCommand _gMineForkedTunnelSwitchSceneScript[35] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineForkedTunnelSwitchSceneAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineForkedTunnelSwitchScenePlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineForkedTunnelSwitchScenePlayerTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_OBJECT_ID }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineForkedTunnelAreaObjectResetCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_CHILD_TILT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_OBJECT_ID }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineForkedTunnelAreaObjectTiltChildCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 3) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_PATH_START_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineForkedTunnelSwitchScenePlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_MINE_FORKED_TUNNEL_OBJECT_MOVE }, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_OBJECT_ID }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineForkedTunnelAreaObjectStartPathCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_PATH_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineForkedTunnelSwitchScenePlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineForkedTunnelRestoreAreaObjectSoundMix }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 59 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = SOUND_MINE_FORKED_TUNNEL_OBJECT_MOVE }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_OBJECT_ID }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineForkedTunnelAreaObjectParkCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 6) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Input-skip stream that parks the switch prop and restores the closing room view.
///
/// Stops movement sound, fades to black, resumes the player and weapons, parks
/// the prop and dirties view 3 before scene cleanup and fade return. Twelve
/// commands include END; the player, prop and referenced room records must live
/// through execution. The switch-used flag is already set by the spawning task.
static EvsCommand _gMineForkedTunnelSwitchSceneSkipScript[12] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = SOUND_MINE_FORKED_TUNNEL_OBJECT_MOVE }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_OBJECT_ID }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineForkedTunnelAreaObjectParkCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 6) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_FORKED_TUNNEL_SWITCH_SCENE_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Shared flare centre for mapped views 2 and 3, in integer world-coordinate units.
static SVECTOR _gMineForkedTunnelView2And3FlarePoint = { 2250, -1870, 9120, 0 };

/// Two flare centres drawn together in mapped view 4, in integer world-coordinate units.
static SVECTOR _gMineForkedTunnelView4FlarePoints[2] = {
    { 2250, -1260, 6010, 0 },
    { 2250, -1060, 3200, 0 },
};

/// Flare centre for mapped view 5, in integer world-coordinate units.
static SVECTOR _gMineForkedTunnelView5FlarePoint = { 430, -950, 2530, 0 };

WorldCoordRoomLighting gMineForkedTunnelRoomLighting[1] = {
    { &_gMineForkedTunnelRoomLights, _gMineForkedTunnelViewAmbientTable },
};

WorldCollisionRoomResources gMineForkedTunnelRoomCollisionResources[1] = {
    {
        .grid                 = &_gMineForkedTunnelCollisionGrid,
        .viewBoundaryTriggers = _gMineForkedTunnelViewBoundaryTriggers,
        .actionTriggers       = _gMineForkedTunnelActionTriggers,
        .occluders            = NULL,
    },
};

u8* gMineForkedTunnelRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gMineForkedTunnelRoomViewCounts[1] = { ARRAY_SIZE(gMineForkedTunnelViewCameras) };

/// Room-owned warp sound requests; the endpoint has no blocked-transition sound.
enum {
    MINE_FORKED_TUNNEL_WARP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 2),
    MINE_FORKED_TUNNEL_WARP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 1),
};

DirectionWarpEntry gMineForkedTunnelWarpEntries[1] = {
    {
        .player         = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 2350, 0, 10500 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 2350, 0, 10500 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = MINE_FORKED_TUNNEL_WARP_ARRIVAL_SOUND,
        .departureSound = MINE_FORKED_TUNNEL_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-space collision normals; the first three are reserved for the area-object box.
///
/// Unit length is 4096. The box rebuild replaces only XYZ in the reserved prefix.
static SVECTOR _gMineForkedTunnelCollisionGridNormals[33] = {
#include "assets/mine_forked_tunnel_collision_067B0_normals.inc"
};

/// Room-space collision vertices in integer game units; the first eight form the movable box.
static SVECTOR _gMineForkedTunnelCollisionGridVerts[88] = {
#include "assets/mine_forked_tunnel_collision_067B0_verts.inc"
};

/// Indexed collision faces; the first three are replaced by the area-object box faces.
///
/// Remaining faces use surface classes 0 and 1 and index this grid's vertex
/// and normal pools. The zeroed box prefix is populated during object setup.
static WorldCollisionGridFace _gMineForkedTunnelCollisionGridFaces[37] = {
#include "assets/mine_forked_tunnel_collision_067B0_faces.inc"
};

/// Packed face-index lists for the collision grid's eight cells, each terminated by -1.
static s16 _gMineForkedTunnelCollisionGridCells[160] = {
#include "assets/mine_forked_tunnel_collision_067B0_cells.inc"
};

/// Collision-grid dimensions; cells are square and measured in integer game units.
enum {
    MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_COUNT_X = 2,
    MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_COUNT_Z = 4,
    MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_SIZE    = 4000,
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMineForkedTunnelCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MINE_FORKED_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMineForkedTunnelCollisionGridCells[(halfwordOffset)])
/// X-major cell directory, indexed by x * cellCountZ + z.
static s16* _gMineForkedTunnelCollisionGridTable[MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_COUNT_X * MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_COUNT_Z] = {
#include "assets/mine_forked_tunnel_collision_067B0_table.inc"
};
#undef MINE_FORKED_TUNNEL_CELL_FACE_LIST_AT_OFFSET

/// Active tunnel collision grid with reserved storage for the switch-controlled prop box.
///
/// The room-resource descriptor borrows this grid; activation binds its view
/// coordinate. Object setup and parking replace three normals/faces and eight
/// vertices, lifting the box out of the passage after the switch is used.
/// All pools and the fixed cell directory stay live with the room overlay.
static WorldCollisionGrid _gMineForkedTunnelCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gMineForkedTunnelCollisionGridNormals,
    .vertices    = _gMineForkedTunnelCollisionGridVerts,
    .faces       = _gMineForkedTunnelCollisionGridFaces,
    .cellFaceIds = _gMineForkedTunnelCollisionGridTable,
    .xBias       = 650,
    .zBias       = 520,
    .cellCountX  = MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_COUNT_X,
    .cellCountZ  = MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_COUNT_Z,
    .cellSize    = MINE_FORKED_TUNNEL_COLLISION_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gMineForkedTunnelCollisionGridFaces),
};

ViewCamera gMineForkedTunnelViewCameras[7] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { -1500, 0x40CA, -5500 } }, 289 },
    { { { { 4001, 0, 872 }, { -228, 3952, 1049 }, { -842, -1074, 3861 } }, { -2676, -186, -4405 } }, 289 },
    { { { { 2839, 0, 2952 }, { 2773, 1402, -2667 }, { -1010, 3848, 972 } }, { -2889, 3366, -9548 } }, 269 },
    { { { { -3961, 0, 1040 }, { 284, 3939, 1084 }, { -1001, 1121, -3810 } }, { -2883, 1117, -8571 } }, 289 },
    { { { { -2159, 0, 3480 }, { 2753, 2505, 1708 }, { -2128, 3240, -1320 } }, { -2927, 2785, -2683 } }, 257 },
    { { { { -605, 0, 4050 }, { 477, 4067, 71 }, { -4022, 483, -601 } }, { -3100, 876, -9740 } }, 289 },
    { { { { -3711, 0, -1732 }, { -1330, 2622, 2850 }, { 1109, 3145, -2376 } }, { -1662, 2731, -0x2A11 } }, 269 },
};

/// Neutral channel modulation for the authored sprite sources; 128 preserves texture intensity.
enum { MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR = 128 };

/// Empty sprite list for mapped view 1; the zero-count first record requests image strips.
static SpriteBatch _gMineForkedTunnelView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles and sorting depths for mapped view 2's 35 room sprites.
///
/// Positions and dimensions are screen pixels; UV selects texels within each
/// encoded texture page. The view's batches partition the complete array.
static SpriteSource _gMineForkedTunnelView2SpriteSources[35] = {
    { 142, 0x3FC0, { .fields = { 40, 64 } }, -136, -120, 517, { .fields = { 104, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -144, -56, 483, { .fields = { 88, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, 24, 444, { .fields = { 16, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 429, { .fields = { 48, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -56, 434, { .fields = { 112, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, -120, 469, { .fields = { 8, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, -120, 433, { .fields = { 112, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 96, -96, 389, { .fields = { 64, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, -16, 364, { .fields = { 88, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 112, 64, 364, { .fields = { 120, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 128, 64, 314, { .fields = { 104, 176 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 128, -16, 331, { .fields = { 88, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, -96, 356, { .fields = { 48, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 374, { .fields = { 40, 184 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -56, 1246, { .fields = { 32, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -80, 1575, { .fields = { 40, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -24, -112, 1338, { .fields = { 64, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 16, -112, 1320, { .fields = { 64, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 48, -112, 1149, { .fields = { 64, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, -72, 1110, { .fields = { 96, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 64, -56, 1096, { .fields = { 64, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -24, -72, 1269, { .fields = { 32, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -24, 0, 1186, { .fields = { 16, 72 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 64, 16, 1034, { .fields = { 112, 64 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, 48, 571, { .fields = { 88, 104 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, 88, 492, { .fields = { 72, 104 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 478, { .fields = { 88, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 16, 629, { .fields = { 80, 72 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 594, { .fields = { 88, 224 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 569, { .fields = { 88, 184 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, -8, 683, { .fields = { 104, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, 0, 634, { .fields = { 24, 208 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 32, 655, { .fields = { 8, 208 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, 80, 613, { .fields = { 96, 64 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, 0, 702, { .fields = { 120, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Three source ranges for mapped view 2, between the background selector and list terminator.
///
/// Ranges cover source entries 0..13, 14..23 and 24..34. Authored trailing
/// metadata bytes are retained; their role is unproven.
static SpriteBatch _gMineForkedTunnelView2SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 10, 0, 0, { 2, 0 } },
    { 24, ARRAY_SIZE(_gMineForkedTunnelView2SpriteSources) - 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles and sorting depths for mapped view 3's 36 room sprites.
///
/// Positions and dimensions are screen pixels; UV selects texels within each
/// encoded texture page. The view's batches partition the complete array.
static SpriteSource _gMineForkedTunnelView3SpriteSources[36] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 40, 350, { .fields = { 80, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 48, 96, 213, { .fields = { 24, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 72, 224, { .fields = { 80, 176 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 72, 72, 219, { .fields = { 40, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 88, 48, 224, { .fields = { 40, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, 16, 235, { .fields = { 64, 136 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -8, 250, { .fields = { 72, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 16, 528, { .fields = { 96, 168 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 16, 850, { .fields = { 120, 248 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 24, 591, { .fields = { 112, 248 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 32, 671, { .fields = { 104, 168 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 32, 777, { .fields = { 88, 168 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 40, 819, { .fields = { 56, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 24, 844, { .fields = { 64, 72 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 40, 737, { .fields = { 64, 168 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 32, 641, { .fields = { 80, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 24, 568, { .fields = { 72, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 16, 518, { .fields = { 72, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -48, 436, { .fields = { 112, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -32, 486, { .fields = { 112, 136 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -16, 549, { .fields = { 96, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -8, 647, { .fields = { 112, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 8, 705, { .fields = { 88, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 24, 703, { .fields = { 64, 200 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 16, 616, { .fields = { 64, 184 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 8, 548, { .fields = { 56, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 0, 493, { .fields = { 56, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 117, { .fields = { 96, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 101, { .fields = { 56, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 112, 119, { .fields = { 48, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 112, 150, { .fields = { 40, 72 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 104, 195, { .fields = { 96, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 104, 246, { .fields = { 96, 32 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 96, 362, { .fields = { 96, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 96, 588, { .fields = { 80, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 104, 747, { .fields = { 88, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Two source ranges for mapped view 3, between the background selector and list terminator.
///
/// Ranges cover source entries 0..6 and 7..35. Authored trailing metadata
/// bytes are retained; their role is unproven.
static SpriteBatch _gMineForkedTunnelView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, ARRAY_SIZE(_gMineForkedTunnelView3SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for mapped view 4: 80 room sprites followed by two oak-board sprites.
///
/// Positions and dimensions are screen pixels; depth is the unscaled OT
/// sorting value. The final two sources use the board's separate palette and
/// share the batch whose visibility follows the saved pickup state.
static SpriteSource _gMineForkedTunnelView4SpriteSources[82] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 80, 857, { .fields = { 16, 112 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -24, -120, 604, { .fields = { 64, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, -144, -120, 542, { .fields = { 64, 64 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -120, 40, 630, { .fields = { 72, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 72, 656, { .fields = { 32, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, -128, 0, 607, { .fields = { 104, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -136, -40, 584, { .fields = { 104, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -136, -88, 561, { .fields = { 64, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -80, 736, { .fields = { 96, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -120, 706, { .fields = { 40, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 56, -32, 756, { .fields = { 72, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, -120, 683, { .fields = { 40, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -80, 710, { .fields = { 104, 208 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -32, 738, { .fields = { 0, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 0, 753, { .fields = { 104, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, 16, 793, { .fields = { 32, 136 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 56, 822, { .fields = { 80, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 80, 64, 798, { .fields = { 80, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 16, 791, { .fields = { 72, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -120, 576, { .fields = { 48, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -96, 1286, { .fields = { 56, 136 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -88, 1982, { .fields = { 72, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -120, 1926, { .fields = { 0, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, -120, 1182, { .fields = { 72, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -96, 1208, { .fields = { 8, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -88, -64, 1246, { .fields = { 120, 32 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -88, -32, 1282, { .fields = { 96, 32 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 0, 1335, { .fields = { 56, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -32, -120, 1276, { .fields = { 104, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -96, 1309, { .fields = { 16, 176 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, -8, 1427, { .fields = { 96, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, -56, 1363, { .fields = { 96, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 432, { .fields = { 56, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 72, 326, { .fields = { 40, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 48, 351, { .fields = { 104, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 24, 374, { .fields = { 56, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 8, 406, { .fields = { 88, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -144, 16, 425, { .fields = { 112, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, 48, 480, { .fields = { 64, 136 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, 64, 452, { .fields = { 96, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 88, 436, { .fields = { 56, 200 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 104, 425, { .fields = { 16, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 104, 400, { .fields = { 40, 176 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 104, 377, { .fields = { 80, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 88, 401, { .fields = { 80, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 72, 409, { .fields = { 0, 64 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, 48, 408, { .fields = { 80, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 24, 441, { .fields = { 72, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -160, -120, 484, { .fields = { 120, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -120, 1083, { .fields = { 16, 32 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 900, { .fields = { 40, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 16, 1375, { .fields = { 32, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -8, 1337, { .fields = { 64, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 0, -120, 1260, { .fields = { 72, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, -112, 1211, { .fields = { 88, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -104, 1298, { .fields = { 72, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 24, -96, 1437, { .fields = { 120, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 1321, { .fields = { 24, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, -120, 1044, { .fields = { 72, 120 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -120, 1014, { .fields = { 96, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 0, 1175, { .fields = { 32, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 24, -32, 1190, { .fields = { 120, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -120, 934, { .fields = { 56, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -120, 805, { .fields = { 48, 56 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -104, 997, { .fields = { 48, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -104, 866, { .fields = { 56, 184 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -88, 1120, { .fields = { 64, 240 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -88, 1021, { .fields = { 56, 152 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, -72, 1176, { .fields = { 24, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -72, 1068, { .fields = { 16, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -32, 1114, { .fields = { 56, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -32, 980, { .fields = { 56, 40 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -16, 1110, { .fields = { 56, 216 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 0, 1112, { .fields = { 56, 168 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 16, 1092, { .fields = { 56, 112 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 32, 969, { .fields = { 72, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -16, 918, { .fields = { 88, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 964, { .fields = { 40, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 32, 892, { .fields = { 0, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 64, -120, 877, { .fields = { 120, 72 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 8, -88, 1984, { .fields = { 72, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 0, -40, 2019, { .fields = { 72, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Five source ranges for mapped view 4, with the final range controlling the oak board.
///
/// Ranges partition source entries 0..81. Batch 5 selects entries 80..81 and
/// is hidden after the board is collected. The first zero-count record selects
/// image strips; the terminal record ends the list. Trailing metadata is unproven.
static SpriteBatch _gMineForkedTunnelView4SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { 19, 13, 0, 0, { 3, 0 } },
    { 32, 17, 0, 0, { 2, 0 } },
    { 49, 31, 0, 0, { 4, 0 } },
    { 80, ARRAY_SIZE(_gMineForkedTunnelView4SpriteSources) - 80, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for mapped view 5: 20 room sprites followed by six oak-board sprites.
///
/// Positions and dimensions are screen pixels; depth is the unscaled OT
/// sorting value. The final six sources use the board's separate palette and
/// share the batch whose visibility follows the saved pickup state.
static SpriteSource _gMineForkedTunnelView5SpriteSources[26] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, -72, 696, { .fields = { 80, 112 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, -88, 672, { .fields = { 64, 160 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 685, { .fields = { 24, 176 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, -120, 642, { .fields = { 88, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 96, -120, 611, { .fields = { 64, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -88, 358, { .fields = { 88, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -72, 381, { .fields = { 112, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -56, 404, { .fields = { 120, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 16, 272, { .fields = { 64, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 24, 285, { .fields = { 72, 128 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 32, 300, { .fields = { 64, 128 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 40, 317, { .fields = { 64, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, -16, 437, { .fields = { 72, 208 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 564, { .fields = { 56, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -64, 513, { .fields = { 72, 24 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -40, 473, { .fields = { 64, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 8, 410, { .fields = { 56, 104 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 32, 383, { .fields = { 64, 232 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 56, 357, { .fields = { 72, 176 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 330, { .fields = { 72, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -8, -120, 919, { .fields = { 96, 0 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -32, -120, 961, { .fields = { 104, 144 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -8, -72, 1039, { .fields = { 104, 96 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -32, -72, 1087, { .fields = { 104, 48 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -8, -24, 1213, { .fields = { 104, 192 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -32, -24, 1271, { .fields = { 80, 80 } }, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, MINE_FORKED_TUNNEL_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Three source ranges for mapped view 5, with the final range controlling the oak board.
///
/// Ranges cover source entries 0..1, 2..19 and 20..25. Batch 3 selects the
/// board and is hidden after collection. The first zero-count record selects
/// image strips; the terminal record ends the list. Trailing metadata is unproven.
static SpriteBatch _gMineForkedTunnelView5SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 1, 0 } },
    { 2, 18, 0, 0, { 2, 0 } },
    { 20, ARRAY_SIZE(_gMineForkedTunnelView5SpriteSources) - 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 6; the zero-count first record requests image strips.
static SpriteBatch _gMineForkedTunnelView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 7; the zero-count first record requests image strips.
static SpriteBatch _gMineForkedTunnelView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gMineForkedTunnelSpriteViews[7] = {
    { { .empty = _gMineForkedTunnelView1SpriteBatches }, _gMineForkedTunnelView1SpriteBatches, NULL },
    { { .elements = _gMineForkedTunnelView2SpriteSources }, _gMineForkedTunnelView2SpriteBatches, NULL },
    { { .elements = _gMineForkedTunnelView3SpriteSources }, _gMineForkedTunnelView3SpriteBatches, NULL },
    { { .elements = _gMineForkedTunnelView4SpriteSources }, _gMineForkedTunnelView4SpriteBatches, NULL },
    { { .elements = _gMineForkedTunnelView5SpriteSources }, _gMineForkedTunnelView5SpriteBatches, NULL },
    { { .empty = _gMineForkedTunnelView6SpriteBatches }, _gMineForkedTunnelView6SpriteBatches, NULL },
    { { .empty = _gMineForkedTunnelView7SpriteBatches }, _gMineForkedTunnelView7SpriteBatches, NULL },
};

/// Four mutable point lights shading models throughout the tunnel.
///
/// Positions and falloff radii use integer world units; RGB uses 4096 for full
/// intensity. All admit every logical view. Room-light updates bind the view
/// parent and compose coordinates; queries overwrite attenuation.
static WorldCoordPointLight _gMineForkedTunnelPointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2240, -1639, 9120 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3316, 2385 }, { 0, 0 } }, 259, 2501 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2310, -657, 6485 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3211, 2810, 1964 }, { 0, 0 } }, 201, 3004 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2240, -900, 3700 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3397, 2467 }, { 0, 0 } }, 259, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1421, -769, 2480 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3316, 2385 }, { 0, 0 } }, 280, 2961 },
};

/// The tunnel's light collection: four point lights, with no directional or cone lights.
static WorldCoordRoomLights _gMineForkedTunnelRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gMineForkedTunnelPointLights),
    .pointLights           = _gMineForkedTunnelPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed view boundaries for both directions of links 5/4, 4/2 and 2/3.
///
/// Coordinates are room-local game units and normals use 4096 per unit.
/// Gameplay binds and enables all six mutable records, stopping after the
/// LAST-marked final boundary. Links and hit latches require writable storage
/// kept live until unlinked; views 1, 6 and 7 have no boundaries here.
static WorldCollisionTrigger _gMineForkedTunnelViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 1572, -960, 2665, 0 }, { { -829, -3520, 1976, 0 }, { 830, -3520, -1975, 0 }, { -829, 3520, 1976, 0 }, { 830, 3520, -1975, 0 } }, { -3786, 0, -1590, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1440, -976, 2644, 0 }, { { 796, -3552, -1936, 0 }, { -795, -3552, 1937, 0 }, { 796, 3552, -1936, 0 }, { -795, 3552, 1937, 0 } }, { 3789, 0, 1556, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2272, -992, 6400, 0 }, { { 1536, -2864, 0, 0 }, { -1536, -2864, 0, 0 }, { 1536, 2864, 0, 0 }, { -1536, 2864, 0, 0 } }, { 0, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2304, -1041, 6561, 0 }, { { -1536, -3008, 0, 0 }, { 1536, -3008, 0, 0 }, { -1536, 3008, 0, 0 }, { 1536, 3008, 0, 0 } }, { 0, 0, -4114, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2304, -1040, 9488, 0 }, { { -1536, -3040, 0, 0 }, { 1536, -3040, 0, 0 }, { -1536, 3040, 0, 0 }, { 1536, 3040, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 3405, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2336, -944, 9408, 0 }, { { 1536, -2976, 0, 0 }, { -1536, -2976, 0, 0 }, { 1536, 2976, 0, 0 }, { -1536, 2976, 0, 0 } }, { 0, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Departure/arrival slot pair for the tunnel-to-entrance warp: local 1, destination 2.
enum { MINE_FORKED_TUNNEL_ENTRANCE_WARP_ENDPOINTS = (1 << 4) | 2 };

/// Facing interaction quads for the exit, switch, oak board and CAP command points.
///
/// The exit requests Tunnel Entrance using departure slot 1 and arrival slot 2.
/// The switch sends room action 1; the board sends room command 2. Remaining
/// records run CAP command 3 or 4. Gameplay binds/links all six writable
/// room-space quads through the LAST-marked final record. Positions/radii use
/// game units and normals use 4096 per unit; keep storage live until unlinked.
static WorldCollisionTrigger _gMineForkedTunnelActionTriggers[6] = {
    { NULL, NULL, NULL, { 2240, -48, 0x2A10, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MINE_TUNNEL_ENTRANCE, MINE_FORKED_TUNNEL_ENTRANCE_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2208, -64, 9600, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, 896, 976, 0 }, { { -656, 0, -1088, 0 }, { 656, 0, -1088, 0 }, { -656, 0, 1088, 0 }, { 656, 0, 1088, 0 } }, { 0, 4105, 0, 0 }, { 4052, 0, 601, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1632, 896, 576, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2304, 912, 2080, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, 4091, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1584, 896, 1088, 0 }, { { -304, 0, -1136, 0 }, { 304, 0, -1136, 0 }, { -304, 0, 1136, 0 }, { 304, 0, 1136, 0 } }, { 0, 4112, 0, 0 }, { -4091, 0, 201, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource keys joining the encounter placements to their loading/spawn recipes.
enum {
    MINE_FORKED_TUNNEL_RESOURCE_MOTH         = 8,
    MINE_FORKED_TUNNEL_RESOURCE_MIND_SUCKLER = 15,
    MINE_FORKED_TUNNEL_RESOURCE_SCAVENGER    = 16,
    MINE_FORKED_TUNNEL_RESOURCE_SCORPION     = 25,
    MINE_FORKED_TUNNEL_RESOURCE_BAT          = 37,
};

/// Actor-specific placement modes; these equal values have different meanings.
enum {
    MINE_FORKED_TUNNEL_MIND_SUCKLER_WALL_NO_REPERCH = 2,
    MINE_FORKED_TUNNEL_BAT_BACK_PERCH               = 2,
};

/// Empty encounter's resource list, containing only the end record.
static AreaResource _gMineForkedTunnelEmptyResources[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Scavenger loading/spawn recipe for variant 2, followed by the end record.
///
/// Resource key 16 selects base-group-20 file 16 and its sole task descriptor.
/// Loading and spawning borrow this list while the room overlay is loaded.
static AreaResource _gMineForkedTunnelScavengerResources[2] = {
    { MINE_FORKED_TUNNEL_RESOURCE_SCAVENGER, 16, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor01600ScavengerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Scorpion and moth loading/spawn recipes for variant 3, followed by the end record.
///
/// Key 25 selects base-group-20 file 25 and descriptor 0 (the scorpion body);
/// key 8 selects base-group-30 file 7 and the moth descriptor. Loading and
/// spawning borrow this list while the room overlay is loaded.
static AreaResource _gMineForkedTunnelScorpionAndMothResources[3] = {
    { MINE_FORKED_TUNNEL_RESOURCE_SCORPION, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor02500Tasks },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor300700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Scorpion and Mind Suckler loading/spawn recipes for variant 4, followed by the end record.
///
/// Key 25 selects base-group-20 file 25 and descriptor 0 (the scorpion body);
/// key 15 selects base-group-30 file 15 and the Mind Suckler descriptor.
/// Loading and spawning borrow this list while the room overlay is loaded.
static AreaResource _gMineForkedTunnelScorpionAndMindSucklerResources[3] = {
    { MINE_FORKED_TUNNEL_RESOURCE_SCORPION, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor02500Tasks },
    { MINE_FORKED_TUNNEL_RESOURCE_MIND_SUCKLER, 15, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Bat loading/spawn recipe for variant 11, followed by the end record.
///
/// Resource key 37 selects base-group-10 file 37 and its sole task descriptor.
/// Loading and spawning borrow this list while the room overlay is loaded.
static AreaResource _gMineForkedTunnelBatResources[2] = {
    { MINE_FORKED_TUNNEL_RESOURCE_BAT, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103700BatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Empty encounter's placement list, containing only the end record.
static AreaPlacement _gMineForkedTunnelEmptyPlacements[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Four scavenger placements for variant 2, followed by the end record.
///
/// XYZ uses world game units and yaw uses 4096 units per turn. All use the
/// base resource, actor variant/mode/row zero, texture offset 2 pages and
/// CLUT offset 4 rows. Actors borrow these records while the room is loaded.
static AreaPlacement _gMineForkedTunnelScavengerPlacements[5] = {
    { MINE_FORKED_TUNNEL_RESOURCE_SCAVENGER, 0, 0, 1900, 225, 7900, 1900, 0, 2, 4, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_SCAVENGER, 0, 0, 2640, 500, 6140, 2250, 0, 2, 4, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_SCAVENGER, 0, 0, 1380, 1000, 630, 1580, 0, 2, 4, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_SCAVENGER, 0, 0, 860, 1000, 800, 2650, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two scorpion and eight moth placements for variant 3, followed by the end record.
///
/// XYZ uses world game units and yaw uses 4096 units per turn. Scorpions
/// select movement-tuning row 2; moths select pursuit-speed row 3, both within
/// eight-row tables. All use base files and actor variant/mode zero. Texture
/// page/CLUT-row offsets are 2/4 for scorpions and 4/6 for moths. Actors borrow
/// these records while the room is loaded.
static AreaPlacement _gMineForkedTunnelScorpionAndMothPlacements[11] = {
    { MINE_FORKED_TUNNEL_RESOURCE_SCORPION, 0, 0, 2000, 550, 5650, 0, 0, 2, 4, 2 },
    { MINE_FORKED_TUNNEL_RESOURCE_SCORPION, 0, 0, 2200, 1000, 2300, 0, 0, 2, 4, 2 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 2300, -1300, 9300, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 2500, -1400, 9100, 3072, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 2000, -1300, 9400, 1024, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 2000, -1000, 3950, 2750, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 2100, -1000, 3700, 3072, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 2000, -1200, 3600, 3500, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 700, -800, 2250, 3500, 0, 4, 6, 3 },
    { MINE_FORKED_TUNNEL_RESOURCE_MOTH, 0, 0, 600, -900, 2400, 3800, 0, 4, 6, 3 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two scorpion and three Mind Suckler placements for variant 4, followed by the end record.
///
/// XYZ uses world game units and yaw uses 4096 units per turn. Mind Sucklers
/// start on walls and cannot perch again during pursuit. All use base files,
/// actor variant and tuning row zero. Texture page/CLUT-row offsets are 2/4
/// for scorpions and 4/6 for Mind Sucklers. Actors borrow the loaded room's records.
static AreaPlacement _gMineForkedTunnelScorpionAndMindSucklerPlacements[6] = {
    { MINE_FORKED_TUNNEL_RESOURCE_SCORPION, 0, 0, 2200, 900, 3500, 0, 0, 2, 4, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_SCORPION, 0, 0, 2200, 1000, 2300, 0, 0, 2, 4, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_MIND_SUCKLER, 0, MINE_FORKED_TUNNEL_MIND_SUCKLER_WALL_NO_REPERCH, 2500, -500, 3000, 1024, 0, 4, 6, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_MIND_SUCKLER, 0, MINE_FORKED_TUNNEL_MIND_SUCKLER_WALL_NO_REPERCH, 1500, -500, 3000, 3072, 0, 4, 6, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_MIND_SUCKLER, 0, MINE_FORKED_TUNNEL_MIND_SUCKLER_WALL_NO_REPERCH, 1500, -500, 6000, 3072, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two bat placements for variant 11, followed by the end record.
///
/// XYZ uses world game units and yaw uses 4096 units per turn. Both start
/// perched with a backwards takeoff, using the base file, actor variant/row
/// zero, texture offset 2 pages and CLUT offset 4 rows. Actors borrow these
/// records while the room overlay is loaded.
static AreaPlacement _gMineForkedTunnelBatPlacements[3] = {
    { MINE_FORKED_TUNNEL_RESOURCE_BAT, 0, MINE_FORKED_TUNNEL_BAT_BACK_PERCH, 330, -500, 1700, 3072, 0, 2, 4, 0 },
    { MINE_FORKED_TUNNEL_RESOURCE_BAT, 0, MINE_FORKED_TUNNEL_BAT_BACK_PERCH, 1100, -500, 380, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Saved placement-variant selectors populated by this room's encounter directory.
enum {
    MINE_FORKED_TUNNEL_VARIANT_EMPTY                       = 1,
    MINE_FORKED_TUNNEL_VARIANT_SCAVENGERS                  = 2,
    MINE_FORKED_TUNNEL_VARIANT_SCORPIONS_AND_MOTHS         = 3,
    MINE_FORKED_TUNNEL_VARIANT_SCORPIONS_AND_MIND_SUCKLERS = 4,
    MINE_FORKED_TUNNEL_VARIANT_BATS                        = 11,
};

AreaVariant gMineForkedTunnelAreaVariants[12] = {
    [MINE_FORKED_TUNNEL_VARIANT_EMPTY] = {
        .placements = _gMineForkedTunnelEmptyPlacements,
        .resources  = _gMineForkedTunnelEmptyResources,
    },
    [MINE_FORKED_TUNNEL_VARIANT_SCAVENGERS] = {
        .placements = _gMineForkedTunnelScavengerPlacements,
        .resources  = _gMineForkedTunnelScavengerResources,
    },
    [MINE_FORKED_TUNNEL_VARIANT_SCORPIONS_AND_MOTHS] = {
        .placements = _gMineForkedTunnelScorpionAndMothPlacements,
        .resources  = _gMineForkedTunnelScorpionAndMothResources,
    },
    [MINE_FORKED_TUNNEL_VARIANT_SCORPIONS_AND_MIND_SUCKLERS] = {
        .placements = _gMineForkedTunnelScorpionAndMindSucklerPlacements,
        .resources  = _gMineForkedTunnelScorpionAndMindSucklerResources,
    },
    [MINE_FORKED_TUNNEL_VARIANT_BATS] = {
        .placements = _gMineForkedTunnelBatPlacements,
        .resources  = _gMineForkedTunnelBatResources,
    },
};

/// Minimum ambient colours for the tunnel's seven logical views, preceded by their count.
///
/// Entries 1..7 are RGB and stored weighted brightness, at 16 units per colour
/// level. Lighting clamps model ambient RGB to these minima; it does not use
/// the brightness component. The table stays live with the room overlay.
static WorldCoordRoomAmbientEntry _gMineForkedTunnelViewAmbientTable[8] = {
    { .viewCount = ARRAY_SIZE(_gMineForkedTunnelViewAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 480, 480, 360, 465 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 430, 440, 350, 425 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

// Cue 2 selects each base; cue 1 selects the next entry in the loaded type-1 bank.
enum {
    MINE_FORKED_TUNNEL_FOOTSTEP_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1D,
    MINE_FORKED_TUNNEL_FOOTSTEP_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x1F,
};

/// Paired type-1 footstep requests for tunnel surface class 1.
///
/// Walking and scripted jumps share a cue base; running uses another pair.
/// The surface record borrows these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gMineForkedTunnelSurfaceClass1FootstepSounds = {
    .walk         = MINE_FORKED_TUNNEL_FOOTSTEP_WALK_SOUND,
    .run          = MINE_FORKED_TUNNEL_FOOTSTEP_RUN_SOUND,
    .scriptedJump = MINE_FORKED_TUNNEL_FOOTSTEP_WALK_SOUND,
};

/// Shared blocking collision policy for tunnel surface classes 0 and 2..7, without footstep cues.
///
/// Allows weapon impacts and applies pushback. The class directory borrows this
/// scalar record while the room is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineForkedTunnelBlockingSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking collision policy and paired footstep cues for tunnel surface class 1.
///
/// Allows weapon impacts and applies pushback. The class directory borrows this
/// scalar record while the room is loaded; the first byte's role is unproven.
static WorldCollisionSurfaceProperties _gMineForkedTunnelSurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gMineForkedTunnelSurfaceClass1FootstepSounds,
};

WorldCollisionSurfaceProperties* gMineForkedTunnelSurfacePropertiesByClass[8] = {
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
    &_gMineForkedTunnelSurfaceClass1Properties,
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
    &_gMineForkedTunnelBlockingSilentSurfaceProperties,
};

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Initializes the switch-controlled area object and its optional tilting child.
///
/// Requires a live TMD body and the area object's enemy allocation in
/// `spawnArg2`. Owns one zeroed work block; teardown releases it and the child.
/// Uses the path start while the switch flag is clear, otherwise the parked
/// placement. Allocation failure starts teardown; child-spawn failure is tolerated.
static void _mineForkedTunnelInitAreaObject(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_AREA_OBJECT_BUFFER_RELEASE_IDLE = -1,
        MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_DESCRIPTOR    = 1,
    };
    _MineForkedTunnelAreaObjectWork* work;
    ActorTransform                   placement;

    work = memCalloc(sizeof(_MineForkedTunnelAreaObjectWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->freeCountdown = MINE_FORKED_TUNNEL_AREA_OBJECT_BUFFER_RELEASE_IDLE;

    if (gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED) == 0) {
        placement.pos.vx = _gMineForkedTunnelAreaObjectPositionPath[0].vx;
        placement.pos.vy = _gMineForkedTunnelAreaObjectPositionPath[0].vy;
        placement.pos.vz = _gMineForkedTunnelAreaObjectPositionPath[0].vz;
        placement.rot.vx = _gMineForkedTunnelAreaObjectRotationPath[0].vx;
        placement.rot.vy = _gMineForkedTunnelAreaObjectRotationPath[0].vy;
        placement.rot.vz = _gMineForkedTunnelAreaObjectRotationPath[0].vz;
        actorMsgPlaceEulerZyx(task, ACTOR_MESSAGE_PLACE, &placement, 0);
    } else {
        actorMsgPlaceEulerZyx(task, ACTOR_MESSAGE_PLACE, &_gMineForkedTunnelAreaObjectParkedPlacement, 0);
    }

    _mineForkedTunnelSetAreaObjectDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_AUTO, 0);
    _mineForkedTunnelBindAreaObjectLighting(task);
    // The child borrows the parent's coordinate and work-owned lighting matrices.
    work->child    = taskSpawnFromTable(_gMineForkedTunnelAreaObjectTaskDescs, MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_DESCRIPTOR, 0, task);
    task->msgTable = _gMineForkedTunnelAreaObjectMessages;
    _mineForkedTunnelUpdateAreaObjectCollision(gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED));
    task->exitCallback = _mineForkedTunnelExitAreaObject;
    task->state++;
}

/// Advances the area object's path, shadow, lighting and deferred buffer release.
///
/// Requires initialized work and a live TMD body. `spawnArg1.value` gates motion;
/// `killCountdown` is a nonnegative path index, advancing through entries 0..109
/// and holding at 110. The full authored position and rotation tables are longer.
/// Hidden drawing still permits motion and buffer release. An armed release
/// frees on zero and decrements to -1, leaving subsequent updates idle.
static void _mineForkedTunnelTickAreaObject(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_AREA_OBJECT_PATH_STEPS       = 110,
        MINE_FORKED_TUNNEL_AREA_OBJECT_SHADOW_HALF_SIZE = 512,
        MINE_FORKED_TUNNEL_AREA_OBJECT_LIGHT_COUNT      = 3,
    };
    TmdObject*     model;
    ActorTransform placement;
    VECTOR3        groundPoint;

    model = task->extra.tmd;

    // The signed task counter is a path cursor until task teardown takes it over.
    if (task->spawnArg1.value == MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING && task->killCountdown < MINE_FORKED_TUNNEL_AREA_OBJECT_PATH_STEPS) {
        placement.pos.vx = _gMineForkedTunnelAreaObjectPositionPath[task->killCountdown].vx;
        placement.pos.vy = _gMineForkedTunnelAreaObjectPositionPath[task->killCountdown].vy;
        placement.pos.vz = _gMineForkedTunnelAreaObjectPositionPath[task->killCountdown].vz;
        placement.rot.vx = _gMineForkedTunnelAreaObjectRotationPath[task->killCountdown].vx;
        placement.rot.vy = _gMineForkedTunnelAreaObjectRotationPath[task->killCountdown].vy;
        placement.rot.vz = _gMineForkedTunnelAreaObjectRotationPath[task->killCountdown].vz;

        actorMsgPlaceEulerZyx(task, ACTOR_MESSAGE_PLACE, &placement, 0);
        task->killCountdown++;
    }

    // The shadow samples the cached world transform before lighting composes it.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords->workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, MINE_FORKED_TUNNEL_AREA_OBJECT_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        actorRenderComposeCoord(task->extra.tmd->coords);
        worldCoordSetModelLighting(model, task->extra.tmd->coords->workm.t, 0, MINE_FORKED_TUNNEL_AREA_OBJECT_LIGHT_COUNT);
    }

    if (((_MineForkedTunnelAreaObjectWork*)task->work)->freeCountdown >= 0) {
        if (((_MineForkedTunnelAreaObjectWork*)task->work)->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        ((_MineForkedTunnelAreaObjectWork*)task->work)->freeCountdown--;
    }
}

/// Sets an area-object coordinate's translation and ZYX orientation from a placement.
///
/// Both pointers must refer to live, disjoint storage for the call. Coordinates
/// use the destination's parent frame and angles use 4096 units per turn.
/// Borrows the placement's XYZ and Euler components, ignoring fourth components.
/// Replaces any scale or shear with a rotation at unit scale (4096), stores the
/// Euler angles and marks composition dirty; the parent and cached matrix remain.
static inline void _mineForkedTunnelApplyAreaObjectPlacement(GfxCoord* coord, const ActorTransform* placement)
{
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies a script command to the switch-controlled area object.
///
/// Borrows `command` during dispatch and requires an initialized area-object task.
/// Command 0 stops and rewinds both motion cursors and places the parent at the
/// path start; 1 starts the child's tilt if present; 2 starts the parent's path;
/// 3 parks and stops the parent and refreshes collision from the current switch
/// flag, leaving both cursors and the child unchanged. The command's location,
/// message ID and final payload are ignored. All commands, including unknown
/// values with no effect, return zero.
static s32 _mineForkedTunnelApplyAreaObjectCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused)
{
    ActorTransform                   placement;
    const ActorTransform*            parkedPlacement;
    GfxCoord*                        coord;
    _MineForkedTunnelAreaObjectWork* work;

    switch (command->command) {
        case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_RESET:
            work                  = task->work;
            task->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE;
            task->killCountdown   = 0;
            if (work->child != NULL) {
                work->child->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE;
                work->child->killCountdown   = 0;
            }
            placement.pos.vx = _gMineForkedTunnelAreaObjectPositionPath[0].vx;
            placement.pos.vy = _gMineForkedTunnelAreaObjectPositionPath[0].vy;
            placement.pos.vz = _gMineForkedTunnelAreaObjectPositionPath[0].vz;
            placement.rot.vx = _gMineForkedTunnelAreaObjectRotationPath[0].vx;
            placement.rot.vy = _gMineForkedTunnelAreaObjectRotationPath[0].vy;
            placement.rot.vz = _gMineForkedTunnelAreaObjectRotationPath[0].vz;

            coord = task->extra.tmd->coords;
            _mineForkedTunnelApplyAreaObjectPlacement(coord, &placement);
            break;
        case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_TILT_CHILD:
            work = task->work;
            if (work->child != NULL) {
                work->child->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING;
            }
            break;
        case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_START_PATH:
            task->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING;
            break;
            // Retained block shape preserves the target's placement loads.
            do {
                case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_PARK:
                    parkedPlacement = &_gMineForkedTunnelAreaObjectParkedPlacement;
                    coord           = task->extra.tmd->coords;
                    _mineForkedTunnelApplyAreaObjectPlacement(coord, parkedPlacement);

                    _mineForkedTunnelUpdateAreaObjectCollision(gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED));
                    task->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE;
                    break;
            } while (0);
    }
    return 0;
}

/// Advances the child's tilt while keeping its translation fixed in the parent frame.
///
/// Requires an attached TMD child and a nonnegative `killCountdown` cursor.
/// Motion runs only for `spawnArg1.value` 1, applies the 54 authored rotations
/// in order (4096 angle units per turn), and holds once the cursor reaches 54.
static void _mineForkedTunnelTiltAreaObjectChild(Task* task)
{
    ActorTransform placement;
    GfxCoord*      coord;

    if (task->spawnArg1.value == MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING && task->killCountdown < ARRAY_SIZE(_gMineForkedTunnelAreaObjectChildRotationPath)) {
        placement.pos.vx = _gMineForkedTunnelAreaObjectChildPlacement.pos.vx;
        placement.pos.vy = _gMineForkedTunnelAreaObjectChildPlacement.pos.vy;
        placement.pos.vz = _gMineForkedTunnelAreaObjectChildPlacement.pos.vz;
        placement.rot.vx = _gMineForkedTunnelAreaObjectChildRotationPath[task->killCountdown].vx;
        placement.rot.vy = _gMineForkedTunnelAreaObjectChildRotationPath[task->killCountdown].vy;
        placement.rot.vz = _gMineForkedTunnelAreaObjectChildRotationPath[task->killCountdown].vz;

        coord = task->extra.tmd->coords;
        _mineForkedTunnelApplyAreaObjectPlacement(coord, &placement);

        task->killCountdown++;
    }
}

void mineForkedTunnelAreaObjectTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = _gMineForkedTunnelAreaObjectStateHandlers;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Releases the area object's enemy record and tears down its task and child model.
///
/// Requires a live enemy allocation in `task->spawnArg2`; task teardown owns
/// the work matrices and model body. The task may be freed before return.
static void _mineForkedTunnelExitAreaObject(Task* task)
{
    enemyTaskExit(task);
}

/// Binds the area object's model to its work-owned light and colour matrices.
///
/// Requires a TMD body and initialized area-object work. The model and its
/// child borrow the matrices until task teardown; this does not fill them.
static void _mineForkedTunnelBindAreaObjectLighting(Task* task)
{
    TmdObject*                       model;
    _MineForkedTunnelAreaObjectWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
}

#include "../../shared/actor/actor_messages_place_euler_zyx.inc.c"

/// Sets the area object's active drawing and primitive-buffer policy.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with a signed mode word: 0 hides
/// with automatic buffer recovery, 1 shows and allocates missing buffers,
/// 2 hides and schedules buffer release, and 3 shows with automatic recovery
/// disabled. Mode 2 requires area-object work and releases on the third
/// subsequent object update, after two countdown decrements. Other modes do
/// not cancel an armed release. Returns 0 for modes 0..3, or 1 without changes
/// for any other mode. The message ID and final payload are ignored.
static s32 _mineForkedTunnelSetAreaObjectDrawMode(Task* task, s32 messageId, s32 drawMode, s32 unused)
{
    TmdObject* model;
    s32        result;

    model  = task->extra.tmd;
    result = 0;
    switch (drawMode) {
        case MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_AUTO:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_AUTO:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_RELEASE: {
            _MineForkedTunnelAreaObjectWork* work;

            // Keep hidden buffers alive for two decrements before releasing them.
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        }
        case MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_MANUAL:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Runs the area-object child's attach, tilt or teardown state while actors run.
///
/// Requires `state` in 0..2 and a live TMD body before teardown. State 0 borrows
/// the parent task passed in `spawnArg2.pointer`; later updates use its attached
/// coordinate and lighting. The owning room overlay must remain loaded.
static void _mineForkedTunnelAreaObjectChildTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = _gMineForkedTunnelAreaObjectChildStateHandlers;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Attaches the tilting child to its area object and sets its local placement.
///
/// `spawnArg2.pointer` must supply a live parent task; both tasks need TMD bodies.
/// The child borrows the parent's coordinate and work-owned lighting matrices
/// until parent teardown. It becomes visible with an ordering-table offset of
/// -1, adopts the fixed parent-relative placement, and advances to the tilt state.
static void _mineForkedTunnelAttachAreaObjectChild(Task* task)
{
    enum { MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_OT_OFFSET = -1 };

    Task*      parent;
    TmdObject* model;
    TmdObject* parentModel;
    GfxCoord*  parentCoord;
    GfxCoord*  coord;
    GfxCoord*  placementCoord;

    parent      = task->spawnArg2.pointer;
    model       = task->extra.tmd;
    parentModel = parent->extra.tmd;
    coord       = model->coords;
    parentCoord = parentModel->coords;

    // Share the model frame and lighting before joining the parent's teardown tree.
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = parentCoord;
    model->lightMtx     = parentModel->lightMtx;
    model->colorMtx     = parentModel->colorMtx;
    model->otOffset     = MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_OT_OFFSET;
    taskReparent(parent, task);
    model->flags = model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;

    placementCoord = task->extra.tmd->coords;
    _mineForkedTunnelApplyAreaObjectPlacement(placementCoord, &_gMineForkedTunnelAreaObjectChildPlacement);
    task->state = task->state + 1;
}

/// Rebuilds the area object's collision box in the reserved prefix of the room grid.
///
/// Copies three normals and faces and eight vertices from the box template;
/// the rest of the room grid and the vectors' fourth words remain intact.
/// Offsets vertices by (0, 0, -200) world units while `switchUsed` is zero,
/// or (0, -3000, -200) for any nonzero value, moving the box above the passage.
/// Both grid pools must be live, writable on the room side, and disjoint.
static void _mineForkedTunnelUpdateAreaObjectCollision(s32 switchUsed)
{
    enum {
        MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_Z_OFFSET = -200,
        MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CLEAR_Y  = -3000,
    };
    WorldCollisionGrid*       roomGrid;
    const WorldCollisionGrid* templateGrid;
    SVECTOR                   offset;
    s32                       entryIndex;

    roomGrid     = &_gMineForkedTunnelCollisionGrid;
    templateGrid = &_gMineForkedTunnelAreaObjectCollisionBox;

    // The room's cell lists already reserve these entries for the movable box.
    MINE_FORKED_TUNNEL_COPY_AREA_OBJECT_COLLISION_BOX(roomGrid, templateGrid, entryIndex);

    if (switchUsed == 0) {
        offset.vx = 0;
        offset.vy = 0;
        offset.vz = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_Z_OFFSET;
    } else {
        offset.vy = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CLEAR_Y;
        offset.vx = 0;
        offset.vz = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_Z_OFFSET;
    }

    for (entryIndex = 0; entryIndex < MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT; entryIndex++) {
        roomGrid->vertices[entryIndex].vx += offset.vx;
        roomGrid->vertices[entryIndex].vy += offset.vy;
        roomGrid->vertices[entryIndex].vz += offset.vz;
    }
}

/// Rejects every key-item-use request with zero, leaving the item and room unchanged.
///
/// `itemId` is the inventory's collected item ID; all four arguments are ignored.
static s32 _mineForkedTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a tunnel room transition and resolves its Mine/Shelter destination variant.
///
/// The stage map overlay must be loaded. Borrows live eight-byte request and
/// writable reply records for synchronous dispatch; they may be the same object.
/// Copies the complete request before resolving the room from game progress.
/// Queries keep the copied destination unchanged. Retains neither pointer and
/// returns 1; the receiving task and message ID are ignored.
static s32 _mineForkedTunnelResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { MINE_FORKED_TUNNEL_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return MINE_FORKED_TUNNEL_TRANSITION_ALLOWED;
}

/// Chooses the oak-board response to CAP room command 2 while the board is present.
///
/// Saved object slot 1 must have state 1. A clear progress flag selects CAP 5;
/// a nonzero flag starts the board pickup prompt. Other inputs do nothing.
/// Requires current saved-area and room CAP data; always returns zero and
/// retains no payload.
static s32 _mineForkedTunnelBoardCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        MINE_FORKED_TUNNEL_COMMAND_EXAMINE_BOARD      = 2,
        MINE_FORKED_TUNNEL_BOARD_OBJECT_ID            = 1,
        MINE_FORKED_TUNNEL_BOARD_PRESENT              = 1,
        MINE_FORKED_TUNNEL_BOARD_FALLBACK_CAP_COMMAND = 5,
        MINE_FORKED_TUNNEL_BOARD_PROMPT_DESCRIPTOR    = 1,
    };

    if ((commandId == MINE_FORKED_TUNNEL_COMMAND_EXAMINE_BOARD) &&
        (areaGetCurrentObjectState(MINE_FORKED_TUNNEL_BOARD_OBJECT_ID) == MINE_FORKED_TUNNEL_BOARD_PRESENT)) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_152) == 0) {
            capRunCommandWithTransition(MINE_FORKED_TUNNEL_BOARD_FALLBACK_CAP_COMMAND);
        } else {
            taskSpawnFromTable(_gMineForkedTunnelInteractionTaskDescriptors, MINE_FORKED_TUNNEL_BOARD_PROMPT_DESCRIPTOR, 0, 0);
        }
    }
    return 0;
}

/// Starts the switch interaction when requested and the switch has not been used.
///
/// Borrows a live `DirectionActionRequest` during synchronous room dispatch.
/// Action ID 1 spawns the switch-interaction task while the switch flag is zero;
/// other IDs and an already-used switch have no effect. Ignores control,
/// argument, receiving task, message ID and final payload. Returns zero even
/// when spawning fails; the room task does not retain the request.
static s32 _mineForkedTunnelHandleSwitchAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unused)
{
    enum {
        MINE_FORKED_TUNNEL_ACTION_USE_SWITCH             = 1,
        MINE_FORKED_TUNNEL_SWITCH_INTERACTION_DESCRIPTOR = 0,
    };

    if ((actionRequest->actionId == MINE_FORKED_TUNNEL_ACTION_USE_SWITCH) && (gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED) == 0)) {
        taskSpawnFromTable(_gMineForkedTunnelInteractionTaskDescriptors, MINE_FORKED_TUNNEL_SWITCH_INTERACTION_DESCRIPTOR, 0, 0);
    }
    return 0;
}

/// Registers the tunnel room task and initializes countdown music and oak-board visibility.
///
/// Enter at state 0 with the Mine/Shelter map and room resources loaded. Installs
/// the room message table, publishes `GAME_TASK_SLOT_ROOM`, selects countdown-music
/// entry 1 and hides the board sprites if its saved placement state is collected.
/// Advances to the idle state. The registered task and room overlay must remain
/// live while receiving room messages.
static void _mineForkedTunnelInitRoomTask(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_COUNTDOWN_MUSIC_ENTRY = 1,
        MINE_FORKED_TUNNEL_OAK_BOARD_OBJECT_ID   = 1,
        MINE_FORKED_TUNNEL_OAK_BOARD_COLLECTED   = 2,
    };

    task->msgTable = _gMineForkedTunnelRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    gStageCountdownMusicEntry = MINE_FORKED_TUNNEL_COUNTDOWN_MUSIC_ENTRY;
    _mineForkedTunnelSetSpriteBatchesHidden(areaGetCurrentObjectState(MINE_FORKED_TUNNEL_OAK_BOARD_OBJECT_ID) == MINE_FORKED_TUNNEL_OAK_BOARD_COLLECTED);
    task->state = task->state + 1;
}

/// Keeps the initialized room task available to receive messages without per-frame work.
static void _mineForkedTunnelIdleRoomTask(Task* unusedTask)
{
    // Preserve the idle callback's otherwise unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

void mineForkedTunnelRoomTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = _gMineForkedTunnelRoomTaskStates;

    stateHandlers.funcs[task->state](task);
}

/// Requests centred, unattenuated playback for the area object's movement sound.
///
/// The event script has already started the sound with attenuation 32. This
/// queued mix request changes an existing matching slot; it starts no sound.
static void _mineForkedTunnelRestoreAreaObjectSoundMix(void)
{
    sndEvtRequestScriptMix(SOUND_MINE_FORKED_TUNNEL_OBJECT_MOVE, 0, 0);
}

/// Runs the switch prompt and starts the switch-motion scene after acceptance.
///
/// Enter in state 0 with the room CAP and event scripts loaded. State 1 waits
/// for CAP completion; reply key 1 starts the skippable scene and sets the
/// switch-used flag immediately. Every completed reply releases the task.
static void _mineForkedTunnelSwitchInteractionTask(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_SWITCH_PROMPT_START = 0,
        MINE_FORKED_TUNNEL_SWITCH_PROMPT_WAIT  = 1,
        MINE_FORKED_TUNNEL_SWITCH_CAP_COMMAND  = 1,
        MINE_FORKED_TUNNEL_SWITCH_USED         = 1,
    };
    s32 promptState;

    promptState = task->state;
    switch (promptState) {
        case MINE_FORKED_TUNNEL_SWITCH_PROMPT_START:
            capRunCommandWithTransition(MINE_FORKED_TUNNEL_SWITCH_CAP_COMMAND);
            task->state = task->state + 1;
            break;
        case MINE_FORKED_TUNNEL_SWITCH_PROMPT_WAIT:
            if (capIsBusy() == 0) {
                // The wait state and accepted CAP key both have value 1.
                if (capGetVariantKey() == promptState) {
                    evsStartScriptWithSkip(_gMineForkedTunnelSwitchSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gMineForkedTunnelSwitchSceneSkipScript);
                    gameFlagSetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED, MINE_FORKED_TUNNEL_SWITCH_USED);
                }
                taskKill(task);
            }
            break;
    }
}

/// Runs the oak-board pickup prompt and hides its room sprites after acceptance.
///
/// Starts at state 0 with a zeroed signed-halfword `killCountdown`. Holds player
/// control through CAP playback and eleven subsequent callback ticks. Reply key
/// 2 starts the acknowledgement and hides the board; every reply resumes control
/// and releases the task. Requires loaded room CAP and sprite resources.
static void _mineForkedTunnelOakBoardPromptTask(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_BOARD_PROMPT_START        = 0,
        MINE_FORKED_TUNNEL_BOARD_PROMPT_WAIT         = 1,
        MINE_FORKED_TUNNEL_BOARD_PROMPT_REPLY        = 2,
        MINE_FORKED_TUNNEL_BOARD_CAP_COMMAND         = 2,
        MINE_FORKED_TUNNEL_BOARD_PROMPT_KEY          = 0,
        MINE_FORKED_TUNNEL_BOARD_ACKNOWLEDGEMENT_KEY = 1,
        MINE_FORKED_TUNNEL_BOARD_REPLY_DELAY_TICKS   = 11,
    };
    s32 state;
    s16 replyDelayTicks;

    state = task->state;
    switch (state) {
        case MINE_FORKED_TUNNEL_BOARD_PROMPT_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capStartSequenceSlot(MINE_FORKED_TUNNEL_BOARD_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE, MINE_FORKED_TUNNEL_BOARD_PROMPT_KEY);
            task->state = task->state + 1;
            break;
        case MINE_FORKED_TUNNEL_BOARD_PROMPT_WAIT:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case MINE_FORKED_TUNNEL_BOARD_PROMPT_REPLY:
            replyDelayTicks     = task->killCountdown + 1;
            task->killCountdown = replyDelayTicks;
            if (replyDelayTicks >= MINE_FORKED_TUNNEL_BOARD_REPLY_DELAY_TICKS) {
                // The reply phase and the accepted CAP key both have value 2.
                if (capGetVariantKey() == state) {
                    capStartSequenceSlot(MINE_FORKED_TUNNEL_BOARD_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE, MINE_FORKED_TUNNEL_BOARD_ACKNOWLEDGEMENT_KEY);
                    _mineForkedTunnelSetSpriteBatchesHidden(true);
                }
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
    }
}

/// Hides or shows the selected sprite batches in mapped views 4 and 5.
///
/// Requires this room's stage/area sprite directory to be loaded. A zero byte
/// shows both ranges; any other byte hides them, normalized to 1. View 4's
/// batch 5 covers source sprites 80..81; view 5's batch 3 covers 20..25.
static void _mineForkedTunnelSetSpriteBatchesHidden(u8 hidden)
{
    const GameLocationKey* location;
    const SpriteView*      areaViews;
    SpriteBatch*           view4Batches;
    SpriteBatch*           view5Batches;

    location  = &gGameSession->location;
    areaViews = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];

    if (!hidden) {
        view4Batches           = areaViews[3].batches;
        view4Batches[5].hidden = false;
        view5Batches           = areaViews[4].batches;
        view5Batches[3].hidden = false;
        return;
    }
    view4Batches           = areaViews[3].batches;
    view4Batches[5].hidden = true;
    view5Batches           = areaViews[4].batches;
    view5Batches[3].hidden = true;
}

#include "../../shared/effects/glow_draw_flare.inc.c"

void mineForkedTunnelDrawViewFlaresTask(Task* unusedTask)
{
    enum {
        MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX = 1,
        MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE  = 0x300,
    };

    s32 mappedViewIndex;

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    mappedViewIndex                  = viewGetMappedIndex() & 0xFF;

    switch (mappedViewIndex) {
        case 2:
        case 3:
            _glowDrawFlare(&_gMineForkedTunnelView2And3FlarePoint, MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            break;
        case 4:
            _glowDrawFlare(&_gMineForkedTunnelView4FlarePoints[0], MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&_gMineForkedTunnelView4FlarePoints[1], MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            break;
        case 5:
            _glowDrawFlare(&_gMineForkedTunnelView5FlarePoint, MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            break;
        default:
            return;
    }
}
