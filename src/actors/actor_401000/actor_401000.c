#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor/player_detection.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/actor/actor_contacts.h"
/// Selects this package's compiled Odd Stranger work layout and behavior.
///
/// Keep the binding through all shared fragments. Spawn arguments separately
/// select the opening state and combat tuning of each live enemy.
#define ODD_STRANGER_BUILD ODD_STRANGER_BUILD_401000
#include "../../shared/creatures/odd_stranger/odd_stranger.h"

static void _oddStrangerTick(Enemy* enemy, Task* actor);

static TmdSource _gStrangerOddBBurstLeg;

/// Handlers defined after the state table and the init that name them.
static void _actor401000Hidden(Task* task);
static void _actor401000RiseFront(Task* task);

/// Integer part of the last delta `_actorContactApplyGridPushback` resolved.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Player placement lent synchronously to the grab's place message.
///
/// Position uses the roots' common parent space; yaw faces the enemy, in
/// 4096 units per turn, with zero pitch and roll. The trailing eight bytes
/// have no established role and remain in `OddStrangerTransformStorage`.
OddStrangerTransformStorage _gOddStrangerPlayerGrabPlacement;

/// Eight-button struggle request lent synchronously to the player.
///
/// Only `pressCount` is written or consumed; the animation fallback stays
/// zero because this request addresses the player's hold handler.
GameActorButtonPressHold _gActor401000PlayerHoldRequest;

static TmdSource _gAkropolisCafeteriaBossStrangerBody;
static void      _actor401000StrangerTask(Task* task);

static AnimationSet _gStrangerOddPlayerGrabHoldClip;
static AnimationSet _gStrangerOddPlayerGrabStrikeClip;
static AnimationSet _gStrangerOddPlayerGrabReleaseClip;

static AnimationSet _gStrangerOddBWalkClip;
static AnimationSet _gStrangerOddBRunClip;
static AnimationSet _gStrangerOddSlideClip;
static AnimationSet _gStrangerOddFlinchClip;
static AnimationSet _gStrangerOddDownFrontClip;
static AnimationSet _gStrangerOddDormantFidgetClip;
static AnimationSet _gStrangerOddBRiseBackClip;
static AnimationSet _gStrangerOddRiseFrontClip;
static AnimationSet _gStrangerOddRefallFrontClip;
static AnimationSet _gStrangerOddAlertClip;
static AnimationSet _gStrangerOddBackOffClip;
static AnimationSet _gStrangerOddDormantClip;
static AnimationSet _gStrangerOddFallBackClip;
static AnimationSet _gStrangerDownBackClip;
static AnimationSet _gStrangerBurstCollapseClip;
static AnimationSet _gStrangerOddAmbushClip;
static AnimationSet _gStrangerOddSidestepNegativeClip;
static AnimationSet _gStrangerOddSidestepPositiveClip;
static AnimationSet _gStrangerOddGrabPullClip;
static AnimationSet _gStrangerOddGrabStrikeClip;
static AnimationSet _gStrangerOddGrabReleaseClip;
static AnimationSet _gStrangerOddGrabReachClip;

/// Two attack definitions belonging to this Odd Stranger enemy kind.
///
/// Entry 0 supplies the grab strike's 20-power reaction-7 hit. Entry 1 is
/// an 18-power ordinary hit with no recovered attacker selecting it.
static DamageAttack _gActor401000Attacks[2] = {
    { 20, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 18, GAME_ACTOR_REACTION_ORDINARY },
};

/// Shared health, attack, reward and hit-reaction parameters of this Odd Stranger.
///
/// Spawn copies the 180 starting HP and lends this record through `Enemy::param`
/// for the enemy's lifetime; the attack rows stay loaded with it.
static EnemyParams _gActor401000EnemyParams = { _gActor401000Attacks, 180, 42, 82, 4, 100, 10, 100, 0 };

/// Three combat tunings selected by the spawn argument's low nibble.
///
/// Value 2 selects row 0, value 1 row 2, and all others row 1. Each row sets
/// down ticks, sidestep angle (4096 units per turn), chase delay ticks and
/// notice distance in the roots' common parent space. Unknown row bytes stay zero.
static ActorStrangerVariant _gActor401000StrangerVariants[3] = {
    { 20, 900, 12, 2000, { 0, 0, 0, 0 } },
    { 10, 800, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 500, 12, 3000, { 0, 0, 0, 0 } },
};

#include "assets/rooms/akropolis/cafeteria/boss_stranger_body.inc"

/// Detached debris model spawned from body part 9 in both burst deaths.
///
/// The effect borrows this mutable source and stream for its lifetime and
/// inherits the enemy placement's texture offsets.
#include "assets/enemies/stranger/burst_hand.inc"

#include "assets/enemies/stranger/odd/b_burst_leg.inc"

/// Detached debris model spawned from body part 3 in both burst deaths.
///
/// The effect borrows this mutable source and stream for its lifetime and
/// inherits the enemy placement's texture offsets.
#include "assets/enemies/stranger/odd/b_burst_head.inc"

/// Detached debris model spawned from body part 1 in both burst deaths.
///
/// The effect borrows this mutable source and stream for its lifetime and
/// inherits the enemy placement's texture offsets.
#include "assets/enemies/stranger/odd/b_burst_torso.inc"

#include "assets/enemies/stranger/odd/package_b_4_clips.inc"

/// Crossfade durations in whole normal-rate animation frames.
///
/// Indexed [previous clip][requested clip], each in 0..44; zero means no
/// transition time. Signed bytes are promoted for `animationSeekSlotWithBlend`.
/// This table excludes the clip bank's trailing slot 45.
static s8 _gOddStrangerTransitionFrames[ODD_STRANGER_TRANSITION_ANIM_COUNT][ODD_STRANGER_TRANSITION_ANIM_COUNT] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Mutable clip bank borrowed by both nineteen-part animation rigs.
///
/// Indices are `ODD_STRANGER_ANIM_*`. Slots 0, 1 and 27..45 are NULL at load;
/// slot 16 is installed on scripted-dormant entry. Script-message slots
/// 34..37 and 39 require loaded sets before playback; no population is recovered.
/// Status slots 23 and 24 alias the back-down and front-down sets.
#include "assets/enemies/stranger/odd/b_bank.inc"

/// Character-selected player clip bank for the grab hold, strike and release.
///
/// Character 1 uses base + 2 and clips 1..3 (loaded entries 3..5). The other
/// branch uses base + 0; its required entries 1 and 2 are NULL at load and
/// their population or supported encounter domain is unproven. Entries 0..2
/// and 6 remain NULL in the embedded bank; playback borrows the selected sets.
#include "assets/enemies/stranger/odd/player_grab_bank.inc"

enum { ODD_STRANGER_PLAYER_GRAB_HOLD_ANIMATION = 1 };

