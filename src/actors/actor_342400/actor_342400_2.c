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
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
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
#include "../../shared/hopping_enemy.h"
#include "../../shared/hopper_waves.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

// the main enemy's `GpEnemy::param` record
extern u8 gHopperAnimBank[]; // animation bank handed to `func_800B3F84`
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s16, VECTOR3*);
        void (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} Actor3424002MessageEntry;
STATIC_ASSERT_SIZEOF(Actor3424002MessageEntry, 8);

extern Actor3424002MessageEntry gHopperMsgTable[3];   // stored into `Task::msgTable` by hopperSpawn
extern u8                       gHopperAnimStance[];  // per animation id (1-based): value for `field_44F`
extern u8                       gHopperSettleAnims[]; // per animation id (1-based): the animation to follow it

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_342400_801640B0(Task* arg0);
static void func_actor_342400_80165FC0(Task* arg0);
static void func_actor_342400_8016666C(Task* arg0);
static void func_actor_342400_801670C0(Task* arg0);
static void func_actor_342400_80168394(Task* arg0);
static void func_actor_342400_80168F14(Task* arg0);
static void func_actor_342400_801690FC(Task* arg0);
static void func_actor_342400_80169408(Task* arg0);
void        func_actor_342400_8016978C(Task* arg0);
void        func_actor_342400_80169810(Task* arg0);
static void func_actor_342400_80169880(Task* arg0);
static void func_actor_342400_80169968(Task* arg0);
static void func_actor_342400_8016997C(Task* arg0);
static void func_actor_342400_80169990(Task* arg0);
static void func_actor_342400_801699A4(Task* arg0);
static void func_actor_342400_80169A2C(Task* arg0);
static void func_actor_342400_80169A98(Task* arg0);
static void func_actor_342400_80169B04(Task* arg0);
static void func_actor_342400_80169B58(Task* arg0);
static void func_actor_342400_80169BAC(Task* arg0);
static void func_actor_342400_80169C00(Task* arg0);
static void func_actor_342400_80169C84(Task* arg0);
static void func_actor_342400_80169CF8(Task* arg0);
static void func_actor_342400_8016A240(Task* arg0);
static void func_actor_342400_8016A4FC(Task* arg0);
static void func_actor_342400_8016A9AC(Task* arg0);
static void func_actor_342400_8016A9C4(Task* arg0);
static void func_actor_342400_8016AA9C(Task* arg0);
static void func_actor_342400_8016AE24(Task* arg0);
static void func_actor_342400_8016AEAC(Task* arg0);
static void func_actor_342400_8016AFA8(Task* arg0);
static void func_actor_342400_8016B038(Task* arg0);
static void func_actor_342400_8016BAF4(Task* arg0);
static void func_actor_342400_8016BBD0(Task* arg0);
static void func_actor_342400_8016BBD8(Task* arg0);
static void func_actor_342400_8016BD3C(Task* arg0);
static void func_actor_342400_8016BED8(Task* arg0);

/// Six task-state handlers of the first enemy form, dispatched by
/// `func_actor_342400_80169810` on `Task::state`.
static const TaskFuncTable6 D_actor_342400_80161E68 = { {
    hopperSpawn,
    func_actor_342400_8016666C,
    hopperDangleFrame,
    func_actor_342400_801640B0,
    func_actor_342400_80165FC0,
    func_actor_342400_80169880,
} };

/// Ten task-state handlers of the second enemy form, dispatched by
/// `func_actor_342400_8016978C` on `Task::state`.
static const TaskFuncTable10 D_actor_342400_80161E80 = { {
    hopperSpawnHidden,
    func_actor_342400_8016666C,
    hopperDangleFrame,
    func_actor_342400_801640B0,
    func_actor_342400_80165FC0,
    func_actor_342400_80169880,
    func_actor_342400_801670C0,
    func_actor_342400_80169408,
    func_actor_342400_80168F14,
    func_actor_342400_801690FC,
} };

