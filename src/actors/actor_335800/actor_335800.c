#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/dryfield_night_motel_balcony.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"
#include "../../shared/model_placement.h"

/// Work block of the overlay's walker, allocated zeroed by its spawn routine
/// and kept at `Task::work`: a nineteen-part rig, the walk state, and
/// `freeCountdown`, the frames until the model buffers are freed, -1
/// disabling the countdown.
typedef struct Actor335800Work {
    ActorAnimRig19  rig;
    ActorModelState model;
    ActorWalkState  walk;
    s16             freeCountdown;
    byte            pad_4C6[0x2];
} Actor335800Work;
STATIC_ASSERT_SIZEOF(Actor335800Work, 0x4C8);

/// Work block of the overlay's parent walker, allocated zeroed by its spawn
/// routine and kept at `Task::work`: a twenty-part rig and the walk state,
/// the two child tasks the spawn routine starts, whose models the visibility
/// command drives alongside the walker's, `freeCountdown`, the frames until
/// the model buffers are freed, -1 disabling the countdown, and `field_508`,
/// the flag bits of the last animation record the tick latched.
typedef struct Actor335800MainWork {
    ActorAnimRig20  rig;
    ActorModelState model;
    ActorWalkState  walk;
    Task*           child0;
    Task*           child1;
    s16             field_504;
    s16             freeCountdown;
    s32             field_508;
} Actor335800MainWork;
STATIC_ASSERT_SIZEOF(Actor335800MainWork, 0x50C);

/// Optional start animation for `actorMotionStartWalk`: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are 0xD and 1.
typedef GpSpawnAnimArg Actor335800SpawnAnim;

extern ActorTransform D_actor_335800_80164F80;

extern TaskDesc             D_actor_335800_80164DE0[];
extern AnimationPlayRequest D_actor_335800_80164E7C;

/// The warp-payload table the two dispatchers reach by entry:
/// `func_actor_335800_801621B4` selects an entry of it by index.
extern ActorTransform D_actor_335800_80164EA4[5];
extern GpEvsCmd       D_actor_335800_80165FC0[];
extern GpEvsCmd       D_actor_335800_80166098[];

/// Animation bank tables of the parent and the child block.
extern AnimationSet*  D_actor_335800_8016EAC4[5];
extern AnimationSet** gActorMotionAnimBanks[1];
extern AnimationSet*  D_actor_335800_80172E90[2];
extern AnimationSet** D_actor_335800_80172E98[1];

/// The two part tasks the parent block spawns, and its message table; both
/// live in this overlay's trailing data.
extern TaskDesc D_actor_335800_8016EADC[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call2)(Task*, s32, ActorCommand* request, s32);
        s32 (*call3)(Task*, s32, ActorTransform*);
        s32 (*call4)(Task*, s32, ActorTransform*, Actor335800SpawnAnim*);
        s32 (*call5)(Task*, s32, s32);
    } handler;
} Actor335800MsgEntry;
STATIC_ASSERT_SIZEOF(Actor335800MsgEntry, 8);

