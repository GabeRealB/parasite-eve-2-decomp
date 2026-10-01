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
#include "../../shared/model_placement.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

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
/// `gActorMotionAnimBanks[work->model.bank]` is the bank handed to
/// `func_800B3F84`.
extern AnimationSet*  D_actor_120400_8013E6D0[29];
extern AnimationSet** gActorMotionAnimBanks[1];

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
        s32                (*call0)(void);
        s32                (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32                (*call2)(Task*, s32, ActorTransform*);
        s32                (*call3)(Task*, s32, ActorTransform*, Actor120400SpawnAnim*);
        TaskMessageHandler call4;
    } handler;
} Actor120400MsgEntry;
STATIC_ASSERT_SIZEOF(Actor120400MsgEntry, 8);

extern Actor120400MsgEntry D_actor_120400_8013E76C[];

static void func_actor_120400_80131E5C(Task* arg0);
static void func_actor_120400_80132050(Task* arg0);
static void func_actor_120400_801327B4(Task* task);
static void func_actor_120400_801327D4(Task* task);
static void func_actor_120400_801327F0(Task* arg0);
static void func_actor_120400_801327F8(Task* task);
static void func_actor_120400_80132920(Task* task);

/// Spawn, tick and teardown handlers of the two child tasks, dispatched by
/// `func_actor_120400_8013254C`.
static const TaskFuncTable3 D_actor_120400_80131E24 = { {
    modelPlacementAttachChild,
    modelPlacementMirrorParent,
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
    actorMotionFaceTarget,
    func_actor_120400_80132920,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The constant local-space offset the walk rotates into its velocity:
/// straight ahead along the root part's own +Z.
static const VECTOR D_actor_120400_80131E4C = { 0, 0, 0x200000, 0 };

extern TmdSource D_actor_120400_8013783C;
extern TmdSource D_actor_120400_80137C90;
extern TmdSource D_actor_120400_801380E4;
s32              func_actor_120400_80132398(Task*, s32, ActorTransform* place, Actor120400SpawnAnim*);
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

AnimationSet D_actor_120400_8013835C = {
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

AnimationSet D_actor_120400_80138AF0 = {
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

AnimationSet D_actor_120400_801390D4 = {
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

AnimationSet D_actor_120400_80139648 = {
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

AnimationSet D_actor_120400_80139820 = {
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

AnimationSet D_actor_120400_80139C3C = {
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

AnimationSet D_actor_120400_80139DF8 = {
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

AnimationSet D_actor_120400_80139FE0 = {
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

AnimationSet D_actor_120400_8013A29C = {
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

AnimationSet D_actor_120400_8013A47C = {
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

AnimationSet D_actor_120400_8013A724 = {
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

AnimationSet D_actor_120400_8013AA00 = {
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

AnimationSet D_actor_120400_8013ACD4 = {
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

AnimationSet D_actor_120400_8013AFEC = {
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

AnimationSet D_actor_120400_8013B604 = {
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

AnimationSet D_actor_120400_8013BFB0 = {
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

AnimationSet D_actor_120400_8013C1D0 = {
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

AnimationSet D_actor_120400_8013C540 = {
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

AnimationSet D_actor_120400_8013C780 = {
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

AnimationSet D_actor_120400_8013CA70 = {
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

AnimationSet D_actor_120400_8013CF30 = {
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

AnimationSet D_actor_120400_8013D364 = {
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

AnimationSet D_actor_120400_8013D800 = {
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

AnimationSet D_actor_120400_8013DA50 = {
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

AnimationSet D_actor_120400_8013DCA0 = {
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

AnimationSet D_actor_120400_8013E098 = {
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

AnimationSet D_actor_120400_8013E494 = {
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

AnimationSet D_actor_120400_8013E6A8 = {
    D_actor_120400_8013E52C,
    D_actor_120400_8013E680,
    { NULL, D_actor_120400_8013E4BC, NULL, NULL, D_actor_120400_8013E4D4, NULL, NULL, NULL },
};

AnimationSet* D_actor_120400_8013E6D0[29] = {
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

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_120400_8013E6D0,
};

TaskDesc D_actor_120400_8013E748[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120400_80132748, { .model = &D_actor_120400_8013783C } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_120400_8013254C, { .model = &D_actor_120400_801380E4 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_120400_8013254C, { .model = &D_actor_120400_80137C90 } },
};

Actor120400MsgEntry D_actor_120400_8013E76C[6] = {
    { 2003, { .call1 = actorMotionPlayAnim } },
    { 2004, { .call2 = actorMsgPlaceEuler } },
    { 2005, { .call4 = func_actor_120400_80132C38 } },
    { 2013, { .call3 = func_actor_120400_80132398 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_120400_80132D14 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
}; /// The parent's spawn handler. Allocates the 0x504 `Actor120400MainWork` block, seeds it, and spawns the
/// two children `D_actor_120400_8013E748` holds -- table entries 1 and 2. Each
/// has `TmdObject::texturePageOffset` / `clutRowOffset` loaded with the texture page and CLUT
/// row of the `AreaPlacement` that entry selects, reached through the area key
/// `&gGameSession->location.loc` and indexed by the model id the child's own
/// `spawnArg2` carries at `Enemy::placeKey >> ENEMY_PLACE_INDEX_SHIFT`, and each then has its
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

    work = memCalloc(0x504, false);
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
    spawned                = Task_SpawnFromTable(D_actor_120400_8013E748, 1, 8, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        model      = spawned->extra.tmd;
        idx        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey = &gGameSession->location.loc;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        key.view   = sessionKey->view;
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
    spawned = Task_SpawnFromTable(D_actor_120400_8013E748, 2, 0xC, arg0);
    if (spawned != NULL) {
        TmdObject*     model;
        GpAreaVariant* rec;
        AreaPlacement* place;
        s32            idx;

        model = spawned->extra.tmd;
        idx   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
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
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

#include "../../shared/actor_motion_arrive.inc.c"

/// Message 0x7DD handler of the parent: starts the walk sequence toward a
/// placement. The position and rotation are copied into `target` and
/// `walk.rotX`..`walk.rotZ`, `walk.motion` selects the walk and `walk.motionStep` restarts
/// it, and a start preset is built on the stack -- bank id 0, the optional start
/// animation's id and companion byte (0x10 and 1 when absent), 1, 5 and 1 --
/// and then applied in-line. A changed bank id latches `model.bank` and reseeds
/// the animation through `func_800B3F84` with the bank this overlay's
/// `gActorMotionAnimBanks` selects; `model.animId` takes the preset's animation
/// id, and a preset asking for slots while `model.ticking` says the slots are
/// already ticking is pushed onto `func_800B4114`'s per-slot loop instead of
/// the `Gp_AnimResetSlot` one, followed by a `Gp_AnimTickIndex` pass over the
/// same 0x14 slots and `model.ticking` raised. Returns 0 either way.
s32 func_actor_120400_80132398(Task* task, s32 arg1, ActorTransform* place, Actor120400SpawnAnim* anim)
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
        func_800B3F84(&work->rig.anim, gActorMotionAnimBanks[work->model.bank], ext, work->rig.poses,
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

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// State dispatcher of the parent task: copies its spawn/tick/teardown table
/// onto the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_120400_80132748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
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

#include "../../shared/actor_motion_face.inc.c"

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

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

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
    return ret;
}

/// Message 0x7DB handler of the parent: ignores the message and returns 0.
s32 func_actor_120400_80132D14(void)
{
    return 0;
}