/// Persistent player playback request for the Odd Stranger grab sequence.
///
/// Each carrier selects a saved-character bank, then reuses it for hold,
/// strike and release animations 1/2/3. Replacement resets playback and
/// disables world collision; blendFrames is ignored. Dispatch consumes the
/// request synchronously, while the player borrows its selected bank and sets
/// until playback is replaced. The actor overlay must stay loaded throughout.
static AnimationPlayRequest _gOddStrangerPlayerGrabAnimationRequest = {
    .source               = { .sets = _gActor401000PlayerGrabAnimSets },
    .animationId          = ODD_STRANGER_PLAYER_GRAB_HOLD_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 3,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Part-local hit-effect positions grouped by the hit's relative bearing.
///
/// Front uses entries 0..3, rear 5 or 7, positive side 8..9 and negative
/// side 10..11. Entries 4 and 6 are retained although their selectors are
/// unreachable. XYZ use integer model units; `pad` stores the attachment
/// part index (2, 7 or 9) and is copied intact with the `SVECTOR`.
static SVECTOR _gOddStrangerHitOffsets[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

enum { ACTOR_401000_MESSAGE_IGNORE = 2015 };

static s32 _actor401000IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra);

/// Synchronous message handlers installed on the Odd Stranger task.
///
/// Entries cover ignored 2015, animation, visibility, presence, placement,
/// hold release and room commands. `TASK_MESSAGE_TABLE_END` ends the search;
/// the table and callback code stay loaded while the task receives messages.
static TaskMessageEntry _gActor401000Messages[8] = {
    { ACTOR_401000_MESSAGE_IGNORE, _actor401000IgnoreMessage2015 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _oddStrangerPlayMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, _oddStrangerApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Room-specific limits on the root's Y translation in parent-coordinate units.
///
/// Only the Patio and woodland-path rows are consulted, selected by stage
/// and area without view or variant. The trailing zero row is never read;
/// its ownership and each row's unread bytes remain unproven.
static ActorHeightClamp _gActor401000HeightClamps[3] = {
    { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

/// Sum of requested circling-dash steps in parent-coordinate units, modulo 65536.
///
/// Reset on dash entry and incremented even when proximity prevents movement.
/// Shared by all tasks in this carrier; no recovered consumer reads the total.
static u16 _gOddStrangerCircleDashDistance = 0;

TaskDesc gActor401000StrangerTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor401000StrangerTask, { .model = &_gAkropolisCafeteriaBossStrangerBody } };

SVECTOR ActorContact_ScratchPosition;

/// Active room rows and correction limits, in whole parent-coordinate units.
enum {
    ACTOR_401000_HEIGHT_CLAMP_ACTIVE_ROWS = 2,
    ACTOR_401000_GRID_PUSH_MAX_Y          = 300,
    ACTOR_401000_GRID_PUSH_MAX_XZ         = 150,
    ACTOR_401000_GRID_ROOT_Y_OFFSET       = 75
};

static inline void _actor401000BindLightingMatrices(Task* actor);
static inline void _actor401000InitRootPose(GfxCoord* rootCoord, OddStrangerWork* work);
static void        _actor401000Spawn(Enemy* enemy, Task* actor);
static void        _actor401000ClampRootHeight(const GameLocationKey* location, GfxCoord* root);
static inline s32  _actor401000HasRoomHeightClamp(const GameLocationKey* location);
static s32         _actor401000ApplyGridPushback(GfxCoord* root, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset);
static void        _actor401000Chase(Task* task);
static void        _oddStrangerGrabReach(Task* task);
static void        _actor401000FallBack(Task* task);
static void        _actor401000FallFront(Task* task);
static void        _actor401000Dormant(Task* task);
static void        _actor401000Ambush(Task* task);
static void        _actor401000DeathBurst(Task* task);
static void        _actor401000RefallBack(Task* task);
static void        _actor401000RefallFront(Task* task);

#include "../../shared/actor/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/actor/player_detection_reach.inc.c"

#include "../../shared/actor/player_detection_sight.inc.c"

#include "../../shared/creatures/odd_stranger/odd_stranger_tick_blended.inc.c"

#include "../../shared/creatures/odd_stranger/odd_stranger.inc.c"

/// Binds the model's lighting matrices to storage in its task's work block.
///
/// Requires initialized `OddStrangerWork` and a live TMD body. The model borrows
/// both matrices until teardown; the work block must outlive those references.
static inline void _actor401000BindLightingMatrices(Task* actor)
{
    OddStrangerWork* work;
    TmdObject*       model;

    work            = actor->work;
    model           = actor->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Initializes root yaw/scale, the position-history cursor and collision contacts.
///
/// Requires live root and initialized work/contact tables. Replaces the root's
/// rotation at ODD_STRANGER_ROOT_SCALE (signed Q12) and marks it dirty, retaining
/// translation. Resets the history cursor without clearing its samples; clears
/// occupied grid/hit contacts while retaining their end markers and body links.
static inline void _actor401000InitRootPose(GfxCoord* rootCoord, OddStrangerWork* work)
{
    _actorRenderRescaleYaw(rootCoord, ODD_STRANGER_ROOT_SCALE);
    work->bodyPosCursor = 0;
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
}

/// Initializes this Odd Stranger instance, collision bodies and opening behavior.
///
/// Allocates owned work or destroys the enemy/task on failure. Acquires a battle
/// reference and exit handler, binds both nineteen-part animation contexts and
/// work-owned lighting matrices, and links three spheres. Placement index selects
/// one of five chase rates in sixteenths of a frame; spawn-argument nibbles select
/// hidden/dormant/patrol behavior and one of three combat tunings. The two patrol
/// points are 2000 game units apart. Advances the task state after initialization.
static void _actor401000Spawn(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_401000_CHASE_RATE_VARIANTS = 5,
        ACTOR_401000_GRID_CENTER_Y       = -172,
        ACTOR_401000_GRID_RADIUS         = 300,
        ACTOR_401000_HIT_RADIUS          = 430,
        ACTOR_401000_ATTACK_RADIUS       = 384,
        ACTOR_401000_SPAWN_HIDDEN        = 2,
        ACTOR_401000_SPAWN_DORMANT       = 4,
        ACTOR_401000_PREVIOUS_STATE_NONE = -1,
        ACTOR_401000_INITIAL_HIT_BODY_ID = 10,
        ACTOR_401000_EFFECT_SPAWN_ARG_LO = 768,
        ACTOR_401000_EFFECT_SPAWN_ARG_HI = 2,
        ACTOR_401000_SPAWN_MODE_SHIFT    = 16,
        ACTOR_401000_SPAWN_NIBBLE_MASK   = 0xF,
    };
    SVECTOR             patrolDirection;
    VECTOR              composedPosition;
    SVECTOR*            direction;
    TmdObject*          model;
    GfxCoord*           rootCoord;
    OddStrangerWork*    work;
    WorldCollisionBody* hitBody;
    WorldCollisionBody* attackBody;
    s32                 spawnMode;

    rootCoord   = actor->extra.tmd->coords;
    model       = actor->extra.tmd;
    work        = memCalloc(sizeof(OddStrangerWork), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    (sceneAcquireBattleRef)(0);
    actor->exitCallback = _oddStrangerExit;
    _actor401000BindLightingMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)_gActor401000EnemyParams.hpMax;
    enemy->param                  = &_gActor401000EnemyParams;
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, _gOddStrangerAnimSets, model,
                         work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, _gOddStrangerAnimSets,
                         model, work->blend.poses,
                         work->blend.slots);
    work->animRequest   = ODD_STRANGER_ANIM_REQUEST_RESET;
    work->animId        = ODD_STRANGER_ANIM_WALK;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->chaseRate     = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    switch (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % ACTOR_401000_CHASE_RATE_VARIANTS) {
        case 0:
            work->chaseRate = ANIMATION_RATE_ONE + 1;
            break;
        case 1:
            work->chaseRate = ANIMATION_RATE_ONE - 1;
            break;
        case 2:
            work->chaseRate = ANIMATION_RATE_ONE;
            break;
        case 3:
            work->chaseRate = ANIMATION_RATE_ONE + 2;
            break;
        case 4:
        default:
            work->chaseRate = ANIMATION_RATE_ONE - 2;
            break;
    }
    _oddStrangerDriveAnimation(actor);

    // Link distinct grid, incoming-hit and attack spheres with owned contact storage.
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = rootCoord;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = ACTOR_401000_GRID_CENTER_Y;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    work->gridBody.radius           = ACTOR_401000_GRID_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->hitCooldown     = 0;
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    hitBody                   = &work->hitBody;
    hitBody->coord            = &actor->extra.tmd->coords[2];
    hitBody->context.contacts = work->hitContacts;
    hitBody->pos.vx           = 0;
    hitBody->pos.vy           = 0;
    hitBody->pos.vz           = 0;
    hitBody->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_401000_INITIAL_HIT_BODY_ID;
    hitBody->radius           = ACTOR_401000_HIT_RADIUS;
    hitBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.key = WORLD_COLLISION_CONTACT_ENEMY_BODY;

    patrolDirection.vx           = 0;
    patrolDirection.vy           = 0;
    patrolDirection.vz           = 0;
    attackBody                   = &work->attackBody;
    attackBody->coord            = &actor->extra.tmd->coords[6];
    attackBody->context.contacts = work->attackContacts;
    direction                    = &patrolDirection;
    attackBody->pos.vx           = direction->vx;
    attackBody->pos.vy           = direction->vy;
    attackBody->pos.vz           = direction->vz;
    attackBody->radius           = ACTOR_401000_ATTACK_RADIUS;
    attackBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);

#include "../../shared/creatures/odd_stranger/odd_stranger_patrol_points.h"
    ODD_STRANGER_INITIALIZE_PATROL_POINTS(actor, work, patrolDirection, direction);
#undef ODD_STRANGER_INITIALIZE_PATROL_POINTS

    actor->msgTable         = _gActor401000Messages;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    composedPosition.vx = rootCoord->workm.t[0];
    composedPosition.vy = rootCoord->workm.t[1];
    composedPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &composedPosition, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = ACTOR_401000_EFFECT_SPAWN_ARG_LO;
    work->effectArg.spawnArgHi = ACTOR_401000_EFFECT_SPAWN_ARG_HI;
    // Spawn argument nibbles independently select opening state and combat tuning.
    spawnMode = (actor->spawnArg1.value >> ACTOR_401000_SPAWN_MODE_SHIFT);
    switch (spawnMode & ACTOR_401000_SPAWN_NIBBLE_MASK) {
        case ACTOR_401000_SPAWN_HIDDEN:
            work->prevState = ACTOR_401000_PREVIOUS_STATE_NONE;
            work->state     = ODD_STRANGER_STATE_HIDDEN;
            break;
        case ACTOR_401000_SPAWN_DORMANT:
            work->prevState = ACTOR_401000_PREVIOUS_STATE_NONE;
            work->state     = ODD_STRANGER_STATE_DORMANT;
            break;
        default:
            work->prevState = ACTOR_401000_PREVIOUS_STATE_NONE;
            work->state     = ODD_STRANGER_STATE_PATROL;
            tmdAllocPrimitiveBuffer(model);
            break;
    }
    switch (actor->spawnArg1.value & ACTOR_401000_SPAWN_NIBBLE_MASK) {
        case 2:
            work->downFramesBase = _gActor401000StrangerVariants[0].downFramesBase;
            work->sidestepAngle  = _gActor401000StrangerVariants[0].sidestepAngle;
            work->sidestepDelay  = _gActor401000StrangerVariants[0].sidestepDelay;
            work->noticeRadius   = _gActor401000StrangerVariants[0].noticeRadius;
            break;
        case 1:
            work->downFramesBase = _gActor401000StrangerVariants[2].downFramesBase;
            work->sidestepAngle  = _gActor401000StrangerVariants[2].sidestepAngle;
            work->sidestepDelay  = _gActor401000StrangerVariants[2].sidestepDelay;
            work->noticeRadius   = _gActor401000StrangerVariants[2].noticeRadius;
            break;
        case 0:
        default:
            work->downFramesBase = _gActor401000StrangerVariants[1].downFramesBase;
            work->sidestepAngle  = _gActor401000StrangerVariants[1].sidestepAngle;
            work->sidestepDelay  = _gActor401000StrangerVariants[1].sidestepDelay;
            work->noticeRadius   = _gActor401000StrangerVariants[1].noticeRadius;
            break;
    }

    _actor401000InitRootPose(actor->extra.tmd->coords, work);

    actor->state++;
}

#include "../../shared/creatures/odd_stranger/odd_stranger_spawn_hit_effect.inc.c"

/// Clamps the root's Y translation to this actor's bounds for the stage and area.
///
/// View and room variant do not select a row. A matching row invalidates the
/// composed transform even when Y was already in bounds; other rooms leave it
/// untouched. The final zero row of the table is outside the active domain.
static void _actor401000ClampRootHeight(const GameLocationKey* location, GfxCoord* root)
{
    const ActorHeightClamp* row;
    s32                     rootY;
    s32                     minY;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_401000_HEIGHT_CLAMP_ACTIVE_ROWS; rowIndex++) {
        row = &_gActor401000HeightClamps[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            minY  = row->minY;
            rootY = root->coord.t[1];
            if (rootY < minY) {
                root->coord.t[1] = minY;
            } else if (row->maxY < rootY) {
                root->coord.t[1] = row->maxY;
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
}

/// Returns 1 when this actor has height bounds for the supplied stage and area.
///
/// View and room-variant bytes are ignored. Only the two active table rows
/// participate; the trailing zero row is not a bound for stage zero.
static inline s32 _actor401000HasRoomHeightClamp(const GameLocationKey* location)
{
    const ActorHeightClamp* row;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_401000_HEIGHT_CLAMP_ACTIVE_ROWS; rowIndex++) {
        row = &_gActor401000HeightClamps[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            return 1;
        }
    }
    return 0;
}

/// Limits a resolved grid correction's Y step to +/-300 parent-coordinate units.
///
/// The room-bound probe is retained before the unconditional cap. Both paths
/// leave X/Z and the original signed 16.16 correction unchanged.
static inline void _actor401000CapGridPushHeight(ActorContactCappedPushScratch* push)
{
    s16 stepY;
    s16 cappedY;

    if (_actor401000HasRoomHeightClamp(&gGameSession->location)) {
        stepY = push->step.vy;
        if (((stepY >= 0) ? stepY : -stepY) <= ACTOR_401000_GRID_PUSH_MAX_Y) {
            return;
        }
        cappedY       = (stepY <= 0) ? -ACTOR_401000_GRID_PUSH_MAX_Y : ACTOR_401000_GRID_PUSH_MAX_Y;
        push->step.vy = cappedY;
    }
    stepY = push->step.vy;
    if (((stepY >= 0) ? stepY : -stepY) <= ACTOR_401000_GRID_PUSH_MAX_Y) {
        return;
    }
    push->step.vy = (stepY <= 0) ? -ACTOR_401000_GRID_PUSH_MAX_Y : ACTOR_401000_GRID_PUSH_MAX_Y;
}

/// Applies a capped room-grid correction and reports nonzero resolved X/Z.
///
/// Requires `contactCount` readable contacts (1..32767), a live root and an
/// initialized scratch stack. The resolver supplies signed 16.16 corrections:
/// Y is capped at 300 and X/Z length at 150 whole parent-coordinate units.
/// Fractional X/Z adds one further unit in its sign, after the cap. In a bounded
/// room, root Y is then clamped to the room bounds and raised by `heightOffset`.
/// This last adjustment runs even without a grid hit. Frozen actors return 0.
/// The caller invalidates root composition outside the room-clamp path.
static s32 _actor401000ApplyGridPushback(GfxCoord* root, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset)
{
    enum {
        WORLD_COLLISION_DELTA_FRACTION_BITS = 16,
        WORLD_COLLISION_DELTA_FRACTION_MASK = 0xFFFF
    };
    ActorContactCappedPushScratch* savedCursor;
    ActorContactCappedPushScratch* push;
    SVECTOR*                       step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    savedCursor                                         = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = savedCursor - 1;
    push                                                = savedCursor - 1;
    push->moved                                         = 0;
    // Apply whole-unit Y first, then capped X/Z and their fractional remainder.
    if (worldCollisionResolvePushback(contacts, &push->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        push->step.vx = savedCursor[-1].delta.fixed.vx.word >> WORLD_COLLISION_DELTA_FRACTION_BITS;
        push->step.vy = push->delta.fixed.vy.word >> WORLD_COLLISION_DELTA_FRACTION_BITS;
        push->step.vz = push->delta.fixed.vz.word >> WORLD_COLLISION_DELTA_FRACTION_BITS;
        _actor401000CapGridPushHeight(push);
        root->coord.t[1] += push->step.vy;
        push->stepLength  = push->step.vx * push->step.vx + push->step.vz * push->step.vz;
        push->stepLength  = SquareRoot0(push->stepLength);
        step              = &push->step;
        if (push->stepLength >= ACTOR_401000_GRID_PUSH_MAX_XZ) {
            push->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(ACTOR_401000_GRID_PUSH_MAX_XZ);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            root->coord.t[0] += push->step.vx;
            root->coord.t[2] += push->step.vz;
        } else {
            root->coord.t[0] += push->step.vx;
            root->coord.t[2] += push->step.vz;
        }
        if (push->delta.fixed.vx.word & WORLD_COLLISION_DELTA_FRACTION_MASK) {
            if (push->delta.fixed.vx.word > 0) {
                root->coord.t[0]++;
            } else {
                root->coord.t[0]--;
            }
        }
        if (push->delta.fixed.vz.word & WORLD_COLLISION_DELTA_FRACTION_MASK) {
            if (push->delta.fixed.vz.word > 0) {
                root->coord.t[2]++;
            } else {
                root->coord.t[2]--;
            }
        }
    }
    // Room bounds precede the root offset even when no grid contact was found.
    if (_actor401000HasRoomHeightClamp(&gGameSession->location)) {
        _actor401000ClampRootHeight(&gGameSession->location, root);
        root->coord.t[1] += heightOffset;
    }
    if (push->delta.fixed.vx.word != 0 || push->delta.fixed.vz.word != 0) {
        push->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return push->moved;
}

#include "../../shared/creatures/odd_stranger/odd_stranger_push_body.inc.c"

/// Reads the player's facing and the bearing from the player to this actor.
///
/// Requires live actor and player model roots in the same parent frame and
/// writable caller-owned chase scratch. Refreshes the signed-halfword XYZ
/// offset to the player, the player's matrix yaw and the reverse XZ bearing.
/// Angles use 4096 units per turn; the reverse bearing retains both +/-2048
/// endpoints. Reads local transforms without composing them; retains no pointer.
static inline void _actor401000ReadPlayerBearings(const Task* task, ActorChaseScratch* chase)
{
    chase->playerYaw = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                              gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
}

/// Pursues the player, choosing a sidestep, grab or renewed alert.
///
/// A stalking spawn selects `STALK` immediately. Entry starts the run clip and
/// enables drawing, lock-on and room-grid response. Later ticks steer by up to
/// 64 angle units (4096 per turn), move with the animation rate and resolve
/// grid/body contacts. Clear sight allows a grab within 1100 units; the turn
/// test retains its one-sided comparison. After 76 pursuit ticks it alerts.
/// Requires live actor/player roots, initialized work and scratch storage.
static void _actor401000Chase(Task* task)
{
    enum {
        ACTOR_401000_CHASE_SPAWN_MODE_MASK     = 0xF0,
        ACTOR_401000_CHASE_STALK_SPAWN         = 0x10,
        ACTOR_401000_CHASE_PLAYER_FACING_LIMIT = 68,
        ACTOR_401000_CHASE_SIDESTEP_TURN_LIMIT = 128,
        ACTOR_401000_CHASE_SIDESTEP_RANGE      = 1800,
        ACTOR_401000_CHASE_GRAB_RANGE          = 1100,
        ACTOR_401000_CHASE_GRAB_TURN_LIMIT     = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_401000_CHASE_BLOCKED_TURN_BIAS   = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_401000_CHASE_SIDE_SWITCH_TICKS   = 361,
        ACTOR_401000_CHASE_TURN_STEP           = 64,
        ACTOR_401000_CHASE_CLEARANCE_RADIUS    = 300,
        ACTOR_401000_CHASE_ALERT_TICKS         = 76
    };
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* chase;
    OddStrangerWork*   work;
    TmdObject*         model;
    GfxCoord*          root;
    s32                turnMagnitude;
    s32                playerFacingError;
    s32                spawnMode;

    spawnMode = (task->spawnArg1.value >> 16);
    work      = task->work;
    if ((spawnMode & ACTOR_401000_CHASE_SPAWN_MODE_MASK) == ACTOR_401000_CHASE_STALK_SPAWN) {
        work->state = ODD_STRANGER_STATE_STALK;
        return;
    }
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = ODD_STRANGER_ANIM_RUN;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        _oddStrangerDriveAnimation(task);
        work->dashCount   = 0;
        work->stateTimer  = 0;
        work->exitCounter = 0;
        return;
    }
    work->stateTimer++;
    work->exitCounter++;
    savedCursor = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase       = (SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &savedCursor[-1].delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
    // Compare player facing and actor bearing before choosing a sidestep or grab.
    _actor401000ReadPlayerBearings(task, chase);
    root                = task->extra.tmd->coords;
    chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-root->coord.m[2][0], root->coord.m[2][2]));
    work->lookYawTarget = chase->turn;
    playerFacingError   = chase->yawFromPlayer - chase->playerYaw;
    if (ABS(playerFacingError) < ACTOR_401000_CHASE_PLAYER_FACING_LIMIT) {
        if (work->sidestepDelay + work->sidestepCount / 2 < work->stateTimer) {
            turnMagnitude = chase->turn;
            if (turnMagnitude < 0) {
                turnMagnitude = -turnMagnitude;
            }
            if (turnMagnitude < ACTOR_401000_CHASE_SIDESTEP_TURN_LIMIT) {
                if (_oddStrangerOutOfRange(&chase->delta, ACTOR_401000_CHASE_SIDESTEP_RANGE) && _playerDetectionSightBlocked(task) != 1) {
                    work->state = ODD_STRANGER_STATE_SIDESTEP;
                }
            }
        }
    }
    if (_playerDetectionSightBlocked(task) != 1) {
        work->stateTimer++;
        root                = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-root->coord.m[2][0], root->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn < ACTOR_401000_CHASE_GRAB_TURN_LIMIT) {
            if (!_oddStrangerOutOfRange(&chase->delta, ACTOR_401000_CHASE_GRAB_RANGE) && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            }
        }
    } else {
        // The blocked-sight path resets this timer before its retained side-switch test.
        work->stateTimer    = 0;
        root                = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-root->coord.m[2][0], root->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (work->sidestepSide == 1) {
            chase->turn += ACTOR_401000_CHASE_BLOCKED_TURN_BIAS;
        } else {
            chase->turn -= ACTOR_401000_CHASE_BLOCKED_TURN_BIAS;
        }
        if (work->stateTimer >= ACTOR_401000_CHASE_SIDE_SWITCH_TICKS) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    if (chase->turn > ACTOR_401000_CHASE_TURN_STEP) {
        chase->turn = ACTOR_401000_CHASE_TURN_STEP;
    }
    if (chase->turn < -ACTOR_401000_CHASE_TURN_STEP) {
        chase->turn = -ACTOR_401000_CHASE_TURN_STEP;
    }
    chase->turn += ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ODD_STRANGER_ANIM_RUN) {
        if (work->blendActive == 0) {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_401000_CHASE_CLEARANCE_RADIUS, ((work->chaseRate + 2) * 0x78) / 0x12) != 0) {
                _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ((work->chaseRate + 2) * 0x78) / 0x12);
            }
        } else {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_401000_CHASE_CLEARANCE_RADIUS, ((work->chaseRate + 2) * 0x78) / 0x12 >> 2) != 0) {
                _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ((work->chaseRate + 2) * 0x78) / 0x12 >> 2);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = ODD_STRANGER_ANIM_RUN;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    // Prefer grid corrections; separate body overlap only when neither grid push moved X/Z.
    if (_actor401000ApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401000_GRID_ROOT_Y_OFFSET) != 1 && _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldCollisionClearContacts(work->hitContacts);
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    if (work->exitCounter >= ACTOR_401000_CHASE_ALERT_TICKS) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#include "../../shared/creatures/odd_stranger/odd_stranger_chase.inc.c"