extern Actor335800MsgEntry D_actor_335800_8016EB00[];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_335800_80163AA0`; terminator id 0x7FFFFFFF.
extern Actor335800MsgEntry D_actor_335800_80172EA8[];

static void func_actor_335800_80162640(Task* arg0);
static void func_actor_335800_80162844(Task* task);
static void func_actor_335800_80162F08(Task* task);
static void func_actor_335800_80162F7C(Task* arg0);
static void func_actor_335800_80162F9C(Task* arg0);
static void func_actor_335800_80162FF4(Task* arg0);
static void func_actor_335800_80162FFC(Task* task);
static void func_actor_335800_80163124(Task* task);
static void func_actor_335800_80163568(Task* task);
static void func_actor_335800_80163AA0(Task* arg0);
static void func_actor_335800_80163B34(Task* arg0);
static void func_actor_335800_80163B54(Task* arg0);
static void func_actor_335800_80163B70(Task* arg0);
static void func_actor_335800_80163B78(Task* arg0);
static void func_actor_335800_80163BE0(Task* task);
static void func_actor_335800_80163CA0(Task* task);
static void func_actor_335800_80163D20(Task* arg0);

/// Spawn, tick and teardown handlers of the two part tasks the parent block
/// spawns, dispatched by `func_actor_335800_80162E34`.
static const TaskFuncTable3 D_actor_335800_80161E24 = { {
    modelPlacementAttachPart,
    func_actor_335800_80162F08,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent block, dispatched by
/// `func_actor_335800_80162F10`.
static const TaskFuncTable3 D_actor_335800_80161E30 = { {
    func_actor_335800_80162640,
    func_actor_335800_80162844,
    func_actor_335800_80162F7C,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_335800_80161E3C = { {
    actorMotionFaceTarget,
    func_actor_335800_80163124,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The constant local-space offset `func_actor_335800_80163124` rotates for
/// the parent block: straight ahead along the part's own +Z.
static const VECTOR D_actor_335800_80161E4C = { 0, 0, 0x200000, 0 };

/// Spawn, tick and teardown handlers of the child block, dispatched by
/// `func_actor_335800_80163A34`.
static const TaskFuncTable3 D_actor_335800_80161E5C = { {
    func_actor_335800_80163AA0,
    func_actor_335800_80163568,
    func_actor_335800_80163B34,
} };

/// Step handlers of the child block's motion sequence, indexed by
/// `ActorWalkState::motionStep`, in the same order as the parent's.
static const TaskFuncTable4 D_actor_335800_80161E68 = { {
    func_actor_335800_80163BE0,
    func_actor_335800_80163CA0,
    actorMotionArrive19,
    func_actor_335800_80163D20,
} };

/// The child block's copy of the forward offset, rotated by
/// `func_actor_335800_80163CA0`.
static const VECTOR D_actor_335800_80161E78 = { 0, 0, 0x200000, 0 };

void func_actor_335800_80161E88(Task*);
void func_actor_335800_80162364(Task*);
void func_actor_335800_801624DC(Task*);
void func_actor_335800_80162588(Task*);

extern ActorTransform       D_actor_335800_80164F80;
extern AnimationPlayRequest D_actor_335800_80164E54;
extern AnimationPlayRequest D_actor_335800_80164E90;
extern AnimationPlayRequest D_actor_335800_80164F44;
extern AnimationPlayRequest D_actor_335800_80164F58;
extern AnimationPlayRequest D_actor_335800_80164F6C;
extern ActorCommand         D_actor_335800_80164FC8;
extern GpCopyArg            D_actor_335800_80164E24;
extern ActorTransform       D_actor_335800_80164EA4[5];
extern ActorTransform       D_actor_335800_80164F98;
extern ActorTransform       D_actor_335800_80164FB0;
void                        func_actor_335800_80162040(void);
void                        func_actor_335800_80162060(void);
void                        func_actor_335800_80162080(void);
void                        func_actor_335800_801620C0(void);
void                        func_actor_335800_801623D8(void);
void                        func_actor_335800_80162408(void);
void                        func_actor_335800_801624B8(s32);
void                        func_actor_335800_80162558(void);

extern AnimationPlayRequest D_actor_335800_80164E40;
extern AnimationPlayRequest D_actor_335800_80164E7C;
extern AnimationPlayRequest D_actor_335800_80164E90;
extern ActorCommand         D_actor_335800_8016502C;
extern ActorCommand         D_actor_335800_80165038;
extern ActorCommand         D_actor_335800_8016503C;
extern ActorCommand         D_actor_335800_80165040;
extern ActorCommand         D_actor_335800_80165044;
extern GpCopyArg            D_actor_335800_80164E24;
extern EvsSceneKey          D_actor_335800_80165050;
extern EvsSceneKey          D_actor_335800_80165058;
extern ActorTransform       D_actor_335800_80164EA4[5];
s32                         func_actor_335800_8016343C(Task*, s32, s32);
s32                         func_actor_335800_8016354C(Task*, s32, ActorCommand* request, s32);
s32                         func_actor_335800_80163880(Task*, s32, ActorTransform* place, Actor335800SpawnAnim*);
s32                         func_actor_335800_80163F3C(Task*, s32, ActorTransform* args, s32 arg3);
s32                         func_actor_335800_80163FB8(Task*, s32, s32);
s32                         func_actor_335800_80164098(void);
void                        func_actor_335800_80162040(void);
void                        func_actor_335800_80162060(void);
void                        func_actor_335800_80162080(void);
void                        func_actor_335800_801620A0(void);
void                        func_actor_335800_801620F0(u8);
void                        func_actor_335800_80162114(void);
void                        func_actor_335800_801621B4(s32);
void                        func_actor_335800_8016224C(void);
void                        func_actor_335800_801622C0(s32);
void                        func_actor_335800_80162408(void);
void                        func_actor_335800_80162428(s8);
void                        func_actor_335800_80162434(s32);
void                        func_actor_335800_80162460(void);
void                        func_actor_335800_80162484(void);
void                        func_actor_335800_801624B8(s32);
void                        func_actor_335800_80162558(void);
void                        func_actor_335800_80162E34(Task*);
void                        func_actor_335800_80162F10(Task*);
void                        func_actor_335800_80163A34(Task*);

AnimationPackedPose D_actor_335800_801640A0[6] = {
#include "assets/actor_335800_animation_0255C_bank1.inc"
};

AnimationPackedRotation D_actor_335800_801640E8[46] = {
#include "assets/actor_335800_animation_0255C_bank4.inc"
};

AnimationRecord D_actor_335800_801641A0[109] = {
#include "assets/actor_335800_animation_0255C_records.inc"
};

u16 D_actor_335800_80164354[20] = {
#include "assets/actor_335800_animation_0255C_indices.inc"
};

AnimationSet D_actor_335800_8016437C = {
    D_actor_335800_801641A0,
    D_actor_335800_80164354,
    { NULL, D_actor_335800_801640A0, NULL, NULL, D_actor_335800_801640E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_335800_801643A4[12] = {
#include "assets/actor_335800_animation_02B78_bank1.inc"
};

AnimationPackedRotation D_actor_335800_80164434[146] = {
#include "assets/actor_335800_animation_02B78_bank4.inc"
};

AnimationRecord D_actor_335800_8016467C[189] = {
#include "assets/actor_335800_animation_02B78_records.inc"
};

u16 D_actor_335800_80164970[20] = {
#include "assets/actor_335800_animation_02B78_indices.inc"
};

AnimationSet D_actor_335800_80164998 = {
    D_actor_335800_8016467C,
    D_actor_335800_80164970,
    { NULL, D_actor_335800_801643A4, NULL, NULL, D_actor_335800_80164434, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_335800_801649C0[2] = {
#include "assets/actor_335800_animation_02D8C_bank1.inc"
};

AnimationPackedRotation D_actor_335800_801649D8[24] = {
#include "assets/actor_335800_animation_02D8C_bank4.inc"
};

AnimationRecord D_actor_335800_80164A38[83] = {
#include "assets/actor_335800_animation_02D8C_records.inc"
};

u16 D_actor_335800_80164B84[20] = {
#include "assets/actor_335800_animation_02D8C_indices.inc"
};

AnimationSet D_actor_335800_80164BAC = {
    D_actor_335800_80164A38,
    D_actor_335800_80164B84,
    { NULL, D_actor_335800_801649C0, NULL, NULL, D_actor_335800_801649D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_335800_80164BD4[2] = {
#include "assets/actor_335800_animation_02F98_bank1.inc"
};

AnimationPackedRotation D_actor_335800_80164BEC[23] = {
#include "assets/actor_335800_animation_02F98_bank4.inc"
};

AnimationRecord D_actor_335800_80164C48[82] = {
#include "assets/actor_335800_animation_02F98_records.inc"
};

u16 D_actor_335800_80164D90[20] = {
#include "assets/actor_335800_animation_02F98_indices.inc"
};

AnimationSet D_actor_335800_80164DB8 = {
    D_actor_335800_80164C48,
    D_actor_335800_80164D90,
    { NULL, D_actor_335800_80164BD4, NULL, NULL, D_actor_335800_80164BEC, NULL, NULL, NULL },
};

TaskDesc D_actor_335800_80164DE0[4] = {
    { TASK_BODY_COORD, 192, func_actor_335800_80161E88, { .model = NULL } },
    { 0, 192, func_actor_335800_801624DC, { .model = NULL } },
    { 0, 192, func_actor_335800_80162364, { .model = NULL } },
    { 0, 192, func_actor_335800_80162588, { .model = NULL } },
};

AnimationSet* D_actor_335800_80164E10[5] = {
    NULL,
    &D_actor_335800_8016437C,
    &D_actor_335800_80164998,
    &D_actor_335800_80164BAC,
    &D_actor_335800_80164DB8,
};

GpCopyArg D_actor_335800_80164E24 = { { .sets = D_actor_335800_80164E10 }, 5 };

AnimationPlayRequest D_actor_335800_80164E2C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164E40 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E54 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E68 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164E7C = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_335800_80164E90 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_335800_80164EA4[5] = {
    { { -7180, -3200, 4310, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 1000, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 5570, 0 }, { 0, 1024, 0, 0 } },
    { { -6280, -3200, 1000, 0 }, { 0, 2047, 0, 0 } },
    { { -6280, -3200, 5570, 0 }, { 0, 2047, 0, 0 } },
};

AnimationPlayRequest D_actor_335800_80164F1C[2] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_335800_80164F44 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164F58 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164F6C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_335800_80164F80 = { { -4000, 0, 1970, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_actor_335800_80164F98 = { { -7730, 0, 1280, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_335800_80164FB0 = { { -6260, 0, 1280, 0 }, { 0, -1024, 0, 0 } };

ActorCommand D_actor_335800_80164FC8 = { { .loc = { 3, 29 } }, 0 };

AnimationPlayRequest D_actor_335800_80164FCC = { 0 };

AnimationPlayRequest D_actor_335800_80164FE0 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_335800_80164FF4 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_335800_80165008 = { { -1287, 0, -2750, 0 }, { 0, -1650, 0, 0 } };

ActorCommand D_actor_335800_80165020 = { { .loc = { 3, 29 } }, 1 };

ActorCommand D_actor_335800_80165024 = { { .loc = { 3, 29 } }, 2 };

ActorCommand D_actor_335800_80165028 = { { .loc = { 3, 29 } }, 3 };

ActorCommand D_actor_335800_8016502C = { { .loc = { 3, 29 } }, 0xFFFF };

ActorCommand D_actor_335800_80165030 = { { .loc = { 3, 29 } }, 4 };

ActorCommand D_actor_335800_80165034 = { { .loc = { 3, 29 } }, 5 };

ActorCommand D_actor_335800_80165038 = { { .loc = { 3, 29 } }, 6 };

ActorCommand D_actor_335800_8016503C = { { .loc = { 3, 29 } }, 7 };

ActorCommand D_actor_335800_80165040 = { { .loc = { 3, 29 } }, 8 };

ActorCommand D_actor_335800_80165044 = { { .loc = { 3, 29 } }, 10 };

EvsSceneKey D_actor_335800_80165048 = { 3, 58, 11 };

EvsSceneKey D_actor_335800_80165050 = { 3, 59, 11 };

EvsSceneKey D_actor_335800_80165058 = { 3, 60, 11 };

GpEvsCmd D_actor_335800_80165060[72] = {
    { 13, { .callback = func_actor_335800_801624B8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_801623D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162408 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_335800_80165048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162040 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162558 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_335800_80164E24 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = D_actor_335800_80164EA4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 3, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E54 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165034 } }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162060 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 49 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165020 } }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 3, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165024 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2004 }, { .storage = &D_actor_335800_80165008 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_335800_80164FE0 }, { .value = 0 } },
    { 4, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_335800_80164FF4 }, { .value = 0 } },
    { 4, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_335800_80164FF4 }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_335800_80164FF4 }, { .value = 0 } },
    { 4, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2003 }, { .storage = &D_actor_335800_80164FF4 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_335800_80164F80 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_335800_80164F44 }, { .value = 0 } },
    { 3, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_801620C0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165030 } }, { .value = 0 } },
    { 4, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80164FC8 } }, { .value = 0 } },
    { 4, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_335800_80164F98 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_335800_80164F58 }, { .value = 0 } },
    { 4, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165028 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_actor_335800_80164FB0 }, { .value = 0 } },
    { 3, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_actor_335800_80164F6C }, { .value = 0 } },
    { 4, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E90 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016502C } }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80165720[5] = {
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80165798[19] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_801620A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801624B8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162484 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E90 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = D_actor_335800_80164EA4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016502C } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 48, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162408 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80165960[21] = {
    { 13, { .callbackNoArg = func_actor_335800_80162484 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_335800_80164E24 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E40 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162114 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162558 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162060 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801622C0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801622C0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU8 = func_actor_335800_801620F0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162460 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 45, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80165B58[26] = {
    { 3, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_335800_80164E24 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_8016503C } }, { .value = 0 } },
    { 4, { .value = 270 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165040 } }, { .value = 0 } },
    { 4, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801624B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_8016224C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801621B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS8 = func_actor_335800_80162428 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 45, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80165DC8[21] = {
    { 3, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_335800_80164E24 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165044 } }, { .value = 0 } },
    { 4, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801621B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x401F000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 1 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_801624B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_8016224C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 45, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80165FC0[9] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_335800_80164E24 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_335800_80165050 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162040 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165038 } }, { .value = 0 } },
    { 44, { .commands = D_actor_335800_80165960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_335800_80165B58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_335800_80166098[10] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_actor_335800_80164E24 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_335800_80164E7C }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_335800_80162434 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 12, { .overlays = &D_actor_335800_80165058 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_335800_80162040 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_335800_80165038 } }, { .value = 0 } },
    { 44, { .commands = D_actor_335800_80165960 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 44, { .commands = D_actor_335800_80165DC8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_335800_80166188[20] = {
#include "assets/actor_335800_model_09DD4_skeleton.inc"
};

u32 D_actor_335800_80166458[20] = {
#include "assets/actor_335800_model_09DD4_partVerts.inc"
};

SVECTOR D_actor_335800_801664A8[364] = {
#include "assets/actor_335800_model_09DD4_verts.inc"
};

SVECTOR D_actor_335800_80167008[354] = {
#include "assets/actor_335800_model_09DD4_normals.inc"
};

u32 D_actor_335800_80167B18[4151] = {
#include "assets/actor_335800_model_09DD4_stream.inc"
};

TmdSource D_actor_335800_8016BBF4 = {
    0,
    22008,
    6952,
    20,
    D_actor_335800_80166458,
    D_actor_335800_801664A8,
    D_actor_335800_80167008,
    D_actor_335800_80166188,
    D_actor_335800_80167B18,
};

TmdBone D_actor_335800_8016BC18[1] = {
#include "assets/actor_335800_model_0A2AC_skeleton.inc"
};

u32 D_actor_335800_8016BC3C[1] = {
#include "assets/actor_335800_model_0A2AC_partVerts.inc"
};

SVECTOR D_actor_335800_8016BC40[21] = {
#include "assets/actor_335800_model_0A2AC_verts.inc"
};

SVECTOR D_actor_335800_8016BCE8[21] = {
#include "assets/actor_335800_model_0A2AC_normals.inc"
};

u32 D_actor_335800_8016BD90[207] = {
#include "assets/actor_335800_model_0A2AC_stream.inc"
};

TmdSource D_actor_335800_8016C0CC = {
    0,
    1352,
    0,
    1,
    D_actor_335800_8016BC3C,
    D_actor_335800_8016BC40,
    D_actor_335800_8016BCE8,
    D_actor_335800_8016BC18,
    D_actor_335800_8016BD90,
};

TmdBone D_actor_335800_8016C0F0[1] = {
#include "assets/actor_335800_model_0A8A0_skeleton.inc"
};

u32 D_actor_335800_8016C114[1] = {
#include "assets/actor_335800_model_0A8A0_partVerts.inc"
};

SVECTOR D_actor_335800_8016C118[34] = {
#include "assets/actor_335800_model_0A8A0_verts.inc"
};

SVECTOR D_actor_335800_8016C228[28] = {
#include "assets/actor_335800_model_0A8A0_normals.inc"
};

u32 D_actor_335800_8016C308[238] = {
#include "assets/actor_335800_model_0A8A0_stream.inc"
};

TmdSource D_actor_335800_8016C6C0 = {
    0,
    1692,
    0,
    1,
    D_actor_335800_8016C114,
    D_actor_335800_8016C118,
    D_actor_335800_8016C228,
    D_actor_335800_8016C0F0,
    D_actor_335800_8016C308,
};

AnimationPackedPose D_actor_335800_8016C6E4[2] = {
#include "assets/actor_335800_animation_0AB18_bank1.inc"
};

AnimationPackedRotation D_actor_335800_8016C6FC[32] = {
#include "assets/actor_335800_animation_0AB18_bank4.inc"
};

AnimationRecord D_actor_335800_8016C77C[101] = {
#include "assets/actor_335800_animation_0AB18_records.inc"
};

u16 D_actor_335800_8016C910[20] = {
#include "assets/actor_335800_animation_0AB18_indices.inc"
};

AnimationSet D_actor_335800_8016C938 = {
    D_actor_335800_8016C77C,
    D_actor_335800_8016C910,
    { NULL, D_actor_335800_8016C6E4, NULL, NULL, D_actor_335800_8016C6FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_335800_8016C960[11] = {
#include "assets/actor_335800_animation_0B59C_bank1.inc"
};

AnimationPackedRotation D_actor_335800_8016C9E4[255] = {
#include "assets/actor_335800_animation_0B59C_bank4.inc"
};

AnimationRecord D_actor_335800_8016CDE0[365] = {
#include "assets/actor_335800_animation_0B59C_records.inc"
};

u16 D_actor_335800_8016D394[20] = {
#include "assets/actor_335800_animation_0B59C_indices.inc"
};

AnimationSet D_actor_335800_8016D3BC = {
    D_actor_335800_8016CDE0,
    D_actor_335800_8016D394,
    { NULL, D_actor_335800_8016C960, NULL, NULL, D_actor_335800_8016C9E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_335800_8016D3E4[34] = {
#include "assets/actor_335800_animation_0CA48_bank1.inc"
};

AnimationPackedRotation D_actor_335800_8016D57C[546] = {
#include "assets/actor_335800_animation_0CA48_bank4.inc"
};

AnimationRecord D_actor_335800_8016DE04[655] = {
#include "assets/actor_335800_animation_0CA48_records.inc"
};

u16 D_actor_335800_8016E840[20] = {
#include "assets/actor_335800_animation_0CA48_indices.inc"
};

AnimationSet D_actor_335800_8016E868 = {
    D_actor_335800_8016DE04,
    D_actor_335800_8016E840,
    { NULL, D_actor_335800_8016D3E4, NULL, NULL, D_actor_335800_8016D57C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_335800_8016E890[4] = {
#include "assets/actor_335800_animation_0CC7C_bank1.inc"
};

AnimationPackedRotation D_actor_335800_8016E8C0[35] = {
#include "assets/actor_335800_animation_0CC7C_bank4.inc"
};

AnimationRecord D_actor_335800_8016E94C[74] = {
#include "assets/actor_335800_animation_0CC7C_records.inc"
};

u16 D_actor_335800_8016EA74[20] = {
#include "assets/actor_335800_animation_0CC7C_indices.inc"
};

AnimationSet D_actor_335800_8016EA9C = {
    D_actor_335800_8016E94C,
    D_actor_335800_8016EA74,
    { NULL, D_actor_335800_8016E890, NULL, NULL, D_actor_335800_8016E8C0, NULL, NULL, NULL },
};

AnimationSet* D_actor_335800_8016EAC4[5] = {
    NULL,
    &D_actor_335800_8016C938,
    &D_actor_335800_8016D3BC,
    &D_actor_335800_8016E868,
    &D_actor_335800_8016EA9C,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_335800_8016EAC4,
};

TaskDesc D_actor_335800_8016EADC[3] = {
    { (TASK_BODY_TMD | 0x100), 192, func_actor_335800_80162F10, { .model = &D_actor_335800_8016BBF4 } },
    { TASK_BODY_TMD, 192, func_actor_335800_80162E34, { .model = &D_actor_335800_8016C0CC } },
    { TASK_BODY_TMD, 192, func_actor_335800_80162E34, { .model = &D_actor_335800_8016C6C0 } },
};

Actor335800MsgEntry D_actor_335800_8016EB00[6] = {
    { 2003, { .call1 = actorMotionPlayAnim } },
    { 2004, { .call3 = actorMsgPlaceEuler } },
    { 2005, { .call5 = func_actor_335800_8016343C } },
    { 2013, { .call4 = actorMotionStartWalk } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_335800_8016354C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TmdBone D_actor_335800_8016EB30[19] = {
#include "assets/actor_335800_model_10A68_skeleton.inc"
};

u32 D_actor_335800_8016EDDC[19] = {
#include "assets/actor_335800_model_10A68_partVerts.inc"
};

SVECTOR D_actor_335800_8016EE28[238] = {
#include "assets/actor_335800_model_10A68_verts.inc"
};

SVECTOR D_actor_335800_8016F598[238] = {
#include "assets/actor_335800_model_10A68_normals.inc"
};

u32 D_actor_335800_8016FD08[2784] = {
#include "assets/actor_335800_model_10A68_stream.inc"
};

TmdSource D_actor_335800_80172888 = {
    0,
    13636,
    6016,
    19,
    D_actor_335800_8016EDDC,
    D_actor_335800_8016EE28,
    D_actor_335800_8016F598,
    D_actor_335800_8016EB30,
    D_actor_335800_8016FD08,
};

AnimationPackedPose D_actor_335800_801728AC[7] = {
#include "assets/actor_335800_animation_11048_bank1.inc"
};

AnimationPackedRotation D_actor_335800_80172900[128] = {
#include "assets/actor_335800_animation_11048_bank4.inc"
};

AnimationRecord D_actor_335800_80172B00[208] = {
#include "assets/actor_335800_animation_11048_records.inc"
};

u16 D_actor_335800_80172E40[20] = {
#include "assets/actor_335800_animation_11048_indices.inc"
};

AnimationSet D_actor_335800_80172E68 = {
    D_actor_335800_80172B00,
    D_actor_335800_80172E40,
    { NULL, D_actor_335800_801728AC, NULL, NULL, D_actor_335800_80172900, NULL, NULL, NULL },
};

AnimationSet* D_actor_335800_80172E90[2] = {
    NULL,
    &D_actor_335800_80172E68,
};

AnimationSet** D_actor_335800_80172E98[1] = {
    D_actor_335800_80172E90,
};

TaskDesc D_actor_335800_80172E9C = { (TASK_BODY_TMD | 0x100), 192, func_actor_335800_80163A34, { .model = &D_actor_335800_80172888 } };

Actor335800MsgEntry D_actor_335800_80172EA8[6] = {
    { 2003, { .call1 = actorMotionPlayAnim19 } },
    { 2004, { .call3 = func_actor_335800_80163F3C } },
    { 2005, { .call5 = func_actor_335800_80163FB8 } },
    { 2013, { .call4 = func_actor_335800_80163880 } },
    { 2011, { .call0 = func_actor_335800_80164098 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

static inline void _actor335800SetView(s32 view);

/// Places the root part 1000 units short of its pose `D_actor_335800_80164F80`
/// and moves it along Z at `killCountdown` (100) per frame; once past the pose
/// height the velocity drops by 6 a frame until it falls below -60, which ends
/// the move. A ground shadow is drawn every frame. The
/// task kills itself once the session's `viewReady` flag is set, or a few
/// frames into state 2.
void func_actor_335800_80161E88(Task* task)
{
    GfxCoord* coord;
    VECTOR3   pos;
    SVECTOR*  rot;

    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case 0:
            coord->coord.t[0]   = D_actor_335800_80164F80.pos.vx;
            coord->coord.t[1]   = D_actor_335800_80164F80.pos.vy;
            coord->coord.t[2]   = D_actor_335800_80164F80.pos.vz - 1000;
            rot                 = &D_actor_335800_80164F80.rot;
            coord->param.rot.vx = rot->vx;
            coord->param.rot.vy = rot->vy;
            coord->param.rot.vz = rot->vz;
            RotMatrix(&coord->param.rot, &coord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->killCountdown = 100;
            task->state++;
        case 1:
            if (D_actor_335800_80164F80.pos.vz < coord->coord.t[2]) {
                task->killCountdown -= 6;
                if (task->killCountdown < -60) {
                    task->killCountdown = 0;
                    task->state++;
                }
            }
            coord->coord.t[2] += task->killCountdown;
            if (gGameSession->viewReady != 0) {
                taskKill(task);
            }
            break;
        case 2:
            if (++task->killCountdown < 4) {
                if (gGameSession->viewReady != 0) {
                    taskKill(task);
                }
            } else {
                taskKill(task);
            }
            break;
        default:
            taskKill(task);
            break;
    }
    if (func_800EA1A8(MATRIX_TRANS(&task->extra.coordBody->coord->workm), &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x800, gRoomEffectState->groundShadowShade);
    }
}

/// Script callback: queues the replacement overlay load.
void func_actor_335800_80162040(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_335800_80162060(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state.
void func_actor_335800_80162080(void)
{
    Gp_RestoreStreamRng();
}

/// Script callback: cancels the pending overlay replacement and activates the
/// loaded one.
void func_actor_335800_801620A0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_335800_801620C0(void)
{
    Task_SpawnFromTable(D_actor_335800_80164DE0, 0, 0, 0);
}

void func_actor_335800_801620F0(u8 arg0)
{
    gGameSession->location.loc.room = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = arg0;
    gGameSession->roomObjsDirty                                                                  = 1;
}

void func_actor_335800_80162114(void)
{
    Task*      slot;
    TmdObject* extra;
    GfxCoord*  coord;

    slot = gameGetPtrSlot(3);
    if (slot != NULL) {
        extra = slot->extra.tmd;
        coord = extra->coords;
        if ((u32)(coord->coord.t[2] - 0xC53) < 0x96F) {
            Gp_DispatchMsgPtr(slot, 0x3E9, &D_actor_335800_80164EA4[2], 0);
        }
        if ((u32)(coord->coord.t[2] - 0x3E9) < 0x86A) {
            Gp_DispatchMsgPtr(slot, 0x3E9, &D_actor_335800_80164EA4[1], 0);
        }
    }
}

void func_actor_335800_801621B4(s32 arg0)
{
    Task*     slot;
    GfxCoord* coord;
    s32       lowIdx;
    s32       highIdx;

    slot = gameGetPtrSlot(3);
    if (slot != NULL) {
        coord  = slot->extra.tmd->coords;
        lowIdx = 1;
        if (arg0 != 0) {
            highIdx = 2;
        } else {
            lowIdx  = 3;
            highIdx = 4;
        }
        if (coord->coord.t[2] >= 0xC53) {
            Gp_DispatchMsgPtr(slot, 0x3E9, &D_actor_335800_80164EA4[highIdx], 0);
        } else {
            Gp_DispatchMsgPtr(slot, 0x3E9, &D_actor_335800_80164EA4[lowIdx], 0);
        }
    }
}

/// Moves the saved and the live location to `view` and marks the view and
/// the room objects for reloading.
static inline void _actor335800SetView(s32 view)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = view;
    gGameSession->location.loc.view                            = view;
    gGameSession->viewDirty                                    = 1;
    gGameSession->roomObjsDirty                                = 1;
}

void func_actor_335800_8016224C(void)
{
    Task* slot;
    s16   view;

    slot = gameGetPtrSlot(3);
    if (slot != NULL) {
        view = 6;
        if (slot->extra.tmd->coords->coord.t[2] >= 0xC53) {
            view = 5;
        }
        _actor335800SetView(view);
    }
}

/// Sets `SpriteBatch::hidden` on two sprite commands of the area's 39th view
/// record: 1 keeps their sprites out of the ordering table and also sets game
/// flag 0x7F's nibble to 1, 0 draws them again.
void func_actor_335800_801622C0(s32 arg0)
{
    GameSession*     g;
    GameLocationKey* sess;
    GpSprtRec*       rec;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    rec  = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1];
    switch (arg0) {
        case 0:
            batches           = rec[38].field_4;
            batches[2].hidden = 0;
            batches[3].hidden = 0;
            break;
        case 1:
            batches           = rec[38].field_4;
            batches[2].hidden = arg0;
            batches[3].hidden = arg0;
            GameFlag_SetNibble(0x7F, 1);
            break;
    }
}

void func_actor_335800_80162364(Task* arg0)
{
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value != 0) {
            func_800E8614(D_actor_335800_80166098, 0);
        } else {
            func_800E8614(D_actor_335800_80165FC0, 0);
        }
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_actor_335800_801623D8(void)
{
    Task_SpawnFromTable(D_dryfield_night_motel_balcony_80182834, 0, 0, 0);
}

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

void func_actor_335800_80162428(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

void func_actor_335800_80162434(s32 arg0)
{
    SndEvt_EnqueueType2(gStageSceneMusicEntry, arg0 & 0xFFFF);
}

void func_actor_335800_80162460(void)
{
    func_800E3FAC(0xA2, 0x18);
}

void func_actor_335800_80162484(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_actor_335800_801624B8(s32 arg0)
{
    GameFlag_SetNibble(0x108, arg0);
}

void func_actor_335800_801624DC(Task* arg0)
{
    Task* slot;

    if (gGameSession->battleResetPending != 0) {
        slot = gameGetPtrSlot(3);
        Gp_PlayerWeaponId(&D_actor_335800_80164E7C.source.index);
        Gp_DispatchMsgPtr(slot, ANIMATION_MESSAGE_PLAY, &D_actor_335800_80164E7C, 0);
        taskKill(arg0);
    }
}

void func_actor_335800_80162558(void)
{
    Task_SpawnFromTable(D_actor_335800_80164DE0, 3, 0, 0);
}

void func_actor_335800_80162588(Task* arg0)
{
    s8  var_a0;
    u8  temp_v1;
    s32 count;

    if ((gGameSession->eventState != 0) && (gGameSession->evtSkipped == 0)) {
        temp_v1 = gGameSession->padScriptFlags;
        var_a0  = 0;
        if (temp_v1 & GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE) {
            count  = (u16)arg0->killCountdown;
            var_a0 = count & 1;
        }
        if ((temp_v1 & GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE) && !(arg0->killCountdown & 1)) {
            var_a0 = -1;
        }
        displaySetShakeY(var_a0);
        arg0->killCountdown += 1;
        return;
    }
    displaySetShakeY(0);
    taskKill(arg0);
}

static void func_actor_335800_80162640(Task* arg0)
{
    Actor335800MainWork* work;
    GameLocationKey      key;
    GameLocationKey*     sessionKey;
    GameLocationKey*     keyAddr;
    Task*                spawned;

    work = (Actor335800MainWork*)memCalloc(0x50C, false);
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
    spawned             = Task_SpawnFromTable(D_actor_335800_8016EADC, 1, 4, arg0);
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
    spawned = Task_SpawnFromTable(D_actor_335800_8016EADC, 2, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        work->child1 = spawned;
        model        = spawned->extra.tmd;
        idx          = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey   = (keyAddr = &gGameSession->location.loc);
        key.stage    = sessionKey->stage;
        key.area     = sessionKey->area;
        key.room     = keyAddr->room;
        key.view     = gGameSession->location.loc.view;
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
    func_actor_335800_80162F9C(arg0);
    arg0->msgTable     = D_actor_335800_8016EB00;
    arg0->exitCallback = func_actor_335800_80162F7C;
    arg0->state       += 1;
}

static void func_actor_335800_80162844(Task* task)
{
    TmdObject*             ext      = task->extra.tmd;
    Actor335800MainWork*   work     = (Actor335800MainWork*)task->work;
    TaskFunc               funcs[2] = { func_actor_335800_80162FF4, func_actor_335800_80162FFC };
    VECTOR3                pos;
    GfxCoord*              coord;
    const AnimationRecord* rec;
    s32                    i;
    s32                    j;

    funcs[work->walk.motion](task);
    coord                = task->extra.tmd->coords;
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
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->rig.anim, i);
            }
            rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_508 & ANIMATION_RECORD_CUE_2)) {
                    Gp_SpawnEff(0x600A1, &task->extra.tmd->coords[8], 0xD, NULL);
                }
                work->field_508 = rec->flags & ANIMATION_RECORD_CUE_MASK;
            }
        }
        if (work->field_504 == 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->model.color.m[i][j] >>= 1;
                    work->model.light.m[i][j] >>= 1;
                }
                work->model.color.t[i] >>= 1;
                work->model.light.t[i] >>= 1;
            }
            work->field_504 = -1;
        }
        if (work->field_504 > 0) {
            if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
                Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
            }
        }
    }
    if (gGameSession->viewReady != 0) {
        work->field_504 = 1;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
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
/// onto the stack and runs the entry `Task::state` selects.
void func_actor_335800_80162E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach_part.inc.c"

static void func_actor_335800_80162F08(Task* task)
{
}

/// State dispatcher of the parent block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80162F10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E30;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

static void func_actor_335800_80162F7C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

static void func_actor_335800_80162F9C(Task* arg0)
{
    TmdObject*           ext;
    Actor335800MainWork* work;

    work          = (Actor335800MainWork*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
    func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    work->field_504 = 1;
}

static void func_actor_335800_80162FF4(Task* arg0)
{
}

/// Motion handler 1 of the parent block: copies the step-handler table onto
/// the stack and runs the entry `walk.motionStep` selects.
static void func_actor_335800_80162FFC(Task* task)
{
    Actor335800MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor335800MainWork*)task->work;
    fns  = D_actor_335800_80161E3C;
    fns.funcs[work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Step 1: rotates the constant forward offset `D_actor_335800_80161E4C`
/// through the root part's matrix into `work->walk.step`, opens the per-axis stop
/// threshold to 0x7FFF, which disables it, and advances the step.
static void func_actor_335800_80163124(Task* task)
{
    Actor335800MainWork* work;
    GfxCoord*            coord;
    VECTOR               vec;

    coord = task->extra.tmd->coords;
    work  = (Actor335800MainWork*)task->work;

    vec = D_actor_335800_80161E4C;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

s32 func_actor_335800_8016343C(Task* task, s32 arg1, s32 mode)
{
    Actor335800MainWork* work;
    TmdObject*           obj;
    TmdObject*           objA;
    TmdObject*           objB;
    s32                  ret;

    work = (Actor335800MainWork*)task->work;
    obj  = task->extra.tmd;
    objA = work->child0->extra.tmd;
    objB = work->child1->extra.tmd;
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
    objA->flags = obj->flags;
    objB->flags = obj->flags;
    return ret;
}

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor335800MainWork* work;

    work = (Actor335800MainWork*)arg0->work;
    if (request->command == 0) {
        work->field_504 = 0;
    }
    return 0;
}

/// Per-frame tick of the child block: runs the motion handler `walk.motion`
/// selects, adds the 16.16 velocity `walk.step` onto the accumulator `walk.acc`,
/// moves the coordinate by the integer part and keeps only the fraction, then
/// ticks the animation slots, draws the ground shadow and rebuilds the colour
/// matrix while visible, and counts `freeCountdown` down to the buffer free.
static void func_actor_335800_80163568(Task* task)
{
    TmdObject*       ext      = task->extra.tmd;
    Actor335800Work* work     = (Actor335800Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_335800_80163B70, func_actor_335800_80163B78 };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    funcs[work->walk.motion](task);
    coord                = task->extra.tmd->coords;
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
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive19.inc.c"

/// Placement handler for the child block, the twin of
/// `actorMotionStartWalk`: stores the spawn position and rotation, then
/// applies a start preset exactly as `actorMotionPlayAnim19` does
/// (inlined here).
s32 func_actor_335800_80163880(Task* task, s32 arg1, ActorTransform* place, Actor335800SpawnAnim* anim)
{
    Actor335800Work*      work;
    Actor335800Work*      w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                   = (Actor335800Work*)task->work;
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
        preset.animationId  = 0xD;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (Actor335800Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        func_800B3F84(&work->rig.anim, D_actor_335800_80172E98[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
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
    return 0;
}

/// State dispatcher of the child block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80163A34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E5C;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the two -1 bytes,
/// the -1 halfword and three cleared words the work's own init expects,
/// republishes the light and colour matrices onto the display object, then
/// installs the message table and the exit handler. An allocation failure
/// ends the task instead of leaving a half-built actor behind.
static void func_actor_335800_80163AA0(Task* arg0)
{
    Actor335800Work* work;

    work = memCalloc(sizeof(Actor335800Work), false);
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

    func_actor_335800_80163B54(arg0);

    arg0->msgTable     = D_actor_335800_80172EA8;
    arg0->exitCallback = func_actor_335800_80163B34;
    arg0->state       += 1;
}

static void func_actor_335800_80163B34(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

static void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*       ext;
    Actor335800Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor335800Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

static void func_actor_335800_80163B70(Task* arg0)
{
}

/// Motion handler 1 of the child block: copies the four-handler table onto
/// the stack and runs the entry `walk.motionStep` selects.
static void func_actor_335800_80163B78(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor335800Work* work;

    work = (Actor335800Work*)arg0->work;
    sp   = D_actor_335800_80161E68;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// State handler 0 of the child block's table `D_actor_335800_80161E68`:
/// turns the root part to face `work->walk.target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances
/// `walk.motionStep` to the next handler.
static void func_actor_335800_80163BE0(Task* task)
{
    Actor335800Work* work;
    GfxCoord*        coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = (Actor335800Work*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}

/// State handler 1 of the child block's table `D_actor_335800_80161E68`, the
/// step after the turn-to-face handler `func_actor_335800_80163BE0`: rotates
/// the constant forward offset `D_actor_335800_80161E78` through the root
/// part's matrix into `work->walk.step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it for the update loop, and advances `walk.motionStep` so
/// the dispatcher `func_actor_335800_80163B78` runs the next handler.
static void func_actor_335800_80163CA0(Task* task)
{
    Actor335800Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = task->extra.tmd->coords;
    work  = (Actor335800Work*)task->work;

    vec = D_actor_335800_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// State handler 3 of `D_actor_335800_80161E68`, the child block's
/// turn-to-yaw step: Euler-extracts the root coordinate into `vec`, and while
/// the yaw gap to `work->walk.rotY` is at least 0x41 it steps `vec.vy` toward
/// it by 0x40, taking the step on an `s32` widening of the extracted yaw;
/// otherwise it snaps the yaw to the target and plays anim 0x7D3 with a
/// preset carrying the `model.nextAnimId` byte, clearing the motion index and step.
/// Either way the root coordinate is rebuilt as the identity matrix rotated by
/// `vec`.
static void func_actor_335800_80163D20(Task* arg0)
{
    Actor335800Work*     work;
    GpMtxWords*          words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor335800Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                      = work->walk.rotY;
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
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

/// Message 0x7D3 handler of the child block, the 19-slot counterpart of
/// `actorMotionPlayAnim`: re-seeds the slot array off bank table
/// `D_actor_335800_80172E98` when the preset's bank index changes, then
/// restarts or resets every slot and ticks them.
s32 actorMotionPlayAnim19(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor335800Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor335800Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        func_800B3F84(&work->rig.anim, D_actor_335800_80172E98[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
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
    return 0;
}

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceEuler func_actor_335800_80163F3C
#include "../../shared/actor_messages_place_euler.inc.c"
#undef actorMsgPlaceEuler

s32 func_actor_335800_80163FB8(Task* task, s32 arg1, s32 mode)
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
            ((Actor335800Work*)task->work)->freeCountdown = mode;
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

s32 func_actor_335800_80164098(void)
{
    return 0;
}
