#include "actors/actor_04400.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

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
#include "main/random.h"
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

static const TaskFuncTable3 gMadChaserWalkSteps; // dispatcher table _madChaserWalkState copies onto its stack

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`): the angle is a `long`,
/// so a negated angle is passed without re-truncation to 16 bits.

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
static void _madChaserDropDeathHold(Task* task);
static void _madChaserShrinkDeathStart(Task* task);
static void _madChaserShrinkDeathTurnTranslucent(Task* task);
static void _madChaserShrinkDeathStartDespawn(Task* task);

extern TmdSource gMadChaserChunkModel2;

static const TaskFuncTable3 gMadChaserWalkSteps;

static TmdSource _gActor04400MadChaserBody;

static TmdBone _gMadChaserBurstHeadSkeleton[] = {
#include "assets/mad_chaser_burst_head_skeleton.inc"
};

static u32 _gMadChaserBurstHeadPartVerts[] = {
#include "assets/mad_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gMadChaserBurstHeadVerts[] = {
#include "assets/mad_chaser_burst_head_verts.inc"
};

static SVECTOR _gMadChaserBurstHeadNormals[] = {
#include "assets/mad_chaser_burst_head_normals.inc"
};

static u32 _gMadChaserBurstHeadStream[] = {
#include "assets/mad_chaser_burst_head_stream.inc"
};

/// Detached Mad Chaser head thrown from body part 6 during a burst death.
///
/// One rigid part, with 62 textured triangles and 15 textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
static TmdSource _gMadChaserBurstHead = {
    .handlersResolved    = MAD_CHASER_BURST_HEAD_HANDLERS_UNRESOLVED,
    .bufferHalfBytes     = MAD_CHASER_BURST_HEAD_TRIANGLE_COUNT * sizeof(POLY_GT3) + MAD_CHASER_BURST_HEAD_QUAD_COUNT * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gMadChaserBurstHeadSkeleton),
    .partVertexCounts    = _gMadChaserBurstHeadPartVerts,
    .verts               = _gMadChaserBurstHeadVerts,
    .normals             = _gMadChaserBurstHeadNormals,
    .skeleton            = _gMadChaserBurstHeadSkeleton,
    .stream              = _gMadChaserBurstHeadStream,
};

static TmdBone _gMadChaserBurstArmSkeleton[1] = {
#include "assets/mad_chaser_burst_arm_skeleton.inc"
};

static u32 _gMadChaserBurstArmPartVerts[1] = {
#include "assets/mad_chaser_burst_arm_partVerts.inc"
};

static SVECTOR _gMadChaserBurstArmVerts[28] = {
#include "assets/mad_chaser_burst_arm_verts.inc"
};

static SVECTOR _gMadChaserBurstArmNormals[37] = {
#include "assets/mad_chaser_burst_arm_normals.inc"
};

static u32 _gMadChaserBurstArmStream[276] = {
#include "assets/mad_chaser_burst_arm_stream.inc"
};

/// Detached Mad Chaser arm thrown from body part 8 during a burst death.
///
/// One rigid part, with 34 textured triangles and nine textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
static TmdSource _gMadChaserBurstArm = {
    0,
    34 * sizeof(POLY_GT3) + 9 * sizeof(POLY_GT4),
    0,
    ARRAY_SIZE(_gMadChaserBurstArmSkeleton),
    _gMadChaserBurstArmPartVerts,
    _gMadChaserBurstArmVerts,
    _gMadChaserBurstArmNormals,
    _gMadChaserBurstArmSkeleton,
    _gMadChaserBurstArmStream,
};

static TmdBone _gActor04400MadChaserBurstTailSkeleton[1] = {
#include "assets/mad_chaser_burst_tail_skeleton.inc"
};

static u32 _gActor04400MadChaserBurstTailPartVerts[1] = {
#include "assets/mad_chaser_burst_tail_partVerts.inc"
};

static SVECTOR _gActor04400MadChaserBurstTailVerts[25] = {
#include "assets/mad_chaser_burst_tail_verts.inc"
};

static SVECTOR _gActor04400MadChaserBurstTailNormals[32] = {
#include "assets/mad_chaser_burst_tail_normals.inc"
};

static u32 _gActor04400MadChaserBurstTailStream[215] = {
#include "assets/mad_chaser_burst_tail_stream.inc"
};

TmdSource gMadChaserChunkModel2 = {
    0,
    1448,
    0,
    1,
    _gActor04400MadChaserBurstTailPartVerts,
    _gActor04400MadChaserBurstTailVerts,
    _gActor04400MadChaserBurstTailNormals,
    _gActor04400MadChaserBurstTailSkeleton,
    _gActor04400MadChaserBurstTailStream,
};

static TmdBone _gActor04400MadChaserBodySkeleton[9] = {
#include "assets/mad_chaser_body_skeleton.inc"
};

static u32 _gActor04400MadChaserBodyPartVerts[9] = {
#include "assets/mad_chaser_body_partVerts.inc"
};

static SVECTOR _gActor04400MadChaserBodyVerts[160] = {
#include "assets/mad_chaser_body_verts.inc"
};

static SVECTOR _gActor04400MadChaserBodyNormals[206] = {
#include "assets/mad_chaser_body_normals.inc"
};

static u32 _gActor04400MadChaserBodyStream[2105] = {
#include "assets/mad_chaser_body_stream.inc"
};

static TmdSource _gActor04400MadChaserBody = {
    0,
    11212,
    3088,
    9,
    _gActor04400MadChaserBodyPartVerts,
    _gActor04400MadChaserBodyVerts,
    _gActor04400MadChaserBodyNormals,
    _gActor04400MadChaserBodySkeleton,
    _gActor04400MadChaserBodyStream,
};

enum { MAD_CHASER_ATTACK_ORDINARY_REACTION = 0 };

/// Leap-lunge attack: base power 22 with the ordinary, status-free player reaction.
static DamageAttack _gMadChaserAttacks[1] = {
    { 22, MAD_CHASER_ATTACK_ORDINARY_REACTION },
};

/// Shared Mad Chaser tuning and release rewards for this overlay's instances.
///
/// Ordinary and hidden spawn paths borrow the record and seed current/maximum
/// HP to 110. Its attack table is overlay-owned; rewards are 20 EXP, 40 BP, 1 MP.
static EnemyParams _gMadChaserEnemyParams = { _gMadChaserAttacks, 110, 20, 40, 1, 100, 10, 100, 0 };

static AnimationPackedPose _gActor04400Actor104400Animation0D554Bank1[6] = {
#include "assets/actor_104400_animation_0D554_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0D554Bank4[39] = {
#include "assets/actor_104400_animation_0D554_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0D554Records[77] = {
#include "assets/actor_104400_animation_0D554_records.inc"
};

static u16 _gActor04400Actor104400Animation0D554Indices[10] = {
#include "assets/actor_104400_animation_0D554_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0D554 = {
    _gActor04400Actor104400Animation0D554Records,
    _gActor04400Actor104400Animation0D554Indices,
    { NULL, _gActor04400Actor104400Animation0D554Bank1, NULL, NULL, _gActor04400Actor104400Animation0D554Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0D6F8Bank1[3] = {
#include "assets/actor_104400_animation_0D6F8_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0D6F8Bank4[21] = {
#include "assets/actor_104400_animation_0D6F8_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0D6F8Records[60] = {
#include "assets/actor_104400_animation_0D6F8_records.inc"
};

static u16 _gActor04400Actor104400Animation0D6F8Indices[10] = {
#include "assets/actor_104400_animation_0D6F8_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0D6F8 = {
    _gActor04400Actor104400Animation0D6F8Records,
    _gActor04400Actor104400Animation0D6F8Indices,
    { NULL, _gActor04400Actor104400Animation0D6F8Bank1, NULL, NULL, _gActor04400Actor104400Animation0D6F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0DBCCBank1[20] = {
#include "assets/actor_104400_animation_0DBCC_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0DBCCBank4[99] = {
#include "assets/actor_104400_animation_0DBCC_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0DBCCRecords[135] = {
#include "assets/actor_104400_animation_0DBCC_records.inc"
};

static u16 _gActor04400Actor104400Animation0DBCCIndices[10] = {
#include "assets/actor_104400_animation_0DBCC_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0DBCC = {
    _gActor04400Actor104400Animation0DBCCRecords,
    _gActor04400Actor104400Animation0DBCCIndices,
    { NULL, _gActor04400Actor104400Animation0DBCCBank1, NULL, NULL, _gActor04400Actor104400Animation0DBCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0E150Bank1[25] = {
#include "assets/actor_104400_animation_0E150_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0E150Bank4[110] = {
#include "assets/actor_104400_animation_0E150_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0E150Records[153] = {
#include "assets/actor_104400_animation_0E150_records.inc"
};

static u16 _gActor04400Actor104400Animation0E150Indices[10] = {
#include "assets/actor_104400_animation_0E150_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0E150 = {
    _gActor04400Actor104400Animation0E150Records,
    _gActor04400Actor104400Animation0E150Indices,
    { NULL, _gActor04400Actor104400Animation0E150Bank1, NULL, NULL, _gActor04400Actor104400Animation0E150Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0E250Bank1[2] = {
#include "assets/actor_104400_animation_0E250_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0E250Bank4[7] = {
#include "assets/actor_104400_animation_0E250_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0E250Records[36] = {
#include "assets/actor_104400_animation_0E250_records.inc"
};

static u16 _gActor04400Actor104400Animation0E250Indices[10] = {
#include "assets/actor_104400_animation_0E250_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0E250 = {
    _gActor04400Actor104400Animation0E250Records,
    _gActor04400Actor104400Animation0E250Indices,
    { NULL, _gActor04400Actor104400Animation0E250Bank1, NULL, NULL, _gActor04400Actor104400Animation0E250Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0E350Bank1[2] = {
#include "assets/actor_104400_animation_0E350_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0E350Bank4[7] = {
#include "assets/actor_104400_animation_0E350_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0E350Records[36] = {
#include "assets/actor_104400_animation_0E350_records.inc"
};

static u16 _gActor04400Actor104400Animation0E350Indices[10] = {
#include "assets/actor_104400_animation_0E350_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0E350 = {
    _gActor04400Actor104400Animation0E350Records,
    _gActor04400Actor104400Animation0E350Indices,
    { NULL, _gActor04400Actor104400Animation0E350Bank1, NULL, NULL, _gActor04400Actor104400Animation0E350Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0E780Bank1[24] = {
#include "assets/actor_104400_animation_0E780_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0E780Bank4[69] = {
#include "assets/actor_104400_animation_0E780_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0E780Records[112] = {
#include "assets/actor_104400_animation_0E780_records.inc"
};

static u16 _gActor04400Actor104400Animation0E780Indices[10] = {
#include "assets/actor_104400_animation_0E780_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0E780 = {
    _gActor04400Actor104400Animation0E780Records,
    _gActor04400Actor104400Animation0E780Indices,
    { NULL, _gActor04400Actor104400Animation0E780Bank1, NULL, NULL, _gActor04400Actor104400Animation0E780Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0EC24Bank1[22] = {
#include "assets/actor_104400_animation_0EC24_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0EC24Bank4[87] = {
#include "assets/actor_104400_animation_0EC24_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0EC24Records[129] = {
#include "assets/actor_104400_animation_0EC24_records.inc"
};

static u16 _gActor04400Actor104400Animation0EC24Indices[10] = {
#include "assets/actor_104400_animation_0EC24_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0EC24 = {
    _gActor04400Actor104400Animation0EC24Records,
    _gActor04400Actor104400Animation0EC24Indices,
    { NULL, _gActor04400Actor104400Animation0EC24Bank1, NULL, NULL, _gActor04400Actor104400Animation0EC24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0F17CBank1[14] = {
#include "assets/actor_104400_animation_0F17C_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0F17CBank4[99] = {
#include "assets/actor_104400_animation_0F17C_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0F17CRecords[186] = {
#include "assets/actor_104400_animation_0F17C_records.inc"
};

static u16 _gActor04400Actor104400Animation0F17CIndices[10] = {
#include "assets/actor_104400_animation_0F17C_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0F17C = {
    _gActor04400Actor104400Animation0F17CRecords,
    _gActor04400Actor104400Animation0F17CIndices,
    { NULL, _gActor04400Actor104400Animation0F17CBank1, NULL, NULL, _gActor04400Actor104400Animation0F17CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0F430Bank1[8] = {
#include "assets/actor_104400_animation_0F430_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0F430Bank4[54] = {
#include "assets/actor_104400_animation_0F430_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0F430Records[80] = {
#include "assets/actor_104400_animation_0F430_records.inc"
};

static u16 _gActor04400Actor104400Animation0F430Indices[10] = {
#include "assets/actor_104400_animation_0F430_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0F430 = {
    _gActor04400Actor104400Animation0F430Records,
    _gActor04400Actor104400Animation0F430Indices,
    { NULL, _gActor04400Actor104400Animation0F430Bank1, NULL, NULL, _gActor04400Actor104400Animation0F430Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0F6C4Bank1[7] = {
#include "assets/actor_104400_animation_0F6C4_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0F6C4Bank4[50] = {
#include "assets/actor_104400_animation_0F6C4_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0F6C4Records[79] = {
#include "assets/actor_104400_animation_0F6C4_records.inc"
};

static u16 _gActor04400Actor104400Animation0F6C4Indices[10] = {
#include "assets/actor_104400_animation_0F6C4_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0F6C4 = {
    _gActor04400Actor104400Animation0F6C4Records,
    _gActor04400Actor104400Animation0F6C4Indices,
    { NULL, _gActor04400Actor104400Animation0F6C4Bank1, NULL, NULL, _gActor04400Actor104400Animation0F6C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0F974Bank1[7] = {
#include "assets/actor_104400_animation_0F974_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0F974Bank4[53] = {
#include "assets/actor_104400_animation_0F974_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0F974Records[83] = {
#include "assets/actor_104400_animation_0F974_records.inc"
};

static u16 _gActor04400Actor104400Animation0F974Indices[10] = {
#include "assets/actor_104400_animation_0F974_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0F974 = {
    _gActor04400Actor104400Animation0F974Records,
    _gActor04400Actor104400Animation0F974Indices,
    { NULL, _gActor04400Actor104400Animation0F974Bank1, NULL, NULL, _gActor04400Actor104400Animation0F974Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0FB88Bank1[6] = {
#include "assets/actor_104400_animation_0FB88_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0FB88Bank4[42] = {
#include "assets/actor_104400_animation_0FB88_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0FB88Records[58] = {
#include "assets/actor_104400_animation_0FB88_records.inc"
};

static u16 _gActor04400Actor104400Animation0FB88Indices[10] = {
#include "assets/actor_104400_animation_0FB88_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0FB88 = {
    _gActor04400Actor104400Animation0FB88Records,
    _gActor04400Actor104400Animation0FB88Indices,
    { NULL, _gActor04400Actor104400Animation0FB88Bank1, NULL, NULL, _gActor04400Actor104400Animation0FB88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0FCA8Bank1[2] = {
#include "assets/actor_104400_animation_0FCA8_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0FCA8Bank4[11] = {
#include "assets/actor_104400_animation_0FCA8_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0FCA8Records[40] = {
#include "assets/actor_104400_animation_0FCA8_records.inc"
};

static u16 _gActor04400Actor104400Animation0FCA8Indices[10] = {
#include "assets/actor_104400_animation_0FCA8_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0FCA8 = {
    _gActor04400Actor104400Animation0FCA8Records,
    _gActor04400Actor104400Animation0FCA8Indices,
    { NULL, _gActor04400Actor104400Animation0FCA8Bank1, NULL, NULL, _gActor04400Actor104400Animation0FCA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0FDE4Bank1[4] = {
#include "assets/actor_104400_animation_0FDE4_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0FDE4Bank4[18] = {
#include "assets/actor_104400_animation_0FDE4_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0FDE4Records[34] = {
#include "assets/actor_104400_animation_0FDE4_records.inc"
};

static u16 _gActor04400Actor104400Animation0FDE4Indices[10] = {
#include "assets/actor_104400_animation_0FDE4_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0FDE4 = {
    _gActor04400Actor104400Animation0FDE4Records,
    _gActor04400Actor104400Animation0FDE4Indices,
    { NULL, _gActor04400Actor104400Animation0FDE4Bank1, NULL, NULL, _gActor04400Actor104400Animation0FDE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation0FFE0Bank1[10] = {
#include "assets/actor_104400_animation_0FFE0_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation0FFE0Bank4[31] = {
#include "assets/actor_104400_animation_0FFE0_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation0FFE0Records[51] = {
#include "assets/actor_104400_animation_0FFE0_records.inc"
};

static u16 _gActor04400Actor104400Animation0FFE0Indices[10] = {
#include "assets/actor_104400_animation_0FFE0_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation0FFE0 = {
    _gActor04400Actor104400Animation0FFE0Records,
    _gActor04400Actor104400Animation0FFE0Indices,
    { NULL, _gActor04400Actor104400Animation0FFE0Bank1, NULL, NULL, _gActor04400Actor104400Animation0FFE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation1041CBank1[16] = {
#include "assets/actor_104400_animation_1041C_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation1041CBank4[83] = {
#include "assets/actor_104400_animation_1041C_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation1041CRecords[125] = {
#include "assets/actor_104400_animation_1041C_records.inc"
};

static u16 _gActor04400Actor104400Animation1041CIndices[10] = {
#include "assets/actor_104400_animation_1041C_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation1041C = {
    _gActor04400Actor104400Animation1041CRecords,
    _gActor04400Actor104400Animation1041CIndices,
    { NULL, _gActor04400Actor104400Animation1041CBank1, NULL, NULL, _gActor04400Actor104400Animation1041CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation105A8Bank1[5] = {
#include "assets/actor_104400_animation_105A8_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation105A8Bank4[26] = {
#include "assets/actor_104400_animation_105A8_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation105A8Records[43] = {
#include "assets/actor_104400_animation_105A8_records.inc"
};

static u16 _gActor04400Actor104400Animation105A8Indices[10] = {
#include "assets/actor_104400_animation_105A8_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation105A8 = {
    _gActor04400Actor104400Animation105A8Records,
    _gActor04400Actor104400Animation105A8Indices,
    { NULL, _gActor04400Actor104400Animation105A8Bank1, NULL, NULL, _gActor04400Actor104400Animation105A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04400Actor104400Animation10750Bank1[5] = {
#include "assets/actor_104400_animation_10750_bank1.inc"
};

static AnimationPackedRotation _gActor04400Actor104400Animation10750Bank4[30] = {
#include "assets/actor_104400_animation_10750_bank4.inc"
};

static AnimationRecord _gActor04400Actor104400Animation10750Records[46] = {
#include "assets/actor_104400_animation_10750_records.inc"
};

static u16 _gActor04400Actor104400Animation10750Indices[10] = {
#include "assets/actor_104400_animation_10750_indices.inc"
};

static AnimationSet _gActor04400Actor104400Animation10750 = {
    _gActor04400Actor104400Animation10750Records,
    _gActor04400Actor104400Animation10750Indices,
    { NULL, _gActor04400Actor104400Animation10750Bank1, NULL, NULL, _gActor04400Actor104400Animation10750Bank4, NULL, NULL, NULL },
};

/// Loaded animation sets addressed directly by the one-based clip ID.
///
/// Entries 1..19 are playable; entries 0 and 20 are NULL, not clips. The
/// animation context borrows the pointer table and all tracks until teardown.
static AnimationSet* _gMadChaserAnimBank[21] = {
    NULL,
    &_gActor04400Actor104400Animation0D554,
    &_gActor04400Actor104400Animation0D6F8,
    &_gActor04400Actor104400Animation0DBCC,
    &_gActor04400Actor104400Animation0E150,
    &_gActor04400Actor104400Animation0E250,
    &_gActor04400Actor104400Animation0E350,
    &_gActor04400Actor104400Animation0E780,
    &_gActor04400Actor104400Animation0EC24,
    &_gActor04400Actor104400Animation0F17C,
    &_gActor04400Actor104400Animation0F430,
    &_gActor04400Actor104400Animation0F6C4,
    &_gActor04400Actor104400Animation0F974,
    &_gActor04400Actor104400Animation0FB88,
    &_gActor04400Actor104400Animation0FCA8,
    &_gActor04400Actor104400Animation0FDE4,
    &_gActor04400Actor104400Animation0FFE0,
    &_gActor04400Actor104400Animation1041C,
    &_gActor04400Actor104400Animation105A8,
    &_gActor04400Actor104400Animation10750,
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

enum { ACTOR_04400_MAD_CHASER_TASK_PRIORITY = 96 };

TaskDesc gActor04400MadChaserTaskDescs[ACTOR_04400_MAD_CHASER_TASK_COUNT] = {
    { { { TASK_BODY_TMD, ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, _madChaserTask, { .model = &_gActor04400MadChaserBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, _madChaserHiddenTask, { .model = &_gActor04400MadChaserBody } },
    { { { TASK_BODY_COORD, ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_TMD, ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, _madChaserHiddenTask, { .model = &_gActor04400MadChaserBody } },
};

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

#include "../../shared/mad_chaser_inlines.inc.c"

static void _madChaserRecoilRecover(Task* task);

/// Selects one axis of collision correction, giving grid pushback priority.
///
/// Both arguments and the result are signed whole parent-coordinate units.
/// With no grid correction, use the actor correction. Opposing corrections
/// retain the grid step; aligned corrections use the larger displacement.
static __inline__ s16 _madChaserSelectPushbackStep(s16 gridStep, s16 actorStep)
{
    if (gridStep == 0) {
        return actorStep;
    }
    if ((gridStep > 0 && actorStep < 0) || (gridStep < 0 && actorStep > 0)) {
        return gridStep;
    }
    if (gridStep > 0) {
        if (actorStep < gridStep) {
            return gridStep;
        }
        return actorStep;
    }
    if (actorStep < gridStep) {
        return actorStep;
    }
    return gridStep;
}

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

/// Phases of the combat alert cry and sidestep.
///
/// `MadChaserWork::subState` selects 0 cry, 1 wait, 2 release the alert claim, 3 start the
/// sidestep, 4 sidestep.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable5 _gMadChaserAlertSteps = { {
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

#include "../../shared/mad_chaser_limb_shadow.inc.c"

#include "../../shared/mad_chaser_spawn_gibs.inc.c"

#include "../../shared/mad_chaser_twist.inc.c"

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

/// Applies Mad Chaser body contacts, hit reactions and horizontal correction.
///
/// Requires live enemy/model/work and eight writable body contacts. A nonzero
/// `ignorePlayerBody` suppresses player-body overlap push only. Attack damage
/// narrows to s16, including critical multiplication; hit cooldown gates later
/// contacts in the same pass and decreases at the end of this update.
/// Status damage still ticks without an attack contact. Opposed grid normals
/// restore saved root X/Z and suppress all anchor/root correction.
/// Otherwise each axis selects the grid step or one eighth of its strongest
/// overlap push. Fractional grid steps add their sign to the floored integer
/// half, including an extra negative unit. Y is retained. Clears the contacts
/// and releases its eight-byte scratch reservation before returning.
static void _madChaserApplyContacts(Task* task, s16 ignorePlayerBody)
{
    // Unnamed attributes are identified here only by this enemy's response.
    enum {
        MAD_CHASER_HEAVY_HIT_MIN_DAMAGE      = 40,
        MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT   = 3,
        MAD_CHASER_ATTACK_ATTRIBUTE_BLAST_4  = 4,
        MAD_CHASER_ATTACK_ATTRIBUTE_HEAVY_5  = 5,
        MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_8 = 8,
        MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_9 = 9,
        MAD_CHASER_PUSH_FRACTION_BITS        = 16,
        MAD_CHASER_PUSH_FRACTION_MASK        = 0xFFFF
    };
    /// Adds the signed 16.16 correction's sign to its floored halfword step.
    ///
    /// Both arguments are side-effect-free scalar lvalues. correctionWord is
    /// read up to twice and gridStep once on the selected path; increments
    /// narrow to s16. Negative fractions subtract one below their integer half.
#define MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP(gridStep, correctionWord) \
    {                                                                    \
        if ((correctionWord) & MAD_CHASER_PUSH_FRACTION_MASK) {          \
            if ((correctionWord) > 0) {                                  \
                (gridStep)++;                                            \
            } else {                                                     \
                (gridStep)--;                                            \
            }                                                            \
        }                                                                \
    }
    WorldCollisionDelta gridCorrection;
    SVECTOR             contactPush;
    s16                 strongestPushX;
    s16                 strongestPushZ;
    s16                 gridStepX;
    s16                 gridStepZ;
    u8                  opposedGridNormals;
    MadChaserWork*      work;
    Enemy*              enemy;
    GfxCoord*           rootCoord;
    s16                 hitAmount;
    s32                 attackDamage;
    s32                 statusDamage;
    s16                 statusReadoutAmount;
    s32                 contactIndex;

    gridStepZ          = 0;
    strongestPushX     = 0;
    strongestPushZ     = 0;
    gridStepX          = 0;
    opposedGridNormals = 0;
    work               = task->work;
    rootCoord          = task->extra.tmd->coords;
    enemy              = task->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->hitTaken = 0;
    // Accumulate body overlaps and apply attacks admitted by the hit cooldown.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        switch (work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (ignorePlayerBody != 0) {
                    break;
                }
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                _madChaserCalcContactPushback(task, rootCoord, &work->contacts[contactIndex], &contactPush);
                if (ABS(strongestPushX) < ABS(contactPush.vx)) {
                    strongestPushX = contactPush.vx;
                }
                if (ABS(strongestPushZ) < ABS(contactPush.vz)) {
                    strongestPushZ = contactPush.vz;
                }
                break;
            case WORLD_COLLISION_CONTACT_ATTACK:
                if (work->hitCooldown == 0) {
                    work->hitTaken    = 1;
                    attackDamage      = damageComputePlayerAttack(work->contacts[contactIndex].key.value, work->playerDist, 0, 0);
                    hitAmount         = attackDamage;
                    work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                    if (damageRollCriticalHit(enemy, work->contacts[contactIndex].key.value, 0) != 0) {
                        hitAmount = ((u32)attackDamage << 16) >> 14;
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 0, NULL);
                    }
                    damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, hitAmount, 0);
                    worldTargetAddReadoutAmount(&enemy->node, hitAmount, 0);
                    enemy->hp -= hitAmount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value),
                                   &task->extra.tmd->coords[1], NULL, &work->effectArg);
                    if (hitAmount >= MAD_CHASER_HEAVY_HIT_MIN_DAMAGE) {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                    } else {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_LIGHT;
                    }
                    switch (damageGetPlayerAttackReaction(work->contacts[contactIndex].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            damageStartEnemyStagger(enemy);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(enemy, work->contacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->contacts[contactIndex].key.value, 0);
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_BLAST_4:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_HEAVY_5:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_8:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                        case MAD_CHASER_ATTACK_ATTRIBUTE_STATUS_9:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                    }
                } else if ((damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value)) == EFFECT_HIT_KIND_LIFE_DRAIN_MOTES) {
                    effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[1], NULL, &work->effectArg);
                }
                break;
        }
    }

    // Consume status reactions independently of this frame's weapon contacts.
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = MAD_CHASER_HIT_REACTION_KNOCKDOWN;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = MAD_CHASER_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        work->damageOverTimeSeen = 1;
        statusDamage             = damageTickEnemyDamageOverTime(enemy);
        statusReadoutAmount      = statusDamage;
        if (statusReadoutAmount != 0) {
            enemy->hp -= statusDamage;
            worldTargetAddReadoutAmount(&enemy->node, statusReadoutAmount, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->hitTaken    = 1;
            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }

    // Resolve grid normals before applying the strongest overlap on each axis.
    switch (worldCollisionResolvePushback(work->contacts, &gridCorrection, ARRAY_SIZE(work->contacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            gridStepZ = gridCorrection.fixed.vz.halves.integer;
            gridStepX = gridCorrection.fixed.vx.word >> MAD_CHASER_PUSH_FRACTION_BITS;
            MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP(gridStepX, gridCorrection.fixed.vx.word);
            MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP(gridStepZ, gridCorrection.fixed.vz.word);
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0]   = work->prevRootPos.vx;
            rootCoord->coord.t[2]   = work->prevRootPos.vz;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            opposedGridNormals      = 1;
            break;
    }

    worldCollisionClearContacts(work->contacts);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    }
    if (opposedGridNormals == 0) {
        work->anchorPos.vx     += _madChaserSelectPushbackStep(gridStepX, strongestPushX >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        work->anchorPos.vz     += _madChaserSelectPushbackStep(gridStepZ, strongestPushZ >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        rootCoord->coord.t[0]  += _madChaserSelectPushbackStep(gridStepX, strongestPushX >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        rootCoord->coord.t[2]  += _madChaserSelectPushbackStep(gridStepZ, strongestPushZ >> MAD_CHASER_BODY_PUSH_DIVISOR_SHIFT);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
#undef MAD_CHASER_ADJUST_FRACTIONAL_GRID_STEP
}

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

/// Finishes light recoil according to the interrupted clip's stance.
///
/// Requires initialized Mad Chaser body work and its live animation rig. An
/// upright light hit restarts recoil at twice normal speed. Otherwise an
/// upright actor consumes any new hit before a clip boundary can resume walking;
/// a low actor returns to alert with a fresh hold at the boundary. The stance
/// was copied from the interrupted clip's stance table by recoil entry.
static void _madChaserRecoilRecover(Task* task)
{
    enum {
        MAD_CHASER_RECOIL_UPRIGHT_STANCE = 1,
        MAD_CHASER_RECOIL_UPRIGHT_ANIM   = 11,
    };
    MadChaserWork* work = task->work;

    if (work->stateScratch == MAD_CHASER_RECOIL_UPRIGHT_STANCE) {
        if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_LIGHT) {
            work->animRate    = 2 * ANIMATION_RATE_ONE;
            work->animId      = MAD_CHASER_RECOIL_UPRIGHT_ANIM;
            work->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
            return;
        }
        if (_madChaserTakeHitReaction(task) == 0 && _madChaserAnimHasBoundaryStatusInline(task)) {
            _madChaserSetBehaviorStateS16(task, MAD_CHASER_COMBAT_STATE_WALK);
        }
    } else if (_madChaserAnimHasBoundaryStatusInline(task)) {
        _madChaserSetAlertHold(task, 1);
        _madChaserSetBehaviorStateS16(task, MAD_CHASER_COMBAT_STATE_ALERT);
    }
}

/// Lurking behaviors shared by ordinary and scripted Mad Chasers.
///
/// `MadChaserWork::state` selects 0 idle, 1 look, 2 rise, 3 alert, 4 shift.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable5 _gMadChaserLurkStates = { {
    _madChaserLurkIdleState,
    _madChaserLurkLookState,
    _madChaserLurkRiseState,
    _madChaserLurkAlertState,
    _madChaserLurkShiftState,
} };

/// Runs the Mad Chaser's five lurking behaviors and consumes combat transitions.
///
/// Requires live model/enemy/work with a lurk state in 0..4. Running updates
/// track the player, dispatch unless a pull/vanish command intervenes, advance
/// animation and apply contacts before choosing death or combat. Paused updates
/// still refresh color and limb shadows; hidden updates suppress drawing.
/// The local dispatch table is copied before any handler can change state.
static void _madChaserLurkTick(Task* task)
{
    TmdObject*     model     = task->extra.tmd;
    Enemy*         enemy     = task->spawnArg2.pointer;
    MadChaserWork* work      = task->work;
    GfxCoord*      rootCoord = model->coords;
    TaskFuncTable5 states    = _gMadChaserLurkStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _madChaserTrackPlayer(task);
            if (_madChaserTakePullOrVanishCommand(task) == 0) {
                states.funcs[(s16)work->state](task);
            }
            _madChaserTickAnim(task);
            _madChaserTwistSpine(task);
            _madChaserUpdateRotation(task);
            _madChaserApplyContacts(task, 0);
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_SHRINK_DEATH);
            } else if (_madChaserTakeHitReaction(task)) {
                work->busy = 0;
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            _madChaserDrawLimbShadow(task, 2, 6, 0xC8, 0, 0xFF);
            _madChaserDrawLimbShadow(task, 1, 7, 0x80, 0, 0xFF);
            _madChaserDrawLimbShadow(task, 7, 8, 0x80, 0, 0xFF);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Phases of the lurking idle hold.
///
/// `MadChaserWork::subState` selects 0 start the hold, 1 wait, 2 change to look.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable3 _gMadChaserLurkIdleSteps = { {
    _madChaserLurkStartIdleHold,
    _madChaserLurkWait,
    _madChaserLurkIdleEnd,
} };

/// Phases of the lurking look hold.
///
/// `MadChaserWork::subState` selects 0 prepare the look, 1 start the hold, 2 look around.
/// The dispatcher copies every non-NULL void(Task*) callback before indexing;
/// the selected index must be in range and the carrier must remain loaded.
static const TaskFuncTable3 _gMadChaserLurkLookSteps = { {
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
#define MAD_CHASER_STEP_TABLE             gMadChaserWalkSteps
#define MAD_CHASER_WALK_INTERRUPT_HANDLER _madChaserTakeHitRequest
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_STEP_TABLE
#undef MAD_CHASER_WALK_INTERRUPT_HANDLER

#include "../../shared/mad_chaser_leap_state.inc.c"

/// Selects this carrier's declared static void(Task*) combat-alert dispatcher.
#define MAD_CHASER_ALERT_STATE_HANDLER _madChaserAlertState
/// Supplies the complete TaskFuncTable5 of this carrier's alert phases.
#define MAD_CHASER_ALERT_STEP_HANDLERS _gMadChaserAlertSteps
#include "../../shared/mad_chaser_alert_state.inc.c"
#undef MAD_CHASER_ALERT_STATE_HANDLER
#undef MAD_CHASER_ALERT_STEP_HANDLERS

/// Selects this carrier's declared static void(Task*) light-recoil dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserRecoilLightState
/// Use this carrier's signed-halfword behavior setter during recoil recovery.
#define _madChaserRecoilLightRecover _madChaserRecoilRecover
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER
#undef _madChaserRecoilLightRecover

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
#define MAD_CHASER_STEP_TABLE _gMadChaserLurkIdleSteps
// This lurk instance joins a shared alert instead of consuming a hit reaction.
#define MAD_CHASER_WALK_INTERRUPT_HANDLER _madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_STEP_TABLE
#undef MAD_CHASER_WALK_INTERRUPT_HANDLER

/// Runs the lurk look sequence, yielding to a claimed shared alert.
///
/// Requires live Mad Chaser work and `subState` in 0..2: prepare the transition,
/// start a timed look hold, then sway or face the player until rise or alert.
#define MAD_CHASER_STEP_STATE _madChaserLurkLookState
#define MAD_CHASER_STEP_TABLE _gMadChaserLurkLookSteps
// This lurk instance joins a shared alert instead of consuming a hit reaction.
#define MAD_CHASER_WALK_INTERRUPT_HANDLER _madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_STEP_TABLE
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

/// Holds the final drop-death state after the collision bodies are unlinked.
///
/// The task and model remain alive for the enclosing death tick's drawing and
/// periodic blast effects. This handler leaves the live task unchanged.
static void _madChaserDropDeathHold(Task* task)
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
