#include "actor_342400_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

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
extern u8 gMadChaserAnimBank[]; // animation bank handed to `animationInitContext`
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gMadChaserMsgTable[3];   // stored into `Task::msgTable` by madChaserSpawn
extern u8               gMadChaserAnimStance[];  // per animation id (1-based): value for `stateScratch`
extern u8               gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it

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

/// Eleven state handlers, indexed by `MadChaserWork::state`; copied to
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

/// Sub-state handlers `madChaserKnockdownState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserKnockdownSteps = { {
    madChaserKnockdownStart,
    madChaserKnockdownRise,
    madChaserKnockdownEnd,
} };

/// Sub-state handlers `madChaserWalkState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserWalkSteps = { {
    madChaserWalkStart,
    madChaserWalkApproach,
    madChaserWalkFinish,
} };

/// Sub-state handlers `madChaserLeapState` dispatches by `subState`.
static const TaskFuncTable5 gMadChaserLeapSteps = { {
    madChaserStartLeap,
    madChaserLeapAttack,
    madChaserLeapTurnAway,
    madChaserLeapRebound,
    madChaserLeapLand,
} };

/// Sub-state handlers `func_actor_342400_80169A98` dispatches by `subState`.
static const TaskFuncTable5 D_actor_342400_80161F00 = { {
    madChaserAlertCry,
    madChaserAlertWait,
    madChaserAlertRelease,
    madChaserAlertCrouch,
    madChaserAlertSidestep,
} };

/// Sub-state handlers `madChaserDangleState` dispatches by `subState`.
static const TaskFuncTable4 gMadChaserDangleSteps = { {
    madChaserDangleStart,
    madChaserDangleSway,
    madChaserDangleFall,
    madChaserDangleLand,
} };

static AnimationPackedPose _gActor342400Animation0E9A4Bank1[6] = {
#include "assets/actor_342400_animation_0E9A4_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0E9A4Bank4[39] = {
#include "assets/actor_342400_animation_0E9A4_bank4.inc"
};

static AnimationRecord _gActor342400Animation0E9A4Records[77] = {
#include "assets/actor_342400_animation_0E9A4_records.inc"
};

static u16 _gActor342400Animation0E9A4Indices[10] = {
#include "assets/actor_342400_animation_0E9A4_indices.inc"
};

