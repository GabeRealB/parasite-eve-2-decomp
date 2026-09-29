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

/// Work block of the parent task, allocated zeroed by its spawn routine and
/// kept at `Task::work`: a twenty-part rig, the walk state, and
/// `freeCountdown`, the frames until the model buffers are freed, -1
/// disabling the countdown.
typedef struct Actor120400MainWork {
    ActorAnimRig20  rig;
    ActorModelState model;
    ActorWalkState  walk;
    byte            pad_4FC[0x4];
    s16             freeCountdown;
    byte            pad_502[0x2];
} Actor120400MainWork;
STATIC_ASSERT_SIZEOF(Actor120400MainWork, 0x504);

/// Animation source indexed by the bank id the presets latch:
/// `D_actor_120400_8013E744[work->model.bank]` is the bank handed to
/// `func_800B3F84`.
extern GpAnimSet*  D_actor_120400_8013E6D0[29];
extern GpAnimSet** D_actor_120400_8013E744[1];

/// Optional start animation for `func_actor_120400_80132398`: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are 0x10 and 1.
typedef GpSpawnAnimArg Actor120400SpawnAnim;
STATIC_ASSERT_SIZEOF(Actor120400SpawnAnim, 0x8);

/// The task table the parent is spawned from and its two children are spawned
/// from (entries 1 and 2), and the message table the parent points its
/// `Task::msgTable` at; both live in this overlay's trailing data.
extern TaskDesc D_actor_120400_8013E748[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, GpXformArg*, Actor120400SpawnAnim*);
        s32 (*call4)(Task*, s32, s32, s32);
    } handler;
} Actor120400MsgEntry;
STATIC_ASSERT_SIZEOF(Actor120400MsgEntry, 8);

extern Actor120400MsgEntry D_actor_120400_8013E76C[];

