/* Code of the Mad Chaser shared by actor_04400 (actor_104400/342200),
 * actor_341700 and actor_342400: it twists its spine by a third of the turn per joint, links
 * its three collision spheres, pins a model part to a world position and
 * starts its death squash. The two later packages also share a creep-forward-
 * until-hit state.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The code sees the work block as MadChaserWork. The
 * later two packages name the creep state twice in their state table; the
 * second entry includes the fragment again under its own name.
 */

#ifndef SRC_SHARED_MAD_CHASER_H
#define SRC_SHARED_MAD_CHASER_H

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// Values of `MadChaserWork::animRequest`.
enum {
    MAD_CHASER_ANIM_REQUEST_BLEND   = 1, // blend into `animId` over `animBlendFrames` frames
    MAD_CHASER_ANIM_REQUEST_RESET   = 2, // cut straight to `animId`
    MAD_CHASER_ANIM_REQUEST_PLAYING = 3  // `animId` has been applied and is playing
};

/// Values of `MadChaserWork::hitReaction`, the reaction a hit asks for.
enum {
    MAD_CHASER_HIT_REACTION_NONE      = 0,
    MAD_CHASER_HIT_REACTION_LIGHT     = 1, // light recoil
    MAD_CHASER_HIT_REACTION_HEAVY     = 2, // heavy recoil
    MAD_CHASER_HIT_REACTION_STATUS    = 3, // held until the status buildup runs out
    MAD_CHASER_HIT_REACTION_BLAST     = 4, // heavy recoil; a blast that kills bursts the body
    MAD_CHASER_HIT_REACTION_KNOCKDOWN = 5  // knocked down
};

/// Low nibble of `MadChaserWork::command`: what the room asks of the enemy.
enum {
    MAD_CHASER_COMMAND_KIND_MASK    = 0xF,
    MAD_CHASER_COMMAND_EMERGE       = 1, // appear at a room spot and jump out
    MAD_CHASER_COMMAND_PULL         = 2, // be dragged to the room's pull point
    MAD_CHASER_COMMAND_VANISH       = 3, // disappear without dying
    MAD_CHASER_COMMAND_DROP_DEATH   = 4, // die where it stands
    MAD_CHASER_COMMAND_SHRINK_DEATH = 5  // die and shrink away
};

/// Task dispatch indices; the ordinary form supports only SPAWN through DESPAWN.
enum {
    MAD_CHASER_TASK_SPAWN        = 0,
    MAD_CHASER_TASK_LURK         = 1,
    MAD_CHASER_TASK_DANGLE       = 2,
    MAD_CHASER_TASK_COMBAT       = 3,
    MAD_CHASER_TASK_DEATH        = 4,
    MAD_CHASER_TASK_DESPAWN      = 5,
    MAD_CHASER_TASK_EMERGE       = 6,
    MAD_CHASER_TASK_VANISH       = 7,
    MAD_CHASER_TASK_DROP_DEATH   = 8,
    MAD_CHASER_TASK_SHRINK_DEATH = 9
};

/// Behavior indices within the lurk task's five-entry dispatch table.
enum {
    MAD_CHASER_LURK_STATE_IDLE  = 0,
    MAD_CHASER_LURK_STATE_LOOK  = 1,
    MAD_CHASER_LURK_STATE_RISE  = 2,
    MAD_CHASER_LURK_STATE_ALERT = 3,
    MAD_CHASER_LURK_STATE_SHIFT = 4
};

/// Behavior indices selected by the combat-state setters.
enum {
    MAD_CHASER_COMBAT_STATE_WALK         = 3,
    MAD_CHASER_COMBAT_STATE_LEAP         = 4,
    MAD_CHASER_COMBAT_STATE_ALERT        = 5,
    MAD_CHASER_COMBAT_STATE_RECOIL_LIGHT = 6,
    MAD_CHASER_COMBAT_STATE_RECOIL_HEAVY = 7,
    MAD_CHASER_COMBAT_STATE_STATUS_HOLD  = 8,
    MAD_CHASER_COMBAT_STATE_KNOCKDOWN    = 9
};

