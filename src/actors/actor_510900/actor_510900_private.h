#ifndef SRC_ACTORS_ACTOR_510900_ACTOR_510900_PRIVATE_H
#define SRC_ACTORS_ACTOR_510900_ACTOR_510900_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Length of one side of the square lap the golem walks, corner to corner, in
/// world units. `Actor510900Work::lapDistance` counts along four of them.
#define ACTOR_510900_LAP_SIDE 13200

/// Values of `Actor510900Work::state`: the behaviour the fighting golem runs
/// each frame. Each one steps through its own `subState` values.
enum {
    ACTOR_510900_STATE_OPENING       = 0,  // First moves after the fight starts: a slash pair that ends in the flame lunge's charge
    ACTOR_510900_STATE_PATROL        = 1,  // Walks the lap, firing as it goes, and picks the next attack
    ACTOR_510900_STATE_SLASH         = 2,  // Slash cycles while walking on; also what breaks a helipad light
    ACTOR_510900_STATE_FLAME_LUNGE   = 3,  // Lights the flame, then charges the distance to the player
    ACTOR_510900_STATE_GRENADE       = 4,  // Throws the stun grenade
    ACTOR_510900_STATE_DASH          = 5,  // Runs the lap until the player is in reach, then strikes
    ACTOR_510900_STATE_LETHAL_ATTACK = 6,  // End of the lap: the attack that kills the player when it lands
    ACTOR_510900_STATE_BUILDUP_STUN  = 7,  // Held down until the status build-up has run out
    ACTOR_510900_STATE_FLINCH        = 8,  // Staggers in place
    ACTOR_510900_STATE_RECOIL        = 9,  // Driven back along the lap
    ACTOR_510900_STATE_SPARK_RECOIL  = 10, // Driven back in a shower of sparks by a helipad light's blast
    ACTOR_510900_STATE_SPARK_STUN    = 11, // Stands in a shower of sparks, then recovers
    ACTOR_510900_STATE_DEATH         = 12, // Hit points gone: silences its sounds and stops being present
};

/// Values of `Actor510900Work::flameMode`, which the flame-jet effect reads
/// from its task's first payload word.
enum {
    ACTOR_510900_FLAME_OFF      = 0, // No flame; also the mode before the first ignition
    ACTOR_510900_FLAME_BURNING  = 1, // Lit: the jet grows to full size and keeps throwing flame
    ACTOR_510900_FLAME_BLAST    = 2, // Full-size burst from the first frame; the golem never sets it and only steps it back to burning
    ACTOR_510900_FLAME_DYING    = 3, // Burn time over: the jet thins out
    ACTOR_510900_FLAME_RELEASED = 4, // The golem has let go of the effect, which ends itself once room effects stop running
};