/// Starts the player's struggle hold and enters pull only when it is accepted.
///
/// Requires live actor work, the player task and the selected grab animation bank.
/// Requests eight direction/face-button presses. Recovery rejects the hold with
/// reply 1, leaving actor state and animation untouched; reply 0 enters pull and
/// sets the held flag before replacing the player's bank and playing clip 1.
/// Requests are consumed synchronously; the installed animation data stays borrowed.
static inline void _oddStrangerTryStartPlayerHold(OddStrangerWork* work)
{
    enum { ODD_STRANGER_GRAB_HOLD_PRESS_COUNT = 8,
           ODD_STRANGER_GRAB_PLAYER_HOLD_ANIM = 1,
           ODD_STRANGER_GRAB_HOLD_ACCEPTED    = 0 };

    _gActor401000PlayerHoldRequest.pressCount = ODD_STRANGER_GRAB_HOLD_PRESS_COUNT;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &_gActor401000PlayerHoldRequest, 0) == ODD_STRANGER_GRAB_HOLD_ACCEPTED) {
        work->state                                         = ODD_STRANGER_STATE_GRAB_PULL;
        work->playerHeld                                    = 1;
        _gOddStrangerPlayerGrabAnimationRequest.animationId = ODD_STRANGER_GRAB_PLAYER_HOLD_ANIM;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &_gOddStrangerPlayerGrabAnimationRequest, 0);
    }
}

