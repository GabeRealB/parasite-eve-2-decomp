#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80163354.h"

#include "actors/actors_shared_801673f8.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/mad_chaser.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

extern EnemyParams   gMadChaserEnemyParams;  // the main enemy's `Enemy::param` record
extern AnimationSet* gMadChaserAnimBank[21]; // animation bank handed to `func_800B3F84`
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s16, VECTOR3*);
        void (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} Actor341700MessageEntry;
STATIC_ASSERT_SIZEOF(Actor341700MessageEntry, 8);

extern Actor341700MessageEntry gMadChaserMsgTable[3];   // stored into `Task::msgTable` by madChaserSpawn
extern u8                      gMadChaserAnimStance[];  // per animation id (1-based): value for `field_44F`
extern u8                      gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_341700_80162DCC(Task* arg0);
static void func_actor_341700_80164CDC(Task* arg0);
static void func_actor_341700_80165388(Task* arg0);
static void func_actor_341700_80165DDC(Task* arg0);
static void func_actor_341700_801670B0(Task* arg0);
static void func_actor_341700_80167C30(Task* arg0);
static void func_actor_341700_80167E18(Task* arg0);
static void func_actor_341700_80168124(Task* arg0);
void        func_actor_341700_801684A8(Task* arg0);
void        func_actor_341700_8016852C(Task* arg0);
static void func_actor_341700_8016859C(Task* arg0);
static void func_actor_341700_80168684(Task* arg0);
static void func_actor_341700_80168698(Task* arg0);
static void func_actor_341700_801686AC(Task* arg0);
static void func_actor_341700_801686C0(Task* arg0);
static void func_actor_341700_80168748(Task* arg0);
static void func_actor_341700_801687B4(Task* arg0);
static void func_actor_341700_80168820(Task* arg0);
static void func_actor_341700_80168874(Task* arg0);
static void func_actor_341700_801688C8(Task* arg0);
static void func_actor_341700_8016891C(Task* arg0);
static void func_actor_341700_801689A0(Task* arg0);
static void func_actor_341700_80168A14(Task* arg0);
static void func_actor_341700_80168F5C(Task* arg0);
static void func_actor_341700_80169218(Task* arg0);
static void func_actor_341700_801696C8(Task* arg0);
static void func_actor_341700_801696E0(Task* arg0);
static void func_actor_341700_801697B8(Task* arg0);
static void func_actor_341700_80169B40(Task* arg0);
static void func_actor_341700_80169BC8(Task* arg0);
static void func_actor_341700_80169CC4(Task* arg0);
static void func_actor_341700_80169D54(Task* arg0);
static void func_actor_341700_8016A810(Task* arg0);
static void func_actor_341700_8016A8EC(Task* arg0);
static void func_actor_341700_8016A8F4(Task* arg0);
static void func_actor_341700_8016AA58(Task* arg0);
static void func_actor_341700_8016ABF4(Task* arg0);

/// Six task-state handlers of the first enemy form, dispatched by
/// `func_actor_341700_8016852C` on `Task::state`.
static const TaskFuncTable6 D_actor_341700_80161E24 = { {
    madChaserSpawn,
    func_actor_341700_80165388,
    madChaserDangleFrame,
    func_actor_341700_80162DCC,
    func_actor_341700_80164CDC,
    func_actor_341700_8016859C,
} };

/// Ten task-state handlers of the second enemy form, dispatched by
/// `func_actor_341700_801684A8` on `Task::state`.
static const TaskFuncTable10 D_actor_341700_80161E3C = { {
    madChaserSpawnHidden,
    func_actor_341700_80165388,
    madChaserDangleFrame,
    func_actor_341700_80162DCC,
    func_actor_341700_80164CDC,
    func_actor_341700_8016859C,
    func_actor_341700_80165DDC,
    func_actor_341700_80168124,
    func_actor_341700_80167C30,
    func_actor_341700_80167E18,
} };

/// Eleven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable11 D_actor_341700_80161E64 = { {
    func_actor_341700_80168684,
    func_actor_341700_80168698,
    func_actor_341700_801686AC,
    func_actor_341700_801686C0,
    func_actor_341700_80168748,
    func_actor_341700_801687B4,
    func_actor_341700_80168820,
    func_actor_341700_80168874,
    func_actor_341700_801688C8,
    func_actor_341700_8016891C,
    func_actor_341700_801689A0,
} };

/// Sub-state handlers `func_actor_341700_8016891C` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_341700_80161E90 = { {
    madChaserKnockdownStart,
    madChaserKnockdownRise,
    madChaserKnockdownEnd,
} };

/// Sub-state handlers `func_actor_341700_801686C0` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_341700_80161E9C = { {
    madChaserWalkStart,
    madChaserWalkApproach,
    madChaserWalkFinish,
} };

/// Sub-state handlers `func_actor_341700_80168748` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_341700_80161EA8 = { {
    madChaserStartLeap,
    madChaserLeapAttack,
    madChaserLeapTurnAway,
    madChaserLeapRebound,
    madChaserLeapLand,
} };

/// Sub-state handlers `func_actor_341700_801687B4` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_341700_80161EBC = { {
    madChaserAlertCry,
    func_actor_341700_80168F5C,
    madChaserAlertRelease,
    madChaserAlertCrouch,
    madChaserAlertSidestep,
} };

/// Sub-state handlers `madChaserDangleState` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_341700_80161ED0 = { {
    func_actor_341700_80169218,
    madChaserDangleSway,
    madChaserDangleFall,
    madChaserDangleLand,
} };

extern TmdSource D_actor_341700_80171864;
void             func_actor_341700_801684A8(Task*);

