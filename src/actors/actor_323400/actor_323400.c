#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"
#include "../../shared/desert_chaser.h"

/// Psy-Q `RotMatrixY`.

/// Whole-unit part of the last movement step `func_actor_323400_80162A2C`
/// applied, rounded away from zero when the step had a fraction.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    SVECTOR value;
    u8      retained[8];
} Actor323400Storage1218;
STATIC_ASSERT_SIZEOF(Actor323400Storage1218, 16);

static Actor323400Storage1218 ActorContact_ScratchPosition;

/// Per-state animation table `desertChaserAnimTick` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 gRigClipStartFrames[];

/// Animation source `func_800B3F84` is handed for both of the work block's
/// contexts.
extern u8 gRigAnimSource[];

/// Message table published as `Task::msgTable` by the spawn handler.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*);
        s32  (*call1)(Task*, s32, AnimationPlayRequest*, s32);
        s32  (*call2)(Task*, s32, ActorCommand* request, s32);
        s32  (*call3)(Task*, s32, ActorTransform*);
        s32  (*call4)(Task*, s32, s32);
        void (*call5)(void);
    } handler;
} Actor323400MessageEntry;
STATIC_ASSERT_SIZEOF(Actor323400MessageEntry, 8);

extern Actor323400MessageEntry gRigMessages[7];

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    EffectSpawnArg value;
    u8             retained[88];
} Actor323400Storage1228;
STATIC_ASSERT_SIZEOF(Actor323400Storage1228, 96);

extern Actor323400Storage1228 gRigEffectRec;

/// Enemy parameters the spawn handler stores in `Enemy::param`.
extern EnemyParams gRigParams;

static void func_actor_323400_801641C4(Enemy* enemy, Task* task);
static void func_actor_323400_801644C4(Enemy* enemy, Task* task);
static void func_actor_323400_80164B98(Enemy* arg0, Task* arg1);
static void func_actor_323400_80164BD0(Enemy* enemy, Task* task);
static void func_actor_323400_80164C4C(Enemy* enemy, Task* task);

/// State handlers `func_actor_323400_801644C4` runs by `Actor323000Work::field_0`.
#include "../../shared/actor_contacts.h"

static const GpEnemyTaskFuncTable4 D_actor_323400_80161E24 = {
    func_actor_323400_80164B98,
    func_actor_323400_80164BD0,
    func_actor_323400_801641C4,
    func_actor_323400_80164C4C,
};

/// Task states `func_actor_323400_80164CEC` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 D_actor_323400_80161E34 = {
    desertChaserSpawn,
    func_actor_323400_801644C4,
    Gp_DestroyEnemy,
};

extern TmdSource D_actor_323400_80169878;
s32              func_actor_323400_80164824(Task*);
s32              func_actor_323400_80164974(Task*, s32, ActorCommand* msg, s32);
s32              func_actor_323400_80164A50(Task*, s32, AnimationPlayRequest*, s32);
void             func_actor_323400_8016475C(void);
void             func_actor_323400_80164CEC(Task*);