/// Reaches for the player and starts the Odd Stranger's hold on acceptance.
///
/// Requires initialized work/model, live player state and loaded grab banks.
/// Entry disables attacks and resolves grid contacts; the next tick saves the
/// root and faces the player. Offsets narrow to signed halfwords in a common
/// parent frame; angles use 4096 units per turn. Cue 16 requires a turn strictly
/// below 16 and range strictly below 1100 units. An accepted eight-press hold enters
/// GRAB_PULL and starts player animation 1. After the cue, retreats ten units
/// strictly within 1400 units, then corrects grid/body contacts. Direct retreat ignores
/// actor freeze. The animation boundary returns to CHASE.
/// Requires initialized scratch/GTE state; squared X/Z sums must fit s32.
static void _oddStrangerGrabReach(Task* task)
{
    enum {
        ODD_STRANGER_GRAB_CUE                 = 16,
        ODD_STRANGER_GRAB_TURN_LIMIT          = 16,
        ODD_STRANGER_GRAB_REACH               = 1100,
        ODD_STRANGER_GRAB_RETREAT_RADIUS      = 1400,
        ODD_STRANGER_GRAB_RETREAT_STEP        = 10,
        ODD_STRANGER_GRAB_PRIMARY_CHARACTER   = 1,
        ODD_STRANGER_GRAB_PRIMARY_BANK_OFFSET = 2,
        ODD_STRANGER_GRAB_COOLDOWN_TICKS      = 10
    };
    SVECTOR             playerOffset;
    OddStrangerWork*    work;
    Enemy*              enemy;
    const GameActor*    playerActor;
    const PlayerStatus* playerStatus;
    GfxCoord*           stepCoord;
    SVECTOR*            offsetVector;
    s16                 playerTurn;

    enemy        = task->spawnArg2.pointer;
    work         = task->work;
    playerActor  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = ANIMATION_RATE_ONE;
        work->animId                  = ODD_STRANGER_ANIM_GRAB_REACH;
        _oddStrangerDriveAnimation(task);
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        work->sidestepCount = 0;
        work->playerHeld    = 0;
        work->grabCooldown  = ODD_STRANGER_GRAB_COOLDOWN_TICKS;
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->stateTimer = 0;
        return;
    }
    // Resolve the entry contacts before fixing the facing on the next tick.
    if (++work->stateTimer == 1) {
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        work->grabStartPos.vx                 = task->extra.tmd->coords->coord.t[0];
        work->grabStartPos.vy                 = task->extra.tmd->coords->coord.t[1];
        work->grabStartPos.vz                 = task->extra.tmd->coords->coord.t[2];
        work->hitBody.radius                  = ODD_STRANGER_BODY_RADIUS;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, _actorAngleTurnToPlayer(task, &playerOffset, playerStatus), GRAPHICS_ROTATION_COMPOSE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
        playerOffset.vx                       = task->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        playerOffset.vy                       = 0;
        playerOffset.vz                       = task->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
        work->grabCooldown                    = ODD_STRANGER_GRAB_COOLDOWN_TICKS;
    }
    _oddStrangerDriveAnimation(task);
    // Only the reach cue can request a hold from a nonscripted player.
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ODD_STRANGER_GRAB_CUE && playerActor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        playerTurn = _actorAngleTurnToMatrixPosition(task, &playerOffset, gPlayerStatus.coordMtx);
        if (abs(playerTurn) < ODD_STRANGER_GRAB_TURN_LIMIT && !_oddStrangerOutOfRange(&playerOffset, ODD_STRANGER_GRAB_REACH)) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ODD_STRANGER_GRAB_PRIMARY_CHARACTER) {
                _gOddStrangerPlayerGrabAnimationRequest.source.sets = &_gActor401000PlayerGrabAnimSets[ODD_STRANGER_GRAB_PRIMARY_BANK_OFFSET];
            } else {
                _gOddStrangerPlayerGrabAnimationRequest.source.sets = _gActor401000PlayerGrabAnimSets;
            }
            _oddStrangerTryStartPlayerHold(work);
        }
    }
    if (work->animId == ODD_STRANGER_ANIM_GRAB_REACH && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    // After the reach cue, step away along the horizontal separation.
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= ODD_STRANGER_GRAB_CUE + 1) {
        offsetVector    = &playerOffset;
        playerOffset.vx = task->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        playerOffset.vy = 0;
        playerOffset.vz = task->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        if (!_oddStrangerOutOfRange(offsetVector, ODD_STRANGER_GRAB_RETREAT_RADIUS)) {
            VectorNormalSS(offsetVector, offsetVector);
            gte_lddp(ODD_STRANGER_GRAB_RETREAT_STEP);
            gte_ldsv(offsetVector);
            gte_gpf12();
            gte_stsv(offsetVector);
            stepCoord                             = task->extra.tmd->coords;
            stepCoord->coord.t[0]                += playerOffset.vx;
            stepCoord                             = task->extra.tmd->coords;
            stepCoord->coord.t[2]                += playerOffset.vz;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_grab.inc.c"

/// Staggers backward after a front hit, then plays the back-down animation.
///
/// While staggering, takes an 87-unit backward step when the player-clearance
/// probe permits it and resolves both contact tables. At the down clip boundary,
/// health and status buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000FallBack(Task* task)
{
    enum {
        ACTOR_401000_FALL_BACK_STAGGER_ANIM     = 10,
        ACTOR_401000_FALL_BACK_STEP             = 87,
        ACTOR_401000_FALL_BACK_CLEARANCE_RADIUS = 300
    };
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_401000_FALL_BACK_STAGGER_ANIM;
        work->blendActive             = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (work->animId == ACTOR_401000_FALL_BACK_STAGGER_ANIM && (s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_401000_FALL_BACK_CLEARANCE_RADIUS, -ACTOR_401000_FALL_BACK_STEP) != 0) {
        _actorMovementStepForward(task->extra.tmd->coords, -ACTOR_401000_FALL_BACK_STEP);
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        // The stagger finishes before the back-down clip begins.
        if (work->animId == ACTOR_401000_FALL_BACK_STAGGER_ANIM) {
            work->animId      = ODD_STRANGER_ANIM_DOWN_BACK;
            work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
            _oddStrangerDriveAnimation(task);
        }
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == ODD_STRANGER_ANIM_DOWN_BACK) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                    work->state = ODD_STRANGER_STATE_STATUS_HOLD;
                } else {
                    work->state = ODD_STRANGER_STATE_DOWN;
                }
            } else {
                work->state = ODD_STRANGER_STATE_DEATH_BURN;
            }
        }
    }
}