/// Eleven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable11 D_actor_342400_80161EA8 = { {
    func_actor_342400_80169968,
    func_actor_342400_8016997C,
    func_actor_342400_80169990,
    func_actor_342400_801699A4,
    func_actor_342400_80169A2C,
    func_actor_342400_80169A98,
    func_actor_342400_80169B04,
    func_actor_342400_80169B58,
    func_actor_342400_80169BAC,
    func_actor_342400_80169C00,
    func_actor_342400_80169C84,
} };

/// Sub-state handlers `func_actor_342400_80169C00` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161ED4 = { {
    hopperKnockdownStart,
    hopperKnockdownRise,
    hopperKnockdownEnd,
} };

/// Sub-state handlers `func_actor_342400_801699A4` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161EE0 = { {
    hopperWalkStart,
    hopperWalkApproach,
    hopperWalkFinish,
} };

/// Sub-state handlers `func_actor_342400_80169A2C` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_342400_80161EEC = { {
    hopperStartLeap,
    hopperLeapAttack,
    hopperLeapTurnAway,
    hopperLeapRebound,
    hopperLeapLand,
} };

/// Sub-state handlers `func_actor_342400_80169A98` dispatches by `field_422`.
static const TaskFuncTable5 D_actor_342400_80161F00 = { {
    hopperAlertCry,
    func_actor_342400_8016A240,
    hopperAlertRelease,
    hopperAlertCrouch,
    hopperAlertSidestep,
} };

/// Sub-state handlers `hopperDangleState` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_342400_80161F14 = { {
    func_actor_342400_8016A4FC,
    hopperDangleSway,
    hopperDangleFall,
    hopperDangleLand,
} };

void func_actor_342400_8016978C(Task*);
void func_actor_342400_80169810(Task*);

void func_actor_342400_80169620(Task*, s16, VECTOR3*);

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