/// Entry moves selected within the emerge task's ten-entry behavior table.
enum {
    MAD_CHASER_EMERGE_STATE_BACKFLIP = 1,
    MAD_CHASER_EMERGE_STATE_ARC_BACK = 4,
    MAD_CHASER_EMERGE_STATE_HIGH_ARC = 7
};

/// Parent-coordinate distance thresholds shared by the lurk idle and look holds.
enum {
    MAD_CHASER_LURK_ALERT_DISTANCE = 3500,
    MAD_CHASER_LURK_LOOK_DISTANCE  = 5000
};

/// Stance-table values saved for recoil recovery and pull selection.
enum {
    MAD_CHASER_STANCE_LOW     = 0,
    MAD_CHASER_STANCE_UPRIGHT = 1
};

/// Upright clip requested by light recoil entry and repeated-hit recovery.
enum { MAD_CHASER_LIGHT_RECOIL_UPRIGHT_CLIP = 11 };

/// Capture distance in parent-coordinate units and clips used by both pull steps.
enum {
    MAD_CHASER_PULL_CAPTURE_DISTANCE    = 800,
    MAD_CHASER_PULL_SETTLE_BLEND_FRAMES = 4,
    MAD_CHASER_PULL_LEAP_CLIP           = 8,
    MAD_CHASER_PULL_LOW_SETTLE_CLIP     = 5,
    MAD_CHASER_PULL_UPRIGHT_SETTLE_CLIP = 6
};

/// Walk tuning: distances in the root's parent frame, rates in sixteenths of a
/// frame, and turn steps in 4096ths of a turn.
enum {
    MAD_CHASER_WALK_CLIP                    = 7,
    MAD_CHASER_WALK_BLEND_FRAMES            = 8,
    MAD_CHASER_WALK_DISTANCE_BAND           = 1000,
    MAD_CHASER_WALK_RATE_BAND_STEP          = 4,
    MAD_CHASER_WALK_BASE_TURN_STEP          = 16,
    MAD_CHASER_WALK_TURN_BAND_STEP          = 2,
    MAD_CHASER_WALK_FAR_RATE                = 4 * ANIMATION_RATE_ONE,
    MAD_CHASER_WALK_FAR_TURN_STEP           = 32,
    MAD_CHASER_WALK_DISTANCE_AT_NORMAL_RATE = -16,
    MAD_CHASER_WALK_LEAP_RANGE_BONUS_MASK   = 0x7FF,
    MAD_CHASER_WALK_BASE_LEAP_DISTANCE      = 2000,
    MAD_CHASER_WALK_CLOSE_LEAP_DISTANCE     = 1500,
    MAD_CHASER_WALK_LEAP_HALF_ARC           = ACTOR_TRANSFORM_ANGLE_TURN / 8
};