void func_actor_341700_8016833C(Task*, s16, VECTOR3*);
void func_actor_341700_801684A8(Task*);
void func_actor_341700_8016852C(Task*);

TmdBone D_actor_341700_8016D388[1] = {
#include "assets/actor_341700_model_0C050_skeleton.inc"
};

u32 D_actor_341700_8016D3AC[1] = {
#include "assets/actor_341700_model_0C050_partVerts.inc"
};

SVECTOR D_actor_341700_8016D3B0[48] = {
#include "assets/actor_341700_model_0C050_verts.inc"
};

SVECTOR D_actor_341700_8016D530[53] = {
#include "assets/actor_341700_model_0C050_normals.inc"
};

u32 D_actor_341700_8016D6D8[486] = {
#include "assets/actor_341700_model_0C050_stream.inc"
};

TmdSource gMadChaserChunkModel0 = {
    0,
    3260,
    0,
    1,
    D_actor_341700_8016D3AC,
    D_actor_341700_8016D3B0,
    D_actor_341700_8016D530,
    D_actor_341700_8016D388,
    D_actor_341700_8016D6D8,
};

TmdBone D_actor_341700_8016DE94[1] = {
#include "assets/actor_341700_model_0C6F4_skeleton.inc"
};

u32 D_actor_341700_8016DEB8[1] = {
#include "assets/actor_341700_model_0C6F4_partVerts.inc"
};

SVECTOR D_actor_341700_8016DEBC[28] = {
#include "assets/actor_341700_model_0C6F4_verts.inc"
};

SVECTOR D_actor_341700_8016DF9C[37] = {
#include "assets/actor_341700_model_0C6F4_normals.inc"
};

u32 D_actor_341700_8016E0C4[276] = {
#include "assets/actor_341700_model_0C6F4_stream.inc"
};

TmdSource gMadChaserChunkModel1 = {
    0,
    1828,
    0,
    1,
    D_actor_341700_8016DEB8,
    D_actor_341700_8016DEBC,
    D_actor_341700_8016DF9C,
    D_actor_341700_8016DE94,
    D_actor_341700_8016E0C4,
};

TmdBone D_actor_341700_8016E538[1] = {
#include "assets/actor_341700_model_0CC64_skeleton.inc"
};

u32 D_actor_341700_8016E55C[1] = {
#include "assets/actor_341700_model_0CC64_partVerts.inc"
};

SVECTOR D_actor_341700_8016E560[25] = {
#include "assets/actor_341700_model_0CC64_verts.inc"
};

SVECTOR D_actor_341700_8016E628[32] = {
#include "assets/actor_341700_model_0CC64_normals.inc"
};

u32 D_actor_341700_8016E728[215] = {
#include "assets/actor_341700_model_0CC64_stream.inc"
};

TmdSource gMadChaserChunkModel2 = {
    0,
    1448,
    0,
    1,
    D_actor_341700_8016E55C,
    D_actor_341700_8016E560,
    D_actor_341700_8016E628,
    D_actor_341700_8016E538,
    D_actor_341700_8016E728,
};

TmdBone D_actor_341700_8016EAA8[9] = {
#include "assets/actor_341700_model_0FA44_skeleton.inc"
};

u32 D_actor_341700_8016EBEC[9] = {
#include "assets/actor_341700_model_0FA44_partVerts.inc"
};

SVECTOR D_actor_341700_8016EC10[160] = {
#include "assets/actor_341700_model_0FA44_verts.inc"
};

SVECTOR D_actor_341700_8016F110[206] = {
#include "assets/actor_341700_model_0FA44_normals.inc"
};

u32 D_actor_341700_8016F780[2105] = {
#include "assets/actor_341700_model_0FA44_stream.inc"
};

TmdSource D_actor_341700_80171864 = {
    0,
    11212,
    3088,
    9,
    D_actor_341700_8016EBEC,
    D_actor_341700_8016EC10,
    D_actor_341700_8016F110,
    D_actor_341700_8016EAA8,
    D_actor_341700_8016F780,
};

DamageAttack D_actor_341700_80171888[1] = {
    { 22, 0 },
};

EnemyParams gMadChaserEnemyParams = { D_actor_341700_80171888, 110, 20, 40, 1, 100, 10, 100, 0 };

AnimationPackedPose D_actor_341700_8017189C[6] = {
#include "assets/actor_341700_animation_0FCA8_bank1.inc"
};

AnimationPackedRotation D_actor_341700_801718E4[39] = {
#include "assets/actor_341700_animation_0FCA8_bank4.inc"
};

AnimationRecord D_actor_341700_80171980[77] = {
#include "assets/actor_341700_animation_0FCA8_records.inc"
};

u16 D_actor_341700_80171AB4[10] = {
#include "assets/actor_341700_animation_0FCA8_indices.inc"
};

