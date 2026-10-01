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
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call2)(Task*, s32, ActorCommand* request);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, ActorTransform*, GpSpawnAnimArg*);
        s32 (*call5)(Task*, s32, s32);
    } handler;
} Actor350700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor350700MsgEntry, 8);

extern Actor350700MsgEntry gReverseWalkMessages[];

/// The `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks from,
/// and the message table it points the parent's `Task::msgTable` at: ids
/// 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `actorMotionPlayAnim`, terminated by `TASK_MESSAGE_TABLE_END`.
extern TaskDesc            D_actor_350700_801708DC[];
extern Actor350700MsgEntry D_actor_350700_8017090C[];

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

/// Tick handlers of the enemy actor, indexed by `Actor350500Work::walk.motionStep`:
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

extern TmdSource D_actor_350700_8016E86C;
extern TmdSource D_actor_350700_8016ECC0;
extern TmdSource D_actor_350700_8016F1B0;
extern TmdSource D_actor_350700_8016F5F4;
s32              func_actor_350700_801637C4(Task*, s32, ActorTransform* args, s32 arg3);
s32              func_actor_350700_80163840(Task*, s32, s32);
s32              func_actor_350700_8016395C(void);
void             func_actor_350700_80163274(Task*);
void             func_actor_350700_80163350(Task*);

s32  func_actor_350700_80162AF4(Task*, s32, ActorCommand* msg);
void func_actor_350700_80162398(Task*);

TmdBone D_actor_350700_80163964[19] = {
#include "assets/eve_brea_masked_body_skeleton.inc"
};

u32 D_actor_350700_80163C10[19] = {
#include "assets/eve_brea_masked_body_partVerts.inc"
};

SVECTOR D_actor_350700_80163C5C[312] = {
#include "assets/eve_brea_masked_body_verts.inc"
};

SVECTOR D_actor_350700_8016461C[338] = {
#include "assets/eve_brea_masked_body_normals.inc"
};

u32 D_actor_350700_801650AC[3463] = {
#include "assets/eve_brea_masked_body_stream.inc"
};

TmdSource D_actor_350700_801686C8 = {
    0,
    18392,
    6232,
    19,
    D_actor_350700_80163C10,
    D_actor_350700_80163C5C,
    D_actor_350700_8016461C,
    D_actor_350700_80163964,
    D_actor_350700_801650AC,
};

AnimationPackedPose D_actor_350700_801686EC[2] = {
#include "assets/actor_350700_animation_06AB8_bank1.inc"
};

AnimationPackedRotation D_actor_350700_80168704[23] = {
#include "assets/actor_350700_animation_06AB8_bank4.inc"
};

AnimationRecord D_actor_350700_80168760[84] = {
#include "assets/actor_350700_animation_06AB8_records.inc"
};

u16 D_actor_350700_801688B0[20] = {
#include "assets/actor_350700_animation_06AB8_indices.inc"
};

