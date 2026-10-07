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
/// Selects the scripted walker's writable signed-halfword approach mode.
///
/// Only element zero is a mode; the remaining halfword has an unproven role.
#define SCRIPTED_WALK_MODE gScriptedWalkModeValue
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

static s16 _gScriptedWalkModeStorage[2];

/// Signed-halfword approach mode at the start of the scripted walker's storage.
///
/// Values are `SCRIPTED_WALK_MODE_*`. The scalar view retains the access
/// shape required by the walk-to handler; the trailing halfword is not read.
extern s16 gScriptedWalkModeValue __asm__("_gScriptedWalkModeStorage");

static ScriptedWalkAttachmentsWork* _gScriptedWalkWork;

/// The task the first variant's work block above belongs to, published by
/// `func_actor_461800_80132390` alongside it.
extern Task* D_actor_461800_80143898;

extern FootstepWalkWork* gFootstepWalkWork;

/// The second variant's task, published by its spawn routine
/// `footstepWalkSpawn` so the handlers can reach its model.
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

/// Reset argument the second variant forwards to every reseeded slot.
extern s16 gFootstepWalkBlendFrames;

/// Approach mode the last `_footstepWalkSetWalkTarget` call selected.
extern s16 gFootstepWalkMode;

static void func_actor_461800_80132A0C(Enemy* enemy, Task* task);
static void func_actor_461800_80132A90(Task* task);
static void func_actor_461800_801335B0(Enemy* enemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

s32  func_actor_461800_80132D84(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_461800_80132E14(Task*, s32, s32, s32);
s32  func_actor_461800_80132F20(Task* task, s32 msgId, ActorCommand* request, s32);
s32  func_actor_461800_80133928(Task*, s32, s32, s32);
s32  func_actor_461800_801339EC(Task* task, s32 msgId, ActorCommand* msg, s32);
void func_actor_461800_801329B0(Task*);
void func_actor_461800_80132B74(Task*);
void func_actor_461800_80133554(Task*);

void func_actor_461800_80131E38(Task*);
void func_actor_461800_80132048(Task*);
void func_actor_461800_801321DC(s32);
void func_actor_461800_8013223C(s32);
void func_actor_461800_8013229C(void);

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
    { { { TASK_BODY_NONE, 32 } }, func_actor_461800_80132048, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_actor_461800_80131E38, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_8013223C }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_461800_80133EEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_461800_80133F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_8013223C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 55 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_8013223C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_461800_80133F18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_461800_801321DC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_461800_8013229C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0xF4240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_461800_80134470[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_461800_8013229C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_461800_80132D84 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_461800_80132E14 },
    { ACTOR_MESSAGE_PLACE, scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_461800_80132F20 },
    { ACTOR_MESSAGE_WALK_TO, scriptedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_461800_80139F8C[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_801329B0, { .model = &_gActor461800KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_80132B74, { .model = &_gActor461800HandLeft } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_461800_80132B74, { .model = &_gActor461800HandRight } },
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

s16 gFootstepWalkBlendFrames = 8;

TaskMessageEntry gFootstepWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_461800_80133928 },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_461800_801339EC },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_461800_801437EC = { { { TASK_BODY_TMD, 192 } }, func_actor_461800_80133554, { .model = &_gActor461800AyaBreaBody } };

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

/// Halfword storage containing the scripted walker's approach mode.
///
/// Element zero is the writable signed mode selected by the walk-to message
/// (`SCRIPTED_WALK_MODE_*`). Element one is never accessed; its role is
/// unproven. Keep both halfwords, including the second's original contents.
static s16 _gScriptedWalkModeStorage[2] = {
    0,
    -0x3658,
};

FootstepWalkWork* gFootstepWalkWork;

Task* gFootstepWalkTask;

s16 gFootstepWalkMode;

static void func_actor_461800_80132390(Enemy* enemy, Task* task);

void func_actor_461800_80131E38(Task* task)
{
    switch (task->state) {
        case 0:
            D_actor_461800_80143884 = 0x10;
            break;
        case 1:
            if (D_actor_461800_80143888 != 0) {
                if (D_actor_461800_80143884 < 0x10) {
                    D_actor_461800_80143884++;
                } else {
                    D_actor_461800_80143888 = 0;
                }
            } else if (D_actor_461800_80143884 > 0) {
                D_actor_461800_80143884--;
            } else {
                D_actor_461800_80143888 = 1;
            }
            if (D_actor_461800_8014388C != 0) {
                if (D_actor_461800_80143890 < 0x13) {
                    D_actor_461800_80143890++;
                } else {
                    D_actor_461800_8014388C = 0;
                }
            } else if (D_actor_461800_80143890 > 0) {
                D_actor_461800_80143890--;
            } else {
                D_actor_461800_8014388C = 1;
            }
            break;
        case 2:
            if (D_actor_461800_80143884 < 0x10) {
                D_actor_461800_80143884++;
            }
            if (D_actor_461800_80143890 < 0x13) {
                D_actor_461800_80143890++;
            }
            if (D_actor_461800_80143884 == 0x10 && D_actor_461800_80143890 == 0x13) {
                task->state = 3;
            }
            break;
        case 3:
            if (!(task->killCountdown & 3)) {
                if (D_actor_461800_80143884 > 0) {
                    D_actor_461800_80143884--;
                }
                if (D_actor_461800_80143890 > 0) {
                    D_actor_461800_80143890--;
                }
            }
            break;
        case 4:
            displaySetShakeY(0);
            D_neo_ark_r31_8017DC54 = -1;
            return;
    }
    task->killCountdown++;
    D_neo_ark_r31_8017DC54 = D_actor_461800_80143884 / 3 + 3;
}

/// Full-screen fade overlay: `Task::state` picks the ramp (0 snaps it to 90,
/// 1 clears it, 2 counts down, 3 counts up) held in `Task::killCountdown`, which
/// then scales a grey semi-transparent `TILE` linked with its `DR_TPAGE` into
/// OT slot 5.
void func_actor_461800_80132048(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        c;

    switch (task->state) {
        case 1:
            task->killCountdown = 0;
            break;
        case 3:
            if (task->killCountdown < 90) {
                task->killCountdown++;
            }
            break;
        case 2:
            if (task->killCountdown > 0) {
                task->killCountdown--;
            }
            break;
        case 0:
            task->killCountdown = 90;
            break;
    }
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    c              = (task->killCountdown * 0xFF) / 90;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    tile->y0 = -0x80;
    tile->w  = 0x140;
    tile->h  = 0x100;
    tile->r0 = c;
    tile->g0 = c;
    tile->b0 = c;
    addPrim(gGpuCurrentOt + 5, tile);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
    addPrim(gGpuCurrentOt + 5, dr);
}

void func_actor_461800_801321DC(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = taskSpawnFromTable(D_actor_461800_80133EBC, 0, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = arg0;
    }
}

void func_actor_461800_8013223C(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = taskSpawnFromTable(D_actor_461800_80133EBC, 1, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = arg0;
    }
}

/// Exit path taken when the player leaves through this actor: two flag awards
/// first, then one of two endings depending on whether the two event flags have
/// been seen. With neither seen the session bails out (`restartMode` / `deathFadeFrames`
/// are the stage-load sentinels); otherwise the save header is primed and the
/// boot loader started, with the stream RNG restored behind it. Skipped whole
/// when `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` (the current screen id) is 9.
void func_actor_461800_8013229C(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == 2) {
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_SOLDIER_RESCUE_BONUS);
        }
        if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) != 0) {
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_PIERCE_RESCUE_BONUS);
        }
        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) == 0 && gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == 0) {
            gGameSession->restartMode     = GAME_SESSION_RESTART_ENDING;
            gGameSession->deathFadeFrames = 0xF;
            return;
        }
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_MINE_SHELTER;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_R36;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
        gDisplayState.spriteVariant                                 = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad((u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 0);
        Gp_RestoreStreamRng();
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
    task->exitCallback               = func_actor_461800_80132A90;
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
    scriptedWalkUpdate(task);
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
        func_actor_461800_80132A0C,
    };

    _gScriptedWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_461800_80132A0C
