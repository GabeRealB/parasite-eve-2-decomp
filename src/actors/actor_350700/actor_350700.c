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
#include "../../shared/reversing_walker.h"

/// Animation bank tables of the enemy actor and of the parent block.
extern AnimationSet*  D_actor_350700_80169CF8[5];
extern AnimationSet** gActorMotionAnimBanks19[1];
extern AnimationSet*  D_actor_350700_801708C0[6];
extern AnimationSet** gActorMotionAnimBanks[1];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `_reverseWalkSpawn`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry gReverseWalkMessages[];

/// The `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks from,
/// and the message table it points the parent's `Task::msgTable` at: ids
/// 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `_actorMotionPlayAnim`, terminated by `TASK_MESSAGE_TABLE_END`.
extern TaskDesc         D_actor_350700_801708DC[];
extern TaskMessageEntry D_actor_350700_8017090C[];

static void _modelPlacementAttachPartTask(Task* childTask);
static void func_actor_350700_80162B30(Task* arg0);
static void _actor350700KyleMadiganWalkerUpdate(Task* task);
static void _actor350700KyleMadiganAttachmentIdle(Task* task);
static void _actor350700KyleMadiganWalkerExit(Task* task);
static void _actor350700KyleMadiganWalkerBindLighting(Task* task);
static void _actor350700KyleMadiganWalkerIdle(Task* task);
static void _actor350700KyleMadiganWalkerRunStep(Task* task);
static void _actor350700KyleMadiganWalkerBeginMove(Task* task);

/// Spawn, tick and exit handlers of the enemy actor, dispatched by
/// `_actor350700ReverseWalkTask`.
static const TaskFuncTable3 D_actor_350700_80161E24 = { {
    _reverseWalkSpawn,
    _reverseWalkUpdate,
    _reverseWalkExit,
} };

/// Tick handlers of the enemy actor, indexed by `ReverseWalkWork::walk.motionStep`:
/// turn to face `target`, start moving, approach until arrival, then turn to
/// the placement yaw.
static const TaskFuncTable4 D_actor_350700_80161E30 = { {
    _reverseWalkOrientForWalk,
    _reverseWalkBeginMove,
    _actorMotionArrive19,
    _reverseWalkTurnToYaw,
} };

/// The constant local-space offset `_reverseWalkBeginMove` rotates:
/// straight ahead along the part's own +Z.
static const VECTOR _gReverseWalkForward = { 0, 0, 0x200000, 0 };

/// Spawn, tick and exit handlers of the child part tasks, dispatched by
/// `_actor350700KyleMadiganAttachmentTask`.
static const TaskFuncTable3 D_actor_350700_80161E50 = { {
    _modelPlacementAttachPartTask,
    _actor350700KyleMadiganAttachmentIdle,
    taskKill,
} };

/// Spawn, tick and exit handlers of the parent actor, dispatched by
/// `_actor350700KyleMadiganWalkerTask`.
static const TaskFuncTable3 D_actor_350700_80161E5C = { {
    func_actor_350700_80162B30,
    _actor350700KyleMadiganWalkerUpdate,
    _actor350700KyleMadiganWalkerExit,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_350700_80161E68 = { {
    _actorMotionFaceTarget,
    _actor350700KyleMadiganWalkerBeginMove,
    _actorMotionArrive,
    _actorMotionTurnToYaw,
} };

/// The parent's copy of the forward offset, rotated by
/// `_actor350700KyleMadiganWalkerBeginMove`.
static const VECTOR D_actor_350700_80161E78 = { 0, 0, 0x200000, 0 };

static TmdSource _gActor350700KyleMadiganBody;
static TmdSource _gActor350700KyleMadiganHandRight;
static TmdSource _gActor350700KyleMadiganHandLeft;
static TmdSource _gActor350700KyleMadiganGun;
static s32       _actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
static s32       _actor350700KyleMadiganWalkerSetDrawModeMsg(Task* task, s32 msgId, s32 mode, s32 unusedArg);
static s32       _actor350700KyleMadiganWalkerIgnoreCommandMsg(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void      _actor350700KyleMadiganAttachmentTask(Task* task);
static void      _actor350700KyleMadiganWalkerTask(Task* task);

static s32  _actor350700ReverseWalkCommandMsg(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static void _actor350700ReverseWalkTask(Task* task);

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

TaskDesc D_actor_350700_80169D10 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor350700ReverseWalkTask, { .model = &_gActor350700EveBreaMaskedBody } };

TaskMessageEntry gReverseWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _reverseWalkSetDrawModeMsg },
    { ACTOR_MESSAGE_WALK_TO, _reverseWalkStartWalkMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor350700ReverseWalkCommandMsg },
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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor350700KyleMadiganWalkerTask, { .model = &_gActor350700KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor350700KyleMadiganAttachmentTask, { .model = &_gActor350700KyleMadiganHandLeft } },
    { { { TASK_BODY_TMD, 192 } }, _actor350700KyleMadiganAttachmentTask, { .model = &_gActor350700KyleMadiganHandRight } },
    { { { TASK_BODY_TMD, 192 } }, _actor350700KyleMadiganAttachmentTask, { .model = &_gActor350700KyleMadiganGun } },
};

TaskMessageEntry D_actor_350700_8017090C[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actorMotionPlayAnim },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor350700KyleMadiganWalkerSetDrawModeMsg },
    { ACTOR_MESSAGE_WALK_TO, _actorMotionStartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor350700KyleMadiganWalkerIgnoreCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};
