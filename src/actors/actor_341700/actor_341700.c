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

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

extern EnemyParams gMadChaserEnemyParams; // the main enemy's `Enemy::param` record

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void _madChaserLurkTick(Task* task);
static void _madChaserCombatTick(Task* task);
static void _madChaserDangleFrame(Task* task);
static void _madChaserEmergeTick(Task* task);
static void _madChaserEmergeCreep3(Task* task);
static void _madChaserSpawn(Task* task);
static void _madChaserEmergeAtSpot(Task* task);
static void _madChaserLeapState(Task* task);
static void _madChaserShrink(Task* task);
static void _madChaserStartDespawn(Task* task);
static void _madChaserCombatToAlertState0(Task* task);
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
static void _madChaserAlertState(Task* task);
static void _madChaserRecoilLightState(Task* task);
static void _madChaserRecoilHeavyState(Task* task);
static void _madChaserStatusHoldState(Task* task);
static void _madChaserLurkIdleState(Task* task);
static void _madChaserLurkLookState(Task* task);
static void _madChaserLurkShiftState(Task* task);
static void _madChaserCommandDeathWaitAnimBoundary(Task* task);
static void _madChaserDropDeathHold(Task* unusedTask);
static void _madChaserShrinkDeathStart(Task* task);
static void _madChaserShrinkDeathTurnTranslucent(Task* task);
static void _madChaserShrinkDeathStartDespawn(Task* task);

/// Task states of an ordinary Mad Chaser.
///
/// `Task::state` selects 0 spawn, 1 lurk, 2 dangle, 3 combat, 4 death, 5 despawn.
/// The dispatcher copies all 6 non-NULL void(Task*) callbacks before the call.
/// Their code and state-specific task storage must stay live through dispatch.
static const TaskFuncTable6 _gMadChaserTaskStates = { {
    _madChaserSpawn,
    _madChaserLurkTick,
    _madChaserDangleFrame,
    _madChaserCombatTick,
    _madChaserDeathTick,
    _madChaserDespawnState,
} };

/// Task states of a Mad Chaser with scripted emergence and death.
///
/// `Task::state` selects 0 hidden spawn, 1 lurk, 2 dangle, 3 combat, 4 death, 5 despawn, 6
/// emerge, 7 vanish, 8 drop death, 9 shrink death.
/// The dispatcher copies all 10 non-NULL void(Task*) callbacks before the call.
/// Their code and state-specific task storage must stay live through dispatch.
static const TaskFuncTable10 _gMadChaserHiddenTaskStates = { {
    _madChaserSpawnHidden,
    _madChaserLurkTick,
    _madChaserDangleFrame,
    _madChaserCombatTick,
    _madChaserDeathTick,
    _madChaserDespawnState,
    _madChaserEmergeTick,
    _madChaserVanishState,
    _madChaserDropDeathTick,
    _madChaserShrinkDeathTick,
} };

/// Combat behaviors of the Mad Chaser.
///
/// `MadChaserWork::state` selects 0..2 enter alert, 3 walk, 4 leap, 5 alert, 6 light
/// recoil, 7 heavy recoil, 8 status hold, 9 knockdown, 10 pull.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable11 _gMadChaserCombatStates = { {
    _madChaserCombatToAlertState0,
    _madChaserCombatToAlertState1,
    _madChaserCombatToAlertState2,
    _madChaserWalkState,
    _madChaserLeapState,
    _madChaserAlertState,
    _madChaserRecoilLightState,
    _madChaserRecoilHeavyState,
    _madChaserStatusHoldState,
    _madChaserKnockdownState,
    _madChaserPullState,
} };

/// Phases of knockdown and stance-dependent recovery.
///
/// `MadChaserWork::subState` selects 0 start knockdown, 1 rise, 2 finish recovery.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable3 _gMadChaserKnockdownSteps = { {
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

/// Phases of the leap attack and return to walking.
///
/// `MadChaserWork::subState` selects 0 windup, 1 lunge, 2 turn away, 3 rebound, 4 land.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable5 _gMadChaserLeapSteps = { {
    _madChaserStartLeap,
    _madChaserLeapAttack,
    _madChaserLeapTurnAway,
    _madChaserLeapRebound,
    _madChaserLeapLand,
} };

