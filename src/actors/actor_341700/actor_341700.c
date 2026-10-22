#include "actors/actor_341700.h"

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
#include "../../shared/creatures/mad_chaser/mad_chaser.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.

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

/// Phases of the combat walk and transition to the leap attack.
///
/// `MadChaserWork::subState` selects 0 start approach, 1 approach, 2 finish the
/// walk cycle. All entries are non-NULL void(Task*) callbacks. Dispatch copies
/// the complete table before testing the hit interrupt; keep the carrier loaded.
static const TaskFuncTable3 _gMadChaserWalkSteps = { {
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
/// `MadChaserWork::subState` selects 0 cry, 1 wait, 2 release the shared alert
/// claim, 3 start sidestep, 4 sidestep. Dispatch copies all five non-NULL
/// void(Task*) callbacks; the index must be in range and the carrier loaded.
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

extern TmdSource gChaserMadBody;

/// Detached Mad Chaser head thrown from body part 6 during a burst death.
///
/// One rigid part, with 62 textured triangles and 15 textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
#include "assets/enemies/chaser/mad/burst_head.inc"

/// Detached Mad Chaser arm thrown from body part 8 during a burst death.
///
/// One rigid part, with 34 textured triangles and nine textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
#include "assets/enemies/chaser/mad/burst_arm.inc"

/// Detached Mad Chaser tail thrown from body part 2 during a burst death.
///
/// One rigid part, with 18 textured triangles and 14 textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
#include "assets/enemies/chaser/mad/burst_tail.inc"

#include "assets/enemies/chaser/mad/body.inc"

/// The Mad Chaser's single attack: base damage 22 and an ordinary hit with no status.
///
/// The attack sphere packs entry 0 through the enemy parameters. The entry and
/// parameters remain borrowed from this carrier for the enemy's lifetime.
static DamageAttack _gMadChaserAttacks[1] = {
    { 22, GAME_ACTOR_REACTION_ORDINARY },
};

/// Shared Mad Chaser tuning and release rewards for this overlay's instances.
///
/// Ordinary and hidden spawn paths borrow the record and seed current/maximum
/// HP to 110. Its attack table is overlay-owned; rewards are 20 EXP, 40 BP, 1 MP.
static EnemyParams _gMadChaserEnemyParams = { _gMadChaserAttacks, 110, 20, 40, 1, 100, 10, 100, 0 };

#include "assets/rooms/shelter/b3_dumping_hole/mad_chaser_clips.inc"

/// Loaded animation sets addressed directly by the one-based clip ID.
///
/// Entries 1..19 are playable; entries 0 and 20 are NULL, not clips. The
/// animation context borrows the pointer table and all tracks until teardown.
#include "assets/rooms/shelter/b3_dumping_hole/mad_chaser_bank.inc"

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

enum { ACTOR_341700_MAD_CHASER_TASK_PRIORITY = 96 };

TaskDesc gActor341700MadChaserTaskDescs[ACTOR_341700_MAD_CHASER_TASK_COUNT] = {
    { { { TASK_BODY_TMD, ACTOR_341700_MAD_CHASER_TASK_PRIORITY } }, _madChaserTask, { .model = &gChaserMadBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_341700_MAD_CHASER_TASK_PRIORITY } }, _madChaserHiddenTask, { .model = &gChaserMadBody } },
    { { { TASK_BODY_COORD, ACTOR_341700_MAD_CHASER_TASK_PRIORITY } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_TMD, ACTOR_341700_MAD_CHASER_TASK_PRIORITY } }, _madChaserHiddenTask, { .model = &gChaserMadBody } },
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

/// Unreferenced bytes before this package's prop geometry; their role is unproven.
static u8 _gActor341700Unknown12F90[20] = { 2, 70, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 0, 62, 200, 210, 1 };

static inline void _madChaserResetBehaviorStateS16(Task* task, s16 behaviorState);

#include "../../shared/creatures/mad_chaser/mad_chaser_limb_shadow.inc.c"

/* The records closing three of the overlay's model streams, selected through
   `gTaskBank2Descs[TASK_BANK2_BURST_MODEL_PART_SLOT].data.model`. */

#include "../../shared/creatures/mad_chaser/mad_chaser_spawn_gibs.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_inlines.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_spawn.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_contacts.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_tick_anim.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_recoil_recover.inc.c"

/// Lurking behaviors shared by the ordinary and scripted Mad Chaser tasks.
///
/// `MadChaserWork::state` selects 0 idle, 1 look, 2 rise, 3 alert, 4 shift
/// (`MAD_CHASER_LURK_STATE_*`). Dispatch copies all five non-NULL void(Task*)
/// callbacks; the index must be in range and the carrier must remain loaded.
static const TaskFuncTable5 _gMadChaserLurkStates = { {
    _madChaserLurkIdleState,
    _madChaserLurkLookState,
    _madChaserLurkRiseState,
    _madChaserLurkAlertState,
    _madChaserLurkShiftState,
} };

/// Supplies this carrier's lurking behavior table to the shared frame update.
///
/// Must be a readable, side-effect-free `TaskFuncTable5` value, copied once at
/// frame entry even when actors are paused or hidden. Slots 0..4 are idle, look,
/// rise, alert and shift (`MAD_CHASER_LURK_STATE_*`), selected by
/// `MadChaserWork::state` without a bounds check. Every entry is a non-NULL
/// `void(Task*)` callback that leaves the task, model, enemy and work live for
/// the rest of the frame; callback code must remain loaded through the call.
/// Bind before `mad_chaser_lurk_tick.inc.c`; that fragment undefines the binding.
#define MAD_CHASER_LURK_STATE_HANDLERS _gMadChaserLurkStates
#include "../../shared/creatures/mad_chaser/mad_chaser_lurk_tick.inc.c"

/// Phases of the lurking idle hold and transition to looking.
///
/// `MadChaserWork::subState` selects 0 start idle hold, 1 wait, 2 finish idle.
/// Dispatch copies all three non-NULL void(Task*) callbacks before checking
/// for a shared alert claim; the index must be in range and the carrier loaded.
static const TaskFuncTable3 _gMadChaserLurkIdleSteps = { {
    _madChaserLurkStartIdleHold,
    _madChaserLurkWait,
    _madChaserLurkIdleEnd,
} };

/// Phases of the lurking look sequence.
///
/// `MadChaserWork::subState` selects 0 prepare look, 1 start look hold,
/// 2 look around. Dispatch copies all three non-NULL void(Task*) callbacks
/// before checking for a shared alert claim; keep the index in range and carrier loaded.
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

#include "../../shared/creatures/mad_chaser/mad_chaser_lurk_look.inc.c"

/// Selects a signed-halfword behavior index and resets its sub-state to the start.
///
/// Requires live MadChaserWork and an index valid for the active behavior table.
/// Retains the task state, animation and counters. This local copy has no users;
/// active transitions use the shared `_madChaserSetBehaviorStateS16` helper.
static inline void _madChaserResetBehaviorStateS16(Task* task, s16 behaviorState)
{
    enum { MAD_CHASER_BEHAVIOR_START_SUBSTATE = 0 };
    MadChaserWork* work = task->work;

    work->state    = behaviorState;
    work->subState = MAD_CHASER_BEHAVIOR_START_SUBSTATE;
}

#include "../../shared/creatures/mad_chaser/mad_chaser_emerge_at_spot.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_creep.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_emerge.inc.c"

/// Selects the carrier's static void(Task*) creep handler for emerge behavior 9.
#define MAD_CHASER_EMERGE_CREEP_HANDLER _madChaserEmergeCreep9
#include "../../shared/creatures/mad_chaser/mad_chaser_creep.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_pulled_struggle.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_to_alert.inc.c"

/// Selects the static void(Task*) alert transition for combat table slot 1.
#define MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER _madChaserCombatToAlertState1
#include "../../shared/creatures/mad_chaser/mad_chaser_to_alert.inc.c"
#undef MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER

/// Selects the static void(Task*) alert transition for combat table slot 2.
#define MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER _madChaserCombatToAlertState2
#include "../../shared/creatures/mad_chaser/mad_chaser_to_alert.inc.c"
#undef MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER

#define MAD_CHASER_STEP_STATE _madChaserWalkState
/// Selects the combat walk's three-phase table for this dispatch instance.
///
/// A readable `TaskFuncTable3` value, copied once before the hit interrupt.
/// When uninterrupted, `subState` 0..2 selects start, approach or finish.
/// All entries are non-NULL void(Task*) callbacks; keep their code loaded.
/// Undefine this binding after the fragment; it has no default.
#define MAD_CHASER_STEP_TABLE _gMadChaserWalkSteps
/// Selects the hit-request predicate that can interrupt the combat walk.
///
/// Names a declared s16(Task*) function; the task must have live MadChaserWork.
/// Called once after the table copy and before reading `subState`: zero allows
/// step dispatch, nonzero skips it. A hit latch equal to one consumes the
/// reaction and interrupts even when no combat behavior is selected.
/// Bind only for this fragment inclusion, then undefine; there is no default.
#define MAD_CHASER_STEP_INTERRUPT_HANDLER _madChaserTakeHitRequest
#include "../../shared/creatures/mad_chaser/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_STEP_TABLE
#undef MAD_CHASER_STEP_INTERRUPT_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_leap_state.inc.c"
#undef MAD_CHASER_ALERT_STATE_HANDLER

/// Selects this carrier's declared static void(Task*) light-recoil dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserRecoilLightState
/// Selects the light-recoil entry callback for this reaction instance.
///
/// Must name a declared void(Task*) callback for subState 0. Entry borrows the
/// live task, requests the reaction pose and advances subState to 1.
/// Bind only for this fragment inclusion, then undefine; there is no default.
#define MAD_CHASER_REACTION_ENTRY_HANDLER _madChaserRecoilLight
/// Selects the light-recoil continuation for this reaction instance.
///
/// Must name a declared void(Task*) callback for subState 1, borrowing live
/// task-owned MadChaserWork. Continues recovery or changes behavior and resets
/// subState; animation ticking belongs to the combat frame callback.
/// Bind only for this fragment inclusion, then undefine; there is no default.
#define MAD_CHASER_REACTION_CONTINUE_HANDLER _madChaserRecoilLightRecover
#include "../../shared/creatures/mad_chaser/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER
#undef MAD_CHASER_REACTION_ENTRY_HANDLER
#undef MAD_CHASER_REACTION_CONTINUE_HANDLER

/// Selects this carrier's declared static void(Task*) heavy-recoil dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserRecoilHeavyState
/// Selects the declared void(Task*) entry that starts heavy recoil at subState 0.
#define MAD_CHASER_REACTION_ENTRY_HANDLER _madChaserRecoilHeavy
/// Selects the declared void(Task*) heavy-recoil continuation at subState 1.
#define MAD_CHASER_REACTION_CONTINUE_HANDLER _madChaserRecoilHeavyEnd
#include "../../shared/creatures/mad_chaser/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER
#undef MAD_CHASER_REACTION_ENTRY_HANDLER
#undef MAD_CHASER_REACTION_CONTINUE_HANDLER

/// Selects this carrier's declared static void(Task*) buildup-status dispatcher.
#define MAD_CHASER_REACTION_STATE_HANDLER _madChaserStatusHoldState
/// Selects the declared void(Task*) entry that starts status hold at subState 0.
#define MAD_CHASER_REACTION_ENTRY_HANDLER _madChaserStatusHoldStart
/// Selects the declared void(Task*) buildup-status continuation at subState 1.
#define MAD_CHASER_REACTION_CONTINUE_HANDLER _madChaserStatusHold
#include "../../shared/creatures/mad_chaser/mad_chaser_recoil_light_state.inc.c"
#undef MAD_CHASER_REACTION_STATE_HANDLER
#undef MAD_CHASER_REACTION_ENTRY_HANDLER
#undef MAD_CHASER_REACTION_CONTINUE_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_knockdown_state.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_dangle_state.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_death_wait_anim.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_begin_death.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_death_translucent.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_start_despawn.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_death_pause.inc.c"

/// Runs the lurk idle hold, yielding to a claimed shared alert.
///
/// Requires live Mad Chaser work and `subState` in 0..2: start the idle hold,
/// wait for its timeout or player proximity, then wait to enter the look state.
#define MAD_CHASER_STEP_STATE _madChaserLurkIdleState
/// Selects the lurk idle hold's three-phase table for this dispatch instance.
///
/// A readable `TaskFuncTable3` value, copied once before the alert interrupt.
/// When uninterrupted, `subState` 0..2 selects start hold, wait or enter look.
/// All entries are non-NULL void(Task*) callbacks; keep their code loaded.
/// Undefine this binding after the fragment; it has no default.
#define MAD_CHASER_STEP_TABLE _gMadChaserLurkIdleSteps
/// Selects the shared-alert predicate that can interrupt the lurk idle hold.
///
/// Names a declared s16(Task*) function; the task must have live MadChaserWork.
/// Called once after the table copy and before reading `subState`: zero allows
/// step dispatch, nonzero skips it. Any claimed shared alert enters combat at
/// its alert behavior and interrupts this dispatch.
/// Bind only for this fragment inclusion, then undefine; there is no default.
#define MAD_CHASER_STEP_INTERRUPT_HANDLER _madChaserJoinAlert
#include "../../shared/creatures/mad_chaser/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_STEP_TABLE
#undef MAD_CHASER_STEP_INTERRUPT_HANDLER

/// Runs the lurk look sequence, yielding to a claimed shared alert.
///
/// Requires live Mad Chaser work and `subState` in 0..2: prepare the transition,
/// start a timed look hold, then sway or face the player until rise or alert.
#define MAD_CHASER_STEP_STATE _madChaserLurkLookState
/// Selects the lurk look hold's three-phase table for this dispatch instance.
///
/// A readable `TaskFuncTable3` value, copied once before the alert interrupt.
/// When uninterrupted, `subState` 0..2 selects prepare, start hold or look around.
/// All entries are non-NULL void(Task*) callbacks; keep their code loaded.
/// Undefine this binding after the fragment; it has no default.
#define MAD_CHASER_STEP_TABLE _gMadChaserLurkLookSteps
/// Selects the shared-alert predicate that can interrupt the lurk look hold.
///
/// Names a declared s16(Task*) function; the task must have live MadChaserWork.
/// Called once after the table copy and before reading `subState`: zero allows
/// step dispatch, nonzero skips it. Any claimed shared alert enters combat at
/// its alert behavior and interrupts this dispatch.
/// Bind only for this fragment inclusion, then undefine; there is no default.
#define MAD_CHASER_STEP_INTERRUPT_HANDLER _madChaserJoinAlert
#include "../../shared/creatures/mad_chaser/mad_chaser_walk_state.inc.c"
#undef MAD_CHASER_STEP_STATE
#undef MAD_CHASER_STEP_TABLE
#undef MAD_CHASER_STEP_INTERRUPT_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_lurk.inc.c"

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
#include "../../shared/creatures/mad_chaser/mad_chaser_dangle_state.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_start_hold.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_death_cry_unlink.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_death_settle_quiet.inc.c"

/// Selects the private command-death animation wait instance.
///
/// Must name a statically declared void(Task*) callback. The following fragment
/// include consumes this identifier-only binding; undefine it afterwards so the
/// ordinary death instance remains `_madChaserDeathWaitAnim`.
#define MAD_CHASER_DEATH_WAIT_ANIM_HANDLER _madChaserCommandDeathWaitAnimBoundary
#include "../../shared/creatures/mad_chaser/mad_chaser_death_wait_anim.inc.c"
#undef MAD_CHASER_DEATH_WAIT_ANIM_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_drop_bodies.inc.c"

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
#include "../../shared/creatures/mad_chaser/mad_chaser_death_cry_unlink.inc.c"
#undef MAD_CHASER_SHRINK_DEATH_START_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_begin_shrink.inc.c"

/// Selects the private translucency-delay state of scripted shrink death.
///
/// Names a statically declared void(Task*) callback for the following fragment.
/// This identifier-only binding captures no runtime values; undefine it after
/// inclusion to restore the ordinary-death definition.
#define MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER _madChaserShrinkDeathTurnTranslucent
#include "../../shared/creatures/mad_chaser/mad_chaser_death_translucent.inc.c"
#undef MAD_CHASER_SHRINK_DEATH_TRANSLUCENT_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_shrink.inc.c"

/// Selects the private final behavior of scripted shrink death.
///
/// Names a statically declared void(Task*) callback for the following fragment.
/// This identifier-only binding captures no runtime values; undefine it after
/// inclusion to restore the ordinary-death definition.
#define MAD_CHASER_SHRINK_DEATH_DESPAWN_HANDLER _madChaserShrinkDeathStartDespawn
#include "../../shared/creatures/mad_chaser/mad_chaser_start_despawn.inc.c"
#undef MAD_CHASER_SHRINK_DEATH_DESPAWN_HANDLER

#include "../../shared/creatures/mad_chaser/mad_chaser_take_knockdown_request.inc.c"
