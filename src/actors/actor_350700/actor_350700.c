#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"
#include "../../shared/model_placement.h"
#include "../../shared/reversing_walker.h"

/// Animation bank tables of the enemy actor and of the parent block.
extern AnimationSet*  D_actor_350700_80169CF8[5];
extern AnimationSet** gActorMotionAnimBanks19[1];
extern AnimationSet*  D_actor_350700_801708C0[6];
extern AnimationSet** gActorMotionAnimBanks[1];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `reverseWalkSpawn`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gReverseWalkMessages[];

/// The `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks from,
/// and the message table it points the parent's `Task::msgTable` at: ids
/// 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `actorMotionPlayAnim`, terminated by `TASK_MESSAGE_TABLE_END`.
extern TaskDesc         D_actor_350700_801708DC[];
extern TaskMessageEntry D_actor_350700_8017090C[];

static void func_actor_350700_80162B30(Task* arg0);
static void func_actor_350700_80162D5C(Task* arg0);
static void func_actor_350700_80163348(Task* task);
static void func_actor_350700_801633BC(Task* arg0);
static void func_actor_350700_801633DC(Task* task);
static void func_actor_350700_801633F8(Task* arg0);
static void func_actor_350700_80163400(Task* task);
static void func_actor_350700_80163528(Task* task);

/// Spawn, tick and exit handlers of the enemy actor, dispatched by
/// `func_actor_350700_80162398`.
static const TaskFuncTable3 D_actor_350700_80161E24 = { {
    reverseWalkSpawn,
    reverseWalkUpdate,
    reverseWalkExit,
} };

/// Tick handlers of the enemy actor, indexed by `ReverseWalkWork::walk.motionStep`:
/// turn to face `target`, start moving, approach until arrival, then turn to
/// the placement yaw.
static const TaskFuncTable4 D_actor_350700_80161E30 = { {
    reverseWalkFaceTarget,
    reverseWalkBeginMove,
    actorMotionArrive19,
    reverseWalkTurnToYaw,
} };

/// The constant local-space offset `reverseWalkBeginMove` rotates:
/// straight ahead along the part's own +Z.
static const VECTOR _gReverseWalkForward = { 0, 0, 0x200000, 0 };

/// Spawn, tick and exit handlers of the child part tasks, dispatched by
/// `func_actor_350700_80163274`.
static const TaskFuncTable3 D_actor_350700_80161E50 = { {
    modelPlacementAttachPart,
    func_actor_350700_80163348,
    taskKill,
} };