AnimationSet D_actor_350700_801688D8 = {
    D_actor_350700_80168760,
    D_actor_350700_801688B0,
    { NULL, D_actor_350700_801686EC, NULL, NULL, D_actor_350700_80168704, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_80168900[22] = {
#include "assets/actor_350700_animation_0769C_bank1.inc"
};

AnimationPackedRotation D_actor_350700_80168A08[298] = {
#include "assets/actor_350700_animation_0769C_bank4.inc"
};

AnimationRecord D_actor_350700_80168EB0[377] = {
#include "assets/actor_350700_animation_0769C_records.inc"
};

u16 D_actor_350700_80169494[20] = {
#include "assets/actor_350700_animation_0769C_indices.inc"
};

AnimationSet D_actor_350700_801694BC = {
    D_actor_350700_80168EB0,
    D_actor_350700_80169494,
    { NULL, D_actor_350700_80168900, NULL, NULL, D_actor_350700_80168A08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_801694E4[5] = {
#include "assets/actor_350700_animation_07AA4_bank1.inc"
};

AnimationPackedRotation D_actor_350700_80169520[76] = {
#include "assets/actor_350700_animation_07AA4_bank4.inc"
};

AnimationRecord D_actor_350700_80169650[147] = {
#include "assets/actor_350700_animation_07AA4_records.inc"
};

u16 D_actor_350700_8016989C[20] = {
#include "assets/actor_350700_animation_07AA4_indices.inc"
};

AnimationSet D_actor_350700_801698C4 = {
    D_actor_350700_80169650,
    D_actor_350700_8016989C,
    { NULL, D_actor_350700_801694E4, NULL, NULL, D_actor_350700_80169520, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_801698EC[5] = {
#include "assets/actor_350700_animation_07EB0_bank1.inc"
};

AnimationPackedRotation D_actor_350700_80169928[89] = {
#include "assets/actor_350700_animation_07EB0_bank4.inc"
};

AnimationRecord D_actor_350700_80169A8C[135] = {
#include "assets/actor_350700_animation_07EB0_records.inc"
};

u16 D_actor_350700_80169CA8[20] = {
#include "assets/actor_350700_animation_07EB0_indices.inc"
};

AnimationSet D_actor_350700_80169CD0 = {
    D_actor_350700_80169A8C,
    D_actor_350700_80169CA8,
    { NULL, D_actor_350700_801698EC, NULL, NULL, D_actor_350700_80169928, NULL, NULL, NULL },
};

AnimationSet* D_actor_350700_80169CF8[5] = {
    NULL,
    &D_actor_350700_801688D8,
    &D_actor_350700_801694BC,
    &D_actor_350700_801698C4,
    &D_actor_350700_80169CD0,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_350700_80169CF8,
};

TaskDesc D_actor_350700_80169D10 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_350700_80162398, { .model = &D_actor_350700_801686C8 } };

Actor350700MsgEntry gReverseWalkMessages[6] = {
    { 2003, { .call1 = actorMotionPlayAnim19 } },
    { 2004, { .call3 = actorMsgPlaceEuler } },
    { 2005, { .call5 = reverseWalkVisibilityMsg } },
    { 2013, { .call4 = reverseWalkStartMsg } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_350700_80162AF4 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

TmdBone D_actor_350700_80169D4C[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

u32 D_actor_350700_8016A01C[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

SVECTOR D_actor_350700_8016A06C[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

SVECTOR D_actor_350700_8016A9CC[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

u32 D_actor_350700_8016B31C[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

TmdSource D_actor_350700_8016E86C = {
    0,
    18224,
    5696,
    20,
    D_actor_350700_8016A01C,
    D_actor_350700_8016A06C,
    D_actor_350700_8016A9CC,
    D_actor_350700_80169D4C,
    D_actor_350700_8016B31C,
};

TmdBone D_actor_350700_8016E890[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

u32 D_actor_350700_8016E8B4[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

SVECTOR D_actor_350700_8016E8B8[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

SVECTOR D_actor_350700_8016E970[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

u32 D_actor_350700_8016EA28[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

TmdSource D_actor_350700_8016ECC0 = {
    0,
    1148,
    0,
    1,
    D_actor_350700_8016E8B4,
    D_actor_350700_8016E8B8,
    D_actor_350700_8016E970,
    D_actor_350700_8016E890,
    D_actor_350700_8016EA28,
};

TmdBone D_actor_350700_8016ECE4[1] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

u32 D_actor_350700_8016ED08[1] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

SVECTOR D_actor_350700_8016ED0C[27] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

SVECTOR D_actor_350700_8016EDE4[27] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

u32 D_actor_350700_8016EEBC[189] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

TmdSource D_actor_350700_8016F1B0 = {
    0,
    1328,
    0,
    1,
    D_actor_350700_8016ED08,
    D_actor_350700_8016ED0C,
    D_actor_350700_8016EDE4,
    D_actor_350700_8016ECE4,
    D_actor_350700_8016EEBC,
};

TmdBone D_actor_350700_8016F1D4[1] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

u32 D_actor_350700_8016F1F8[1] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

SVECTOR D_actor_350700_8016F1FC[22] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

SVECTOR D_actor_350700_8016F2AC[24] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

u32 D_actor_350700_8016F36C[162] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

TmdSource D_actor_350700_8016F5F4 = {
    0,
    1108,
    0,
    1,
    D_actor_350700_8016F1F8,
    D_actor_350700_8016F1FC,
    D_actor_350700_8016F2AC,
    D_actor_350700_8016F1D4,
    D_actor_350700_8016F36C,
};

AnimationPackedPose D_actor_350700_8016F618[2] = {
#include "assets/actor_350700_animation_0DA4C_bank1.inc"
};

AnimationPackedRotation D_actor_350700_8016F630[32] = {
#include "assets/actor_350700_animation_0DA4C_bank4.inc"
};

AnimationRecord D_actor_350700_8016F6B0[101] = {
#include "assets/actor_350700_animation_0DA4C_records.inc"
};

u16 D_actor_350700_8016F844[20] = {
#include "assets/actor_350700_animation_0DA4C_indices.inc"
};

AnimationSet D_actor_350700_8016F86C = {
    D_actor_350700_8016F6B0,
    D_actor_350700_8016F844,
    { NULL, D_actor_350700_8016F618, NULL, NULL, D_actor_350700_8016F630, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_8016F894[3] = {
#include "assets/actor_350700_animation_0DD04_bank1.inc"
};

AnimationPackedRotation D_actor_350700_8016F8B8[29] = {
#include "assets/actor_350700_animation_0DD04_bank4.inc"
};

AnimationRecord D_actor_350700_8016F92C[116] = {
#include "assets/actor_350700_animation_0DD04_records.inc"
};

u16 D_actor_350700_8016FAFC[20] = {
#include "assets/actor_350700_animation_0DD04_indices.inc"
};

AnimationSet D_actor_350700_8016FB24 = {
    D_actor_350700_8016F92C,
    D_actor_350700_8016FAFC,
    { NULL, D_actor_350700_8016F894, NULL, NULL, D_actor_350700_8016F8B8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_8016FB4C[3] = {
#include "assets/actor_350700_animation_0E08C_bank1.inc"
};

AnimationPackedRotation D_actor_350700_8016FB70[55] = {
#include "assets/actor_350700_animation_0E08C_bank4.inc"
};

AnimationRecord D_actor_350700_8016FC4C[142] = {
#include "assets/actor_350700_animation_0E08C_records.inc"
};

u16 D_actor_350700_8016FE84[20] = {
#include "assets/actor_350700_animation_0E08C_indices.inc"
};

AnimationSet D_actor_350700_8016FEAC = {
    D_actor_350700_8016FC4C,
    D_actor_350700_8016FE84,
    { NULL, D_actor_350700_8016FB4C, NULL, NULL, D_actor_350700_8016FB70, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_8016FED4[2] = {
#include "assets/actor_350700_animation_0E284_bank1.inc"
};

AnimationPackedRotation D_actor_350700_8016FEEC[33] = {
#include "assets/actor_350700_animation_0E284_bank4.inc"
};

AnimationRecord D_actor_350700_8016FF70[67] = {
#include "assets/actor_350700_animation_0E284_records.inc"
};

u16 D_actor_350700_8017007C[20] = {
#include "assets/actor_350700_animation_0E284_indices.inc"
};

AnimationSet D_actor_350700_801700A4 = {
    D_actor_350700_8016FF70,
    D_actor_350700_8017007C,
    { NULL, D_actor_350700_8016FED4, NULL, NULL, D_actor_350700_8016FEEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350700_801700CC[20] = {
#include "assets/actor_350700_animation_0EA78_bank1.inc"
};

AnimationPackedRotation D_actor_350700_801701BC[164] = {
#include "assets/actor_350700_animation_0EA78_bank4.inc"
};

AnimationRecord D_actor_350700_8017044C[265] = {
#include "assets/actor_350700_animation_0EA78_records.inc"
};

u16 D_actor_350700_80170870[20] = {
#include "assets/actor_350700_animation_0EA78_indices.inc"
};

AnimationSet D_actor_350700_80170898 = {
    D_actor_350700_8017044C,
    D_actor_350700_80170870,
    { NULL, D_actor_350700_801700CC, NULL, NULL, D_actor_350700_801701BC, NULL, NULL, NULL },
};

AnimationSet* D_actor_350700_801708C0[6] = {
    NULL,
    &D_actor_350700_8016F86C,
    &D_actor_350700_8016FB24,
    &D_actor_350700_8016FEAC,
    &D_actor_350700_801700A4,
    &D_actor_350700_80170898,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_350700_801708C0,
};

TaskDesc D_actor_350700_801708DC[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_350700_80163350, { .model = &D_actor_350700_8016E86C } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_350700_80163274, { .model = &D_actor_350700_8016F1B0 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_350700_80163274, { .model = &D_actor_350700_8016ECC0 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_350700_80163274, { .model = &D_actor_350700_8016F5F4 } },
};

Actor350700MsgEntry D_actor_350700_8017090C[6] = {
    { 2003, { .call1 = actorMotionPlayAnim } },
    { 2004, { .call3 = func_actor_350700_801637C4 } },
    { 2005, { .call5 = func_actor_350700_80163840 } },
    { 2013, { .call4 = actorMotionStartWalk } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_350700_8016395C } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
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
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the enemy work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the actor draws with its own
/// lighting.
void reverseWalkBindLighting(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor350500Work*)arg0->work;
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
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350700_80161E30;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

#include "../../shared/reversing_walker_face.inc.c"

#include "../../shared/reversing_walker_move.inc.c"

#include "../../shared/reversing_walker_turn.inc.c"

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

#include "../../shared/reversing_walker_visibility.inc.c"

/// `taskMessageDispatch` handler: latches the variant the message's halfword at
/// 0x2 selects into `field_4C4` -- 1 clears it, 2 sets it, anything else
/// leaves it. Always returns 0.
s32 func_actor_350700_80162AF4(Task* task, s32 arg1, ActorCommand* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->work;
    switch (msg->command) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}

/// The parent's spawn handler. Allocates the 0x50C `Actor135600Work` block, seeds it, and spawns the
/// three children `D_actor_350700_801708DC` holds -- table entries 1, 2 and 3 --
/// parking them at `child0` / `child1` / `child2`. The first two are
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
    Actor135600Work* work;
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    GameLocationKey* keyAddr;
    Task*            spawned;

    work = memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work             = work;
    work->model.animId     = -1;
    work->model.bank       = -1;
    work->freeCountdown    = -1;
    work->walk.acc[0].word = 0;
    work->walk.acc[1].word = 0;
    work->walk.acc[2].word = 0;
    spawned                = Task_SpawnFromTable(D_actor_350700_801708DC, 1, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        work->child0 = spawned;
        model        = spawned->extra.tmd;
        idx          = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey   = &gGameSession->location.loc;
        key.stage    = sessionKey->stage;
        key.area     = sessionKey->area;
        key.room     = sessionKey->room;
        key.view     = sessionKey->view;
        areaSyncLocationVariant(&key);
        rec                      = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(rec->field_0, idx);
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
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        work->child1 = spawned;
        model        = spawned->extra.tmd;
        idx          = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        // Keep this block's key address separate across the spawn calls.
        sessionKey = (keyAddr = &gGameSession->location.loc);
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = keyAddr->room;
        key.view   = gGameSession->location.loc.view;
        areaSyncLocationVariant(&key);
        rec                      = Gp_GetNestedAreaRec(&key);
        place                    = gpAreaPlaceAt(rec->field_0, idx);
        model->texturePageOffset = place->texturePageOffset;
        model->clutRowOffset     = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(D_actor_350700_801708DC, 3, 8, arg0);
    if (spawned != NULL) {
        work->child2 = spawned;
    }
    func_actor_350700_801633DC(arg0);
    arg0->msgTable     = D_actor_350700_8017090C;
    arg0->exitCallback = func_actor_350700_801633BC;
    arg0->state       += 1;
}

/// Per-frame tick of the parent actor: dispatches through the local two-entry
/// table `walk.motion` indexes -- the empty `func_actor_350700_801633F8` or the
/// step dispatcher `func_actor_350700_80163400` -- then integrates the
/// per-frame deltas in `walk.step` into the 16.16 accumulators `walk.acc`,
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
    TmdObject*       ext      = arg0->extra.tmd;
    Actor135600Work* work     = (Actor135600Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350700_801633F8, func_actor_350700_80163400 };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    funcs[work->walk.motion](arg0);
    coord                   = arg0->extra.tmd->coords;
    work->walk.acc[0].word += work->walk.step.vx;
    work->walk.acc[1].word += work->walk.step.vy;
    work->walk.acc[2].word += work->walk.step.vz;
    coord->coord.t[0]      += (s16)(work->walk.acc[0].word >> 16);
    coord->coord.t[1]      += (s16)(work->walk.acc[1].word >> 16);
    coord->coord.t[2]      += (s16)(work->walk.acc[2].word >> 16);
    coord->composeStamp     = GRAPHICS_COORD_DIRTY;
    work->walk.acc[0].word  = (u16)work->walk.acc[0].word;
    work->walk.acc[1].word  = (u16)work->walk.acc[1].word;
    work->walk.acc[2].word  = (u16)work->walk.acc[2].word;
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
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
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
/// `Gp_EnemyTaskExit` teardown `reverseWalkExit` performs.
static void func_actor_350700_801633BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the parent work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the parent draws with its own
/// lighting.
static void func_actor_350700_801633DC(Task* task)
{
    TmdObject*       ext;
    Actor135600Work* work;

    ext           = task->extra.tmd;
    work          = (Actor135600Work*)task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// The empty first entry of the parent's two-handler table, selected by
/// `Actor135600Work::walk.motion` -- the idle half of the pair whose other
/// entry is the step dispatcher `func_actor_350700_80163400`.
static void func_actor_350700_801633F8(Task* arg0)
{
}

/// Motion handler 1 of the parent block: copies the step-handler table
/// `D_actor_350700_80161E68` onto the stack and runs the entry `walk.motionStep`
/// selects.
static void func_actor_350700_80163400(Task* task)
{
    Actor135600Work* work;
    TaskFuncTable4   fns;

    work = (Actor135600Work*)task->work;
    fns  = D_actor_350700_80161E68;
    fns.funcs[(s16)work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Step 1 of the parent: rotates the constant forward offset
/// `D_actor_350700_80161E78` through the root part's matrix into `work->walk.step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it, and
/// advances the step.
static void func_actor_350700_80163528(Task* task)
{
    Actor135600Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = task->extra.tmd->coords;
    work  = (Actor135600Work*)task->work;

    vec = D_actor_350700_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
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
/// child tasks the spawn handler parked at `child0` / `child1` /
/// `child2`. The modes are those of `reverseWalkVisibilityMsg`, the
/// countdown mode 2 latches being `freeCountdown`. Anything else returns 1 and
/// leaves the object alone; the handled modes return 0.
s32 func_actor_350700_80163840(Task* task, s32 arg1, s32 mode)
{
    Actor135600Work* work;
    TmdObject*       obj;
    TmdObject*       objA;
    TmdObject*       objB;
    TmdObject*       objC;
    u16              flags;
    s32              ret;

    work = (Actor135600Work*)task->work;
    obj  = task->extra.tmd;
    objA = work->child0->extra.tmd;
    objB = work->child1->extra.tmd;
    objC = work->child2->extra.tmd;
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
s32 func_actor_350700_8016395C(void)
{
    return 0;
}