/// Work block of the Akropolis No. 9 golem, kept at `Task::work` of the body
/// model's task.
///
/// The golem fights on a lap: one pass around a square of side
/// `ACTOR_510900_LAP_SIDE` on the
/// helicopter landing pad, measured by `lapDistance` from the first corner.
/// Each frame the state handler picks the animation and the speed along the
/// lap, and the common tail turns the body to the side's heading, places it,
/// plays the step cues and keeps the flame jet and the pad's collision faces
/// up to date.
///
/// The tasks spawned beside the body are its children and reach this block
/// through `Task::parent`: three models that ride body parts (the weapon, a
/// chest piece and a prop only event animations show), the stun grenade, the
/// three helipad lights and a coordinate-only blast source beside light 2.
/// The part-riding models borrow `color` and `light`; the others stop when
/// `present` clears and exchange the latches at the end of the block with the
/// golem.
///
/// The block is cleared at allocation. No access to `pad_5C6` has been
/// observed; whether it is a member or tail padding is unproven.
typedef struct {
    ActorAnimRig19        rig;                // Playback storage of the nineteen-part body model; slots 1..18 are driven
    MATRIX                color;              // Colour matrix lent to the body model and to the three part-riding models
    MATRIX                light;              // Light matrix lent to the same models
    WorldCollisionBody    body;               // Sphere of radius 0x1C2 at the chest on list 2, receiving attacks; pair tests go off when it dies or is put away
    WorldCollisionContact bodyContacts[3];    // Contacts of `body`, also lent to the enemy record; each frame's hits are read from them and cleared
    WorldCollisionBody    weaponAttack;       // Sphere on list 3 offset along the weapon model, carrying the current attack's key; enabled only while an attack or the flame can hit
    WorldCollisionBody    forearmAttack;      // Sphere on list 3 at the weapon arm's forearm; keyed and enabled together with `weaponAttack`
    WorldCollisionContact attackContacts[1];  // Contact shared by both attack spheres; a player contact latches `attackLanded`
    EffectSpawnArg        hitEffectArg;       // Argument of the effect a landed hit spawns at the chest
    MATRIX                propTossMtx;        // World transform of the off hand, taken as the prop leaves it; the prop model flies from it until it is caught
    Task*                 flameJetTask;       // Flame-jet effect on the weapon model, steered through its first payload word; `NULL` if it never spawned or once released
    Task*                 weaponTask;         // Task of the weapon model riding the weapon hand; the flame jet and `weaponAttack` follow its coordinate
    Task*                 chestModelTask;     // Task of the model riding the chest
    SVECTOR               hitTwist;           // Rotation a plain hit knocks the chest by (angle units of 4096 a turn; `vx` and `vy` only), walked back 0x20 a frame towards zero
    s32                   sparkSound;         // Sound looping while a spark reaction plays, kept so its pan can follow the golem and it can be stopped; 0 when silent
    s32                   flameSound;         // Flame sound started at ignition during the fight, stopped when the flame goes out; 0 when silent
    s32                   eventFlameSound;    // Flame sound started by the event animation that lights the flame, stopped the same way; 0 when silent
    s16                   hitTwistActive;     // Nonzero while `hitTwist` still has to be applied
    s16                   animationId;        // Animation the body should be playing; `ACTOR_MESSAGE_PLAY_ANIMATION` stores its id plus 0x1B
    s16                   seededAnimationId;  // Animation the slots were last started on; a difference from `animationId` restarts them
    s16                   animationFrame;     // Frames the current animation has been stepped since it was started
    s16                   hitCooldown;        // Frames left during which attack hits are ignored, set by the hit that landed
    s16                   state;              // `ACTOR_510900_STATE_*`
    s16                   subState;           // Step within `state`, numbered separately by each state
    s16                   present;            // 1 from spawn until death; the answer to `ACTOR_MESSAGE_IS_PRESENT`, and 0 tells the children to stop
    s16                   flameMode;          // `ACTOR_510900_FLAME_*` the flame jet should be in
    s16                   sentFlameMode;      // `flameMode` as last written to the flame jet's task
    s16                   flameFrames;        // Frames the flame still burns; reaching zero puts it out and disables the attack spheres
    u16                   lastCueFlags;       // `ANIMATION_RECORD_CUE_MASK` bits of slot 1's record on the previous frame; a step sound plays when one drops
    s16                   stateCounter;       // Scratch of the current state: frames to wait, lap distance left to cover, slash cycles done or the lunge's speed
    s16                   shotCountdown;      // Frames until the next muzzle flash while the patrol walks firing
    s16                   yaw;                // Heading of the body (4096 a turn), turned 0x1E a frame towards the heading of the lap side while the golem moves
    s16                   lapSpeed;           // Distance added to `lapDistance` this frame; negative when driven back
    s16                   activation;         // Set by message 2007 (0 put away: the frame handler does nothing, 1 shown, 2 fight started)
    u16                   lapDistance;        // Distance covered around the lap, clamped to 0xC8..0xB66C
    s16                   lapSide;            // Side of the lap the golem is on (0..3), `lapDistance / ACTOR_510900_LAP_SIDE`
    s16                   playerSide;         // Side whose strip of the pad last held the player
    s16                   playerDistance;     // Horizontal distance from the body to the player
    s16                   sideRemaining;      // Distance left to the end of the current side
    s16                   sideTravelled;      // Distance covered since the start of the current side
    s16                   attackLanded;       // Set when an attack sphere has touched the player; the state that reads it clears it
    s16                   lethalAttackPhase;  // Progress of the lethal attack (0 not begun, 1 winding up: hits neither flinch nor build up status, 2 struck: hit points stay at 1 or above)
    s16                   deathSoundLoadStep; // Disc load after the lethal attack lands (0 idle, 1 queue the sound file, 2 wait for the drive, then play the death sound)
    s16                   buildupStunned;     // Nonzero during the build-up stun, when most hits cause no flinch or recoil
    s16                   grenadeLive;        // Nonzero from the throw until the grenade's task has finished; no second one is thrown meanwhile
    s16                   playerEscaped;      // Set by message 2014 while the player lives; ends the grenade's stun early, which clears it
    s16                   slashedLight;       // Helipad light the slash is to break (0 none yet, 1 or 2 light 0 or 1, -1 once that light has broken)
    s16                   lightSlashStruck;   // Set on the frame the slash reaches the light; never cleared
    s16                   light2Status;       // Display status helipad light 2 reports each frame (0 intact, 1 sparking, 2 spent, 3 stopped with the golem)
    s16                   light2State;        // State of helipad light 2 (0 intact, 1 sparking, 2 done); read by the blast source beside it
    byte                  pad_5C6[2];
} Actor510900Work;
STATIC_ASSERT_SIZEOF(Actor510900Work, 0x5C8);

