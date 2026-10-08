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
#include "main/random.h"
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

static void _madChaserEmergeCreep9(Task* task);
static void _madChaserDropDeathStart(Task* task);
static void _madChaserDeathTurnTranslucent(Task* task);
static void _madChaserEmergeArcBack(Task* task);
static void _madChaserEmergeBackOff(Task* task);
static void _madChaserEmergeBackflip(Task* task);
static void _madChaserEmergeFlipOver(Task* task);
static void _madChaserEmergeHighArc(Task* task);
static void _madChaserEmergeHopBack(Task* task);
static void _madChaserEmergeHopForward(Task* task);
static void _madChaserKnockdownState(Task* task);
static void _madChaserPullState(Task* task);
static void _madChaserDespawnState(Task* task);
static void _madChaserCombatToAlertState1(Task* task);
static void _madChaserCombatToAlertState2(Task* task);
static void _madChaserWalkState(Task* task);
static void func_actor_342400_80169A98(Task* arg0);
static void func_actor_342400_80169B58(Task* arg0);
static void func_actor_342400_80169BAC(Task* arg0);
static void _madChaserLurkIdleState(Task* task);
static void _madChaserLurkLookState(Task* task);
static void _madChaserLurkShiftState(Task* task);
static void _madChaserCommandDeathWaitAnimBoundary(Task* task);
static void _madChaserDropDeathHold(Task* unusedTask);
static void _madChaserShrinkDeathStart(Task* task);
static void _madChaserShrinkDeathTurnTranslucent(Task* task);
static void _madChaserShrinkDeathStartDespawn(Task* task);

/// Six task-state handlers of the first enemy form, dispatched by
/// `_madChaserTask` on `Task::state`.
static const TaskFuncTable6 gMadChaserTaskStates = { {
    madChaserSpawn,
    madChaserLurkTick,
    madChaserDangleFrame,
    madChaserCombatTick,
    _madChaserDeathTick,
    _madChaserDespawnState,
} };

/// Ten task-state handlers of the second enemy form, dispatched by
/// `_madChaserHiddenTask` on `Task::state`.
static const TaskFuncTable10 gMadChaserHiddenTaskStates = { {
    _madChaserSpawnHidden,
    madChaserLurkTick,
    madChaserDangleFrame,
    madChaserCombatTick,
    _madChaserDeathTick,
    _madChaserDespawnState,
    madChaserEmergeTick,
    _madChaserVanishState,
    _madChaserDropDeathTick,
    _madChaserShrinkDeathTick,
} };

/// Eleven state handlers, indexed by `MadChaserWork::state`; copied to
/// the stack before dispatch.
static const TaskFuncTable11 gMadChaserCombatStates = { {
    madChaserToAlertState,
    _madChaserCombatToAlertState1,
    _madChaserCombatToAlertState2,
    _madChaserWalkState,
    madChaserLeapState,
    func_actor_342400_80169A98,
    madChaserRecoilLightState,
    func_actor_342400_80169B58,
    func_actor_342400_80169BAC,
    _madChaserKnockdownState,
    _madChaserPullState,
} };

/// Sub-state handlers `_madChaserKnockdownState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserKnockdownSteps = { {
    _madChaserKnockdownStart,
    _madChaserKnockdownRise,
    _madChaserKnockdownEnd,
} };

/// Sub-state handlers `_madChaserWalkState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserWalkSteps = { {
    _madChaserWalkStart,
    _madChaserWalkApproach,
    _madChaserWalkFinish,
} };

/// Sub-state handlers `madChaserLeapState` dispatches by `subState`.
static const TaskFuncTable5 gMadChaserLeapSteps = { {
    _madChaserStartLeap,
    _madChaserLeapAttack,
    _madChaserLeapTurnAway,
    _madChaserLeapRebound,
    _madChaserLeapLand,
} };

/// Sub-state handlers `func_actor_342400_80169A98` dispatches by `subState`.
static const TaskFuncTable5 D_actor_342400_80161F00 = { {
    _madChaserAlertCry,
    _madChaserAlertWait,
    _madChaserAlertRelease,
    _madChaserAlertStartSidestep,
    _madChaserAlertSidestep,
} };

