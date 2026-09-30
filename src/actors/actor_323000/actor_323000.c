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
#include "../../shared/blend_rig_creature.h"

/// Animation source `func_800B3F84` is handed for both of the work block's
/// contexts.
extern u8 gRigAnimSource[];

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    EffectSpawnArg value;
    u8             retained[88];
} Actor323000Storage3A24;
STATIC_ASSERT_SIZEOF(Actor323000Storage3A24, 96);

extern Actor323000Storage3A24 gRigEffectRec;

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
} Actor323000MessageEntry;
STATIC_ASSERT_SIZEOF(Actor323000MessageEntry, 8);

extern Actor323000MessageEntry gRigMessages[7];

/// Enemy parameters the spawn handler stores in `Enemy::param`.
extern EnemyParams gRigParams;

/// Whole-unit part of the last movement step `func_actor_323000_80162A2C`
/// applied, rounded away from zero when the step had a fraction.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    SVECTOR value;
    u8      retained[8];
} Actor323000Storage3A14;
STATIC_ASSERT_SIZEOF(Actor323000Storage3A14, 16);

static Actor323000Storage3A14 ActorContact_ScratchPosition;

/// Per-state animation table `rigAnimTick` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 gRigClipStartFrames[];

/// Psy-Q `RotMatrixY`.

static void func_actor_323000_8016409C(Enemy* enemy, Task* task);
static void func_actor_323000_8016420C(Enemy* enemy, Task* task);
static void func_actor_323000_801645A4(Enemy* enemy, Task* task);
static void func_actor_323000_80164C20(Enemy* arg0, Task* arg1);
static void func_actor_323000_80164C58(Enemy* enemy, Task* task);

/// State handlers `func_actor_323000_801645A4` runs by `Actor323000Work::field_0`.
#include "../../shared/actor_contacts.h"

static const GpEnemyTaskFuncTable4 D_actor_323000_80161E24 = {
    func_actor_323000_80164C20,
    func_actor_323000_8016409C,
    func_actor_323000_80164C58,
    func_actor_323000_8016420C,
};

/// Task states `func_actor_323000_80164CE4` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `Gp_DestroyEnemy`.
static const GpEnemyTaskFuncTable3 D_actor_323000_80161E34 = {
    rigSpawn,
    func_actor_323000_801645A4,
    Gp_DestroyEnemy,
};

extern TmdSource D_actor_323000_80169870;
s32              func_actor_323000_80164904(Task*);
s32              func_actor_323000_80164A54(Task*, s32, ActorCommand* msg, s32);
s32              func_actor_323000_80164AF0(Task*, s32, AnimationPlayRequest*, s32);
void             func_actor_323000_8016483C(void);
void             func_actor_323000_80164CE4(Task*);

