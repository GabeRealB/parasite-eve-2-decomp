#include "actor_342400_private.h"

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
#include "../../shared/mad_chaser_waves.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

// the main enemy's `Enemy::param` record
extern u8 gMadChaserAnimBank[]; // animation bank handed to `func_800B3F84`
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s16, VECTOR3*);
        void (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} Actor3424002MessageEntry;
STATIC_ASSERT_SIZEOF(Actor3424002MessageEntry, 8);

extern Actor3424002MessageEntry gMadChaserMsgTable[3];   // stored into `Task::msgTable` by madChaserSpawn
extern u8                       gMadChaserAnimStance[];  // per animation id (1-based): value for `field_44F`
extern u8                       gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_342400_80168394(Task* arg0);
static void func_actor_342400_80169880(Task* arg0);
static void func_actor_342400_8016997C(Task* arg0);
static void func_actor_342400_80169990(Task* arg0);
static void func_actor_342400_80169A98(Task* arg0);
static void func_actor_342400_80169B58(Task* arg0);
static void func_actor_342400_80169BAC(Task* arg0);
static void func_actor_342400_8016AE24(Task* arg0);
static void func_actor_342400_8016AEAC(Task* arg0);
static void func_actor_342400_8016B038(Task* arg0);
static void func_actor_342400_8016BAF4(Task* arg0);
static void func_actor_342400_8016BBD0(Task* arg0);
static void func_actor_342400_8016BBD8(Task* arg0);
static void func_actor_342400_8016BD3C(Task* arg0);
static void func_actor_342400_8016BED8(Task* arg0);

/// Six task-state handlers of the first enemy form, dispatched by
/// `madChaserTask` on `Task::state`.
static const TaskFuncTable6 gMadChaserTaskStates = { {
    madChaserSpawn,
    madChaserLurkTick,
    madChaserDangleFrame,
    madChaserCombatTick,
    madChaserDeathTick,
    func_actor_342400_80169880,
} };

/// Ten task-state handlers of the second enemy form, dispatched by
/// `madChaserHiddenTask` on `Task::state`.
static const TaskFuncTable10 gMadChaserHiddenTaskStates = { {
    madChaserSpawnHidden,
    madChaserLurkTick,
    madChaserDangleFrame,
    madChaserCombatTick,
    madChaserDeathTick,
    func_actor_342400_80169880,
    madChaserEmergeTick,
    madChaserVanishState,
    madChaserDropDeathTick,
    madChaserShrinkDeathTick,
} };

/// Eleven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable11 gMadChaserCombatStates = { {
    madChaserToAlertState,
    func_actor_342400_8016997C,
    func_actor_342400_80169990,
    madChaserWalkState,
    madChaserLeapState,
    func_actor_342400_80169A98,
    madChaserRecoilLightState,
    func_actor_342400_80169B58,
    func_actor_342400_80169BAC,
    madChaserKnockdownState,
    madChaserPullState,
} };

/// Sub-state handlers `madChaserKnockdownState` dispatches by `field_422`.
static const TaskFuncTable3 gMadChaserKnockdownSteps = { {
    madChaserKnockdownStart,
    madChaserKnockdownRise,
    madChaserKnockdownEnd,
} };

/// Sub-state handlers `madChaserWalkState` dispatches by `field_422`.
static const TaskFuncTable3 gMadChaserWalkSteps = { {
    madChaserWalkStart,
    madChaserWalkApproach,
    madChaserWalkFinish,
} };

/// Sub-state handlers `madChaserLeapState` dispatches by `field_422`.
static const TaskFuncTable5 gMadChaserLeapSteps = { {
    madChaserStartLeap,
    madChaserLeapAttack,
    madChaserLeapTurnAway,
    madChaserLeapRebound,
    madChaserLeapLand,
} };

