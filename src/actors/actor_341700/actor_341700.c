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

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

extern EnemyParams   gMadChaserEnemyParams;  // the main enemy's `Enemy::param` record
extern AnimationSet* gMadChaserAnimBank[21]; // animation bank handed to `animationInitContext`
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gMadChaserMsgTable[3];   // stored into `Task::msgTable` by madChaserSpawn
extern u8               gMadChaserAnimStance[];  // per animation id (1-based): value for `stateScratch`
extern u8               gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_341700_801670B0(Task* arg0);
static void func_actor_341700_8016859C(Task* arg0);
static void func_actor_341700_80168698(Task* arg0);
static void func_actor_341700_801686AC(Task* arg0);
static void func_actor_341700_801687B4(Task* arg0);
static void func_actor_341700_80168874(Task* arg0);
static void func_actor_341700_801688C8(Task* arg0);
static void func_actor_341700_80169B40(Task* arg0);
static void func_actor_341700_80169BC8(Task* arg0);
static void func_actor_341700_80169D54(Task* arg0);
static void func_actor_341700_8016A810(Task* arg0);
static void func_actor_341700_8016A8EC(Task* arg0);
static void func_actor_341700_8016A8F4(Task* arg0);
static void func_actor_341700_8016AA58(Task* arg0);
static void func_actor_341700_8016ABF4(Task* arg0);

/// Six task-state handlers of the first enemy form, dispatched by
/// `madChaserTask` on `Task::state`.
static const TaskFuncTable6 gMadChaserTaskStates = { {
    madChaserSpawn,
    madChaserLurkTick,
    madChaserDangleFrame,
    madChaserCombatTick,
    madChaserDeathTick,
    func_actor_341700_8016859C,
} };

/// Ten task-state handlers of the second enemy form, dispatched by
/// `madChaserHiddenTask` on `Task::state`.
static const TaskFuncTable10 gMadChaserHiddenTaskStates = { {
    madChaserSpawnHidden,
    madChaserLurkTick,
    madChaserDangleFrame,
    madChaserCombatTick,
    madChaserDeathTick,
    func_actor_341700_8016859C,
    madChaserEmergeTick,
    madChaserVanishState,
    madChaserDropDeathTick,
    madChaserShrinkDeathTick,
} };

/// Eleven state handlers, indexed by `MadChaserWork::state`; copied to
/// the stack before dispatch.
static const TaskFuncTable11 gMadChaserCombatStates = { {
    madChaserToAlertState,
    func_actor_341700_80168698,
    func_actor_341700_801686AC,
    madChaserWalkState,
    madChaserLeapState,
    func_actor_341700_801687B4,
    madChaserRecoilLightState,
    func_actor_341700_80168874,
    func_actor_341700_801688C8,
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

/// Sub-state handlers `func_actor_341700_801687B4` dispatches by `subState`.
static const TaskFuncTable5 gMadChaserAlertSteps = { {
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

AnimationSet* gMadChaserAnimBank[21] = {
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

TaskMessageEntry gMadChaserMsgTable[3] = {
    { ACTOR_MESSAGE_PLACE, madChaserMsgPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, madChaserCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_341700_80174D58 = { { { TASK_BODY_TMD, 96 } }, madChaserTask, { .model = &_gActor341700MadChaserBody } };

TaskDesc D_actor_341700_80174D64 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, madChaserHiddenTask, { .model = &_gActor341700MadChaserBody } };

TaskDesc D_actor_341700_80174D70 = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_341700_80174D7C = { { { TASK_BODY_TMD, 96 } }, madChaserHiddenTask, { .model = &_gActor341700MadChaserBody } };

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
    func_actor_341700_80169B40,
    func_actor_341700_80169BC8,
    madChaserLurkRiseState,
    madChaserLurkAlertState,
    func_actor_341700_80169D54,
} };

#include "../../shared/mad_chaser_lurk_tick.inc.c"

/// Sub-state handlers `func_actor_341700_80169B40` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkHoldSteps = { {
    madChaserStartHold,
    madChaserLurkWait,
    madChaserLurkIdleEnd,
} };

/// Sub-state handlers `func_actor_341700_80169BC8` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkCrouchSteps = { {
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

/// Sub-state handlers `func_actor_341700_80169D54` dispatches by `subState`.
static const TaskFuncTable4 gMadChaserLurkShiftSteps = { {
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
    func_actor_341700_801670B0,
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
    func_actor_341700_8016A810,
    madChaserDropBodies,
    func_actor_341700_8016A8EC,
} };

/// Seven state handlers, indexed by `MadChaserWork::state`; copied to
/// the stack before dispatch.
static const TaskFuncTable7 gMadChaserShrinkDeathStates = { {
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
#define madChaserCreepUntilHit func_actor_341700_801670B0
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
#define madChaserVanishState func_actor_341700_8016859C
#define madChaserVanish      madChaserAdvanceState
#define madChaserVanishFree  madChaserDespawn
#include "../../shared/mad_chaser_vanish_state.inc.c"
#undef madChaserVanishState
#undef madChaserVanish
#undef madChaserVanishFree

#include "../../shared/mad_chaser_turn_to_player.inc.c"

#include "../../shared/mad_chaser_to_alert.inc.c"

/// A further copy, under this file's own name.
#define madChaserToAlertState func_actor_341700_80168698
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef madChaserToAlertState

/// A further copy, under this file's own name.
#define madChaserToAlertState func_actor_341700_801686AC
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef madChaserToAlertState

#include "../../shared/mad_chaser_walk_state.inc.c"

#include "../../shared/mad_chaser_leap_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserLeapState  func_actor_341700_801687B4
#define gMadChaserLeapSteps gMadChaserAlertSteps
#include "../../shared/mad_chaser_leap_state.inc.c"
#undef madChaserLeapState
#undef gMadChaserLeapSteps

#include "../../shared/mad_chaser_recoil_light_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserRecoilLightState func_actor_341700_80168874
#define madChaserRecoilLight      madChaserRecoilHeavy
#define madChaserRecoilRecover    madChaserRecoilHeavyEnd
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilLightState
#undef madChaserRecoilLight
#undef madChaserRecoilRecover

/// A further copy, under this file's own name.
#define madChaserRecoilLightState func_actor_341700_801688C8
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
#define madChaserWalkState      func_actor_341700_80169B40
#define gMadChaserWalkSteps     gMadChaserLurkHoldSteps
#define madChaserTakeHitRequest madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef madChaserWalkState
#undef gMadChaserWalkSteps
#undef madChaserTakeHitRequest

/// A further copy, under this file's own name.
#define madChaserWalkState      func_actor_341700_80169BC8
#define gMadChaserWalkSteps     gMadChaserLurkCrouchSteps
#define madChaserTakeHitRequest madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef madChaserWalkState
#undef gMadChaserWalkSteps
#undef madChaserTakeHitRequest

#include "../../shared/mad_chaser_lurk_rise_state.inc.c"

#include "../../shared/mad_chaser_lurk_alert_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserDangleState  func_actor_341700_80169D54
#define gMadChaserDangleSteps gMadChaserLurkShiftSteps
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

/// A further copy, under this file's own name.
#define madChaserStartDespawn func_actor_341700_8016ABF4
#include "../../shared/mad_chaser_start_despawn.inc.c"
#undef madChaserStartDespawn

#include "../../shared/mad_chaser_take_knockdown_request.inc.c"