/// Ordered phases of the anchored dangle and its release into combat.
///
/// `MadChaserWork::subState` selects 0 start, 1 sway, 2 fall or 3 land.
/// All entries are non-NULL void(Task*) callbacks; their code stays loaded
/// through dispatch. Copying the table copies pointers, never task storage.
static const TaskFuncTable4 _gMadChaserDangleSteps = { {
    _madChaserDangleStart,
    _madChaserDangleSway,
    _madChaserDangleFall,
    _madChaserDangleLand,
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
    { ACTOR_MESSAGE_PLACE, _madChaserPlaceRoot },
    { ACTOR_COMMAND_MESSAGE_APPLY, _madChaserQueueCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_342400_80173A54[2] = {
    { { { TASK_BODY_TMD, 96 } }, _madChaserTask, { .model = &gActor342400MadChaserBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _madChaserHiddenTask, { .model = &gActor342400MadChaserBody } },
};

TaskDesc D_actor_342400_80173A6C = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_342400_80173A78 = { { { TASK_BODY_TMD, 96 } }, _madChaserHiddenTask, { .model = &gActor342400MadChaserBody } };

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

/* `D_800678F0` selects the model stream the next `effectSpawn` copies into
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
    _madChaserDeathReleaseTarget,
    _madChaserDeathSettle,
    _madChaserDeathWaitAnim,
    _madChaserDeathStartShrink,
    _madChaserDeathTurnTranslucent,
    _madChaserShrinkWithBurn,
    madChaserStartDespawn,
    _madChaserDeathPause,
    _madChaserBurst,
} };

#include "../../shared/mad_chaser_death_tick.inc.c"

#include "../../shared/mad_chaser_shrink_dust.inc.c"

#include "../../shared/mad_chaser_track_player.inc.c"

#include "../../shared/mad_chaser_recoil_recover.inc.c"

/// The five state handlers of the second enemy form, indexed by
/// `MadChaserWork::state`; copied to the stack before dispatch. It sits
/// between `_madChaserRecoilLightRecover`'s jump table and this function's own.
static const TaskFuncTable5 gMadChaserLurkStates = { {
    _madChaserLurkIdleState,
    _madChaserLurkLookState,
    _madChaserLurkRiseState,
    _madChaserLurkAlertState,
    _madChaserLurkShiftState,
} };

#include "../../shared/mad_chaser_lurk_tick.inc.c"

/// Sub-state handlers `_madChaserLurkIdleState` dispatches by `subState`.
static const TaskFuncTable3 D_actor_342400_80161FB4 = { {
    _madChaserLurkStartIdleHold,
    _madChaserLurkWait,
    _madChaserLurkIdleEnd,
} };

/// Sub-state handlers `_madChaserLurkLookState` dispatches by `subState`.
static const TaskFuncTable3 D_actor_342400_80161FC0 = { {
    _madChaserLurkPrepareLook,
    _madChaserLurkStartLookHold,
    _madChaserLurkLookAround,
} };

/// Sub-state handlers `_madChaserLurkRiseState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkAlertSteps = { {
    _madChaserStartAlert,
    _madChaserLurkAlertStartSidestep,
    _madChaserLurkSidestepToCombat,
} };

/// Ordered phases of the lurk sidestep sequence.
///
/// `MadChaserWork::subState` selects 0 opening animation, 1 right-step start,
/// 2 move right or 3 return left. All entries are non-NULL void(Task*) callbacks;
/// their code stays loaded through dispatch. Completion returns to lurk idle.
static const TaskFuncTable4 _gMadChaserLurkShiftSteps = { {
    _madChaserLurkShiftStart,
    _madChaserLurkShiftStartSidestep,
    _madChaserLurkSidestepRight,
    _madChaserLurkSidestepLeft,
} };

/// Ten state handlers, indexed by `MadChaserWork::state`; copied to the
/// stack before dispatch.
static const TaskFuncTable10 gMadChaserEmergeStates = { {
    madChaserEmergeAtSpot,
    _madChaserEmergeBackflip,
    _madChaserEmergeHopForward,
    madChaserCreepUntilHit,
    _madChaserEmergeArcBack,
    _madChaserEmergeHopBack,
    _madChaserEmergeBackOff,
    _madChaserEmergeHighArc,
    _madChaserEmergeFlipOver,
    _madChaserEmergeCreep9,
} };

/// Sub-state handlers `_madChaserPullState` dispatches by `subState`.
static const TaskFuncTable6 gMadChaserPullSteps = { {
    _madChaserPullStart,
    _madChaserPullReact,
    _madChaserPulledStruggle,
    _madChaserPulledIn,
    _madChaserPulledLimp,
    _madChaserPulledIn,
} };

/// Five state handlers, indexed by `MadChaserWork::state`; copied to
/// the stack before dispatch.
static const TaskFuncTable5 gMadChaserDropDeathStates = { {
    _madChaserDropDeathStart,
    _madChaserDeathRequestSettle,
    _madChaserCommandDeathWaitAnimBoundary,
    _madChaserDropBodies,
    _madChaserDropDeathHold,
} };

/// Seven state handlers, indexed by `MadChaserWork::state`; copied to
/// the stack before dispatch.
static const TaskFuncTable7 gMadChaserShrinkDeathStates = { {
    _madChaserShrinkDeathStart,
    _madChaserDeathRequestSettle,
    _madChaserCommandDeathWaitAnimBoundary,
    _madChaserBeginShrink,
    _madChaserShrinkDeathTurnTranslucent,
    madChaserShrink,
    _madChaserShrinkDeathStartDespawn,
} };

#include "../../shared/mad_chaser_lurk_look.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_combat.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_right.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_left.inc.c"

#include "../../shared/mad_chaser_emerge_tick.inc.c"

/// `_madChaserSetBehaviorStateS16` with an `s16` state. The narrower parameter is load-bearing:
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

/// Selects the carrier's static void(Task*) creep handler for emerge behavior 9.
#define MAD_CHASER_EMERGE_CREEP_HANDLER _madChaserEmergeCreep9
#include "../../shared/mad_chaser_creep.inc.c"

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

/// Selects this carrier's static void(Task*) task-state-5 dispatcher.
#define MAD_CHASER_DESPAWN_STATE _madChaserDespawnState
#include "../../shared/mad_chaser_despawn_state.inc.c"
#undef MAD_CHASER_DESPAWN_STATE

#include "../../shared/mad_chaser_turn_to_player.inc.c"

#include "../../shared/mad_chaser_to_alert.inc.c"

/// Selects the static void(Task*) alert transition for combat table slot 1.
#define MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER _madChaserCombatToAlertState1
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER

/// Selects the static void(Task*) alert transition for combat table slot 2.
#define MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER _madChaserCombatToAlertState2
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER

/// Walk-family interrupt binding: a declared s16(Task*) predicate, called once
/// before sub-state dispatch; nonzero skips that dispatch. Undefine after inclusion.
#define MAD_CHASER_STEP_STATE             _madChaserWalkState
#define MAD_CHASER_WALK_INTERRUPT_HANDLER _madChaserTakeHitRequest
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_WALK_INTERRUPT_HANDLER

#include "../../shared/mad_chaser_leap_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserLeapState  func_actor_342400_80169A98
#define gMadChaserLeapSteps D_actor_342400_80161F00
#include "../../shared/mad_chaser_leap_state.inc.c"
#undef madChaserLeapState
#undef gMadChaserLeapSteps

#include "../../shared/mad_chaser_recoil_light_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserRecoilLightState    func_actor_342400_80169B58
#define _madChaserRecoilLight        _madChaserRecoilHeavy
#define _madChaserRecoilLightRecover _madChaserRecoilHeavyEnd
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilLightState
#undef _madChaserRecoilLight
#undef _madChaserRecoilLightRecover

/// A further copy, under this file's own name.
#define madChaserRecoilLightState    func_actor_342400_80169BAC
#define _madChaserRecoilLight        _madChaserStatusHoldStart
#define _madChaserRecoilLightRecover _madChaserStatusHold
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilLightState
#undef _madChaserRecoilLight
#undef _madChaserRecoilLightRecover

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

/// Runs the lurk idle hold, yielding to a claimed shared alert.
///
/// Requires live Mad Chaser work and `subState` in 0..2: start the idle hold,
/// wait for its timeout or player proximity, then wait to enter the look state.
#define MAD_CHASER_STEP_STATE _madChaserLurkIdleState
#define gMadChaserWalkSteps   D_actor_342400_80161FB4
// This lurk instance joins a shared alert instead of consuming a hit reaction.
#define MAD_CHASER_WALK_INTERRUPT_HANDLER _madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef gMadChaserWalkSteps
#undef MAD_CHASER_WALK_INTERRUPT_HANDLER

/// Runs the lurk look sequence, yielding to a claimed shared alert.
///
/// Requires live Mad Chaser work and `subState` in 0..2: prepare the transition,
/// start a timed look hold, then sway or face the player until rise or alert.
#define MAD_CHASER_STEP_STATE _madChaserLurkLookState
#define gMadChaserWalkSteps   D_actor_342400_80161FC0
// This lurk instance joins a shared alert instead of consuming a hit reaction.
#define MAD_CHASER_WALK_INTERRUPT_HANDLER _madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef gMadChaserWalkSteps
#undef MAD_CHASER_WALK_INTERRUPT_HANDLER

#include "../../shared/mad_chaser_lurk_rise_state.inc.c"

#include "../../shared/mad_chaser_lurk_alert_state.inc.c"

/// Selects the private lurk-shift dispatcher for the next four-step fragment.
///
/// Names a declared static void(Task*) callback; the fragment consumes and
/// undefines this identifier binding. The accompanying table supplies the
/// opening clip, right-step start, right move and left return in that order.
#define MAD_CHASER_FOUR_STEP_STATE _madChaserLurkShiftState
/// Supplies the lurk shift's complete four-phase `TaskFuncTable4` value.
///
/// The fragment copies it and undefines this object-identifier binding.
#define MAD_CHASER_FOUR_STEP_HANDLERS _gMadChaserLurkShiftSteps
#include "../../shared/mad_chaser_dangle_state.inc.c"

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

/// Selects the private command-death animation wait instance.
///
/// Must name a statically declared void(Task*) callback. The following fragment
/// include consumes this identifier-only binding; undefine it afterwards so the
/// ordinary death instance remains `_madChaserDeathWaitAnim`.
#define MAD_CHASER_DEATH_WAIT_ANIM_HANDLER _madChaserCommandDeathWaitAnimBoundary
#include "../../shared/mad_chaser_death_wait_anim.inc.c"
#undef MAD_CHASER_DEATH_WAIT_ANIM_HANDLER

#include "../../shared/mad_chaser_drop_bodies.inc.c"

/// Holds the command drop-death corpse after its collision bodies are detached.
///
/// Final behavior 4 leaves the task and model alive. The enclosing death tick
/// continues drawing, colouring and spawning periodic blasts; unusedTask is
/// retained for state-table dispatch.
static void _madChaserDropDeathHold(Task* unusedTask)
{
}

/// Selects the private first state of scripted shrink death.
///
/// Names a statically declared void(Task*) callback for the following fragment;
/// undefine it afterwards. The unbound fragment supplies the drop-death entry.
#define MAD_CHASER_SHRINK_DEATH_START_HANDLER _madChaserShrinkDeathStart
#include "../../shared/mad_chaser_death_cry_unlink.inc.c"
#undef MAD_CHASER_SHRINK_DEATH_START_HANDLER

#include "../../shared/mad_chaser_begin_shrink.inc.c"

/// Selects the private translucency-delay state of scripted shrink death.
///
/// Names a statically declared void(Task*) callback for the following fragment.
/// This identifier-only binding captures no runtime values; undefine it after
/// inclusion to restore the ordinary-death definition.
#define MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER _madChaserShrinkDeathTurnTranslucent
#include "../../shared/mad_chaser_death_translucent.inc.c"
#undef MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER

#include "../../shared/mad_chaser_shrink.inc.c"

/// Selects the private final behavior of scripted shrink death.
///
/// Names a statically declared void(Task*) callback for the following fragment.
/// This identifier-only binding captures no runtime values; undefine it after
/// inclusion to restore the ordinary-death definition.
#define MAD_CHASER_SHRINK_DEATH_DESPAWN_HANDLER _madChaserShrinkDeathStartDespawn
#include "../../shared/mad_chaser_start_despawn.inc.c"
#undef MAD_CHASER_SHRINK_DEATH_DESPAWN_HANDLER

#include "../../shared/mad_chaser_take_knockdown_request.inc.c"