/// Work block of a Mad Chaser task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It holds
/// the model's matrices, the animation context and its storage, the three
/// collision spheres with their contact records, and the state machine: the
/// task state picks a table of states, `state` an entry of that table, and
/// `subState` a step of that entry's own table. Entering a task state clears
/// both and selecting a state clears `subState`; their ranges are those of the
/// table in use, the largest being eleven states and six steps.
typedef struct {
    MATRIX                savedRootMtx;                          // root matrix at the start of the death shrink; each frame rescales a copy of it
    MATRIX                colorMtx;                              // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;                              // storage for the model's `TmdObject::lightMtx`
    VECTOR                prevRootPos;                           // root position at the start of the frame; restored when the collision step reports a conflict
    SVECTOR               pullPoint;                             // point a pull drags the root to: part 3 of the room's placed actor 0, in the root's parent space
    SVECTOR               rotation;                              // root rotation in 4096ths of a turn: `vx` pitch, `vy` heading, `vz` roll
    SVECTOR               anchorPos;                             // spawn position, moved with every collision step; where part 6 hangs while the enemy dangles
    SVECTOR               toPlayer;                              // offset from the root to the nearer player actor
    SVECTOR               moveStartPos;                          // root position when the leap (`vy` only) or the pull began; the leap lands back on `vy`
    SVECTOR               leapAnchorPos;                         // where part 6 stays during frames 43..46 of the leap, in the root's parent space
    AnimationContext      anim;                                  // animation playback of the model
    AnimationSlot         slots[9];                              // one per model part; 1..8 play `animId`, and slot 1's status tells when it ended
    u8                    poses[9][ANIMATION_POSE_BUFFER_BYTES]; // blend pose of each slot
    WorldCollisionBody    pairBody;                              // sphere other bodies touch; its contacts carry the hits and push-outs
    WorldCollisionBody    gridBody;                              // larger sphere tested against the room grid; shares `contacts`
    WorldCollisionContact contacts[8];                           // contacts of `pairBody` and `gridBody`; also the enemy's hit records
    WorldCollisionBody    attackBody;                            // sphere carrying the enemy's attack key; enabled only while the leap lunges
    WorldCollisionContact attackContacts[2];                     // contacts of `attackBody`
    EffectSpawnArg        effectArg;                             // argument record of the effects its hits and death spawn, hung off part 1
    byte                  field_404[0x8];                        // never accessed
    s16                   leapHeading;                           // heading the leap travels along, locked on frame 45
    s16                   hitCooldown;                           // frames before another hit is taken; set from the hit's id parameter 2
    s16                   leapRangeBonus;                        // 0..0x7FF drawn per walk; the walk ends within 2000 plus this of the player
    u16                   stateFrames;                           // frames spent in the current state or sub-state
    s16                   animRequest;                           // `MAD_CHASER_ANIM_REQUEST_*`
    s16                   appliedAnim;                           // animation last applied to the slots
    s16                   animId;                                // requested animation: index into the animation bank
    u16                   animFrames;                            // frames since `animId` was applied
    s16                   animRate;                              // playback rate of slots 1..8; `ANIMATION_RATE_ONE` is normal speed
    s16                   hitTaken;                              // 1 when a hit or a status tick dealt damage this frame; lets `hitReaction` be consumed
    u16                   state;                                 // index into the state table of the current task state
    u16                   subState;                              // index into the step table of the current state
    s16                   spineYaw;                              // look-around yaw, spread over parts 3..5 a third each
    s16                   animBlendFrames;                       // frames a blend request takes
    s16                   moveAccel;                             // added to `moveSpeed` each frame; itself grows each frame
    s16                   moveSpeed;                             // vertical speed of a jump or fall, or the speed of a pull with six fraction bits
    s16                   lookFrames;                            // frames the look-around has faced a player; 16 raise the alert
    byte                  field_42E[0x2];                        // never accessed
    u16                   shrinkScaleY;                          // Y scale of the death shrink, 0x1000 = 1.0
    s16                   anchored;                              // 1 pins part 6: to `anchorPos` while dangling, to `leapAnchorPos` in combat
    s16                   fallPitch;                             // pitch the dangle sway ended on, eased to zero during the fall
    s16                   turnStep;                              // heading change per frame of the walk
    s16                   busy;                                  // 1 while the current move must finish: death and room commands wait
    s16                   playerDist;                            // horizontal distance to the nearer player actor
    byte                  field_43C[0x2];                        // never accessed
    s16                   field_43E;                             // counted down to zero each frame; nothing sets it, role unproven
    s16                   hasLeaped;                             // set by the first leap, never cleared; picks the settle animation after animation 8 (0: 5, 1: 6)
    s16                   frameCount;                            // frames the enemy has run; phase of the dangle and look-around sways
    u16                   playerBearing;                         // heading to the nearer player actor relative to `rotation.vy`, 0..0xFFF
    s16                   holdFrames;                            // random length of the current lurk hold
    s16                   hitReaction;                           // `MAD_CHASER_HIT_REACTION_*` awaiting the state machine
    s16                   leapCooldown;                          // frames before the walk may leap from beyond 1500 units
    u16                   command;                               // pending room command: kind in bits 0..3, entry move in 4..7, spot in 8..11
    u8                    damageOverTimeSeen;                    // set once the enemy has carried a damage-over-time status; never read
    u8                    stateScratch;                          // recoil, knockdown: stance of the interrupted animation (0 low, 1 upright); pull: ramp of four counts per unit of `animRate`
    byte                  field_450[0x1];                        // never accessed
    u8                    shadowHidden;                          // 1 leaves the limb shadows out
} MadChaserWork;
STATIC_ASSERT_SIZEOF(MadChaserWork, 0x454);