#define walkerUpdate     scriptedWalkUpdate
#define walkerDrawShadow _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// `Task::exitCallback` of the first variant: hands the task's `Enemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `enemyDestroy`, then kills the two attachment tasks the spawn routine
/// started.
static void func_actor_461800_80132A90(Task* task)
{
    ScriptedWalkAttachmentsWork* work = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
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

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame off the actor's part `spawnArg1` and steps to state 1, and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `worldCoordSetModelLighting` for the part colour matrix.
void func_actor_461800_80132B74(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = D_actor_461800_80143898->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            extra->flags        = 0;
            coord->parent       = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            worldCoordSetModelLighting(extra, &vec, 0, 3);
            break;
    }
}

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

/// Applies an animation preset: the id is copied into the work block, the reset
/// mode is picked by the preset's blend flag and the blend duration is latched
/// from the preset only for a blended reseed, then the child slots are re-seeded.
/// Only the six known animation ids are accepted; anything else leaves the work
/// block untouched and reports the failure.
s32 func_actor_461800_80132D84(Task* task, s32 arg1, AnimationPlayRequest* preset, s32 arg3)
{
    if (preset->animationId < 6) {
        _gScriptedWalkWork->st.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = preset->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        scriptedWalkUpdate(D_actor_461800_80143898);
        return 0;
    }
    return -1;
}

/// Sets `TmdObject.flags` on the three model objects this actor owns:
/// the one on its own task and the two attachment tasks' models in the work block.
/// `arg2 & 1` shows them (flags 0); otherwise each gets `TMD_OBJECT_SKIP_ACTIVE_DRAW`.
/// `arg2 & 2` also sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on each. These are object
/// flags, not `tmdCreateModel`'s buffer-flag argument.
s32 func_actor_461800_80132E14(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* own    = D_actor_461800_80143898->extra.tmd;
    TmdObject* first  = _gScriptedWalkWork->attachment1->extra.tmd;
    TmdObject* second = _gScriptedWalkWork->attachment2->extra.tmd;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        first->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        second->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        own->flags    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        first->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        second->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/scripted_walk_place.inc.c"

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    if (request->command == 0) {
        _gScriptedWalkWork->turnFrames = 0x14;
    }
    return 0;
}

#include "../../shared/scripted_walk_to.inc.c"

#include "../../shared/footstep_walk_spawn.inc.c"

#include "../../shared/footstep_walk_update.inc.c"

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `gFootstepWalkWork` on the way through so the
/// rest of the overlay can reach it without the task.
void func_actor_461800_80133554(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        footstepWalkSpawn,
        func_actor_461800_801335B0,
    };

    gFootstepWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_461800_801335B0
#define walkerUpdate     _footstepWalkUpdate
#define walkerDrawShadow _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

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

/// Visibility message of the second variant: applies `arg2` to the model of
/// the task published in `gFootstepWalkTask` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_461800_80133928(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = gFootstepWalkTask->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (arg2 & 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Message handler: the message id selects how the second work block is
/// reseeded -- 0 arms the reset argument, 1 remembers the id in the byte the
/// seeding loop reads. Anything else does nothing.
s32 func_actor_461800_801339EC(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    s32 id;

    id = msg->command;
    switch (id) {
        case 0:
            gFootstepWalkWork->turnFrames = 0x14;
            break;
        case 1:
            gFootstepWalkWork->playFootsteps = id;
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
