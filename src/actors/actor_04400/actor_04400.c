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

extern u8            gMadChaserAnimStance[];  // per animation id (1-based): the value to put in `stateScratch`
extern u8            gMadChaserSettleAnims[]; // per animation id (1-based): the animation to follow it
extern EnemyParams   gMadChaserEnemyParams;   // the main enemy's `Enemy::param` record
extern AnimationSet* gMadChaserAnimBank[21];  // animation bank handed to `animationInitContext`
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry     gMadChaserMsgTable[3];    // stored into `Task::msgTable` by madChaserSpawn
static const TaskFuncTable3 gMadChaserKnockdownSteps; // dispatcher table madChaserKnockdownState copies onto its stack
static const TaskFuncTable3 gMadChaserWalkSteps;      // dispatcher table madChaserWalkState copies onto its stack
static const TaskFuncTable5 gMadChaserLeapSteps;      // dispatcher table madChaserLeapState copies onto its stack
static const TaskFuncTable5 Actor04400_D0009C;        // dispatcher table Actor04400_Fn06964 copies onto its stack
static const TaskFuncTable3 Actor04400_D00150;        // dispatcher table Actor04400_Fn07CF0 copies onto its stack
static const TaskFuncTable3 Actor04400_D0015C;        // dispatcher table Actor04400_Fn07D78 copies onto its stack
static const TaskFuncTable4 Actor04400_D00174;        // dispatcher table Actor04400_Fn07F04 copies onto its stack
static const TaskFuncTable6 gMadChaserPullSteps;      // dispatcher table madChaserPullState copies onto its stack

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`): the angle is a `long`,
/// so a negated angle is passed without re-truncation to 16 bits.

static void Actor04400_Fn03538(Task* arg0);
static void Actor04400_Fn05260(Task* arg0);
static void Actor04400_Fn0674C(Task* arg0);
static void Actor04400_Fn06848(Task* arg0);
static void Actor04400_Fn0685C(Task* arg0);
static void Actor04400_Fn06964(Task* arg0);
static void Actor04400_Fn06A24(Task* arg0);
static void Actor04400_Fn06A78(Task* arg0);
static void Actor04400_Fn07CF0(Task* arg0);
static void Actor04400_Fn07D78(Task* arg0);
static void Actor04400_Fn07F04(Task* arg0);
static void Actor04400_Fn089C0(Task* arg0);
static void Actor04400_Fn08A9C(Task* arg0);
static void Actor04400_Fn08AA4(Task* arg0);
static void Actor04400_Fn08C08(Task* arg0);
static void Actor04400_Fn08DA4(Task* arg0);

/* `D_800678F0` selects the model stream the next `effectSpawn` copies into
 * its effect's `TmdObject`. Declared as a one-element array so GCC 2.8.1
 * cannot treat the store as a non-aliasing scalar and sink it past the
 * `TmdObject` loads. */
extern void*     D_800678F0[1];
extern TmdSource gMadChaserChunkModel0;
extern TmdSource gMadChaserChunkModel1;
extern TmdSource gMadChaserChunkModel2;

static const TaskFuncTable3 gMadChaserKnockdownSteps;
static const TaskFuncTable3 gMadChaserWalkSteps;
static const TaskFuncTable5 gMadChaserLeapSteps;
static const TaskFuncTable5 Actor04400_D0009C;
static const TaskFuncTable3 Actor04400_D00150;
static const TaskFuncTable3 Actor04400_D0015C;
static const TaskFuncTable4 Actor04400_D00174;
static const TaskFuncTable6 gMadChaserPullSteps;

static TmdSource _gActor04400MadChaserBody;

void Actor04400_Fn0648C(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

static TmdBone _gActor04400MadChaserBurstHeadSkeleton[1] = {
#include "assets/mad_chaser_burst_head_skeleton.inc"
};

static u32 _gActor04400MadChaserBurstHeadPartVerts[1] = {
#include "assets/mad_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor04400MadChaserBurstHeadVerts[48] = {
#include "assets/mad_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor04400MadChaserBurstHeadNormals[53] = {
#include "assets/mad_chaser_burst_head_normals.inc"
};

static u32 _gActor04400MadChaserBurstHeadStream[486] = {
#include "assets/mad_chaser_burst_head_stream.inc"
};

TmdSource gMadChaserChunkModel0 = {
    0,
    3260,
    0,
    1,
    _gActor04400MadChaserBurstHeadPartVerts,
    _gActor04400MadChaserBurstHeadVerts,
    _gActor04400MadChaserBurstHeadNormals,
    _gActor04400MadChaserBurstHeadSkeleton,
    _gActor04400MadChaserBurstHeadStream,
};

static TmdBone _gActor04400MadChaserBurstArmSkeleton[1] = {
#include "assets/mad_chaser_burst_arm_skeleton.inc"
};

static u32 _gActor04400MadChaserBurstArmPartVerts[1] = {
#include "assets/mad_chaser_burst_arm_partVerts.inc"
};

static SVECTOR _gActor04400MadChaserBurstArmVerts[28] = {
#include "assets/mad_chaser_burst_arm_verts.inc"
};

static SVECTOR _gActor04400MadChaserBurstArmNormals[37] = {
#include "assets/mad_chaser_burst_arm_normals.inc"
};

static u32 _gActor04400MadChaserBurstArmStream[276] = {
#include "assets/mad_chaser_burst_arm_stream.inc"
};

TmdSource gMadChaserChunkModel1 = {
    0,
    1828,
    0,
    1,
    _gActor04400MadChaserBurstArmPartVerts,
    _gActor04400MadChaserBurstArmVerts,
    _gActor04400MadChaserBurstArmNormals,
    _gActor04400MadChaserBurstArmSkeleton,
    _gActor04400MadChaserBurstArmStream,
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

DamageAttack Actor04400_D0D314[1] = {
    { 22, 0 },
};

EnemyParams gMadChaserEnemyParams = { Actor04400_D0D314, 110, 20, 40, 1, 100, 10, 100, 0 };

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

AnimationSet* gMadChaserAnimBank[21] = {
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

TaskMessageEntry gMadChaserMsgTable[3] = {
    { ACTOR_MESSAGE_PLACE, madChaserMsgPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor04400_Fn0648C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor04400_D107E4 = { { { TASK_BODY_TMD, 96 } }, madChaserTask, { .model = &_gActor04400MadChaserBody } };

TaskDesc Actor04400_D107F0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, madChaserHiddenTask, { .model = &_gActor04400MadChaserBody } };

TaskDesc Actor04400_D107FC = { { { TASK_BODY_COORD, 96 } }, taskKill, { .value = 0 } };

TaskDesc Actor04400_D10808 = { { { TASK_BODY_TMD, 96 } }, madChaserHiddenTask, { .model = &_gActor04400MadChaserBody } };

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

u8 gMadChaserSettleAnims[19] = { 5, 6, 5, 6, 5, 6, 6, 5, 5, 5, 6, 6, 5, 5, 5, 6, 5, 6, 5 };

#include "../../shared/mad_chaser_inlines.inc.c"

static __inline__ s16 Actor04400_PickStep(s16 step, s16 push);
static void           Actor04400_Fn03390(Task* arg0);

/// Picks the per-axis step: the collision `step` when there is one and the
/// push-out opposes it, otherwise whichever of the two is larger in the
/// direction of `step`.
static __inline__ s16 Actor04400_PickStep(s16 step, s16 push)
{
    if (step == 0) {
        return push;
    }
    if ((step > 0 && push < 0) || (step < 0 && push > 0)) {
        return step;
    }
    if (step > 0) {
        if (push < step) {
            return step;
        }
        return push;
    }
    if (push < step) {
        return push;
    }
    return step;
}

/// Task-state handlers of the first enemy form, dispatched by
/// `madChaserTask` on `Task::state`.
static const TaskFuncTable6 gMadChaserTaskStates = { {
    madChaserSpawn,
    Actor04400_Fn03538,
    madChaserDangleFrame,
    madChaserCombatTick,
    madChaserDeathTick,
    Actor04400_Fn0674C,
} };

/// Task-state handlers of the second enemy form, dispatched by
/// `madChaserHiddenTask` on `Task::state`.
static const TaskFuncTable10 gMadChaserHiddenTaskStates = { {
    madChaserSpawnHidden,
    Actor04400_Fn03538,
    madChaserDangleFrame,
    madChaserCombatTick,
    madChaserDeathTick,
    Actor04400_Fn0674C,
    madChaserEmergeTick,
    madChaserVanishState,
    madChaserDropDeathTick,
    madChaserShrinkDeathTick,
} };

/// State handlers `madChaserCombatTick` dispatches by `state`.
static const TaskFuncTable11 gMadChaserCombatStates = { {
    madChaserToAlertState,
    Actor04400_Fn06848,
    Actor04400_Fn0685C,
    madChaserWalkState,
    madChaserLeapState,
    Actor04400_Fn06964,
    madChaserRecoilLightState,
    Actor04400_Fn06A24,
    Actor04400_Fn06A78,
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

/// Sub-state handlers `Actor04400_Fn06964` dispatches by `subState`.
static const TaskFuncTable5 Actor04400_D0009C = { {
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

/// Per-frame contact handling for the enemy. Walks the eight contact records: kind 1 (skipped when
/// `arg1` is set) and kind 3 push the model out, kind 2 applies a hit -
/// damage, status effects and the pending state request in `hitReaction` -
/// unless `hitCooldown` is still cooling down. Then ticks the status flags,
/// applies `worldCollisionResolvePushback`'s collision step (snapping back to `prevRootPos` when
/// it reports a conflict) and moves the root by the combined step and
/// push-out.
void madChaserApplyContacts(Task* arg0, s16 arg1)
{
    WorldCollisionDelta delta;
    SVECTOR             push;
    s16                 maxX;
    s16                 maxZ;
    s16                 stepX;
    s16                 stepZ;
    u8                  blocked;
    MadChaserWork*      work;
    Enemy*              enemy;
    GfxCoord*           coord;
    s16                 amount;
    s32                 dmg;
    s32                 tmp;
    s16                 tick;
    s32                 i;

    stepZ   = 0;
    maxX    = 0;
    maxZ    = 0;
    stepX   = 0;
    blocked = 0;
    work    = (MadChaserWork*)arg0->work;
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->hitTaken = 0;
    for (i = 0; i < 8; i++) {
        switch (work->contacts[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (arg1 != 0) {
                    break;
                }
            case 0x30000:
                madChaserCalcPush(arg0, coord, &work->contacts[i], &push);
                if (ABS(maxX) < ABS(push.vx)) {
                    maxX = push.vx;
                }
                if (ABS(maxZ) < ABS(push.vz)) {
                    maxZ = push.vz;
                }
                break;
            case 0x20000:
                if (work->hitCooldown == 0) {
                    work->hitTaken    = 1;
                    dmg               = damageComputePlayerAttack(work->contacts[i].key.value, work->playerDist, 0, 0);
                    amount            = dmg;
                    work->hitCooldown = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
                    if (damageRollCriticalHit(enemy, work->contacts[i].key.value, 0) != 0) {
                        amount = ((u32)dmg << 16) >> 14;
                        effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    damageAccumulateLifeDrainHp(enemy, work->contacts[i].key.value, amount, 0);
                    worldTargetAddReadoutAmount(&enemy->node, amount, 0);
                    enemy->hp -= amount;
                    if (enemy->hp < 0) {
                        enemy->hp = 0;
                    }
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[i].key.value),
                                   &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
                    if (amount >= 0x28) {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                    } else {
                        work->hitReaction = MAD_CHASER_HIT_REACTION_LIGHT;
                    }
                    switch (damageGetPlayerAttackReaction(work->contacts[i].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            damageStartEnemyStagger(enemy);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(enemy, work->contacts[i].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->contacts[i].key.value, 0);
                            break;
                        case 4:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case 5:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_BLAST;
                            break;
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_HEAVY;
                            break;
                        case 8:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                        case 9:
                            work->hitReaction = MAD_CHASER_HIT_REACTION_STATUS;
                            break;
                    }
                } else if ((damageGetPlayerAttackEffectId(work->contacts[i].key.value)) == 0xD) {
                    effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &arg0->extra.tmd->coords[1], NULL, &work->effectArg);
                }
                break;
        }
    }

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
        tmp                      = damageTickEnemyDamageOverTime(enemy);
        tick                     = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            worldTargetAddReadoutAmount(&enemy->node, tick, 0);
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

    switch (worldCollisionResolvePushback(work->contacts, &delta, 8, NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            stepZ = delta.fixed.vz.halves.integer;
            stepX = delta.fixed.vx.word >> 16;
            if (delta.fixed.vx.word & 0xFFFF) {
                if (delta.fixed.vx.word > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.fixed.vz.word & 0xFFFF) {
                if (delta.fixed.vz.word > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            coord->coord.t[0]   = work->prevRootPos.vx;
            coord->coord.t[2]   = work->prevRootPos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            blocked             = 1;
            break;
    }

    worldCollisionClearContacts(work->contacts);
    if (work->field_43E != 0) {
        work->field_43E--;
    }
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    }
    if (blocked == 0) {
        work->anchorPos.vx += Actor04400_PickStep(stepX, maxX >> 3);
        work->anchorPos.vz += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->coord.t[0]  += Actor04400_PickStep(stepX, maxX >> 3);
        coord->coord.t[2]  += Actor04400_PickStep(stepZ, maxZ >> 3);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

#include "../../shared/mad_chaser_tick_anim.inc.c"

#include "../../shared/mad_chaser_bodies.inc.c"

/// State handlers `madChaserDeathTick` dispatches by `state`.
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

/// State handler: with `stateScratch` 1, a pending request 1 while `hitTaken`
/// is set queues animation 0xB (kind 2, speed 0x20); otherwise a consumed
/// request wins, and a hit moves to state 3. With `stateScratch` clear, a hit
/// calls `_madChaserSetAlertHold` and moves to state 5. The request test
/// compares against the constant 1, which CSE folds into the `stateScratch`
/// register; writing `== work->stateScratch` reloads the byte instead.
static void Actor04400_Fn03390(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->stateScratch == 1) {
        if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_LIGHT) {
            work->animRate    = 0x20;
            work->animId      = 0xB;
            work->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
            return;
        }
        if (madChaserTakeRequest(arg0) == 0 && madChaserIsHit(arg0)) {
            _madChaserSetBehaviorStateS16(arg0, MAD_CHASER_COMBAT_STATE_WALK);
        }
    } else if (madChaserIsHit(arg0)) {
        _madChaserSetAlertHold(arg0, 1);
        _madChaserSetBehaviorStateS16(arg0, MAD_CHASER_COMBAT_STATE_ALERT);
    }
}

/// State handlers `Actor04400_Fn03538` dispatches by `state`.
static const TaskFuncTable5 Actor04400_D00128 = { {
    Actor04400_Fn07CF0,
    Actor04400_Fn07D78,
    madChaserLurkRiseState,
    madChaserLurkAlertState,
    Actor04400_Fn07F04,
} };

/// The five-state per-frame callback of the enemy's state machine, the
/// counterpart of `madChaserDropDeathTick`. Mode 0 counts `frameCount` up, aims
/// (`madChaserTrackPlayer`), lets `madChaserTakeHit` replace the handler
/// `state` selects from `Actor04400_D00128`, rebuilds the model root
/// rotation through part 0's coordinate, and picks the next state: 4 once the
/// `field_40` hold is empty, 8 / 9 for messages 4 / 5, and 3 after a consumed
/// `hitReaction` request. Mode 1 recolours from part 1's world position; both
/// clear bit 0x80 of the model flags, which mode 2 sets.
static void Actor04400_Fn03538(Task* arg0)
{
    TmdObject*     obj   = arg0->extra.tmd;
    Enemy*         enemy = arg0->spawnArg2.pointer;
    MadChaserWork* work  = (MadChaserWork*)arg0->work;
    GfxCoord*      coord = obj->coords;
    TaskFuncTable5 sp    = Actor04400_D00128;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            madChaserTrackPlayer(arg0);
            if (madChaserTakeHit(arg0) == 0) {
                sp.funcs[(s16)work->state](arg0);
            }
            _madChaserTickAnim(arg0);
            madChaserTwistSpine(arg0);
            madChaserUpdateRotation(arg0);
            madChaserApplyContacts(arg0, 0);
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_SHRINK_DEATH);
            } else if (madChaserTakeRequest(arg0)) {
                work->busy = 0;
                _madChaserEnterTaskState(arg0, MAD_CHASER_TASK_COMBAT);
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

/// Sub-state handlers `Actor04400_Fn07CF0` dispatches by `subState`.
static const TaskFuncTable3 Actor04400_D00150 = { {
    _madChaserLurkStartIdleHold,
    _madChaserLurkWait,
    _madChaserLurkIdleEnd,
} };

/// Sub-state handlers `Actor04400_Fn07D78` dispatches by `subState`.
static const TaskFuncTable3 Actor04400_D0015C = { {
    _madChaserLurkPrepareLook,
    _madChaserLurkStartLookHold,
    _madChaserLurkLookAround,
} };

/// Sub-state handlers `madChaserLurkAlertState` dispatches by `subState`.
static const TaskFuncTable3 gMadChaserLurkAlertSteps = { {
    madChaserStartAlert,
    madChaserLurkBrace,
    madChaserLurkSidestepToCombat,
} };

/// Sub-state handlers `Actor04400_Fn07F04` dispatches by `subState`.
static const TaskFuncTable4 Actor04400_D00174 = { {
    madChaserLurkShiftStart,
    madChaserLurkShiftBrace,
    madChaserLurkSidestepRight,
    madChaserLurkSidestepLeft,
} };

/// State handlers `madChaserEmergeTick` dispatches by `state`.
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
    Actor04400_Fn05260,
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

/// State handlers `madChaserDropDeathTick` dispatches by `state`.
static const TaskFuncTable5 gMadChaserDropDeathStates = { {
    madChaserDeathCryUnlink,
    madChaserDeathSettleQuiet,
    Actor04400_Fn089C0,
    madChaserDropBodies,
    Actor04400_Fn08A9C,
} };

/// State handlers `madChaserShrinkDeathTick` dispatches by `state`.
static const TaskFuncTable7 gMadChaserShrinkDeathStates = { {
    Actor04400_Fn08AA4,
    madChaserDeathSettleQuiet,
    Actor04400_Fn089C0,
    madChaserBeginShrink,
    Actor04400_Fn08C08,
    madChaserShrink,
    Actor04400_Fn08DA4,
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

/// A further copy, under this file's own name.
#define madChaserCreepUntilHit Actor04400_Fn05260
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

/// While `hitTaken` is 1, consumes the pending request in `hitReaction`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `hitTaken` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 madChaserTakeHitRequest(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->hitTaken == 1) {
        switch ((s16)(work->hitReaction - 1)) {
            case MAD_CHASER_HIT_REACTION_LIGHT - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 6;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_HEAVY - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 7;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_STATUS - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 8;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_BLAST - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 7;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_KNOCKDOWN - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 9;
                w->subState      = 0;
                break;
            }
        }
        work->hitReaction = MAD_CHASER_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `command`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test. `arg1` is the dispatch's
/// handler index and is unused here.
void Actor04400_Fn0648C(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (request->context.key == 0x2C00) {
        switch (request->command & MAD_CHASER_COMMAND_KIND_MASK) {
            case MAD_CHASER_COMMAND_EMERGE:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_PULL:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_VANISH:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_DROP_DEATH:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_SHRINK_DEATH:
                work->command = request->command;
                break;
        }
    }
}

#include "../../shared/mad_chaser_msg_place.inc.c"

#include "../../shared/mad_chaser_pin_part.inc.c"

#include "../../shared/mad_chaser_scale_by_speed.inc.c"

#include "../../shared/mad_chaser_anim_ended.inc.c"

#include "../../shared/mad_chaser_hidden_task.inc.c"

#include "../../shared/mad_chaser_task.inc.c"

/// A further copy, under this file's own name.
#define madChaserVanishState Actor04400_Fn0674C
#define madChaserVanish      _madChaserAdvanceBehaviorState
#define madChaserVanishFree  _madChaserDespawn
#include "../../shared/mad_chaser_vanish_state.inc.c"
#undef madChaserVanishState
#undef madChaserVanish
#undef madChaserVanishFree

#include "../../shared/mad_chaser_turn_to_player.inc.c"

#include "../../shared/mad_chaser_to_alert.inc.c"

/// A further copy, under this file's own name.
#define madChaserToAlertState Actor04400_Fn06848
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef madChaserToAlertState

/// A further copy, under this file's own name.
#define madChaserToAlertState Actor04400_Fn0685C
#include "../../shared/mad_chaser_to_alert.inc.c"
#undef madChaserToAlertState

#include "../../shared/mad_chaser_walk_state.inc.c"

#include "../../shared/mad_chaser_leap_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserLeapState  Actor04400_Fn06964
#define gMadChaserLeapSteps Actor04400_D0009C
#include "../../shared/mad_chaser_leap_state.inc.c"
#undef madChaserLeapState
#undef gMadChaserLeapSteps

/// This package's own madChaserRecoilRecover stands in.
#define madChaserRecoilRecover Actor04400_Fn03390
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilRecover

/// A further copy, under this file's own name.
#define madChaserRecoilLightState Actor04400_Fn06A24
#define madChaserRecoilLight      madChaserRecoilHeavy
#define madChaserRecoilRecover    madChaserRecoilHeavyEnd
#include "../../shared/mad_chaser_recoil_light_state.inc.c"
#undef madChaserRecoilLightState
#undef madChaserRecoilLight
#undef madChaserRecoilRecover

/// A further copy, under this file's own name.
#define madChaserRecoilLightState Actor04400_Fn06A78
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
#define madChaserWalkState      Actor04400_Fn07CF0
#define gMadChaserWalkSteps     Actor04400_D00150
#define madChaserTakeHitRequest _madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef madChaserWalkState
#undef gMadChaserWalkSteps
#undef madChaserTakeHitRequest

/// A further copy, under this file's own name.
#define madChaserWalkState      Actor04400_Fn07D78
#define gMadChaserWalkSteps     Actor04400_D0015C
#define madChaserTakeHitRequest _madChaserJoinAlert
#include "../../shared/mad_chaser_walk_state.inc.c"
#undef madChaserWalkState
#undef gMadChaserWalkSteps
#undef madChaserTakeHitRequest

#include "../../shared/mad_chaser_lurk_rise_state.inc.c"

#include "../../shared/mad_chaser_lurk_alert_state.inc.c"

/// A further copy, under this file's own name.
#define madChaserDangleState  Actor04400_Fn07F04
#define gMadChaserDangleSteps Actor04400_D00174
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
#define madChaserDeathWaitAnim Actor04400_Fn089C0
#include "../../shared/mad_chaser_death_wait_anim.inc.c"
#undef madChaserDeathWaitAnim

#include "../../shared/mad_chaser_drop_bodies.inc.c"

static void Actor04400_Fn08A9C(Task* arg0)
{
}

/// A further copy, under this file's own name.
#define madChaserDeathCryUnlink Actor04400_Fn08AA4
#include "../../shared/mad_chaser_death_cry_unlink.inc.c"
#undef madChaserDeathCryUnlink

#include "../../shared/mad_chaser_begin_shrink.inc.c"

/// A further copy, under this file's own name.
#define madChaserDeathTurnTranslucent Actor04400_Fn08C08
#include "../../shared/mad_chaser_death_translucent.inc.c"
#undef madChaserDeathTurnTranslucent

#include "../../shared/mad_chaser_shrink.inc.c"

/// A further copy, under this file's own name.
#define madChaserStartDespawn Actor04400_Fn08DA4
#include "../../shared/mad_chaser_start_despawn.inc.c"
#undef madChaserStartDespawn

#include "../../shared/mad_chaser_take_knockdown_request.inc.c"