/// Scratch-stack workspace of `_madChaserDrawLimbShadow`, one shadow quad.
///
/// Everything up to the projection is in world space, the frame under the
/// view coordinate. `corners` lie flat at the shadow's height in GPU quad
/// strip order: 0 and 1 either side of the first part, 2 and 3 either side of
/// the second, each pair pushed outwards along the limb by the half span so
/// the quad is twice as long as the limb. `screenCorners`, `depthCue` and
/// `flag` are `RotTransPers4`'s outputs for those four corners.
///
/// Reserve one complete block and release it after drawing; nothing in it
/// outlives the call.
typedef struct {
    MATRIX  firstMatrix;      // first part's transform relative to the view coordinate; only its translation is read
    MATRIX  secondMatrix;     // second part's transform relative to the view coordinate; only its translation is read
    SVECTOR firstPos;         // first part's X and Z, with `vy` the shadow's height
    SVECTOR secondPos;        // second part's X and Z, with `vy` the shadow's height
    SVECTOR corners[4];       // the quad's corners
    long    screenCorners[4]; // projected corners: screen X in bits 0..15, Y in bits 16..31, copied whole into the primitive
    long    depthCue;         // depth-cueing interpolation value of the projection; never read
    long    flag;             // GTE FLAG word of the projection; a set bit 31 drops the quad
    s32     depth;            // last corner's screen Z / 4, which picks the ordering-table entry
    s16     halfSpanX;        // half the X offset from the second part to the first
    s16     halfSpanZ;        // half the Z offset from the second part to the first
} MadChaserLimbShadowScratch;
STATIC_ASSERT_SIZEOF(MadChaserLimbShadowScratch, 0x90);

void        madChaserTwistSpine(Task* arg0);
static void _madChaserLinkBodies(Task* task);
void        madChaserPinPart(Task* arg0, s16 part, SVECTOR3* pos);
static void _madChaserDeathStartShrink(Task* task);
void        madChaserCreepUntilHit(Task* arg0);

static void _madChaserStartLeap(Task* task);
static void _madChaserLurkStartIdleHold(Task* task);
static void _madChaserStartAlert(Task* task);
void        madChaserBurst(Task* arg0);
static void _madChaserDropBodies(Task* task);
static void _madChaserBeginShrink(Task* task);

void madChaserSpawnGibs(Task* arg0);