/// Spawn, tick and exit handlers of the parent actor, dispatched by
/// `func_actor_350700_80163350`.
static const TaskFuncTable3 D_actor_350700_80161E5C = { {
    func_actor_350700_80162B30,
    func_actor_350700_80162D5C,
    func_actor_350700_801633BC,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_350700_80161E68 = { {
    actorMotionFaceTarget,
    func_actor_350700_80163528,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The parent's copy of the forward offset, rotated by
/// `func_actor_350700_80163528`.
static const VECTOR D_actor_350700_80161E78 = { 0, 0, 0x200000, 0 };

static TmdSource _gActor350700KyleMadiganBody;
static TmdSource _gActor350700KyleMadiganHandRight;
static TmdSource _gActor350700KyleMadiganHandLeft;
static TmdSource _gActor350700KyleMadiganGun;
s32              func_actor_350700_801637C4(Task* task, s32 msgId, ActorTransform* args, s32 arg3);
s32              func_actor_350700_80163840(Task*, s32, s32, s32);
s32              func_actor_350700_8016395C(Task*, s32, s32, s32);
void             func_actor_350700_80163274(Task*);
void             func_actor_350700_80163350(Task*);

s32  func_actor_350700_80162AF4(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void func_actor_350700_80162398(Task*);

static TmdBone _gActor350700EveBreaMaskedBodySkeleton[19] = {
#include "assets/eve_brea_masked_body_skeleton.inc"
};

static u32 _gActor350700EveBreaMaskedBodyPartVerts[19] = {
#include "assets/eve_brea_masked_body_partVerts.inc"
};

static SVECTOR _gActor350700EveBreaMaskedBodyVerts[312] = {
#include "assets/eve_brea_masked_body_verts.inc"
};

static SVECTOR _gActor350700EveBreaMaskedBodyNormals[338] = {
#include "assets/eve_brea_masked_body_normals.inc"
};

static u32 _gActor350700EveBreaMaskedBodyStream[3463] = {
#include "assets/eve_brea_masked_body_stream.inc"
};

static TmdSource _gActor350700EveBreaMaskedBody = {
    0,
    18392,
    6232,
    19,
    _gActor350700EveBreaMaskedBodyPartVerts,
    _gActor350700EveBreaMaskedBodyVerts,
    _gActor350700EveBreaMaskedBodyNormals,
    _gActor350700EveBreaMaskedBodySkeleton,
    _gActor350700EveBreaMaskedBodyStream,
};

static AnimationPackedPose _gActor350700Animation06AB8Bank1[2] = {
#include "assets/actor_350700_animation_06AB8_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation06AB8Bank4[23] = {
#include "assets/actor_350700_animation_06AB8_bank4.inc"
};

static AnimationRecord _gActor350700Animation06AB8Records[84] = {
#include "assets/actor_350700_animation_06AB8_records.inc"
};

static u16 _gActor350700Animation06AB8Indices[20] = {
#include "assets/actor_350700_animation_06AB8_indices.inc"
};

static AnimationSet _gActor350700Animation06AB8 = {
    _gActor350700Animation06AB8Records,
    _gActor350700Animation06AB8Indices,
    { NULL, _gActor350700Animation06AB8Bank1, NULL, NULL, _gActor350700Animation06AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation0769CBank1[22] = {
#include "assets/actor_350700_animation_0769C_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation0769CBank4[298] = {
#include "assets/actor_350700_animation_0769C_bank4.inc"
};

static AnimationRecord _gActor350700Animation0769CRecords[377] = {
#include "assets/actor_350700_animation_0769C_records.inc"
};

static u16 _gActor350700Animation0769CIndices[20] = {
#include "assets/actor_350700_animation_0769C_indices.inc"
};

static AnimationSet _gActor350700Animation0769C = {
    _gActor350700Animation0769CRecords,
    _gActor350700Animation0769CIndices,
    { NULL, _gActor350700Animation0769CBank1, NULL, NULL, _gActor350700Animation0769CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation07AA4Bank1[5] = {
#include "assets/actor_350700_animation_07AA4_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation07AA4Bank4[76] = {
#include "assets/actor_350700_animation_07AA4_bank4.inc"
};

static AnimationRecord _gActor350700Animation07AA4Records[147] = {
#include "assets/actor_350700_animation_07AA4_records.inc"
};

static u16 _gActor350700Animation07AA4Indices[20] = {
#include "assets/actor_350700_animation_07AA4_indices.inc"
};

static AnimationSet _gActor350700Animation07AA4 = {
    _gActor350700Animation07AA4Records,
    _gActor350700Animation07AA4Indices,
    { NULL, _gActor350700Animation07AA4Bank1, NULL, NULL, _gActor350700Animation07AA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation07EB0Bank1[5] = {
#include "assets/actor_350700_animation_07EB0_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation07EB0Bank4[89] = {
#include "assets/actor_350700_animation_07EB0_bank4.inc"
};

static AnimationRecord _gActor350700Animation07EB0Records[135] = {
#include "assets/actor_350700_animation_07EB0_records.inc"
};

static u16 _gActor350700Animation07EB0Indices[20] = {
#include "assets/actor_350700_animation_07EB0_indices.inc"
};

static AnimationSet _gActor350700Animation07EB0 = {
    _gActor350700Animation07EB0Records,
    _gActor350700Animation07EB0Indices,
    { NULL, _gActor350700Animation07EB0Bank1, NULL, NULL, _gActor350700Animation07EB0Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_350700_80169CF8[5] = {
    NULL,
    &_gActor350700Animation06AB8,
    &_gActor350700Animation0769C,
    &_gActor350700Animation07AA4,
    &_gActor350700Animation07EB0,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_350700_80169CF8,
};

TaskDesc D_actor_350700_80169D10 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_350700_80162398, { .model = &_gActor350700EveBreaMaskedBody } };

TaskMessageEntry gReverseWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, reverseWalkVisibilityMsg },
    { ACTOR_MESSAGE_WALK_TO, reverseWalkStartMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_350700_80162AF4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gActor350700KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor350700KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor350700KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor350700KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor350700KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor350700KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor350700KyleMadiganBodyPartVerts,
    _gActor350700KyleMadiganBodyVerts,
    _gActor350700KyleMadiganBodyNormals,
    _gActor350700KyleMadiganBodySkeleton,
    _gActor350700KyleMadiganBodyStream,
};

static TmdBone _gActor350700KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gActor350700KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gActor350700KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gActor350700KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gActor350700KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

static TmdSource _gActor350700KyleMadiganHandRight = {
    0,
    1148,
    0,
    1,
    _gActor350700KyleMadiganHandRightPartVerts,
    _gActor350700KyleMadiganHandRightVerts,
    _gActor350700KyleMadiganHandRightNormals,
    _gActor350700KyleMadiganHandRightSkeleton,
    _gActor350700KyleMadiganHandRightStream,
};

static TmdBone _gActor350700KyleMadiganHandLeftSkeleton[1] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

static u32 _gActor350700KyleMadiganHandLeftPartVerts[1] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

static SVECTOR _gActor350700KyleMadiganHandLeftVerts[27] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

static SVECTOR _gActor350700KyleMadiganHandLeftNormals[27] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

static u32 _gActor350700KyleMadiganHandLeftStream[189] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

static TmdSource _gActor350700KyleMadiganHandLeft = {
    0,
    1328,
    0,
    1,
    _gActor350700KyleMadiganHandLeftPartVerts,
    _gActor350700KyleMadiganHandLeftVerts,
    _gActor350700KyleMadiganHandLeftNormals,
    _gActor350700KyleMadiganHandLeftSkeleton,
    _gActor350700KyleMadiganHandLeftStream,
};

static TmdBone _gActor350700KyleMadiganGunSkeleton[1] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

static u32 _gActor350700KyleMadiganGunPartVerts[1] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

static SVECTOR _gActor350700KyleMadiganGunVerts[22] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

static SVECTOR _gActor350700KyleMadiganGunNormals[24] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

static u32 _gActor350700KyleMadiganGunStream[162] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

static TmdSource _gActor350700KyleMadiganGun = {
    0,
    1108,
    0,
    1,
    _gActor350700KyleMadiganGunPartVerts,
    _gActor350700KyleMadiganGunVerts,
    _gActor350700KyleMadiganGunNormals,
    _gActor350700KyleMadiganGunSkeleton,
    _gActor350700KyleMadiganGunStream,
};

static AnimationPackedPose _gActor350700Animation0DA4CBank1[2] = {
#include "assets/actor_350700_animation_0DA4C_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation0DA4CBank4[32] = {
#include "assets/actor_350700_animation_0DA4C_bank4.inc"
};

static AnimationRecord _gActor350700Animation0DA4CRecords[101] = {
#include "assets/actor_350700_animation_0DA4C_records.inc"
};

static u16 _gActor350700Animation0DA4CIndices[20] = {
#include "assets/actor_350700_animation_0DA4C_indices.inc"
};

static AnimationSet _gActor350700Animation0DA4C = {
    _gActor350700Animation0DA4CRecords,
    _gActor350700Animation0DA4CIndices,
    { NULL, _gActor350700Animation0DA4CBank1, NULL, NULL, _gActor350700Animation0DA4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation0DD04Bank1[3] = {
#include "assets/actor_350700_animation_0DD04_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation0DD04Bank4[29] = {
#include "assets/actor_350700_animation_0DD04_bank4.inc"
};

static AnimationRecord _gActor350700Animation0DD04Records[116] = {
#include "assets/actor_350700_animation_0DD04_records.inc"
};

static u16 _gActor350700Animation0DD04Indices[20] = {
#include "assets/actor_350700_animation_0DD04_indices.inc"
};

static AnimationSet _gActor350700Animation0DD04 = {
    _gActor350700Animation0DD04Records,
    _gActor350700Animation0DD04Indices,
    { NULL, _gActor350700Animation0DD04Bank1, NULL, NULL, _gActor350700Animation0DD04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation0E08CBank1[3] = {
#include "assets/actor_350700_animation_0E08C_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation0E08CBank4[55] = {
#include "assets/actor_350700_animation_0E08C_bank4.inc"
};

static AnimationRecord _gActor350700Animation0E08CRecords[142] = {
#include "assets/actor_350700_animation_0E08C_records.inc"
};

static u16 _gActor350700Animation0E08CIndices[20] = {
#include "assets/actor_350700_animation_0E08C_indices.inc"
};

static AnimationSet _gActor350700Animation0E08C = {
    _gActor350700Animation0E08CRecords,
    _gActor350700Animation0E08CIndices,
    { NULL, _gActor350700Animation0E08CBank1, NULL, NULL, _gActor350700Animation0E08CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation0E284Bank1[2] = {
#include "assets/actor_350700_animation_0E284_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation0E284Bank4[33] = {
#include "assets/actor_350700_animation_0E284_bank4.inc"
};

static AnimationRecord _gActor350700Animation0E284Records[67] = {
#include "assets/actor_350700_animation_0E284_records.inc"
};

static u16 _gActor350700Animation0E284Indices[20] = {
#include "assets/actor_350700_animation_0E284_indices.inc"
};

static AnimationSet _gActor350700Animation0E284 = {
    _gActor350700Animation0E284Records,
    _gActor350700Animation0E284Indices,
    { NULL, _gActor350700Animation0E284Bank1, NULL, NULL, _gActor350700Animation0E284Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor350700Animation0EA78Bank1[20] = {
#include "assets/actor_350700_animation_0EA78_bank1.inc"
};

static AnimationPackedRotation _gActor350700Animation0EA78Bank4[164] = {
#include "assets/actor_350700_animation_0EA78_bank4.inc"
};

static AnimationRecord _gActor350700Animation0EA78Records[265] = {
#include "assets/actor_350700_animation_0EA78_records.inc"
};

static u16 _gActor350700Animation0EA78Indices[20] = {
#include "assets/actor_350700_animation_0EA78_indices.inc"
};

static AnimationSet _gActor350700Animation0EA78 = {
    _gActor350700Animation0EA78Records,
    _gActor350700Animation0EA78Indices,
    { NULL, _gActor350700Animation0EA78Bank1, NULL, NULL, _gActor350700Animation0EA78Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_350700_801708C0[6] = {
    NULL,
    &_gActor350700Animation0DA4C,
    &_gActor350700Animation0DD04,
    &_gActor350700Animation0E08C,
    &_gActor350700Animation0E284,
    &_gActor350700Animation0EA78,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_350700_801708C0,
};

TaskDesc D_actor_350700_801708DC[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_350700_80163350, { .model = &_gActor350700KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_350700_80163274, { .model = &_gActor350700KyleMadiganHandLeft } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_350700_80163274, { .model = &_gActor350700KyleMadiganHandRight } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_350700_80163274, { .model = &_gActor350700KyleMadiganGun } },
};

TaskMessageEntry D_actor_350700_8017090C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, func_actor_350700_801637C4 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_350700_80163840 },
    { ACTOR_MESSAGE_WALK_TO, actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_350700_8016395C },
    { TASK_MESSAGE_TABLE_END, NULL },
}; /// Per-frame tick of the enemy actor: dispatches through the local two-entry table
#include "../../shared/reversing_walker_update.inc.c"

#include "../../shared/actor_motion_arrive19.inc.c"

#include "../../shared/reversing_walker_start.inc.c"

/// Per-frame dispatcher of the enemy actor: runs its spawn, tick or exit state
/// from `D_actor_350700_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350700_80162398(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

#include "../../shared/reversing_walker_spawn.inc.c"

/// Exit callback `reverseWalkSpawn` installs; tears the task down.
void reverseWalkExit(Task* arg0)
{
    enemyTaskExit(arg0);
}

/// Republishes the enemy work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the actor draws with its own
/// lighting.
void reverseWalkBindLighting(Task* arg0)
{
    TmdObject*       ext;
    ReverseWalkWork* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Tick handler 0 of the enemy actor, selected by `walk.motion`: idle.
void reverseWalkIdle(Task* arg0)
{
}

/// Tick handler 1 of the enemy actor: runs the state handler of
/// `D_actor_350700_80161E30` that `walk.motionStep` selects.
void reverseWalkRunStep(Task* arg0)
{
    TaskFuncTable4   handlers;
    ReverseWalkWork* work;

    work     = arg0->work;
    handlers = D_actor_350700_80161E30;
    handlers.funcs[work->walk.motionStep](arg0);
}

#include "../../shared/reversing_walker_face.inc.c"

#include "../../shared/reversing_walker_move.inc.c"

#include "../../shared/reversing_walker_turn.inc.c"

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/reversing_walker_visibility.inc.c"

/// `taskMessageDispatch` handler: latches the walk direction the message's
/// command selects into `walksForward` -- 1 clears it, so the walker backs
/// toward its targets, 2 sets it, anything else leaves it. Always returns 0.
s32 func_actor_350700_80162AF4(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    ReverseWalkWork* work;

    work = task->work;
    switch (msg->command) {
        case 1:
            work->walksForward = 0;
            break;
        case 2:
            work->walksForward = 1;
            break;
    }
    return 0;
}

/// The parent's spawn handler. Allocates the 0x50C `KyleMadiganWalkerWork` block, seeds it, and spawns the
/// three children `D_actor_350700_801708DC` holds -- table entries 1, 2 and 3 --
/// parking them at `handTasks` and `heldItemTask`. The first two are
/// models: each has `TmdObject::texturePageOffset` / `clutRowOffset` loaded with the texture
/// page and CLUT row of the `AreaPlacement` that entry selects, reached through
/// the area key `&gGameSession->location.loc` and indexed by the model id the child's
/// own `spawnArg2` carries at `Enemy::placeKey >> ENEMY_PLACE_INDEX_SHIFT`, and each then has its
/// texture stream processed twice when it has an aux buffer. The body ends by
/// handing the parent to `func_actor_350700_801633DC`, pointing `msgTable` at the
/// message table and installing `func_actor_350700_801633BC` as its exit
/// callback.
static void func_actor_350700_80162B30(Task* arg0)
{
    KyleMadiganWalkerWork* work;
    GameLocationKey        key;
    GameLocationKey*       sessionKey;
    GameLocationKey*       keyAddr;
    Task*                  spawned;

    work = memCalloc(sizeof(KyleMadiganWalkerWork), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }
    arg0->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    spawned                  = Task_SpawnFromTable(D_actor_350700_801708DC, 1, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        work->handTasks[0] = spawned;
        model              = spawned->extra.tmd;
        idx                = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey         = &gGameSession->location.loc;
        key.stage          = sessionKey->stage;
        key.area           = sessionKey->area;
        key.room           = sessionKey->room;
        key.view           = sessionKey->view;
        areaSyncLocationVariant(&key);
        layout                   = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(D_actor_350700_801708DC, 2, 0xC, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        AreaVariant*   layout;
        AreaPlacement* place;
        s32            idx;

        work->handTasks[1] = spawned;
        model              = spawned->extra.tmd;
        idx                = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        // Keep this block's key address separate across the spawn calls.
        sessionKey = (keyAddr = &gGameSession->location.loc);
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = keyAddr->room;
        key.view   = gGameSession->location.loc.view;
        areaSyncLocationVariant(&key);
        layout                   = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(D_actor_350700_801708DC, 3, 8, arg0);
    if (spawned != NULL) {
        work->heldItemTask = spawned;
    }
    func_actor_350700_801633DC(arg0);
    arg0->msgTable     = D_actor_350700_8017090C;
    arg0->exitCallback = func_actor_350700_801633BC;
    arg0->state       += 1;
}

/// Per-frame tick of the parent actor: dispatches through the local two-entry
/// table `walk.motion` indexes -- the empty `func_actor_350700_801633F8` or the
/// step dispatcher `func_actor_350700_80163400` -- then integrates the
/// per-frame deltas in `walk.velocity` into the 16.16 accumulators `walk.carry`,
/// adds their high halves to the root coordinate's translation, clears `composeStamp`
/// and truncates the accumulators back to 16 bits. Ticks the animation slots
/// while `model.ticking` is set; and, unless the display object's `flags` carry
/// 0x80, draws the ground-shadow quad from the second part's world matrix.
/// While `gGameSession->viewReady` is set it also clears that part's `composeStamp`,
/// rebuilds its coordinate and rebuilds the actor colour; the colour rebuild
/// runs once more unconditionally. The `freeCountdown` countdown then runs while it
/// is non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
static void func_actor_350700_80162D5C(Task* arg0)
{
    TmdObject*             ext      = arg0->extra.tmd;
    KyleMadiganWalkerWork* work     = arg0->work;
    TaskFunc               funcs[2] = { func_actor_350700_801633F8, func_actor_350700_80163400 };
    VECTOR3                pos;
    GfxCoord*              coord;
    s32                    i;

    funcs[work->walk.motion](arg0);
    coord                     = arg0->extra.tmd->coords;
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    coord->coord.t[0]        += work->walk.carry[0].halves.integer;
    coord->coord.t[1]        += work->walk.carry[1].halves.integer;
    coord->coord.t[2]        += work->walk.carry[2].halves.integer;
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    }
    func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// State dispatcher of the child part tasks: copies the three-handler table
/// `D_actor_350700_80161E50` onto the stack and runs the entry `Task::state`
/// selects.
void func_actor_350700_80163274(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E50;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Tick state of the child part tasks: nothing to do, the parent drives them.
static void func_actor_350700_80163348(Task* task)
{
}

/// Per-frame dispatcher of the parent actor: runs its spawn, tick or exit
/// state from `D_actor_350700_80161E5C`, skipping the frame while the global
/// freeze byte is set.
void func_actor_350700_80163350(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E5C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback `func_actor_350700_80162B30` installs, the same
/// `enemyTaskExit` teardown `reverseWalkExit` performs.
static void func_actor_350700_801633BC(Task* arg0)
{
    enemyTaskExit(arg0);
}

/// Republishes the parent work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the parent draws with its own
/// lighting.
static void func_actor_350700_801633DC(Task* task)
{
    TmdObject*             ext;
    KyleMadiganWalkerWork* work;

    ext           = task->extra.tmd;
    work          = task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// The empty first entry of the parent's two-handler table, selected by
/// `KyleMadiganWalkerWork::walk.motion` -- the idle half of the pair whose other
/// entry is the step dispatcher `func_actor_350700_80163400`.
static void func_actor_350700_801633F8(Task* arg0)
{
}

/// Motion handler 1 of the parent block: copies the step-handler table
/// `D_actor_350700_80161E68` onto the stack and runs the entry `walk.motionStep`
/// selects.
static void func_actor_350700_80163400(Task* task)
{
    KyleMadiganWalkerWork* work;
    TaskFuncTable4         handlers;

    work     = task->work;
    handlers = D_actor_350700_80161E68;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Step 1 of the parent: rotates the constant forward offset
/// `D_actor_350700_80161E78` through the root part's matrix into `work->walk.velocity`,
/// seeds `walk.lastDistance` with `ACTOR_WALK_DISTANCE_NONE` and advances the
/// step.
static void func_actor_350700_80163528(Task* task)
{
    KyleMadiganWalkerWork* work;
    GfxCoord*              coord;
    VECTOR                 vec;

    coord = task->extra.tmd->coords;
    work  = task->work;

    vec = D_actor_350700_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceEuler func_actor_350700_801637C4
#include "../../shared/actor_messages_place_euler.inc.c"
#undef actorMsgPlaceEuler

/// `taskMessageDispatch` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`,
/// then the resulting flags are republished onto the objects of the three
/// child tasks the spawn handler parked at `handTasks` and `heldItemTask`.
/// The modes are those of `reverseWalkVisibilityMsg`, the
/// countdown mode 2 latches being `freeCountdown`. Anything else returns 1 and
/// leaves the object alone; the handled modes return 0.
s32 func_actor_350700_80163840(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    KyleMadiganWalkerWork* work;
    TmdObject*             obj;
    TmdObject*             objA;
    TmdObject*             objB;
    TmdObject*             objC;
    u16                    flags;
    s32                    ret;

    work = task->work;
    obj  = task->extra.tmd;
    objA = work->handTasks[0]->extra.tmd;
    objB = work->handTasks[1]->extra.tmd;
    objC = work->heldItemTask->extra.tmd;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    flags       = obj->flags;
    objB->flags = flags;
    objA->flags = flags;
    objC->flags = flags;
    return ret;
}

/// Message handler that accepts its message and does nothing with it:
/// returns 0.
s32 func_actor_350700_8016395C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}