static AnimationSet _gActor342400Animation0E9A4 = {
    _gActor342400Animation0E9A4Records,
    _gActor342400Animation0E9A4Indices,
    { NULL, _gActor342400Animation0E9A4Bank1, NULL, NULL, _gActor342400Animation0E9A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation0EB48Bank1[3] = {
#include "assets/actor_342400_animation_0EB48_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0EB48Bank4[21] = {
#include "assets/actor_342400_animation_0EB48_bank4.inc"
};

static AnimationRecord _gActor342400Animation0EB48Records[60] = {
#include "assets/actor_342400_animation_0EB48_records.inc"
};

static u16 _gActor342400Animation0EB48Indices[10] = {
#include "assets/actor_342400_animation_0EB48_indices.inc"
};

static AnimationSet _gActor342400Animation0EB48 = {
    _gActor342400Animation0EB48Records,
    _gActor342400Animation0EB48Indices,
    { NULL, _gActor342400Animation0EB48Bank1, NULL, NULL, _gActor342400Animation0EB48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation0F01CBank1[20] = {
#include "assets/actor_342400_animation_0F01C_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0F01CBank4[99] = {
#include "assets/actor_342400_animation_0F01C_bank4.inc"
};

static AnimationRecord _gActor342400Animation0F01CRecords[135] = {
#include "assets/actor_342400_animation_0F01C_records.inc"
};

static u16 _gActor342400Animation0F01CIndices[10] = {
#include "assets/actor_342400_animation_0F01C_indices.inc"
};

static AnimationSet _gActor342400Animation0F01C = {
    _gActor342400Animation0F01CRecords,
    _gActor342400Animation0F01CIndices,
    { NULL, _gActor342400Animation0F01CBank1, NULL, NULL, _gActor342400Animation0F01CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation0F5A0Bank1[25] = {
#include "assets/actor_342400_animation_0F5A0_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0F5A0Bank4[110] = {
#include "assets/actor_342400_animation_0F5A0_bank4.inc"
};

static AnimationRecord _gActor342400Animation0F5A0Records[153] = {
#include "assets/actor_342400_animation_0F5A0_records.inc"
};

static u16 _gActor342400Animation0F5A0Indices[10] = {
#include "assets/actor_342400_animation_0F5A0_indices.inc"
};

static AnimationSet _gActor342400Animation0F5A0 = {
    _gActor342400Animation0F5A0Records,
    _gActor342400Animation0F5A0Indices,
    { NULL, _gActor342400Animation0F5A0Bank1, NULL, NULL, _gActor342400Animation0F5A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation0F6A0Bank1[2] = {
#include "assets/actor_342400_animation_0F6A0_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0F6A0Bank4[7] = {
#include "assets/actor_342400_animation_0F6A0_bank4.inc"
};

static AnimationRecord _gActor342400Animation0F6A0Records[36] = {
#include "assets/actor_342400_animation_0F6A0_records.inc"
};

static u16 _gActor342400Animation0F6A0Indices[10] = {
#include "assets/actor_342400_animation_0F6A0_indices.inc"
};

static AnimationSet _gActor342400Animation0F6A0 = {
    _gActor342400Animation0F6A0Records,
    _gActor342400Animation0F6A0Indices,
    { NULL, _gActor342400Animation0F6A0Bank1, NULL, NULL, _gActor342400Animation0F6A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation0F7A0Bank1[2] = {
#include "assets/actor_342400_animation_0F7A0_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0F7A0Bank4[7] = {
#include "assets/actor_342400_animation_0F7A0_bank4.inc"
};

static AnimationRecord _gActor342400Animation0F7A0Records[36] = {
#include "assets/actor_342400_animation_0F7A0_records.inc"
};

static u16 _gActor342400Animation0F7A0Indices[10] = {
#include "assets/actor_342400_animation_0F7A0_indices.inc"
};

static AnimationSet _gActor342400Animation0F7A0 = {
    _gActor342400Animation0F7A0Records,
    _gActor342400Animation0F7A0Indices,
    { NULL, _gActor342400Animation0F7A0Bank1, NULL, NULL, _gActor342400Animation0F7A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation0FBD0Bank1[24] = {
#include "assets/actor_342400_animation_0FBD0_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation0FBD0Bank4[69] = {
#include "assets/actor_342400_animation_0FBD0_bank4.inc"
};

static AnimationRecord _gActor342400Animation0FBD0Records[112] = {
#include "assets/actor_342400_animation_0FBD0_records.inc"
};

static u16 _gActor342400Animation0FBD0Indices[10] = {
#include "assets/actor_342400_animation_0FBD0_indices.inc"
};

static AnimationSet _gActor342400Animation0FBD0 = {
    _gActor342400Animation0FBD0Records,
    _gActor342400Animation0FBD0Indices,
    { NULL, _gActor342400Animation0FBD0Bank1, NULL, NULL, _gActor342400Animation0FBD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation10074Bank1[22] = {
#include "assets/actor_342400_animation_10074_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation10074Bank4[87] = {
#include "assets/actor_342400_animation_10074_bank4.inc"
};

static AnimationRecord _gActor342400Animation10074Records[129] = {
#include "assets/actor_342400_animation_10074_records.inc"
};

static u16 _gActor342400Animation10074Indices[10] = {
#include "assets/actor_342400_animation_10074_indices.inc"
};

static AnimationSet _gActor342400Animation10074 = {
    _gActor342400Animation10074Records,
    _gActor342400Animation10074Indices,
    { NULL, _gActor342400Animation10074Bank1, NULL, NULL, _gActor342400Animation10074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation105CCBank1[14] = {
#include "assets/actor_342400_animation_105CC_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation105CCBank4[99] = {
#include "assets/actor_342400_animation_105CC_bank4.inc"
};

static AnimationRecord _gActor342400Animation105CCRecords[186] = {
#include "assets/actor_342400_animation_105CC_records.inc"
};

static u16 _gActor342400Animation105CCIndices[10] = {
#include "assets/actor_342400_animation_105CC_indices.inc"
};

static AnimationSet _gActor342400Animation105CC = {
    _gActor342400Animation105CCRecords,
    _gActor342400Animation105CCIndices,
    { NULL, _gActor342400Animation105CCBank1, NULL, NULL, _gActor342400Animation105CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation10880Bank1[8] = {
#include "assets/actor_342400_animation_10880_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation10880Bank4[54] = {
#include "assets/actor_342400_animation_10880_bank4.inc"
};

static AnimationRecord _gActor342400Animation10880Records[80] = {
#include "assets/actor_342400_animation_10880_records.inc"
};

static u16 _gActor342400Animation10880Indices[10] = {
#include "assets/actor_342400_animation_10880_indices.inc"
};

static AnimationSet _gActor342400Animation10880 = {
    _gActor342400Animation10880Records,
    _gActor342400Animation10880Indices,
    { NULL, _gActor342400Animation10880Bank1, NULL, NULL, _gActor342400Animation10880Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation10B14Bank1[7] = {
#include "assets/actor_342400_animation_10B14_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation10B14Bank4[50] = {
#include "assets/actor_342400_animation_10B14_bank4.inc"
};

static AnimationRecord _gActor342400Animation10B14Records[79] = {
#include "assets/actor_342400_animation_10B14_records.inc"
};

static u16 _gActor342400Animation10B14Indices[10] = {
#include "assets/actor_342400_animation_10B14_indices.inc"
};

static AnimationSet _gActor342400Animation10B14 = {
    _gActor342400Animation10B14Records,
    _gActor342400Animation10B14Indices,
    { NULL, _gActor342400Animation10B14Bank1, NULL, NULL, _gActor342400Animation10B14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation10DC4Bank1[7] = {
#include "assets/actor_342400_animation_10DC4_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation10DC4Bank4[53] = {
#include "assets/actor_342400_animation_10DC4_bank4.inc"
};

static AnimationRecord _gActor342400Animation10DC4Records[83] = {
#include "assets/actor_342400_animation_10DC4_records.inc"
};

static u16 _gActor342400Animation10DC4Indices[10] = {
#include "assets/actor_342400_animation_10DC4_indices.inc"
};

static AnimationSet _gActor342400Animation10DC4 = {
    _gActor342400Animation10DC4Records,
    _gActor342400Animation10DC4Indices,
    { NULL, _gActor342400Animation10DC4Bank1, NULL, NULL, _gActor342400Animation10DC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation10FD8Bank1[6] = {
#include "assets/actor_342400_animation_10FD8_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation10FD8Bank4[42] = {
#include "assets/actor_342400_animation_10FD8_bank4.inc"
};

static AnimationRecord _gActor342400Animation10FD8Records[58] = {
#include "assets/actor_342400_animation_10FD8_records.inc"
};

static u16 _gActor342400Animation10FD8Indices[10] = {
#include "assets/actor_342400_animation_10FD8_indices.inc"
};

static AnimationSet _gActor342400Animation10FD8 = {
    _gActor342400Animation10FD8Records,
    _gActor342400Animation10FD8Indices,
    { NULL, _gActor342400Animation10FD8Bank1, NULL, NULL, _gActor342400Animation10FD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation110F8Bank1[2] = {
#include "assets/actor_342400_animation_110F8_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation110F8Bank4[11] = {
#include "assets/actor_342400_animation_110F8_bank4.inc"
};

static AnimationRecord _gActor342400Animation110F8Records[40] = {
#include "assets/actor_342400_animation_110F8_records.inc"
};

static u16 _gActor342400Animation110F8Indices[10] = {
#include "assets/actor_342400_animation_110F8_indices.inc"
};

static AnimationSet _gActor342400Animation110F8 = {
    _gActor342400Animation110F8Records,
    _gActor342400Animation110F8Indices,
    { NULL, _gActor342400Animation110F8Bank1, NULL, NULL, _gActor342400Animation110F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation11234Bank1[4] = {
#include "assets/actor_342400_animation_11234_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation11234Bank4[18] = {
#include "assets/actor_342400_animation_11234_bank4.inc"
};

static AnimationRecord _gActor342400Animation11234Records[34] = {
#include "assets/actor_342400_animation_11234_records.inc"
};

static u16 _gActor342400Animation11234Indices[10] = {
#include "assets/actor_342400_animation_11234_indices.inc"
};

static AnimationSet _gActor342400Animation11234 = {
    _gActor342400Animation11234Records,
    _gActor342400Animation11234Indices,
    { NULL, _gActor342400Animation11234Bank1, NULL, NULL, _gActor342400Animation11234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation11430Bank1[10] = {
#include "assets/actor_342400_animation_11430_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation11430Bank4[31] = {
#include "assets/actor_342400_animation_11430_bank4.inc"
};

static AnimationRecord _gActor342400Animation11430Records[51] = {
#include "assets/actor_342400_animation_11430_records.inc"
};

static u16 _gActor342400Animation11430Indices[10] = {
#include "assets/actor_342400_animation_11430_indices.inc"
};

static AnimationSet _gActor342400Animation11430 = {
    _gActor342400Animation11430Records,
    _gActor342400Animation11430Indices,
    { NULL, _gActor342400Animation11430Bank1, NULL, NULL, _gActor342400Animation11430Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation1186CBank1[16] = {
#include "assets/actor_342400_animation_1186C_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation1186CBank4[83] = {
#include "assets/actor_342400_animation_1186C_bank4.inc"
};

static AnimationRecord _gActor342400Animation1186CRecords[125] = {
#include "assets/actor_342400_animation_1186C_records.inc"
};

static u16 _gActor342400Animation1186CIndices[10] = {
#include "assets/actor_342400_animation_1186C_indices.inc"
};

static AnimationSet _gActor342400Animation1186C = {
    _gActor342400Animation1186CRecords,
    _gActor342400Animation1186CIndices,
    { NULL, _gActor342400Animation1186CBank1, NULL, NULL, _gActor342400Animation1186CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation119F8Bank1[5] = {
#include "assets/actor_342400_animation_119F8_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation119F8Bank4[26] = {
#include "assets/actor_342400_animation_119F8_bank4.inc"
};

static AnimationRecord _gActor342400Animation119F8Records[43] = {
#include "assets/actor_342400_animation_119F8_records.inc"
};

static u16 _gActor342400Animation119F8Indices[10] = {
#include "assets/actor_342400_animation_119F8_indices.inc"
};

static AnimationSet _gActor342400Animation119F8 = {
    _gActor342400Animation119F8Records,
    _gActor342400Animation119F8Indices,
    { NULL, _gActor342400Animation119F8Bank1, NULL, NULL, _gActor342400Animation119F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342400Animation11BA0Bank1[5] = {
#include "assets/actor_342400_animation_11BA0_bank1.inc"
};

static AnimationPackedRotation _gActor342400Animation11BA0Bank4[30] = {
#include "assets/actor_342400_animation_11BA0_bank4.inc"
};

static AnimationRecord _gActor342400Animation11BA0Records[46] = {
#include "assets/actor_342400_animation_11BA0_records.inc"
};

static u16 _gActor342400Animation11BA0Indices[10] = {
#include "assets/actor_342400_animation_11BA0_indices.inc"
};

static AnimationSet _gActor342400Animation11BA0 = {
    _gActor342400Animation11BA0Records,
    _gActor342400Animation11BA0Indices,
    { NULL, _gActor342400Animation11BA0Bank1, NULL, NULL, _gActor342400Animation11BA0Bank4, NULL, NULL, NULL },
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

TaskMessageEntry gMadChaserMsgTable[3] = {
    { ACTOR_MESSAGE_PLACE, madChaserMsgPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, madChaserCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_342400_80173A54[2] = {
    { { { TASK_BODY_TMD, 96 } }, madChaserTask, { .model = &gActor342400MadChaserBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, madChaserHiddenTask, { .model = &gActor342400MadChaserBody } },
};

TaskDesc D_actor_342400_80173A6C = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_342400_80173A78 = { { { TASK_BODY_TMD, 96 } }, madChaserHiddenTask, { .model = &gActor342400MadChaserBody } };

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

/// Nine state handlers, indexed by `MadChaserWork::state`; copied to the
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
/// `MadChaserWork::state`; copied to the stack before dispatch. It sits
/// between `madChaserRecoilRecover`'s jump table and this function's own.
static const TaskFuncTable5 gMadChaserLurkStates = { {
    func_actor_342400_8016AE24,
    func_actor_342400_8016AEAC,
    madChaserLurkRiseState,
    madChaserLurkAlertState,
    func_actor_342400_8016B038,
} };

#include "../../shared/mad_chaser_lurk_tick.inc.c"

/// Sub-state handlers `func_actor_342400_8016AE24` dispatches by `subState`.
static const TaskFuncTable3 D_actor_342400_80161FB4 = { {
    madChaserStartHold,
    madChaserLurkWait,
    madChaserLurkIdleEnd,
} };

/// Sub-state handlers `func_actor_342400_8016AEAC` dispatches by `subState`.
static const TaskFuncTable3 D_actor_342400_80161FC0 = { {
    madChaserLurkCrouch,
    madChaserLurkRaise,
    madChaserLurkLookAround,
} };

/// Sub-state handlers `madChaserLurkRiseState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkAlertSteps = { {
    madChaserStartAlert,
    madChaserLurkBrace,
    madChaserLurkSidestepToCombat,
} };

/// Sub-state handlers `func_actor_342400_8016B038` dispatches by `subState`.
static const TaskFuncTable4 D_actor_342400_80161FD8 = { {
    madChaserLurkShiftStart,
    madChaserLurkShiftBrace,
    madChaserLurkSidestepRight,
    madChaserLurkSidestepLeft,
} };

/// Ten state handlers, indexed by `MadChaserWork::state`; copied to the
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

/// Sub-state handlers `madChaserPullState` dispatches by `subState`.
static const TaskFuncTable6 gMadChaserPullSteps = { {
    madChaserPullStart,
    madChaserPullReact,
    madChaserPulledStruggle,
    madChaserPulledIn,
    madChaserPulledLimp,
    madChaserPulledIn,
} };

/// Five state handlers, indexed by `MadChaserWork::state`; copied to
/// the stack before dispatch.
static const TaskFuncTable5 gMadChaserDropDeathStates = { {
    madChaserDeathCryUnlink,
    madChaserDeathSettleQuiet,
    func_actor_342400_8016BAF4,
    madChaserDropBodies,
    func_actor_342400_8016BBD0,
} };

/// Seven state handlers, indexed by `MadChaserWork::state`; copied to
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

#include "../../shared/mad_chaser_emerge_tick.inc.c"

/// `madChaserSetStateS16` with an `s16` state. The narrower parameter is load-bearing:
/// with the `s32` one, `madChaserEmergeAtSpot` no longer matches. Each
/// call site reloads `work`, and cross-jumping merges the identical stores,
/// which is what leaves one `lw` per arm in front of a shared tail.
static __inline__ void set_state_s16(Task* arg0, s16 state)
{
    MadChaserWork* w = (MadChaserWork*)arg0->work;

    w->state    = state;
    w->subState = 0;
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