static void _madChaserDrawLimbShadow(Task* task, s16 firstJoint, s16 secondJoint, s16 halfWidth, s32 worldY, u8 shade);
void        madChaserSpawn(Task* task);
static void _madChaserSpawnHidden(Task* task);
static void _madChaserWalkStart(Task* task);
static void _madChaserWalkApproach(Task* task);
static void _madChaserLeapAttack(Task* task);
static void _madChaserLeapTurnAway(Task* task);
static void _madChaserLeapRebound(Task* task);
void        madChaserDangleFrame(Task* arg0);
static void _madChaserDangleFall(Task* task);
static void _madChaserDangleLand(Task* task);
void        madChaserApplyContacts(Task* arg0, s16 arg1);
static void _madChaserTickAnim(Task* task);
void        madChaserShrinkWithDust(Task* arg0);
void        madChaserTrackPlayer(Task* arg0);
static void _madChaserLurkLookAround(Task* task);
static void _madChaserLurkSidestepToCombat(Task* task);
static void _madChaserLurkSidestepRight(Task* task);
static void _madChaserLurkSidestepLeft(Task* task);
void        madChaserEmergeAtSpot(Task* arg0);
static void _madChaserPulledStruggle(Task* task);
static void _madChaserPulledIn(Task* task);
static void _madChaserPulledLimp(Task* task);
static void _madChaserQueueSoundBank(void);
static void _madChaserSetAlertHold(Task* task, s32 claim);
static s16  _madChaserTakeHitRequest(Task* task);
static void _madChaserQueueCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg);
static void _madChaserTurnToPlayer(Task* task, s32 turnStep);
static void _madChaserStatusHold(Task* task);
static void _madChaserKnockdownStart(Task* task);
static void _madChaserKnockdownRise(Task* task);
static void _madChaserKnockdownEnd(Task* task);
static void _madChaserWalkFinish(Task* task);
static void _madChaserLeapLand(Task* task);
static void _madChaserAlertCry(Task* task);
static void _madChaserAlertRelease(Task* task);
static void _madChaserAlertStartSidestep(Task* task);
static void _madChaserAlertSidestep(Task* task);
static void _madChaserDangleSway(Task* task);
static void _madChaserDeathReleaseTarget(Task* task);
static void _madChaserDeathSettle(Task* task);
static void _madChaserDeathWaitAnim(Task* task);
static void _madChaserDespawn(Task* task);
static void _madChaserRecoilLight(Task* task);
static void _madChaserRecoilHeavy(Task* task);
static void _madChaserRecoilHeavyEnd(Task* task);
static void _madChaserLurkRiseState(Task* task);
static void _madChaserLurkWait(Task* task);
static void _madChaserLurkIdleEnd(Task* task);
static void _madChaserLurkPrepareLook(Task* task);
static void _madChaserLurkStartLookHold(Task* task);
static void _madChaserLurkRiseEnd(Task* task);
static void _madChaserLurkAlertStartSidestep(Task* task);
static void _madChaserLurkShiftStart(Task* task);
static void _madChaserLurkShiftStartSidestep(Task* task);
static void _madChaserPullStart(Task* task);
static void _madChaserPullReact(Task* task);
static void _madChaserVanish(Task* task);
static void _madChaserVanishFree(Task* task);
static void _madChaserDeathRequestSettle(Task* task);
void        madChaserShrink(Task* arg0);
static s32  _madChaserTakeKnockdownRequest(Task* task);

static inline void _madChaserEnterTaskState(Task* task, s32 taskState);
static inline void madChaserUpdateColor(void* enemy, GfxCoord* coord);
static inline void _madChaserCalcContactPushback(const Task* task, const GfxCoord* overlapCoord, const WorldCollisionContact* contact, SVECTOR* pushDelta);
static inline void _madChaserSetBehaviorState(Task* task, s32 behaviorState);
static inline s32  _madChaserTakeHitReaction(Task* task);
static inline s32  _madChaserAnimHasBoundaryStatusInline(Task* task);
static inline void _madChaserSetBehaviorStateS16(Task* task, s16 behaviorState);

void        madChaserLurkTick(Task* arg0);
void        madChaserCombatTick(Task* arg0);
void        madChaserEmergeTick(Task* arg0);
void        madChaserShrinkDeathTick(Task* arg0);
void        madChaserDropDeathTick(Task* arg0);
void        madChaserDeathTick(Task* arg0);
static void _madChaserLurkAlertState(Task* task);
static void _madChaserHiddenTask(Task* task);
void        madChaserTask(Task* arg0);
void        madChaserLeapState(Task* arg0);
void        madChaserDangleState(Task* arg0);
static void _madChaserVanishState(Task* task);
void        madChaserRecoilLightState(Task* arg0);
static s16  _madChaserJoinAlert(Task* task);
static void _madChaserDeathPause(Task* task);
static s16  _madChaserAnimHasBoundaryStatus(Task* task);
static void _madChaserAlertWait(Task* task);
static void _madChaserDangleStart(Task* task);
static void _madChaserPlaceRoot(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg);
static void _madChaserStatusHoldStart(Task* task);
static void _madChaserLurkRiseStart(Task* task);
static s32  _madChaserScaleByAnimRate(Task* task, s16 distanceAtNormalRate);
static void _madChaserAdvanceBehaviorState(Task* task);
void        madChaserStartDespawn(Task* arg0);
void        madChaserToAlertState(Task* arg0);

static __inline__ s16  madChaserTakeHit(Task* arg0);
static __inline__ void madChaserUpdateRotation(Task* arg0);
static __inline__ s16  madChaserTakeHitNibble3(Task* arg0);

#endif /* SRC_SHARED_MAD_CHASER_H */
