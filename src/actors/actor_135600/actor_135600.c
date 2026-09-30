#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
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
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor_motion.h"

/// Optional start animation for `actorMotionStartWalk`: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are anim 0xD and 1.
typedef GpSpawnAnimArg Actor135600SpawnAnim;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// The marker quad's two vertex pairs, in the actor's local frame: `-4/+4`
/// and `-3/+3` along X, all coplanar in Z.
extern SVECTOR D_actor_135600_8013B060[4];

/// Animation bank table the 0x7D3 handler seeds the slot array from, indexed
/// by the preset's bank index.
extern AnimationSet*  D_actor_135600_8013B080[16];
extern AnimationSet** gActorMotionAnimBanks[1];

/// Child task table the setup handler spawns from: entry 0 is the actor
/// itself, entries 1 and 2 the two part models and entry 3 the marker task.
extern TaskDesc D_actor_135600_8013B0C4[];

/// The actor's message table, stored in `Task::msgTable`: 0x7D3, 0x7D4, 0x7D5,
/// 0x7DD and 0x7DB against the handlers below.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call2)(Task*, s32, ActorTransform*, Actor135600SpawnAnim*);
        s32 (*call3)(Task*, s32, ActorTransform*, s32);
        s32 (*call4)(Task*, s32, s32, s32);
    } handler;
} Actor135600MsgEntry;
STATIC_ASSERT_SIZEOF(Actor135600MsgEntry, 8);

extern Actor135600MsgEntry D_actor_135600_8013B0F4[];

static void func_actor_135600_80132234(Task* task);
static void func_actor_135600_801324D0(Task* task);
static void func_actor_135600_80132A38(Task* task);
static void func_actor_135600_80132AB4(Task* task);
static void func_actor_135600_80132B14(Task* task);
static void func_actor_135600_80132C18(Task* task);
static void func_actor_135600_80132C80(GfxCoord* coord, MATRIX* mtx, SVECTOR* vec);
static void func_actor_135600_80132DBC(Task* task);
static void func_actor_135600_80132DDC(Task* task);
static void func_actor_135600_80132DF8(Task* task);
static void func_actor_135600_80132E00(Task* task);
static void func_actor_135600_80132F28(Task* task);
s32         func_actor_135600_801331C4(Task* task, s32 msgId, ActorTransform* args, s32 arg3);
s32         func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3);

/// States of the two part tasks (`D_actor_135600_8013B0C4` entries 1 and 2),
/// dispatched by `func_actor_135600_801329E0`: attach to the parent, idle,
/// kill.
static const TaskFuncTable3 D_actor_135600_80131E24 = { {
    func_actor_135600_80132A38,
    func_actor_135600_80132AB4,
    taskKill,
} };

/// States of the marker task (entry 3), dispatched by
/// `func_actor_135600_80132ABC`: attach to the parent with an offset, draw the
/// marker, kill.
static const TaskFuncTable3 D_actor_135600_80131E30 = { {
    func_actor_135600_80132B14,
    func_actor_135600_80132C18,
    taskKill,
} };

/// States of the actor itself (entry 0), dispatched by
/// `func_actor_135600_80132D64`: setup, per-frame tick, exit.
static const TaskFuncTable3 D_actor_135600_80131E3C = { {
    func_actor_135600_80132234,
    func_actor_135600_801324D0,
    func_actor_135600_80132DBC,
} };

/// Step handlers of the motion sequence, indexed by
/// `Actor135600Work::walk.motionStep`: turn to face `target`, start walking forward,
/// walk until arrival, then turn to the placement yaw.
static const TaskFuncTable4 D_actor_135600_80131E48 = { {
    actorMotionFaceTarget,
    func_actor_135600_80132F28,
    actorMotionArrive,
    actorMotionTurnToYaw,
} };

/// The constant local-space offset `func_actor_135600_80132F28` rotates:
/// straight ahead along the part's own +Z.
static const VECTOR D_actor_135600_80131E58 = { 0, 0, 0x200000, 0 };

