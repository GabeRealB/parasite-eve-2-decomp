/* The Skull Stalker (actor_104600's second enemy, actor_207200): an enemy with
 * no attack that lurks out of sight. It spawns hidden and untargetable, fades
 * in after a random wait, stays for a shorter one and fades out again, each
 * fade taking 18 frames. When the player's body enters either of its two
 * sensing bodies (a 0x10000 contact) it raises the scene's enemy alert, shows
 * itself, switches to its alert animation with a repeating cry and starts the
 * battle. Any damaging hit (a 0x20000 contact) or the player's own touch on
 * its body kills it with sparks and a hit sound; a zero-damage hit only
 * applies the id's side effect. A kill takes the model out of the draw at
 * once; the death state still presses the root flat with a decaying Y scale
 * and destroys the enemy 61 frames after its bodies are unlinked. A placement
 * mode picks one of two sound sets and an alternate texture page/CLUT.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_SKULL_STALKER_H
#define SRC_SHARED_SKULL_STALKER_H

#include "types.h"

#include "main/task_types.h"
#include "main/session_types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"

/// Values of `SkullStalkerWork::state`, the behaviour the per-frame dispatch runs.
enum {
    SKULL_STALKER_STATE_IDLE        = 0, // fades in and out until the player is sensed, then cries
    SKULL_STALKER_STATE_INERT       = 2, // runs nothing; nothing selects it
    SKULL_STALKER_STATE_STATUS_HOLD = 3  // kept in sight until the enemy's status buildup runs out, then idle again
};

/// Values of `SkullStalkerWork::deathPhase`.
enum {
    SKULL_STALKER_DEATH_PHASE_COUNTDOWN = 0, // `Task::killCountdown` runs out; the bodies are still linked
    SKULL_STALKER_DEATH_PHASE_LINGER    = 1  // bodies unlinked; waits until `phaseFrames` reaches 61, then destroys the enemy
};

/// Values of `SkullStalkerWork::animId`: indices into the package's table of
/// animation sets, whose entry 0 is empty.
enum {
    SKULL_STALKER_ANIM_IDLE  = 1, // lurking; also left playing through the death
    SKULL_STALKER_ANIM_ALERT = 2  // the player has been sensed
};

/// Value of `SkullStalkerWork::fadeFrames` at which the enemy is out of sight.
enum { SKULL_STALKER_FADE_FRAMES = 18 };

/// Work block of a Skull Stalker task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It holds
/// the animation context and its storage for the model's three parts, the
/// matrices the model is lit through, three collision bodies hung off the
/// root, each followed by its own contact table, and the state machine:
/// `state` picks the behaviour and `deathPhase` the step of the task's death
/// state.
///
/// The enemy is visible only part of the time. `hiding` says which way the
/// fade runs and `fadeFrames` how far it has got; at the hidden end the model
/// leaves the draw pass and the enemy cannot be locked on to.
///
/// Scales are 0x1000 for 1.0.
typedef struct {
    AnimationContext      anim;                                  // animation playback of the model
    AnimationSlot         slots[3];                              // one per model part; 1 and 2 play `animId`, 0 is never started
    u8                    poses[3][ANIMATION_POSE_BUFFER_BYTES]; // blend pose of each slot
    MATRIX                colorMtx;                              // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;                              // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    frontSenseBody;                        // capsule shaped by `frontSenseCapsule`, with no key of its own; a player-body contact alerts the enemy, which then disables it
    WorldCollisionCapsule frontSenseCapsule;                     // shape of `frontSenseBody`: radius 2000 at the root, widening to 4000 at 5000 ahead of it
    WorldCollisionContact frontSenseContacts[1];                 // contact of `frontSenseBody`
    WorldCollisionBody    senseBody;                             // radius-2000 sphere on the root with no key of its own; a player-body contact alerts the enemy, which then disables it
    WorldCollisionContact senseContacts[1];                      // contact of `senseBody`
    WorldCollisionBody    body;                                  // radius-200 sphere 200 above the root, tested against the room grid, the floor and other bodies
    WorldCollisionContact bodyContacts[4];                       // contacts of `body`: wall push-back, the player's touch and hits; also the enemy's hit records
    byte                  field_204[0x50];                       // never accessed
    VECTOR3               prevRootPos;                           // root position restored when the collision step reports a conflict; never written, so it stays the zero the allocation left
    byte                  field_260[4];                          // never accessed
    MATRIX                savedRootMtx;                          // root matrix saved by each death frame; the flatten writes it back scaled along Y
    byte                  field_284[2];                          // never accessed
    s16                   state;                                 // `SKULL_STALKER_STATE_*`
    s16                   deathPhase;                            // `SKULL_STALKER_DEATH_PHASE_*`
    s16                   phaseFrames;                           // frames of the status hold, wrapped every four, or frames of the death linger
    s16                   animId;                                // requested animation, `SKULL_STALKER_ANIM_*`
    s16                   appliedAnim;                           // animation last applied to slots 1 and 2
    s16                   animFrames;                            // frames since `animId` was applied; the idle loop restarts it to time the fades and the cries
    s16                   field_292;                             // set to 0 by the idle animation's frames and the status hold and never read; role unproven
    byte                  field_294[6];                          // never accessed
    s16                   field_29A;                             // set to 1 by the idle animation's frames and never read; role unproven
    byte                  field_29C[4];                          // never accessed
    s16                   flattenScaleY;                         // Y scale of the death flatten; falls 0x50 a frame until it is 0x200 or less. A hit starts it at 0x1000, the player's touch at 0x500
    byte                  field_2A2[2];                          // never accessed
    s16                   fadeFrames;                            // progress of the fade: 0 fully in sight, `SKULL_STALKER_FADE_FRAMES` hidden
    s16                   hiding;                                // 1 fades the enemy out and keeps it hidden, 0 fades it in and keeps it in sight
    s16                   fadeWaitFrames;                        // `animFrames` the idle animation passes before the fade turns round: 100..163 hidden, 18..49 in sight
    s16                   alertRequested;                        // set by a player-body contact on either sensing body; the same frame cries, disables both and starts the battle
    s16                   variant;                               // placement mode: non-zero selects the second of the two sound sets, and 1 also moves the model one texture page and CLUT row on
    byte                  field_2AE[2];                          // never accessed
} SkullStalkerWork;
STATIC_ASSERT_SIZEOF(SkullStalkerWork, 0x2B0);

void skullStalkerSpawnState(Enemy* arg0, Task* arg1);
void skullStalkerIdleTick(Task* arg0);
void skullStalkerHits(Task* arg0);
void skullStalkerDeathState(Enemy* arg0, Task* arg1);
void skullStalkerUpdateState(Enemy* arg0, Task* arg1);
void skullStalkerReactionFlags(Task* arg0);
void skullStalkerReactionDispatch(Task* task);
void skullStalkerLightRamp(Task* task);
void skullStalkerFlatten(Task* arg0);
void skullStalkerExit(Task* task);

void skullStalkerColour(Enemy* arg0, Task* task);

static inline void skullStalkerTickAnim(Task* task);

void skullStalkerTask(Task* arg0);
void skullStalkerAnimate(Task* arg0);

#endif /* SRC_SHARED_SKULL_STALKER_H */