AnimationSet D_actor_341700_80171AC8 = {
    D_actor_341700_80171980,
    D_actor_341700_80171AB4,
    { NULL, D_actor_341700_8017189C, NULL, NULL, D_actor_341700_801718E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80171AF0[3] = {
#include "assets/actor_341700_animation_0FE4C_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80171B14[21] = {
#include "assets/actor_341700_animation_0FE4C_bank4.inc"
};

AnimationRecord D_actor_341700_80171B68[60] = {
#include "assets/actor_341700_animation_0FE4C_records.inc"
};

u16 D_actor_341700_80171C58[10] = {
#include "assets/actor_341700_animation_0FE4C_indices.inc"
};

AnimationSet D_actor_341700_80171C6C = {
    D_actor_341700_80171B68,
    D_actor_341700_80171C58,
    { NULL, D_actor_341700_80171AF0, NULL, NULL, D_actor_341700_80171B14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80171C94[20] = {
#include "assets/actor_341700_animation_10320_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80171D84[99] = {
#include "assets/actor_341700_animation_10320_bank4.inc"
};

AnimationRecord D_actor_341700_80171F10[135] = {
#include "assets/actor_341700_animation_10320_records.inc"
};

u16 D_actor_341700_8017212C[10] = {
#include "assets/actor_341700_animation_10320_indices.inc"
};

AnimationSet D_actor_341700_80172140 = {
    D_actor_341700_80171F10,
    D_actor_341700_8017212C,
    { NULL, D_actor_341700_80171C94, NULL, NULL, D_actor_341700_80171D84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80172168[25] = {
#include "assets/actor_341700_animation_108A4_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80172294[110] = {
#include "assets/actor_341700_animation_108A4_bank4.inc"
};

AnimationRecord D_actor_341700_8017244C[153] = {
#include "assets/actor_341700_animation_108A4_records.inc"
};

u16 D_actor_341700_801726B0[10] = {
#include "assets/actor_341700_animation_108A4_indices.inc"
};

AnimationSet D_actor_341700_801726C4 = {
    D_actor_341700_8017244C,
    D_actor_341700_801726B0,
    { NULL, D_actor_341700_80172168, NULL, NULL, D_actor_341700_80172294, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_801726EC[2] = {
#include "assets/actor_341700_animation_109A4_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80172704[7] = {
#include "assets/actor_341700_animation_109A4_bank4.inc"
};

AnimationRecord D_actor_341700_80172720[36] = {
#include "assets/actor_341700_animation_109A4_records.inc"
};

u16 D_actor_341700_801727B0[10] = {
#include "assets/actor_341700_animation_109A4_indices.inc"
};

AnimationSet D_actor_341700_801727C4 = {
    D_actor_341700_80172720,
    D_actor_341700_801727B0,
    { NULL, D_actor_341700_801726EC, NULL, NULL, D_actor_341700_80172704, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_801727EC[2] = {
#include "assets/actor_341700_animation_10AA4_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80172804[7] = {
#include "assets/actor_341700_animation_10AA4_bank4.inc"
};

AnimationRecord D_actor_341700_80172820[36] = {
#include "assets/actor_341700_animation_10AA4_records.inc"
};

u16 D_actor_341700_801728B0[10] = {
#include "assets/actor_341700_animation_10AA4_indices.inc"
};

AnimationSet D_actor_341700_801728C4 = {
    D_actor_341700_80172820,
    D_actor_341700_801728B0,
    { NULL, D_actor_341700_801727EC, NULL, NULL, D_actor_341700_80172804, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_801728EC[24] = {
#include "assets/actor_341700_animation_10ED4_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80172A0C[69] = {
#include "assets/actor_341700_animation_10ED4_bank4.inc"
};

AnimationRecord D_actor_341700_80172B20[112] = {
#include "assets/actor_341700_animation_10ED4_records.inc"
};

u16 D_actor_341700_80172CE0[10] = {
#include "assets/actor_341700_animation_10ED4_indices.inc"
};

AnimationSet D_actor_341700_80172CF4 = {
    D_actor_341700_80172B20,
    D_actor_341700_80172CE0,
    { NULL, D_actor_341700_801728EC, NULL, NULL, D_actor_341700_80172A0C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80172D1C[22] = {
#include "assets/actor_341700_animation_11378_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80172E24[87] = {
#include "assets/actor_341700_animation_11378_bank4.inc"
};

AnimationRecord D_actor_341700_80172F80[129] = {
#include "assets/actor_341700_animation_11378_records.inc"
};

u16 D_actor_341700_80173184[10] = {
#include "assets/actor_341700_animation_11378_indices.inc"
};

AnimationSet D_actor_341700_80173198 = {
    D_actor_341700_80172F80,
    D_actor_341700_80173184,
    { NULL, D_actor_341700_80172D1C, NULL, NULL, D_actor_341700_80172E24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_801731C0[14] = {
#include "assets/actor_341700_animation_118D0_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80173268[99] = {
#include "assets/actor_341700_animation_118D0_bank4.inc"
};

AnimationRecord D_actor_341700_801733F4[186] = {
#include "assets/actor_341700_animation_118D0_records.inc"
};

u16 D_actor_341700_801736DC[10] = {
#include "assets/actor_341700_animation_118D0_indices.inc"
};

AnimationSet D_actor_341700_801736F0 = {
    D_actor_341700_801733F4,
    D_actor_341700_801736DC,
    { NULL, D_actor_341700_801731C0, NULL, NULL, D_actor_341700_80173268, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80173718[8] = {
#include "assets/actor_341700_animation_11B84_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80173778[54] = {
#include "assets/actor_341700_animation_11B84_bank4.inc"
};

AnimationRecord D_actor_341700_80173850[80] = {
#include "assets/actor_341700_animation_11B84_records.inc"
};

u16 D_actor_341700_80173990[10] = {
#include "assets/actor_341700_animation_11B84_indices.inc"
};

AnimationSet D_actor_341700_801739A4 = {
    D_actor_341700_80173850,
    D_actor_341700_80173990,
    { NULL, D_actor_341700_80173718, NULL, NULL, D_actor_341700_80173778, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_801739CC[7] = {
#include "assets/actor_341700_animation_11E18_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80173A20[50] = {
#include "assets/actor_341700_animation_11E18_bank4.inc"
};

AnimationRecord D_actor_341700_80173AE8[79] = {
#include "assets/actor_341700_animation_11E18_records.inc"
};

u16 D_actor_341700_80173C24[10] = {
#include "assets/actor_341700_animation_11E18_indices.inc"
};

AnimationSet D_actor_341700_80173C38 = {
    D_actor_341700_80173AE8,
    D_actor_341700_80173C24,
    { NULL, D_actor_341700_801739CC, NULL, NULL, D_actor_341700_80173A20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80173C60[7] = {
#include "assets/actor_341700_animation_120C8_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80173CB4[53] = {
#include "assets/actor_341700_animation_120C8_bank4.inc"
};

AnimationRecord D_actor_341700_80173D88[83] = {
#include "assets/actor_341700_animation_120C8_records.inc"
};

u16 D_actor_341700_80173ED4[10] = {
#include "assets/actor_341700_animation_120C8_indices.inc"
};

AnimationSet D_actor_341700_80173EE8 = {
    D_actor_341700_80173D88,
    D_actor_341700_80173ED4,
    { NULL, D_actor_341700_80173C60, NULL, NULL, D_actor_341700_80173CB4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80173F10[6] = {
#include "assets/actor_341700_animation_122DC_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80173F58[42] = {
#include "assets/actor_341700_animation_122DC_bank4.inc"
};

AnimationRecord D_actor_341700_80174000[58] = {
#include "assets/actor_341700_animation_122DC_records.inc"
};

u16 D_actor_341700_801740E8[10] = {
#include "assets/actor_341700_animation_122DC_indices.inc"
};

AnimationSet D_actor_341700_801740FC = {
    D_actor_341700_80174000,
    D_actor_341700_801740E8,
    { NULL, D_actor_341700_80173F10, NULL, NULL, D_actor_341700_80173F58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80174124[2] = {
#include "assets/actor_341700_animation_123FC_bank1.inc"
};

AnimationPackedRotation D_actor_341700_8017413C[11] = {
#include "assets/actor_341700_animation_123FC_bank4.inc"
};

AnimationRecord D_actor_341700_80174168[40] = {
#include "assets/actor_341700_animation_123FC_records.inc"
};

u16 D_actor_341700_80174208[10] = {
#include "assets/actor_341700_animation_123FC_indices.inc"
};

AnimationSet D_actor_341700_8017421C = {
    D_actor_341700_80174168,
    D_actor_341700_80174208,
    { NULL, D_actor_341700_80174124, NULL, NULL, D_actor_341700_8017413C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80174244[4] = {
#include "assets/actor_341700_animation_12538_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80174274[18] = {
#include "assets/actor_341700_animation_12538_bank4.inc"
};

AnimationRecord D_actor_341700_801742BC[34] = {
#include "assets/actor_341700_animation_12538_records.inc"
};

u16 D_actor_341700_80174344[10] = {
#include "assets/actor_341700_animation_12538_indices.inc"
};

AnimationSet D_actor_341700_80174358 = {
    D_actor_341700_801742BC,
    D_actor_341700_80174344,
    { NULL, D_actor_341700_80174244, NULL, NULL, D_actor_341700_80174274, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80174380[10] = {
#include "assets/actor_341700_animation_12734_bank1.inc"
};

AnimationPackedRotation D_actor_341700_801743F8[31] = {
#include "assets/actor_341700_animation_12734_bank4.inc"
};

AnimationRecord D_actor_341700_80174474[51] = {
#include "assets/actor_341700_animation_12734_records.inc"
};

u16 D_actor_341700_80174540[10] = {
#include "assets/actor_341700_animation_12734_indices.inc"
};

AnimationSet D_actor_341700_80174554 = {
    D_actor_341700_80174474,
    D_actor_341700_80174540,
    { NULL, D_actor_341700_80174380, NULL, NULL, D_actor_341700_801743F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_8017457C[16] = {
#include "assets/actor_341700_animation_12B70_bank1.inc"
};

AnimationPackedRotation D_actor_341700_8017463C[83] = {
#include "assets/actor_341700_animation_12B70_bank4.inc"
};

AnimationRecord D_actor_341700_80174788[125] = {
#include "assets/actor_341700_animation_12B70_records.inc"
};

u16 D_actor_341700_8017497C[10] = {
#include "assets/actor_341700_animation_12B70_indices.inc"
};

AnimationSet D_actor_341700_80174990 = {
    D_actor_341700_80174788,
    D_actor_341700_8017497C,
    { NULL, D_actor_341700_8017457C, NULL, NULL, D_actor_341700_8017463C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_801749B8[5] = {
#include "assets/actor_341700_animation_12CFC_bank1.inc"
};

AnimationPackedRotation D_actor_341700_801749F4[26] = {
#include "assets/actor_341700_animation_12CFC_bank4.inc"
};

AnimationRecord D_actor_341700_80174A5C[43] = {
#include "assets/actor_341700_animation_12CFC_records.inc"
};

u16 D_actor_341700_80174B08[10] = {
#include "assets/actor_341700_animation_12CFC_indices.inc"
};

AnimationSet D_actor_341700_80174B1C = {
    D_actor_341700_80174A5C,
    D_actor_341700_80174B08,
    { NULL, D_actor_341700_801749B8, NULL, NULL, D_actor_341700_801749F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_341700_80174B44[5] = {
#include "assets/actor_341700_animation_12EA4_bank1.inc"
};

AnimationPackedRotation D_actor_341700_80174B80[30] = {
#include "assets/actor_341700_animation_12EA4_bank4.inc"
};

AnimationRecord D_actor_341700_80174BF8[46] = {
#include "assets/actor_341700_animation_12EA4_records.inc"
};

u16 D_actor_341700_80174CB0[10] = {
#include "assets/actor_341700_animation_12EA4_indices.inc"
};

AnimationSet D_actor_341700_80174CC4 = {
    D_actor_341700_80174BF8,
    D_actor_341700_80174CB0,
    { NULL, D_actor_341700_80174B44, NULL, NULL, D_actor_341700_80174B80, NULL, NULL, NULL },
};

AnimationSet* gMadChaserAnimBank[21] = {
    NULL,
    &D_actor_341700_80171AC8,
    &D_actor_341700_80171C6C,
    &D_actor_341700_80172140,
    &D_actor_341700_801726C4,
    &D_actor_341700_801727C4,
    &D_actor_341700_801728C4,
    &D_actor_341700_80172CF4,
    &D_actor_341700_80173198,
    &D_actor_341700_801736F0,
    &D_actor_341700_801739A4,
    &D_actor_341700_80173C38,
    &D_actor_341700_80173EE8,
    &D_actor_341700_801740FC,
    &D_actor_341700_8017421C,
    &D_actor_341700_80174358,
    &D_actor_341700_80174554,
    &D_actor_341700_80174990,
    &D_actor_341700_80174B1C,
    &D_actor_341700_80174CC4,
    NULL,
};

Actor341700MessageEntry gMadChaserMsgTable[3] = {
    { 2004, { .call0 = func_actor_341700_8016833C } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = madChaserCommandMsg } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_341700_80174D58 = { { { TASK_BODY_TMD, 96 } }, func_actor_341700_8016852C, { .model = &D_actor_341700_80171864 } };

TaskDesc D_actor_341700_80174D64 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, func_actor_341700_801684A8, { .model = &D_actor_341700_80171864 } };

TaskDesc D_actor_341700_80174D70 = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_341700_80174D7C = { { { TASK_BODY_TMD, 96 } }, func_actor_341700_801684A8, { .model = &D_actor_341700_80171864 } };

u8 gMadChaserAnimStance[20] = {
    0,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    0,
};

u8 gMadChaserSettleAnims[40] = {
    5,
    6,
    5,
    6,
    5,
    6,
    6,
    5,
    5,
    5,
    6,
    6,
    5,
    5,
    5,
    6,
    5,
    6,
    5,
    0,
    2,
    70,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    80,
    0,
    62,
    200,
    210,
    1,
};

extern void* D_800678F0[1];

extern TmdSource gMadChaserChunkModel0;

extern TmdSource gMadChaserChunkModel1;

extern TmdSource gMadChaserChunkModel2;

static __inline__ s16  take_hit(Task* arg0);
static __inline__ void update_rotation(Task* arg0);
static __inline__ s16  take_hit_nibble3(Task* arg0);
static __inline__ void set_state_s16(Task* arg0, s16 state);

#include "../../shared/mad_chaser_limb_shadow.inc.c"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. It is declared as a one-element array for the
 * same reason as in `actor_400500`: as a bare scalar, GCC 2.8.1 decides the
 * store cannot alias the `TmdObject` loads and sinks it past them. */

/* The records closing three of the overlay's model streams, selected through
   `D_800678F0`. */

#include "../../shared/mad_chaser_spawn_gibs.inc.c"

#include "../../shared/mad_chaser_twist.inc.c"

#include "../../shared/mad_chaser_spawn.inc.c"

#include "../../shared/mad_chaser_spawn_hidden.inc.c"

#include "../../shared/mad_chaser_inlines.inc.c"

/// Message 0x2C00 (see `field_44C`) consumes the message and restarts the
/// state machine: low nibble 2 enters state 3 at state index 10 unless
/// `field_438` is set, low nibble 3 enters state 7. Returns 1 when it did, so
/// the caller skips this frame's state handler.
///
/// Each arm has to `return 1` on its own, with `return 0` after them: that
/// leaves a `hit = 0` block between the second arm and the join, so jump2
/// cannot cross-jump the first arm's `field_422` store into the second's
/// (dbr later steals the `hit = 0` into the branch delay slots and the block
/// disappears). A flag set to 0 up front and to 1 in each arm cross-jumps.
static __inline__ s16 take_hit(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    Actor341700Work* w2;

    if ((work->field_44C & 0xF) == 2) {
        if (work->field_438 == 0) {
            work->field_44C = 0;
            madChaserEnterState(arg0, 3);
            w2            = (Actor341700Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        madChaserEnterState(arg0, 7);
        return 1;
    }
    return 0;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off the scratch stack, copying the 3x3 into the root coordinate.
static __inline__ void update_rotation(Task* arg0)
{
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    MATRIX*          m     = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    GfxCoord*        coord = arg0->extra.tmd->coords;
    MATRIX*          dst;

    work->field_78              &= 0xFFF;
    work->field_7A              &= 0xFFF;
    work->field_7C              &= 0xFFF;
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    RotMatrixY(work->field_7A, m);
    dst          = &coord->coord;
    dst->m[0][0] = m->m[0][0];
    dst->m[0][1] = m->m[0][1];
    dst->m[0][2] = m->m[0][2];
    dst->m[1][0] = m->m[1][0];
    dst->m[1][1] = m->m[1][1];
    dst->m[1][2] = m->m[1][2];
    dst->m[2][0] = m->m[2][0];
    dst->m[2][1] = m->m[2][1];
    SCRATCH_STACK_RELEASE_BYTES(0x20);
    dst->m[2][2] = m->m[2][2];
}

/// Per-frame callback for the main enemy, the eleven-state counterpart of
/// `func_actor_341700_80165DDC`. In mode 0 it aims at the nearest actor
/// (`madChaserTrackPlayer`), lets a pending hit (`take_hit`) replace the state
/// handler, eases `field_424` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `field_448` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `field_438` is clear.
static void func_actor_341700_80162DCC(Task* arg0)
{
    Enemy*           enemy = arg0->spawnArg2.pointer;
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable11  sp    = D_actor_341700_80161E64;
    s32              cur;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            madChaserTrackPlayer(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            madChaserTwistSpine(arg0);
            if (work->field_432 == 1) {
                madChaserPinPart(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            update_rotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                madChaserEnterState(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->hp <= 0) {
                madChaserEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                madChaserEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                madChaserEnterState(arg0, 9);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}

#include "../../shared/mad_chaser_walk_start.inc.c"

#include "../../shared/mad_chaser_walk_approach.inc.c"

#include "../../shared/mad_chaser_leap_attack.inc.c"

#include "../../shared/mad_chaser_leap_turn_away.inc.c"

#include "../../shared/mad_chaser_leap_rebound.inc.c"

#include "../../shared/mad_chaser_dangle_frame.inc.c"

#include "../../shared/mad_chaser_dangle_fall.inc.c"

#include "../../shared/mad_chaser_dangle_land.inc.c"

#include "../../shared/mad_chaser_contacts.inc.c"

#include "../../shared/mad_chaser_tick_anim.inc.c"

#include "../../shared/mad_chaser_bodies.inc.c"

/// Nine state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable9 D_actor_341700_80161F0C = { {
    madChaserDeathCry,
    madChaserDeathSettle,
    madChaserDeathWaitAnim,
    madChaserBeginDeath,
    madChaserDeathTurnTranslucent,
    madChaserShrinkWithDust,
    func_actor_341700_801696C8,
    func_actor_341700_801696E0,
    madChaserBurst,
} };

/// Per-frame callback of the main enemy. `gSceneCombatState.actorControl` 2 hides the model,
/// 0 runs the current state handler (then colours it), 1 only colours it.
/// Unless `field_451` is set, it then runs `madChaserDrawLimbShadow` for
/// three part pairs.
static void func_actor_341700_80164CDC(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable9   sp    = D_actor_341700_80161F0C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/mad_chaser_shrink_dust.inc.c"

#include "../../shared/mad_chaser_track_player.inc.c"

#include "../../shared/mad_chaser_recoil_recover.inc.c"

/// The five state handlers of the second enemy form, indexed by
/// `Actor341700Work::field_420`; copied to the stack before dispatch. It sits
/// between `madChaserRecoilRecover`'s jump table and this function's own.
static const TaskFuncTable5 D_actor_341700_80161F48 = { {
    func_actor_341700_80169B40,
    func_actor_341700_80169BC8,
    madChaserLurkRiseState,
    func_actor_341700_80169CC4,
    func_actor_341700_80169D54,
} };

/// Per-frame callback for the second enemy form, the five-state counterpart
/// of `func_actor_341700_80162DCC`: in mode 0 it aims (`madChaserTrackPlayer`),
/// lets a pending hit replace the state handler, rebuilds the root rotation,
/// then picks the next state - 4 when dead, 8 / 9 for messages 4 / 5, and
/// state 3 after a consumed `field_448` request. Mode 1 only recolours; both
/// clear bit 0x80 of the model's `field_C`, which mode 2 sets.
static void func_actor_341700_80165388(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_341700_80161F48;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            madChaserTrackPlayer(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            madChaserTwistSpine(arg0);
            update_rotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->field_438 == 0 && enemy->hp <= 0) {
                madChaserEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                madChaserEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                madChaserEnterState(arg0, 9);
            } else if (madChaserTakeRequest(arg0)) {
                work->field_438 = 0;
                madChaserEnterState(arg0, 3);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Sub-state handlers `func_actor_341700_80169B40` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_341700_80161F70 = { {
    madChaserStartHold,
    madChaserLurkWait,
    madChaserLurkIdleEnd,
} };

/// Sub-state handlers `func_actor_341700_80169BC8` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_341700_80161F7C = { {
    madChaserLurkCrouch,
    madChaserLurkRaise,
    madChaserLurkLookAround,
} };

/// Sub-state handlers `madChaserLurkRiseState` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_341700_80161F88 = { {
    madChaserStartAlert,
    madChaserLurkBrace,
    madChaserLurkSidestepToCombat,
} };

/// Sub-state handlers `func_actor_341700_80169D54` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_341700_80161F94 = { {
    madChaserLurkShiftStart,
    madChaserLurkShiftBrace,
    madChaserLurkSidestepRight,
    madChaserLurkSidestepLeft,
} };

/// Ten state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable10 D_actor_341700_80161FA4 = { {
    madChaserEmergeAtSpot,
    madChaserEmergeBackflip,
    madChaserEmergeHopForward,
    madChaserCreepUntilHit,
    madChaserEmergeArcBack,
    madChaserEmergeHopBack,
    madChaserEmergeBackOff,
    madChaserEmergeHighArc,
    madChaserEmergeFlipOver,
    func_actor_341700_801670B0,
} };

/// Sub-state handlers `func_actor_341700_801689A0` dispatches by `field_422`.
static const TaskFuncTable6 D_actor_341700_80161FCC = { {
    madChaserPullStart,
    madChaserPullReact,
    madChaserPulledStruggle,
    madChaserPulledIn,
    madChaserPulledLimp,
    madChaserPulledIn,
} };

/// Five state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable5 D_actor_341700_80161FE4 = { {
    madChaserDeathCryUnlink,
    madChaserDeathSettleQuiet,
    func_actor_341700_8016A810,
    madChaserDropBodies,
    func_actor_341700_8016A8EC,
} };

/// Seven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable7 D_actor_341700_80161FF8 = { {
    func_actor_341700_8016A8F4,
    madChaserDeathSettleQuiet,
    func_actor_341700_8016A810,
    madChaserBeginShrink,
    func_actor_341700_8016AA58,
    madChaserShrink,
    func_actor_341700_8016ABF4,
} };

#include "../../shared/mad_chaser_lurk_look.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_combat.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_right.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_left.inc.c"

/// Message 0x2C00 with low nibble 3 (see `field_44C`) consumes the message and
/// moves the task to state 7 with a fresh state machine; returns 1 when it did,
/// so the caller skips this frame's state handler. The `s16` result is what
/// keeps the `move` between the flag and its test, and the reload through a
/// second local is what puts it in `$v1`.
static __inline__ s16 take_hit_nibble3(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              hit  = 0;
    Actor341700Work* w2;

    if ((work->field_44C & 0xF) == 3) {
        hit             = 1;
        work->field_44C = 0;
        arg0->state     = 7;
        w2              = (Actor341700Work*)arg0->work;
        w2->field_420   = 0;
        w2->field_422   = 0;
    }
    return hit;
}

/// Per-frame callback, the ten-state counterpart of
/// `func_actor_341700_80164CDC`: in mode 0 a pending hit (`take_hit_nibble3`) replaces
/// the state handler, and the root rotation is rebuilt from 0x78..0x7C before
/// `madChaserApplyContacts`.
static void func_actor_341700_80165DDC(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable10  sp    = D_actor_341700_80161FA4;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            if (take_hit_nibble3(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            madChaserTickAnim(arg0);
            update_rotation(arg0);
            madChaserApplyContacts(arg0, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// `madChaserSetStateS16` with an `s16` state. The narrower parameter is load-bearing:
/// with the `s32` one, `madChaserEmergeAtSpot` no longer matches. Each
/// call site reloads `work`, and cross-jumping merges the identical stores,
/// which is what leaves one `lw` per arm in front of a shared tail.
static __inline__ void set_state_s16(Task* arg0, s16 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

#include "../../shared/mad_chaser_emerge_at_spot.inc.c"

#include "../../shared/mad_chaser_emerge_backflip.inc.c"

#include "../../shared/mad_chaser_emerge_hop_forward.inc.c"

#include "../../shared/mad_chaser_creep.inc.c"

#include "../../shared/mad_chaser_emerge_arc_back.inc.c"

#include "../../shared/mad_chaser_emerge_hop_back.inc.c"

#include "../../shared/mad_chaser_emerge_back_off.inc.c"

#include "../../shared/mad_chaser_emerge_high_arc.inc.c"

#include "../../shared/mad_chaser_emerge_flip_over.inc.c"

/// The same creep as a second entry of the state table.
#define madChaserCreepUntilHit func_actor_341700_801670B0
#include "../../shared/mad_chaser_creep.inc.c"
#undef madChaserCreepUntilHit

#include "../../shared/mad_chaser_pulled_struggle.inc.c"

#include "../../shared/mad_chaser_pulled_in.inc.c"

#include "../../shared/mad_chaser_pulled_limp.inc.c"

/// Per-frame callback, the five-state counterpart of
/// `func_actor_341700_80167E18`; unlike it, clears bit 0x80 of `field_C` on
/// the way out of modes 0 and 1.
static void func_actor_341700_80167C30(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_341700_80161FE4;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Per-frame callback, the seven-state counterpart of
/// `func_actor_341700_80164CDC`: in mode 0 it also spawns effect 3 on the
/// model's second coord part every 32 frames.
static void func_actor_341700_80167E18(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable7   sp    = D_actor_341700_80161FF8;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            madChaserUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                madChaserDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                madChaserDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/mad_chaser_sound_bank.inc.c"

static void func_actor_341700_80168124(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserVanish,
        madChaserVanishFree,
    };

    states[(s16)work->field_420](arg0);
}

/// Once bit 7 of `gSceneCombatState.madChaserAlertOwner` is set, puts the task in state 3 with
/// the state machine at state 5 and returns 1; otherwise returns 0.
s16 madChaserJoinAlert(Task* arg0)
{
    if ((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED) {
        madChaserEnterState(arg0, 3);
        madChaserSetStateS16(arg0, 5);
        return 1;
    }
    return 0;
}

#include "../../shared/mad_chaser_alert_hold.inc.c"

#include "../../shared/mad_chaser_take_request.inc.c"

#include "../../shared/mad_chaser_command_msg.inc.c"

/// Moves the model: writes `pos` into the root part's translation and marks
/// the coordinate dirty. `part` is accepted but unused.
void func_actor_341700_8016833C(Task* task, s16 part, VECTOR3* pos)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = pos->vx;
    coord->coord.t[1]   = pos->vy;
    coord->coord.t[2]   = pos->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/mad_chaser_pin_part.inc.c"

/// Scales `arg1` by the animation speed `field_41C`, in 1/16 units.
s32 madChaserScaleBySpeed(Task* arg0, s16 arg1)
{
    return (s32)((((Actor341700Work*)arg0->work)->field_41C * arg1) << 0xC) >> 0x10;
}

/// Returns 1 when the hit flags are set - bit 0 of the flag halfword or bits
/// 0x102 of the word - and 0 otherwise.
s16 madChaserAnimEnded(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if ((work->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

void func_actor_341700_801684A8(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = D_actor_341700_80161E3C;
    sp.funcs[arg0->state](arg0);
}

/// Runs the handler for the task's `Task::state` from the six-entry table.
void func_actor_341700_8016852C(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_341700_80161E24;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_341700_8016859C(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_341700_801697B8,
        madChaserDespawn,
    };

    states[(s16)work->field_420](arg0);
}

#include "../../shared/mad_chaser_turn_to_player.inc.c"

static void func_actor_341700_80168684(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void func_actor_341700_80168698(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void func_actor_341700_801686AC(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Unless `madChaserTakeHitRequest` consumes a pending request, runs the
/// sub-state handler for `field_422` from a three-entry table.
static void func_actor_341700_801686C0(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161E9C;
    if ((madChaserTakeHitRequest(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Runs the sub-state handler for `field_422` from a five-entry table.
static void func_actor_341700_80168748(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161EA8;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from another five-entry table.
static void func_actor_341700_801687B4(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161EBC;
    sp.funcs[(s16)work->field_422](arg0);
}

static void func_actor_341700_80168820(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserRecoilLight,
        madChaserRecoilRecover,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_341700_80168874(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserRecoilHeavy,
        madChaserRecoilHeavyEnd,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_341700_801688C8(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_341700_80168A14,
        madChaserStatusHold,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_341700_8016891C(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161E90;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        madChaserTakeKnockdownRequest(arg0);
    }
}

static void func_actor_341700_801689A0(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable6   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161FCC;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 0xC and advances the sub-state.
static void func_actor_341700_80168A14(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

#include "../../shared/mad_chaser_status_hold.inc.c"

#include "../../shared/mad_chaser_knockdown_start.inc.c"

#include "../../shared/mad_chaser_knockdown_rise.inc.c"

#include "../../shared/mad_chaser_knockdown_end.inc.c"

#include "../../shared/mad_chaser_walk_finish.inc.c"

#include "../../shared/mad_chaser_start_leap.inc.c"

#include "../../shared/mad_chaser_leap_land.inc.c"

#include "../../shared/mad_chaser_alert_cry.inc.c"

/// Advances the sub-state once the frame counter has passed 0x50.
static void func_actor_341700_80168F5C(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

#include "../../shared/mad_chaser_alert_release.inc.c"

#include "../../shared/mad_chaser_alert_crouch.inc.c"

#include "../../shared/mad_chaser_alert_sidestep.inc.c"

/// Runs the sub-state handler for `field_422` from a four-entry table.
void madChaserDangleState(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161ED0;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Sets `field_432`, requests animation 7 and advances the sub-state.
static void func_actor_341700_80169218(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work             = (Actor341700Work*)arg0->work;
    work->field_432  = 1;
    work2            = (Actor341700Work*)arg0->work;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}

#include "../../shared/mad_chaser_dangle_sway.inc.c"

#include "../../shared/mad_chaser_death_cry.inc.c"

#include "../../shared/mad_chaser_death_settle.inc.c"

#include "../../shared/mad_chaser_death_wait_anim.inc.c"

#include "../../shared/mad_chaser_begin_death.inc.c"

#include "../../shared/mad_chaser_death_translucent.inc.c"

static void func_actor_341700_801696C8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// Advances the state after two frames.
static void func_actor_341700_801696E0(Task* arg0)
{
    u16              ticks;
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

#include "../../shared/mad_chaser_burst.inc.c"

static void func_actor_341700_801697B8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

#include "../../shared/mad_chaser_despawn.inc.c"

#include "../../shared/mad_chaser_recoil_light.inc.c"

#include "../../shared/mad_chaser_recoil_heavy.inc.c"

#include "../../shared/mad_chaser_recoil_heavy_end.inc.c"

static void func_actor_341700_80169B40(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F70;
    if ((madChaserJoinAlert(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

static void func_actor_341700_80169BC8(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F7C;
    if ((madChaserJoinAlert(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

#include "../../shared/mad_chaser_lurk_rise_state.inc.c"

static void func_actor_341700_80169CC4(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F88;
    if ((madChaserJoinAlert(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from a four-entry table.
static void func_actor_341700_80169D54(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_341700_80161F94;
    sp.funcs[(s16)work->field_422](arg0);
}

#include "../../shared/mad_chaser_start_hold.inc.c"

#include "../../shared/mad_chaser_lurk_wait.inc.c"

#include "../../shared/mad_chaser_lurk_idle_end.inc.c"

#include "../../shared/mad_chaser_lurk_crouch.inc.c"

#include "../../shared/mad_chaser_lurk_raise.inc.c"

/// Requests animation 0xF and advances the sub-state.
void madChaserLurkRiseStart(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

#include "../../shared/mad_chaser_lurk_rise_end.inc.c"

#include "../../shared/mad_chaser_start_alert.inc.c"

#include "../../shared/mad_chaser_lurk_brace.inc.c"

#include "../../shared/mad_chaser_lurk_shift_start.inc.c"

#include "../../shared/mad_chaser_lurk_shift_brace.inc.c"

#include "../../shared/mad_chaser_pull_start.inc.c"

#include "../../shared/mad_chaser_pull_react.inc.c"

#include "../../shared/mad_chaser_vanish.inc.c"

#include "../../shared/mad_chaser_vanish_free.inc.c"

#include "../../shared/mad_chaser_death_cry_unlink.inc.c"

#include "../../shared/mad_chaser_death_settle_quiet.inc.c"

/// A further copy, under this file's own name.
#define madChaserDeathWaitAnim func_actor_341700_8016A810
#include "../../shared/mad_chaser_death_wait_anim.inc.c"
#undef madChaserDeathWaitAnim

#include "../../shared/mad_chaser_drop_bodies.inc.c"

/// Empty state handler.
static void func_actor_341700_8016A8EC(Task* arg0)
{
}

/// A further copy, under this file's own name.
#define madChaserDeathCryUnlink func_actor_341700_8016A8F4
#include "../../shared/mad_chaser_death_cry_unlink.inc.c"
#undef madChaserDeathCryUnlink

#include "../../shared/mad_chaser_begin_shrink.inc.c"

/// A further copy, under this file's own name.
#define madChaserDeathTurnTranslucent func_actor_341700_8016AA58
#include "../../shared/mad_chaser_death_translucent.inc.c"
#undef madChaserDeathTurnTranslucent

#include "../../shared/mad_chaser_shrink.inc.c"

static void func_actor_341700_8016ABF4(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

#include "../../shared/mad_chaser_take_knockdown_request.inc.c"