extern AnimationSet D_actor_135600_80138D60;
extern AnimationSet D_actor_135600_80139014;
extern AnimationSet D_actor_135600_801393A8;
extern AnimationSet D_actor_135600_801395CC;
extern AnimationSet D_actor_135600_80139928;
extern AnimationSet D_actor_135600_80139C04;
extern AnimationSet D_actor_135600_80139DD0;
extern AnimationSet D_actor_135600_80139F9C;
extern AnimationSet D_actor_135600_8013A184;
extern AnimationSet D_actor_135600_8013A464;
extern AnimationSet D_actor_135600_8013A804;
extern AnimationSet D_actor_135600_8013AA20;
extern AnimationSet D_actor_135600_8013AC14;
extern AnimationSet D_actor_135600_8013AE18;
extern AnimationSet D_actor_135600_8013B038;
extern TmdSource    D_actor_135600_80137E94;
extern TmdSource    D_actor_135600_801382E8;
extern TmdSource    D_actor_135600_801387D8;
extern TmdSource    D_actor_135600_80138AE8;
s32                 func_actor_135600_801331C4(Task*, s32, ActorTransform* args, s32);
s32                 func_actor_135600_80133240(Task*, s32, s32, s32);
s32                 func_actor_135600_8013336C(void);
void                func_actor_135600_801329E0(Task*);
void                func_actor_135600_80132ABC(Task*);
void                func_actor_135600_80132D64(Task*);

TmdBone D_actor_135600_80133374[20] = {
#include "assets/actor_135600_model_06074_skeleton.inc"
};

u32 D_actor_135600_80133644[20] = {
#include "assets/actor_135600_model_06074_partVerts.inc"
};

SVECTOR D_actor_135600_80133694[300] = {
#include "assets/actor_135600_model_06074_verts.inc"
};

SVECTOR D_actor_135600_80133FF4[298] = {
#include "assets/actor_135600_model_06074_normals.inc"
};

u32 D_actor_135600_80134944[3412] = {
#include "assets/actor_135600_model_06074_stream.inc"
};

TmdSource D_actor_135600_80137E94 = {
    0,
    18224,
    5696,
    20,
    D_actor_135600_80133644,
    D_actor_135600_80133694,
    D_actor_135600_80133FF4,
    D_actor_135600_80133374,
    D_actor_135600_80134944,
};

TmdBone D_actor_135600_80137EB8[1] = {
#include "assets/actor_135600_model_064C8_skeleton.inc"
};

u32 D_actor_135600_80137EDC[1] = {
#include "assets/actor_135600_model_064C8_partVerts.inc"
};

SVECTOR D_actor_135600_80137EE0[23] = {
#include "assets/actor_135600_model_064C8_verts.inc"
};

SVECTOR D_actor_135600_80137F98[23] = {
#include "assets/actor_135600_model_064C8_normals.inc"
};

u32 D_actor_135600_80138050[166] = {
#include "assets/actor_135600_model_064C8_stream.inc"
};

TmdSource D_actor_135600_801382E8 = {
    0,
    1148,
    0,
    1,
    D_actor_135600_80137EDC,
    D_actor_135600_80137EE0,
    D_actor_135600_80137F98,
    D_actor_135600_80137EB8,
    D_actor_135600_80138050,
};

TmdBone D_actor_135600_8013830C[1] = {
#include "assets/actor_135600_model_069B8_skeleton.inc"
};

u32 D_actor_135600_80138330[1] = {
#include "assets/actor_135600_model_069B8_partVerts.inc"
};

SVECTOR D_actor_135600_80138334[27] = {
#include "assets/actor_135600_model_069B8_verts.inc"
};

SVECTOR D_actor_135600_8013840C[27] = {
#include "assets/actor_135600_model_069B8_normals.inc"
};

u32 D_actor_135600_801384E4[189] = {
#include "assets/actor_135600_model_069B8_stream.inc"
};

TmdSource D_actor_135600_801387D8 = {
    0,
    1328,
    0,
    1,
    D_actor_135600_80138330,
    D_actor_135600_80138334,
    D_actor_135600_8013840C,
    D_actor_135600_8013830C,
    D_actor_135600_801384E4,
};

TmdBone D_actor_135600_801387FC[1] = {
#include "assets/actor_135600_model_06CC8_skeleton.inc"
};

u32 D_actor_135600_80138820[1] = {
#include "assets/actor_135600_model_06CC8_partVerts.inc"
};

SVECTOR D_actor_135600_80138824[24] = {
#include "assets/actor_135600_model_06CC8_verts.inc"
};

u32 D_actor_135600_801388E4[129] = {
#include "assets/actor_135600_model_06CC8_stream.inc"
};

TmdSource D_actor_135600_80138AE8 = {
    0,
    928,
    0,
    1,
    D_actor_135600_80138820,
    D_actor_135600_80138824,
    &D_actor_135600_80138824[24],
    D_actor_135600_801387FC,
    D_actor_135600_801388E4,
};

AnimationPackedPose D_actor_135600_80138B0C[2] = {
#include "assets/actor_135600_animation_06F40_bank1.inc"
};

AnimationPackedRotation D_actor_135600_80138B24[32] = {
#include "assets/actor_135600_animation_06F40_bank4.inc"
};

