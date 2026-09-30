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
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

/// Optional start animation for the same handler: the preset's `field_4`
/// and the `model.nextAnimId` byte. Absent, the defaults are anim 3 (or 2 once
/// `field_4C4` is set) and 1.
typedef GpSpawnAnimArg Actor350700SpawnAnim;

/// Animation bank tables of the enemy actor and of the parent block.
extern AnimationSet*  D_actor_350700_80169CF8[5];
extern AnimationSet** gActorMotionAnimBanks19[1];
extern AnimationSet*  D_actor_350700_801708C0[6];
extern AnimationSet** gActorMotionAnimBanks[1];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350700_80162404`; terminator id 0x7FFFFFFF.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call2)(Task*, s32, ActorCommand* request);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, ActorTransform*, Actor350700SpawnAnim*);
        s32 (*call5)(Task*, s32, s32);
    } handler;
} Actor350700MsgEntry;
STATIC_ASSERT_SIZEOF(Actor350700MsgEntry, 8);

extern Actor350700MsgEntry D_actor_350700_80169D1C[];

/// The `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks from,
/// and the message table it points the parent's `Task::msgTable` at: ids
/// 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `actorMotionPlayAnim`, terminated by 0x7FFFFFFF.
extern TaskDesc            D_actor_350700_801708DC[];
extern Actor350700MsgEntry D_actor_350700_8017090C[];

static void func_actor_350700_80161E88(Task* arg0);
static void func_actor_350700_80162404(Task* arg0);
static void func_actor_350700_80162494(Task* arg0);
static void func_actor_350700_801624B4(Task* arg0);
static void func_actor_350700_801624D0(Task* arg0);
static void func_actor_350700_801624D8(Task* arg0);
static void func_actor_350700_80162540(Task* task);
static void func_actor_350700_8016261C(Task* arg0);
static void func_actor_350700_80162764(Task* arg0);
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
    func_actor_350700_80162404,
    func_actor_350700_80161E88,
    func_actor_350700_80162494,
} };

/// Tick handlers of the enemy actor, indexed by `Actor350500Work::walk.motionStep`:
/// turn to face `target`, start moving, approach until arrival, then turn to
/// the placement yaw.
static const TaskFuncTable4 D_actor_350700_80161E30 = { {
    func_actor_350700_80162540,
    func_actor_350700_8016261C,
    actorMotionArrive19,
    func_actor_350700_80162764,
} };

/// The constant local-space offset `func_actor_350700_8016261C` rotates:
/// straight ahead along the part's own +Z.
static const VECTOR D_actor_350700_80161E40 = { 0, 0, 0x200000, 0 };

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

s32  func_actor_350700_801621B4(Task*, s32, ActorTransform* place, Actor350700SpawnAnim*);
s32  func_actor_350700_80162A14(Task*, s32, s32);
s32  func_actor_350700_80162AF4(Task*, s32, ActorCommand* msg);
void func_actor_350700_80162398(Task*);

TmdBone D_actor_350700_80163964[19] = {
#include "assets/actor_350700_model_068A8_skeleton.inc"
};

u32 D_actor_350700_80163C10[19] = {
#include "assets/actor_350700_model_068A8_partVerts.inc"
};

SVECTOR D_actor_350700_80163C5C[312] = {
#include "assets/actor_350700_model_068A8_verts.inc"
};

SVECTOR D_actor_350700_8016461C[338] = {
#include "assets/actor_350700_model_068A8_normals.inc"
};

