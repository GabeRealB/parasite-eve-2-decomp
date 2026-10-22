#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
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
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/creatures/rat/rat.h"
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

static void _mothSpawn(Enemy* enemy, Task* task);
static void _mothUpdate(Enemy* enemy, Task* task);
static void _mothContacts(Task* task);
static void _mothOscillateParts(Task* task);
static void _mothSteer(Task* task);
static void _mothDrift(Task* task);
static void _mothDeath(Enemy* enemy, Task* task);
static void _mothDrawBurst(Task* task);
static void _mothTask(Task* task);
static void _mothSquash(Task* task);
static void _mothUpdateColor(Task* task);

/// Execution priority of this package's two enemy model tasks.
enum { ACTOR_00700_TASK_PRIORITY = 96 };

/// Task-state callbacks: 0 spawn, 1 living update, 2 death; copied before dispatch.
///
/// Indexed by `Task::state` in 0..2, with no terminator or bounds check.
static const EnemyTaskFuncTable3 _gRatStateHandlers = {
    { [RAT_TASK_SPAWN] = ratSpawn, [RAT_TASK_UPDATE] = _ratUpdate, [RAT_TASK_DEATH] = _ratDeath },
};

#include "assets/encounters/rat_and_moth/rat_body.inc"

#include "assets/encounters/rat_and_moth/package_1_clips_1.inc"

/// Bite attack shared by the rat parameters and the briefly enabled attack sphere.
///
/// Base power 6; the poison reaction is packed into the category-4 contact key.
static DamageAttack _gRatAttack = { .power = 6, .reaction = RAT_BITE_REACTION_POISON };

/// Shared rat health, battle rewards and hit-reaction tuning, borrowed by live enemies.
static EnemyParams _gRatParams = {
    .attacks              = &_gRatAttack,
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

TaskDesc gActor00700RatTaskDesc = { { { TASK_BODY_TMD, ACTOR_00700_TASK_PRIORITY } }, _ratTask, { .model = &gRatAndMothRatBody } };

/// Rat clips indexed by `RAT_ANIM_*`: entry 0 empty, entries 1..10 drive slots 1..6.
///
/// The animation context borrows the table and its package-local clip data
/// until the rat's task is destroyed.
#include "assets/encounters/rat_and_moth/rat_bank.inc"

/// Whole-frame transition durations indexed by the requested `RAT_ANIM_*` clip.
///
/// Signed s16 entries are promoted for playback; 0 seeks without blending.
/// Entries 0..10 cover all requests; the stored twelfth zero is never indexed.
static s16 _gRatAnimBlendFrames[RAT_ANIM_SET_COUNT + 1] = {
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
    0,
};

#include "assets/encounters/rat_and_moth/moth_body.inc"

#include "assets/encounters/rat_and_moth/package_1_clips_2.inc"

/// Attack delivered by the moth's death sphere: base power 5 and darkness reaction.
static DamageAttack _gMothAttack = { .power = 5, .reaction = MOTH_BURST_REACTION_DARKNESS };

/// Shared moth health, battle rewards and hit-reaction tuning, borrowed by live enemies.
static EnemyParams _gMothParams = {
    .attacks              = &_gMothAttack,
    .hpMax                = 1,
    .exp                  = 2,
    .bp                   = 18,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 99,
};

/// Base forward speeds for alerted flight, in parent-coordinate units per update.
///
/// Indexed by placement `rowIndex` 0..7; a 0..31 draw is added before
/// narrowing to s16 and stepping along the root's Q12 forward axis.
static u16 _gMothPursuitSpeeds[MOTH_PLACEMENT_ROW_COUNT] = {
    2,
    8,
    16,
    24,
    32,
    32,
    32,
    36,
};

TaskDesc gActor00700MothTaskDesc = { { { TASK_BODY_TMD, ACTOR_00700_TASK_PRIORITY } }, _mothTask, { .model = &_gRatAndMothMothBody } };

/// Moth rest-pose sets: entry 0 empty, entry 1 seeds articulated slots 1..3.
///
/// The rig borrows this table while its package is loaded. Wing motion is
/// procedural; the rest-pose clip is never ticked after spawn.
#include "assets/encounters/rat_and_moth/moth_bank.inc"

/// Texture origins of the eight 32-texel-square cells in the moth death burst.
///
/// Timer ticks 1..23 select `timer / MOTH_BURST_TICKS_PER_CELL`; the drawer
/// supplies the texture page and palette from the live model's offsets.
static ActorSpriteUv _gMothBurstUvs[MOTH_BURST_CELL_COUNT] = {
    { 96, 0, 96, 0 },
    { 0, 0, 160, 0 },
    { 0, 0, 192, 0 },
    { 32, 0, 192, 0 },
    { 0, 0, 224, 0 },
    { 32, 0, 224, 0 },
    { 64, 0, 224, 0 },
    { 96, 0, 224, 0 },
};

/// Bite descriptor borrowed by the rat spawn fragment.
///
/// Points to the single `DamageAttack` entry also selected by the enemy parameters.
/// Spawn reads entry zero once and retains only its packed collision key.
/// Define before including the fragment and undefine afterwards; see `rat.h`.
#define RAT_BITE_ATTACK (&_gRatAttack)

/// Enemy-kind parameter record supplied to the shared rat spawner.
///
/// Binds this TU's addressable, non-const `EnemyParams` object. Spawn retains
/// its address in `Enemy::param` and copies `hpMax` to the rat's hit points.
/// Rats borrow the same record for damage reactions and battle rewards; it
/// must remain loaded until enemy teardown. Both uses must select the same
/// object without side effects. Define before including `rat_spawn.inc.c`
/// and undefine afterwards; see `rat.h`.
#define RAT_ENEMY_PARAMS (_gRatParams)

/// Animation-set table supplied to the shared rat spawn fragment.
///
/// Binds this TU's `AnimationSet*` array with `RAT_ANIM_SET_COUNT` entries:
/// entry 0 is NULL; `RAT_ANIM_*` entries 1..10 supply tracks for slots 1..6.
/// Evaluated once at spawn; the context and slots borrow the table and clip
/// data without modifying them until task teardown. Define before including
/// `rat_spawn.inc.c` and undefine afterwards; see `rat.h`.
#define RAT_ANIM_SETS _gRatAnimSets
#include "../../shared/creatures/rat/rat_spawn.inc.c"
#undef RAT_BITE_ATTACK
#undef RAT_ENEMY_PARAMS
#undef RAT_ANIM_SETS

#include "../../shared/creatures/rat/rat.inc.c"

/// Task-state callbacks: 0 spawn, 1 living update, 2 death; copied before dispatch.
///
/// Indexed by `Task::state` in 0..2, with no terminator or bounds check.
static const EnemyTaskFuncTable3 _gMothStateHandlers = {
    { [MOTH_TASK_SPAWN] = _mothSpawn, [MOTH_TASK_UPDATE] = _mothUpdate, [MOTH_TASK_DEATH] = _mothDeath },
};

#include "../../shared/creatures/moth/moth.inc.c"

#include "../../shared/creatures/moth/moth_task.inc.c"

PACKAGE_ALIASES