/// Restores grid response after a fall and selects the next downed state.
///
/// Borrows initialized writable body work and its live enemy record through
/// the call. Nonpositive HP selects burning death; living enemies with buildup
/// enter status hold, otherwise stay down. Other body flags are retained, and
/// the reaction flag is tested without consuming it. Call after a fall boundary
/// or a settled interrupted-rise pose; no animation or lifetime changes occur here.
static inline void _actor401000FinishFall(OddStrangerWork* work, Enemy* enemy)
{
    work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    if (enemy->hp <= 0) {
        work->state = ODD_STRANGER_STATE_DEATH_BURN;
    } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        work->state = ODD_STRANGER_STATE_STATUS_HOLD;
    } else {
        work->state = ODD_STRANGER_STATE_DOWN;
    }
}

/// Plays the front-down animation after a hit from behind.
///
/// Resolves both contact tables while the fall plays. At its clip boundary,
/// health and status buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000FallFront(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId                  = ODD_STRANGER_ANIM_DOWN_FRONT;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        _actor401000FinishFall(work, enemy);
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_die.inc.c"

/// Waits in place until the player enters the notice radius or makes combat noise.
///
/// Alternates idle clips at control jumps and clip boundaries. After 2400 quiet
/// ticks, a random one-in-sixteen gate skips the whole update, including notice
/// tests. Entry saves the model's color matrix in work storage.
static void _actor401000Dormant(Task* task)
{
    enum {
        ACTOR_401000_DORMANT_IDLE_ANIM   = 14,
        ACTOR_401000_DORMANT_SHIFT_ANIM  = 15,
        ACTOR_401000_DORMANT_QUIET_TICKS = 2400,
        ACTOR_401000_DORMANT_SKIP_MASK   = 15
    };
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        root;
    SVECTOR          delta;
    SVECTOR*         playerDelta;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model        = task->extra.tmd;
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = ACTOR_401000_DORMANT_IDLE_ANIM;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    // Long-idle throttling also skips awareness and animation work on this tick.
    if (work->stateTimer > ACTOR_401000_DORMANT_QUIET_TICKS) {
        RANDOM_LCG_ADVANCE();
        if (!(RANDOM_LCG_DRAW() & ACTOR_401000_DORMANT_SKIP_MASK)) {
            return;
        }
    } else {
        work->stateTimer = (u16)work->stateTimer + 1;
    }
    root            = task->extra.tmd->coords;
    playerDelta     = &delta;
    delta.vx        = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
    playerDelta->vy = gPlayerStatus.coordMtx->t[1] - root->coord.t[1];
    playerDelta->vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
    if (!_oddStrangerOutOfRange(playerDelta, work->noticeRadius)) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        sceneEngageBattle(1);
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    _oddStrangerDriveAnimation(task);
    if (work->animId == ACTOR_401000_DORMANT_IDLE_ANIM && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
        RANDOM_LCG_ADVANCE();
        if (RANDOM_LCG_DRAW() & 1) {
            work->animId      = ACTOR_401000_DORMANT_SHIFT_ANIM;
            work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
            _oddStrangerDriveAnimation(task);
        }
    }
    if (work->animId == ACTOR_401000_DORMANT_SHIFT_ANIM && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = ACTOR_401000_DORMANT_IDLE_ANIM;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        _oddStrangerDriveAnimation(task);
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_dormant.inc.c"

/// Adds a part pitch and refreshes the selected composition coordinate.
///
/// Rotation and composition indices remain separate for the retained part-5
/// rotation / part-4 refresh. Use only inside a braced statement block. `task`
/// is evaluated three times and `composeIndex` twice, so arguments must be free
/// of side effects. Pitch uses 4096 units per turn.
#define ACTOR_401000_APPLY_AMBUSH_PITCH(task, partIndex, composeIndex, pitch)                         \
    gfxRotMatrixX(&(task)->extra.tmd->coords[(partIndex)].coord, (pitch), GRAPHICS_ROTATION_COMPOSE); \
    (task)->extra.tmd->coords[(composeIndex)].composeStamp = GRAPHICS_COORD_DIRTY;                    \
    actorRenderComposeCoord(&(task)->extra.tmd->coords[(composeIndex)])

/// Holds a bent ambush pose, straightens and turns toward the player, then chases.
///
/// Entry restarts the ambush clip with two pose ticks. Look yaw advances by at
/// most 40 angle units per tick. The part pitches hold for 49 ticks and then
/// decay every four ticks; root yaw turns by at most 36 units. Alignment or tick
/// 79 selects `CHASE`. Angles use 4096 units per turn. Requires initialized
/// work, model parts 0..5, a live player and one free chase scratch block.
static void _actor401000Ambush(Task* task)
{
    enum {
        ACTOR_401000_AMBUSH_ANIM              = 19,
        ACTOR_401000_AMBUSH_POSE_HOLD_TICKS   = 50,
        ACTOR_401000_AMBUSH_CHASE_TICKS       = 79,
        ACTOR_401000_AMBUSH_LOOK_STEP         = 40,
        ACTOR_401000_AMBUSH_TURN_STEP         = 36,
        ACTOR_401000_AMBUSH_PART1_PITCH       = ACTOR_TRANSFORM_ANGLE_TURN / 64,
        ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH = ACTOR_TRANSFORM_ANGLE_TURN / 32,
        ACTOR_401000_AMBUSH_PART5_PITCH       = ACTOR_TRANSFORM_ANGLE_TURN / 16
    };
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         model;
    GfxCoord*          root;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animRate          = 8;
        work->animId            = ACTOR_401000_AMBUSH_ANIM;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        _oddStrangerDriveAnimation(task);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = _actorAngleTurnToPlayer(task, &aim->delta, &gPlayerStatus);
    if (work->lookYawTarget < aim->turn) {
        if (aim->turn - work->lookYawTarget > ACTOR_401000_AMBUSH_LOOK_STEP) {
            work->lookYawTarget += ACTOR_401000_AMBUSH_LOOK_STEP;
        } else {
            work->lookYawTarget = aim->turn;
        }
    } else if (work->lookYawTarget - aim->turn > ACTOR_401000_AMBUSH_LOOK_STEP) {
        work->lookYawTarget -= ACTOR_401000_AMBUSH_LOOK_STEP;
    } else {
        work->lookYawTarget = aim->turn;
    }
    root      = task->extra.tmd->coords;
    aim->turn = ratan2(-root->coord.m[2][0], root->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, aim->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    _oddStrangerDriveAnimation(task);
    // Hold the bent pose, then stagger the pitch decay of its five parts.
    if (work->stateTimer < ACTOR_401000_AMBUSH_POSE_HOLD_TICKS) {
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 1, 1, ACTOR_401000_AMBUSH_PART1_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 2, 2, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 3, 3, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 4, 4, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH);
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 5, 4, ACTOR_401000_AMBUSH_PART5_PITCH);
    } else {
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 1, 1, ACTOR_401000_AMBUSH_PART1_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 1)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 2, 2, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 2)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 3, 3, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 3)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 4, 4, ACTOR_401000_AMBUSH_MIDDLE_PART_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 4)) / 4));
        ACTOR_401000_APPLY_AMBUSH_PITCH(task, 5, 4, ACTOR_401000_AMBUSH_PART5_PITCH >> ((work->stateTimer - (ACTOR_401000_AMBUSH_POSE_HOLD_TICKS - 1)) / 4));
        aim->turn = _actorAngleTurnToPlayer(task, &aim->delta, &gPlayerStatus);
        if (aim->turn > ACTOR_401000_AMBUSH_TURN_STEP) {
            aim->turn = ACTOR_401000_AMBUSH_TURN_STEP;
        } else if (aim->turn < -ACTOR_401000_AMBUSH_TURN_STEP) {
            aim->turn = -ACTOR_401000_AMBUSH_TURN_STEP;
        }
        if (ABS(aim->turn) < ACTOR_401000_AMBUSH_TURN_STEP || work->stateTimer >= ACTOR_401000_AMBUSH_CHASE_TICKS) {
            work->state = ODD_STRANGER_STATE_CHASE;
        }
        root       = task->extra.tmd->coords;
        aim->turn += ratan2(-root->coord.m[2][0], root->coord.m[2][2]);
        gfxRotMatrixY(&task->extra.tmd->coords->coord, aim->turn, GRAPHICS_ROTATION_REPLACE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ACTOR_401000_APPLY_AMBUSH_PITCH

/// Hides the Odd Stranger and bursts four detached body-part models.
///
/// Requires live work, enemy placement, coordinates through part 12 and loaded
/// effects. Entry emits a size-768 subtractive gravity particle and releases
/// the battle reference with rewards. Ticks 3/5/7/8
/// replace bank-10 model slot 5 immediately before spawning its debris; the
/// effects copy their offsets and inherit placement textures. Tick 5 retains
/// the +100 Z offset from tick 3. At tick 61 selects the hidden state.
/// The timer wraps as a halfword and its comparisons use signed values.
static void _actor401000DeathBurst(Task* task)
{
    enum {
        ACTOR_401000_BURST_PARTICLE_ARG = 0x10300,
        ACTOR_401000_BURST_PART_SIZE    = 512,
        ACTOR_401000_BURST_OFFSET       = 100,
        ACTOR_401000_BURST_MODEL_SLOT   = 5,
        ACTOR_401000_BURST_FIRST_TICK   = 3,
        ACTOR_401000_BURST_SECOND_TICK  = 5,
        ACTOR_401000_BURST_BODY_TICK    = 7,
        ACTOR_401000_BURST_HEAD_TICK    = 8,
        ACTOR_401000_BURST_HIDE_TICK    = 61,
    };
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              nextTick;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        work->effectOffset.vx         = ACTOR_401000_BURST_OFFSET;
        work->effectOffset.vz         = 0;
        work->effectOffset.vy         = 0;
        effectSpawn(EFFECT_030, task->extra.tmd->coords + 1, ACTOR_401000_BURST_PARTICLE_ARG, &work->effectOffset);
        sceneReleaseBattleRefWithRewards(task, 0xA);
    }
    // The shared model descriptor must select each part before its spawn.
    nextTick         = work->stateTimer + 1;
    work->stateTimer = nextTick;
    if ((s16)nextTick == ACTOR_401000_BURST_FIRST_TICK) {
        gEffectBurstTaskDescriptors[ACTOR_401000_BURST_MODEL_SLOT].data.model = &_gStrangerBurstHand;
        work->effectOffset.vz                                                 = ACTOR_401000_BURST_OFFSET;
        work->effectOffset.vy                                                 = 0;
        work->effectOffset.vx                                                 = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 9, ACTOR_401000_BURST_PART_SIZE, &work->effectOffset), enemy);
    }
    if (work->stateTimer == ACTOR_401000_BURST_SECOND_TICK) {
        gEffectBurstTaskDescriptors[ACTOR_401000_BURST_MODEL_SLOT].data.model = &_gStrangerOddBBurstLeg;
        work->effectOffset.vy                                                 = 0;
        work->effectOffset.vx                                                 = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 12, ACTOR_401000_BURST_PART_SIZE, &work->effectOffset), enemy);
    }
    if (work->stateTimer == ACTOR_401000_BURST_BODY_TICK) {
        gEffectBurstTaskDescriptors[ACTOR_401000_BURST_MODEL_SLOT].data.model = &_gStrangerOddBBurstTorso;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 1, ACTOR_401000_BURST_PART_SIZE, NULL), enemy);
    }
    if (work->stateTimer == ACTOR_401000_BURST_HEAD_TICK) {
        gEffectBurstTaskDescriptors[ACTOR_401000_BURST_MODEL_SLOT].data.model = &_gStrangerOddBBurstHead;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 3, ACTOR_401000_BURST_PART_SIZE, NULL), enemy);
    }
    if (work->stateTimer >= ACTOR_401000_BURST_HIDE_TICK) {
        work->state = ODD_STRANGER_STATE_HIDDEN;
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_walking_death.inc.c"

/// Restarts the back-down pose after a hit interrupts rising from the back.
///
/// Resolves both contact tables. Once the pose is settled, health and status
/// buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000RefallBack(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ODD_STRANGER_ANIM_DOWN_BACK;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401000FinishFall(work, enemy);
    }
}

