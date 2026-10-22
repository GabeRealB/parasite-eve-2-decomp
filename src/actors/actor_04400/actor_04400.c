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
#include "../../shared/creatures/mad_chaser/mad_chaser.h"

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

static const TaskFuncTable3 gMadChaserWalkSteps;

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

#include "assets/enemies/chaser/mad/package_1_clips.inc"

/// Loaded animation sets addressed directly by the one-based clip ID.
///
/// Entries 1..19 are playable; entries 0 and 20 are NULL, not clips. The
/// animation context borrows the pointer table and all tracks until teardown.
#include "assets/enemies/chaser/mad/bank.inc"

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
    { { { TASK_BODY_TMD, ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, _madChaserTask, { .model = &gChaserMadBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, _madChaserHiddenTask, { .model = &gChaserMadBody } },
    { { { TASK_BODY_COORD, ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_TMD, ACTOR_04400_MAD_CHASER_TASK_PRIORITY } }, _madChaserHiddenTask, { .model = &gChaserMadBody } },
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

#include "../../shared/creatures/mad_chaser/mad_chaser_inlines.inc.c"

static void _madChaserRecoilRecover(Task* task);

/// Selects one axis of collision correction, giving grid pushback priority.
///
/// Both arguments and the result are signed whole parent-coordinate units.
/// With no grid correction, use the actor correction. Opposing corrections
/// retain the grid step; aligned corrections use the larger displacement.
static inline s16 _madChaserSelectPushbackStep(s16 gridStep, s16 actorStep)
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

#include "../../shared/creatures/mad_chaser/mad_chaser_limb_shadow.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_spawn_gibs.inc.c"

#include "../../shared/creatures/mad_chaser/mad_chaser_spawn.inc.c"

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
    /// gridStep is a writable s16 containing the correction's integer half;
    /// correctionWord is its s32 Q16 word. Both expressions are side-effect-free,
    /// disjoint lvalues. A zero low half leaves the step unchanged; otherwise
    /// adds +1 for positive words and -1 for negative words, narrowing to s16.
    /// Thus -0.5 becomes -2 rather than -1; this is not rounding towards zero.
    /// correctionWord is read up to twice. Captures the function's fraction-mask
    /// enum, with no caller locals; invoke as a standalone compound statement.
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

#include "../../shared/creatures/mad_chaser/mad_chaser_tick_anim.inc.c"

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

#include "../../shared/creatures/mad_chaser/mad_chaser_lurk_look.inc.c"

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
#define MAD_CHASER_STEP_TABLE gMadChaserWalkSteps
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
/// Selects this carrier's light-recoil continuation for this reaction instance.
///
/// Must name a declared void(Task*) callback for subState 1, borrowing live
/// task-owned MadChaserWork. Continues recovery or changes behavior and resets
/// subState; animation ticking belongs to the combat frame callback. This
/// carrier's light recovery uses its signed-halfword behavior setter.
/// Bind only for this fragment inclusion, then undefine; there is no default.
#define MAD_CHASER_REACTION_CONTINUE_HANDLER _madChaserRecoilRecover
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