/// Sub-state handlers `_madChaserAlertState` dispatches by `subState`.
static const TaskFuncTable5 gMadChaserAlertSteps = { {
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

static TmdSource _gActor341700MadChaserBody;

static TmdBone _gActor341700MadChaserBurstHeadSkeleton[1] = {
#include "assets/mad_chaser_burst_head_skeleton.inc"
};

static u32 _gActor341700MadChaserBurstHeadPartVerts[1] = {
#include "assets/mad_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor341700MadChaserBurstHeadVerts[48] = {
#include "assets/mad_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor341700MadChaserBurstHeadNormals[53] = {
#include "assets/mad_chaser_burst_head_normals.inc"
};

static u32 _gActor341700MadChaserBurstHeadStream[486] = {
#include "assets/mad_chaser_burst_head_stream.inc"
};

TmdSource gMadChaserChunkModel0 = {
    0,
    3260,
    0,
    1,
    _gActor341700MadChaserBurstHeadPartVerts,
    _gActor341700MadChaserBurstHeadVerts,
    _gActor341700MadChaserBurstHeadNormals,
    _gActor341700MadChaserBurstHeadSkeleton,
    _gActor341700MadChaserBurstHeadStream,
};

static TmdBone _gActor341700MadChaserBurstArmSkeleton[1] = {
#include "assets/mad_chaser_burst_arm_skeleton.inc"
};

static u32 _gActor341700MadChaserBurstArmPartVerts[1] = {
#include "assets/mad_chaser_burst_arm_partVerts.inc"
};

static SVECTOR _gActor341700MadChaserBurstArmVerts[28] = {
#include "assets/mad_chaser_burst_arm_verts.inc"
};

static SVECTOR _gActor341700MadChaserBurstArmNormals[37] = {
#include "assets/mad_chaser_burst_arm_normals.inc"
};

static u32 _gActor341700MadChaserBurstArmStream[276] = {
#include "assets/mad_chaser_burst_arm_stream.inc"
};

TmdSource gMadChaserChunkModel1 = {
    0,
    1828,
    0,
    1,
    _gActor341700MadChaserBurstArmPartVerts,
    _gActor341700MadChaserBurstArmVerts,
    _gActor341700MadChaserBurstArmNormals,
    _gActor341700MadChaserBurstArmSkeleton,
    _gActor341700MadChaserBurstArmStream,
};

static TmdBone _gActor341700MadChaserBurstTailSkeleton[1] = {
#include "assets/mad_chaser_burst_tail_skeleton.inc"
};

static u32 _gActor341700MadChaserBurstTailPartVerts[1] = {
#include "assets/mad_chaser_burst_tail_partVerts.inc"
};

static SVECTOR _gActor341700MadChaserBurstTailVerts[25] = {
#include "assets/mad_chaser_burst_tail_verts.inc"
};

static SVECTOR _gActor341700MadChaserBurstTailNormals[32] = {
#include "assets/mad_chaser_burst_tail_normals.inc"
};

static u32 _gActor341700MadChaserBurstTailStream[215] = {
#include "assets/mad_chaser_burst_tail_stream.inc"
};

TmdSource gMadChaserChunkModel2 = {
    0,
    1448,
    0,
    1,
    _gActor341700MadChaserBurstTailPartVerts,
    _gActor341700MadChaserBurstTailVerts,
    _gActor341700MadChaserBurstTailNormals,
    _gActor341700MadChaserBurstTailSkeleton,
    _gActor341700MadChaserBurstTailStream,
};

static TmdBone _gActor341700MadChaserBodySkeleton[9] = {
#include "assets/mad_chaser_body_skeleton.inc"
};

static u32 _gActor341700MadChaserBodyPartVerts[9] = {
#include "assets/mad_chaser_body_partVerts.inc"
};

static SVECTOR _gActor341700MadChaserBodyVerts[160] = {
#include "assets/mad_chaser_body_verts.inc"
};

static SVECTOR _gActor341700MadChaserBodyNormals[206] = {
#include "assets/mad_chaser_body_normals.inc"
};

static u32 _gActor341700MadChaserBodyStream[2105] = {
#include "assets/mad_chaser_body_stream.inc"
};

static TmdSource _gActor341700MadChaserBody = {
    0,
    11212,
    3088,
    9,
    _gActor341700MadChaserBodyPartVerts,
    _gActor341700MadChaserBodyVerts,
    _gActor341700MadChaserBodyNormals,
    _gActor341700MadChaserBodySkeleton,
    _gActor341700MadChaserBodyStream,
};

DamageAttack D_actor_341700_80171888[1] = {
    { 22, 0 },
};

EnemyParams gMadChaserEnemyParams = { D_actor_341700_80171888, 110, 20, 40, 1, 100, 10, 100, 0 };

static AnimationPackedPose _gActor341700Animation0FCA8Bank1[6] = {
#include "assets/actor_341700_animation_0FCA8_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation0FCA8Bank4[39] = {
#include "assets/actor_341700_animation_0FCA8_bank4.inc"
};

static AnimationRecord _gActor341700Animation0FCA8Records[77] = {
#include "assets/actor_341700_animation_0FCA8_records.inc"
};

static u16 _gActor341700Animation0FCA8Indices[10] = {
#include "assets/actor_341700_animation_0FCA8_indices.inc"
};

static AnimationSet _gActor341700Animation0FCA8 = {
    _gActor341700Animation0FCA8Records,
    _gActor341700Animation0FCA8Indices,
    { NULL, _gActor341700Animation0FCA8Bank1, NULL, NULL, _gActor341700Animation0FCA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation0FE4CBank1[3] = {
#include "assets/actor_341700_animation_0FE4C_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation0FE4CBank4[21] = {
#include "assets/actor_341700_animation_0FE4C_bank4.inc"
};

static AnimationRecord _gActor341700Animation0FE4CRecords[60] = {
#include "assets/actor_341700_animation_0FE4C_records.inc"
};

static u16 _gActor341700Animation0FE4CIndices[10] = {
#include "assets/actor_341700_animation_0FE4C_indices.inc"
};

static AnimationSet _gActor341700Animation0FE4C = {
    _gActor341700Animation0FE4CRecords,
    _gActor341700Animation0FE4CIndices,
    { NULL, _gActor341700Animation0FE4CBank1, NULL, NULL, _gActor341700Animation0FE4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation10320Bank1[20] = {
#include "assets/actor_341700_animation_10320_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation10320Bank4[99] = {
#include "assets/actor_341700_animation_10320_bank4.inc"
};

static AnimationRecord _gActor341700Animation10320Records[135] = {
#include "assets/actor_341700_animation_10320_records.inc"
};

static u16 _gActor341700Animation10320Indices[10] = {
#include "assets/actor_341700_animation_10320_indices.inc"
};

static AnimationSet _gActor341700Animation10320 = {
    _gActor341700Animation10320Records,
    _gActor341700Animation10320Indices,
    { NULL, _gActor341700Animation10320Bank1, NULL, NULL, _gActor341700Animation10320Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation108A4Bank1[25] = {
#include "assets/actor_341700_animation_108A4_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation108A4Bank4[110] = {
#include "assets/actor_341700_animation_108A4_bank4.inc"
};

static AnimationRecord _gActor341700Animation108A4Records[153] = {
#include "assets/actor_341700_animation_108A4_records.inc"
};

static u16 _gActor341700Animation108A4Indices[10] = {
#include "assets/actor_341700_animation_108A4_indices.inc"
};

static AnimationSet _gActor341700Animation108A4 = {
    _gActor341700Animation108A4Records,
    _gActor341700Animation108A4Indices,
    { NULL, _gActor341700Animation108A4Bank1, NULL, NULL, _gActor341700Animation108A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation109A4Bank1[2] = {
#include "assets/actor_341700_animation_109A4_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation109A4Bank4[7] = {
#include "assets/actor_341700_animation_109A4_bank4.inc"
};

static AnimationRecord _gActor341700Animation109A4Records[36] = {
#include "assets/actor_341700_animation_109A4_records.inc"
};

static u16 _gActor341700Animation109A4Indices[10] = {
#include "assets/actor_341700_animation_109A4_indices.inc"
};

static AnimationSet _gActor341700Animation109A4 = {
    _gActor341700Animation109A4Records,
    _gActor341700Animation109A4Indices,
    { NULL, _gActor341700Animation109A4Bank1, NULL, NULL, _gActor341700Animation109A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation10AA4Bank1[2] = {
#include "assets/actor_341700_animation_10AA4_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation10AA4Bank4[7] = {
#include "assets/actor_341700_animation_10AA4_bank4.inc"
};

static AnimationRecord _gActor341700Animation10AA4Records[36] = {
#include "assets/actor_341700_animation_10AA4_records.inc"
};

static u16 _gActor341700Animation10AA4Indices[10] = {
#include "assets/actor_341700_animation_10AA4_indices.inc"
};

static AnimationSet _gActor341700Animation10AA4 = {
    _gActor341700Animation10AA4Records,
    _gActor341700Animation10AA4Indices,
    { NULL, _gActor341700Animation10AA4Bank1, NULL, NULL, _gActor341700Animation10AA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation10ED4Bank1[24] = {
#include "assets/actor_341700_animation_10ED4_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation10ED4Bank4[69] = {
#include "assets/actor_341700_animation_10ED4_bank4.inc"
};

static AnimationRecord _gActor341700Animation10ED4Records[112] = {
#include "assets/actor_341700_animation_10ED4_records.inc"
};

static u16 _gActor341700Animation10ED4Indices[10] = {
#include "assets/actor_341700_animation_10ED4_indices.inc"
};

static AnimationSet _gActor341700Animation10ED4 = {
    _gActor341700Animation10ED4Records,
    _gActor341700Animation10ED4Indices,
    { NULL, _gActor341700Animation10ED4Bank1, NULL, NULL, _gActor341700Animation10ED4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation11378Bank1[22] = {
#include "assets/actor_341700_animation_11378_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation11378Bank4[87] = {
#include "assets/actor_341700_animation_11378_bank4.inc"
};

static AnimationRecord _gActor341700Animation11378Records[129] = {
#include "assets/actor_341700_animation_11378_records.inc"
};

static u16 _gActor341700Animation11378Indices[10] = {
#include "assets/actor_341700_animation_11378_indices.inc"
};

static AnimationSet _gActor341700Animation11378 = {
    _gActor341700Animation11378Records,
    _gActor341700Animation11378Indices,
    { NULL, _gActor341700Animation11378Bank1, NULL, NULL, _gActor341700Animation11378Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation118D0Bank1[14] = {
#include "assets/actor_341700_animation_118D0_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation118D0Bank4[99] = {
#include "assets/actor_341700_animation_118D0_bank4.inc"
};

static AnimationRecord _gActor341700Animation118D0Records[186] = {
#include "assets/actor_341700_animation_118D0_records.inc"
};

static u16 _gActor341700Animation118D0Indices[10] = {
#include "assets/actor_341700_animation_118D0_indices.inc"
};

static AnimationSet _gActor341700Animation118D0 = {
    _gActor341700Animation118D0Records,
    _gActor341700Animation118D0Indices,
    { NULL, _gActor341700Animation118D0Bank1, NULL, NULL, _gActor341700Animation118D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation11B84Bank1[8] = {
#include "assets/actor_341700_animation_11B84_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation11B84Bank4[54] = {
#include "assets/actor_341700_animation_11B84_bank4.inc"
};

static AnimationRecord _gActor341700Animation11B84Records[80] = {
#include "assets/actor_341700_animation_11B84_records.inc"
};

static u16 _gActor341700Animation11B84Indices[10] = {
#include "assets/actor_341700_animation_11B84_indices.inc"
};

static AnimationSet _gActor341700Animation11B84 = {
    _gActor341700Animation11B84Records,
    _gActor341700Animation11B84Indices,
    { NULL, _gActor341700Animation11B84Bank1, NULL, NULL, _gActor341700Animation11B84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation11E18Bank1[7] = {
#include "assets/actor_341700_animation_11E18_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation11E18Bank4[50] = {
#include "assets/actor_341700_animation_11E18_bank4.inc"
};

static AnimationRecord _gActor341700Animation11E18Records[79] = {
#include "assets/actor_341700_animation_11E18_records.inc"
};

static u16 _gActor341700Animation11E18Indices[10] = {
#include "assets/actor_341700_animation_11E18_indices.inc"
};

static AnimationSet _gActor341700Animation11E18 = {
    _gActor341700Animation11E18Records,
    _gActor341700Animation11E18Indices,
    { NULL, _gActor341700Animation11E18Bank1, NULL, NULL, _gActor341700Animation11E18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation120C8Bank1[7] = {
#include "assets/actor_341700_animation_120C8_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation120C8Bank4[53] = {
#include "assets/actor_341700_animation_120C8_bank4.inc"
};

static AnimationRecord _gActor341700Animation120C8Records[83] = {
#include "assets/actor_341700_animation_120C8_records.inc"
};

static u16 _gActor341700Animation120C8Indices[10] = {
#include "assets/actor_341700_animation_120C8_indices.inc"
};

static AnimationSet _gActor341700Animation120C8 = {
    _gActor341700Animation120C8Records,
    _gActor341700Animation120C8Indices,
    { NULL, _gActor341700Animation120C8Bank1, NULL, NULL, _gActor341700Animation120C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation122DCBank1[6] = {
#include "assets/actor_341700_animation_122DC_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation122DCBank4[42] = {
#include "assets/actor_341700_animation_122DC_bank4.inc"
};

static AnimationRecord _gActor341700Animation122DCRecords[58] = {
#include "assets/actor_341700_animation_122DC_records.inc"
};

static u16 _gActor341700Animation122DCIndices[10] = {
#include "assets/actor_341700_animation_122DC_indices.inc"
};

static AnimationSet _gActor341700Animation122DC = {
    _gActor341700Animation122DCRecords,
    _gActor341700Animation122DCIndices,
    { NULL, _gActor341700Animation122DCBank1, NULL, NULL, _gActor341700Animation122DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation123FCBank1[2] = {
#include "assets/actor_341700_animation_123FC_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation123FCBank4[11] = {
#include "assets/actor_341700_animation_123FC_bank4.inc"
};

static AnimationRecord _gActor341700Animation123FCRecords[40] = {
#include "assets/actor_341700_animation_123FC_records.inc"
};

static u16 _gActor341700Animation123FCIndices[10] = {
#include "assets/actor_341700_animation_123FC_indices.inc"
};

static AnimationSet _gActor341700Animation123FC = {
    _gActor341700Animation123FCRecords,
    _gActor341700Animation123FCIndices,
    { NULL, _gActor341700Animation123FCBank1, NULL, NULL, _gActor341700Animation123FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation12538Bank1[4] = {
#include "assets/actor_341700_animation_12538_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation12538Bank4[18] = {
#include "assets/actor_341700_animation_12538_bank4.inc"
};

static AnimationRecord _gActor341700Animation12538Records[34] = {
#include "assets/actor_341700_animation_12538_records.inc"
};

static u16 _gActor341700Animation12538Indices[10] = {
#include "assets/actor_341700_animation_12538_indices.inc"
};

static AnimationSet _gActor341700Animation12538 = {
    _gActor341700Animation12538Records,
    _gActor341700Animation12538Indices,
    { NULL, _gActor341700Animation12538Bank1, NULL, NULL, _gActor341700Animation12538Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation12734Bank1[10] = {
#include "assets/actor_341700_animation_12734_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation12734Bank4[31] = {
#include "assets/actor_341700_animation_12734_bank4.inc"
};

static AnimationRecord _gActor341700Animation12734Records[51] = {
#include "assets/actor_341700_animation_12734_records.inc"
};

static u16 _gActor341700Animation12734Indices[10] = {
#include "assets/actor_341700_animation_12734_indices.inc"
};

static AnimationSet _gActor341700Animation12734 = {
    _gActor341700Animation12734Records,
    _gActor341700Animation12734Indices,
    { NULL, _gActor341700Animation12734Bank1, NULL, NULL, _gActor341700Animation12734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation12B70Bank1[16] = {
#include "assets/actor_341700_animation_12B70_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation12B70Bank4[83] = {
#include "assets/actor_341700_animation_12B70_bank4.inc"
};

static AnimationRecord _gActor341700Animation12B70Records[125] = {
#include "assets/actor_341700_animation_12B70_records.inc"
};

static u16 _gActor341700Animation12B70Indices[10] = {
#include "assets/actor_341700_animation_12B70_indices.inc"
};

static AnimationSet _gActor341700Animation12B70 = {
    _gActor341700Animation12B70Records,
    _gActor341700Animation12B70Indices,
    { NULL, _gActor341700Animation12B70Bank1, NULL, NULL, _gActor341700Animation12B70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation12CFCBank1[5] = {
#include "assets/actor_341700_animation_12CFC_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation12CFCBank4[26] = {
#include "assets/actor_341700_animation_12CFC_bank4.inc"
};

static AnimationRecord _gActor341700Animation12CFCRecords[43] = {
#include "assets/actor_341700_animation_12CFC_records.inc"
};

static u16 _gActor341700Animation12CFCIndices[10] = {
#include "assets/actor_341700_animation_12CFC_indices.inc"
};

static AnimationSet _gActor341700Animation12CFC = {
    _gActor341700Animation12CFCRecords,
    _gActor341700Animation12CFCIndices,
    { NULL, _gActor341700Animation12CFCBank1, NULL, NULL, _gActor341700Animation12CFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor341700Animation12EA4Bank1[5] = {
#include "assets/actor_341700_animation_12EA4_bank1.inc"
};

static AnimationPackedRotation _gActor341700Animation12EA4Bank4[30] = {
#include "assets/actor_341700_animation_12EA4_bank4.inc"
};

static AnimationRecord _gActor341700Animation12EA4Records[46] = {
#include "assets/actor_341700_animation_12EA4_records.inc"
};

static u16 _gActor341700Animation12EA4Indices[10] = {
#include "assets/actor_341700_animation_12EA4_indices.inc"
};

static AnimationSet _gActor341700Animation12EA4 = {
    _gActor341700Animation12EA4Records,
    _gActor341700Animation12EA4Indices,
    { NULL, _gActor341700Animation12EA4Bank1, NULL, NULL, _gActor341700Animation12EA4Bank4, NULL, NULL, NULL },
};

/// Loaded animation sets addressed directly by the one-based clip ID.
///
/// Entries 1..19 are playable; entries 0 and 20 are NULL, not clips. The
/// animation context borrows the pointer table and all tracks until teardown.
static AnimationSet* _gMadChaserAnimBank[21] = {
    NULL,
    &_gActor341700Animation0FCA8,
    &_gActor341700Animation0FE4C,
    &_gActor341700Animation10320,
    &_gActor341700Animation108A4,
    &_gActor341700Animation109A4,
    &_gActor341700Animation10AA4,
    &_gActor341700Animation10ED4,
    &_gActor341700Animation11378,
    &_gActor341700Animation118D0,
    &_gActor341700Animation11B84,
    &_gActor341700Animation11E18,
    &_gActor341700Animation120C8,
    &_gActor341700Animation122DC,
    &_gActor341700Animation123FC,
    &_gActor341700Animation12538,
    &_gActor341700Animation12734,
    &_gActor341700Animation12B70,
    &_gActor341700Animation12CFC,
    &_gActor341700Animation12EA4,
    NULL,
};

/// Placement and scripted-command messages accepted by either Mad Chaser form.
///
/// PLACE borrows an ActorTransform; APPLY borrows an ActorCommand and queues
/// it for the state machine. The third entry terminates lookup. Tasks borrow
/// this table and its callbacks while the carrier is loaded. Both handlers
/// return void; senders must ignore the dispatch result.
static TaskMessageEntry _gMadChaserMsgTable[3] = {
    { ACTOR_MESSAGE_PLACE, _madChaserPlaceRoot },
    { ACTOR_COMMAND_MESSAGE_APPLY, _madChaserQueueCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_341700_80174D58 = { { { TASK_BODY_TMD, 96 } }, _madChaserTask, { .model = &_gActor341700MadChaserBody } };

TaskDesc D_actor_341700_80174D64 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _madChaserHiddenTask, { .model = &_gActor341700MadChaserBody } };

TaskDesc D_actor_341700_80174D70 = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_341700_80174D7C = { { { TASK_BODY_TMD, 96 } }, _madChaserHiddenTask, { .model = &_gActor341700MadChaserBody } };

/// Interrupted-clip stance used by recoil, knockdown and pull selection.
///
/// Index clipId - 1 for playable clips 1..19; values are MAD_CHASER_STANCE_LOW
/// or MAD_CHASER_STANCE_UPRIGHT. The final zero byte is never selected by the
/// known clip domain; its original storage role is unproven.
static u8 _gMadChaserAnimStance[20] = {
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_UPRIGHT,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_LOW,
    MAD_CHASER_STANCE_UPRIGHT,
    0,
};

/// Settle clip selected when death or a pull interrupts the current clip.
///
/// Index clipId - 1 for clips 1..19; each selects low clip 5 or upright clip 6.
/// Clip 8 is overridden by the caller: low before the first leap, upright
/// afterwards. The mapping and clip bank remain borrowed from the carrier.
static u8 _gMadChaserSettleAnims[19] = {
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
    MAD_CHASER_SETTLE_UPRIGHT_CLIP,
    MAD_CHASER_SETTLE_LOW_CLIP,
};

/// Unreferenced bytes before this package's prop geometry; their role is unproven.
static u8 _gActor341700Unknown12F90[20] = { 2, 70, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 0, 62, 200, 210, 1 };

extern void* D_800678F0[1];

extern TmdSource gMadChaserChunkModel0;

extern TmdSource gMadChaserChunkModel1;

extern TmdSource gMadChaserChunkModel2;

static __inline__ void _madChaserResetBehaviorStateS16(Task* task, s16 behaviorState);

#include "../../shared/mad_chaser_limb_shadow.inc.c"

/* `D_800678F0` selects the model stream the next `effectSpawn` copies into
 * its effect's `TmdObject`. It is declared as a one-element array for the
 * same reason as in `actor_400500`: as a bare scalar, GCC 2.8.1 decides the
 * store cannot alias the `TmdObject` loads and sinks it past them. */

/* The records closing three of the overlay's model streams, selected through
   `D_800678F0`. */

#include "../../shared/mad_chaser_spawn_gibs.inc.c"

#include "../../shared/mad_chaser_twist.inc.c"

#include "../../shared/mad_chaser_inlines.inc.c"

#include "../../shared/mad_chaser_spawn.inc.c"

#include "../../shared/mad_chaser_spawn_hidden.inc.c"

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

/// Behaviors of ordinary death, including the alternate blast-burst path.
///
/// `MadChaserWork::state` selects 0 release the target, 1 settle, 2 wait for animation, 3
/// start shrink, 4 turn translucent, 5 shrink with burn, 6 start despawn, 7 pause, 8 burst.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable9 _gMadChaserDeathStates = { {
    _madChaserDeathReleaseTarget,
    _madChaserDeathSettle,
    _madChaserDeathWaitAnim,
    _madChaserDeathStartShrink,
    _madChaserDeathTurnTranslucent,
    _madChaserShrinkWithBurn,
    _madChaserStartDespawn,
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

#define MAD_CHASER_LURK_STATE_HANDLERS gMadChaserLurkStates
#include "../../shared/mad_chaser_lurk_tick.inc.c"

/// Sub-state handlers `_madChaserLurkIdleState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkHoldSteps = { {
    _madChaserLurkStartIdleHold,
    _madChaserLurkWait,
    _madChaserLurkIdleEnd,
} };

/// Sub-state handlers `_madChaserLurkLookState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkCrouchSteps = { {
    _madChaserLurkPrepareLook,
    _madChaserLurkStartLookHold,
    _madChaserLurkLookAround,
} };

/// Phases of the lurking alert and sidestep into combat.
///
/// `MadChaserWork::subState` selects 0 start alert, 1 start the sidestep, 2 sidestep into
/// combat.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable3 _gMadChaserLurkAlertSteps = { {
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

/// Behaviors of scripted placement and emergence.
///
/// `MadChaserWork::state` selects 0 place at the room spot, 1 backflip, 2 hop forward, 3
/// creep, 4 arc back, 5 hop back, 6 back off, 7 high arc, 8 flip over, 9 creep.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable10 _gMadChaserEmergeStates = { {
    _madChaserEmergeAtSpot,
    _madChaserEmergeBackflip,
    _madChaserEmergeHopForward,
    _madChaserEmergeCreep3,
    _madChaserEmergeArcBack,
    _madChaserEmergeHopBack,
    _madChaserEmergeBackOff,
    _madChaserEmergeHighArc,
    _madChaserEmergeFlipOver,
    _madChaserEmergeCreep9,
} };

/// Phases of a scripted pull toward the room capture point.
///
/// `MadChaserWork::subState` selects 0 start, 1 select reaction, 2 resist the pull, 3
/// capture, 4 limp pull, 5 capture.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable6 _gMadChaserPullSteps = { {
    _madChaserPullStart,
    _madChaserPullReact,
    _madChaserPulledStruggle,
    _madChaserPulledIn,
    _madChaserPulledLimp,
    _madChaserPulledIn,
} };

/// Behaviors of room-command death that leaves the body in place.
///
/// `MadChaserWork::state` selects 0 release rewards and alert ownership, 1 settle, 2 wait
/// for animation, 3 unlink collision bodies, 4 hold the corpse.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable5 _gMadChaserDropDeathStates = { {
    _madChaserDropDeathStart,
    _madChaserDeathRequestSettle,
    _madChaserCommandDeathWaitAnimBoundary,
    _madChaserDropBodies,
    _madChaserDropDeathHold,
} };

/// Behaviors of room-command death that shrinks and removes the body.
///
/// `MadChaserWork::state` selects 0 release rewards and alert ownership, 1 settle, 2 wait
/// for animation, 3 start shrink, 4 turn translucent, 5 shrink, 6 start despawn.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable7 _gMadChaserShrinkDeathStates = { {
    _madChaserShrinkDeathStart,
    _madChaserDeathRequestSettle,
    _madChaserCommandDeathWaitAnimBoundary,
    _madChaserBeginShrink,
    _madChaserShrinkDeathTurnTranslucent,
    _madChaserShrink,
    _madChaserShrinkDeathStartDespawn,
} };

#include "../../shared/mad_chaser_lurk_look.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_combat.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_right.inc.c"

#include "../../shared/mad_chaser_lurk_sidestep_left.inc.c"

#include "../../shared/mad_chaser_emerge_tick.inc.c"

/// Selects a signed-halfword behavior index and resets its sub-state to the start.
///
/// Requires live MadChaserWork and an index valid for the active behavior table.
/// Retains the task state, animation and counters. This local copy has no users;
/// active transitions use the shared `_madChaserSetBehaviorStateS16` helper.
static __inline__ void _madChaserResetBehaviorStateS16(Task* task, s16 behaviorState)
{
    enum { MAD_CHASER_BEHAVIOR_START_SUBSTATE = 0 };
    MadChaserWork* work = task->work;

    work->state    = behaviorState;
    work->subState = MAD_CHASER_BEHAVIOR_START_SUBSTATE;
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

/// Selects this carrier's declared static void(Task*) combat-alert dispatcher.
#define MAD_CHASER_ALERT_STATE_HANDLER _madChaserAlertState
/// Supplies the complete TaskFuncTable5 of this carrier's alert phases.
#define MAD_CHASER_ALERT_STEP_HANDLERS gMadChaserAlertSteps
#include "../../shared/mad_chaser_alert_state.inc.c"
#undef MAD_CHASER_ALERT_STATE_HANDLER
#undef MAD_CHASER_ALERT_STEP_HANDLERS

/// Selects this carrier's declared static void(Task*) light-recoil dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserRecoilLightState
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER

/// Selects this carrier's declared static void(Task*) heavy-recoil dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserRecoilHeavyState
#define _madChaserRecoilLight             _madChaserRecoilHeavy
#define _madChaserRecoilLightRecover      _madChaserRecoilHeavyEnd
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER
#undef _madChaserRecoilLight
#undef _madChaserRecoilLightRecover

/// Selects this carrier's declared static void(Task*) buildup-status dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserStatusHoldState
#define _madChaserRecoilLight             _madChaserStatusHoldStart
#define _madChaserRecoilLightRecover      _madChaserStatusHold
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER
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
#define gMadChaserWalkSteps   gMadChaserLurkHoldSteps
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
#define gMadChaserWalkSteps   gMadChaserLurkCrouchSteps
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