#include "../../shared/reversing_walker_update.inc.c"

#include "../../shared/actor_motion_arrive19.inc.c"

#include "../../shared/reversing_walker_start.inc.c"

/// Runs the nineteen-part reversing walker's task while scene actors are running.
///
/// Requires a live TMD task with state 0 (spawn), 1 (update) or 2 (exit);
/// the state index is unchecked. Spawn and exit require its owned `Enemy`
/// in `spawnArg2.pointer`; update requires initialized `ReverseWalkWork`.
/// Actor-control values other than `SCENE_COMBAT_ACTORS_RUNNING` suspend
/// all three states. Spawn failure or exit may release the task and resources.
static void _actor350700ReverseWalkTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = D_actor_350700_80161E24;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

#include "../../shared/reversing_walker_spawn.inc.c"

/// Releases the walker's enemy record and begins task and model teardown.
///
/// Requires a live task with its owned primary-heap `Enemy` in
/// `spawnArg2.pointer`. Target tracking and actor locks are released before
/// children, work and model storage. Do not access the task or enemy afterwards.
static void _reverseWalkExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the walker's lighting matrices to its model for lighting and drawing.
///
/// Requires a live TMD task with allocated `ReverseWalkWork`. The model borrows
/// writable matrices in that work; keep it live until model use ends.
/// This binds storage without calculating or initializing either matrix.
static void _reverseWalkBindLighting(Task* task)
{
    TmdObject*       model;
    ReverseWalkWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Leaves walk state unchanged while no scripted walk is in progress.
///
/// The frame update still integrates velocity, ticks animation and draws.
/// The task parameter is unused but retains the `TaskFunc` callback signature.
static void _reverseWalkIdle(Task* task)
{
}

/// Runs the current phase of a scripted forward or backward walk.
///
/// Requires initialized `ReverseWalkWork` and `walk.motionStep` in 0..3:
/// face the target, begin movement, approach until arrival, then turn to the
/// destination yaw. Runs one phase per tick without checking the index.
static void _reverseWalkRunStep(Task* task)
{
    ReverseWalkWork*     work     = task->work;
    const TaskFuncTable4 handlers = D_actor_350700_80161E30;

    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/reversing_walker_face.inc.c"

#include "../../shared/reversing_walker_move.inc.c"

#include "../../shared/reversing_walker_turn.inc.c"

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/reversing_walker_visibility.inc.c"

/// Selects whether the reversing walker approaches its targets forward or backward.
///
/// Requires an initialized `ReverseWalkWork` and a borrowed, readable command.
/// Command 1 selects backing, 2 selects forward walking; other commands leave
/// the direction unchanged. The context, message ID and second payload are
/// ignored. Retains no command pointer and always returns 0.
static s32 _actor350700ReverseWalkCommandMsg(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_350700_REVERSE_WALK_COMMAND_BACKWARD = 1,
        ACTOR_350700_REVERSE_WALK_COMMAND_FORWARD  = 2,
    };
    ReverseWalkWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_350700_REVERSE_WALK_COMMAND_BACKWARD:
            work->walksForward = 0;
            break;
        case ACTOR_350700_REVERSE_WALK_COMMAND_FORWARD:
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
/// handing the parent to `_actor350700KyleMadiganWalkerBindLighting`, pointing `msgTable` at the
/// message table and installing `_actor350700KyleMadiganWalkerExit` as its exit
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
    spawned                  = taskSpawnFromTable(D_actor_350700_801708DC, 1, 8, arg0);
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
        layout                   = areaGetVariant(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    spawned = taskSpawnFromTable(D_actor_350700_801708DC, 2, 0xC, arg0);
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
        layout                   = areaGetVariant(&key);
        place                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    spawned = taskSpawnFromTable(D_actor_350700_801708DC, 3, 8, arg0);
    if (spawned != NULL) {
        work->heldItemTask = spawned;
    }
    _actor350700KyleMadiganWalkerBindLighting(arg0);
    arg0->msgTable     = D_actor_350700_8017090C;
    arg0->exitCallback = _actor350700KyleMadiganWalkerExit;
    arg0->state       += 1;
}

/// Applies Kyle Madigan's walk velocity to his root's integer translation.
///
/// Work and root must be separate, live and writable. Velocity is signed
/// 16.16 displacement in the root's parent frame, in coordinate units per
/// update. Adds the signed integer halves to XYZ and retains each unsigned
/// fractional half (0..65535) for the next update, including negative motion.
/// Marks composition dirty even for zero velocity; rotation is unchanged.
/// Neither pointer is retained, and the caller keeps ownership of both objects.
static inline void _actor350700KyleMadiganWalkerIntegrateVelocity(KyleMadiganWalkerWork* work, GfxCoord* rootCoord)
{
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    rootCoord->coord.t[0]    += work->walk.carry[0].halves.integer;
    rootCoord->coord.t[1]    += work->walk.carry[1].halves.integer;
    rootCoord->coord.t[2]    += work->walk.carry[2].halves.integer;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
}

/// Advances Kyle Madigan's scripted walk, animation and model presentation.
///
/// Requires initialized `KyleMadiganWalkerWork`, a body with coordinates 0..19,
/// and motion 0 (idle) or 1 (walking). A ticking rig must already be bound.
/// Integrates velocity in signed 16.16 coordinate units per tick and drives
/// animation slots 1..19. Part 1 supplies the world position for the shadow and
/// lighting; it is recomposed when the view is ready. Lighting is also refreshed
/// unconditionally, including a second refresh on those frames.
/// A nonnegative buffer countdown is decremented once; a tick entering at zero
/// releases the body's primitive buffer. -1 disables the countdown.
static void _actor350700KyleMadiganWalkerUpdate(Task* task)
{
    enum { ACTOR_350700_KYLE_MADIGAN_SHADOW_HALF_SIZE = 0x300 };
    TmdObject*             bodyModel        = task->extra.tmd;
    KyleMadiganWalkerWork* work             = task->work;
    TaskFunc               motionHandlers[] = { _actor350700KyleMadiganWalkerIdle, _actor350700KyleMadiganWalkerRunStep };
    VECTOR3                groundPoint;
    GfxCoord*              rootCoord;
    s32                    slotIndex;

    // Run the walk step before consuming its velocity for this frame.
    motionHandlers[work->walk.motion](task);
    rootCoord = task->extra.tmd->coords;
    _actor350700KyleMadiganWalkerIntegrateVelocity(work, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    if (!(bodyModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_350700_KYLE_MADIGAN_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    // The presentation frame is part 1; retain both lighting queries when ready.
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    worldCoordSetModelLighting(bodyModel, task->extra.tmd->coords[1].workm.t, 0, 3);
    // Buffer release is delayed while the hidden body stops drawing.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(bodyModel);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// Runs the attachment task for either hand or the gun on Kyle Madigan's body.
///
/// State must be 0 (attach to the parent part), 1 (idle) or 2 (kill the task).
/// Setup requires the live parent task and part index in the spawn arguments;
/// the parent owns the attachment's coordinate and lighting lifetime.
static void _actor350700KyleMadiganAttachmentTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_350700_80161E50;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

/// Leaves an attached hand or gun idle while the parent's rig drives its transform.
static void _actor350700KyleMadiganAttachmentIdle(Task* task)
{
}

/// Runs Kyle Madigan's body task while actor control is running.
///
/// State must be 0 (spawn), 1 (update) or 2 (exit). Actor-control values other
/// than `SCENE_COMBAT_ACTORS_RUNNING` suspend all three states; attachment
/// dispatch is independent of that gate.
static void _actor350700KyleMadiganWalkerTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_350700_80161E5C;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Releases Kyle Madigan's enemy record and tears down the body and its children.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2.pointer`, including
/// an incomplete spawn. Task teardown releases the work and model resources.
/// The task and enemy must not be accessed after this call.
static void _actor350700KyleMadiganWalkerExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends Kyle Madigan's work-owned lighting matrices to his body model.
///
/// Requires an allocated `KyleMadiganWalkerWork` and a live TMD body. The work
/// must outlive model use and attachments that copy these matrix pointers.
static void _actor350700KyleMadiganWalkerBindLighting(Task* task)
{
    TmdObject*             bodyModel;
    KyleMadiganWalkerWork* work;

    bodyModel           = task->extra.tmd;
    work                = task->work;
    bodyModel->lightMtx = &work->model.light;
    bodyModel->colorMtx = &work->model.color;
}

/// Leaves the scripted walk idle while the body update still drives presentation.
static void _actor350700KyleMadiganWalkerIdle(Task* task)
{
}

/// Runs the current step of Kyle Madigan's scripted walk.
///
/// Requires initialized work and `walk.motionStep` in 0..3: face the target,
/// start moving, detect arrival, then turn to the requested final yaw.
static void _actor350700KyleMadiganWalkerRunStep(Task* task)
{
    KyleMadiganWalkerWork* work;
    TaskFuncTable4         handlers;

    work     = task->work;
    handlers = D_actor_350700_80161E68;
    handlers.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Starts Kyle Madigan moving along his root's local forward axis.
///
/// Requires initialized work and the root rotation set by the facing step.
/// Rotates a +Z velocity of 32 coordinate units per tick, encoded in signed
/// 16.16, into the root's parent frame. Seeds the first arrival comparison and
/// advances to walk step 2; existing fractional displacement is retained.
static void _actor350700KyleMadiganWalkerBeginMove(Task* task)
{
    KyleMadiganWalkerWork* work;
    GfxCoord*              rootCoord;
    VECTOR                 forwardVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    // Rotate velocity only; the root's translation must not affect the step.
    forwardVelocity = D_actor_350700_80161E78;
    ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

/// Selects the private placement handler for the Kyle Madigan model task.
#define ACTOR_MESSAGE_PLACE_EULER_HANDLER _actorMsgPlaceEuler
#include "../../shared/actor_messages_place_euler.inc.c"
#undef ACTOR_MESSAGE_PLACE_EULER_HANDLER

/// Applies Kyle Madigan's body draw mode and copies its flags to both hands and gun.
///
/// Requires initialized work and all four live TMD tasks, even for an unknown
/// mode. Mode 0 hides and enables automatic buffer recovery; 1 shows, allocates
/// the body's buffer and enables recovery; 2 hides, disables recovery and sets
/// the body buffer-release countdown to 2; 3 shows with recovery disabled.
/// Other modes return 1 without changing body flags; handled modes return 0.
/// Every mode replaces each attachment's entire flag word with the body's.
/// Showing does not cancel a pending release. The message ID and second
/// payload are ignored; no payload storage is borrowed.
static s32 _actor350700KyleMadiganWalkerSetDrawModeMsg(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    enum { ACTOR_350700_KYLE_MADIGAN_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    KyleMadiganWalkerWork* work;
    TmdObject*             bodyModel;
    TmdObject*             leftHandModel;
    TmdObject*             rightHandModel;
    TmdObject*             heldItemModel;
    u16                    flags;
    s32                    result;

    work           = task->work;
    bodyModel      = task->extra.tmd;
    leftHandModel  = work->handTasks[0]->extra.tmd;
    rightHandModel = work->handTasks[1]->extra.tmd;
    heldItemModel  = work->heldItemTask->extra.tmd;
    result         = 0;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            bodyModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(bodyModel);
            bodyModel->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            bodyModel->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            bodyModel->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_350700_KYLE_MADIGAN_DRAW_SHOW_SKIP_AUTO_BUFFER:
            bodyModel->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    // Attachments inherit the whole flag word, including unrelated body flags.
    flags                 = bodyModel->flags;
    rightHandModel->flags = flags;
    leftHandModel->flags  = flags;
    heldItemModel->flags  = flags;
    return result;
}

/// Accepts actor-command messages for Kyle Madigan without changing his state.
///
/// Ignores every argument, dereferences no payload and always returns 0. This
/// consumes scene broadcasts while animation and walk messages drive the actor.
static s32 _actor350700KyleMadiganWalkerIgnoreCommandMsg(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
    return 0;
}
