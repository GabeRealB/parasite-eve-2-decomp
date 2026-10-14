#include "actor_300700_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/creatures/moth/moth.h"

static void _ratDeath(Enemy* enemy, Task* actor);
static void _ratUpdate(Enemy* enemy, Task* actor);
static void _ratContacts(Task* actor);
static void _ratAnimate(Task* actor);
static void _ratAttack(Task* actor);
static void _ratBehavior(Task* actor);
static void _ratBuildup(Task* actor);
static void _ratHurt(Task* actor);
static void _ratIdle(Task* actor);
static void _ratIdleSound(Task* actor);
static void _ratReactions(Task* actor);
static void _ratShadow(Task* actor);
static void _ratSquash(Task* actor);
static void _ratStagger(Task* actor);
static void _ratStep(Task* actor);
static void _ratTask(Task* actor);
static void _ratTurn(Task* actor);
static void _ratUpdateColor(Task* actor);

/// Task-state callbacks: 0 spawn, 1 living update, 2 death; copied before dispatch.
///
/// Indexed by `Task::state` in 0..2, with no terminator or bounds check.
static const EnemyTaskFuncTable3 _gRatStateHandlers = {
    {
        [RAT_TASK_SPAWN]  = ratSpawn,
        [RAT_TASK_UPDATE] = _ratUpdate,
        [RAT_TASK_DEATH]  = _ratDeath,
    },
};

static AnimationSet _gActor300700Animation05AEC;
static AnimationSet _gActor300700Animation05DA8;
static AnimationSet _gActor300700Animation06238;
static AnimationSet _gActor300700Animation06578;
static AnimationSet _gActor300700Animation067B8;
static AnimationSet _gActor300700Animation06A34;
static AnimationSet _gActor300700Animation06D74;
static AnimationSet _gActor300700Animation06F08;
static AnimationSet _gActor300700Animation0719C;
static AnimationSet _gActor300700Animation074E0;

static AnimationPackedPose _gActor300700Animation05AECBank1[20] = {
#include "assets/actor_300700_animation_05AEC_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation05AECBank4[105] = {
#include "assets/actor_300700_animation_05AEC_bank4.inc"
};

static AnimationRecord _gActor300700Animation05AECRecords[145] = {
#include "assets/actor_300700_animation_05AEC_records.inc"
};

static u16 _gActor300700Animation05AECIndices[8] = {
#include "assets/actor_300700_animation_05AEC_indices.inc"
};