DamageAttack D_actor_323000_80164D40[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams gRigParams = { D_actor_323000_80164D40, 200, 75, 50, 4, 100, 10, 100, 0 };

s16 D_actor_323000_80164D64[16] = {
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

TmdBone D_actor_323000_80164D84[18] = {
#include "assets/actor_323000_model_07A50_skeleton.inc"
};

u32 D_actor_323000_8016500C[18] = {
#include "assets/actor_323000_model_07A50_partVerts.inc"
};

SVECTOR D_actor_323000_80165054[266] = {
#include "assets/actor_323000_model_07A50_verts.inc"
};

SVECTOR D_actor_323000_801658A4[324] = {
#include "assets/actor_323000_model_07A50_normals.inc"
};

u32 D_actor_323000_801662C4[3435] = {
#include "assets/actor_323000_model_07A50_stream.inc"
};

TmdSource D_actor_323000_80169870 = {
    0,
    17616,
    5944,
    18,
    D_actor_323000_8016500C,
    D_actor_323000_80165054,
    D_actor_323000_801658A4,
    D_actor_323000_80164D84,
    D_actor_323000_801662C4,
};

AnimationPackedPose D_actor_323000_80169894[10] = {
#include "assets/actor_323000_animation_07F84_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016990C[110] = {
#include "assets/actor_323000_animation_07F84_bank4.inc"
};

AnimationRecord D_actor_323000_80169AC4[175] = {
#include "assets/actor_323000_animation_07F84_records.inc"
};

u16 D_actor_323000_80169D80[18] = {
#include "assets/actor_323000_animation_07F84_indices.inc"
};

AnimationSet D_actor_323000_80169DA4 = {
    D_actor_323000_80169AC4,
    D_actor_323000_80169D80,
    { NULL, D_actor_323000_80169894, NULL, NULL, D_actor_323000_8016990C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_80169DCC[5] = {
#include "assets/actor_323000_animation_08240_bank1.inc"
};

AnimationPackedRotation D_actor_323000_80169E08[29] = {
#include "assets/actor_323000_animation_08240_bank4.inc"
};

AnimationRecord D_actor_323000_80169E7C[112] = {
#include "assets/actor_323000_animation_08240_records.inc"
};

u16 D_actor_323000_8016A03C[18] = {
#include "assets/actor_323000_animation_08240_indices.inc"
};

AnimationSet D_actor_323000_8016A060 = {
    D_actor_323000_80169E7C,
    D_actor_323000_8016A03C,
    { NULL, D_actor_323000_80169DCC, NULL, NULL, D_actor_323000_80169E08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016A088[13] = {
#include "assets/actor_323000_animation_08874_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016A124[129] = {
#include "assets/actor_323000_animation_08874_bank4.inc"
};

AnimationRecord D_actor_323000_8016A328[210] = {
#include "assets/actor_323000_animation_08874_records.inc"
};

u16 D_actor_323000_8016A670[18] = {
#include "assets/actor_323000_animation_08874_indices.inc"
};

AnimationSet D_actor_323000_8016A694 = {
    D_actor_323000_8016A328,
    D_actor_323000_8016A670,
    { NULL, D_actor_323000_8016A088, NULL, NULL, D_actor_323000_8016A124, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016A6BC[11] = {
#include "assets/actor_323000_animation_08E4C_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016A740[131] = {
#include "assets/actor_323000_animation_08E4C_bank4.inc"
};

AnimationRecord D_actor_323000_8016A94C[191] = {
#include "assets/actor_323000_animation_08E4C_records.inc"
};

u16 D_actor_323000_8016AC48[18] = {
#include "assets/actor_323000_animation_08E4C_indices.inc"
};

AnimationSet D_actor_323000_8016AC6C = {
    D_actor_323000_8016A94C,
    D_actor_323000_8016AC48,
    { NULL, D_actor_323000_8016A6BC, NULL, NULL, D_actor_323000_8016A740, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016AC94[9] = {
#include "assets/actor_323000_animation_09280_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016AD00[83] = {
#include "assets/actor_323000_animation_09280_bank4.inc"
};

AnimationRecord D_actor_323000_8016AE4C[140] = {
#include "assets/actor_323000_animation_09280_records.inc"
};

u16 D_actor_323000_8016B07C[18] = {
#include "assets/actor_323000_animation_09280_indices.inc"
};

AnimationSet D_actor_323000_8016B0A0 = {
    D_actor_323000_8016AE4C,
    D_actor_323000_8016B07C,
    { NULL, D_actor_323000_8016AC94, NULL, NULL, D_actor_323000_8016AD00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016B0C8[19] = {
#include "assets/actor_323000_animation_09958_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016B1AC[138] = {
#include "assets/actor_323000_animation_09958_bank4.inc"
};

AnimationRecord D_actor_323000_8016B3D4[224] = {
#include "assets/actor_323000_animation_09958_records.inc"
};

u16 D_actor_323000_8016B754[18] = {
#include "assets/actor_323000_animation_09958_indices.inc"
};

AnimationSet D_actor_323000_8016B778 = {
    D_actor_323000_8016B3D4,
    D_actor_323000_8016B754,
    { NULL, D_actor_323000_8016B0C8, NULL, NULL, D_actor_323000_8016B1AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016B7A0[14] = {
#include "assets/actor_323000_animation_09F58_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016B848[99] = {
#include "assets/actor_323000_animation_09F58_bank4.inc"
};

AnimationRecord D_actor_323000_8016B9D4[224] = {
#include "assets/actor_323000_animation_09F58_records.inc"
};

u16 D_actor_323000_8016BD54[18] = {
#include "assets/actor_323000_animation_09F58_indices.inc"
};

AnimationSet D_actor_323000_8016BD78 = {
    D_actor_323000_8016B9D4,
    D_actor_323000_8016BD54,
    { NULL, D_actor_323000_8016B7A0, NULL, NULL, D_actor_323000_8016B848, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016BDA0[4] = {
#include "assets/actor_323000_animation_0A1B4_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016BDD0[34] = {
#include "assets/actor_323000_animation_0A1B4_bank4.inc"
};

AnimationRecord D_actor_323000_8016BE58[86] = {
#include "assets/actor_323000_animation_0A1B4_records.inc"
};

u16 D_actor_323000_8016BFB0[18] = {
#include "assets/actor_323000_animation_0A1B4_indices.inc"
};

AnimationSet D_actor_323000_8016BFD4 = {
    D_actor_323000_8016BE58,
    D_actor_323000_8016BFB0,
    { NULL, D_actor_323000_8016BDA0, NULL, NULL, D_actor_323000_8016BDD0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016BFFC[12] = {
#include "assets/actor_323000_animation_0A7D8_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016C08C[147] = {
#include "assets/actor_323000_animation_0A7D8_bank4.inc"
};

AnimationRecord D_actor_323000_8016C2D8[191] = {
#include "assets/actor_323000_animation_0A7D8_records.inc"
};

u16 D_actor_323000_8016C5D4[18] = {
#include "assets/actor_323000_animation_0A7D8_indices.inc"
};

AnimationSet D_actor_323000_8016C5F8 = {
    D_actor_323000_8016C2D8,
    D_actor_323000_8016C5D4,
    { NULL, D_actor_323000_8016BFFC, NULL, NULL, D_actor_323000_8016C08C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016C620[9] = {
#include "assets/actor_323000_animation_0AC28_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016C68C[73] = {
#include "assets/actor_323000_animation_0AC28_bank4.inc"
};

AnimationRecord D_actor_323000_8016C7B0[157] = {
#include "assets/actor_323000_animation_0AC28_records.inc"
};

u16 D_actor_323000_8016CA24[18] = {
#include "assets/actor_323000_animation_0AC28_indices.inc"
};

AnimationSet D_actor_323000_8016CA48 = {
    D_actor_323000_8016C7B0,
    D_actor_323000_8016CA24,
    { NULL, D_actor_323000_8016C620, NULL, NULL, D_actor_323000_8016C68C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016CA70[12] = {
#include "assets/actor_323000_animation_0B20C_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016CB00[126] = {
#include "assets/actor_323000_animation_0B20C_bank4.inc"
};

AnimationRecord D_actor_323000_8016CCF8[196] = {
#include "assets/actor_323000_animation_0B20C_records.inc"
};

u16 D_actor_323000_8016D008[18] = {
#include "assets/actor_323000_animation_0B20C_indices.inc"
};

AnimationSet D_actor_323000_8016D02C = {
    D_actor_323000_8016CCF8,
    D_actor_323000_8016D008,
    { NULL, D_actor_323000_8016CA70, NULL, NULL, D_actor_323000_8016CB00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016D054[2] = {
#include "assets/actor_323000_animation_0B388_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016D06C[16] = {
#include "assets/actor_323000_animation_0B388_bank4.inc"
};

AnimationRecord D_actor_323000_8016D0AC[54] = {
#include "assets/actor_323000_animation_0B388_records.inc"
};

u16 D_actor_323000_8016D184[18] = {
#include "assets/actor_323000_animation_0B388_indices.inc"
};

AnimationSet D_actor_323000_8016D1A8 = {
    D_actor_323000_8016D0AC,
    D_actor_323000_8016D184,
    { NULL, D_actor_323000_8016D054, NULL, NULL, D_actor_323000_8016D06C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016D1D0[13] = {
#include "assets/actor_323000_animation_0BA2C_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016D26C[158] = {
#include "assets/actor_323000_animation_0BA2C_bank4.inc"
};

AnimationRecord D_actor_323000_8016D4E4[209] = {
#include "assets/actor_323000_animation_0BA2C_records.inc"
};

u16 D_actor_323000_8016D828[18] = {
#include "assets/actor_323000_animation_0BA2C_indices.inc"
};

AnimationSet D_actor_323000_8016D84C = {
    D_actor_323000_8016D4E4,
    D_actor_323000_8016D828,
    { NULL, D_actor_323000_8016D1D0, NULL, NULL, D_actor_323000_8016D26C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016D874[21] = {
#include "assets/actor_323000_animation_0C274_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016D970[193] = {
#include "assets/actor_323000_animation_0C274_bank4.inc"
};

AnimationRecord D_actor_323000_8016DC74[254] = {
#include "assets/actor_323000_animation_0C274_records.inc"
};

u16 D_actor_323000_8016E06C[20] = {
#include "assets/actor_323000_animation_0C274_indices.inc"
};

AnimationSet D_actor_323000_8016E094 = {
    D_actor_323000_8016DC74,
    D_actor_323000_8016E06C,
    { NULL, D_actor_323000_8016D874, NULL, NULL, D_actor_323000_8016D970, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016E0BC[20] = {
#include "assets/actor_323000_animation_0CA10_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016E1AC[172] = {
#include "assets/actor_323000_animation_0CA10_bank4.inc"
};

AnimationRecord D_actor_323000_8016E45C[235] = {
#include "assets/actor_323000_animation_0CA10_records.inc"
};

u16 D_actor_323000_8016E808[20] = {
#include "assets/actor_323000_animation_0CA10_indices.inc"
};

AnimationSet D_actor_323000_8016E830 = {
    D_actor_323000_8016E45C,
    D_actor_323000_8016E808,
    { NULL, D_actor_323000_8016E0BC, NULL, NULL, D_actor_323000_8016E1AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016E858[15] = {
#include "assets/actor_323000_animation_0D21C_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016E90C[206] = {
#include "assets/actor_323000_animation_0D21C_bank4.inc"
};

AnimationRecord D_actor_323000_8016EC44[244] = {
#include "assets/actor_323000_animation_0D21C_records.inc"
};

u16 D_actor_323000_8016F014[20] = {
#include "assets/actor_323000_animation_0D21C_indices.inc"
};

AnimationSet D_actor_323000_8016F03C = {
    D_actor_323000_8016EC44,
    D_actor_323000_8016F014,
    { NULL, D_actor_323000_8016E858, NULL, NULL, D_actor_323000_8016E90C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016F064[14] = {
#include "assets/actor_323000_animation_0DA30_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016F10C[207] = {
#include "assets/actor_323000_animation_0DA30_bank4.inc"
};

AnimationRecord D_actor_323000_8016F448[248] = {
#include "assets/actor_323000_animation_0DA30_records.inc"
};

u16 D_actor_323000_8016F828[20] = {
#include "assets/actor_323000_animation_0DA30_indices.inc"
};

AnimationSet D_actor_323000_8016F850 = {
    D_actor_323000_8016F448,
    D_actor_323000_8016F828,
    { NULL, D_actor_323000_8016F064, NULL, NULL, D_actor_323000_8016F10C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016F878[5] = {
#include "assets/actor_323000_animation_0DCE4_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016F8B4[55] = {
#include "assets/actor_323000_animation_0DCE4_bank4.inc"
};

AnimationRecord D_actor_323000_8016F990[83] = {
#include "assets/actor_323000_animation_0DCE4_records.inc"
};

u16 D_actor_323000_8016FADC[20] = {
#include "assets/actor_323000_animation_0DCE4_indices.inc"
};

AnimationSet D_actor_323000_8016FB04 = {
    D_actor_323000_8016F990,
    D_actor_323000_8016FADC,
    { NULL, D_actor_323000_8016F878, NULL, NULL, D_actor_323000_8016F8B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016FB2C[6] = {
#include "assets/actor_323000_animation_0E004_bank1.inc"
};

AnimationPackedRotation D_actor_323000_8016FB74[66] = {
#include "assets/actor_323000_animation_0E004_bank4.inc"
};

AnimationRecord D_actor_323000_8016FC7C[96] = {
#include "assets/actor_323000_animation_0E004_records.inc"
};

u16 D_actor_323000_8016FDFC[20] = {
#include "assets/actor_323000_animation_0E004_indices.inc"
};

AnimationSet D_actor_323000_8016FE24 = {
    D_actor_323000_8016FC7C,
    D_actor_323000_8016FDFC,
    { NULL, D_actor_323000_8016FB2C, NULL, NULL, D_actor_323000_8016FB74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_8016FE4C[38] = {
#include "assets/actor_323000_animation_0EF14_bank1.inc"
};

AnimationPackedRotation D_actor_323000_80170014[347] = {
#include "assets/actor_323000_animation_0EF14_bank4.inc"
};

AnimationRecord D_actor_323000_80170580[484] = {
#include "assets/actor_323000_animation_0EF14_records.inc"
};

u16 D_actor_323000_80170D10[18] = {
#include "assets/actor_323000_animation_0EF14_indices.inc"
};

AnimationSet D_actor_323000_80170D34 = {
    D_actor_323000_80170580,
    D_actor_323000_80170D10,
    { NULL, D_actor_323000_8016FE4C, NULL, NULL, D_actor_323000_80170014, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_323000_80170D5C[99] = {
#include "assets/actor_323000_animation_11248_bank1.inc"
};

AnimationPackedRotation D_actor_323000_80171200[767] = {
#include "assets/actor_323000_animation_11248_bank4.inc"
};

AnimationRecord D_actor_323000_80171DFC[1170] = {
#include "assets/actor_323000_animation_11248_records.inc"
};

u16 D_actor_323000_80173044[18] = {
#include "assets/actor_323000_animation_11248_indices.inc"
};

AnimationSet D_actor_323000_80173068 = {
    D_actor_323000_80171DFC,
    D_actor_323000_80173044,
    { NULL, D_actor_323000_80170D5C, NULL, NULL, D_actor_323000_80171200, NULL, NULL, NULL },
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
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    164,
    157,
    22,
    128,
    96,
    160,
    22,
    128,
    148,
    166,
    22,
    128,
    108,
    172,
    22,
    128,
    160,
    176,
    22,
    128,
    120,
    183,
    22,
    128,
    120,
    189,
    22,
    128,
    212,
    191,
    22,
    128,
    248,
    197,
    22,
    128,
    72,
    202,
    22,
    128,
    44,
    208,
    22,
    128,
    168,
    209,
    22,
    128,
    76,
    216,
    22,
    128,
    52,
    13,
    23,
    128,
    104,
    48,
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
    148,
    224,
    22,
    128,
    60,
    240,
    22,
    128,
    4,
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
    48,
    232,
    22,
    128,
    80,
    248,
    22,
    128,
    36,
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

Actor323000MessageEntry gRigMessages[7] = {
    { 2015, { .call5 = func_actor_323000_8016483C } },
    { 2005, { .call4 = rigSetVisibility } },
    { 2006, { .call0 = func_actor_323000_80164904 } },
    { 2004, { .call3 = actorMsgPlaceYawFirst } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_323000_80164A54 } },
    { 2003, { .call1 = func_actor_323000_80164AF0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_323000_80173A08 = { (TASK_BODY_TMD | 0x100), 96, func_actor_323000_80164CE4, { .model = &D_actor_323000_80169870 } };

static Actor323000Storage3A14 ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &(ActorContact_ScratchPosition.value);
}

Actor323000Storage3A24 gRigEffectRec = { { 0 }, { 0 } };

static void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/blend_rig_creature_blend_tick.inc.c"

/// Effect and sound step of the tick: for the clip in `field_82E`, watches
/// the clip each relevant slot plays, and the first frame one reaches a
/// watched value spawns effect 0x60054 at the matching coordinate and returns
/// the `SndEvt_EnqueueType6` id to play (0 where only effects fire).
/// `field_848` remembers each slot's last clip so the step fires once; it is
/// cleared when none of the watched clips is playing.
s32 rigAnimCues(Task* task, Actor323000Work* work)
{
    SVECTOR vec;
    s32     reset;

    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;

            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vx             = -500;
                    vec.vz             = 200;
                    vec.vy             = 650;
                    Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002220, &vec);
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
                        vec.vx             = -1000;
                        vec.vz             = 200;
                        vec.vy             = 650;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002220, &vec);
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
                        vec.vy              = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80002220, &vec);
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
                        vec.vy              = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80002220, &vec);
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
                    Gp_SpawnEff(0x60054, task->extra.tmd->coords, 0x80004A00, &vec);
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
                    vec.vx             = -500;
                    vec.vz             = 200;
                    vec.vy             = 650;
                    Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80003200, &vec);
                    vec.vx = -1000;
                    vec.vz = 200;
                    vec.vy = 650;
                    Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80003200, &vec);
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
                        vec.vy             = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80004480, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80004480, &vec);
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
                        vec.vx             = -500;
                        vec.vz             = 200;
                        vec.vy             = 650;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80002200, &vec);
                        vec.vx = -1000;
                        vec.vz = 200;
                        vec.vy = 650;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002240, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80003300, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80003340, &vec);
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
                        vec.vy             = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80002200, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80002300, &vec);
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

#include "../../shared/blend_rig_creature_anim_tick.inc.c"

#include "../../shared/blend_rig_creature_spawn.inc.c"

/// State 1: on entry clears the enemy's link flag and the model's flags,
/// rebuilds its buffers and asks the tick to reset the slots. Each frame it
/// ticks; when slot 1 sets flag bit 0 during clip 0xF it moves on to clip
/// 0x10, and during clip 0xE it spawns effect 0x60054 at coordinate 7 with
/// spawn argument 0x80002300 while slot 1 plays clip 7 or 9, 0x80003400 for 8.
static void func_actor_323000_8016409C(Enemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    SVECTOR          sp10;

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
        rigAnimTick(task);
        return;
    }
    rigAnimTick(task);
    if (work->slots[1].flags & ANIMATION_SLOT_REACHED_END) {
        if (work->field_82E == 0xF) {
            work->field_828 = 2;
            work->field_82E = 0x10;
        }
        rigAnimTick(task);
    }
    if (work->field_82E == 0xE) {
        if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 7 || (work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 9) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80002300, &sp10);
        }
        if ((work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 8) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80003400, &sp10);
        }
    }
}

/// State 3: on entry flags the enemy's link node, clears the model's flags,
/// rebuilds its buffers and starts clip 0xE with the frame counter at 0. Each
/// frame it ticks and, on frames 29, 32 and 33, spawns effect 0x60054 at the
/// limb coordinates; frame 32 also plays a sound chosen by the enemy's
/// `placeKey`.
static void func_actor_323000_8016420C(Enemy* enemy, Task* task)
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
        work->field_82E = 0xE;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        rigAnimTick(task);
        return;
    }
    rigAnimTick(task);
    switch (++work->field_6) {
        case 29: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80005600, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80005A00, p);
        } break;
        case 32: {
            SVECTOR* p = &ofs;
            p->vx      = -0x3E8;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80005A80, p);
            p->vx = -0x1F4;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80006800, p);
            id  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000D;
            pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));
            ofs2.vy = -0x258;
            ofs2.vx = 0;
            ofs2.vz = -0x384;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[1], 0x80005A00, &ofs2);
        } break;
        case 33: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[9], 0x80006800, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80006B00, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[7], 0x80004400, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[14], 0x80003800, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[17], 0x80004900, p);
            ofs2.vy = -0x2BC;
            ofs2.vx = 0;
            ofs2.vz = -0x258;
            Gp_SpawnEff(0x60054, &task->extra.tmd->coords[1], 0x80005A00, &ofs2);
        } break;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `field_4`, and runs the state handler `field_0` selects from a stack copy
/// of `D_actor_323000_80161E24`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
static void func_actor_323000_801645A4(Enemy* enemy, Task* task)
{
    Actor323000Work*        work;
    GpEnemyTaskFuncTable4   sp;
    Actor323000TickScratch* scratch;
    u8*                     head;
    GfxCoord*               walker;
    SVECTOR*                pos;

    work = (Actor323000Work*)task->work;
    gameGetPtrSlot(3);
    sp                                    = D_actor_323000_80161E24;
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

/// Handler for message 0x7DF: does nothing.
void func_actor_323000_8016483C(void)
{
}

#include "../../shared/blend_rig_creature_visibility.inc.c"

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points,
/// and otherwise 1 only when the model has neither flag 0x80 nor flag 2 set.
s32 func_actor_323000_80164904(Task* task)
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
/// the work block and, when `code` is 0x202, selects the state from `mode`:
/// 1 starts state 2, 0 and 2 state 0, and 3 state 3. Other codes only store
/// the bytes.
s32 func_actor_323000_80164A54(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    Actor323000Work* work;

    work = (Actor323000Work*)task->work;

    work->field_91C = msg->context.loc.stage;
    work->field_91D = msg->context.loc.area;
    work->field_91E = (u8)msg->command;

    if (msg->context.key == 0x202) {
        switch (msg->command) {
            case 1:
                work->field_0 = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
            case 3:
                work->field_0 = msg->command;
                break;
        }
    }
    return 0;
}

/// Handler for message 0x7D3: latches the requested animation id into
/// `field_82E` and restarts the state machine at state 1.
s32 func_actor_323000_80164AF0(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor323000Work* work = (Actor323000Work*)task->work;

    work->field_82E = msg->animationId;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

/// `Task::exitCallback` the spawn handler installs: destroys the enemy the
/// task carries.
void rigExit(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Spawns effect 0x60054 at coordinate `arg1` with the offset that limb
/// uses; the spawn argument is `arg2` with bit 31 set. Coordinates the
/// switch does not list use whatever the offset holds. Nothing in this
/// package calls it.
static void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2)
{
    SVECTOR    sp10;
    TmdObject* obj;

    switch (arg1) {
        case 0:
        case 1:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            sp10.vx = -0x1F4;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 7:
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 14:
        case 17:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
    }

    obj = task->extra.tmd;
    Gp_SpawnEff(0x60054, &obj->coords[arg1], arg2 | 0x80000000, &sp10);
}

/// State 0 of `D_actor_323000_80161E24`: when the work block's `field_4` flag
/// is set, flags the enemy's link node and raises bit 0x80 of the model's
/// flags. `obj` gets its own local: the fused form ranks the `Task::extra`
/// load with the store and transposes it.
static void func_actor_323000_80164C20(Enemy* arg0, Task* arg1)
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

/// State 2: on entry flags the enemy's link node, clears the model's flags,
/// rebuilds its buffers and starts clip 0xD with the frame counter at 0; the
/// tick runs every frame.
static void func_actor_323000_80164C58(Enemy* enemy, Task* task)
{
    Actor323000Work* work;
    TmdObject*       obj;
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has

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
        rigAnimTick(task);
    } else {
        rigAnimTick(task);
    }
}

/// Task body of the actor's descriptor: runs the handler for the task's
/// state from a stack copy of `D_actor_323000_80161E34` - the spawn handler,
/// the per-frame driver, then `Gp_DestroyEnemy`.
void func_actor_323000_80164CE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_323000_80161E34;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