DamageAttack D_actor_323400_80164D48[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams gRigParams = { D_actor_323400_80164D48, 200, 75, 50, 4, 100, 10, 100, 0 };

s16 D_actor_323400_80164D6C[16] = {
    60,
    36,
    10,
    150,
    40,
    26,
    10,
    120,
    20,
    18,
    10,
    120,
    60,
    60,
    10,
    150,
};

TmdBone D_actor_323400_80164D8C[18] = {
#include "assets/actor_323400_model_07A58_skeleton.inc"
};

u32 D_actor_323400_80165014[18] = {
#include "assets/actor_323400_model_07A58_partVerts.inc"
};

SVECTOR D_actor_323400_8016505C[266] = {
#include "assets/actor_323400_model_07A58_verts.inc"
};

SVECTOR D_actor_323400_801658AC[324] = {
#include "assets/actor_323400_model_07A58_normals.inc"
};

u32 D_actor_323400_801662CC[3435] = {
#include "assets/actor_323400_model_07A58_stream.inc"
};

TmdSource D_actor_323400_80169878 = {
    0,
    17616,
    5944,
    18,
    D_actor_323400_80165014,
    D_actor_323400_8016505C,
    D_actor_323400_801658AC,
    D_actor_323400_80164D8C,
    D_actor_323400_801662CC,
};

AnimationPackedPose D_actor_323400_8016989C[10] = {
#include "assets/actor_323400_animation_07F8C_bank1.inc"
};

AnimationPackedRotation D_actor_323400_80169914[110] = {
#include "assets/actor_323400_animation_07F8C_bank4.inc"
};

AnimationRecord D_actor_323400_80169ACC[175] = {
#include "assets/actor_323400_animation_07F8C_records.inc"
};

u16 D_actor_323400_80169D88[18] = {
#include "assets/actor_323400_animation_07F8C_indices.inc"
};

AnimationSet D_actor_323400_80169DAC = {
    D_actor_323400_80169ACC,
    D_actor_323400_80169D88,
    { NULL, D_actor_323400_8016989C, NULL, NULL, D_actor_323400_80169914, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_80169DD4[5] = {
#include "assets/actor_323400_animation_08248_bank1.inc"
};

AnimationPackedRotation D_actor_323400_80169E10[29] = {
#include "assets/actor_323400_animation_08248_bank4.inc"
};

AnimationRecord D_actor_323400_80169E84[112] = {
#include "assets/actor_323400_animation_08248_records.inc"
};

u16 D_actor_323400_8016A044[18] = {
#include "assets/actor_323400_animation_08248_indices.inc"
};

AnimationSet D_actor_323400_8016A068 = {
    D_actor_323400_80169E84,
    D_actor_323400_8016A044,
    { NULL, D_actor_323400_80169DD4, NULL, NULL, D_actor_323400_80169E10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016A090[13] = {
#include "assets/actor_323400_animation_0887C_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016A12C[129] = {
#include "assets/actor_323400_animation_0887C_bank4.inc"
};

AnimationRecord D_actor_323400_8016A330[210] = {
#include "assets/actor_323400_animation_0887C_records.inc"
};

u16 D_actor_323400_8016A678[18] = {
#include "assets/actor_323400_animation_0887C_indices.inc"
};

AnimationSet D_actor_323400_8016A69C = {
    D_actor_323400_8016A330,
    D_actor_323400_8016A678,
    { NULL, D_actor_323400_8016A090, NULL, NULL, D_actor_323400_8016A12C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016A6C4[11] = {
#include "assets/actor_323400_animation_08E54_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016A748[131] = {
#include "assets/actor_323400_animation_08E54_bank4.inc"
};

AnimationRecord D_actor_323400_8016A954[191] = {
#include "assets/actor_323400_animation_08E54_records.inc"
};

u16 D_actor_323400_8016AC50[18] = {
#include "assets/actor_323400_animation_08E54_indices.inc"
};

AnimationSet D_actor_323400_8016AC74 = {
    D_actor_323400_8016A954,
    D_actor_323400_8016AC50,
    { NULL, D_actor_323400_8016A6C4, NULL, NULL, D_actor_323400_8016A748, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016AC9C[9] = {
#include "assets/actor_323400_animation_09288_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016AD08[83] = {
#include "assets/actor_323400_animation_09288_bank4.inc"
};

AnimationRecord D_actor_323400_8016AE54[140] = {
#include "assets/actor_323400_animation_09288_records.inc"
};

u16 D_actor_323400_8016B084[18] = {
#include "assets/actor_323400_animation_09288_indices.inc"
};

AnimationSet D_actor_323400_8016B0A8 = {
    D_actor_323400_8016AE54,
    D_actor_323400_8016B084,
    { NULL, D_actor_323400_8016AC9C, NULL, NULL, D_actor_323400_8016AD08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016B0D0[19] = {
#include "assets/actor_323400_animation_09960_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016B1B4[138] = {
#include "assets/actor_323400_animation_09960_bank4.inc"
};

AnimationRecord D_actor_323400_8016B3DC[224] = {
#include "assets/actor_323400_animation_09960_records.inc"
};

u16 D_actor_323400_8016B75C[18] = {
#include "assets/actor_323400_animation_09960_indices.inc"
};

AnimationSet D_actor_323400_8016B780 = {
    D_actor_323400_8016B3DC,
    D_actor_323400_8016B75C,
    { NULL, D_actor_323400_8016B0D0, NULL, NULL, D_actor_323400_8016B1B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016B7A8[14] = {
#include "assets/actor_323400_animation_09F60_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016B850[99] = {
#include "assets/actor_323400_animation_09F60_bank4.inc"
};

AnimationRecord D_actor_323400_8016B9DC[224] = {
#include "assets/actor_323400_animation_09F60_records.inc"
};

u16 D_actor_323400_8016BD5C[18] = {
#include "assets/actor_323400_animation_09F60_indices.inc"
};

AnimationSet D_actor_323400_8016BD80 = {
    D_actor_323400_8016B9DC,
    D_actor_323400_8016BD5C,
    { NULL, D_actor_323400_8016B7A8, NULL, NULL, D_actor_323400_8016B850, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016BDA8[4] = {
#include "assets/actor_323400_animation_0A1BC_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016BDD8[34] = {
#include "assets/actor_323400_animation_0A1BC_bank4.inc"
};

AnimationRecord D_actor_323400_8016BE60[86] = {
#include "assets/actor_323400_animation_0A1BC_records.inc"
};

u16 D_actor_323400_8016BFB8[18] = {
#include "assets/actor_323400_animation_0A1BC_indices.inc"
};

AnimationSet D_actor_323400_8016BFDC = {
    D_actor_323400_8016BE60,
    D_actor_323400_8016BFB8,
    { NULL, D_actor_323400_8016BDA8, NULL, NULL, D_actor_323400_8016BDD8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016C004[12] = {
#include "assets/actor_323400_animation_0A7E0_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016C094[147] = {
#include "assets/actor_323400_animation_0A7E0_bank4.inc"
};

AnimationRecord D_actor_323400_8016C2E0[191] = {
#include "assets/actor_323400_animation_0A7E0_records.inc"
};

u16 D_actor_323400_8016C5DC[18] = {
#include "assets/actor_323400_animation_0A7E0_indices.inc"
};

AnimationSet D_actor_323400_8016C600 = {
    D_actor_323400_8016C2E0,
    D_actor_323400_8016C5DC,
    { NULL, D_actor_323400_8016C004, NULL, NULL, D_actor_323400_8016C094, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016C628[9] = {
#include "assets/actor_323400_animation_0AC30_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016C694[73] = {
#include "assets/actor_323400_animation_0AC30_bank4.inc"
};

AnimationRecord D_actor_323400_8016C7B8[157] = {
#include "assets/actor_323400_animation_0AC30_records.inc"
};

u16 D_actor_323400_8016CA2C[18] = {
#include "assets/actor_323400_animation_0AC30_indices.inc"
};

AnimationSet D_actor_323400_8016CA50 = {
    D_actor_323400_8016C7B8,
    D_actor_323400_8016CA2C,
    { NULL, D_actor_323400_8016C628, NULL, NULL, D_actor_323400_8016C694, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016CA78[12] = {
#include "assets/actor_323400_animation_0B214_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016CB08[126] = {
#include "assets/actor_323400_animation_0B214_bank4.inc"
};

AnimationRecord D_actor_323400_8016CD00[196] = {
#include "assets/actor_323400_animation_0B214_records.inc"
};

u16 D_actor_323400_8016D010[18] = {
#include "assets/actor_323400_animation_0B214_indices.inc"
};

AnimationSet D_actor_323400_8016D034 = {
    D_actor_323400_8016CD00,
    D_actor_323400_8016D010,
    { NULL, D_actor_323400_8016CA78, NULL, NULL, D_actor_323400_8016CB08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016D05C[2] = {
#include "assets/actor_323400_animation_0B390_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016D074[16] = {
#include "assets/actor_323400_animation_0B390_bank4.inc"
};

AnimationRecord D_actor_323400_8016D0B4[54] = {
#include "assets/actor_323400_animation_0B390_records.inc"
};

u16 D_actor_323400_8016D18C[18] = {
#include "assets/actor_323400_animation_0B390_indices.inc"
};

AnimationSet D_actor_323400_8016D1B0 = {
    D_actor_323400_8016D0B4,
    D_actor_323400_8016D18C,
    { NULL, D_actor_323400_8016D05C, NULL, NULL, D_actor_323400_8016D074, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016D1D8[13] = {
#include "assets/actor_323400_animation_0BA34_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016D274[158] = {
#include "assets/actor_323400_animation_0BA34_bank4.inc"
};

AnimationRecord D_actor_323400_8016D4EC[209] = {
#include "assets/actor_323400_animation_0BA34_records.inc"
};

u16 D_actor_323400_8016D830[18] = {
#include "assets/actor_323400_animation_0BA34_indices.inc"
};

AnimationSet D_actor_323400_8016D854 = {
    D_actor_323400_8016D4EC,
    D_actor_323400_8016D830,
    { NULL, D_actor_323400_8016D1D8, NULL, NULL, D_actor_323400_8016D274, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016D87C[21] = {
#include "assets/actor_323400_animation_0C27C_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016D978[193] = {
#include "assets/actor_323400_animation_0C27C_bank4.inc"
};

AnimationRecord D_actor_323400_8016DC7C[254] = {
#include "assets/actor_323400_animation_0C27C_records.inc"
};

u16 D_actor_323400_8016E074[20] = {
#include "assets/actor_323400_animation_0C27C_indices.inc"
};

AnimationSet D_actor_323400_8016E09C = {
    D_actor_323400_8016DC7C,
    D_actor_323400_8016E074,
    { NULL, D_actor_323400_8016D87C, NULL, NULL, D_actor_323400_8016D978, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016E0C4[20] = {
#include "assets/actor_323400_animation_0CA18_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016E1B4[172] = {
#include "assets/actor_323400_animation_0CA18_bank4.inc"
};

AnimationRecord D_actor_323400_8016E464[235] = {
#include "assets/actor_323400_animation_0CA18_records.inc"
};

u16 D_actor_323400_8016E810[20] = {
#include "assets/actor_323400_animation_0CA18_indices.inc"
};

AnimationSet D_actor_323400_8016E838 = {
    D_actor_323400_8016E464,
    D_actor_323400_8016E810,
    { NULL, D_actor_323400_8016E0C4, NULL, NULL, D_actor_323400_8016E1B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016E860[15] = {
#include "assets/actor_323400_animation_0D224_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016E914[206] = {
#include "assets/actor_323400_animation_0D224_bank4.inc"
};

AnimationRecord D_actor_323400_8016EC4C[244] = {
#include "assets/actor_323400_animation_0D224_records.inc"
};

u16 D_actor_323400_8016F01C[20] = {
#include "assets/actor_323400_animation_0D224_indices.inc"
};

AnimationSet D_actor_323400_8016F044 = {
    D_actor_323400_8016EC4C,
    D_actor_323400_8016F01C,
    { NULL, D_actor_323400_8016E860, NULL, NULL, D_actor_323400_8016E914, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016F06C[14] = {
#include "assets/actor_323400_animation_0DA38_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016F114[207] = {
#include "assets/actor_323400_animation_0DA38_bank4.inc"
};

AnimationRecord D_actor_323400_8016F450[248] = {
#include "assets/actor_323400_animation_0DA38_records.inc"
};

u16 D_actor_323400_8016F830[20] = {
#include "assets/actor_323400_animation_0DA38_indices.inc"
};

AnimationSet D_actor_323400_8016F858 = {
    D_actor_323400_8016F450,
    D_actor_323400_8016F830,
    { NULL, D_actor_323400_8016F06C, NULL, NULL, D_actor_323400_8016F114, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016F880[5] = {
#include "assets/actor_323400_animation_0DCEC_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016F8BC[55] = {
#include "assets/actor_323400_animation_0DCEC_bank4.inc"
};

AnimationRecord D_actor_323400_8016F998[83] = {
#include "assets/actor_323400_animation_0DCEC_records.inc"
};

u16 D_actor_323400_8016FAE4[20] = {
#include "assets/actor_323400_animation_0DCEC_indices.inc"
};

AnimationSet D_actor_323400_8016FB0C = {
    D_actor_323400_8016F998,
    D_actor_323400_8016FAE4,
    { NULL, D_actor_323400_8016F880, NULL, NULL, D_actor_323400_8016F8BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016FB34[6] = {
#include "assets/actor_323400_animation_0E00C_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016FB7C[66] = {
#include "assets/actor_323400_animation_0E00C_bank4.inc"
};

AnimationRecord D_actor_323400_8016FC84[96] = {
#include "assets/actor_323400_animation_0E00C_records.inc"
};

u16 D_actor_323400_8016FE04[20] = {
#include "assets/actor_323400_animation_0E00C_indices.inc"
};

AnimationSet D_actor_323400_8016FE2C = {
    D_actor_323400_8016FC84,
    D_actor_323400_8016FE04,
    { NULL, D_actor_323400_8016FB34, NULL, NULL, D_actor_323400_8016FB7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323400_8016FE54[20] = {
#include "assets/actor_323400_animation_0EA4C_bank1.inc"
};

AnimationPackedRotation D_actor_323400_8016FF44[247] = {
#include "assets/actor_323400_animation_0EA4C_bank4.inc"
};

AnimationRecord D_actor_323400_80170320[330] = {
#include "assets/actor_323400_animation_0EA4C_records.inc"
};

u16 D_actor_323400_80170848[18] = {
#include "assets/actor_323400_animation_0EA4C_indices.inc"
};

AnimationSet D_actor_323400_8017086C = {
    D_actor_323400_80170320,
    D_actor_323400_80170848,
    { NULL, D_actor_323400_8016FE54, NULL, NULL, D_actor_323400_8016FF44, NULL, NULL, NULL },
};

s8 gRigClipStartFrames[2028] = {
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    5,
    5,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 gRigAnimSource[340] = {
    172,
    157,
    22,
    128,
    104,
    160,
    22,
    128,
    156,
    166,
    22,
    128,
    116,
    172,
    22,
    128,
    168,
    176,
    22,
    128,
    128,
    183,
    22,
    128,
    128,
    189,
    22,
    128,
    220,
    191,
    22,
    128,
    0,
    198,
    22,
    128,
    80,
    202,
    22,
    128,
    52,
    208,
    22,
    128,
    176,
    209,
    22,
    128,
    84,
    216,
    22,
    128,
    108,
    8,
    23,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    156,
    224,
    22,
    128,
    68,
    240,
    22,
    128,
    12,
    251,
    22,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    56,
    232,
    22,
    128,
    88,
    248,
    22,
    128,
    44,
    254,
    22,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    60,
    0,
    244,
    255,
    30,
    0,
    2,
    0,
    206,
    255,
    126,
    255,
    29,
    0,
    2,
    0,
    20,
    0,
    186,
    255,
    25,
    0,
    2,
    0,
    226,
    255,
    191,
    255,
    25,
    0,
    2,
    0,
    60,
    0,
    136,
    255,
    30,
    0,
    2,
    0,
    20,
    0,
    236,
    255,
    251,
    255,
    2,
    0,
    241,
    255,
    206,
    255,
    0,
    0,
    2,
    0,
    2,
    0,
    10,
    0,
    241,
    255,
    2,
    0,
    14,
    0,
    0,
    0,
    0,
    0,
    7,
    0,
    25,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    242,
    255,
    0,
    0,
    0,
    0,
    9,
    0,
    231,
    255,
    0,
    0,
    0,
    0,
    2,
    0,
};

Actor323400MessageEntry gRigMessages[7] = {
    { 2015, { .call5 = func_actor_323400_8016475C } },
    { 2005, { .call4 = desertChaserSetVisibility } },
    { 2006, { .call0 = func_actor_323400_80164824 } },
    { 2004, { .call3 = actorMsgPlaceYawFirst } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_323400_80164974 } },
    { 2003, { .call1 = func_actor_323400_80164A50 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_323400_8017120C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, func_actor_323400_80164CEC, { .model = &D_actor_323400_80169878 } };

static Actor323400Storage1218 ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &(ActorContact_ScratchPosition.value);
}

Actor323400Storage1228 gRigEffectRec;

static void func_actor_323400_80164AA0(Task* task, s16 arg1, s16 arg2);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/desert_chaser_blend_tick.inc.c"

/// Per-frame effect dispatch keyed on `field_82E` and the record each animation
/// slot has reached. A recognised record is handled once: `field_848` remembers,
/// per slot, the record last handled, and meeting it again only clears `reset`.
/// A handled record spawns its effects while the room effect mode is 2 and
/// returns a request word; otherwise the result is 0, after wiping `field_848`
/// when no case claimed a record.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 desertChaserAnimCues(Task* task, Actor323000Work* work)
{
    SVECTOR vec;
    s32     reset;
    s32     steer;
    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002220, &vec);
                    }
                    return 0x40010002;
                }
                work->field_848[9] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[7].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0x3E) {
                    old = work->field_848[7];
                    if (old != clip) {
                        work->field_848[7] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[7] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0x84) {
                    old = work->field_848[14];
                    if (old != clip) {
                        work->field_848[14] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80002220, &vec);
                        }
                        return 0x40010002;
                    }
                    work->field_848[14] = old;
                    reset               = 0;
                }
            }
            {
                s32 clip = work->slots[17].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xA9) {
                    old = work->field_848[17];
                    if (old != clip) {
                        work->field_848[17] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[17] = old;
                    reset               = 0;
                }
            }
            break;

        case 10: {
            s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x9) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, task->extra.tmd->coords, 0x80004A00, &vec);
                    }
                    return 0x40010005;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        } break;

        case 3: {
            s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x4) {
                reset = 0;
                old   = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    return 0x40010004;
                }
                work->field_848[1] = old;
            }
        }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0x8) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        return 0x40010003;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;

        case 6: {
            s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x6) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80003200, &vec);
                    }
                    vec.vz = 0;
                    vec.vx = 0;
                    vec.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80003200, &vec);
                    }
                    return 0;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xB) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80004480, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80004480, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xC) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002200, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x2BC;
                        if (steer == 0 && gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002240, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (steer == 0 && gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80003300, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80003340, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xD) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80002200, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80002300, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;
    }

    if (reset == 1) {
        Mem_Set(work->field_848, 0, 0x48);
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

#include "../../shared/desert_chaser_spawn.inc.c"

/// State 2 of `D_actor_323400_80161E24`. On entry it flags the enemy's link
/// node, shows the model (clears its flags) and rebuilds its buffers, resets
/// the slots to clip 0xD and zeroes the frame counter `field_6` before the
/// tick. Otherwise it advances `field_6` and, on frames 9, 10, 12 and 13,
/// spawns effect 0x60054 at the matching model part while the room's effect
/// mode is 2 (0x2BC up at parts 9 and 7, 0x258 up at 14 and 17); frame 10
/// also plays a placed sound, and frame 13 always spawns one more at part 1.
/// The tick then runs and the root coordinate is marked for rebuilding.
static void func_actor_323400_801641C4(Enemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        desertChaserAnimTick(task);
        return;
    }
    switch (++work->field_6) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000E;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    desertChaserAnimTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `field_4`, and runs the state handler `field_0` selects from a stack copy
/// of `D_actor_323400_80161E24`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
static void func_actor_323400_801644C4(Enemy* enemy, Task* task)
{
    Actor323000Work*        work;
    GpEnemyTaskFuncTable4   sp;
    Actor323000TickScratch* scratch;
    u8*                     head;
    GfxCoord*               walker;
    SVECTOR*                pos;

    work = (Actor323000Work*)task->work;
    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sp                                    = D_actor_323400_80161E24;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    head                                  = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8)              = head - 0x1C;
    scratch                               = (Actor323000TickScratch*)(head - 0x1C);
    Gp_UpdateCoord(task->extra.tmd->coords);
    scratch->pos.vx = task->extra.tmd->coords->workm.t[0];
    scratch->pos.vy = task->extra.tmd->coords->workm.t[1];
    scratch->pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &scratch->pos, 0, 0);
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    sp.funcs[work->field_0](enemy, task);
    scratch->local.vx = 0;
    scratch->local.vy = 0;
    scratch->local.vz = 0;
    {
        SVECTOR  local;
        VECTOR   result;
        s32      flag;
        SVECTOR* localp = &local;

        walker   = &task->extra.tmd->coords[2];
        pos      = &scratch->local;
        local.vx = scratch->local.vx;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->parent == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(localp);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->parent;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    enemy->bodyPos.vx = scratch->local.vx;
    enemy->bodyPos.vy = scratch->local.vy;
    enemy->bodyPos.vz = scratch->local.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

void func_actor_323400_8016475C(void)
{
}

#include "../../shared/desert_chaser_visibility.inc.c"

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points,
/// and otherwise 1 only when the model has neither flag 0x80 nor flag 2 set.
s32 func_actor_323400_80164824(Task* task)
{
    u16 flags;

    if (((Enemy*)task->spawnArg2.pointer)->hp <= 0) {
        flags = task->extra.tmd->flags;
        if (flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 0;
        }
        if (flags & 2) {
            return 0;
        }
    }
    return 1;
}

#include "../../shared/actor_messages_place_yaw_first.inc.c"

/// Handler for message 0x7DB: copies the payload's three leading bytes into
/// the work block and, when its `code` is 0x1602, picks the state from `mode`:
/// 1 moves the root coordinate to (0x4330, 1, 0xA8C), marks it for rebuilding
/// and starts state 2; 0 and 2 restart state 0; any other mode only stores the
/// bytes.
s32 func_actor_323400_80164974(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    Actor323000Work* work;
    u16              mode;

    work = (Actor323000Work*)task->work;

    work->field_91C = msg->context.loc.stage;
    work->field_91D = msg->context.loc.area;
    work->field_91E = (u8)msg->command;

    if (msg->context.key == 0x1602) {
        mode = msg->command;
        switch (mode) {
            case 1:
                task->extra.tmd->coords->coord.t[0]   = 0x4330;
                task->extra.tmd->coords->coord.t[1]   = mode;
                task->extra.tmd->coords->coord.t[2]   = 0xA8C;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->field_0                         = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
        }
    }
    return 0;
}

/// Handler for message 0x7D3: latches the requested animation id into
/// `field_82E` and restarts the state machine at state 1.
s32 func_actor_323400_80164A50(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor323000Work* work = (Actor323000Work*)task->work;

    work->field_82E = msg->animationId;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void desertChaserExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Spawns effect 0x60054 at coordinate `arg1` of the actor's model while the
/// room's effect set is live, with the offset that limb uses (none at the
/// root and part 1, 0x2BC up at parts 9 and 7, 0x258 up at 14 and 17); the
/// spawn argument is `arg2` with bit 31 set. Other coordinates spawn nothing.
/// Nothing in this package calls it.
static void func_actor_323400_80164AA0(Task* task, s16 arg1, s16 arg2)
{
    SVECTOR sp10;
    s32     spawn;

    switch (arg1) {
        case 0:
        case 1:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 7:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn   = 1;
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && spawn == 1) {
        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[arg1], arg2 | 0x80000000, &sp10);
    }
}

/// State 0 of `D_actor_323400_80161E24`: when the work block's `field_4` flag
/// is set, flags the enemy's link node and hides the model (raises flag
/// 0x80). `obj` gets its own local: the fused form ranks the `Task::extra`
/// load with the store and transposes it.
static void func_actor_323400_80164B98(Enemy* arg0, Task* arg1)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)arg1->work;
    if (work->field_4 != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// State 1 of `D_actor_323400_80161E24`: on entry clears the enemy's link-node
/// flags, shows the model (clears its flags), rebuilds its buffers and resets
/// the slots to the current clip `field_82E` with both turn targets zeroed.
/// The tick runs every frame.
static void func_actor_323400_80164BD0(Enemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        desertChaserAnimTick(task);
    } else {
        desertChaserAnimTick(task);
    }
}

/// State 3 of `D_actor_323400_80161E24`: on entry flags the enemy's link node,
/// shows the model (clears its flags), rebuilds its buffers and re-seeds the
/// slots with clip 2 from the per-state table, with both turn targets zeroed.
/// On later frames the tick runs and the root coordinate is marked for
/// rebuilding.
static void func_actor_323400_80164C4C(Enemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;

    work = (Actor323000Work*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 2;
        work->field_828 = 1;
        work->field_83E = 0;
        work->field_840 = 0;
        desertChaserAnimTick(task);
    } else {
        desertChaserAnimTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Task body of the actor's descriptor: runs the handler for the task's
/// state from a stack copy of `D_actor_323400_80161E34` - the spawn handler,
/// the per-frame driver, then `Gp_DestroyEnemy`.
void func_actor_323400_80164CEC(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_323400_80161E34;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
