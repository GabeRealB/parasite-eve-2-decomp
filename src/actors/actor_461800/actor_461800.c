#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/neo_ark_r31.h"
#define FOOTSTEP_WALK_WORK_T FootstepWalkWork
#include "../../shared/footstep_walk.h"
#include "../../shared/scripted_walk.h"

static void _footstepWalkUpdate(Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkPlayStepSound(Task* task);
static void _footstepWalkTickAnim(void);
static void _footstepWalkResetAnim(void);
static void _footstepWalkBlendAnim(void);
static s32  _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

static s16 _gScriptedWalkMode;

static ScriptedWalkAttachmentsWork* _gScriptedWalkWork;

/// The task the first variant's work block above belongs to, published by
/// `func_actor_461800_80132390` alongside it.
extern Task* D_actor_461800_80143898;

static FootstepWalkWork* _gFootstepWalkWork;

/// The second variant's task, published by its spawn routine
/// `_footstepWalkSpawn` so the handlers can reach its model.
extern Task* gFootstepWalkTask;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern Task* D_actor_461800_80133EB8;

extern Task*    D_actor_461800_80133EB4;
extern TaskDesc D_actor_461800_80133EBC[];

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_461800_80139F5C[6];
extern TaskDesc         D_actor_461800_80139F8C[];
extern AnimationSet*    D_actor_461800_80139FB0[6];

extern s32 D_actor_461800_80143884;
extern s32 D_actor_461800_80143888;
extern s32 D_actor_461800_8014388C;
extern s32 D_actor_461800_80143890;

extern TaskMessageEntry gFootstepWalkMsgTable[6];
extern AnimationSet*    gFootstepWalkAnims[35];

static s16 _gFootstepWalkMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor461800ExitScriptedWalker(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static s32  _actor461800PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor461800SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor461800ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static s32  _actor461800SetFootstepWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor461800ApplyFootstepWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
void        func_actor_461800_801329B0(Task*);
static void _actor461800ScriptedWalkerAttachmentTask(Task* task);
static void _actor461800FootstepWalkerTask(Task* task);

static void _actor461800SceneDistortionTask(Task* task);
static void _actor461800SceneFadeTask(Task* task);
static void _actor461800SetSceneFadeState(s32 state);
static void _actor461800SetSceneDistortionState(s32 state);
static void _actor461800FinishScene(void);

/// Scene-effect descriptor indices and the negative callback argument that starts them.
enum {
    ACTOR_461800_SCENE_EFFECT_START      = -1,
    ACTOR_461800_SCENE_EFFECT_FADE       = 0,
    ACTOR_461800_SCENE_EFFECT_DISTORTION = 1,
};

/// Fade states selected by the scene callbacks; ramp changes take one unit per update.
enum {
    ACTOR_461800_SCENE_FADE_OPAQUE = 0,
    ACTOR_461800_SCENE_FADE_CLEAR  = 1,
    ACTOR_461800_SCENE_FADE_IN     = 2,
    ACTOR_461800_SCENE_FADE_OUT    = 3,
};

/// Updates needed for the scene fade to span its full intensity range.
enum { ACTOR_461800_SCENE_FADE_RAMP_UPDATES = 90 };

/// Distortion states; settling reaches both ramp maxima before starting the slow decay.
enum {
    ACTOR_461800_SCENE_DISTORTION_INITIALIZE = 0,
    ACTOR_461800_SCENE_DISTORTION_PULSE      = 1,
    ACTOR_461800_SCENE_DISTORTION_SETTLE     = 2,
    ACTOR_461800_SCENE_DISTORTION_DECAY      = 3,
    ACTOR_461800_SCENE_DISTORTION_STOP       = 4,
};

/// Commands shared by the walkers and their duration in turning updates.
enum {
    ACTOR_461800_WALKER_COMMAND_TURN             = 0,
    ACTOR_461800_WALKER_COMMAND_ENABLE_FOOTSTEPS = 1,
    ACTOR_461800_WALKER_TURN_UPDATES             = 20,
};

/// Model-draw request bits shared by the two scene walkers.
enum {
    ACTOR_461800_WALKER_DRAW_SHOW             = 1 << 0,
    ACTOR_461800_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1,
};

static AnimationPackedPose _gActor461800Animation0206CBank1[3] = {
#include "assets/actor_461800_animation_0206C_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0206CBank4[27] = {
#include "assets/actor_461800_animation_0206C_bank4.inc"
};

static AnimationRecord _gActor461800Animation0206CRecords[104] = {
#include "assets/actor_461800_animation_0206C_records.inc"
};

static u16 _gActor461800Animation0206CIndices[20] = {
#include "assets/actor_461800_animation_0206C_indices.inc"
};

static AnimationSet _gActor461800Animation0206C = {
    _gActor461800Animation0206CRecords,
    _gActor461800Animation0206CIndices,
    { NULL, _gActor461800Animation0206CBank1, NULL, NULL, _gActor461800Animation0206CBank4, NULL, NULL, NULL },
};

Task* D_actor_461800_80133EB4 = NULL;

Task* D_actor_461800_80133EB8 = NULL;

TaskDesc D_actor_461800_80133EBC[2] = {
    { { { TASK_BODY_NONE, 32 } }, _actor461800SceneFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _actor461800SceneDistortionTask, { .value = 0 } },
};

ActorTransform D_actor_461800_80133ED4 = { { 7710, 980, 6290, 0 }, { 0, -1479, 0, 0 } };

ActorTransform D_actor_461800_80133EEC = { { 7030, 980, 7530, 0 }, { 0, -1820, 0, 0 } };

AnimationPlayRequest D_actor_461800_80133F04 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F18 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F2C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F40 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F54 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_461800_80133F68 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_actor_461800_80133F7C[1] = {
    &_gActor461800Animation0206C,
};

AnimationBankCopyRequest D_actor_461800_80133F80 = { { .sets = D_actor_461800_80133F7C }, ARRAY_SIZE(D_actor_461800_80133F7C) };

EvsSceneKey D_actor_461800_80133F88 = { 6, 18, 11 };

EvsCommand D_actor_461800_80133F90[52] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_461800_80133F88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_461800_80133F80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_461800_80133ED4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_EFFECT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneDistortionState }, { .value = ACTOR_461800_SCENE_EFFECT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_461800_WALKER_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_461800_80133EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_461800_80133F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneDistortionState }, { .value = ACTOR_461800_SCENE_DISTORTION_SETTLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 55 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneDistortionState }, { .value = ACTOR_461800_SCENE_DISTORTION_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_461800_WALKER_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor461800FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_461800_80134470[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor461800FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor461800KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor461800KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor461800KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor461800KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor461800KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor461800KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor461800KyleMadiganBodyPartVerts,
    _gActor461800KyleMadiganBodyVerts,
    _gActor461800KyleMadiganBodyNormals,
    _gActor461800KyleMadiganBodySkeleton,
    _gActor461800KyleMadiganBodyStream,
};

static TmdBone _gActor461800HandRightSkeleton[1] = {
#include "assets/actor_461800_hand_right_skeleton.inc"
};

static u32 _gActor461800HandRightPartVerts[1] = {
#include "assets/actor_461800_hand_right_partVerts.inc"
};

static SVECTOR _gActor461800HandRightVerts[18] = {
#include "assets/actor_461800_hand_right_verts.inc"
};

static SVECTOR _gActor461800HandRightNormals[18] = {
#include "assets/actor_461800_hand_right_normals.inc"
};

static u32 _gActor461800HandRightStream[151] = {
#include "assets/actor_461800_hand_right_stream.inc"
};

static TmdSource _gActor461800HandRight = {
    0,
    1000,
    0,
    1,
    _gActor461800HandRightPartVerts,
    _gActor461800HandRightVerts,
    _gActor461800HandRightNormals,
    _gActor461800HandRightSkeleton,
    _gActor461800HandRightStream,
};

static TmdBone _gActor461800HandLeftSkeleton[1] = {
#include "assets/actor_461800_hand_left_skeleton.inc"
};

static u32 _gActor461800HandLeftPartVerts[1] = {
#include "assets/actor_461800_hand_left_partVerts.inc"
};

static SVECTOR _gActor461800HandLeftVerts[27] = {
#include "assets/actor_461800_hand_left_verts.inc"
};

static SVECTOR _gActor461800HandLeftNormals[27] = {
#include "assets/actor_461800_hand_left_normals.inc"
};

static u32 _gActor461800HandLeftStream[189] = {
#include "assets/actor_461800_hand_left_stream.inc"
};

static TmdSource _gActor461800HandLeft = {
    0,
    1328,
    0,
    1,
    _gActor461800HandLeftPartVerts,
    _gActor461800HandLeftVerts,
    _gActor461800HandLeftNormals,
    _gActor461800HandLeftSkeleton,
    _gActor461800HandLeftStream,
};

static AnimationPackedPose _gActor461800Animation07D34Bank1[2] = {
#include "assets/actor_461800_animation_07D34_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation07D34Bank4[26] = {
#include "assets/actor_461800_animation_07D34_bank4.inc"
};

static AnimationRecord _gActor461800Animation07D34Records[96] = {
#include "assets/actor_461800_animation_07D34_records.inc"
};

static u16 _gActor461800Animation07D34Indices[20] = {
#include "assets/actor_461800_animation_07D34_indices.inc"
};

static AnimationSet _gActor461800Animation07D34 = {
    _gActor461800Animation07D34Records,
    _gActor461800Animation07D34Indices,
    { NULL, _gActor461800Animation07D34Bank1, NULL, NULL, _gActor461800Animation07D34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation07F58Bank1[2] = {
#include "assets/actor_461800_animation_07F58_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation07F58Bank4[31] = {
#include "assets/actor_461800_animation_07F58_bank4.inc"
};

static AnimationRecord _gActor461800Animation07F58Records[80] = {
#include "assets/actor_461800_animation_07F58_records.inc"
};

static u16 _gActor461800Animation07F58Indices[20] = {
#include "assets/actor_461800_animation_07F58_indices.inc"
};

static AnimationSet _gActor461800Animation07F58 = {
    _gActor461800Animation07F58Records,
    _gActor461800Animation07F58Indices,
    { NULL, _gActor461800Animation07F58Bank1, NULL, NULL, _gActor461800Animation07F58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation08110Bank1[2] = {
#include "assets/actor_461800_animation_08110_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation08110Bank4[20] = {
#include "assets/actor_461800_animation_08110_bank4.inc"
};

static AnimationRecord _gActor461800Animation08110Records[64] = {
#include "assets/actor_461800_animation_08110_records.inc"
};

static u16 _gActor461800Animation08110Indices[20] = {
#include "assets/actor_461800_animation_08110_indices.inc"
};

static AnimationSet _gActor461800Animation08110 = {
    _gActor461800Animation08110Records,
    _gActor461800Animation08110Indices,
    { NULL, _gActor461800Animation08110Bank1, NULL, NULL, _gActor461800Animation08110Bank4, NULL, NULL, NULL },
};

/// Latched duration of the first walker's next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword; walk completion replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`.
/// Plain resets leave it intact. Zero requests no transition time; 0..2047
/// keeps the playback timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry D_actor_461800_80139F5C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor461800PlayScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor461800SetScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor461800ApplyScriptedWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _scriptedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_461800_80139F8C[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_801329B0, { .model = &_gActor461800KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor461800ScriptedWalkerAttachmentTask, { .model = &_gActor461800HandLeft } },
    { { { TASK_BODY_TMD, 192 } }, _actor461800ScriptedWalkerAttachmentTask, { .model = &_gActor461800HandRight } },
};

AnimationSet* D_actor_461800_80139FB0[6] = {
    NULL,
    &_gActor461800Animation07D34,
    &_gActor461800Animation07F58,
    &_gActor461800Animation08110,
    NULL,
    NULL,
};

static TmdBone _gActor461800AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor461800AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor461800AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor461800AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor461800AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor461800AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor461800AyaBreaBodyPartVerts,
    _gActor461800AyaBreaBodyVerts,
    _gActor461800AyaBreaBodyNormals,
    _gActor461800AyaBreaBodySkeleton,
    _gActor461800AyaBreaBodyStream,
};

static AnimationPackedPose _gActor461800Animation0DB6CBank1[2] = {
#include "assets/actor_461800_animation_0DB6C_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0DB6CBank4[23] = {
#include "assets/actor_461800_animation_0DB6C_bank4.inc"
};

static AnimationRecord _gActor461800Animation0DB6CRecords[84] = {
#include "assets/actor_461800_animation_0DB6C_records.inc"
};

static u16 _gActor461800Animation0DB6CIndices[20] = {
#include "assets/actor_461800_animation_0DB6C_indices.inc"
};

static AnimationSet _gActor461800Animation0DB6C = {
    _gActor461800Animation0DB6CRecords,
    _gActor461800Animation0DB6CIndices,
    { NULL, _gActor461800Animation0DB6CBank1, NULL, NULL, _gActor461800Animation0DB6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0DF64Bank1[7] = {
#include "assets/actor_461800_animation_0DF64_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0DF64Bank4[75] = {
#include "assets/actor_461800_animation_0DF64_bank4.inc"
};

static AnimationRecord _gActor461800Animation0DF64Records[138] = {
#include "assets/actor_461800_animation_0DF64_records.inc"
};

static u16 _gActor461800Animation0DF64Indices[20] = {
#include "assets/actor_461800_animation_0DF64_indices.inc"
};

static AnimationSet _gActor461800Animation0DF64 = {
    _gActor461800Animation0DF64Records,
    _gActor461800Animation0DF64Indices,
    { NULL, _gActor461800Animation0DF64Bank1, NULL, NULL, _gActor461800Animation0DF64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0EB48Bank1[22] = {
#include "assets/actor_461800_animation_0EB48_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0EB48Bank4[298] = {
#include "assets/actor_461800_animation_0EB48_bank4.inc"
};

static AnimationRecord _gActor461800Animation0EB48Records[377] = {
#include "assets/actor_461800_animation_0EB48_records.inc"
};

static u16 _gActor461800Animation0EB48Indices[20] = {
#include "assets/actor_461800_animation_0EB48_indices.inc"
};

static AnimationSet _gActor461800Animation0EB48 = {
    _gActor461800Animation0EB48Records,
    _gActor461800Animation0EB48Indices,
    { NULL, _gActor461800Animation0EB48Bank1, NULL, NULL, _gActor461800Animation0EB48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0EDB0Bank1[4] = {
#include "assets/actor_461800_animation_0EDB0_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0EDB0Bank4[48] = {
#include "assets/actor_461800_animation_0EDB0_bank4.inc"
};

static AnimationRecord _gActor461800Animation0EDB0Records[74] = {
#include "assets/actor_461800_animation_0EDB0_records.inc"
};

static u16 _gActor461800Animation0EDB0Indices[20] = {
#include "assets/actor_461800_animation_0EDB0_indices.inc"
};

static AnimationSet _gActor461800Animation0EDB0 = {
    _gActor461800Animation0EDB0Records,
    _gActor461800Animation0EDB0Indices,
    { NULL, _gActor461800Animation0EDB0Bank1, NULL, NULL, _gActor461800Animation0EDB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0F1B8Bank1[5] = {
#include "assets/actor_461800_animation_0F1B8_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0F1B8Bank4[76] = {
#include "assets/actor_461800_animation_0F1B8_bank4.inc"
};

static AnimationRecord _gActor461800Animation0F1B8Records[147] = {
#include "assets/actor_461800_animation_0F1B8_records.inc"
};

static u16 _gActor461800Animation0F1B8Indices[20] = {
#include "assets/actor_461800_animation_0F1B8_indices.inc"
};

static AnimationSet _gActor461800Animation0F1B8 = {
    _gActor461800Animation0F1B8Records,
    _gActor461800Animation0F1B8Indices,
    { NULL, _gActor461800Animation0F1B8Bank1, NULL, NULL, _gActor461800Animation0F1B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0F5C4Bank1[5] = {
#include "assets/actor_461800_animation_0F5C4_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0F5C4Bank4[89] = {
#include "assets/actor_461800_animation_0F5C4_bank4.inc"
};

static AnimationRecord _gActor461800Animation0F5C4Records[135] = {
#include "assets/actor_461800_animation_0F5C4_records.inc"
};

static u16 _gActor461800Animation0F5C4Indices[20] = {
#include "assets/actor_461800_animation_0F5C4_indices.inc"
};

static AnimationSet _gActor461800Animation0F5C4 = {
    _gActor461800Animation0F5C4Records,
    _gActor461800Animation0F5C4Indices,
    { NULL, _gActor461800Animation0F5C4Bank1, NULL, NULL, _gActor461800Animation0F5C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0F8D4Bank1[4] = {
#include "assets/actor_461800_animation_0F8D4_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0F8D4Bank4[59] = {
#include "assets/actor_461800_animation_0F8D4_bank4.inc"
};

static AnimationRecord _gActor461800Animation0F8D4Records[105] = {
#include "assets/actor_461800_animation_0F8D4_records.inc"
};

static u16 _gActor461800Animation0F8D4Indices[20] = {
#include "assets/actor_461800_animation_0F8D4_indices.inc"
};

static AnimationSet _gActor461800Animation0F8D4 = {
    _gActor461800Animation0F8D4Records,
    _gActor461800Animation0F8D4Indices,
    { NULL, _gActor461800Animation0F8D4Bank1, NULL, NULL, _gActor461800Animation0F8D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0FB2CBank1[2] = {
#include "assets/actor_461800_animation_0FB2C_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0FB2CBank4[30] = {
#include "assets/actor_461800_animation_0FB2C_bank4.inc"
};

static AnimationRecord _gActor461800Animation0FB2CRecords[94] = {
#include "assets/actor_461800_animation_0FB2C_records.inc"
};

static u16 _gActor461800Animation0FB2CIndices[20] = {
#include "assets/actor_461800_animation_0FB2C_indices.inc"
};

static AnimationSet _gActor461800Animation0FB2C = {
    _gActor461800Animation0FB2CRecords,
    _gActor461800Animation0FB2CIndices,
    { NULL, _gActor461800Animation0FB2CBank1, NULL, NULL, _gActor461800Animation0FB2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation0FDBCBank1[2] = {
#include "assets/actor_461800_animation_0FDBC_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation0FDBCBank4[48] = {
#include "assets/actor_461800_animation_0FDBC_bank4.inc"
};

static AnimationRecord _gActor461800Animation0FDBCRecords[90] = {
#include "assets/actor_461800_animation_0FDBC_records.inc"
};

static u16 _gActor461800Animation0FDBCIndices[20] = {
#include "assets/actor_461800_animation_0FDBC_indices.inc"
};

static AnimationSet _gActor461800Animation0FDBC = {
    _gActor461800Animation0FDBCRecords,
    _gActor461800Animation0FDBCIndices,
    { NULL, _gActor461800Animation0FDBCBank1, NULL, NULL, _gActor461800Animation0FDBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation101A4Bank1[5] = {
#include "assets/actor_461800_animation_101A4_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation101A4Bank4[59] = {
#include "assets/actor_461800_animation_101A4_bank4.inc"
};

static AnimationRecord _gActor461800Animation101A4Records[156] = {
#include "assets/actor_461800_animation_101A4_records.inc"
};

static u16 _gActor461800Animation101A4Indices[20] = {
#include "assets/actor_461800_animation_101A4_indices.inc"
};

static AnimationSet _gActor461800Animation101A4 = {
    _gActor461800Animation101A4Records,
    _gActor461800Animation101A4Indices,
    { NULL, _gActor461800Animation101A4Bank1, NULL, NULL, _gActor461800Animation101A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation10454Bank1[3] = {
#include "assets/actor_461800_animation_10454_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation10454Bank4[29] = {
#include "assets/actor_461800_animation_10454_bank4.inc"
};

static AnimationRecord _gActor461800Animation10454Records[114] = {
#include "assets/actor_461800_animation_10454_records.inc"
};

static u16 _gActor461800Animation10454Indices[20] = {
#include "assets/actor_461800_animation_10454_indices.inc"
};

static AnimationSet _gActor461800Animation10454 = {
    _gActor461800Animation10454Records,
    _gActor461800Animation10454Indices,
    { NULL, _gActor461800Animation10454Bank1, NULL, NULL, _gActor461800Animation10454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation10610Bank1[2] = {
#include "assets/actor_461800_animation_10610_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation10610Bank4[20] = {
#include "assets/actor_461800_animation_10610_bank4.inc"
};

static AnimationRecord _gActor461800Animation10610Records[65] = {
#include "assets/actor_461800_animation_10610_records.inc"
};

static u16 _gActor461800Animation10610Indices[20] = {
#include "assets/actor_461800_animation_10610_indices.inc"
};

static AnimationSet _gActor461800Animation10610 = {
    _gActor461800Animation10610Records,
    _gActor461800Animation10610Indices,
    { NULL, _gActor461800Animation10610Bank1, NULL, NULL, _gActor461800Animation10610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation109F8Bank1[3] = {
#include "assets/actor_461800_animation_109F8_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation109F8Bank4[65] = {
#include "assets/actor_461800_animation_109F8_bank4.inc"
};

static AnimationRecord _gActor461800Animation109F8Records[156] = {
#include "assets/actor_461800_animation_109F8_records.inc"
};

static u16 _gActor461800Animation109F8Indices[20] = {
#include "assets/actor_461800_animation_109F8_indices.inc"
};

static AnimationSet _gActor461800Animation109F8 = {
    _gActor461800Animation109F8Records,
    _gActor461800Animation109F8Indices,
    { NULL, _gActor461800Animation109F8Bank1, NULL, NULL, _gActor461800Animation109F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation113A8Bank1[18] = {
#include "assets/actor_461800_animation_113A8_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation113A8Bank4[232] = {
#include "assets/actor_461800_animation_113A8_bank4.inc"
};

static AnimationRecord _gActor461800Animation113A8Records[314] = {
#include "assets/actor_461800_animation_113A8_records.inc"
};

static u16 _gActor461800Animation113A8Indices[20] = {
#include "assets/actor_461800_animation_113A8_indices.inc"
};

static AnimationSet _gActor461800Animation113A8 = {
    _gActor461800Animation113A8Records,
    _gActor461800Animation113A8Indices,
    { NULL, _gActor461800Animation113A8Bank1, NULL, NULL, _gActor461800Animation113A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation11724Bank1[4] = {
#include "assets/actor_461800_animation_11724_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation11724Bank4[68] = {
#include "assets/actor_461800_animation_11724_bank4.inc"
};

static AnimationRecord _gActor461800Animation11724Records[123] = {
#include "assets/actor_461800_animation_11724_records.inc"
};

static u16 _gActor461800Animation11724Indices[20] = {
#include "assets/actor_461800_animation_11724_indices.inc"
};

static AnimationSet _gActor461800Animation11724 = {
    _gActor461800Animation11724Records,
    _gActor461800Animation11724Indices,
    { NULL, _gActor461800Animation11724Bank1, NULL, NULL, _gActor461800Animation11724Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor461800Animation11970Bank1[2] = {
#include "assets/actor_461800_animation_11970_bank1.inc"
};

static AnimationPackedRotation _gActor461800Animation11970Bank4[27] = {
#include "assets/actor_461800_animation_11970_bank4.inc"
};

static AnimationRecord _gActor461800Animation11970Records[94] = {
#include "assets/actor_461800_animation_11970_records.inc"
};

static u16 _gActor461800Animation11970Indices[20] = {
#include "assets/actor_461800_animation_11970_indices.inc"
};

static AnimationSet _gActor461800Animation11970 = {
    _gActor461800Animation11970Records,
    _gActor461800Animation11970Indices,
    { NULL, _gActor461800Animation11970Bank1, NULL, NULL, _gActor461800Animation11970Bank4, NULL, NULL, NULL },
};

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry gFootstepWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor461800SetFootstepWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor461800ApplyFootstepWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_461800_801437EC = { { { TASK_BODY_TMD, 192 } }, _actor461800FootstepWalkerTask, { .model = &_gActor461800AyaBreaBody } };

AnimationSet* gFootstepWalkAnims[35] = {
    NULL,
    &_gActor461800Animation0EDB0,
    &_gActor461800Animation0F1B8,
    &_gActor461800Animation0F5C4,
    &_gActor461800Animation0F8D4,
    &_gActor461800Animation0FB2C,
    &_gActor461800Animation0FDBC,
    &_gActor461800Animation101A4,
    &_gActor461800Animation10454,
    NULL,
    &_gActor461800Animation10610,
    NULL,
    NULL,
    &_gActor461800Animation0DB6C,
    &_gActor461800Animation0DF64,
    &_gActor461800Animation0EB48,
    &_gActor461800Animation109F8,
    &_gActor461800Animation113A8,
    &_gActor461800Animation11970,
    &_gActor461800Animation11724,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

s32 D_actor_461800_80143884 = 0;

s32 D_actor_461800_80143888 = 0;

s32 D_actor_461800_8014388C = 0;

s32 D_actor_461800_80143890 = 0;

/// Borrowed work block of the scripted walker and its two model attachments.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// Animation and message handlers require it to remain live; task teardown
/// releases it without clearing this pointer.
static ScriptedWalkAttachmentsWork* _gScriptedWalkWork = NULL;

Task* D_actor_461800_80143898 = NULL;

/// Signed approach mode of the scripted walker's last walk-to message (`SCRIPTED_WALK_MODE_*`).
static s16 _gScriptedWalkMode = 0;

/// A halfword stored after the mode; nothing references it.
u16 D_actor_461800_8014389E = 0xC9A8;

/// Borrowed pointer to the sound walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkWork* _gFootstepWalkWork;

Task* gFootstepWalkTask;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

static void func_actor_461800_80132390(Enemy* enemy, Task* task);

/// Advances the scene ramps and publishes the room framebuffer's horizontal shift.
///
/// Requires the live Neo Ark room-31 effect and a bodyless task. State 0 sets the
/// shift ramp to 16; 1 pulses both ramps; 2 raises them to 16/19 and enters 3;
/// 3 decays them every four calls. The room receives 3..8 pixels of left shift.
/// State 4 clears vertical shake and tells the room effect to exit, leaving this
/// task alive. Other states still advance the signed-halfword tick counter and
/// publish the shift. The second ramp has no reader; its purpose is unproven.
static void _actor461800SceneDistortionTask(Task* task)
{
    enum {
        ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX    = 16,
        ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX   = 19,
        ACTOR_461800_SCENE_DISTORTION_DECAY_PERIOD      = 4,
        ACTOR_461800_SCENE_DISTORTION_RAMP_PER_PIXEL    = 3,
        ACTOR_461800_SCENE_DISTORTION_BASE_SHIFT_PIXELS = 3,
        ACTOR_461800_SCENE_DISTORTION_DISABLED          = -1,
    };

    /// Steps one triangle-wave ramp and holds each endpoint for one update.
    ///
    /// ramp and increasing are distinct writable `s32` lvalues without side
    /// effects; both are evaluated repeatedly. maximum is positive and has no
    /// side effects, with ramp initially in 0..maximum. No locals are
    /// captured. This compound-statement macro is used only in this body.
#define ACTOR_461800_STEP_SCENE_RAMP(ramp, increasing, maximum) \
    {                                                           \
        if ((increasing) != 0) {                                \
            if ((ramp) < (maximum)) {                           \
                (ramp)++;                                       \
            } else {                                            \
                (increasing) = 0;                               \
            }                                                   \
        } else if ((ramp) > 0) {                                \
            (ramp)--;                                           \
        } else {                                                \
            (increasing) = 1;                                   \
        }                                                       \
    }

    switch (task->state) {
        case ACTOR_461800_SCENE_DISTORTION_INITIALIZE:
            D_actor_461800_80143884 = ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX;
            break;
        case ACTOR_461800_SCENE_DISTORTION_PULSE:
            ACTOR_461800_STEP_SCENE_RAMP(D_actor_461800_80143884, D_actor_461800_80143888,
                                         ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX);
            ACTOR_461800_STEP_SCENE_RAMP(D_actor_461800_80143890, D_actor_461800_8014388C,
                                         ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX);
            break;
        case ACTOR_461800_SCENE_DISTORTION_SETTLE:
            if (D_actor_461800_80143884 < ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX) {
                D_actor_461800_80143884++;
            }
            if (D_actor_461800_80143890 < ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX) {
                D_actor_461800_80143890++;
            }
            if (D_actor_461800_80143884 == ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX && D_actor_461800_80143890 == ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX) {
                task->state = ACTOR_461800_SCENE_DISTORTION_DECAY;
            }
            break;
        case ACTOR_461800_SCENE_DISTORTION_DECAY:
            if (!(task->killCountdown & (ACTOR_461800_SCENE_DISTORTION_DECAY_PERIOD - 1))) {
                if (D_actor_461800_80143884 > 0) {
                    D_actor_461800_80143884--;
                }
                if (D_actor_461800_80143890 > 0) {
                    D_actor_461800_80143890--;
                }
            }
            break;
        case ACTOR_461800_SCENE_DISTORTION_STOP:
            displaySetShakeY(0);
            D_neo_ark_r31_8017DC54 = ACTOR_461800_SCENE_DISTORTION_DISABLED;
            return;
    }
#undef ACTOR_461800_STEP_SCENE_RAMP
    task->killCountdown++;
    D_neo_ark_r31_8017DC54 = D_actor_461800_80143884 / ACTOR_461800_SCENE_DISTORTION_RAMP_PER_PIXEL + ACTOR_461800_SCENE_DISTORTION_BASE_SHIFT_PIXELS;
}

/// Queues the scene's centered 320x256 subtractive fade at its current intensity.
///
/// `fadeTask->killCountdown` must be in 0..90 ramp updates; scaling to 0..255
/// truncates to a colour byte without clamping. The task is borrowed and unchanged.
/// Requires tag 5 in the current OT and word-aligned frame-arena space for
/// `sizeof(TILE) + sizeof(DR_TPAGE)`, reserved without a capacity check.
/// Packets must remain live until GPU completion. The command precedes the tile
/// and leaves subtractive blending and dithering enabled, with displayed-area
/// drawing disabled. Its texture page at VRAM (0, 0) is unused by the tile.
static __inline__ void _actor461800QueueSceneFade(const Task* fadeTask)
{
    enum {
        ACTOR_461800_SCENE_FADE_MAX_INTENSITY      = 255,
        ACTOR_461800_SCENE_FADE_WIDTH_PIXELS       = 320,
        ACTOR_461800_SCENE_FADE_HEIGHT_PIXELS      = 256,
        ACTOR_461800_SCENE_FADE_OT_TAG             = 5,
        ACTOR_461800_SCENE_FADE_TEXTURE_DEPTH_4BIT = 0,
    };
    TILE*     tile;
    DR_TPAGE* blendCommand;
    u8        intensity;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    intensity      = (fadeTask->killCountdown * ACTOR_461800_SCENE_FADE_MAX_INTENSITY) / ACTOR_461800_SCENE_FADE_RAMP_UPDATES;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->x0 = -ACTOR_461800_SCENE_FADE_WIDTH_PIXELS / 2;
    tile->y0 = -ACTOR_461800_SCENE_FADE_HEIGHT_PIXELS / 2;
    tile->w  = ACTOR_461800_SCENE_FADE_WIDTH_PIXELS;
    tile->h  = ACTOR_461800_SCENE_FADE_HEIGHT_PIXELS;
    tile->r0 = intensity;
    tile->g0 = intensity;
    tile->b0 = intensity;
    addPrim(gGpuCurrentOt + ACTOR_461800_SCENE_FADE_OT_TAG, tile);

    // OT insertion prepends the draw mode so it executes before the tile.
    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(ACTOR_461800_SCENE_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + ACTOR_461800_SCENE_FADE_OT_TAG, blendCommand);
}

/// Queues the scene's 320x256 subtractive fade tile and advances its intensity.
///
/// State 0 holds black, 1 holds clear, 2 reveals the scene and 3 darkens it.
/// The signed-halfword ramp in `killCountdown` runs from 0 to 90 in one-unit
/// updates and is scaled to an unsigned colour byte; other states hold it.
/// Requires a live bodyless task, OT tag 5 and a word-aligned primitive arena
/// with `sizeof(TILE) + sizeof(DR_TPAGE)` free bytes. Packets stay live until GPU
/// completion. Subtractive blending and dithering remain active after drawing;
/// displayed-area drawing is disabled. Neither ramp endpoint ends the task.
static void _actor461800SceneFadeTask(Task* task)
{
    switch (task->state) {
        case ACTOR_461800_SCENE_FADE_CLEAR:
            task->killCountdown = 0;
            break;
        case ACTOR_461800_SCENE_FADE_OUT:
            if (task->killCountdown < ACTOR_461800_SCENE_FADE_RAMP_UPDATES) {
                task->killCountdown++;
            }
            break;
        case ACTOR_461800_SCENE_FADE_IN:
            if (task->killCountdown > 0) {
                task->killCountdown--;
            }
            break;
        case ACTOR_461800_SCENE_FADE_OPAQUE:
            task->killCountdown = ACTOR_461800_SCENE_FADE_RAMP_UPDATES;
            break;
    }
    _actor461800QueueSceneFade(task);
}

/// Starts or selects a state of the scene's fade task.
///
/// Any negative state spawns the singleton if it is absent; scripts use
/// `ACTOR_461800_SCENE_EFFECT_START`. A nonnegative state requires that spawn to
/// have succeeded and the published task to remain live. The state is copied
/// unchanged; this callback neither waits for the ramp nor releases the task.
static void _actor461800SetSceneFadeState(s32 state)
{
    if (state < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = taskSpawnFromTable(D_actor_461800_80133EBC, ACTOR_461800_SCENE_EFFECT_FADE, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = state;
    }
}

/// Starts or selects a state of the scene's distortion task.
///
/// Any negative state spawns the singleton if it is absent; scripts use
/// `ACTOR_461800_SCENE_EFFECT_START`. A nonnegative state requires that spawn to
/// have succeeded and the published task to remain live. The state is copied
/// unchanged; this callback neither waits for the ramp nor releases the task.
static void _actor461800SetSceneDistortionState(s32 state)
{
    if (state < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = taskSpawnFromTable(D_actor_461800_80133EBC, ACTOR_461800_SCENE_EFFECT_DISTORTION, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = state;
    }
}

/// Awards rescue bonuses and chooses the scene's ending or Shelter transition.
///
/// Does nothing during attract demo 9. The companion sterilization event and
/// meeting progress award their collected bits independently. With neither the
/// item follow-up nor meeting progress, requests the ending with a 15-frame
/// fade. Otherwise selects Shelter R36, room/warp 1, in the live save destination,
/// starts the session transition and normal load caption, then finishes scene
/// streaming and restores its saved RNG state. Called by normal and skip scripts.
static void _actor461800FinishScene(void)
{
    enum {
        ACTOR_461800_SCENE_DEMO                   = 9,
        ACTOR_461800_STERILIZATION_WITH_COMPANION = 2,
        ACTOR_461800_ENDING_FADE_FRAMES           = 15,
        ACTOR_461800_TRANSITION_TASK_BANK         = 0,
        ACTOR_461800_TRANSITION_TASK_INDEX        = 17,
    };
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_461800_SCENE_DEMO) {
        // Award the rescue bonuses before selecting the ending path.
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == ACTOR_461800_STERILIZATION_WITH_COMPANION) {
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_SOLDIER_RESCUE_BONUS);
        }
        if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) != 0) {
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_PIERCE_RESCUE_BONUS);
        }
        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) == 0 && gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == 0) {
            gGameSession->restartMode     = GAME_SESSION_RESTART_ENDING;
            gGameSession->deathFadeFrames = ACTOR_461800_ENDING_FADE_FRAMES;
            return;
        }
        // Hand the live save destination to the session transition and finish streaming.
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_MINE_SHELTER;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_R36;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
        gDisplayState.spriteVariant                                 = 1;
        taskSpawn(ACTOR_461800_TRANSITION_TASK_BANK, ACTOR_461800_TRANSITION_TASK_INDEX, 0, 0);
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
        streamFinishScene();
    }
}

/// Spawn tick of the first actor variant: allocates the work block, hangs the
/// model off the view, seeds the animation context and starts the two attachment
/// tasks. Each attachment takes its texture page and CLUT row from the nested area
/// record the actor's spawn index selects, and is streamed twice once its aux
/// buffer exists.
static void func_actor_461800_80132390(Enemy* enemy, Task* task)
{
    VECTOR     vec;
    GfxCoord*  coord;
    TmdObject* obj;
    Task*      spawned1;
    Task*      spawned2;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (_gScriptedWalkWork = memCalloc(sizeof(ScriptedWalkAttachmentsWork), false));
    if (_gScriptedWalkWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor461800ExitScriptedWalker;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    obj->lightMtx                    = &_gScriptedWalkWork->light;
    obj->colorMtx                    = &_gScriptedWalkWork->color;
    vec.vx                           = coord->workm.t[0];
    vec.vy                           = coord->workm.t[1] - 0x320;
    D_actor_461800_80143898          = task;
    vec.vz                           = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&_gScriptedWalkWork->rig.anim, D_actor_461800_80139FB0, obj,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = 1;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;

    spawned1 = taskSpawnFromTable(D_actor_461800_80139F8C, 1, 8, 0);
    if (spawned1 != NULL) {
        _gScriptedWalkWork->attachment1 = spawned1;
        actorTintModel(spawned1->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    spawned2 = taskSpawnFromTable(D_actor_461800_80139F8C, 2, 0xC, 0);
    if (spawned2 != NULL) {
        _gScriptedWalkWork->attachment2 = spawned2;
        actorTintModel(spawned2->extra.tmd, (Enemy*)task->spawnArg2.pointer);
    }

    _gScriptedWalkWork->st.travel  = 0;
    _gScriptedWalkWork->turnFrames = 0;
    task->msgTable                 = D_actor_461800_80139F5C;
    _scriptedWalkUpdate(task);
    task->state++;
}

#include "../../shared/scripted_walk_update.inc.c"

/// Two-state dispatcher: publishes the task's work block in
/// `_gScriptedWalkWork` on the way through, then calls the handler its
/// state selects.
void func_actor_461800_801329B0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_461800_80132390,
        _actorRenderWalkerFrame,
    };

    _gScriptedWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
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
#define ACTOR_RENDER_UPDATE_WALKER _scriptedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Begins teardown of the scripted walker and its two attachment tasks.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2.pointer` and a
/// `ScriptedWalkAttachmentsWork` block with two live, non-NULL attachments.
/// The published work and task pointers are left stale. Work is released by
/// `enemyDestroy` before its attachment pointers are read; this retained order
/// depends on those freed bytes remaining readable until consumed.
static void _actor461800ExitScriptedWalker(Task* task)
{
    ScriptedWalkAttachmentsWork* work = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    // Preserve the post-release attachment reads and teardown order.
    taskKill(work->attachment1);
    taskKill(work->attachment2);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Attaches a model root to a borrowed walker part and clears all model flags.
///
/// `attachmentRoot` is the preloaded `attachmentModel->coords` of a live model.
/// `walkerPart` must be live and its parent chain must not reach that root.
/// The root's local transform is retained in the walker's part space; its cached
/// composition is marked stale. Clearing every flag permits active drawing and
/// automatic buffer allocation, retaining any existing primitive buffer.
/// No lighting or teardown ownership is transferred. The borrowed parent must
/// remain live whenever the attachment root is composed.
static __inline__ void _actor461800AttachWalkerModel(TmdObject* attachmentModel, GfxCoord* attachmentRoot, GfxCoord* walkerPart)
{
    attachmentRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    attachmentModel->flags       = 0;
    attachmentRoot->parent       = walkerPart;
}

/// Parents a hand model to the scripted walker and updates its room lighting.
///
/// Requires live TMD models on this task and the published walker task.
/// `spawnArg1.value` is a coordinate index in that walker's twenty-part model;
/// the two hand spawns use 8 and 12. State 0 attaches the root and clears model
/// flags, then enters state 1. State 1 samples all three lights at the walker
/// root's composed world translation, 800 world units above it in negative Y.
/// The attachment borrows the parent coordinate until teardown; transforms must
/// be composed and the lighting helper's scratch/GTE state available.
static void _actor461800ScriptedWalkerAttachmentTask(Task* task)
{
    enum {
        ACTOR_461800_ATTACHMENT_INITIALIZE   = 0,
        ACTOR_461800_ATTACHMENT_LIGHT        = 1,
        ACTOR_461800_ATTACHMENT_LIGHT_HEIGHT = 800,
        ACTOR_461800_ATTACHMENT_LIGHT_COUNT  = 3,
    };
    TmdObject* attachmentModel = task->extra.tmd;
    GfxCoord*  attachmentRoot  = attachmentModel->coords;
    GfxCoord*  walkerCoords    = D_actor_461800_80143898->extra.tmd->coords;
    GfxCoord*  parentPart      = walkerCoords + task->spawnArg1.value;
    VECTOR     lightSample;

    switch (task->state) {
        case ACTOR_461800_ATTACHMENT_INITIALIZE:
            _actor461800AttachWalkerModel(attachmentModel, attachmentRoot, parentPart);
            task->state++;
            break;
        case ACTOR_461800_ATTACHMENT_LIGHT:
            lightSample.vx = walkerCoords->workm.t[0];
            lightSample.vy = walkerCoords->workm.t[1] - ACTOR_461800_ATTACHMENT_LIGHT_HEIGHT;
            lightSample.vz = walkerCoords->workm.t[2];
            worldCoordSetModelLighting(attachmentModel, &lightSample, 0, ACTOR_461800_ATTACHMENT_LIGHT_COUNT);
            break;
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Reseeds the published scripted walker from a borrowed animation request.
///
/// Requires a live walker and initialized rig. Loaded clip keys are 1..3. The
/// signed check rejects IDs >= 6 with -1 but also accepts negative IDs and NULL
/// entries 0/4/5; those are unsafe unless narrowing selects a loaded clip. An
/// accepted ID is narrowed to `s16`. Nonzero blend latches the low signed halfword
/// of `blendFrames` (whole normal-rate frames; 0..2047 keeps playback nonnegative);
/// zero blend restarts and leaves that latch intact. Reseeding runs immediately.
/// Receiver, message ID, second payload and the other request words are ignored.
/// Returns 0 after acceptance and retains no request pointer.
static s32 _actor461800PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    if (request->animationId < (s32)ARRAY_SIZE(D_actor_461800_80139FB0)) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        _scriptedWalkUpdate(D_actor_461800_80143898);
        return 0;
    }
    return -1;
}

/// Replaces the scripted walker and both hand models' draw flags.
///
/// Requires a live published walker, work block and both attachment models.
/// Bit 0 permits active drawing; bit 1 suppresses automatic primitive-buffer
/// allocation. All other model flags are cleared and other request bits ignored.
/// No buffers are allocated or released. Receiver, message ID and second payload
/// are ignored. Returns 0.
static s32 _actor461800SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* walkerModel      = D_actor_461800_80143898->extra.tmd;
    TmdObject* attachment1Model = _gScriptedWalkWork->attachment1->extra.tmd;
    TmdObject* attachment2Model = _gScriptedWalkWork->attachment2->extra.tmd;

    if (drawFlags & ACTOR_461800_WALKER_DRAW_SHOW) {
        walkerModel->flags      = 0;
        attachment1Model->flags = 0;
        attachment2Model->flags = 0;
    } else {
        walkerModel->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment1Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment2Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_461800_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment1Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment2Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/scripted_walk_place.inc.c"

/// Schedules twenty turning updates for the published scripted walker.
///
/// Requires live published work and a command borrowed through dispatch.
/// Command 0 arms the countdown consumed while turn clip 3 plays; it does not
/// select the clip. Other commands do nothing. Context tags, receiver, message
/// ID and second payload are ignored. Returns 0.
static s32 _actor461800ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    if (request->command == ACTOR_461800_WALKER_COMMAND_TURN) {
        _gScriptedWalkWork->turnFrames = ACTOR_461800_WALKER_TURN_UPDATES;
    }
    return 0;
}

#include "../../shared/scripted_walk_to.inc.c"

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// Dispatches the scene's footstep walker through initialization or its frame update.
///
/// `task` must own a live nineteen-part TMD model and an `Enemy` in
/// `spawnArg2.pointer`. State 0 requires no existing work allocation, allocates
/// and publishes `FootstepWalkWork`, binds playback and messages, and enters
/// state 1; allocation failure tears it down.
/// State 1 requires that initialized work and refreshes lighting, movement,
/// animation and the ground shadow. Other state indices are out of bounds.
/// Work is published before dispatch for singleton animation and message handlers;
/// it remains borrowed from the task and must not be used after teardown.
/// Required model, clip, room-light, scratch and GTE state must remain available
/// during the selected handler. Initialization may invalidate the task or enemy;
/// neither is accessed after dispatch.
static void _actor461800FootstepWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _footstepWalkSpawn,
        _actorRenderWalkerFrameSecond,
    };

    _gFootstepWalkWork = task->work;
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
#define ACTOR_RENDER_UPDATE_WALKER             _footstepWalkUpdate
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy and begins teardown of its task and model.
///
/// `task` must be live with its owned `Enemy` in `spawnArg2.pointer`.
/// Enemy storage is invalid on return; task work and the model follow
/// `taskKill`'s immediate/deferred release rules. Do not use the task afterwards.
static void _footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_play_steps.inc.c"

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_reset_anim.inc.c"

#include "../../shared/footstep_walk_blend_anim.inc.c"

#include "../../shared/footstep_walk_play.inc.c"

/// Replaces the published footstep walker's model draw flags.
///
/// Requires a live TMD model in `gFootstepWalkTask`.
/// Bit 0 permits active drawing; bit 1 suppresses automatic primitive-buffer
/// allocation. All other model flags are cleared and other request bits ignored.
/// No buffers are allocated or released. Receiver, message ID and second payload
/// are ignored. Returns 0.
static s32 _actor461800SetFootstepWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* model;

    model = gFootstepWalkTask->extra.tmd;
    if (drawFlags & ACTOR_461800_WALKER_DRAW_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_461800_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Applies scheduled turning or footstep enablement to the published sound walker.
///
/// Requires live `_gFootstepWalkWork` and a command borrowed through dispatch.
/// Command 0 schedules twenty updates while turn clip 3 plays, without selecting
/// that clip. Command 1 enables footstep sounds until work teardown; other
/// commands do nothing. Context tags, receiver, message ID and second payload
/// are ignored. Returns 0.
static s32 _actor461800ApplyFootstepWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    s32 commandId;

    commandId = request->command;
    switch (commandId) {
        case ACTOR_461800_WALKER_COMMAND_TURN:
            _gFootstepWalkWork->turnFrames = ACTOR_461800_WALKER_TURN_UPDATES;
            break;
        case ACTOR_461800_WALKER_COMMAND_ENABLE_FOOTSTEPS:
            _gFootstepWalkWork->playFootsteps = commandId;
            break;
    }
    return 0;
}

#include "../../shared/footstep_walk_to.inc.c"

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW
