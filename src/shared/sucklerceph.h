/* The Sucklerceph, the first enemy of actor_104600. It spawns
 * either standing (dormant) or hidden until a message drops it into place from
 * a spawn point (maps 0x27/0x28). While dormant it rocks back and forth. When
 * the player enters its wide sensing sphere (a 0x10000 contact) it wakes,
 * turns toward the player by up to 0x20 a frame and crawls at 0x14 a step,
 * with a randomly timed idle sound. Within 0x320 of the player it swells, its
 * body-part scale growing 0xC8 a frame, and on the fifth frame it dies. The
 * death is picked at random (or forced): either it bursts - hit spheres armed,
 * burst effects, a death script, model hidden - or it slumps and its body is
 * flattened into the floor by a decaying Y scale before the enemy is
 * destroyed. It takes damage and critical kills from player hits (0x20000
 * contacts), is pushed back by the room's walls and clear of other enemies'
 * bodies (0x30000 contacts), and can be told by message to stand still giving
 * off puffs (commands 4/5). A spawn-arg mode picks one of two sound banks and
 * an alternate texture page/CLUT.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_SUCKLERCEPH_H
#define SRC_SHARED_SUCKLERCEPH_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"

/// Values of `SucklercephWork::state`, the behaviour the per-frame dispatch runs.
enum {
    SUCKLERCEPH_STATE_DORMANT       = 0, // rocks in place until the player comes near or a hit lands
    SUCKLERCEPH_STATE_AWAKE         = 1, // runs `SucklercephWork::awakeStage`
    SUCKLERCEPH_STATE_STATUS_HOLD   = 3, // held still until the enemy's status buildup runs out, then awake again
    SUCKLERCEPH_STATE_PUFFING       = 4, // room command: stands frozen, giving off a puff every 16 frames
    SUCKLERCEPH_STATE_PUFFING_DEATH = 5, // the same, dying on the third puff; nothing selects it
    SUCKLERCEPH_STATE_SLUMP_DEATH   = 6  // killed without bursting: the body stays visible and is flattened
};

/// Values of `SucklercephWork::awakeStage`.
enum {
    SUCKLERCEPH_AWAKE_STAGE_NONE  = 0, // dormant, hidden or waiting to drop
    SUCKLERCEPH_AWAKE_STAGE_CRAWL = 1, // turns toward the player and crawls
    SUCKLERCEPH_AWAKE_STAGE_SWELL = 2  // swells for five frames, then dies
};

/// Values of `SucklercephWork::deathPhase`.
enum {
    SUCKLERCEPH_DEATH_PHASE_COUNTDOWN = 0, // `Task::killCountdown` runs out while the swelling goes down
    SUCKLERCEPH_DEATH_PHASE_FLATTEN   = 1, // the saved root transform is squashed along Y
    SUCKLERCEPH_DEATH_PHASE_LINGER    = 2  // waits until `deathFrames` reaches 61, then destroys the enemy
};

/// Values of `SucklercephWork::animId`: indices into the package's table of
/// animation sets, whose entry 0 is empty. The table's third set is never
/// requested.
enum {
    SUCKLERCEPH_ANIM_IDLE  = 1, // dormant, puffing and slumping
    SUCKLERCEPH_ANIM_CRAWL = 2  // awake, and after a drop has landed
};

/// Work block of a Sucklerceph task.
///
/// Both spawn handlers allocate it zeroed and keep it at `Task::work`. It holds
/// the animation context and its storage for the model's three parts, the
/// matrices the model is lit through, four collision spheres hung off the
/// root, each followed by its own contact table, and the state machine: `state`
/// picks the behaviour, `awakeStage` the step of the awake one, and
/// `deathPhase` the step of the task's death state.
///
/// Speeds are game-coordinate units a frame, headings 4096ths of a turn and
/// scales 0x1000 for 1.0.
typedef struct {
    AnimationContext      anim;                                  // animation playback of the model
    AnimationSlot         slots[3];                              // one per model part; 1 and 2 play `animId`, 0 is never started
    u8                    poses[3][ANIMATION_POSE_BUFFER_BYTES]; // blend pose of each slot
    MATRIX                colorMtx;                              // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;                              // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    senseBody;                             // radius-3000 sphere with no key of its own; a player-body contact wakes the dormant enemy, which then disables it
    WorldCollisionContact senseContact;                          // contact of `senseBody`
    WorldCollisionBody    body;                                  // radius-200 sphere 200 above the root, tested against the room grid, the floor and other bodies
    WorldCollisionContact contacts[4];                           // contacts of `body`: wall push-back, the player's touch, hits and other enemies; also the enemy's hit records
    WorldCollisionBody    attackBody;                            // radius-1000 sphere carrying the enemy's attack key; enabled by the burst until it touches something
    WorldCollisionContact attackContact;                         // contact of `attackBody`
    WorldCollisionBody    blastBody;                             // radius-1000 sphere carrying the hit key 0x22323, which enemy bodies take as damage; enabled by the burst, disabled as the death countdown starts
    WorldCollisionContact blastContact;                          // contact of `blastBody`; never read
    byte                  field_224[0x50];                       // never accessed
    VECTOR3               prevRootPos;                           // root position before the last step; restored when the collision step reports a conflict
    byte                  field_280[4];                          // never accessed
    EffectSpawnArg        hitEffectArg;                          // argument record of the effect a survived hit spawns, hung off model part 1
    MATRIX                savedRootMtx;                          // root matrix when the death countdown ended; each flatten frame rescales a copy of it
    u32                   swellScale;                            // scale of model part 1; grows 200 a frame while swelling, falls 300 a frame in the death countdown, clamped to 0x1000..0x13E8 when applied
    s16                   heading;                               // root heading, turned toward the player by at most 0x20 a frame
    s16                   state;                                 // `SUCKLERCEPH_STATE_*`
    s16                   deathPhase;                            // `SUCKLERCEPH_DEATH_PHASE_*`
    s16                   deathFrames;                           // frames since the death countdown ended; a status hold also clears it and never reads it
    s16                   animId;                                // requested animation, `SUCKLERCEPH_ANIM_*`
    s16                   appliedAnim;                           // animation last applied to slots 1 and 2; 0 forces `animId` to be applied again
    u16                   animFrames;                            // frames since `animId` was applied, wrapped by the dormant and crawl loops; the puffing states count the frames between puffs in it
    s16                   forwardSpeed;                          // distance the root moves along its facing each step; negative backs away
    byte                  field_2C0[6];                          // never accessed
    s16                   field_2C6;                             // set to 1 by every dormant frame and never read; role unproven
    s16                   awakeStage;                            // `SUCKLERCEPH_AWAKE_STAGE_*`
    s16                   flattenScaleY;                         // Y scale of the death flatten; falls 0x50 a frame from 0x1000 until it is 0x200 or less
    s16                   field_2CC;                             // set to 15 by a survived hit and never read; role unproven
    s16                   hitCooldown;                           // frames before another hit is taken; set from the hit's id parameter 2
    s16                   idleSoundFrames;                       // frames until the next idle sound; redrawn from 80..179 each time
    s16                   animFrozen;                            // 1 holds the animation: `animId` is neither applied nor advanced
    s16                   swellFrames;                           // frames spent swelling; the fifth kills the enemy. The unreached puffing death counts its puffs here
    s16                   variant;                               // low half of the spawn argument: non-zero takes the sounds from character bank 0x46 instead of 0x2E, and 1 at a standing spawn also moves the model one texture page and CLUT row on
    s16                   wakeRequested;                         // set by a player-body contact on `senseBody` or a survived hit under the idle animation; wakes the enemy on its next dormant frame
    s16                   hasBurst;                              // 1 once the enemy has burst; the end of the death countdown then spawns the ground glow
    s16                   field_2DC;                             // high half of the spawn argument, never 1 (that value cancels the spawn) and never read; role unproven
    s16                   fallSpeed;                             // downward speed of the drop into place
    s16                   dropCollided;                          // 1 once the drop has been pushed back by the room; the fall then accelerates twice as fast
    s16                   dropArmed;                             // 1 once a room command has started the drop; cleared when the enemy is hidden again
} SucklercephWork;
STATIC_ASSERT_SIZEOF(SucklercephWork, 0x2E4);

/// Scratch-stack block of the Sucklerceph's contact pass.
///
/// The pass reserves one block a frame. It has the push-back of the room's
/// collision grid resolved from the contact records into `delta` and adds the
/// whole units of that correction to the root. `delta` then takes the offset
/// to the player, whose X and Z length is both the range that sets off the
/// swelling and the range a hit's damage is worked out for. For each contact
/// with another enemy's body, kind 0x30000, it takes the offset from that
/// body's centre, which is normalised into `normal` and turned back into
/// `delta` in the frame of the collision grid's coordinate; a crawling
/// Sucklerceph is pushed along it by the depth of the overlap. The block is
/// released before the pass returns.
///
/// The block opens as `ActorContactDeltaScratch` does. No pass touches the
/// bytes either side of `delta`, so what they were laid out to hold is
/// unproven.
typedef struct {
    byte                unknown_0[0x20]; // Reserved with the block and never accessed; role unproven
    WorldCollisionDelta delta;           // Correction resolved from the contact records, in signed 16.16 units; then, in whole world units, the offset to the player or from the centre of the body being tested; then `normal` in the grid coordinate's frame, 4096 = 1.0
    byte                unknown_30[0x8]; // Reserved with the block and never accessed; role unproven
    VECTOR              normal;          // Offset from the body being tested, normalised: away from that body, 4096 = 1.0
    s32                 gridKeyMask;     // One bit for each grid contact the correction was resolved from, at the position the low five bits of its key select; stored, never read
} SucklercephContactsScratch;
STATIC_ASSERT_SIZEOF(SucklercephContactsScratch, 0x4C);

static void _sucklercephSpawnState(Enemy* enemy, Task* task);
void        sucklercephReactionDispatch(Task* arg0);
static void _sucklercephDormantTick(Task* task);
void        sucklercephAwakeTick(Task* arg0);
void        sucklercephContacts(Task* arg0);
void        sucklercephTakeDamage(Task* arg0, s32 arg1);
static void _sucklercephTurnToPlayer(Task* task);
void        sucklercephDeathState(Enemy* enemy, Task* task);
static void _sucklercephKill(Task* task, u8 forceBurst);
void        sucklercephDropSpawnState(Enemy* arg0, Task* arg1);
static void _sucklercephDropCollide(Task* task);
s32         sucklercephMessage(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3);
void        sucklercephUpdateState(Enemy* arg0, Task* arg1);
void        sucklercephReactionFlags(Task* arg0);
static void _sucklercephStep(Task* task);
static void _sucklercephScalePart(Task* task, GfxCoord* coord);
static void _sucklercephFlatten(Task* task);
static void _sucklercephExit(Task* task);
static void _sucklercephFallStep(Task* task);

static __inline__ void _sucklercephTickAnim(Task* task);

static void _sucklercephTask(Task* task);
static void _sucklercephAnimate(Task* task);
static void _sucklercephColour(Enemy* enemy, Task* task);
static void _sucklercephDrawShadow(Task* task);
void        sucklercephDropTask(Task* arg0);

#endif /* SRC_SHARED_SUCKLERCEPH_H */
