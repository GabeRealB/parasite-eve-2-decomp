#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_r49.h"
/// Selects the first walker's writable signed-halfword approach mode.
///
/// Only element zero is a mode; the remaining halfword has an unproven role.
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
// The scripted walk's update and walk-to handler run on the first walker's
// block; the second walker's copies of the two rebind the type to its own.
#define SCRIPTED_WALK_WORK_T _Actor143900Work
#include "../../shared/scripted_walk.h"

/// Draw-request bits shared by this package's two walker message handlers.
enum {
    ACTOR_143900_WALKER_DRAW_SHOW             = 1,
    ACTOR_143900_WALKER_DRAW_SKIP_AUTO_BUFFER = 2,
};

/// Results of either walker's play-animation request, without validating the loaded-clip domain.
enum {
    ACTOR_143900_PLAY_ANIMATION_APPLIED  = 0,
    ACTOR_143900_PLAY_ANIMATION_REJECTED = -1,
};

/// Both walkers start on clip 1 and sample three lights 800 coordinate units above the root.
enum {
    ACTOR_143900_WALKER_INITIAL_CLIP   = 1,
    ACTOR_143900_WALKER_LIGHT_Y_OFFSET = 800,
    ACTOR_143900_WALKER_LIGHT_COUNT    = 3,
};

static s16 _gScriptedWalkModeStorage[2];

/// Signed-halfword approach mode at the start of the first walker's storage.
///
/// Values are `SCRIPTED_WALK_MODE_*`. The scalar view retains the access
/// shape required by the walk-to handler; the trailing halfword is not read.
extern s16 gScriptedWalkModeValue __asm__("_gScriptedWalkModeStorage");

/// Work block of the package's first walker, allocated zeroed by the walker's
/// spawn state and kept both at `Task::work` and in `_gScriptedWalkWork`.
///
/// It is the block of a scripted walker that carries nothing: the two light
/// matrices, the rig, the animation request and the turn countdown, which is
/// where `ScriptedWalkAttachmentsWork`, the block of the package's second
/// walker, goes on to its two attachment tasks.
typedef struct {
    MATRIX          light;      // Light-direction matrix lent to the model object
    MATRIX          color;      // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;        // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;         // Animation request, heading last given the root and frames of walk left
    s16             turnFrames; // Frames the update still turns the model for while the turn clip plays; command 0 starts 20
} _Actor143900Work;
STATIC_ASSERT_SIZEOF(_Actor143900Work, 0x4F0);

static _Actor143900Work* _gScriptedWalkWork;

/// The first variant's task, published by its spawn routine so the
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496BC;

/// The first variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_143900_801413BC[];

/// Animation stream the first variant's spawn routine binds into its work
/// block's animation context with `animationInitContext`.
extern u8 D_actor_143900_801413F8[];

static ScriptedWalkAttachmentsWork* _gScriptedWalkSecondWork;

/// The second variant's task, published by its spawn routine so the placement,
/// visibility and play-animation handlers can reach it.
extern Task* D_actor_143900_801496C8;

/// Approach mode of the second walker, stored as `SCRIPTED_WALK_MODE_*`.
///
/// The walk-to message narrows its argument to this signed halfword. Its
/// independent value selects a 60-unit forward, 15-unit backward or 25-unit
/// forward step until the next approach message.
static s16 _gScriptedWalkSecondMode;

/// The second variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
extern TaskMessageEntry D_actor_143900_80149634[];

/// Spawn table the second variant's spawn routine starts its two attachment tasks
/// from, indices 1 and 2; the tasks are parked in `attachment1` / `attachment2`.
extern TaskDesc D_actor_143900_80149664[];