static void func_actor_120400_80131E5C(Task* arg0);
static void func_actor_120400_80132050(Task* arg0);
static void func_actor_120400_80132254(Task* arg0);
static void func_actor_120400_801325A4(Task* task);
static void func_actor_120400_801326B0(Task* task);
static void func_actor_120400_801327B4(Task* task);
static void func_actor_120400_801327D4(Task* task);
static void func_actor_120400_801327F0(Task* arg0);
static void func_actor_120400_801327F8(Task* task);
static void func_actor_120400_80132860(Task* task);
static void func_actor_120400_80132920(Task* task);
static void func_actor_120400_801329A0(Task* arg0);
s32         func_actor_120400_80132AA0(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

/// Spawn, tick and teardown handlers of the two child tasks, dispatched by
/// `func_actor_120400_8013254C`.
static const TaskFuncTable3 D_actor_120400_80131E24 = { {
    func_actor_120400_801325A4,
    func_actor_120400_801326B0,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent task, dispatched by
/// `func_actor_120400_80132748`.
static const TaskFuncTable3 D_actor_120400_80131E30 = { {
    func_actor_120400_80131E5C,
    func_actor_120400_80132050,
    func_actor_120400_801327B4,
} };

/// Steps of the parent's walk sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_120400_80131E3C = { {
    func_actor_120400_80132860,
    func_actor_120400_80132920,
    func_actor_120400_80132254,
    func_actor_120400_801329A0,
} };

/// The constant local-space offset the walk rotates into its velocity:
/// straight ahead along the root part's own +Z.
static const VECTOR D_actor_120400_80131E4C = { 0, 0, 0x200000, 0 };

extern TmdSource D_actor_120400_8013783C;
extern TmdSource D_actor_120400_80137C90;
extern TmdSource D_actor_120400_801380E4;
s32              func_actor_120400_80132398(Task*, s32, GpXformArg*, Actor120400SpawnAnim*);
s32              func_actor_120400_80132AA0(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_120400_80132BBC(Task*, s32, GpXformArg*);
s32              func_actor_120400_80132C38(Task*, s32, s32, s32);
s32              func_actor_120400_80132D14(void);
void             func_actor_120400_8013254C(Task*);
void             func_actor_120400_80132748(Task*);

TmdBone D_actor_120400_80132D1C[20] = {
#include "assets/actor_120400_model_05A1C_skeleton.inc"
};

u32 D_actor_120400_80132FEC[20] = {
#include "assets/actor_120400_model_05A1C_partVerts.inc"
};

SVECTOR D_actor_120400_8013303C[300] = {
#include "assets/actor_120400_model_05A1C_verts.inc"
};

SVECTOR D_actor_120400_8013399C[298] = {
#include "assets/actor_120400_model_05A1C_normals.inc"
};

u32 D_actor_120400_801342EC[3412] = {
#include "assets/actor_120400_model_05A1C_stream.inc"
};

TmdSource D_actor_120400_8013783C = {
    0,
    18224,
    5696,
    20,
    D_actor_120400_80132FEC,
    D_actor_120400_8013303C,
    D_actor_120400_8013399C,
    D_actor_120400_80132D1C,
    D_actor_120400_801342EC,
};

TmdBone D_actor_120400_80137860[1] = {
#include "assets/actor_120400_model_05E70_skeleton.inc"
};

u32 D_actor_120400_80137884[1] = {
#include "assets/actor_120400_model_05E70_partVerts.inc"
};

SVECTOR D_actor_120400_80137888[23] = {
#include "assets/actor_120400_model_05E70_verts.inc"
};

SVECTOR D_actor_120400_80137940[23] = {
#include "assets/actor_120400_model_05E70_normals.inc"
};

u32 D_actor_120400_801379F8[166] = {
#include "assets/actor_120400_model_05E70_stream.inc"
};

TmdSource D_actor_120400_80137C90 = {
    0,
    1148,
    0,
    1,
    D_actor_120400_80137884,
    D_actor_120400_80137888,
    D_actor_120400_80137940,
    D_actor_120400_80137860,
    D_actor_120400_801379F8,
};

TmdBone D_actor_120400_80137CB4[1] = {
#include "assets/actor_120400_model_062C4_skeleton.inc"
};

u32 D_actor_120400_80137CD8[1] = {
#include "assets/actor_120400_model_062C4_partVerts.inc"
};

SVECTOR D_actor_120400_80137CDC[23] = {
#include "assets/actor_120400_model_062C4_verts.inc"
};

SVECTOR D_actor_120400_80137D94[23] = {
#include "assets/actor_120400_model_062C4_normals.inc"
};

u32 D_actor_120400_80137E4C[166] = {
#include "assets/actor_120400_model_062C4_stream.inc"
};

TmdSource D_actor_120400_801380E4 = {
    0,
    1148,
    0,
    1,
    D_actor_120400_80137CD8,
    D_actor_120400_80137CDC,
    D_actor_120400_80137D94,
    D_actor_120400_80137CB4,
    D_actor_120400_80137E4C,
};

AnimationPackedPose D_actor_120400_80138108[2] = {
#include "assets/actor_120400_animation_0653C_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80138120[32] = {
#include "assets/actor_120400_animation_0653C_bank4.inc"
};

AnimationRecord D_actor_120400_801381A0[101] = {
#include "assets/actor_120400_animation_0653C_records.inc"
};

u16 D_actor_120400_80138334[20] = {
#include "assets/actor_120400_animation_0653C_indices.inc"
};

GpAnimSet D_actor_120400_8013835C = {
    D_actor_120400_801381A0,
    D_actor_120400_80138334,
    { NULL, D_actor_120400_80138108, NULL, NULL, D_actor_120400_80138120, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_80138384[21] = {
#include "assets/actor_120400_animation_06CD0_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80138480[156] = {
#include "assets/actor_120400_animation_06CD0_bank4.inc"
};

AnimationRecord D_actor_120400_801386F0[246] = {
#include "assets/actor_120400_animation_06CD0_records.inc"
};

u16 D_actor_120400_80138AC8[20] = {
#include "assets/actor_120400_animation_06CD0_indices.inc"
};

GpAnimSet D_actor_120400_80138AF0 = {
    D_actor_120400_801386F0,
    D_actor_120400_80138AC8,
    { NULL, D_actor_120400_80138384, NULL, NULL, D_actor_120400_80138480, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_80138B18[2] = {
#include "assets/actor_120400_animation_072B4_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80138B30[152] = {
#include "assets/actor_120400_animation_072B4_bank4.inc"
};

AnimationRecord D_actor_120400_80138D90[199] = {
#include "assets/actor_120400_animation_072B4_records.inc"
};

u16 D_actor_120400_801390AC[20] = {
#include "assets/actor_120400_animation_072B4_indices.inc"
};

GpAnimSet D_actor_120400_801390D4 = {
    D_actor_120400_80138D90,
    D_actor_120400_801390AC,
    { NULL, D_actor_120400_80138B18, NULL, NULL, D_actor_120400_80138B30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_801390FC[2] = {
#include "assets/actor_120400_animation_07828_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80139114[135] = {
#include "assets/actor_120400_animation_07828_bank4.inc"
};

AnimationRecord D_actor_120400_80139330[188] = {
#include "assets/actor_120400_animation_07828_records.inc"
};

u16 D_actor_120400_80139620[20] = {
#include "assets/actor_120400_animation_07828_indices.inc"
};

GpAnimSet D_actor_120400_80139648 = {
    D_actor_120400_80139330,
    D_actor_120400_80139620,
    { NULL, D_actor_120400_801390FC, NULL, NULL, D_actor_120400_80139114, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_80139670[3] = {
#include "assets/actor_120400_animation_07A00_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80139694[29] = {
#include "assets/actor_120400_animation_07A00_bank4.inc"
};

AnimationRecord D_actor_120400_80139708[60] = {
#include "assets/actor_120400_animation_07A00_records.inc"
};

u16 D_actor_120400_801397F8[20] = {
#include "assets/actor_120400_animation_07A00_indices.inc"
};

GpAnimSet D_actor_120400_80139820 = {
    D_actor_120400_80139708,
    D_actor_120400_801397F8,
    { NULL, D_actor_120400_80139670, NULL, NULL, D_actor_120400_80139694, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_80139848[3] = {
#include "assets/actor_120400_animation_07E1C_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013986C[80] = {
#include "assets/actor_120400_animation_07E1C_bank4.inc"
};

AnimationRecord D_actor_120400_801399AC[154] = {
#include "assets/actor_120400_animation_07E1C_records.inc"
};

u16 D_actor_120400_80139C14[20] = {
#include "assets/actor_120400_animation_07E1C_indices.inc"
};

GpAnimSet D_actor_120400_80139C3C = {
    D_actor_120400_801399AC,
    D_actor_120400_80139C14,
    { NULL, D_actor_120400_80139848, NULL, NULL, D_actor_120400_8013986C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_80139C64[2] = {
#include "assets/actor_120400_animation_07FD8_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80139C7C[25] = {
#include "assets/actor_120400_animation_07FD8_bank4.inc"
};

AnimationRecord D_actor_120400_80139CE0[60] = {
#include "assets/actor_120400_animation_07FD8_records.inc"
};

u16 D_actor_120400_80139DD0[20] = {
#include "assets/actor_120400_animation_07FD8_indices.inc"
};

GpAnimSet D_actor_120400_80139DF8 = {
    D_actor_120400_80139CE0,
    D_actor_120400_80139DD0,
    { NULL, D_actor_120400_80139C64, NULL, NULL, D_actor_120400_80139C7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_80139E20[3] = {
#include "assets/actor_120400_animation_081C0_bank1.inc"
};

AnimationPackedRotation D_actor_120400_80139E44[33] = {
#include "assets/actor_120400_animation_081C0_bank4.inc"
};

AnimationRecord D_actor_120400_80139EC8[60] = {
#include "assets/actor_120400_animation_081C0_records.inc"
};

u16 D_actor_120400_80139FB8[20] = {
#include "assets/actor_120400_animation_081C0_indices.inc"
};

GpAnimSet D_actor_120400_80139FE0 = {
    D_actor_120400_80139EC8,
    D_actor_120400_80139FB8,
    { NULL, D_actor_120400_80139E20, NULL, NULL, D_actor_120400_80139E44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013A008[3] = {
#include "assets/actor_120400_animation_0847C_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013A02C[39] = {
#include "assets/actor_120400_animation_0847C_bank4.inc"
};

AnimationRecord D_actor_120400_8013A0C8[107] = {
#include "assets/actor_120400_animation_0847C_records.inc"
};

u16 D_actor_120400_8013A274[20] = {
#include "assets/actor_120400_animation_0847C_indices.inc"
};

GpAnimSet D_actor_120400_8013A29C = {
    D_actor_120400_8013A0C8,
    D_actor_120400_8013A274,
    { NULL, D_actor_120400_8013A008, NULL, NULL, D_actor_120400_8013A02C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013A2C4[3] = {
#include "assets/actor_120400_animation_0865C_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013A2E8[31] = {
#include "assets/actor_120400_animation_0865C_bank4.inc"
};

AnimationRecord D_actor_120400_8013A364[60] = {
#include "assets/actor_120400_animation_0865C_records.inc"
};

u16 D_actor_120400_8013A454[20] = {
#include "assets/actor_120400_animation_0865C_indices.inc"
};

GpAnimSet D_actor_120400_8013A47C = {
    D_actor_120400_8013A364,
    D_actor_120400_8013A454,
    { NULL, D_actor_120400_8013A2C4, NULL, NULL, D_actor_120400_8013A2E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013A4A4[4] = {
#include "assets/actor_120400_animation_08904_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013A4D4[52] = {
#include "assets/actor_120400_animation_08904_bank4.inc"
};

AnimationRecord D_actor_120400_8013A5A4[86] = {
#include "assets/actor_120400_animation_08904_records.inc"
};

u16 D_actor_120400_8013A6FC[20] = {
#include "assets/actor_120400_animation_08904_indices.inc"
};

GpAnimSet D_actor_120400_8013A724 = {
    D_actor_120400_8013A5A4,
    D_actor_120400_8013A6FC,
    { NULL, D_actor_120400_8013A4A4, NULL, NULL, D_actor_120400_8013A4D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013A74C[4] = {
#include "assets/actor_120400_animation_08BE0_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013A77C[59] = {
#include "assets/actor_120400_animation_08BE0_bank4.inc"
};

AnimationRecord D_actor_120400_8013A868[92] = {
#include "assets/actor_120400_animation_08BE0_records.inc"
};

u16 D_actor_120400_8013A9D8[20] = {
#include "assets/actor_120400_animation_08BE0_indices.inc"
};

GpAnimSet D_actor_120400_8013AA00 = {
    D_actor_120400_8013A868,
    D_actor_120400_8013A9D8,
    { NULL, D_actor_120400_8013A74C, NULL, NULL, D_actor_120400_8013A77C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013AA28[4] = {
#include "assets/actor_120400_animation_08EB4_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013AA58[58] = {
#include "assets/actor_120400_animation_08EB4_bank4.inc"
};

AnimationRecord D_actor_120400_8013AB40[91] = {
#include "assets/actor_120400_animation_08EB4_records.inc"
};

u16 D_actor_120400_8013ACAC[20] = {
#include "assets/actor_120400_animation_08EB4_indices.inc"
};

GpAnimSet D_actor_120400_8013ACD4 = {
    D_actor_120400_8013AB40,
    D_actor_120400_8013ACAC,
    { NULL, D_actor_120400_8013AA28, NULL, NULL, D_actor_120400_8013AA58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013ACFC[4] = {
#include "assets/actor_120400_animation_091CC_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013AD2C[66] = {
#include "assets/actor_120400_animation_091CC_bank4.inc"
};

AnimationRecord D_actor_120400_8013AE34[100] = {
#include "assets/actor_120400_animation_091CC_records.inc"
};

u16 D_actor_120400_8013AFC4[20] = {
#include "assets/actor_120400_animation_091CC_indices.inc"
};

GpAnimSet D_actor_120400_8013AFEC = {
    D_actor_120400_8013AE34,
    D_actor_120400_8013AFC4,
    { NULL, D_actor_120400_8013ACFC, NULL, NULL, D_actor_120400_8013AD2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013B014[4] = {
#include "assets/actor_120400_animation_097E4_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013B044[147] = {
#include "assets/actor_120400_animation_097E4_bank4.inc"
};

AnimationRecord D_actor_120400_8013B290[211] = {
#include "assets/actor_120400_animation_097E4_records.inc"
};

u16 D_actor_120400_8013B5DC[20] = {
#include "assets/actor_120400_animation_097E4_indices.inc"
};

GpAnimSet D_actor_120400_8013B604 = {
    D_actor_120400_8013B290,
    D_actor_120400_8013B5DC,
    { NULL, D_actor_120400_8013B014, NULL, NULL, D_actor_120400_8013B044, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013B62C[9] = {
#include "assets/actor_120400_animation_0A190_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013B698[245] = {
#include "assets/actor_120400_animation_0A190_bank4.inc"
};

AnimationRecord D_actor_120400_8013BA6C[327] = {
#include "assets/actor_120400_animation_0A190_records.inc"
};

u16 D_actor_120400_8013BF88[20] = {
#include "assets/actor_120400_animation_0A190_indices.inc"
};

GpAnimSet D_actor_120400_8013BFB0 = {
    D_actor_120400_8013BA6C,
    D_actor_120400_8013BF88,
    { NULL, D_actor_120400_8013B62C, NULL, NULL, D_actor_120400_8013B698, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013BFD8[2] = {
#include "assets/actor_120400_animation_0A3B0_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013BFF0[25] = {
#include "assets/actor_120400_animation_0A3B0_bank4.inc"
};

AnimationRecord D_actor_120400_8013C054[85] = {
#include "assets/actor_120400_animation_0A3B0_records.inc"
};

u16 D_actor_120400_8013C1A8[20] = {
#include "assets/actor_120400_animation_0A3B0_indices.inc"
};

GpAnimSet D_actor_120400_8013C1D0 = {
    D_actor_120400_8013C054,
    D_actor_120400_8013C1A8,
    { NULL, D_actor_120400_8013BFD8, NULL, NULL, D_actor_120400_8013BFF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013C1F8[6] = {
#include "assets/actor_120400_animation_0A720_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013C240[73] = {
#include "assets/actor_120400_animation_0A720_bank4.inc"
};

AnimationRecord D_actor_120400_8013C364[109] = {
#include "assets/actor_120400_animation_0A720_records.inc"
};

u16 D_actor_120400_8013C518[20] = {
#include "assets/actor_120400_animation_0A720_indices.inc"
};

GpAnimSet D_actor_120400_8013C540 = {
    D_actor_120400_8013C364,
    D_actor_120400_8013C518,
    { NULL, D_actor_120400_8013C1F8, NULL, NULL, D_actor_120400_8013C240, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013C568[2] = {
#include "assets/actor_120400_animation_0A960_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013C580[25] = {
#include "assets/actor_120400_animation_0A960_bank4.inc"
};

AnimationRecord D_actor_120400_8013C5E4[93] = {
#include "assets/actor_120400_animation_0A960_records.inc"
};

u16 D_actor_120400_8013C758[20] = {
#include "assets/actor_120400_animation_0A960_indices.inc"
};

GpAnimSet D_actor_120400_8013C780 = {
    D_actor_120400_8013C5E4,
    D_actor_120400_8013C758,
    { NULL, D_actor_120400_8013C568, NULL, NULL, D_actor_120400_8013C580, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013C7A8[5] = {
#include "assets/actor_120400_animation_0AC50_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013C7E4[52] = {
#include "assets/actor_120400_animation_0AC50_bank4.inc"
};

AnimationRecord D_actor_120400_8013C8B4[101] = {
#include "assets/actor_120400_animation_0AC50_records.inc"
};

u16 D_actor_120400_8013CA48[20] = {
#include "assets/actor_120400_animation_0AC50_indices.inc"
};

GpAnimSet D_actor_120400_8013CA70 = {
    D_actor_120400_8013C8B4,
    D_actor_120400_8013CA48,
    { NULL, D_actor_120400_8013C7A8, NULL, NULL, D_actor_120400_8013C7E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013CA98[8] = {
#include "assets/actor_120400_animation_0B110_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013CAF8[93] = {
#include "assets/actor_120400_animation_0B110_bank4.inc"
};

AnimationRecord D_actor_120400_8013CC6C[167] = {
#include "assets/actor_120400_animation_0B110_records.inc"
};

u16 D_actor_120400_8013CF08[20] = {
#include "assets/actor_120400_animation_0B110_indices.inc"
};

GpAnimSet D_actor_120400_8013CF30 = {
    D_actor_120400_8013CC6C,
    D_actor_120400_8013CF08,
    { NULL, D_actor_120400_8013CA98, NULL, NULL, D_actor_120400_8013CAF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013CF58[2] = {
#include "assets/actor_120400_animation_0B544_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013CF70[98] = {
#include "assets/actor_120400_animation_0B544_bank4.inc"
};

AnimationRecord D_actor_120400_8013D0F8[145] = {
#include "assets/actor_120400_animation_0B544_records.inc"
};

u16 D_actor_120400_8013D33C[20] = {
#include "assets/actor_120400_animation_0B544_indices.inc"
};

GpAnimSet D_actor_120400_8013D364 = {
    D_actor_120400_8013D0F8,
    D_actor_120400_8013D33C,
    { NULL, D_actor_120400_8013CF58, NULL, NULL, D_actor_120400_8013CF70, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013D38C[4] = {
#include "assets/actor_120400_animation_0B9E0_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013D3BC[105] = {
#include "assets/actor_120400_animation_0B9E0_bank4.inc"
};

AnimationRecord D_actor_120400_8013D560[158] = {
#include "assets/actor_120400_animation_0B9E0_records.inc"
};

u16 D_actor_120400_8013D7D8[20] = {
#include "assets/actor_120400_animation_0B9E0_indices.inc"
};

GpAnimSet D_actor_120400_8013D800 = {
    D_actor_120400_8013D560,
    D_actor_120400_8013D7D8,
    { NULL, D_actor_120400_8013D38C, NULL, NULL, D_actor_120400_8013D3BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013D828[3] = {
#include "assets/actor_120400_animation_0BC30_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013D84C[43] = {
#include "assets/actor_120400_animation_0BC30_bank4.inc"
};

AnimationRecord D_actor_120400_8013D8F8[76] = {
#include "assets/actor_120400_animation_0BC30_records.inc"
};

u16 D_actor_120400_8013DA28[20] = {
#include "assets/actor_120400_animation_0BC30_indices.inc"
};

GpAnimSet D_actor_120400_8013DA50 = {
    D_actor_120400_8013D8F8,
    D_actor_120400_8013DA28,
    { NULL, D_actor_120400_8013D828, NULL, NULL, D_actor_120400_8013D84C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013DA78[3] = {
#include "assets/actor_120400_animation_0BE80_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013DA9C[43] = {
#include "assets/actor_120400_animation_0BE80_bank4.inc"
};

AnimationRecord D_actor_120400_8013DB48[76] = {
#include "assets/actor_120400_animation_0BE80_records.inc"
};

u16 D_actor_120400_8013DC78[20] = {
#include "assets/actor_120400_animation_0BE80_indices.inc"
};

GpAnimSet D_actor_120400_8013DCA0 = {
    D_actor_120400_8013DB48,
    D_actor_120400_8013DC78,
    { NULL, D_actor_120400_8013DA78, NULL, NULL, D_actor_120400_8013DA9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013DCC8[3] = {
#include "assets/actor_120400_animation_0C278_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013DCEC[86] = {
#include "assets/actor_120400_animation_0C278_bank4.inc"
};

AnimationRecord D_actor_120400_8013DE44[139] = {
#include "assets/actor_120400_animation_0C278_records.inc"
};

u16 D_actor_120400_8013E070[20] = {
#include "assets/actor_120400_animation_0C278_indices.inc"
};

GpAnimSet D_actor_120400_8013E098 = {
    D_actor_120400_8013DE44,
    D_actor_120400_8013E070,
    { NULL, D_actor_120400_8013DCC8, NULL, NULL, D_actor_120400_8013DCEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013E0C0[3] = {
#include "assets/actor_120400_animation_0C674_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013E0E4[89] = {
#include "assets/actor_120400_animation_0C674_bank4.inc"
};

AnimationRecord D_actor_120400_8013E248[137] = {
#include "assets/actor_120400_animation_0C674_records.inc"
};

u16 D_actor_120400_8013E46C[20] = {
#include "assets/actor_120400_animation_0C674_indices.inc"
};

GpAnimSet D_actor_120400_8013E494 = {
    D_actor_120400_8013E248,
    D_actor_120400_8013E46C,
    { NULL, D_actor_120400_8013E0C0, NULL, NULL, D_actor_120400_8013E0E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_120400_8013E4BC[2] = {
#include "assets/actor_120400_animation_0C888_bank1.inc"
};

AnimationPackedRotation D_actor_120400_8013E4D4[22] = {
#include "assets/actor_120400_animation_0C888_bank4.inc"
};

AnimationRecord D_actor_120400_8013E52C[85] = {
#include "assets/actor_120400_animation_0C888_records.inc"
};

u16 D_actor_120400_8013E680[20] = {
#include "assets/actor_120400_animation_0C888_indices.inc"
};

GpAnimSet D_actor_120400_8013E6A8 = {
    D_actor_120400_8013E52C,
    D_actor_120400_8013E680,
    { NULL, D_actor_120400_8013E4BC, NULL, NULL, D_actor_120400_8013E4D4, NULL, NULL, NULL },
};

GpAnimSet* D_actor_120400_8013E6D0[29] = {
    NULL,
    &D_actor_120400_8013835C,
    &D_actor_120400_8013A724,
    &D_actor_120400_8013AA00,
    &D_actor_120400_8013ACD4,
    &D_actor_120400_8013AFEC,
    &D_actor_120400_8013B604,
    &D_actor_120400_8013BFB0,
    &D_actor_120400_8013C1D0,
    &D_actor_120400_80139820,
    &D_actor_120400_80139C3C,
    &D_actor_120400_80139DF8,
    &D_actor_120400_80139FE0,
    &D_actor_120400_8013A29C,
    &D_actor_120400_8013A47C,
    &D_actor_120400_80139648,
    &D_actor_120400_80138AF0,
    &D_actor_120400_801390D4,
    &D_actor_120400_8013C540,
    &D_actor_120400_8013C780,
    &D_actor_120400_8013CA70,
    &D_actor_120400_8013CF30,
    &D_actor_120400_8013D364,
    &D_actor_120400_8013D800,
    &D_actor_120400_8013DA50,
    &D_actor_120400_8013DCA0,
    &D_actor_120400_8013E098,
    &D_actor_120400_8013E494,
    &D_actor_120400_8013E6A8,
};

GpAnimSet** D_actor_120400_8013E744[1] = {
    D_actor_120400_8013E6D0,
};

TaskDesc D_actor_120400_8013E748[3] = {
    { 257, 192, func_actor_120400_80132748, { .model = &D_actor_120400_8013783C } },
    { 1, 192, func_actor_120400_8013254C, { .model = &D_actor_120400_801380E4 } },
    { 1, 192, func_actor_120400_8013254C, { .model = &D_actor_120400_80137C90 } },
};

Actor120400MsgEntry D_actor_120400_8013E76C[6] = {
    { 2003, { .call1 = func_actor_120400_80132AA0 } },
    { 2004, { .call2 = func_actor_120400_80132BBC } },
    { 2005, { .call4 = func_actor_120400_80132C38 } },
    { 2013, { .call3 = func_actor_120400_80132398 } },
    { 2011, { .call0 = func_actor_120400_80132D14 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
}; /// The parent's spawn handler. Allocates the 0x504 `Actor120400MainWork` block, seeds it, and spawns the
/// two children `D_actor_120400_8013E748` holds -- table entries 1 and 2. Each
/// has `TmdObject::tpage` / `clut` loaded with the texture page and CLUT
/// row of the `AreaPlacement` that entry selects, reached through the area key
/// `&gGameSession->at4.loc.view` and indexed by the model id the child's own
/// `spawnArg2` carries at `GpEnemy::placeKey >> 12`, and each then has its
/// texture stream processed twice when it has a buffer. The body ends by
/// pointing the parent's model at its light/colour matrices
/// (`func_actor_120400_801327D4`), pointing `msgTable` at the message table and
/// installing `func_actor_120400_801327B4` as its exit callback.
static void func_actor_120400_80131E5C(Task* arg0)
{
    Actor120400MainWork* work;
    GameLocationKey      key;
    GameLocationKey*     sessionKey;
    GameLocationKey*     keyAddr;
    Task*                spawned;

    work = (Actor120400MainWork*)memCalloc(0x504, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work          = (TaskIdMap*)work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;
    spawned             = Task_SpawnFromTable(D_actor_120400_8013E748, 1, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        model      = spawned->extra.tmd;
        idx        = ((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12;
        sessionKey = &gGameSession->at4.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
        areaSyncLocationVariant(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = gpAreaPlaceAt(rec->field_0, idx);
        model->tpage = place->texturePageOffset;
        model->clut  = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(D_actor_120400_8013E748, 2, 0xC, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        model = spawned->extra.tmd;
        idx   = ((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12;
        // Keep this block's key address separate across the spawn calls.
        sessionKey = (keyAddr = &gGameSession->at4.loc);
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = keyAddr->room;
        key.view   = gGameSession->at4.loc.view;
        areaSyncLocationVariant(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = gpAreaPlaceAt(rec->field_0, idx);
        model->tpage = place->texturePageOffset;
        model->clut  = place->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_120400_801327D4(arg0);
    arg0->msgTable     = D_actor_120400_8013E76C;
    arg0->exitCallback = func_actor_120400_801327B4;
    arg0->state       += 1;
}

/// The parent's per-frame update: the motion handler -- entry `walk.motion` of
/// the pair `{func_actor_120400_801327F0, func_actor_120400_801327F8}`, idle or
/// the walk sequence -- runs first, then
/// the three 16.16 step accumulators at 0x4D8..0x4E0 take this frame's `step`,
/// their integer halves are added onto the root coordinate's translation and
/// the fraction is dropped, and `composeStamp` is cleared so the tree rebuilds. With
/// `model.ticking` set every animation slot is ticked. Unless the model is hidden
/// (bit 0x80 of `TmdObject::flags`), the second coordinate's work matrix
/// feeds `func_800EA1A8` and a non-zero result draws the ground-effect quad;
/// when `gGameSession->viewReady` is set the same coordinate is flagged stale,
/// updated and re-ranked through `func_800D7A9C`. The body ends decrementing
/// the `freeCountdown` teardown timer, freeing the model's buffers on the frame it
/// reaches zero.
static void func_actor_120400_80132050(Task* arg0)
{
    TmdObject*           ext      = arg0->extra.tmd;
    Actor120400MainWork* work     = (Actor120400MainWork*)arg0->work;
    TaskFunc             funcs[2] = { func_actor_120400_801327F0, func_actor_120400_801327F8 };
    VECTOR3              pos;
    GfxCoord*            coord;
    s32                  i;

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
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
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

/// Walk step 2, the arrival check: takes the X/Z distance from the root
/// coordinate to `target`. While it keeps shrinking below `limit` it is stored
/// as the new `limit`; once it no longer does, the target has been reached or
/// passed, so the step plays the preset carrying the `model.nextAnimId` byte through
/// the 0x7D3 handler, stops the velocity `step` and advances `walk.motionStep`.
static void func_actor_120400_80132254(Task* arg0)
{
    Actor120400MainWork* work;
    GfxCoord*            coord;
    SVECTOR              d;
    s32                  dx;
    s32                  dz;
    AnimationPlayRequest preset;

    work  = (Actor120400MainWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_120400_80132AA0(arg0, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Message 0x7DD handler of the parent: starts the walk sequence toward a
/// placement. The position and rotation are copied into `target` and
/// `walk.rotX`..`walk.rotZ`, `walk.motion` selects the walk and `walk.motionStep` restarts
/// it, and a start preset is built on the stack -- bank id 0, the optional start
/// animation's id and companion byte (0x10 and 1 when absent), 1, 5 and 1 --
/// and then applied in-line. A changed bank id latches `model.bank` and reseeds
/// the animation through `func_800B3F84` with the bank this overlay's
/// `D_actor_120400_8013E744` selects; `model.animId` takes the preset's animation
/// id, and a preset asking for slots while `model.ticking` says the slots are
/// already ticking is pushed onto `func_800B4114`'s per-slot loop instead of
/// the `Gp_AnimResetSlot` one, followed by a `Gp_AnimTickIndex` pass over the
/// same 0x14 slots and `model.ticking` raised. Returns 0 either way.
s32 func_actor_120400_80132398(Task* task, s32 arg1, GpXformArg* place, Actor120400SpawnAnim* anim)
{
    Actor120400MainWork*  work;
    Actor120400MainWork*  w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                   = (Actor120400MainWork*)task->work;
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
        preset.animationId  = 0x10;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (Actor120400MainWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        func_800B3F84(&work->rig.anim, D_actor_120400_8013E744[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// State dispatcher of the two child tasks: copies their spawn/tick/teardown
/// table onto the stack and runs the entry `Task::state` selects.
void func_actor_120400_8013254C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E24;
    sp.funcs[task->state](task);
}

/// Spawn state of a child task: its model starts hidden and then takes the
/// parent model's bits 0x80 and 0x4 as the tick does, allocating its buffers
/// while bit 0x4 is clear. The model is linked at ordering-table offset -2, its
/// root coordinate hangs off the parent's coordinate `spawnArg1`, it shares
/// the parent's light and colour matrices, and the task is reparented under
/// the parent before the state advances.
static void func_actor_120400_801325A4(Task* task)
{
    Task*      parent;
    TmdObject* obj;
    TmdObject* parentObj;
    GfxCoord*  coords;
    GfxCoord*  root;

    parent      = task->spawnArg2.pointer;
    obj         = task->extra.tmd;
    parentObj   = parent->extra.tmd;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset      = -2;
    coords            += task->spawnArg1.value;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    root->parent       = coords;
    obj->lightMtx      = parentObj->lightMtx;
    obj->colorMtx      = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

/// Per-frame tick of a child task: copies the parent model's hidden bit (0x80)
/// and bit 0x4 onto the child's own model. While the parent's bit 0x4 is
/// clear the child's display buffers are (re)allocated as well.
static void func_actor_120400_801326B0(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = ((Task*)task->spawnArg2.pointer)->extra.tmd;
    object       = task->extra.tmd;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}

/// State dispatcher of the parent task: copies its spawn/tick/teardown table
/// onto the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_120400_80132748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E30;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback of the parent task, installed by its spawn handler: runs the
/// common enemy teardown.
static void func_actor_120400_801327B4(Task* task)
{
    Gp_EnemyTaskExit(task);
}

/// Points the parent's model at the light and colour matrices held in its own
/// work block.
static void func_actor_120400_801327D4(Task* task)
{
    TmdObject*           ext;
    Actor120400MainWork* work;

    ext           = task->extra.tmd;
    work          = (Actor120400MainWork*)task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Motion handler 0 of the parent, idle: does nothing.
static void func_actor_120400_801327F0(Task* arg0)
{
}

/// Motion handler 1 of the parent, the walk sequence: copies the step table
/// onto the stack and runs the entry `walk.motionStep` selects.
static void func_actor_120400_801327F8(Task* task)
{
    Actor120400MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor120400MainWork*)task->work;
    fns  = D_actor_120400_80131E3C;
    fns.funcs[work->walk.motionStep](task);
}

/// Walk step 0: turns the root part to face `target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
static void func_actor_120400_80132860(Task* task)
{
    Actor120400MainWork* work;
    GfxCoord*            coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor120400MainWork*)task->work;
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

/// Walk step 1: rotates the constant forward offset `D_actor_120400_80131E4C`
/// through the root part's matrix into `step`, opens the arrival threshold to
/// 0x7FFF, which disables it, and advances the step.
static void func_actor_120400_80132920(Task* task)
{
    Actor120400MainWork* work;
    GfxCoord*            coord;
    VECTOR               vec;

    coord = task->extra.tmd->coords;
    work  = (Actor120400MainWork*)task->work;

    vec = D_actor_120400_80131E4C;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// Walk step 3, the turn to the placement yaw: Euler-extracts the root
/// coordinate into `vec`, and while the yaw gap to `walk.rotY` is at least
/// 0x41 steps `vec.vy` toward it by 0x40, taking the step on an `s32` widening
/// of the extracted yaw. Otherwise it snaps the yaw to the target, plays the
/// preset carrying the `model.nextAnimId` byte through the 0x7D3 handler and clears
/// `walk.motion` / `walk.motionStep`, which returns the parent to idle. Either way the
/// root coordinate is rebuilt as the identity matrix rotated by `vec`.
static void func_actor_120400_801329A0(Task* arg0)
{
    Actor120400MainWork* work;
    GpMtxWords*          words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor120400MainWork*)arg0->work;

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
        func_actor_120400_80132AA0(arg0, 0x7D3, &preset, 0);
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

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_120400_80132AA0(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor120400MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor120400MainWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        func_800B3F84(&work->rig.anim, D_actor_120400_8013E744[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// Message 0x7D4 handler of the parent: places the root part at the message's
/// position and Euler angles, rebuilding the rotation from them and clearing
/// `composeStamp` so the world matrix is recomputed. Returns 0.
s32 func_actor_120400_80132BBC(Task* task, s32 arg1, GpXformArg* args)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message 0x7D5 handler of the parent: shows or hides its model. `mode`
/// drives the `TmdObject` parked in `Task::extra` -- bit 0x80 hides it, bit
/// 0x4 is the one the children copy alongside it:
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, start the `freeCountdown` countdown to freeing the buffers, raise 0x4
///   mode 3  show, raise 0x4
///
/// Returns 0 for the four known modes and 1 for any other.
s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*           obj;
    Actor120400MainWork* work;
    s32                  ret;

    obj  = task->extra.tmd;
    work = (Actor120400MainWork*)task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags         |= 0x80;
            work->freeCountdown = mode;
            obj->flags         |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message 0x7DB handler of the parent: ignores the message and returns 0.
s32 func_actor_120400_80132D14(void)
{
    return 0;
}