/// `TaskDesc` table the state hands `enemySpawnFromTable` (entry 4).
extern TaskDesc D_actor_510900_80167A18[];

/// The enemy parameters the context's `field_50` points at; its `hpMax` seeds
/// the enemy's hit points.
extern EnemyParams D_actor_510900_80167980;

/// The animation data `animationInitContext` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_510900_80167AA4[];

extern TmdSource gActor510900No9GolemAkropolisBody;

extern TmdSource gActor510900Model0FE60;

extern TmdSource gActor510900No9GolemAkropolisProp;

extern TmdSource gActor510900Model10468;

extern TmdSource gActor510900GolemGrenade;

extern TmdSource gActor510900Model10C8C;

extern AnimationSet gActor510900Animation27994;

extern AnimationSet gActor510900Animation27FDC;

extern AnimationSet gActor510900Animation35474;

extern AnimationSet gActor510900Animation35B20;

extern DamageAttack D_actor_510900_80167968;

extern TaskMessageEntry D_actor_510900_80167A6C[7];

/// Initializes the Akropolis No. 9 golem body, children and collision spheres.
///
/// Requires a live descriptor-created nineteen-part body and unlinked enemy.
/// Owns zeroed primary-heap work; failed allocation destroys the enemy. The
/// model borrows work-owned lighting and animation storage until teardown.
/// Starts seven children: prop, weapon, chest model, three lights and a blast
/// source, plus an optional adopted flame jet. Weapon/chest allocation must
/// succeed: their returned enemy/task pointers are used without a NULL check.
/// Incoming body tests start enabled; both attack spheres remain disabled.
/// Restores the live helipad grid and advances to the frame-update state.
void actor510900InitBody(Enemy* enemy, Task* task);

/// Advances No. 9 event playback and its frame-triggered flame presentation.
///
/// Requires live initialized body work/model/enemy and an event-selected rig.
/// Animation 32 at elapsed frame 210 starts 255 flame ticks and two sounds.
/// Advances tracks 1..19, composes the root, refreshes lighting and publishes
/// changed flame modes to the adopted jet without running combat behavior.
void actor510900TickEvent(Enemy* enemy, Task* task);

/// Installs or collapses the landing pad's extra collision-grid face.
///
/// Requires the active landing-pad grid with at least four normals/faces and
/// sixteen vertices. `enabled == 1` copies the fourth face and vertices 12..15;
/// every other value zeroes only their XYZ and the fourth normal's XYZ, leaving
/// the face record and vector fourth halfwords unchanged.
void actor510900SetExtraGridFace(s32 enabled);

/// Restores the three landing-pad wall faces carried with the moving golem.
///
/// Requires the active grid with at least three normals/faces and twelve vertices.
/// Copies complete vectors and face records from the rest-pose arrays. The task
/// argument is unused; this function retains the spawn call's original interface.
void actor510900RestoreGridFaces(Task* unusedTask);

/// Unlinks the body's receiving sphere and both attack spheres, then destroys it.
///
/// Requires the successfully initialized body task and its live enemy/work.
/// Enemy destruction handles the target record, owned work and task tree.
void actor510900ExitBody(Task* task);

/// Samples room lighting at the body's cached translation and applies its colour state.
///
/// `task` must borrow its live enemy through the second spawn argument, and
/// `coord->workm` must already be composed. Its three translation words are
/// passed unchanged as the lighting sample, borrowed only through the update.
void actor510900UpdateLighting(Task* task, const GfxCoord* coord);

#endif // SRC_ACTORS_ACTOR_510900_ACTOR_510900_PRIVATE_H