u8 gHopperAnimBank[84] = {
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

Actor3424002MessageEntry gHopperMsgTable[3] = {
    { 2004, { .call0 = func_actor_342400_80169620 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = hopperCommandMsg } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_342400_80173A54[2] = {
    { TASK_BODY_TMD, 96, func_actor_342400_80169810, { .model = &D_actor_342400_80170560 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_342400_8016978C, { .model = &D_actor_342400_80170560 } },
};

TaskDesc D_actor_342400_80173A6C = { TASK_BODY_COORD, 96, taskKill, { .model = NULL } };

TaskDesc D_actor_342400_80173A78 = { TASK_BODY_TMD, 96, func_actor_342400_8016978C, { .model = &D_actor_342400_80170560 } };

u8 gHopperAnimStance[20] = {
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

u8 gHopperSettleAnims[20] = {
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

u16 gHopperWaveEnemyCount = 0;

extern void* D_800678F0[1];

static __inline__ s16  take_hit(Task* arg0);
static __inline__ void update_rotation(Task* arg0);
static __inline__ s16  take_hit_nibble3(Task* arg0);
static __inline__ void set_state_s16(Task* arg0, s16 state);

#include "../../shared/hopping_enemy_limb_shadow.inc.c"

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` copies into
 * its effect's `TmdObject`. It is declared as a one-element array for the
 * same reason as in `actor_400500`: as a bare scalar, GCC 2.8.1 decides the
 * store cannot alias the `TmdObject` loads and sinks it past them. */

void hopperSpawnGibs(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* src;
    TmdObject* src2;

    D_800678F0[0] = &D_actor_342400_8016CB6C;
    eff           = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[6], 0x200, NULL);
    if (eff != NULL) {
        src                    = arg0->extra.tmd;
        dst                    = eff->task->extra.tmd;
        dst->texturePageOffset = src->texturePageOffset;
        dst->clutRowOffset     = src->clutRowOffset;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((Gp_LcgState >> 16) & 1) {
        D_800678F0[0] = &D_actor_342400_8016D210;
        eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[8], 0x200, NULL);
    } else {
        D_800678F0[0] = &D_actor_342400_8016D780;
        eff2          = Gp_SpawnEff(0x20010, &arg0->extra.tmd->coords[2], 0x200, NULL);
    }
    if (eff2 != NULL) {
        src2                    = arg0->extra.tmd;
        dst2                    = eff2->task->extra.tmd;
        dst2->texturePageOffset = src2->texturePageOffset;
        dst2->clutRowOffset     = src2->clutRowOffset;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[3], 0x200, NULL);
    Gp_SpawnEff(0x60030, &arg0->extra.tmd->coords[4], 0x200, NULL);
}

#include "../../shared/hopping_enemy_twist.inc.c"

#include "../../shared/hopping_enemy_spawn.inc.c"

#include "../../shared/hopping_enemy_spawn_hidden.inc.c"

#include "../../shared/hopping_enemy_inlines.inc.c"

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
            hopperEnterState(arg0, 3);
            w2            = (Actor341700Work*)arg0->work;
            w2->field_420 = 10;
            w2->field_422 = 0;
            return 1;
        }
    } else if ((work->field_44C & 0xF) == 3) {
        work->field_44C = 0;
        hopperEnterState(arg0, 7);
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
/// `func_actor_342400_801670C0`. In mode 0 it aims at the nearest actor
/// (`hopperTrackPlayer`), lets a pending hit (`take_hit`) replace the state
/// handler, eases `field_424` toward zero, rebuilds the root rotation, and
/// then picks the next state: the `field_448` request once dead, state 4 when
/// dead, 8 / 9 for messages 4 / 5 while `field_438` is clear.
static void func_actor_342400_801640B0(Task* arg0)
{
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable11  sp    = D_actor_342400_80161EA8;
    s32              cur;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
            work->field_442++;
            hopperTrackPlayer(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            hopperTickAnim(arg0);
            cur             = (u16)work->field_424;
            work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
            hopperTwistSpine(arg0);
            if (work->field_432 == 1) {
                hopperPinPart(arg0, 6, (SVECTOR3*)&work->field_98);
            }
            update_rotation(arg0);
            hopperApplyContacts(arg0, 0);
            if (work->field_44A != 0) {
                work->field_44A--;
            }
            if (work->field_41E != 0 && work->field_448 == 4 && enemy->hp <= 0) {
                hopperEnterState(arg0, work->field_448);
            }
            if (work->field_438 == 0 && enemy->hp <= 0) {
                hopperEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                hopperEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                hopperEnterState(arg0, 9);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            return;
    }
}

#include "../../shared/hopping_enemy_walk_start.inc.c"

#include "../../shared/hopping_enemy_walk_approach.inc.c"

#include "../../shared/hopping_enemy_leap_attack.inc.c"

#include "../../shared/hopping_enemy_leap_turn_away.inc.c"

#include "../../shared/hopping_enemy_leap_rebound.inc.c"

#include "../../shared/hopping_enemy_dangle_frame.inc.c"

#include "../../shared/hopping_enemy_dangle_fall.inc.c"

#include "../../shared/hopping_enemy_dangle_land.inc.c"

#include "../../shared/hopping_enemy_contacts.inc.c"

#include "../../shared/hopping_enemy_tick_anim.inc.c"

#include "../../shared/hopping_enemy_bodies.inc.c"

/// Nine state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable9 D_actor_342400_80161F50 = { {
    hopperDeathCry,
    hopperDeathSettle,
    hopperDeathWaitAnim,
    hopperBeginDeath,
    hopperDeathTurnTranslucent,
    hopperShrinkWithDust,
    func_actor_342400_8016A9AC,
    func_actor_342400_8016A9C4,
    hopperBurst,
} };

/// Per-frame callback of the main enemy. `Gp_StateF0.field_4` 2 hides the model,
/// 0 runs the current state handler (then colours it), 1 only colours it.
/// Unless `field_451` is set, it then runs `hopperDrawLimbShadow` for
/// three part pairs.
static void func_actor_342400_80165FC0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable9   sp    = D_actor_342400_80161F50;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/hopping_enemy_shrink_dust.inc.c"

#include "../../shared/hopping_enemy_track_player.inc.c"

#include "../../shared/hopping_enemy_recoil_recover.inc.c"

/// The five state handlers of the second enemy form, indexed by
/// `Actor341700Work::field_420`; copied to the stack before dispatch. It sits
/// between `hopperRecoilRecover`'s jump table and this function's own.
static const TaskFuncTable5 D_actor_342400_80161F8C = { {
    func_actor_342400_8016AE24,
    func_actor_342400_8016AEAC,
    hopperLurkRiseState,
    func_actor_342400_8016AFA8,
    func_actor_342400_8016B038,
} };

/// Per-frame callback for the second enemy form, the five-state counterpart
/// of `func_actor_342400_801640B0`: in mode 0 it aims (`hopperTrackPlayer`),
/// lets a pending hit replace the state handler, rebuilds the root rotation,
/// then picks the next state - 4 when dead, 8 / 9 for messages 4 / 5, and
/// state 3 after a consumed `field_448` request. Mode 1 only recolours; both
/// clear bit 0x80 of the model's `field_C`, which mode 2 sets.
static void func_actor_342400_8016666C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_342400_80161F8C;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
            work->field_442++;
            hopperTrackPlayer(arg0);
            if (take_hit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            hopperTickAnim(arg0);
            hopperTwistSpine(arg0);
            update_rotation(arg0);
            hopperApplyContacts(arg0, 0);
            if (work->field_438 == 0 && enemy->hp <= 0) {
                hopperEnterState(arg0, 4);
            } else if (work->field_44C == 4 && work->field_438 == 0) {
                hopperEnterState(arg0, 8);
            } else if (work->field_44C == 5 && work->field_438 == 0) {
                hopperEnterState(arg0, 9);
            } else if (hopperTakeRequest(arg0)) {
                work->field_438 = 0;
                hopperEnterState(arg0, 3);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
            hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
            hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Sub-state handlers `func_actor_342400_8016AE24` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FB4 = { {
    hopperStartHold,
    hopperLurkWait,
    hopperLurkIdleEnd,
} };

/// Sub-state handlers `func_actor_342400_8016AEAC` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FC0 = { {
    hopperLurkCrouch,
    hopperLurkRaise,
    hopperLurkLookAround,
} };

/// Sub-state handlers `hopperLurkRiseState` dispatches by `field_422`.
static const TaskFuncTable3 D_actor_342400_80161FCC = { {
    hopperStartAlert,
    hopperLurkBrace,
    hopperLurkSidestepToCombat,
} };

/// Sub-state handlers `func_actor_342400_8016B038` dispatches by `field_422`.
static const TaskFuncTable4 D_actor_342400_80161FD8 = { {
    hopperLurkShiftStart,
    hopperLurkShiftBrace,
    hopperLurkSidestepRight,
    hopperLurkSidestepLeft,
} };

/// Ten state handlers, indexed by `Actor341700Work::field_420`; copied to the
/// stack before dispatch.
static const TaskFuncTable10 D_actor_342400_80161FE8 = { {
    hopperEmergeAtSpot,
    hopperEmergeBackflip,
    hopperEmergeHopForward,
    hopperCreepUntilHit,
    hopperEmergeArcBack,
    hopperEmergeHopBack,
    hopperEmergeBackOff,
    hopperEmergeHighArc,
    hopperEmergeFlipOver,
    func_actor_342400_80168394,
} };

/// Sub-state handlers `func_actor_342400_80169C84` dispatches by `field_422`.
static const TaskFuncTable6 D_actor_342400_80162010 = { {
    hopperPullStart,
    hopperPullReact,
    hopperPulledStruggle,
    hopperPulledIn,
    hopperPulledLimp,
    hopperPulledIn,
} };

/// Five state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable5 D_actor_342400_80162028 = { {
    hopperDeathCryUnlink,
    hopperDeathSettleQuiet,
    func_actor_342400_8016BAF4,
    hopperDropBodies,
    func_actor_342400_8016BBD0,
} };

/// Seven state handlers, indexed by `Actor341700Work::field_420`; copied to
/// the stack before dispatch.
static const TaskFuncTable7 D_actor_342400_8016203C = { {
    func_actor_342400_8016BBD8,
    hopperDeathSettleQuiet,
    func_actor_342400_8016BAF4,
    hopperBeginShrink,
    func_actor_342400_8016BD3C,
    hopperShrink,
    func_actor_342400_8016BED8,
} };

#include "../../shared/hopping_enemy_lurk_look.inc.c"

#include "../../shared/hopping_enemy_lurk_sidestep_combat.inc.c"

#include "../../shared/hopping_enemy_lurk_sidestep_right.inc.c"

#include "../../shared/hopping_enemy_lurk_sidestep_left.inc.c"

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
/// `func_actor_342400_80165FC0`: in mode 0 a pending hit (`take_hit_nibble3`) replaces
/// the state handler, and the root rotation is rebuilt from 0x78..0x7C before
/// `hopperApplyContacts`.
static void func_actor_342400_801670C0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable10  sp    = D_actor_342400_80161FE8;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
            work->field_442++;
            if (take_hit_nibble3(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            hopperTickAnim(arg0);
            update_rotation(arg0);
            hopperApplyContacts(arg0, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// `hopperSetStateS16` with an `s16` state. The narrower parameter is load-bearing:
/// with the `s32` one, `hopperEmergeAtSpot` no longer matches. Each
/// call site reloads `work`, and cross-jumping merges the identical stores,
/// which is what leaves one `lw` per arm in front of a shared tail.
static __inline__ void set_state_s16(Task* arg0, s16 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

#include "../../shared/hopping_enemy_emerge_at_spot.inc.c"

#include "../../shared/hopping_enemy_emerge_backflip.inc.c"

#include "../../shared/hopping_enemy_emerge_hop_forward.inc.c"

#include "../../shared/hopping_enemy_creep.inc.c"

#include "../../shared/hopping_enemy_emerge_arc_back.inc.c"

#include "../../shared/hopping_enemy_emerge_hop_back.inc.c"

#include "../../shared/hopping_enemy_emerge_back_off.inc.c"

#include "../../shared/hopping_enemy_emerge_high_arc.inc.c"

#include "../../shared/hopping_enemy_emerge_flip_over.inc.c"

/// The same creep as a second entry of the state table.
#define hopperCreepUntilHit func_actor_342400_80168394
#include "../../shared/hopping_enemy_creep.inc.c"
#undef hopperCreepUntilHit

#include "../../shared/hopping_enemy_pulled_struggle.inc.c"

#include "../../shared/hopping_enemy_pulled_in.inc.c"

#include "../../shared/hopping_enemy_pulled_limp.inc.c"

/// Per-frame callback, the five-state counterpart of
/// `func_actor_342400_801690FC`; unlike it, clears bit 0x80 of `field_C` on
/// the way out of modes 0 and 1.
static void func_actor_342400_80168F14(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable5   sp    = D_actor_342400_80162028;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Per-frame callback, the seven-state counterpart of
/// `func_actor_342400_80165FC0`: in mode 0 it also spawns effect 3 on the
/// model's second coord part every 32 frames.
static void func_actor_342400_801690FC(Task* arg0)
{
    TmdObject*       obj   = arg0->extra.tmd;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    GfxCoord*        coord = obj->coords;
    TaskFuncTable7   sp    = D_actor_342400_8016203C;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &arg0->extra.tmd->coords[1], NULL, &work->eff_3FC);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        case 1:
            hopperUpdateColor(arg0->spawnArg2.pointer, &arg0->extra.tmd->coords[1]);
            if (work->field_451 == 0) {
                hopperDrawLimbShadow(arg0, 2, 6, 0xC8, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 1, 7, 0x80, 0, 0xFF);
                hopperDrawLimbShadow(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

#include "../../shared/hopping_enemy_sound_bank.inc.c"

static void func_actor_342400_80169408(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        hopperVanish,
        hopperVanishFree,
    };

    states[(s16)work->field_420](arg0);
}

/// Once bit 7 of `Gp_StateF0.field_1F` is set, puts the task in state 3 with
/// the state machine at state 5 and returns 1; otherwise returns 0.
s16 hopperJoinAlert(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        hopperEnterState(arg0, 3);
        hopperSetStateS16(arg0, 5);
        return 1;
    }
    return 0;
}

#include "../../shared/hopping_enemy_alert_hold.inc.c"

#include "../../shared/hopping_enemy_take_request.inc.c"

#include "../../shared/hopping_enemy_command_msg.inc.c"

/// Moves the model: writes `pos` into the root part's translation and marks
/// the coordinate dirty. `part` is accepted but unused.
void func_actor_342400_80169620(Task* task, s16 part, VECTOR3* pos)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = pos->vx;
    coord->coord.t[1]   = pos->vy;
    coord->coord.t[2]   = pos->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/hopping_enemy_pin_part.inc.c"

/// Scales `arg1` by the animation speed `field_41C`, in 1/16 units.
s32 hopperScaleBySpeed(Task* arg0, s16 arg1)
{
    return (s32)((((Actor341700Work*)arg0->work)->field_41C * arg1) << 0xC) >> 0x10;
}

/// Returns 1 when the hit flags are set - bit 0 of the flag halfword or bits
/// 0x102 of the word - and 0 otherwise.
s16 hopperAnimEnded(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

void func_actor_342400_8016978C(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = D_actor_342400_80161E80;
    sp.funcs[arg0->state](arg0);
}

/// Runs the handler for the task's `Task::state` from the six-entry table.
void func_actor_342400_80169810(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_342400_80161E68;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_342400_80169880(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AA9C,
        hopperDespawn,
    };

    states[(s16)work->field_420](arg0);
}

#include "../../shared/hopping_enemy_turn_to_player.inc.c"

static void func_actor_342400_80169968(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void func_actor_342400_8016997C(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

static void func_actor_342400_80169990(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Unless `hopperTakeHitRequest` consumes a pending request, runs the
/// sub-state handler for `field_422` from a three-entry table.
static void func_actor_342400_801699A4(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161EE0;
    if ((hopperTakeHitRequest(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Runs the sub-state handler for `field_422` from a five-entry table.
static void func_actor_342400_80169A2C(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161EEC;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from another five-entry table.
static void func_actor_342400_80169A98(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161F00;
    sp.funcs[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169B04(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        hopperRecoilLight,
        hopperRecoilRecover,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169B58(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        hopperRecoilHeavy,
        hopperRecoilHeavyEnd,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169BAC(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_80169CF8,
        hopperStatusHold,
    };

    states[(s16)work->field_422](arg0);
}

static void func_actor_342400_80169C00(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161ED4;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        hopperTakeKnockdownRequest(arg0);
    }
}

static void func_actor_342400_80169C84(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable6   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80162010;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 0xC and advances the sub-state.
static void func_actor_342400_80169CF8(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

#include "../../shared/hopping_enemy_status_hold.inc.c"

#include "../../shared/hopping_enemy_knockdown_start.inc.c"

#include "../../shared/hopping_enemy_knockdown_rise.inc.c"

#include "../../shared/hopping_enemy_knockdown_end.inc.c"

#include "../../shared/hopping_enemy_walk_finish.inc.c"

#include "../../shared/hopping_enemy_start_leap.inc.c"

#include "../../shared/hopping_enemy_leap_land.inc.c"

#include "../../shared/hopping_enemy_alert_cry.inc.c"

/// Advances the sub-state once the frame counter has passed 0x50.
static void func_actor_342400_8016A240(Task* arg0)
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

#include "../../shared/hopping_enemy_alert_release.inc.c"

#include "../../shared/hopping_enemy_alert_crouch.inc.c"

#include "../../shared/hopping_enemy_alert_sidestep.inc.c"

/// Runs the sub-state handler for `field_422` from a four-entry table.
void hopperDangleState(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161F14;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Sets `field_432`, requests animation 7 and advances the sub-state.
static void func_actor_342400_8016A4FC(Task* arg0)
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

#include "../../shared/hopping_enemy_dangle_sway.inc.c"

#include "../../shared/hopping_enemy_death_cry.inc.c"

#include "../../shared/hopping_enemy_death_settle.inc.c"

#include "../../shared/hopping_enemy_death_wait_anim.inc.c"

#include "../../shared/hopping_enemy_begin_death.inc.c"

#include "../../shared/hopping_enemy_death_translucent.inc.c"

static void func_actor_342400_8016A9AC(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

/// Advances the state after two frames.
static void func_actor_342400_8016A9C4(Task* arg0)
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

#include "../../shared/hopping_enemy_burst.inc.c"

static void func_actor_342400_8016AA9C(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

#include "../../shared/hopping_enemy_despawn.inc.c"

#include "../../shared/hopping_enemy_recoil_light.inc.c"

#include "../../shared/hopping_enemy_recoil_heavy.inc.c"

#include "../../shared/hopping_enemy_recoil_heavy_end.inc.c"

static void func_actor_342400_8016AE24(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FB4;
    if ((hopperJoinAlert(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

static void func_actor_342400_8016AEAC(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FC0;
    if ((hopperJoinAlert(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

#include "../../shared/hopping_enemy_lurk_rise_state.inc.c"

static void func_actor_342400_8016AFA8(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FCC;
    if ((hopperJoinAlert(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from a four-entry table.
static void func_actor_342400_8016B038(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = D_actor_342400_80161FD8;
    sp.funcs[(s16)work->field_422](arg0);
}

#include "../../shared/hopping_enemy_start_hold.inc.c"

#include "../../shared/hopping_enemy_lurk_wait.inc.c"

#include "../../shared/hopping_enemy_lurk_idle_end.inc.c"

#include "../../shared/hopping_enemy_lurk_crouch.inc.c"

#include "../../shared/hopping_enemy_lurk_raise.inc.c"

/// Requests animation 0xF and advances the sub-state.
void hopperLurkRiseStart(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

#include "../../shared/hopping_enemy_lurk_rise_end.inc.c"

#include "../../shared/hopping_enemy_start_alert.inc.c"

#include "../../shared/hopping_enemy_lurk_brace.inc.c"

#include "../../shared/hopping_enemy_lurk_shift_start.inc.c"

#include "../../shared/hopping_enemy_lurk_shift_brace.inc.c"

#include "../../shared/hopping_enemy_pull_start.inc.c"

#include "../../shared/hopping_enemy_pull_react.inc.c"

#include "../../shared/hopping_enemy_vanish.inc.c"

#include "../../shared/hopping_enemy_vanish_free.inc.c"

#include "../../shared/hopping_enemy_death_cry_unlink.inc.c"

#include "../../shared/hopping_enemy_death_settle_quiet.inc.c"

/// A further copy, under this file's own name.
#define hopperDeathWaitAnim func_actor_342400_8016BAF4
#include "../../shared/hopping_enemy_death_wait_anim.inc.c"
#undef hopperDeathWaitAnim

#include "../../shared/hopping_enemy_drop_bodies.inc.c"

/// Empty state handler.
static void func_actor_342400_8016BBD0(Task* arg0)
{
}

/// A further copy, under this file's own name.
#define hopperDeathCryUnlink func_actor_342400_8016BBD8
#include "../../shared/hopping_enemy_death_cry_unlink.inc.c"
#undef hopperDeathCryUnlink

#include "../../shared/hopping_enemy_begin_shrink.inc.c"

/// A further copy, under this file's own name.
#define hopperDeathTurnTranslucent func_actor_342400_8016BD3C
#include "../../shared/hopping_enemy_death_translucent.inc.c"
#undef hopperDeathTurnTranslucent

#include "../../shared/hopping_enemy_shrink.inc.c"

static void func_actor_342400_8016BED8(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

#include "../../shared/hopping_enemy_take_knockdown_request.inc.c"