/// Restarts the front-down pose after a hit interrupts rising from the front.
///
/// Resolves both contact tables. Once the pose is settled, health and status
/// buildup select `DEATH_BURN`, `STATUS_HOLD` or `DOWN`.
static void _actor401000RefallFront(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ODD_STRANGER_ANIM_REFALL_FRONT;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _oddStrangerDriveAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401000FinishFall(work, enemy);
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_tick.inc.c"

/// Ignores message 2015 without accessing the receiver or either payload.
///
/// The binary leaves the reply register unspecified, so callers must ignore
/// the result. The signed callback signature is retained without inventing a reply.
static s32 _actor401000IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra)
{
}

/// Enemy task handlers: state 0 spawns, state 1 ticks, state 2 destroys.
///
/// Copied before unchecked dispatch with the live enemy and its owning task;
/// the table has no terminator and all three callbacks remain loaded.
static const EnemyTaskFuncTable3 _gActor401000TaskStates = { {
    _actor401000Spawn,
    _oddStrangerTick,
    enemyDestroy,
} };

#include "../../shared/creatures/odd_stranger/odd_stranger_play_message.inc.c"

#include "../../shared/actor/actor_messages_visibility.inc.c"

#include "../../shared/actor/actor_messages_is_present.inc.c"

/// Selects the actor_401000 work layout for recording placement heading.
///
/// The following XYZ-placement fragment borrows live `Task::work` as
/// `OddStrangerWork` with `ODD_STRANGER_BUILD_401000` selected and writes its
/// `s16 placedYaw` in 4096 units per turn. Scope the binding to that include.
#define ACTOR_MESSAGE_YAW_WORK_TYPE OddStrangerWork
#include "../../shared/actor/actor_messages_place_yaw.inc.c"
#undef ACTOR_MESSAGE_YAW_WORK_TYPE

