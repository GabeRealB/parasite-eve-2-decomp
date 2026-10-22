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

static AnimationSet _gRatAndMothRatIdleClip;
static AnimationSet _gRatAndMothRatRunClip;
static AnimationSet _gRatAndMothRatBackOffClip;
static AnimationSet _gRatAndMothRatBiteClip;
static AnimationSet _gRatAndMothRatHurtClip;
static AnimationSet _gRatAndMothRatCollapseClip;
static AnimationSet _gRatAndMothRatWalkClip;
static AnimationSet _gRatAndMothRatBuildupHoldClip;
static AnimationSet _gRatAndMothRatStaggerRecoverClip;
static AnimationSet _gRatAndMothRatStaggerClip;

#include "assets/encounters/rat_and_moth/package_3_clips_2.inc"

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

TaskDesc gActor300700RatTaskDesc = { { { TASK_BODY_TMD, ACTOR_300700_RAT_TASK_PRIORITY } }, _ratTask, { .model = &gRatAndMothRatBody } };

#include "assets/encounters/rat_and_moth/rat_bank_2.inc"

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