/// Animation stream the second variant's spawn routine binds into its work
/// block's animation context with `animationInitContext`.
extern u8 D_actor_143900_80149688[];

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor143900ExitScriptedWalker(Task* task);
static void _scriptedWalkUpdateSecond(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actor143900ExitSecondScriptedWalker(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);
static void _scriptedWalkTickSecondAnim(void);
static void _scriptedWalkResetSecondAnim(void);
static void _scriptedWalkBlendSecondAnim(void);

static TmdSource _gActor143900Body2;
static TmdSource _gActor143900Model173A8;
static TmdSource _gActor143900Model17688;
static s32       _actor143900PlaySecondScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32       _actor143900SetSecondScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32       _scriptedWalkPlaceSecond(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32       _actor143900SelectSecondScriptedWalkerAttachment(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static s32       _scriptedWalkToSecond(Task* task, s32 messageId, const VECTOR* target, s32 mode);
static void      _actor143900SecondScriptedWalkerTask(Task* task);
static void      _actor143900ScriptedWalkerAttachmentTask(Task* task);
static void      _actor143900SpawnSecondScriptedWalker(Enemy* enemy, Task* task);

static s32  _actor143900PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor143900SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor143900ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static void _actor143900ScriptedWalkerTask(Task* task);
static void _actor143900SpawnScriptedWalker(Enemy* enemy, Task* task);

void func_actor_143900_80131E24(void);

void func_actor_143900_80131E24(void);

AnimationPlayRequest D_actor_143900_801334FC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133510 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133524 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_80133538 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_143900_8013354C = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_actor_143900_80133560[32] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_143900_801334FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_80133510 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_80133524 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_80133538 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_143900_8013354C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143900_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_143900_80133860[12] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_143900_80131E24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor143900Body1Skeleton[20] = {
#include "assets/actor_143900_body_1_skeleton.inc"
};

static u32 _gActor143900Body1PartVerts[20] = {
#include "assets/actor_143900_body_1_partVerts.inc"
};

static SVECTOR _gActor143900Body1Verts[342] = {
#include "assets/actor_143900_body_1_verts.inc"
};

static SVECTOR _gActor143900Body1Normals[442] = {
#include "assets/actor_143900_body_1_normals.inc"
};

static u32 _gActor143900Body1Stream[4280] = {
#include "assets/actor_143900_body_1_stream.inc"
};

static TmdSource _gActor143900Body1 = {
    0,
    21544,
    8516,
    20,
    _gActor143900Body1PartVerts,
    _gActor143900Body1Verts,
    _gActor143900Body1Normals,
    _gActor143900Body1Skeleton,
    _gActor143900Body1Stream,
};

static AnimationPackedPose _gActor143900Animation07C70Bank1[2] = {
#include "assets/actor_143900_animation_07C70_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation07C70Bank4[31] = {
#include "assets/actor_143900_animation_07C70_bank4.inc"
};

static AnimationRecord _gActor143900Animation07C70Records[108] = {
#include "assets/actor_143900_animation_07C70_records.inc"
};

static u16 _gActor143900Animation07C70Indices[20] = {
#include "assets/actor_143900_animation_07C70_indices.inc"
};

static AnimationSet _gActor143900Animation07C70 = {
    _gActor143900Animation07C70Records,
    _gActor143900Animation07C70Indices,
    { NULL, _gActor143900Animation07C70Bank1, NULL, NULL, _gActor143900Animation07C70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0800CBank1[2] = {
#include "assets/actor_143900_animation_0800C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0800CBank4[56] = {
#include "assets/actor_143900_animation_0800C_bank4.inc"
};

static AnimationRecord _gActor143900Animation0800CRecords[149] = {
#include "assets/actor_143900_animation_0800C_records.inc"
};

static u16 _gActor143900Animation0800CIndices[20] = {
#include "assets/actor_143900_animation_0800C_indices.inc"
};

static AnimationSet _gActor143900Animation0800C = {
    _gActor143900Animation0800CRecords,
    _gActor143900Animation0800CIndices,
    { NULL, _gActor143900Animation0800CBank1, NULL, NULL, _gActor143900Animation0800CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation08398Bank1[2] = {
#include "assets/actor_143900_animation_08398_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation08398Bank4[58] = {
#include "assets/actor_143900_animation_08398_bank4.inc"
};

static AnimationRecord _gActor143900Animation08398Records[143] = {
#include "assets/actor_143900_animation_08398_records.inc"
};

static u16 _gActor143900Animation08398Indices[20] = {
#include "assets/actor_143900_animation_08398_indices.inc"
};

static AnimationSet _gActor143900Animation08398 = {
    _gActor143900Animation08398Records,
    _gActor143900Animation08398Indices,
    { NULL, _gActor143900Animation08398Bank1, NULL, NULL, _gActor143900Animation08398Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation087A4Bank1[2] = {
#include "assets/actor_143900_animation_087A4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation087A4Bank4[59] = {
#include "assets/actor_143900_animation_087A4_bank4.inc"
};

static AnimationRecord _gActor143900Animation087A4Records[174] = {
#include "assets/actor_143900_animation_087A4_records.inc"
};

static u16 _gActor143900Animation087A4Indices[20] = {
#include "assets/actor_143900_animation_087A4_indices.inc"
};

static AnimationSet _gActor143900Animation087A4 = {
    _gActor143900Animation087A4Records,
    _gActor143900Animation087A4Indices,
    { NULL, _gActor143900Animation087A4Bank1, NULL, NULL, _gActor143900Animation087A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation09138Bank1[11] = {
#include "assets/actor_143900_animation_09138_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation09138Bank4[245] = {
#include "assets/actor_143900_animation_09138_bank4.inc"
};

static AnimationRecord _gActor143900Animation09138Records[315] = {
#include "assets/actor_143900_animation_09138_records.inc"
};

static u16 _gActor143900Animation09138Indices[20] = {
#include "assets/actor_143900_animation_09138_indices.inc"
};

static AnimationSet _gActor143900Animation09138 = {
    _gActor143900Animation09138Records,
    _gActor143900Animation09138Indices,
    { NULL, _gActor143900Animation09138Bank1, NULL, NULL, _gActor143900Animation09138Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation09364Bank1[2] = {
#include "assets/actor_143900_animation_09364_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation09364Bank4[33] = {
#include "assets/actor_143900_animation_09364_bank4.inc"
};

static AnimationRecord _gActor143900Animation09364Records[80] = {
#include "assets/actor_143900_animation_09364_records.inc"
};

static u16 _gActor143900Animation09364Indices[20] = {
#include "assets/actor_143900_animation_09364_indices.inc"
};

static AnimationSet _gActor143900Animation09364 = {
    _gActor143900Animation09364Records,
    _gActor143900Animation09364Indices,
    { NULL, _gActor143900Animation09364Bank1, NULL, NULL, _gActor143900Animation09364Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation099B4Bank1[5] = {
#include "assets/actor_143900_animation_099B4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation099B4Bank4[147] = {
#include "assets/actor_143900_animation_099B4_bank4.inc"
};

static AnimationRecord _gActor143900Animation099B4Records[222] = {
#include "assets/actor_143900_animation_099B4_records.inc"
};

static u16 _gActor143900Animation099B4Indices[20] = {
#include "assets/actor_143900_animation_099B4_indices.inc"
};

static AnimationSet _gActor143900Animation099B4 = {
    _gActor143900Animation099B4Records,
    _gActor143900Animation099B4Indices,
    { NULL, _gActor143900Animation099B4Bank1, NULL, NULL, _gActor143900Animation099B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A040Bank1[12] = {
#include "assets/actor_143900_animation_0A040_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A040Bank4[152] = {
#include "assets/actor_143900_animation_0A040_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A040Records[211] = {
#include "assets/actor_143900_animation_0A040_records.inc"
};

static u16 _gActor143900Animation0A040Indices[20] = {
#include "assets/actor_143900_animation_0A040_indices.inc"
};

static AnimationSet _gActor143900Animation0A040 = {
    _gActor143900Animation0A040Records,
    _gActor143900Animation0A040Indices,
    { NULL, _gActor143900Animation0A040Bank1, NULL, NULL, _gActor143900Animation0A040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A21CBank1[2] = {
#include "assets/actor_143900_animation_0A21C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A21CBank4[25] = {
#include "assets/actor_143900_animation_0A21C_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A21CRecords[68] = {
#include "assets/actor_143900_animation_0A21C_records.inc"
};

static u16 _gActor143900Animation0A21CIndices[20] = {
#include "assets/actor_143900_animation_0A21C_indices.inc"
};

static AnimationSet _gActor143900Animation0A21C = {
    _gActor143900Animation0A21CRecords,
    _gActor143900Animation0A21CIndices,
    { NULL, _gActor143900Animation0A21CBank1, NULL, NULL, _gActor143900Animation0A21CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A738Bank1[2] = {
#include "assets/actor_143900_animation_0A738_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A738Bank4[123] = {
#include "assets/actor_143900_animation_0A738_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A738Records[178] = {
#include "assets/actor_143900_animation_0A738_records.inc"
};

static u16 _gActor143900Animation0A738Indices[20] = {
#include "assets/actor_143900_animation_0A738_indices.inc"
};

static AnimationSet _gActor143900Animation0A738 = {
    _gActor143900Animation0A738Records,
    _gActor143900Animation0A738Indices,
    { NULL, _gActor143900Animation0A738Bank1, NULL, NULL, _gActor143900Animation0A738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0A8D0Bank1[2] = {
#include "assets/actor_143900_animation_0A8D0_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0A8D0Bank4[16] = {
#include "assets/actor_143900_animation_0A8D0_bank4.inc"
};

static AnimationRecord _gActor143900Animation0A8D0Records[60] = {
#include "assets/actor_143900_animation_0A8D0_records.inc"
};

static u16 _gActor143900Animation0A8D0Indices[20] = {
#include "assets/actor_143900_animation_0A8D0_indices.inc"
};

static AnimationSet _gActor143900Animation0A8D0 = {
    _gActor143900Animation0A8D0Records,
    _gActor143900Animation0A8D0Indices,
    { NULL, _gActor143900Animation0A8D0Bank1, NULL, NULL, _gActor143900Animation0A8D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0AB6CBank1[2] = {
#include "assets/actor_143900_animation_0AB6C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0AB6CBank4[40] = {
#include "assets/actor_143900_animation_0AB6C_bank4.inc"
};

static AnimationRecord _gActor143900Animation0AB6CRecords[101] = {
#include "assets/actor_143900_animation_0AB6C_records.inc"
};

static u16 _gActor143900Animation0AB6CIndices[20] = {
#include "assets/actor_143900_animation_0AB6C_indices.inc"
};

static AnimationSet _gActor143900Animation0AB6C = {
    _gActor143900Animation0AB6CRecords,
    _gActor143900Animation0AB6CIndices,
    { NULL, _gActor143900Animation0AB6CBank1, NULL, NULL, _gActor143900Animation0AB6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0B2B4Bank1[11] = {
#include "assets/actor_143900_animation_0B2B4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0B2B4Bank4[175] = {
#include "assets/actor_143900_animation_0B2B4_bank4.inc"
};

static AnimationRecord _gActor143900Animation0B2B4Records[238] = {
#include "assets/actor_143900_animation_0B2B4_records.inc"
};

static u16 _gActor143900Animation0B2B4Indices[20] = {
#include "assets/actor_143900_animation_0B2B4_indices.inc"
};

static AnimationSet _gActor143900Animation0B2B4 = {
    _gActor143900Animation0B2B4Records,
    _gActor143900Animation0B2B4Indices,
    { NULL, _gActor143900Animation0B2B4Bank1, NULL, NULL, _gActor143900Animation0B2B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0B4A0Bank1[2] = {
#include "assets/actor_143900_animation_0B4A0_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0B4A0Bank4[24] = {
#include "assets/actor_143900_animation_0B4A0_bank4.inc"
};

static AnimationRecord _gActor143900Animation0B4A0Records[73] = {
#include "assets/actor_143900_animation_0B4A0_records.inc"
};

static u16 _gActor143900Animation0B4A0Indices[20] = {
#include "assets/actor_143900_animation_0B4A0_indices.inc"
};

static AnimationSet _gActor143900Animation0B4A0 = {
    _gActor143900Animation0B4A0Records,
    _gActor143900Animation0B4A0Indices,
    { NULL, _gActor143900Animation0B4A0Bank1, NULL, NULL, _gActor143900Animation0B4A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0BC24Bank1[15] = {
#include "assets/actor_143900_animation_0BC24_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0BC24Bank4[177] = {
#include "assets/actor_143900_animation_0BC24_bank4.inc"
};

static AnimationRecord _gActor143900Animation0BC24Records[239] = {
#include "assets/actor_143900_animation_0BC24_records.inc"
};

static u16 _gActor143900Animation0BC24Indices[20] = {
#include "assets/actor_143900_animation_0BC24_indices.inc"
};

static AnimationSet _gActor143900Animation0BC24 = {
    _gActor143900Animation0BC24Records,
    _gActor143900Animation0BC24Indices,
    { NULL, _gActor143900Animation0BC24Bank1, NULL, NULL, _gActor143900Animation0BC24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0C718Bank1[12] = {
#include "assets/actor_143900_animation_0C718_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0C718Bank4[255] = {
#include "assets/actor_143900_animation_0C718_bank4.inc"
};

static AnimationRecord _gActor143900Animation0C718Records[390] = {
#include "assets/actor_143900_animation_0C718_records.inc"
};

static u16 _gActor143900Animation0C718Indices[20] = {
#include "assets/actor_143900_animation_0C718_indices.inc"
};

static AnimationSet _gActor143900Animation0C718 = {
    _gActor143900Animation0C718Records,
    _gActor143900Animation0C718Indices,
    { NULL, _gActor143900Animation0C718Bank1, NULL, NULL, _gActor143900Animation0C718Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0D1C4Bank1[14] = {
#include "assets/actor_143900_animation_0D1C4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0D1C4Bank4[263] = {
#include "assets/actor_143900_animation_0D1C4_bank4.inc"
};

static AnimationRecord _gActor143900Animation0D1C4Records[358] = {
#include "assets/actor_143900_animation_0D1C4_records.inc"
};

static u16 _gActor143900Animation0D1C4Indices[20] = {
#include "assets/actor_143900_animation_0D1C4_indices.inc"
};

static AnimationSet _gActor143900Animation0D1C4 = {
    _gActor143900Animation0D1C4Records,
    _gActor143900Animation0D1C4Indices,
    { NULL, _gActor143900Animation0D1C4Bank1, NULL, NULL, _gActor143900Animation0D1C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0ED80Bank1[103] = {
#include "assets/actor_143900_animation_0ED80_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0ED80Bank4[620] = {
#include "assets/actor_143900_animation_0ED80_bank4.inc"
};

static AnimationRecord _gActor143900Animation0ED80Records[826] = {
#include "assets/actor_143900_animation_0ED80_records.inc"
};

static u16 _gActor143900Animation0ED80Indices[20] = {
#include "assets/actor_143900_animation_0ED80_indices.inc"
};

static AnimationSet _gActor143900Animation0ED80 = {
    _gActor143900Animation0ED80Records,
    _gActor143900Animation0ED80Indices,
    { NULL, _gActor143900Animation0ED80Bank1, NULL, NULL, _gActor143900Animation0ED80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0F188Bank1[2] = {
#include "assets/actor_143900_animation_0F188_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0F188Bank4[93] = {
#include "assets/actor_143900_animation_0F188_bank4.inc"
};

static AnimationRecord _gActor143900Animation0F188Records[139] = {
#include "assets/actor_143900_animation_0F188_records.inc"
};

static u16 _gActor143900Animation0F188Indices[20] = {
#include "assets/actor_143900_animation_0F188_indices.inc"
};

static AnimationSet _gActor143900Animation0F188 = {
    _gActor143900Animation0F188Records,
    _gActor143900Animation0F188Indices,
    { NULL, _gActor143900Animation0F188Bank1, NULL, NULL, _gActor143900Animation0F188Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation0F570Bank1[2] = {
#include "assets/actor_143900_animation_0F570_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation0F570Bank4[89] = {
#include "assets/actor_143900_animation_0F570_bank4.inc"
};

static AnimationRecord _gActor143900Animation0F570Records[135] = {
#include "assets/actor_143900_animation_0F570_records.inc"
};

static u16 _gActor143900Animation0F570Indices[20] = {
#include "assets/actor_143900_animation_0F570_indices.inc"
};

static AnimationSet _gActor143900Animation0F570 = {
    _gActor143900Animation0F570Records,
    _gActor143900Animation0F570Indices,
    { NULL, _gActor143900Animation0F570Bank1, NULL, NULL, _gActor143900Animation0F570Bank4, NULL, NULL, NULL },
};

/// Latched duration of the first walker's next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword; walk completion replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`.
/// Plain resets leave it intact. Zero requests no transition time; 0..2047
/// keeps the playback timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry D_actor_143900_801413BC[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor143900PlayScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor143900SetScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, SCRIPTED_WALK_PLACE },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor143900ApplyScriptedWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, SCRIPTED_WALK_TO },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_143900_801413EC = { { { TASK_BODY_TMD, 192 } }, _actor143900ScriptedWalkerTask, { .model = &_gActor143900Body1 } };

u8 D_actor_143900_801413F8[80] = {
    0,
    0,
    0,
    0,
    144,
    154,
    19,
    128,
    44,
    158,
    19,
    128,
    184,
    161,
    19,
    128,
    196,
    165,
    19,
    128,
    88,
    175,
    19,
    128,
    132,
    177,
    19,
    128,
    212,
    183,
    19,
    128,
    96,
    190,
    19,
    128,
    240,
    198,
    19,
    128,
    140,
    201,
    19,
    128,
    212,
    208,
    19,
    128,
    192,
    210,
    19,
    128,
    68,
    218,
    19,
    128,
    56,
    229,
    19,
    128,
    228,
    239,
    19,
    128,
    160,
    11,
    20,
    128,
    60,
    192,
    19,
    128,
    88,
    197,
    19,
    128,
    168,
    15,
    20,
    128,
};

static TmdBone _gActor143900Body2Skeleton[20] = {
#include "assets/actor_143900_body_2_skeleton.inc"
};

static u32 _gActor143900Body2PartVerts[20] = {
#include "assets/actor_143900_body_2_partVerts.inc"
};

static SVECTOR _gActor143900Body2Verts[339] = {
#include "assets/actor_143900_body_2_verts.inc"
};

static SVECTOR _gActor143900Body2Normals[389] = {
#include "assets/actor_143900_body_2_normals.inc"
};

static u32 _gActor143900Body2Stream[4166] = {
#include "assets/actor_143900_body_2_stream.inc"
};

static TmdSource _gActor143900Body2 = {
    0,
    21036,
    8232,
    20,
    _gActor143900Body2PartVerts,
    _gActor143900Body2Verts,
    _gActor143900Body2Normals,
    _gActor143900Body2Skeleton,
    _gActor143900Body2Stream,
};

static AnimationPackedPose _gActor143900Animation15330Bank1[2] = {
#include "assets/actor_143900_animation_15330_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation15330Bank4[30] = {
#include "assets/actor_143900_animation_15330_bank4.inc"
};

static AnimationRecord _gActor143900Animation15330Records[77] = {
#include "assets/actor_143900_animation_15330_records.inc"
};

static u16 _gActor143900Animation15330Indices[20] = {
#include "assets/actor_143900_animation_15330_indices.inc"
};

static AnimationSet _gActor143900Animation15330 = {
    _gActor143900Animation15330Records,
    _gActor143900Animation15330Indices,
    { NULL, _gActor143900Animation15330Bank1, NULL, NULL, _gActor143900Animation15330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation157D4Bank1[2] = {
#include "assets/actor_143900_animation_157D4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation157D4Bank4[107] = {
#include "assets/actor_143900_animation_157D4_bank4.inc"
};

static AnimationRecord _gActor143900Animation157D4Records[164] = {
#include "assets/actor_143900_animation_157D4_records.inc"
};

static u16 _gActor143900Animation157D4Indices[20] = {
#include "assets/actor_143900_animation_157D4_indices.inc"
};

static AnimationSet _gActor143900Animation157D4 = {
    _gActor143900Animation157D4Records,
    _gActor143900Animation157D4Indices,
    { NULL, _gActor143900Animation157D4Bank1, NULL, NULL, _gActor143900Animation157D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation15B1CBank1[2] = {
#include "assets/actor_143900_animation_15B1C_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation15B1CBank4[41] = {
#include "assets/actor_143900_animation_15B1C_bank4.inc"
};

static AnimationRecord _gActor143900Animation15B1CRecords[143] = {
#include "assets/actor_143900_animation_15B1C_records.inc"
};

static u16 _gActor143900Animation15B1CIndices[20] = {
#include "assets/actor_143900_animation_15B1C_indices.inc"
};

static AnimationSet _gActor143900Animation15B1C = {
    _gActor143900Animation15B1CRecords,
    _gActor143900Animation15B1CIndices,
    { NULL, _gActor143900Animation15B1CBank1, NULL, NULL, _gActor143900Animation15B1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation15DECBank1[7] = {
#include "assets/actor_143900_animation_15DEC_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation15DECBank4[26] = {
#include "assets/actor_143900_animation_15DEC_bank4.inc"
};

static AnimationRecord _gActor143900Animation15DECRecords[113] = {
#include "assets/actor_143900_animation_15DEC_records.inc"
};

static u16 _gActor143900Animation15DECIndices[20] = {
#include "assets/actor_143900_animation_15DEC_indices.inc"
};

static AnimationSet _gActor143900Animation15DEC = {
    _gActor143900Animation15DECRecords,
    _gActor143900Animation15DECIndices,
    { NULL, _gActor143900Animation15DECBank1, NULL, NULL, _gActor143900Animation15DECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation162D4Bank1[2] = {
#include "assets/actor_143900_animation_162D4_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation162D4Bank4[120] = {
#include "assets/actor_143900_animation_162D4_bank4.inc"
};

static AnimationRecord _gActor143900Animation162D4Records[168] = {
#include "assets/actor_143900_animation_162D4_records.inc"
};

static u16 _gActor143900Animation162D4Indices[20] = {
#include "assets/actor_143900_animation_162D4_indices.inc"
};

static AnimationSet _gActor143900Animation162D4 = {
    _gActor143900Animation162D4Records,
    _gActor143900Animation162D4Indices,
    { NULL, _gActor143900Animation162D4Bank1, NULL, NULL, _gActor143900Animation162D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation16564Bank1[2] = {
#include "assets/actor_143900_animation_16564_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation16564Bank4[39] = {
#include "assets/actor_143900_animation_16564_bank4.inc"
};

static AnimationRecord _gActor143900Animation16564Records[99] = {
#include "assets/actor_143900_animation_16564_records.inc"
};

static u16 _gActor143900Animation16564Indices[20] = {
#include "assets/actor_143900_animation_16564_indices.inc"
};

static AnimationSet _gActor143900Animation16564 = {
    _gActor143900Animation16564Records,
    _gActor143900Animation16564Indices,
    { NULL, _gActor143900Animation16564Bank1, NULL, NULL, _gActor143900Animation16564Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation166ACBank1[2] = {
#include "assets/actor_143900_animation_166AC_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation166ACBank4[16] = {
#include "assets/actor_143900_animation_166AC_bank4.inc"
};

static AnimationRecord _gActor143900Animation166ACRecords[40] = {
#include "assets/actor_143900_animation_166AC_records.inc"
};

static u16 _gActor143900Animation166ACIndices[20] = {
#include "assets/actor_143900_animation_166AC_indices.inc"
};

static AnimationSet _gActor143900Animation166AC = {
    _gActor143900Animation166ACRecords,
    _gActor143900Animation166ACIndices,
    { NULL, _gActor143900Animation166ACBank1, NULL, NULL, _gActor143900Animation166ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation168ECBank1[2] = {
#include "assets/actor_143900_animation_168EC_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation168ECBank4[33] = {
#include "assets/actor_143900_animation_168EC_bank4.inc"
};

static AnimationRecord _gActor143900Animation168ECRecords[85] = {
#include "assets/actor_143900_animation_168EC_records.inc"
};

static u16 _gActor143900Animation168ECIndices[20] = {
#include "assets/actor_143900_animation_168EC_indices.inc"
};

static AnimationSet _gActor143900Animation168EC = {
    _gActor143900Animation168ECRecords,
    _gActor143900Animation168ECIndices,
    { NULL, _gActor143900Animation168ECBank1, NULL, NULL, _gActor143900Animation168ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation16E10Bank1[12] = {
#include "assets/actor_143900_animation_16E10_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation16E10Bank4[104] = {
#include "assets/actor_143900_animation_16E10_bank4.inc"
};

static AnimationRecord _gActor143900Animation16E10Records[169] = {
#include "assets/actor_143900_animation_16E10_records.inc"
};

static u16 _gActor143900Animation16E10Indices[20] = {
#include "assets/actor_143900_animation_16E10_indices.inc"
};

static AnimationSet _gActor143900Animation16E10 = {
    _gActor143900Animation16E10Records,
    _gActor143900Animation16E10Indices,
    { NULL, _gActor143900Animation16E10Bank1, NULL, NULL, _gActor143900Animation16E10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor143900Animation17280Bank1[9] = {
#include "assets/actor_143900_animation_17280_bank1.inc"
};

static AnimationPackedRotation _gActor143900Animation17280Bank4[95] = {
#include "assets/actor_143900_animation_17280_bank4.inc"
};

static AnimationRecord _gActor143900Animation17280Records[142] = {
#include "assets/actor_143900_animation_17280_records.inc"
};

static u16 _gActor143900Animation17280Indices[20] = {
#include "assets/actor_143900_animation_17280_indices.inc"
};

static AnimationSet _gActor143900Animation17280 = {
    _gActor143900Animation17280Records,
    _gActor143900Animation17280Indices,
    { NULL, _gActor143900Animation17280Bank1, NULL, NULL, _gActor143900Animation17280Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor143900Model173A8Skeleton[1] = {
#include "assets/actor_143900_model_173A8_skeleton.inc"
};

static u32 _gActor143900Model173A8PartVerts[1] = {
#include "assets/actor_143900_model_173A8_partVerts.inc"
};

static SVECTOR _gActor143900Model173A8Verts[18] = {
#include "assets/actor_143900_model_173A8_verts.inc"
};

static SVECTOR _gActor143900Model173A8Normals[9] = {
#include "assets/actor_143900_model_173A8_normals.inc"
};

static u32 _gActor143900Model173A8Stream[121] = {
#include "assets/actor_143900_model_173A8_stream.inc"
};

static TmdSource _gActor143900Model173A8 = {
    0,
    944,
    0,
    1,
    _gActor143900Model173A8PartVerts,
    _gActor143900Model173A8Verts,
    _gActor143900Model173A8Normals,
    _gActor143900Model173A8Skeleton,
    _gActor143900Model173A8Stream,
};

static TmdBone _gActor143900Model17688Skeleton[1] = {
#include "assets/actor_143900_model_17688_skeleton.inc"
};

static u32 _gActor143900Model17688PartVerts[1] = {
#include "assets/actor_143900_model_17688_partVerts.inc"
};

static SVECTOR _gActor143900Model17688Verts[14] = {
#include "assets/actor_143900_model_17688_verts.inc"
};

static SVECTOR _gActor143900Model17688Normals[8] = {
#include "assets/actor_143900_model_17688_normals.inc"
};

static u32 _gActor143900Model17688Stream[89] = {
#include "assets/actor_143900_model_17688_stream.inc"
};

static TmdSource _gActor143900Model17688 = {
    0,
    680,
    0,
    1,
    _gActor143900Model17688PartVerts,
    _gActor143900Model17688Verts,
    _gActor143900Model17688Normals,
    _gActor143900Model17688Skeleton,
    _gActor143900Model17688Stream,
};

/// Latched duration of the second walker's next child-part blend, in whole normal-rate frames.
///
/// Independent of the first walker's latch. Play requests narrow
/// `AnimationPlayRequest.blendFrames` to this signed halfword; walk completion
/// replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`. Plain resets leave it
/// intact. Zero requests no transition time; 0..2047 keeps the playback timer
/// nonnegative. The range is not checked.
static s16 _gScriptedWalkSecondBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry D_actor_143900_80149634[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor143900PlaySecondScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor143900SetSecondScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlaceSecond },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor143900SelectSecondScriptedWalkerAttachment },
    { ACTOR_MESSAGE_WALK_TO, _scriptedWalkToSecond },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_143900_80149664[3] = {
    { { { TASK_BODY_TMD, 192 } }, _actor143900SecondScriptedWalkerTask, { .model = &_gActor143900Body2 } },
    { { { TASK_BODY_TMD, 192 } }, _actor143900ScriptedWalkerAttachmentTask, { .model = &_gActor143900Model173A8 } },
    { { { TASK_BODY_TMD, 192 } }, _actor143900ScriptedWalkerAttachmentTask, { .model = &_gActor143900Model17688 } },
};

u8 D_actor_143900_80149688[48] = {
    0,
    0,
    0,
    0,
    60,
    121,
    20,
    128,
    12,
    124,
    20,
    128,
    244,
    128,
    20,
    128,
    132,
    131,
    20,
    128,
    204,
    132,
    20,
    128,
    12,
    135,
    20,
    128,
    48,
    140,
    20,
    128,
    160,
    144,
    20,
    128,
    80,
    113,
    20,
    128,
    244,
    117,
    20,
    128,
    0,
    0,
    0,
    0,
};

/// Borrowed work block of the first scripted walker.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// Animation and message handlers require it to remain live; task teardown
/// releases it without clearing this pointer.
static _Actor143900Work* _gScriptedWalkWork = NULL;

Task* D_actor_143900_801496BC = NULL;

/// Halfword storage containing the first walker's approach mode.
///
/// Element zero is the writable signed mode selected by the walk-to message
/// (`SCRIPTED_WALK_MODE_*`). Element one is never accessed; its role is
/// unproven. Keep both halfwords, including the second's original contents.
static s16 _gScriptedWalkModeStorage[2] = {
    0,
    0x49E7,
};

/// Borrowed work block of the second scripted walker and its two model attachments.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// The fragment binding selects this pointer while operating on the second
/// walker. Task teardown ends its lifetime without clearing the pointer.
static ScriptedWalkAttachmentsWork* _gScriptedWalkSecondWork = NULL;

Task* D_actor_143900_801496C8;

/// Arms `sceneEvent` and starts the room's spawn-table task, unless
/// `demoScene` is 9, so this story trigger is skipped while the attract demo
/// plays.
void func_actor_143900_80131E24(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x14;
        taskSpawnFromTable(D_shelter_r49_8017DA00, 0, 0, 0);
    }
}

/// Initializes the package's first scripted walker and starts its initial clip.
///
/// State 0 requires a live owned enemy and twenty-part TMD model. Allocates
/// zeroed primary-heap work, publishes it for the singleton handlers, and lends
/// its matrices and playback buffers to the model/rig until task teardown.
/// Allocation failure destroys the enemy and task without advancing state.
/// The root is parented to the view and made untargetable. Initial lighting
/// reads its cached translation with Y reduced by 800 coordinate units;
/// this does not compose the root or convert the sample to world space.
/// Requires loaded clips and the lighting/animation helpers' scratch/GTE state.
static void _actor143900SpawnScriptedWalker(Enemy* enemy, Task* task)
{
    enum { ACTOR_143900_WALKER_OT_OFFSET = 1 };
    VECTOR            lightSample;
    _Actor143900Work* walkerWork;
    TmdObject*        walkerModel;
    GfxCoord*         rootCoord;

    walkerModel = task->extra.tmd;
    rootCoord   = walkerModel->coords;
    // Publish the owned allocation before lending any of its storage.
    walkerWork         = memCalloc(sizeof(*walkerWork), false);
    _gScriptedWalkWork = walkerWork;
    task->work         = walkerWork;
    if (walkerWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor143900ExitScriptedWalker;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    walkerModel->otOffset            = ACTOR_143900_WALKER_OT_OFFSET;
    walkerModel->flags               = 0;
    walkerModel->lightMtx            = &_gScriptedWalkWork->light;
    walkerModel->colorMtx            = &_gScriptedWalkWork->color;
    lightSample.vx                   = rootCoord->workm.t[0];
    lightSample.vy                   = rootCoord->workm.t[1] - ACTOR_143900_WALKER_LIGHT_Y_OFFSET;
    D_actor_143900_801496BC          = task;
    lightSample.vz                   = rootCoord->workm.t[2];
    worldCoordSetModelLighting(walkerModel, &lightSample, 0, ACTOR_143900_WALKER_LIGHT_COUNT);
    // Seed the child tracks now; the next task state performs ordinary updates.
    animationInitContext(&_gScriptedWalkWork->rig.anim, (AnimationSet**)D_actor_143900_801413F8, walkerModel,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId  = ACTOR_143900_WALKER_INITIAL_CLIP;
    _gScriptedWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    _gScriptedWalkWork->st.travel  = 0;
    _gScriptedWalkWork->turnFrames = 0;
    task->msgTable                 = D_actor_143900_801413BC;
    SCRIPTED_WALK_UPDATE(task);
    task->state += 1;
}

#include "../../shared/scripted_walk_update.inc.c"

/// Dispatches initialization or one frame of the first scripted walker.
///
/// `task->state` must be 0 (spawn) or 1 (frame); indexing is unchecked.
/// The task owns its enemy in `spawnArg2.pointer`, TMD model and initialized
/// work. Republishes that work for the singleton animation/message handlers
/// before dispatch. State 0 creates the work and may destroy the task on failure.
static void _actor143900ScriptedWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor143900SpawnScriptedWalker,
        _actorRenderWalkerFrame,
    };

    _gScriptedWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER SCRIPTED_WALK_UPDATE
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the first scripted walker's enemy and begins task/model teardown.
///
/// Requires a live task owning its enemy in `spawnArg2.pointer` and its work.
/// The published task/work pointers are left stale and must not be used again.
/// Model release follows the resident task system's immediate/deferred policy.
static void _actor143900ExitScriptedWalker(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Reseeds the published first scripted walker from a borrowed animation request.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, message ID, second payload
/// and other request words are ignored. Requires a live walker and initialized
/// rig with loaded child-part tracks. Playable table keys are 1..19. The signed
/// check rejects IDs >= 20 with -1 but accepts zero and negative IDs; an accepted
/// ID narrows to `s16` and must select a loaded clip after narrowing.
/// Nonzero blend latches the low signed halfword of `blendFrames` in whole
/// normal-rate frames (0..2047 keeps playback nonnegative); zero blend restarts
/// and leaves that latch intact. Applies the reseed immediately, returns 0 on
/// acceptance and retains no request pointer.
static s32 _actor143900PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_143900_WALKER_CLIP_COUNT = (s32)(sizeof(D_actor_143900_801413F8) / sizeof(AnimationSet*)) };

    if (request->animationId < ACTOR_143900_WALKER_CLIP_COUNT) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        SCRIPTED_WALK_UPDATE(D_actor_143900_801496BC);
        return ACTOR_143900_PLAY_ANIMATION_APPLIED;
    }
    return ACTOR_143900_PLAY_ANIMATION_REJECTED;
}

/// Replaces the published first scripted walker's model draw flags.
///
/// Requires a live published task/model. `ACTOR_MESSAGE_SET_MODEL_DRAW` bit 0
/// enables active drawing; bit 1 suppresses automatic primitive-buffer allocation.
/// Clears all other model flags and ignores other request bits. Allocates and
/// releases no buffers. Receiver, message ID and second payload are ignored;
/// returns 0.
static s32 _actor143900SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* walkerModel;

    walkerModel = D_actor_143900_801496BC->extra.tmd;
    if (drawFlags & ACTOR_143900_WALKER_DRAW_SHOW) {
        walkerModel->flags = 0;
    } else {
        walkerModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_143900_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/scripted_walk_place.inc.c"

/// Schedules twenty turning updates for the published first scripted walker.
///
/// Requires live work and a command borrowed through `ACTOR_COMMAND_MESSAGE_APPLY`.
/// Command 0 arms the countdown consumed while turn clip 3 plays; it does not
/// select that clip. Each turn adds 51/4096 turns to the root yaw independently
/// of actor freezing. Other commands do nothing. Context tags, receiver, message
/// ID and second payload are ignored; returns 0 and retains no request pointer.
static s32 _actor143900ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_143900_WALKER_COMMAND_TURN = 0,
        ACTOR_143900_WALKER_TURN_UPDATES = 20,
    };

    if (request->command == ACTOR_143900_WALKER_COMMAND_TURN) {
        _gScriptedWalkWork->turnFrames = ACTOR_143900_WALKER_TURN_UPDATES;
    }
    return 0;
}

#include "../../shared/scripted_walk_to.inc.c"

/// Initializes the second scripted walker, its initial clip and two model attachments.
///
/// State 0 requires a live owned enemy and twenty-part TMD model. Allocates
/// zeroed primary-heap work and publishes it for the singleton handlers, lending
/// its matrices and rig storage until teardown. Allocation failure destroys the
/// enemy and task. The root becomes view-parented and untargetable; lighting uses
/// its cached translation with Y reduced by 800 coordinate units, without
/// composition or conversion to world space. Requires loaded clips and the
/// lighting/animation helpers' scratch/GTE state.
/// Attachment entries 1/2 borrow model parts 1/12 for their lifetime. Failed
/// attachment spawns leave NULL handles; later draw/command/exit handlers require
/// both spawns to have succeeded. The attachment tasks are owned separately from
/// the walker's task tree and killed explicitly by its exit callback.
static void _actor143900SpawnSecondScriptedWalker(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_143900_SECOND_WALKER_OT_OFFSET = 16,
        ACTOR_143900_ATTACHMENT1_ENTRY       = 1,
        ACTOR_143900_ATTACHMENT2_ENTRY       = 2,
        ACTOR_143900_ATTACHMENT1_PART        = 1,
        ACTOR_143900_ATTACHMENT2_PART        = 12,
    };
    VECTOR                       lightSample;
    ScriptedWalkAttachmentsWork* walkerWork;
    GfxCoord*                    rootCoord;
    TmdObject*                   walkerModel;
    Task*                        attachmentTask;

    walkerModel = task->extra.tmd;
    rootCoord   = walkerModel->coords;
    // Publish the owned allocation before lending its matrices and rig storage.
    walkerWork               = memCalloc(sizeof(*walkerWork), false);
    _gScriptedWalkSecondWork = walkerWork;
    task->work               = walkerWork;
    if (walkerWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor143900ExitSecondScriptedWalker;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    walkerModel->otOffset            = ACTOR_143900_SECOND_WALKER_OT_OFFSET;
    walkerModel->lightMtx            = &_gScriptedWalkSecondWork->light;
    walkerModel->colorMtx            = &_gScriptedWalkSecondWork->color;
    walkerModel->flags               = 0;
    lightSample.vx                   = rootCoord->workm.t[0];
    lightSample.vy                   = rootCoord->workm.t[1] - ACTOR_143900_WALKER_LIGHT_Y_OFFSET;
    lightSample.vz                   = rootCoord->workm.t[2];
    D_actor_143900_801496C8          = task;
    worldCoordSetModelLighting(walkerModel, &lightSample, 0, ACTOR_143900_WALKER_LIGHT_COUNT);
    animationInitContext(&_gScriptedWalkSecondWork->rig.anim, (AnimationSet**)D_actor_143900_80149688, walkerModel,
                         _gScriptedWalkSecondWork->rig.poses, _gScriptedWalkSecondWork->rig.slots);
    _gScriptedWalkSecondWork->st.animId = ACTOR_143900_WALKER_INITIAL_CLIP;
    _gScriptedWalkSecondWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    // The attachments borrow coordinates, but do not join the walker's task tree.
    attachmentTask = taskSpawnFromTable(D_actor_143900_80149664, ACTOR_143900_ATTACHMENT1_ENTRY, ACTOR_143900_ATTACHMENT1_PART, 0);
    if (attachmentTask != NULL) {
        _gScriptedWalkSecondWork->attachment1 = attachmentTask;
    }
    attachmentTask = taskSpawnFromTable(D_actor_143900_80149664, ACTOR_143900_ATTACHMENT2_ENTRY, ACTOR_143900_ATTACHMENT2_PART, 0);
    if (attachmentTask != NULL) {
        _gScriptedWalkSecondWork->attachment2 = attachmentTask;
    }
    _gScriptedWalkSecondWork->st.travel  = 0;
    _gScriptedWalkSecondWork->turnFrames = 0;
    task->msgTable                       = D_actor_143900_80149634;
    _scriptedWalkUpdateSecond(task);
    task->state++;
}

#undef SCRIPTED_WALK_UPDATE
/// Selects the second walker's private `void(Task*)` update for this inclusion.
///
/// The work, mode, duration and animation-helper bindings below select that
/// same walker; its prologue prototype supplies static linkage.
#define SCRIPTED_WALK_UPDATE _scriptedWalkUpdateSecond
#undef SCRIPTED_WALK_TICK_ANIM
/// Routes the second walker's update to its private `void(void)` animation tick.
///
/// The tick fragment is included later with this same function and work binding.
#define SCRIPTED_WALK_TICK_ANIM _scriptedWalkTickSecondAnim
#undef SCRIPTED_WALK_RESET_ANIM
/// Routes the second walker's update to its private `void(void)` track restart.
///
/// The reset fragment uses this function binding and the same live work block.
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetSecondAnim
#undef SCRIPTED_WALK_BLEND_ANIM
/// Routes the second walker's update to its private `void(void)` child-track blend.
///
/// The blend fragment uses this function, work block and blend-frame binding.
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendSecondAnim
#undef SCRIPTED_WALK_WORK
/// Selects the second walker's allocation for this fragment instance.
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#undef SCRIPTED_WALK_BLEND_FRAMES
/// Selects the second walker's independent writable `s16` duration latch.
///
/// Whole normal-rate frames, with the same binding at the blend definition.
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkSecondBlendFrames)
#undef SCRIPTED_WALK_MODE
/// Selects the second walker's independent signed-halfword approach mode.
#define SCRIPTED_WALK_MODE (_gScriptedWalkSecondMode)
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T ScriptedWalkAttachmentsWork
#include "../../shared/scripted_walk_update.inc.c"
#undef SCRIPTED_WALK_UPDATE
#define SCRIPTED_WALK_UPDATE _scriptedWalkUpdate
#undef SCRIPTED_WALK_TICK_ANIM
#undef SCRIPTED_WALK_RESET_ANIM
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetAnim
#undef SCRIPTED_WALK_BLEND_ANIM
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork
#undef SCRIPTED_WALK_BLEND_FRAMES
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkBlendFrames)
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T _Actor143900Work

/// Dispatches initialization or one frame of the second scripted walker.
///
/// `task->state` must be 0 (spawn) or 1 (frame); indexing is unchecked.
/// Owns its enemy in `spawnArg2.pointer`, TMD model and initialized attachment
/// work. Republishes that work before the selected singleton update. State 0
/// allocates work and may destroy the task on failure.
static void _actor143900SecondScriptedWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor143900SpawnSecondScriptedWalker,
        _actorRenderWalkerFrameSecond,
    };
    // Retain the original 0x60-byte frame; these 64 bytes are never accessed.
    u8 reservedFrame[0x40]; // Original purpose unproven.

    _gScriptedWalkSecondWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _scriptedWalkUpdateSecond
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Begins teardown of the second scripted walker and its two attachment tasks.
///
/// Requires a live task with its owned enemy in `spawnArg2.pointer` and two
/// live, non-NULL attachment handles in its work. Published work/task pointers
/// remain stale. `enemyDestroy` frees that work before the attachment handles
/// are read; the retained order relies on those released bytes remaining readable.
/// The models follow the task system's immediate/deferred release policy.
static void _actor143900ExitSecondScriptedWalker(Task* task)
{
    ScriptedWalkAttachmentsWork* walkerWork = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    // Preserve the post-release handle reads and teardown order.
    taskKill(walkerWork->attachment1);
    taskKill(walkerWork->attachment2);
}

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Initializes a drawable attachment root beneath a borrowed live walker part.
///
/// `model` owns `root`; the coordinate parent must remain live until model
/// teardown. Clears all model flags and sets the attachment's OT-entry offset.
static __inline__ void _actor143900AttachWalkerModel(TmdObject* model, GfxCoord* root, GfxCoord* parentPart)
{
    enum { ACTOR_143900_ATTACHMENT_OT_OFFSET = 15 };

    root->composeStamp = GRAPHICS_COORD_DIRTY;
    model->flags       = 0;
    model->otOffset    = ACTOR_143900_ATTACHMENT_OT_OFFSET;
    root->parent       = parentPart;
}

/// Parents a model to the second scripted walker and refreshes its room lighting.
///
/// Requires live models on this task and the published walker. `spawnArg1.value`
/// is a coordinate index in that twenty-part model; the two spawns use 1 and 12.
/// State 0 attaches the root and enters state 1, which samples three lights at
/// the walker root's cached translation with Y reduced by 800 coordinate units.
/// The sample keeps that cache's composition frame; this function does not
/// compose coordinates or convert the sample to world space. Other states do
/// nothing. The task borrows its parent coordinate until model teardown and
/// requires the lighting helper's initialized matrices, scratch and GTE state.
static void _actor143900ScriptedWalkerAttachmentTask(Task* task)
{
    enum {
        ACTOR_143900_ATTACHMENT_INITIALIZE = 0,
        ACTOR_143900_ATTACHMENT_LIGHT      = 1,
    };
    TmdObject* attachmentModel = task->extra.tmd;
    GfxCoord*  attachmentRoot  = attachmentModel->coords;
    GfxCoord*  walkerCoords    = D_actor_143900_801496C8->extra.tmd->coords;
    GfxCoord*  parentPart      = walkerCoords + task->spawnArg1.value;
    VECTOR     lightSample;

    switch (task->state) {
        case ACTOR_143900_ATTACHMENT_INITIALIZE:
            _actor143900AttachWalkerModel(attachmentModel, attachmentRoot, parentPart);
            task->state++;
            break;
        case ACTOR_143900_ATTACHMENT_LIGHT:
            lightSample.vx = walkerCoords->workm.t[0];
            lightSample.vy = walkerCoords->workm.t[1] - ACTOR_143900_WALKER_LIGHT_Y_OFFSET;
            lightSample.vz = walkerCoords->workm.t[2];
            worldCoordSetModelLighting(attachmentModel, &lightSample, 0, ACTOR_143900_WALKER_LIGHT_COUNT);
            break;
    }
}

/// Defines the private `void(void)` animation tick for the second walker's body rig.
#define SCRIPTED_WALK_TICK_ANIM _scriptedWalkTickSecondAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#include "../../shared/scripted_walk_tick_anim.inc.c"
#undef SCRIPTED_WALK_TICK_ANIM
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork

#undef SCRIPTED_WALK_RESET_ANIM
/// Defines the private track restart selected by the second walker's update.
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetSecondAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#include "../../shared/scripted_walk_reset_anim.inc.c"
#undef SCRIPTED_WALK_RESET_ANIM
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork

#undef SCRIPTED_WALK_BLEND_ANIM
/// Defines the private child-track blend selected by the second walker's update.
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendSecondAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#undef SCRIPTED_WALK_BLEND_FRAMES
/// Supplies the same whole-frame `s16` latch selected by the second update.
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkSecondBlendFrames)
#include "../../shared/scripted_walk_blend_anim.inc.c"
#undef SCRIPTED_WALK_BLEND_ANIM
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork
#undef SCRIPTED_WALK_BLEND_FRAMES
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkBlendFrames)

/// Reseeds the published second scripted walker from a borrowed animation request.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, message ID, second payload
/// and other request words are ignored. Requires a live walker and initialized
/// rig with loaded child-part tracks. Playable keys are 1..10. The signed check
/// rejects IDs >= 12 with -1 but accepts negative IDs and NULL entries 0/11;
/// an accepted ID narrows to `s16` and must select a loaded clip after narrowing.
/// Nonzero blend latches the low signed halfword of `blendFrames` in whole
/// normal-rate frames (0..2047 keeps playback nonnegative); zero blend restarts
/// and leaves that latch intact. Reseeding runs immediately. Returns 0 on
/// acceptance and retains no request pointer.
static s32 _actor143900PlaySecondScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_143900_WALKER_CLIP_COUNT = (s32)(sizeof(D_actor_143900_80149688) / sizeof(AnimationSet*)) };

    if (request->animationId < ACTOR_143900_WALKER_CLIP_COUNT) {
        _gScriptedWalkSecondWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkSecondWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkSecondBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkSecondWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkSecondWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        _scriptedWalkUpdateSecond(D_actor_143900_801496C8);
        return ACTOR_143900_PLAY_ANIMATION_APPLIED;
    }
    return ACTOR_143900_PLAY_ANIMATION_REJECTED;
}

/// Replaces the second scripted walker and both attachment models' draw flags.
///
/// Requires live published task/work and two non-NULL attachment models.
/// `ACTOR_MESSAGE_SET_MODEL_DRAW` bit 0 enables active drawing; bit 1 suppresses
/// automatic primitive-buffer allocation. Clears all other model flags and
/// ignores other request bits, allocating and releasing no buffers. Receiver,
/// message ID and second payload are ignored; returns 0.
static s32 _actor143900SetSecondScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* walkerModel      = D_actor_143900_801496C8->extra.tmd;
    TmdObject* attachment1Model = _gScriptedWalkSecondWork->attachment1->extra.tmd;
    TmdObject* attachment2Model = _gScriptedWalkSecondWork->attachment2->extra.tmd;

    if (drawFlags & ACTOR_143900_WALKER_DRAW_SHOW) {
        walkerModel->flags      = 0;
        attachment1Model->flags = 0;
        attachment2Model->flags = 0;
    } else {
        walkerModel->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment1Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment2Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_143900_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment1Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment2Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#undef SCRIPTED_WALK_PLACE
/// Defines the private placement callback for the second walker.
///
/// Its static prologue prototype establishes linkage; the work binding below
/// selects the same receiver's allocation for the cached signed yaw.
#define SCRIPTED_WALK_PLACE _scriptedWalkPlaceSecond
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#include "../../shared/scripted_walk_place.inc.c"
#undef SCRIPTED_WALK_PLACE
#define SCRIPTED_WALK_PLACE _scriptedWalkPlace
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork

/// Selects which of the second scripted walker's two attachment models is shown.
///
/// Requires live published work and both non-NULL attachment models. The borrowed
/// `ACTOR_COMMAND_MESSAGE_APPLY` command selects attachment 2 with 0 or attachment
/// 1 with 1, clearing that model's flags and replacing the other's with active-draw
/// suppression. Other commands leave both models unchanged. Context tags,
/// receiver, message ID and second payload are ignored; returns 0 and retains
/// no request pointer. This changes flags only and manages no primitive buffers.
static s32 _actor143900SelectSecondScriptedWalkerAttachment(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT2 = 0,
        ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT1 = 1,
    };
    TmdObject* attachment1Model;
    TmdObject* attachment2Model;

    attachment1Model = _gScriptedWalkSecondWork->attachment1->extra.tmd;
    attachment2Model = _gScriptedWalkSecondWork->attachment2->extra.tmd;
    switch (request->command) {
        case ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT2:
            attachment2Model->flags = 0;
            attachment1Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT1:
            attachment1Model->flags = 0;
            attachment2Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    return 0;
}

#undef SCRIPTED_WALK_TO
/// Selects the private walk-to callback for the second walker.
///
/// The prologue declares its signature and static linkage. The work type and
/// mode bindings below select the same receiver for this fragment inclusion.
#define SCRIPTED_WALK_TO _scriptedWalkToSecond
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE (_gScriptedWalkSecondMode)
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T ScriptedWalkAttachmentsWork
#include "../../shared/scripted_walk_to.inc.c"
#undef SCRIPTED_WALK_TO
#define SCRIPTED_WALK_TO _scriptedWalkTo
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T _Actor143900Work