#include "../../shared/actor/actor_messages_release_hold.inc.c"

#include "../../shared/creatures/odd_stranger/odd_stranger_apply_command.inc.c"

/// Keeps the hidden actor out of active drawing and lock-on.
///
/// On entry disables attack pair contacts and leaves room-grid response enabled;
/// later ticks wait for another handler or room message to change the state.
static void _actor401000Hidden(Task* task)
{
    TmdObject*       model;
    OddStrangerWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                                              = (u16)(model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags                                    = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags                                      = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_script_pose.inc.c"

/// Rises from the front-down pose and resumes chasing at the clip boundary.
///
/// Entry enables drawing, lock-on and room-grid response and restarts the rise
/// animation at the actor's chase rate.
static void _actor401000RiseFront(Task* task)
{
    enum { ACTOR_401000_RISE_FRONT_ANIM = 22 };
    OddStrangerWork* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_401000_RISE_FRONT_ANIM;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
}

#include "../../shared/creatures/odd_stranger/odd_stranger_idle.inc.c"

#undef ODD_STRANGER_BUILD

/// Dispatches the Odd Stranger's spawn, frame update or enemy destruction.
///
/// Task state must be 0, 1 or 2, respectively; dispatch is unchecked. The second
/// spawn argument borrows a live Enemy. Spawn establishes task-owned work and
/// subsequent updates require its work, model, placement and clip data to live.
static void _actor401000StrangerTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = _gActor401000TaskStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