static AnimationSet _gActor300700Animation05AEC = {
    _gActor300700Animation05AECRecords,
    _gActor300700Animation05AECIndices,
    { NULL, _gActor300700Animation05AECBank1, NULL, NULL, _gActor300700Animation05AECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation05DA8Bank1[15] = {
#include "assets/actor_300700_animation_05DA8_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation05DA8Bank4[39] = {
#include "assets/actor_300700_animation_05DA8_bank4.inc"
};

static AnimationRecord _gActor300700Animation05DA8Records[77] = {
#include "assets/actor_300700_animation_05DA8_records.inc"
};

static u16 _gActor300700Animation05DA8Indices[8] = {
#include "assets/actor_300700_animation_05DA8_indices.inc"
};

static AnimationSet _gActor300700Animation05DA8 = {
    _gActor300700Animation05DA8Records,
    _gActor300700Animation05DA8Indices,
    { NULL, _gActor300700Animation05DA8Bank1, NULL, NULL, _gActor300700Animation05DA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06238Bank1[24] = {
#include "assets/actor_300700_animation_06238_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06238Bank4[85] = {
#include "assets/actor_300700_animation_06238_bank4.inc"
};

static AnimationRecord _gActor300700Animation06238Records[121] = {
#include "assets/actor_300700_animation_06238_records.inc"
};

static u16 _gActor300700Animation06238Indices[8] = {
#include "assets/actor_300700_animation_06238_indices.inc"
};

static AnimationSet _gActor300700Animation06238 = {
    _gActor300700Animation06238Records,
    _gActor300700Animation06238Indices,
    { NULL, _gActor300700Animation06238Bank1, NULL, NULL, _gActor300700Animation06238Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06578Bank1[14] = {
#include "assets/actor_300700_animation_06578_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06578Bank4[65] = {
#include "assets/actor_300700_animation_06578_bank4.inc"
};

static AnimationRecord _gActor300700Animation06578Records[87] = {
#include "assets/actor_300700_animation_06578_records.inc"
};

static u16 _gActor300700Animation06578Indices[8] = {
#include "assets/actor_300700_animation_06578_indices.inc"
};

static AnimationSet _gActor300700Animation06578 = {
    _gActor300700Animation06578Records,
    _gActor300700Animation06578Indices,
    { NULL, _gActor300700Animation06578Bank1, NULL, NULL, _gActor300700Animation06578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation067B8Bank1[9] = {
#include "assets/actor_300700_animation_067B8_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation067B8Bank4[40] = {
#include "assets/actor_300700_animation_067B8_bank4.inc"
};

static AnimationRecord _gActor300700Animation067B8Records[63] = {
#include "assets/actor_300700_animation_067B8_records.inc"
};

static u16 _gActor300700Animation067B8Indices[8] = {
#include "assets/actor_300700_animation_067B8_indices.inc"
};

static AnimationSet _gActor300700Animation067B8 = {
    _gActor300700Animation067B8Records,
    _gActor300700Animation067B8Indices,
    { NULL, _gActor300700Animation067B8Bank1, NULL, NULL, _gActor300700Animation067B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06A34Bank1[10] = {
#include "assets/actor_300700_animation_06A34_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06A34Bank4[46] = {
#include "assets/actor_300700_animation_06A34_bank4.inc"
};

static AnimationRecord _gActor300700Animation06A34Records[69] = {
#include "assets/actor_300700_animation_06A34_records.inc"
};

static u16 _gActor300700Animation06A34Indices[8] = {
#include "assets/actor_300700_animation_06A34_indices.inc"
};

static AnimationSet _gActor300700Animation06A34 = {
    _gActor300700Animation06A34Records,
    _gActor300700Animation06A34Indices,
    { NULL, _gActor300700Animation06A34Bank1, NULL, NULL, _gActor300700Animation06A34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06D74Bank1[20] = {
#include "assets/actor_300700_animation_06D74_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06D74Bank4[50] = {
#include "assets/actor_300700_animation_06D74_bank4.inc"
};

static AnimationRecord _gActor300700Animation06D74Records[84] = {
#include "assets/actor_300700_animation_06D74_records.inc"
};

static u16 _gActor300700Animation06D74Indices[8] = {
#include "assets/actor_300700_animation_06D74_indices.inc"
};

static AnimationSet _gActor300700Animation06D74 = {
    _gActor300700Animation06D74Records,
    _gActor300700Animation06D74Indices,
    { NULL, _gActor300700Animation06D74Bank1, NULL, NULL, _gActor300700Animation06D74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation06F08Bank1[5] = {
#include "assets/actor_300700_animation_06F08_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation06F08Bank4[20] = {
#include "assets/actor_300700_animation_06F08_bank4.inc"
};

static AnimationRecord _gActor300700Animation06F08Records[52] = {
#include "assets/actor_300700_animation_06F08_records.inc"
};

static u16 _gActor300700Animation06F08Indices[8] = {
#include "assets/actor_300700_animation_06F08_indices.inc"
};

static AnimationSet _gActor300700Animation06F08 = {
    _gActor300700Animation06F08Records,
    _gActor300700Animation06F08Indices,
    { NULL, _gActor300700Animation06F08Bank1, NULL, NULL, _gActor300700Animation06F08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation0719CBank1[11] = {
#include "assets/actor_300700_animation_0719C_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation0719CBank4[49] = {
#include "assets/actor_300700_animation_0719C_bank4.inc"
};

static AnimationRecord _gActor300700Animation0719CRecords[69] = {
#include "assets/actor_300700_animation_0719C_records.inc"
};

static u16 _gActor300700Animation0719CIndices[8] = {
#include "assets/actor_300700_animation_0719C_indices.inc"
};

static AnimationSet _gActor300700Animation0719C = {
    _gActor300700Animation0719CRecords,
    _gActor300700Animation0719CIndices,
    { NULL, _gActor300700Animation0719CBank1, NULL, NULL, _gActor300700Animation0719CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor300700Animation074E0Bank1[18] = {
#include "assets/actor_300700_animation_074E0_bank1.inc"
};

static AnimationPackedRotation _gActor300700Animation074E0Bank4[54] = {
#include "assets/actor_300700_animation_074E0_bank4.inc"
};

static AnimationRecord _gActor300700Animation074E0Records[87] = {
#include "assets/actor_300700_animation_074E0_records.inc"
};

static u16 _gActor300700Animation074E0Indices[8] = {
#include "assets/actor_300700_animation_074E0_indices.inc"
};

static AnimationSet _gActor300700Animation074E0 = {
    _gActor300700Animation074E0Records,
    _gActor300700Animation074E0Indices,
    { NULL, _gActor300700Animation074E0Bank1, NULL, NULL, _gActor300700Animation074E0Bank4, NULL, NULL, NULL },
};

DamageAttack gRatAttack = { .power = 6, .reaction = RAT_BITE_REACTION_POISON };

EnemyParams gRatParams = {
    .attacks              = &gRatAttack,
    .hpMax                = 18,
    .exp                  = 4,
    .bp                   = 22,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 20,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

/// Walk-start thresholds by placement `rowIndex` 0..7, compared with a 0..15 roll.
///
/// Tested first at each idle move roll; a threshold of 0 never starts a walk
/// and 16 always does.
static s16 _gRatWalkChance[RAT_PLACEMENT_ROW_COUNT] = {
    2,
    4,
    4,
    8,
    8,
    6,
    10,
    3,
};

/// Walk durations in update calls, selected by a uniform random index 0..15.
static u16 _gRatWalkFrames[RAT_MOVE_DURATION_COUNT] = {
    15,
    20,
    25,
    30,
    32,
    34,
    36,
    38,
    40,
    42,
    44,
    46,
    48,
    53,
    58,
    63,
};

/// Run-start thresholds by placement `rowIndex` 0..7, compared with a 0..15 roll.
///
/// Tested only after the walk roll fails; a threshold of 0 never starts a run
/// and 16 always does. These are conditional chances, not percentages.
static s16 _gRatRunChance[RAT_PLACEMENT_ROW_COUNT] = {
    1,
    2,
    5,
    10,
    12,
    12,
    12,
    14,
};

/// Run durations in update calls, selected by a uniform random index 0..15.
static u16 _gRatRunFrames[RAT_MOVE_DURATION_COUNT] = {
    10,
    11,
    12,
    13,
    14,
    16,
    18,
    20,
    22,
    24,
    26,
    28,
    29,
    30,
    31,
    32,
};

/// Bite-repeat thresholds by placement `rowIndex` 0..7, compared with a 0..15 roll.
///
/// After retreat, success restarts the approach; 0 never repeats and 16 always does.
static s16 _gRatAttackRepeatChance[RAT_PLACEMENT_ROW_COUNT] = {
    0,
    0,
    4,
    8,
    12,
    12,
    13,
    16,
};

enum { ACTOR_300700_RAT_TASK_PRIORITY = 96 };

TaskDesc gActor300700RatTaskDesc = { { { TASK_BODY_TMD, ACTOR_300700_RAT_TASK_PRIORITY } }, _ratTask, { .model = &gActor300700RatBody } };

AnimationSet* gRatAnimSets[RAT_ANIM_SET_COUNT] = {
    NULL,
    [RAT_ANIM_IDLE]            = &_gActor300700Animation05AEC,
    [RAT_ANIM_RUN]             = &_gActor300700Animation05DA8,
    [RAT_ANIM_BACK_OFF]        = &_gActor300700Animation06238,
    [RAT_ANIM_ATTACK]          = &_gActor300700Animation06578,
    [RAT_ANIM_HURT]            = &_gActor300700Animation067B8,
    [RAT_ANIM_COLLAPSE]        = &_gActor300700Animation06A34,
    [RAT_ANIM_WALK]            = &_gActor300700Animation06D74,
    [RAT_ANIM_BUILDUP_HOLD]    = &_gActor300700Animation06F08,
    [RAT_ANIM_STAGGER_RECOVER] = &_gActor300700Animation0719C,
    [RAT_ANIM_STAGGER]         = &_gActor300700Animation074E0,
};

/// Whole-frame transition durations indexed by the requested `RAT_ANIM_*` clip.
///
/// Signed s16 entries are promoted for playback; 0 seeks without blending.
/// Entries 0..10 include the zero-duration stagger at the package's final halfword.
static s16 _gRatAnimBlendFrames[RAT_ANIM_SET_COUNT] = {
    0,
    3,
    3,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
    0,
};

#include "../../shared/creatures/rat/rat.inc.c"