/// Sub-state handlers `func_actor_342400_80169A98` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_342400_80161F00 = { {
    madChaserAlertCry,
    madChaserAlertWait,
    madChaserAlertRelease,
    madChaserAlertCrouch,
    madChaserAlertSidestep,
} };

/// Sub-state handlers `madChaserDangleState` dispatches by `field_422`.
static const TaskFuncTable4 gMadChaserDangleSteps = { {
    madChaserDangleStart,
    madChaserDangleSway,
    madChaserDangleFall,
    madChaserDangleLand,
} };

AnimationPackedPose D_actor_342400_80170598[6] = {
#include "assets/actor_342400_animation_0E9A4_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801705E0[39] = {
#include "assets/actor_342400_animation_0E9A4_bank4.inc"
};

AnimationRecord D_actor_342400_8017067C[77] = {
#include "assets/actor_342400_animation_0E9A4_records.inc"
};

u16 D_actor_342400_801707B0[10] = {
#include "assets/actor_342400_animation_0E9A4_indices.inc"
};

AnimationSet D_actor_342400_801707C4 = {
    D_actor_342400_8017067C,
    D_actor_342400_801707B0,
    { NULL, D_actor_342400_80170598, NULL, NULL, D_actor_342400_801705E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801707EC[3] = {
#include "assets/actor_342400_animation_0EB48_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80170810[21] = {
#include "assets/actor_342400_animation_0EB48_bank4.inc"
};

AnimationRecord D_actor_342400_80170864[60] = {
#include "assets/actor_342400_animation_0EB48_records.inc"
};

u16 D_actor_342400_80170954[10] = {
#include "assets/actor_342400_animation_0EB48_indices.inc"
};

AnimationSet D_actor_342400_80170968 = {
    D_actor_342400_80170864,
    D_actor_342400_80170954,
    { NULL, D_actor_342400_801707EC, NULL, NULL, D_actor_342400_80170810, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80170990[20] = {
#include "assets/actor_342400_animation_0F01C_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80170A80[99] = {
#include "assets/actor_342400_animation_0F01C_bank4.inc"
};

AnimationRecord D_actor_342400_80170C0C[135] = {
#include "assets/actor_342400_animation_0F01C_records.inc"
};

u16 D_actor_342400_80170E28[10] = {
#include "assets/actor_342400_animation_0F01C_indices.inc"
};

AnimationSet D_actor_342400_80170E3C = {
    D_actor_342400_80170C0C,
    D_actor_342400_80170E28,
    { NULL, D_actor_342400_80170990, NULL, NULL, D_actor_342400_80170A80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80170E64[25] = {
#include "assets/actor_342400_animation_0F5A0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80170F90[110] = {
#include "assets/actor_342400_animation_0F5A0_bank4.inc"
};

AnimationRecord D_actor_342400_80171148[153] = {
#include "assets/actor_342400_animation_0F5A0_records.inc"
};

u16 D_actor_342400_801713AC[10] = {
#include "assets/actor_342400_animation_0F5A0_indices.inc"
};

AnimationSet D_actor_342400_801713C0 = {
    D_actor_342400_80171148,
    D_actor_342400_801713AC,
    { NULL, D_actor_342400_80170E64, NULL, NULL, D_actor_342400_80170F90, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801713E8[2] = {
#include "assets/actor_342400_animation_0F6A0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171400[7] = {
#include "assets/actor_342400_animation_0F6A0_bank4.inc"
};

AnimationRecord D_actor_342400_8017141C[36] = {
#include "assets/actor_342400_animation_0F6A0_records.inc"
};

u16 D_actor_342400_801714AC[10] = {
#include "assets/actor_342400_animation_0F6A0_indices.inc"
};

AnimationSet D_actor_342400_801714C0 = {
    D_actor_342400_8017141C,
    D_actor_342400_801714AC,
    { NULL, D_actor_342400_801713E8, NULL, NULL, D_actor_342400_80171400, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801714E8[2] = {
#include "assets/actor_342400_animation_0F7A0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171500[7] = {
#include "assets/actor_342400_animation_0F7A0_bank4.inc"
};

AnimationRecord D_actor_342400_8017151C[36] = {
#include "assets/actor_342400_animation_0F7A0_records.inc"
};

u16 D_actor_342400_801715AC[10] = {
#include "assets/actor_342400_animation_0F7A0_indices.inc"
};

AnimationSet D_actor_342400_801715C0 = {
    D_actor_342400_8017151C,
    D_actor_342400_801715AC,
    { NULL, D_actor_342400_801714E8, NULL, NULL, D_actor_342400_80171500, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801715E8[24] = {
#include "assets/actor_342400_animation_0FBD0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171708[69] = {
#include "assets/actor_342400_animation_0FBD0_bank4.inc"
};

AnimationRecord D_actor_342400_8017181C[112] = {
#include "assets/actor_342400_animation_0FBD0_records.inc"
};

u16 D_actor_342400_801719DC[10] = {
#include "assets/actor_342400_animation_0FBD0_indices.inc"
};

AnimationSet D_actor_342400_801719F0 = {
    D_actor_342400_8017181C,
    D_actor_342400_801719DC,
    { NULL, D_actor_342400_801715E8, NULL, NULL, D_actor_342400_80171708, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80171A18[22] = {
#include "assets/actor_342400_animation_10074_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171B20[87] = {
#include "assets/actor_342400_animation_10074_bank4.inc"
};

AnimationRecord D_actor_342400_80171C7C[129] = {
#include "assets/actor_342400_animation_10074_records.inc"
};

u16 D_actor_342400_80171E80[10] = {
#include "assets/actor_342400_animation_10074_indices.inc"
};

AnimationSet D_actor_342400_80171E94 = {
    D_actor_342400_80171C7C,
    D_actor_342400_80171E80,
    { NULL, D_actor_342400_80171A18, NULL, NULL, D_actor_342400_80171B20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80171EBC[14] = {
#include "assets/actor_342400_animation_105CC_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80171F64[99] = {
#include "assets/actor_342400_animation_105CC_bank4.inc"
};

AnimationRecord D_actor_342400_801720F0[186] = {
#include "assets/actor_342400_animation_105CC_records.inc"
};

u16 D_actor_342400_801723D8[10] = {
#include "assets/actor_342400_animation_105CC_indices.inc"
};

AnimationSet D_actor_342400_801723EC = {
    D_actor_342400_801720F0,
    D_actor_342400_801723D8,
    { NULL, D_actor_342400_80171EBC, NULL, NULL, D_actor_342400_80171F64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172414[8] = {
#include "assets/actor_342400_animation_10880_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172474[54] = {
#include "assets/actor_342400_animation_10880_bank4.inc"
};

AnimationRecord D_actor_342400_8017254C[80] = {
#include "assets/actor_342400_animation_10880_records.inc"
};

u16 D_actor_342400_8017268C[10] = {
#include "assets/actor_342400_animation_10880_indices.inc"
};

AnimationSet D_actor_342400_801726A0 = {
    D_actor_342400_8017254C,
    D_actor_342400_8017268C,
    { NULL, D_actor_342400_80172414, NULL, NULL, D_actor_342400_80172474, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801726C8[7] = {
#include "assets/actor_342400_animation_10B14_bank1.inc"
};

AnimationPackedRotation D_actor_342400_8017271C[50] = {
#include "assets/actor_342400_animation_10B14_bank4.inc"
};

AnimationRecord D_actor_342400_801727E4[79] = {
#include "assets/actor_342400_animation_10B14_records.inc"
};

u16 D_actor_342400_80172920[10] = {
#include "assets/actor_342400_animation_10B14_indices.inc"
};

AnimationSet D_actor_342400_80172934 = {
    D_actor_342400_801727E4,
    D_actor_342400_80172920,
    { NULL, D_actor_342400_801726C8, NULL, NULL, D_actor_342400_8017271C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_8017295C[7] = {
#include "assets/actor_342400_animation_10DC4_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801729B0[53] = {
#include "assets/actor_342400_animation_10DC4_bank4.inc"
};

AnimationRecord D_actor_342400_80172A84[83] = {
#include "assets/actor_342400_animation_10DC4_records.inc"
};

u16 D_actor_342400_80172BD0[10] = {
#include "assets/actor_342400_animation_10DC4_indices.inc"
};

AnimationSet D_actor_342400_80172BE4 = {
    D_actor_342400_80172A84,
    D_actor_342400_80172BD0,
    { NULL, D_actor_342400_8017295C, NULL, NULL, D_actor_342400_801729B0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172C0C[6] = {
#include "assets/actor_342400_animation_10FD8_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172C54[42] = {
#include "assets/actor_342400_animation_10FD8_bank4.inc"
};

AnimationRecord D_actor_342400_80172CFC[58] = {
#include "assets/actor_342400_animation_10FD8_records.inc"
};

u16 D_actor_342400_80172DE4[10] = {
#include "assets/actor_342400_animation_10FD8_indices.inc"
};

AnimationSet D_actor_342400_80172DF8 = {
    D_actor_342400_80172CFC,
    D_actor_342400_80172DE4,
    { NULL, D_actor_342400_80172C0C, NULL, NULL, D_actor_342400_80172C54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172E20[2] = {
#include "assets/actor_342400_animation_110F8_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172E38[11] = {
#include "assets/actor_342400_animation_110F8_bank4.inc"
};

AnimationRecord D_actor_342400_80172E64[40] = {
#include "assets/actor_342400_animation_110F8_records.inc"
};

u16 D_actor_342400_80172F04[10] = {
#include "assets/actor_342400_animation_110F8_indices.inc"
};

AnimationSet D_actor_342400_80172F18 = {
    D_actor_342400_80172E64,
    D_actor_342400_80172F04,
    { NULL, D_actor_342400_80172E20, NULL, NULL, D_actor_342400_80172E38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80172F40[4] = {
#include "assets/actor_342400_animation_11234_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80172F70[18] = {
#include "assets/actor_342400_animation_11234_bank4.inc"
};

AnimationRecord D_actor_342400_80172FB8[34] = {
#include "assets/actor_342400_animation_11234_records.inc"
};

u16 D_actor_342400_80173040[10] = {
#include "assets/actor_342400_animation_11234_indices.inc"
};

AnimationSet D_actor_342400_80173054 = {
    D_actor_342400_80172FB8,
    D_actor_342400_80173040,
    { NULL, D_actor_342400_80172F40, NULL, NULL, D_actor_342400_80172F70, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_8017307C[10] = {
#include "assets/actor_342400_animation_11430_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801730F4[31] = {
#include "assets/actor_342400_animation_11430_bank4.inc"
};

AnimationRecord D_actor_342400_80173170[51] = {
#include "assets/actor_342400_animation_11430_records.inc"
};

u16 D_actor_342400_8017323C[10] = {
#include "assets/actor_342400_animation_11430_indices.inc"
};

AnimationSet D_actor_342400_80173250 = {
    D_actor_342400_80173170,
    D_actor_342400_8017323C,
    { NULL, D_actor_342400_8017307C, NULL, NULL, D_actor_342400_801730F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80173278[16] = {
#include "assets/actor_342400_animation_1186C_bank1.inc"
};

AnimationPackedRotation D_actor_342400_80173338[83] = {
#include "assets/actor_342400_animation_1186C_bank4.inc"
};

AnimationRecord D_actor_342400_80173484[125] = {
#include "assets/actor_342400_animation_1186C_records.inc"
};

u16 D_actor_342400_80173678[10] = {
#include "assets/actor_342400_animation_1186C_indices.inc"
};

AnimationSet D_actor_342400_8017368C = {
    D_actor_342400_80173484,
    D_actor_342400_80173678,
    { NULL, D_actor_342400_80173278, NULL, NULL, D_actor_342400_80173338, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_801736B4[5] = {
#include "assets/actor_342400_animation_119F8_bank1.inc"
};

AnimationPackedRotation D_actor_342400_801736F0[26] = {
#include "assets/actor_342400_animation_119F8_bank4.inc"
};

AnimationRecord D_actor_342400_80173758[43] = {
#include "assets/actor_342400_animation_119F8_records.inc"
};

u16 D_actor_342400_80173804[10] = {
#include "assets/actor_342400_animation_119F8_indices.inc"
};

AnimationSet D_actor_342400_80173818 = {
    D_actor_342400_80173758,
    D_actor_342400_80173804,
    { NULL, D_actor_342400_801736B4, NULL, NULL, D_actor_342400_801736F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342400_80173840[5] = {
#include "assets/actor_342400_animation_11BA0_bank1.inc"
};

AnimationPackedRotation D_actor_342400_8017387C[30] = {
#include "assets/actor_342400_animation_11BA0_bank4.inc"
};

AnimationRecord D_actor_342400_801738F4[46] = {
#include "assets/actor_342400_animation_11BA0_records.inc"
};

u16 D_actor_342400_801739AC[10] = {
#include "assets/actor_342400_animation_11BA0_indices.inc"
};

AnimationSet D_actor_342400_801739C0 = {
    D_actor_342400_801738F4,
    D_actor_342400_801739AC,
    { NULL, D_actor_342400_80173840, NULL, NULL, D_actor_342400_8017387C, NULL, NULL, NULL },
};

u8 gMadChaserAnimBank[84] = {
    0,
    0,
    0,
    0,
    196,
    7,
    23,
    128,
    104,
    9,
    23,
    128,
    60,
    14,
    23,
    128,
    192,
    19,
    23,
    128,
    192,
    20,
    23,
    128,
    192,
    21,
    23,
    128,
    240,
    25,
    23,
    128,
    148,
    30,
    23,
    128,
    236,
    35,
    23,
    128,
    160,
    38,
    23,
    128,
    52,
    41,
    23,
    128,
    228,
    43,
    23,
    128,
    248,
    45,
    23,
    128,
    24,
    47,
    23,
    128,
    84,
    48,
    23,
    128,
    80,
    50,
    23,
    128,
    140,
    54,
    23,
    128,
    24,
    56,
    23,
    128,
    192,
    57,
    23,
    128,
    0,
    0,
    0,
    0,
};

Actor3424002MessageEntry gMadChaserMsgTable[3] = {
    { 2004, { .call0 = madChaserMsgPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = madChaserCommandMsg } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_342400_80173A54[2] = {
    { { { TASK_BODY_TMD, 96 } }, madChaserTask, { .model = &D_actor_342400_80170560 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, madChaserHiddenTask, { .model = &D_actor_342400_80170560 } },
};

TaskDesc D_actor_342400_80173A6C = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_342400_80173A78 = { { { TASK_BODY_TMD, 96 } }, madChaserHiddenTask, { .model = &D_actor_342400_80170560 } };

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

u8 gMadChaserSettleAnims[20] = {
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
    1,
};

u16 gMadChaserWaveEnemyCount = 0;

extern void* D_800678F0[1];

static __inline__ s16  take_hit(Task* arg0);
static __inline__ void update_rotation(Task* arg0);
static __inline__ s16  take_hit_nibble3(Task* arg0);
static __inline__ void set_state_s16(Task* arg0, s16 state);

#include "../../shared/mad_chaser_limb_shadow.inc.c"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. It is declared as a one-element array for the
 * same reason as in `actor_400500`: as a bare scalar, GCC 2.8.1 decides the
 * store cannot alias the `TmdObject` loads and sinks it past them. */

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

#include "../../shared/mad_chaser_combat_tick.inc.c"

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
static const TaskFuncTable9 gMadChaserDeathStates = { {
    madChaserDeathCry,
    madChaserDeathSettle,
    madChaserDeathWaitAnim,
    madChaserBeginDeath,
    madChaserDeathTurnTranslucent,
    madChaserShrinkWithDust,
    madChaserStartDespawn,
    madChaserDeathPause,
    madChaserBurst,
} };

#include "../../shared/mad_chaser_death_tick.inc.c"

#include "../../shared/mad_chaser_shrink_dust.inc.c"

#include "../../shared/mad_chaser_track_player.inc.c"

#include "../../shared/mad_chaser_recoil_recover.inc.c"

/// The five state handlers of the second enemy form, indexed by
/// `Actor341700Work::field_420`; copied to the stack before dispatch. It sits
/// between `madChaserRecoilRecover`'s jump table and this function's own.
static const TaskFuncTable5 gMadChaserLurkStates = { {
    func_actor_342400_8016AE24,
    func_actor_342400_8016AEAC,
    madChaserLurkRiseState,
    madChaserLurkAlertState,
    func_actor_342400_8016B038,
} };

#include "../../shared/mad_chaser_lurk_tick.inc.c"

/// Sub-state handlers `func_actor_342400_8016AE24` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FB4 = { {
    madChaserStartHold,
    madChaserLurkWait,
    madChaserLurkIdleEnd,
} };

/// Sub-state handlers `func_actor_342400_8016AEAC` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FC0 = { {
    madChaserLurkCrouch,
    madChaserLurkRaise,
    madChaserLurkLookAround,
} };

/// Sub-state handlers `madChaserLurkRiseState` dispatches by `field_422`.
static const TaskFuncTable3 gMadChaserLurkAlertSteps = { {
    madChaserStartAlert,
    madChaserLurkBrace,
    madChaserLurkSidestepToCombat,
} };

/// Sub-state handlers `func_actor_342400_8016B038` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_342400_80161FD8 = { {
    madChaserLurkShiftStart,
    madChaserLurkShiftBrace,
    madChaserLurkSidestepRight,
    madChaserLurkSidestepLeft,
} };

/// Ten state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable10 gMadChaserEmergeStates = { {
    madChaserEmergeAtSpot,
    madChaserEmergeBackflip,
    madChaserEmergeHopForward,
    madChaserCreepUntilHit,
    madChaserEmergeArcBack,
    madChaserEmergeHopBack,
    madChaserEmergeBackOff,
    madChaserEmergeHighArc,
    madChaserEmergeFlipOver,
    func_actor_342400_80168394,
} };

/// Sub-state handlers `madChaserPullState` dispatches by `field_422`.
static const TaskFuncTable6 gMadChaserPullSteps = { {
    madChaserPullStart,
    madChaserPullReact,
    madChaserPulledStruggle,
    madChaserPulledIn,
    madChaserPulledLimp,
    madChaserPulledIn,
} };

/// Five state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable5 gMadChaserDropDeathStates = { {
    madChaserDeathCryUnlink,
    madChaserDeathSettleQuiet,
    func_actor_342400_8016BAF4,
    madChaserDropBodies,
    func_actor_342400_8016BBD0,
} };

/// Seven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable7 gMadChaserShrinkDeathStates = { {
    func_actor_342400_8016BBD8,
    madChaserDeathSettleQuiet,
    func_actor_342400_8016BAF4,
    madChaserBeginShrink,
    func_actor_342400_8016BD3C,
    madChaserShrink,
    func_actor_342400_8016BED8,
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

#include "../../shared/mad_chaser_emerge_tick.inc.c"

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
#define madChaserCreepUntilHit func_actor_342400_80168394
#include "../../shared/mad_chaser_creep.inc.c"
#undef madChaserCreepUntilHit

#include "../../shared/mad_chaser_pulled_struggle.inc.c"

#include "../../shared/mad_chaser_pulled_in.inc.c"

#include "../../shared/mad_chaser_pulled_limp.inc.c"

#include "../../shared/mad_chaser_drop_death_tick.inc.c"

#include "../../shared/mad_chaser_shrink_death_tick.inc.c"

#include "../../shared/mad_chaser_sound_bank.inc.c"

#include "../../shared/mad_chaser_vanish_state.inc.c"

#include "../../shared/mad_chaser_join_alert.inc.c"

#include "../../shared/mad_chaser_alert_hold.inc.c"

#include "../../shared/mad_chaser_take_request.inc.c"

#include "../../shared/mad_chaser_command_msg.inc.c"

#include "../../shared/mad_chaser_msg_place.inc.c"

#include "../../shared/mad_chaser_pin_part.inc.c"

#include "../../shared/mad_chaser_scale_by_speed.inc.c"

#include "../../shared/mad_chaser_anim_ended.inc.c"

#include "../../shared/mad_chaser_hidden_task.inc.c"

#include "../../shared/mad_chaser_task.inc.c"

/// A further copy, under this file's own name.
#define madChaserVanishState func_actor_342400_80169880
#define madChaserVanish      madChaserAdvanceState
#define madChaserVanishFree  madChaserDespawn
#include "../../shared/mad_chaser_vanish_state.inc.c"
#undef madChaserVanishState
#undef madChaserVanish
#undef madChaserVanishFree

#include "../../shared/mad_chaser_turn_to_player.inc.c"

#include "../../shared/mad_chaser_to_alert.inc.c"

/// A further copy, under this file's own name.
#define madChaserToAlertState func_actor_342400_8016997C
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef madChaserToAlertState

/// A further copy, under this file's own name.
#define madChaserToAlertState func_actor_342400_80169990
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef madChaserToAlertState

#include "../../shared/mad_chaser_walk_state.inc.c"

#include "../../shared/mad_chaser_leap_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserLeapState  func_actor_342400_80169A98
#define gMadChaserLeapSteps D_actor_342400_80161F00
#include "../../shared/mad_chaser_leap_state.inc.c"
#undef madChaserLeapState
#undef gMadChaserLeapSteps

#include "../../shared/mad_chaser_recoil_light_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserRecoilLightState func_actor_342400_80169B58
#define madChaserRecoilLight      madChaserRecoilHeavy
#define madChaserRecoilRecover    madChaserRecoilHeavyEnd
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilLightState
#undef madChaserRecoilLight
#undef madChaserRecoilRecover

/// A further copy, under this file's own name.
#define madChaserRecoilLightState func_actor_342400_80169BAC
#define madChaserRecoilLight      madChaserStatusHoldStart
#define madChaserRecoilRecover    madChaserStatusHold
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilLightState
#undef madChaserRecoilLight
#undef madChaserRecoilRecover

#include "../../shared/mad_chaser_knockdown_state.inc.c"

#include "../../shared/mad_chaser_pull_state.inc.c"

#include "../../shared/mad_chaser_status_hold_start.inc.c"

#include "../../shared/mad_chaser_status_hold.inc.c"

#include "../../shared/mad_chaser_knockdown_start.inc.c"

#include "../../shared/mad_chaser_knockdown_rise.inc.c"

#include "../../shared/mad_chaser_knockdown_end.inc.c"

#include "../../shared/mad_chaser_walk_finish.inc.c"

#include "../../shared/mad_chaser_start_leap.inc.c"

#include "../../shared/mad_chaser_leap_land.inc.c"

#include "../../shared/mad_chaser_alert_cry.inc.c"

#include "../../shared/mad_chaser_alert_wait.inc.c"

#include "../../shared/mad_chaser_alert_release.inc.c"

#include "../../shared/mad_chaser_alert_crouch.inc.c"

#include "../../shared/mad_chaser_alert_sidestep.inc.c"

#include "../../shared/mad_chaser_dangle_state.inc.c"

#include "../../shared/mad_chaser_dangle_start.inc.c"

#include "../../shared/mad_chaser_dangle_sway.inc.c"

#include "../../shared/mad_chaser_death_cry.inc.c"

#include "../../shared/mad_chaser_death_settle.inc.c"

#include "../../shared/mad_chaser_death_wait_anim.inc.c"

#include "../../shared/mad_chaser_begin_death.inc.c"

#include "../../shared/mad_chaser_death_translucent.inc.c"

#include "../../shared/mad_chaser_start_despawn.inc.c"

#include "../../shared/mad_chaser_death_pause.inc.c"

#include "../../shared/mad_chaser_burst.inc.c"

#include "../../shared/mad_chaser_advance_state.inc.c"

#include "../../shared/mad_chaser_despawn.inc.c"

#include "../../shared/mad_chaser_recoil_light.inc.c"

#include "../../shared/mad_chaser_recoil_heavy.inc.c"

#include "../../shared/mad_chaser_recoil_heavy_end.inc.c"

/// A further copy, under this file's own name.
#define madChaserWalkState      func_actor_342400_8016AE24
#define gMadChaserWalkSteps     D_actor_342400_80161FB4
#define madChaserTakeHitRequest madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef madChaserWalkState
#undef gMadChaserWalkSteps
#undef madChaserTakeHitRequest

/// A further copy, under this file's own name.
#define madChaserWalkState      func_actor_342400_8016AEAC
#define gMadChaserWalkSteps     D_actor_342400_80161FC0
#define madChaserTakeHitRequest madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef madChaserWalkState
#undef gMadChaserWalkSteps
#undef madChaserTakeHitRequest

#include "../../shared/mad_chaser_lurk_rise_state.inc.c"

#include "../../shared/mad_chaser_lurk_alert_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserDangleState  func_actor_342400_8016B038
#define gMadChaserDangleSteps D_actor_342400_80161FD8
#include "../../shared/mad_chaser_dangle_state.inc.c"
#undef madChaserDangleState
#undef gMadChaserDangleSteps

#include "../../shared/mad_chaser_start_hold.inc.c"

#include "../../shared/mad_chaser_lurk_wait.inc.c"

#include "../../shared/mad_chaser_lurk_idle_end.inc.c"

#include "../../shared/mad_chaser_lurk_crouch.inc.c"

#include "../../shared/mad_chaser_lurk_raise.inc.c"

#include "../../shared/mad_chaser_lurk_rise_start.inc.c"

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
#define madChaserDeathWaitAnim func_actor_342400_8016BAF4
#include "../../shared/mad_chaser_death_wait_anim.inc.c"
#undef madChaserDeathWaitAnim

#include "../../shared/mad_chaser_drop_bodies.inc.c"

/// Empty state handler.
static void func_actor_342400_8016BBD0(Task* arg0)
{
}

/// A further copy, under this file's own name.
#define madChaserDeathCryUnlink func_actor_342400_8016BBD8
#include "../../shared/mad_chaser_death_cry_unlink.inc.c"
#undef madChaserDeathCryUnlink

#include "../../shared/mad_chaser_begin_shrink.inc.c"

/// A further copy, under this file's own name.
#define madChaserDeathTurnTranslucent func_actor_342400_8016BD3C
#include "../../shared/mad_chaser_death_translucent.inc.c"
#undef madChaserDeathTurnTranslucent

#include "../../shared/mad_chaser_shrink.inc.c"

/// A further copy, under this file's own name.
#define madChaserStartDespawn func_actor_342400_8016BED8
#include "../../shared/mad_chaser_start_despawn.inc.c"
#undef madChaserStartDespawn

#include "../../shared/mad_chaser_take_knockdown_request.inc.c"