AnimationRecord D_actor_135600_80138BA4[101] = {
#include "assets/actor_135600_animation_06F40_records.inc"
};

u16 D_actor_135600_80138D38[20] = {
#include "assets/actor_135600_animation_06F40_indices.inc"
};

AnimationSet D_actor_135600_80138D60 = {
    D_actor_135600_80138BA4,
    D_actor_135600_80138D38,
    { NULL, D_actor_135600_80138B0C, NULL, NULL, D_actor_135600_80138B24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_80138D88[2] = {
#include "assets/actor_135600_animation_071F4_bank1.inc"
};

AnimationPackedRotation D_actor_135600_80138DA0[39] = {
#include "assets/actor_135600_animation_071F4_bank4.inc"
};

AnimationRecord D_actor_135600_80138E3C[108] = {
#include "assets/actor_135600_animation_071F4_records.inc"
};

u16 D_actor_135600_80138FEC[20] = {
#include "assets/actor_135600_animation_071F4_indices.inc"
};

AnimationSet D_actor_135600_80139014 = {
    D_actor_135600_80138E3C,
    D_actor_135600_80138FEC,
    { NULL, D_actor_135600_80138D88, NULL, NULL, D_actor_135600_80138DA0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013903C[6] = {
#include "assets/actor_135600_animation_07588_bank1.inc"
};

AnimationPackedRotation D_actor_135600_80139084[77] = {
#include "assets/actor_135600_animation_07588_bank4.inc"
};

AnimationRecord D_actor_135600_801391B8[114] = {
#include "assets/actor_135600_animation_07588_records.inc"
};

u16 D_actor_135600_80139380[20] = {
#include "assets/actor_135600_animation_07588_indices.inc"
};

AnimationSet D_actor_135600_801393A8 = {
    D_actor_135600_801391B8,
    D_actor_135600_80139380,
    { NULL, D_actor_135600_8013903C, NULL, NULL, D_actor_135600_80139084, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_801393D0[2] = {
#include "assets/actor_135600_animation_077AC_bank1.inc"
};

AnimationPackedRotation D_actor_135600_801393E8[33] = {
#include "assets/actor_135600_animation_077AC_bank4.inc"
};

AnimationRecord D_actor_135600_8013946C[78] = {
#include "assets/actor_135600_animation_077AC_records.inc"
};

u16 D_actor_135600_801395A4[20] = {
#include "assets/actor_135600_animation_077AC_indices.inc"
};

AnimationSet D_actor_135600_801395CC = {
    D_actor_135600_8013946C,
    D_actor_135600_801395A4,
    { NULL, D_actor_135600_801393D0, NULL, NULL, D_actor_135600_801393E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_801395F4[2] = {
#include "assets/actor_135600_animation_07B08_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013960C[74] = {
#include "assets/actor_135600_animation_07B08_bank4.inc"
};

AnimationRecord D_actor_135600_80139734[115] = {
#include "assets/actor_135600_animation_07B08_records.inc"
};

u16 D_actor_135600_80139900[20] = {
#include "assets/actor_135600_animation_07B08_indices.inc"
};

AnimationSet D_actor_135600_80139928 = {
    D_actor_135600_80139734,
    D_actor_135600_80139900,
    { NULL, D_actor_135600_801395F4, NULL, NULL, D_actor_135600_8013960C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_80139950[5] = {
#include "assets/actor_135600_animation_07DE4_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013998C[58] = {
#include "assets/actor_135600_animation_07DE4_bank4.inc"
};

AnimationRecord D_actor_135600_80139A74[90] = {
#include "assets/actor_135600_animation_07DE4_records.inc"
};

u16 D_actor_135600_80139BDC[20] = {
#include "assets/actor_135600_animation_07DE4_indices.inc"
};

AnimationSet D_actor_135600_80139C04 = {
    D_actor_135600_80139A74,
    D_actor_135600_80139BDC,
    { NULL, D_actor_135600_80139950, NULL, NULL, D_actor_135600_8013998C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_80139C2C[2] = {
#include "assets/actor_135600_animation_07FB0_bank1.inc"
};

AnimationPackedRotation D_actor_135600_80139C44[29] = {
#include "assets/actor_135600_animation_07FB0_bank4.inc"
};

AnimationRecord D_actor_135600_80139CB8[60] = {
#include "assets/actor_135600_animation_07FB0_records.inc"
};

u16 D_actor_135600_80139DA8[20] = {
#include "assets/actor_135600_animation_07FB0_indices.inc"
};

AnimationSet D_actor_135600_80139DD0 = {
    D_actor_135600_80139CB8,
    D_actor_135600_80139DA8,
    { NULL, D_actor_135600_80139C2C, NULL, NULL, D_actor_135600_80139C44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_80139DF8[2] = {
#include "assets/actor_135600_animation_0817C_bank1.inc"
};

AnimationPackedRotation D_actor_135600_80139E10[29] = {
#include "assets/actor_135600_animation_0817C_bank4.inc"
};

AnimationRecord D_actor_135600_80139E84[60] = {
#include "assets/actor_135600_animation_0817C_records.inc"
};

u16 D_actor_135600_80139F74[20] = {
#include "assets/actor_135600_animation_0817C_indices.inc"
};

AnimationSet D_actor_135600_80139F9C = {
    D_actor_135600_80139E84,
    D_actor_135600_80139F74,
    { NULL, D_actor_135600_80139DF8, NULL, NULL, D_actor_135600_80139E10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_80139FC4[3] = {
#include "assets/actor_135600_animation_08364_bank1.inc"
};

AnimationPackedRotation D_actor_135600_80139FE8[33] = {
#include "assets/actor_135600_animation_08364_bank4.inc"
};

AnimationRecord D_actor_135600_8013A06C[60] = {
#include "assets/actor_135600_animation_08364_records.inc"
};

u16 D_actor_135600_8013A15C[20] = {
#include "assets/actor_135600_animation_08364_indices.inc"
};

AnimationSet D_actor_135600_8013A184 = {
    D_actor_135600_8013A06C,
    D_actor_135600_8013A15C,
    { NULL, D_actor_135600_80139FC4, NULL, NULL, D_actor_135600_80139FE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013A1AC[2] = {
#include "assets/actor_135600_animation_08644_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013A1C4[54] = {
#include "assets/actor_135600_animation_08644_bank4.inc"
};

AnimationRecord D_actor_135600_8013A29C[104] = {
#include "assets/actor_135600_animation_08644_records.inc"
};

u16 D_actor_135600_8013A43C[20] = {
#include "assets/actor_135600_animation_08644_indices.inc"
};

AnimationSet D_actor_135600_8013A464 = {
    D_actor_135600_8013A29C,
    D_actor_135600_8013A43C,
    { NULL, D_actor_135600_8013A1AC, NULL, NULL, D_actor_135600_8013A1C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013A48C[2] = {
#include "assets/actor_135600_animation_089E4_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013A4A4[71] = {
#include "assets/actor_135600_animation_089E4_bank4.inc"
};

AnimationRecord D_actor_135600_8013A5C0[135] = {
#include "assets/actor_135600_animation_089E4_records.inc"
};

u16 D_actor_135600_8013A7DC[20] = {
#include "assets/actor_135600_animation_089E4_indices.inc"
};

AnimationSet D_actor_135600_8013A804 = {
    D_actor_135600_8013A5C0,
    D_actor_135600_8013A7DC,
    { NULL, D_actor_135600_8013A48C, NULL, NULL, D_actor_135600_8013A4A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013A82C[2] = {
#include "assets/actor_135600_animation_08C00_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013A844[23] = {
#include "assets/actor_135600_animation_08C00_bank4.inc"
};

AnimationRecord D_actor_135600_8013A8A0[86] = {
#include "assets/actor_135600_animation_08C00_records.inc"
};

u16 D_actor_135600_8013A9F8[20] = {
#include "assets/actor_135600_animation_08C00_indices.inc"
};

AnimationSet D_actor_135600_8013AA20 = {
    D_actor_135600_8013A8A0,
    D_actor_135600_8013A9F8,
    { NULL, D_actor_135600_8013A82C, NULL, NULL, D_actor_135600_8013A844, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013AA48[2] = {
#include "assets/actor_135600_animation_08DF4_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013AA60[18] = {
#include "assets/actor_135600_animation_08DF4_bank4.inc"
};

AnimationRecord D_actor_135600_8013AAA8[81] = {
#include "assets/actor_135600_animation_08DF4_records.inc"
};

u16 D_actor_135600_8013ABEC[20] = {
#include "assets/actor_135600_animation_08DF4_indices.inc"
};

AnimationSet D_actor_135600_8013AC14 = {
    D_actor_135600_8013AAA8,
    D_actor_135600_8013ABEC,
    { NULL, D_actor_135600_8013AA48, NULL, NULL, D_actor_135600_8013AA60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013AC3C[2] = {
#include "assets/actor_135600_animation_08FF8_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013AC54[20] = {
#include "assets/actor_135600_animation_08FF8_bank4.inc"
};

AnimationRecord D_actor_135600_8013ACA4[83] = {
#include "assets/actor_135600_animation_08FF8_records.inc"
};

u16 D_actor_135600_8013ADF0[20] = {
#include "assets/actor_135600_animation_08FF8_indices.inc"
};

AnimationSet D_actor_135600_8013AE18 = {
    D_actor_135600_8013ACA4,
    D_actor_135600_8013ADF0,
    { NULL, D_actor_135600_8013AC3C, NULL, NULL, D_actor_135600_8013AC54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_135600_8013AE40[2] = {
#include "assets/actor_135600_animation_09218_bank1.inc"
};

AnimationPackedRotation D_actor_135600_8013AE58[30] = {
#include "assets/actor_135600_animation_09218_bank4.inc"
};

AnimationRecord D_actor_135600_8013AED0[80] = {
#include "assets/actor_135600_animation_09218_records.inc"
};

u16 D_actor_135600_8013B010[20] = {
#include "assets/actor_135600_animation_09218_indices.inc"
};

AnimationSet D_actor_135600_8013B038 = {
    D_actor_135600_8013AED0,
    D_actor_135600_8013B010,
    { NULL, D_actor_135600_8013AE40, NULL, NULL, D_actor_135600_8013AE58, NULL, NULL, NULL },
};

SVECTOR D_actor_135600_8013B060[4] = {
    { -4, 0, 0, 0 },
    { 4, 0, 0, 0 },
    { -3, 0, 0, 0 },
    { 3, 0, 0, 0 },
};

AnimationSet* D_actor_135600_8013B080[16] = {
    NULL,
    &D_actor_135600_80138D60,
    &D_actor_135600_80139014,
    &D_actor_135600_801393A8,
    &D_actor_135600_801395CC,
    &D_actor_135600_80139928,
    &D_actor_135600_80139C04,
    &D_actor_135600_80139DD0,
    &D_actor_135600_80139F9C,
    &D_actor_135600_8013A184,
    &D_actor_135600_8013A464,
    &D_actor_135600_8013A804,
    &D_actor_135600_8013AA20,
    &D_actor_135600_8013AC14,
    &D_actor_135600_8013AE18,
    &D_actor_135600_8013B038,
};

AnimationSet** gActorMotionAnimBanks[1] = {
    D_actor_135600_8013B080,
};

TaskDesc D_actor_135600_8013B0C4[4] = {
    { (TASK_BODY_TMD | 0x100), 192, func_actor_135600_80132D64, { .model = &D_actor_135600_80137E94 } },
    { TASK_BODY_TMD, 192, func_actor_135600_801329E0, { .model = &D_actor_135600_801387D8 } },
    { TASK_BODY_TMD, 192, func_actor_135600_801329E0, { .model = &D_actor_135600_801382E8 } },
    { TASK_BODY_TMD, 192, func_actor_135600_80132ABC, { .model = &D_actor_135600_80138AE8 } },
};

Actor135600MsgEntry D_actor_135600_8013B0F4[6] = {
    { 2003, { .call1 = actorMotionPlayAnim } },
    { 2004, { .call3 = func_actor_135600_801331C4 } },
    { 2005, { .call4 = func_actor_135600_80133240 } },
    { 2013, { .call2 = actorMotionStartWalk } },
    { 2011, { .call0 = func_actor_135600_8013336C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

static s32 func_actor_135600_80131E68(GfxCoord* coord, s32 arg1);

/// Recomputes `coord`'s world matrix (`Gp_UpdateCoord`), composes its parent
/// chain, then projects two offsets along the part's local Z - the near one 10 units
/// out and the far one `arg1 * 0x46 / 0x1000 + 10`, so the pair opens by 70
/// 4096ths of a unit per tick - and returns the signed `ratan2` of the
/// difference between the two projections, the actor's screen-space angle.
/// The pair is drawn as the quad `D_actor_135600_8013B060` describes: the wide
/// vertex pair rotated about the screen origin by that angle and anchored on
/// the near projection, the narrow pair unrotated on the far one, as a
/// semi-transparent `POLY_F4` followed by its texture page, both linked into
/// the ordering table at the far point's depth. Nothing is drawn when that
/// depth is behind the camera. The marker's draw state passes the countdown it
/// runs on as `arg1`.
static s32 func_actor_135600_80131E68(GfxCoord* coord, s32 arg1)
{
    SVECTOR    v0;
    SVECTOR    v1;
    SVECTOR    pos;
    SVECTOR    quad[4];
    OverlayMat m;
    MATRIX*    mtx;
    long       sxy0;
    long       p;
    long       flag;
    long       sxy1;
    s16        y0;
    s16        y1;
    s32        rot;
    u16        x0;
    u16        x1;
    s32        depth;
    POLY_F4*   poly;
    DR_TPAGE*  tpage;
    s32        i;

    Gp_UpdateCoord(coord);
    mtx = &m.mat;
    func_actor_135600_80132C80(coord, &m.mat, &pos);

    v0.vx = 0;
    v0.vy = 0;
    v0.vz = 0xA;
    ApplyMatrixSV(&m.mat, &v0, &v0);

    v1.vx = 0;
    v1.vy = 0;
    v1.vz = arg1 * 0x46 / 0x1000 + 0xA;
    ApplyMatrixSV(&m.mat, &v1, &v1);

    v0.vx += pos.vx;
    v0.vy += pos.vy;
    v0.vz += pos.vz;
    v1.vx += pos.vx;
    v1.vy += pos.vy;
    v1.vz += pos.vz;

    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);

    RotTransPers(&v0, &sxy0, &p, &flag);
    depth = RotTransPers(&v1, &sxy1, &p, &flag);

    x0  = (u16)sxy0;
    x1  = (u16)sxy1;
    y0  = sxy0 >> 16;
    y1  = sxy1 >> 16;
    rot = ratan2((s16)sxy1 - (s16)sxy0, y0 - y1);

    /* Only the middle diagonal and the last entry go through `mtx`: a store
     * written that way keeps its address in the register `RotMatrixZ` is
     * handed, where the ones naming `m` directly fold to a frame-relative
     * address, and the target has both. */
    m.ident.m00_m01           = 0x1000;
    MATRIX_PAIR(&m.mat, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1)    = 0x1000;
    MATRIX_PAIR(&m.mat, 2, 0) = 0;
    mtx->m[2][2]              = 0x1000;
    RotMatrixZ(rot, &m.mat);

    for (i = 0; i < 2; i++) {
        ApplyMatrixSV(&m.mat, &D_actor_135600_8013B060[i], &quad[i]);
        quad[i].vx    += x0;
        quad[i].vy    += y0;
        quad[i + 2].vx = D_actor_135600_8013B060[i + 2].vx + x1;
        quad[i + 2].vy = D_actor_135600_8013B060[i + 2].vy + y1;
    }

    if (p >= 0) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        setRGB0(poly, 0xFF, 0x40, 0);
        poly->x0 = quad[0].vx;
        poly->y0 = quad[0].vy;
        poly->x1 = quad[1].vx;
        poly->y1 = quad[1].vy;
        poly->x2 = quad[2].vx;
        poly->y2 = quad[2].vy;
        poly->x3 = quad[3].vx;
        poly->y3 = quad[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 2, poly);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) - 2, tpage);
    }
    return rot;
}

/// Setup state of the actor (entry 0 of `D_actor_135600_80131E3C`). It
/// allocates the 0x50C-byte work block, seeds the -1 sentinels and spawns
/// entries 1 to 3 of `D_actor_135600_8013B0C4` -- the two part models get the
/// texture page and CLUT of the area record the actor's placement key resolves
/// to, and all three are parked in the work block. It then points the model at
/// the work block's light/colour pair, places it at (0xA6E, 0, 0x5F0) yawed
/// 0x400, applies animation 2, shows it (message 0x7D5 mode 1), publishes the
/// message table `D_actor_135600_8013B0F4`, installs the exit callback and
/// steps to the tick state.
static void func_actor_135600_80132234(Task* task)
{
    Actor135600Work*     work;
    Task*                spawned;
    ActorTransform       args;
    AnimationPlayRequest preset;

    work = (Actor135600Work*)memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;

    spawned = Task_SpawnFromTable(D_actor_135600_8013B0C4, 1, 8, task);
    if (spawned != NULL) {
        work->child1 = spawned;
        actorTintModel(spawned->extra.tmd, (GpEnemy*)task->spawnArg2.pointer);
    }

    spawned = Task_SpawnFromTable(D_actor_135600_8013B0C4, 2, 0xC, task);
    if (spawned != NULL) {
        work->child0 = spawned;
        actorTintModel(spawned->extra.tmd, (GpEnemy*)task->spawnArg2.pointer);
    }

    spawned = Task_SpawnFromTable(D_actor_135600_8013B0C4, 3, 8, task);
    if (spawned != NULL) {
        work->child2 = spawned;
    }

    func_actor_135600_80132DDC(task);

    args.pos.vx = 0xA6E;
    args.pos.vz = 0x5F0;
    args.pos.vy = 0;
    args.rot.vx = 0;
    args.rot.vy = 0x400;
    args.rot.vz = 0;
    func_actor_135600_801331C4(task, 0x7D4, &args, 0);

    preset.source.index = 0;
    preset.animationId  = 2;
    preset.blend        = ANIMATION_BLEND_RESET;
    actorMotionPlayAnim(task, 0x7D3, &preset, 0);

    func_actor_135600_80133240(task, 0x7D5, 1, 0);

    task->msgTable     = D_actor_135600_8013B0F4;
    task->exitCallback = func_actor_135600_80132DBC;
    task->state       += 1;
}

/// Per-frame tick of the actor (entry 1 of `D_actor_135600_80131E3C`).
/// Draws the ground shadow under the second part unless the model is hidden,
/// then -- only while `Gp_StateF0.field_4` is clear -- runs the handler `walk.motion`
/// selects, advances the root coordinate by the high halves of the 16.16
/// accumulators fed from `step` (re-zeroing each high half), ticks slots 1 to
/// 19 while `model.ticking` is set, rebuilds the second part's coordinate and the
/// actor colour while `gGameSession->viewReady` is set, and counts `freeCountdown`
/// down while non-negative, freeing the model's buffers when it reaches zero.
static void func_actor_135600_801324D0(Task* arg0)
{
    TmdObject*       ext      = arg0->extra.tmd;
    Actor135600Work* work     = (Actor135600Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_135600_80132DF8, func_actor_135600_80132E00 };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShadowShade);
        }
    }
    if (Gp_StateF0.field_4 == 0) {
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
}

#include "../../shared/actor_motion_arrive.inc.c"

#include "../../shared/actor_motion_start.inc.c"

/// Dispatcher of the two part tasks: runs their state from
/// `D_actor_135600_80131E24`.
void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

/// Setup state of a part task (entry 0 of `D_actor_135600_80131E24`): chains
/// the task's root coordinate under the parent's part coordinate the spawn
/// arguments name, inherits the parent's light and colour matrices, reparents the task so it
/// is updated with the parent, and advances to the next state.
static void func_actor_135600_80132A38(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent              = (Task*)task->spawnArg2.pointer;
    part                = task->spawnArg1.value;
    extra               = task->extra.tmd;
    parentExtra         = parent->extra.tmd;
    coord               = extra->coords;
    dest                = &parentExtra->coords[part];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = dest;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tick state of a part task: nothing to do, the parent drives it.
static void func_actor_135600_80132AB4(Task* task)
{
}

/// Dispatcher of the marker task: runs its state from
/// `D_actor_135600_80131E30`.
void func_actor_135600_80132ABC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E30;
    sp.funcs[task->state](task);
}

/// Setup state of the marker task (entry 0 of `D_actor_135600_80131E30`):
/// chains its model root under the parent task's part `spawnArg1`,
/// places the part's coordinate at (-150, 80, 0), turns its rotation by 90
/// degrees about Y, inherits the parent's light and colour matrices, and
/// reparents the task so it is updated with the parent. The kill countdown is
/// set to 0x1000, the value the marker's draw state runs on.
static void func_actor_135600_80132B14(Task* task)
{
    OverlayMat m;
    MATRIX*    mtx;
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent      = (Task*)task->spawnArg2.pointer;
    extra       = task->extra.tmd;
    part        = task->spawnArg1.value;
    parentExtra = parent->extra.tmd;
    coord       = extra->coords;
    dest        = &parentExtra->coords[part];

    coord->coord.t[0] = -0x96;
    coord->coord.t[1] = 0x50;
    coord->coord.t[2] = 0;

    mtx                    = &m.mat;
    m.ident.m00_m01        = 0x1000;
    MATRIX_PAIR(mtx, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1) = 0x1000;
    MATRIX_PAIR(mtx, 2, 0) = 0;
    mtx->m[2][2]           = 0x1000;

    RotMatrixY((s16)(0x400), mtx);
    MulMatrix0(&coord->coord, mtx, &coord->coord);

    coord->parent       = dest;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    extra->otOffset     = 0;
    Task_Reparent(parent, task);
    task->killCountdown = 0x1000;
    task->state        += 1;
}

/// Draw state of the marker task: while `gGameSession->eventState` is set and
/// the kill countdown is still running, draws the marker at the countdown's
/// length, cutting the countdown to 0x800 once the marker's angle reaches
/// 0x1F5.
static void func_actor_135600_80132C18(Task* task)
{
    s16 countdown;

    if (gGameSession->eventState != 0) {
        countdown = task->killCountdown;
        if (countdown > 0 && func_actor_135600_80131E68(task->extra.tmd->coords, countdown) >= 0x1F5) {
            task->killCountdown = 0x800;
        }
    }
}

/// Walks `coord->parent` up to world (`gGfxViewCoord`), composing each node's
/// `coord` rotation into `mtx` and accumulating the rotated translation into
/// `vec`. The world parent initializes `mtx` to identity and `vec` to zero.
/// The same algorithm as gameplay's `Gp_ComposeParentWorld`, but through the
/// library `ApplyMatrixSV` / `MulMatrix0` rather than the GTE macros.
static void func_actor_135600_80132C80(GfxCoord* coord, MATRIX* mtx, SVECTOR* vec)
{
    SVECTOR tmp;
    MATRIX* m;

    if (coord->parent != &gGfxViewCoord) {
        func_actor_135600_80132C80(coord->parent, mtx, vec);
    } else {
        m                    = mtx;
        *(s32*)m             = 0x1000;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = 0x1000;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = 0x1000;
        vec->vx              = 0;
        vec->vy              = 0;
        vec->vz              = 0;
    }

    tmp.vx = (u16)coord->coord.t[0];
    tmp.vy = (u16)coord->coord.t[1];
    tmp.vz = (u16)coord->coord.t[2];
    ApplyMatrixSV(mtx, &tmp, &tmp);
    vec->vx += tmp.vx;
    vec->vy += tmp.vy;
    vec->vz += tmp.vz;
    MulMatrix0(mtx, &coord->coord, mtx);
}

/// Dispatcher of the actor itself: runs its state from
/// `D_actor_135600_80131E3C`.
void func_actor_135600_80132D64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E3C;
    sp.funcs[task->state](task);
}

/// Exit state and exit callback of the actor: the `Gp_EnemyTaskExit` teardown.
static void func_actor_135600_80132DBC(Task* task)
{
    Gp_EnemyTaskExit(task);
}

/// Points the model's light and colour matrices at the work block's own pair.
static void func_actor_135600_80132DDC(Task* task)
{
    TmdObject*       ext;
    Actor135600Work* work;

    ext           = task->extra.tmd;
    work          = (Actor135600Work*)task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Entry 0 of the tick's handler pair, selected by `walk.motion` while no motion
/// sequence runs: does nothing.
static void func_actor_135600_80132DF8(Task* arg0)
{
}

/// Entry 1 of the tick's handler pair: runs the step of
/// `D_actor_135600_80131E48` that `walk.motionStep` selects.
static void func_actor_135600_80132E00(Task* task)
{
    Actor135600Work* work;
    TaskFuncTable4   fns;

    work = (Actor135600Work*)task->work;
    fns  = D_actor_135600_80131E48;
    fns.funcs[(s16)work->walk.motionStep](task);
}

#include "../../shared/actor_motion_face.inc.c"

/// Step 1: rotates the forward offset `D_actor_135600_80131E58` through the
/// root part's matrix into `work->walk.step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it, and advances the step.
static void func_actor_135600_80132F28(Task* task)
{
    Actor135600Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = task->extra.tmd->coords;
    work  = (Actor135600Work*)task->work;

    vec = D_actor_135600_80131E58;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_turn.inc.c"

#include "../../shared/actor_motion_play.inc.c"

/// The 0x7D4 entry of `D_actor_135600_8013B0F4`, also called by the setup
/// handler: drops the translation straight into the root part's local matrix, stores
/// the Euler angles in the coordinate's own `rot` slot and rebuilds the
/// rotation from them; clearing `composeStamp` makes the world matrix be recomputed.
/// Returns 0.
s32 func_actor_135600_801331C4(Task* task, s32 msgId, ActorTransform* args, s32 arg3)
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

/// The 0x7D5 entry of `D_actor_135600_8013B0F4`, the actor's visibility,
/// switched on the word `mode`. Flag 0x80 hides the model (the tick skips the
/// shadow while it is set). Mode 0 hides the model and clears flag 4, 1 shows
/// it, allocates its buffers and clears 4, 2 hides it, sets 4 and starts the
/// `freeCountdown` countdown at 2, and 3 shows it while setting 4. Anything else
/// returns 1 and leaves the flags alone; the handled modes return 0. Either
/// way the resulting flags are copied onto the objects of the three tasks the
/// setup state parked at `child0` / `child1` / `child2`.
s32 func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    Actor135600Work* work;
    TmdObject*       obj;
    TmdObject*       objA;
    TmdObject*       objB;
    TmdObject*       objC;
    s32              ret;

    work = (Actor135600Work*)task->work;
    obj  = task->extra.tmd;
    objB = work->child1->extra.tmd;
    objA = work->child0->extra.tmd;
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
    objB->flags = obj->flags;
    objA->flags = obj->flags;
    objC->flags = obj->flags;
    return ret;
}

/// The 0x7DB entry of `D_actor_135600_8013B0F4`: accepts the message and does
/// nothing with it.
s32 func_actor_135600_8013336C(void)
{
    return 0;
}