u32 D_actor_350700_801650AC[3463] = {
#include "assets/actor_350700_model_068A8_stream.inc"
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

TaskDesc D_actor_350700_80169D10 = { (TASK_BODY_TMD | 0x100), 192, func_actor_350700_80162398, { .model = &D_actor_350700_801686C8 } };

Actor350700MsgEntry D_actor_350700_80169D1C[6] = {
    { 2003, { .call1 = actorMotionPlayAnim19 } },
    { 2004, { .call3 = actorMsgPlaceEuler } },
    { 2005, { .call5 = func_actor_350700_80162A14 } },
    { 2013, { .call4 = func_actor_350700_801621B4 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_350700_80162AF4 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TmdBone D_actor_350700_80169D4C[20] = {
#include "assets/actor_350700_model_0CA4C_skeleton.inc"
};

u32 D_actor_350700_8016A01C[20] = {
#include "assets/actor_350700_model_0CA4C_partVerts.inc"
};

SVECTOR D_actor_350700_8016A06C[300] = {
#include "assets/actor_350700_model_0CA4C_verts.inc"
};

SVECTOR D_actor_350700_8016A9CC[298] = {
#include "assets/actor_350700_model_0CA4C_normals.inc"
};

u32 D_actor_350700_8016B31C[3412] = {
#include "assets/actor_350700_model_0CA4C_stream.inc"
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
#include "assets/actor_350700_model_0CEA0_skeleton.inc"
};

u32 D_actor_350700_8016E8B4[1] = {
#include "assets/actor_350700_model_0CEA0_partVerts.inc"
};

SVECTOR D_actor_350700_8016E8B8[23] = {
#include "assets/actor_350700_model_0CEA0_verts.inc"
};

SVECTOR D_actor_350700_8016E970[23] = {
#include "assets/actor_350700_model_0CEA0_normals.inc"
};

u32 D_actor_350700_8016EA28[166] = {
#include "assets/actor_350700_model_0CEA0_stream.inc"
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
#include "assets/actor_350700_model_0D390_skeleton.inc"
};

u32 D_actor_350700_8016ED08[1] = {
#include "assets/actor_350700_model_0D390_partVerts.inc"
};

SVECTOR D_actor_350700_8016ED0C[27] = {
#include "assets/actor_350700_model_0D390_verts.inc"
};

SVECTOR D_actor_350700_8016EDE4[27] = {
#include "assets/actor_350700_model_0D390_normals.inc"
};

u32 D_actor_350700_8016EEBC[189] = {
#include "assets/actor_350700_model_0D390_stream.inc"
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
#include "assets/actor_350700_model_0D7D4_skeleton.inc"
};

u32 D_actor_350700_8016F1F8[1] = {
#include "assets/actor_350700_model_0D7D4_partVerts.inc"
};

SVECTOR D_actor_350700_8016F1FC[22] = {
#include "assets/actor_350700_model_0D7D4_verts.inc"
};

SVECTOR D_actor_350700_8016F2AC[24] = {
#include "assets/actor_350700_model_0D7D4_normals.inc"
};

u32 D_actor_350700_8016F36C[162] = {
#include "assets/actor_350700_model_0D7D4_stream.inc"
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
    { (TASK_BODY_TMD | 0x100), 192, func_actor_350700_80163350, { .model = &D_actor_350700_8016E86C } },
    { TASK_BODY_TMD, 192, func_actor_350700_80163274, { .model = &D_actor_350700_8016F1B0 } },
    { TASK_BODY_TMD, 192, func_actor_350700_80163274, { .model = &D_actor_350700_8016ECC0 } },
    { TASK_BODY_TMD, 192, func_actor_350700_80163274, { .model = &D_actor_350700_8016F5F4 } },
};

Actor350700MsgEntry D_actor_350700_8017090C[6] = {
    { 2003, { .call1 = actorMotionPlayAnim } },
    { 2004, { .call3 = func_actor_350700_801637C4 } },
    { 2005, { .call5 = func_actor_350700_80163840 } },
    { 2013, { .call4 = actorMotionStartWalk } },
    { 2011, { .call0 = func_actor_350700_8016395C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
}; /// Per-frame tick of the enemy actor: dispatches through the local two-entry table
/// the counter at `walk.motion` indexes, then integrates the local-space `step`
/// into the 16.16 accumulators at `walk.acc`, adds their high halves to the
/// root coordinate's translation and truncates them back to 16 bits. Ticks the
/// animation slots while `model.ticking` is set, and -- unless the display object's
/// `flags` carry 0x80 -- draws the ground-shadow quad from the second
/// part's world matrix, clears that part's `composeStamp` and rebuilds its coordinate.
/// The `freeCountdown` countdown then runs while it is non-negative, freeing the
/// model buffers on the frame it reaches zero; the init's -1 disables it.
static void func_actor_350700_80161E88(Task* arg0)
{
    TmdObject*       ext      = arg0->extra.tmd;
    Actor350500Work* work     = (Actor350500Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350700_801624D0, func_actor_350700_801624D8 };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    funcs[(s16)work->walk.motion](arg0);
    coord                = arg0->extra.tmd->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
    coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
    coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
    work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
    work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive19.inc.c"

/// Spawn-placement message handler: seeds the work block's position and
/// rotation from `place`, picks the start animation from `anim` (or anim 3,
/// 2 once `field_4C4` is set) and installs it with the body of
/// `actorMotionPlayAnim19` written out inline. Returns 0.
s32 func_actor_350700_801621B4(Task* task, s32 arg1, ActorTransform* place, Actor350700SpawnAnim* anim)
{
    Actor350500Work*      work;
    Actor350500Work*      w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                   = (Actor350500Work*)task->work;
    w->walk.motion      = 1;
    w->walk.motionStep  = 0;
    w->walk.target.vx   = place->pos.vx;
    w->walk.target.vy   = place->pos.vy;
    w->walk.target.vz   = place->pos.vz;
    w->walk.rotX        = place->rot.vx;
    w->walk.rotY        = place->rot.vy;
    w->walk.rotZ        = place->rot.vz;
    preset.source.index = 0;
    if (anim != NULL) {
        preset.animationId  = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        if (w->field_4C4 != 0) {
            preset.animationId = 2;
        } else {
            preset.animationId = 3;
        }
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (Actor350500Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Per-frame dispatcher of the enemy actor: runs its spawn, tick or exit state
/// from `D_actor_350700_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350700_80162398(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350700_80161E24;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit handler. An allocation failure ends the task instead of
/// leaving a half-built actor behind.
static void func_actor_350700_80162404(Task* arg0)
{
    Actor350500Work* work;

    work = memCalloc(sizeof(Actor350500Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;

    func_actor_350700_801624B4(arg0);

    arg0->msgTable     = D_actor_350700_80169D1C;
    arg0->exitCallback = func_actor_350700_80162494;
    arg0->state       += 1;
}

/// Exit callback `func_actor_350700_80162404` installs; tears the task down.
static void func_actor_350700_80162494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the enemy work block's two matrices onto
/// `TmdObject::lightMtx` / `colorMtx`, so the actor draws with its own
/// lighting.
static void func_actor_350700_801624B4(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor350500Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Tick handler 0 of the enemy actor, selected by `walk.motion`: idle.
static void func_actor_350700_801624D0(Task* arg0)
{
}

/// Tick handler 1 of the enemy actor: runs the state handler of
/// `D_actor_350700_80161E30` that `walk.motionStep` selects.
static void func_actor_350700_801624D8(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350700_80161E30;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// State handler at index 0 of `D_actor_350700_80161E30`: turns the root part
/// toward `work->walk.target`, taking the yaw of the normalised offset from the
/// part's own translation with `ratan2` -- turned half a revolution away while
/// `field_4C4` is clear -- and rebuilding the local matrix from that yaw alone.
/// Clearing `composeStamp` makes the coordinate tree recompute the world matrix, and
/// bumping `walk.motionStep` moves on to the next handler.
static void func_actor_350700_80162540(Task* task)
{
    Actor350500Work* work;
    GfxCoord*        coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = (Actor350500Work*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;
    if (work->field_4C4 == 0) {
        rot.vy += 0x7FF;
    }

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors the parent's `func_actor_350700_80163528`: rotates the constant local-space offset
/// `D_actor_350700_80161E40` through the root part's matrix into `work->walk.step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances `walk.motionStep` so the dispatcher runs the next
/// handler. Where the parent's step rotates its offset unchanged, this one
/// shrinks it to -0.4 of its length whenever `field_4C4` is clear.
static void func_actor_350700_8016261C(Task* arg0)
{
    Actor350500Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = arg0->extra.tmd->coords;
    work  = (Actor350500Work*)arg0->work;

    vec = D_actor_350700_80161E40;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// State handler at index 3 of `D_actor_350700_80161E30`, the turn-to-face body
/// that follows `func_actor_350700_80162540`. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the target `work->walk.rotY` is at least
/// 0x61 it steps `vec.vy` toward it by 0x60 -- the step is taken on an `s32`
/// widening of the extracted yaw -- and otherwise snaps the yaw to the target
/// and plays anim 0x7D3, clearing the two body counters. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, which
/// `actorRenderComposeCoordChain` picks up once `composeStamp` is cleared.
static void func_actor_350700_80162764(Task* arg0)
{
    Actor350500Work*     work;
    GpMtxWords*          words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor350500Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x61) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x60;
        } else {
            vec.vy = vy + 0x60;
        }
    } else {
        vec.vy                      = work->walk.rotY;
        preset.source.index         = 0;
        preset.animationId          = 1;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 4;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        actorMotionPlayAnim19(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 sets the 0x80 flag, under which the tick skips the shadow and the
/// part update, and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 1 clears 0x80, allocates the model
/// buffers and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`; 2 sets 0x80 and `TMD_OBJECT_SKIP_AUTO_BUFFER` and latches the mode into the
/// `freeCountdown` countdown, which frees the buffers when it runs out; 3 clears
/// 0x80 and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. Anything else returns 1 and leaves the object alone; the
/// handled modes return 0.
s32 func_actor_350700_80162A14(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
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
            obj->flags                                   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Actor350500Work*)task->work)->freeCountdown = mode;
            obj->flags                                   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// `Gp_DispatchMsg` handler: latches the variant the message's halfword at
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

    work = (Actor135600Work*)memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work          = work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;
    spawned             = Task_SpawnFromTable(D_actor_350700_801708DC, 1, 8, arg0);
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
    coord                = arg0->extra.tmd->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
    coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
    coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
    work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
    work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
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
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback `func_actor_350700_80162B30` installs, the same
/// `Gp_EnemyTaskExit` teardown `func_actor_350700_80162494` performs.
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

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`,
/// then the resulting flags are republished onto the objects of the three
/// child tasks the spawn handler parked at `child0` / `child1` /
/// `child2`. The modes are those of `func_actor_350700_80162A14`, the
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
