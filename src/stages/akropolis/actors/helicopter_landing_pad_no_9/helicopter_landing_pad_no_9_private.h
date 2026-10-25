#ifndef STAGES_AKROPOLIS_ACTORS_HELICOPTER_LANDING_PAD_NO_9_HELICOPTER_LANDING_PAD_NO_9_PRIVATE_H
#define STAGES_AKROPOLIS_ACTORS_HELICOPTER_LANDING_PAD_NO_9_HELICOPTER_LANDING_PAD_NO_9_PRIVATE_H

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

/// Length of one No. 9 patrol-lap side, corner to corner in world X/Z units.
#define HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH 13200

/// Values of `HelicopterLandingPadNo9Work::state`: the behaviour the fighting golem runs each frame.
enum {
    HELICOPTER_LANDING_PAD_NO_9_STATE_OPENING       = 0,  // First moves after the fight starts: a slash pair that ends in the flame lunge's charge
    HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL        = 1,  // Walks the lap, firing as it goes, and picks the next attack
    HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH         = 2,  // Slash cycles while walking on; also what breaks a transformer
    HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE   = 3,  // Lights the flame, then charges the distance to the player
    HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE       = 4,  // Throws the stun grenade
    HELICOPTER_LANDING_PAD_NO_9_STATE_DASH          = 5,  // Runs the lap until the player is in reach, then strikes
    HELICOPTER_LANDING_PAD_NO_9_STATE_LETHAL_ATTACK = 6,  // End of the lap: the attack that kills the player when it lands
    HELICOPTER_LANDING_PAD_NO_9_STATE_BUILDUP_STUN  = 7,  // Held down until the status build-up has run out
    HELICOPTER_LANDING_PAD_NO_9_STATE_FLINCH        = 8,  // Staggers in place
    HELICOPTER_LANDING_PAD_NO_9_STATE_RECOIL        = 9,  // Driven back along the lap
    HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_RECOIL  = 10, // Driven back in a shower of sparks by a transformer's blast
    HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_STUN    = 11, // Stands in a shower of sparks, then recovers
    HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH         = 12, // Hit points gone: silences its sounds and stops being present
};

/// Values of `HelicopterLandingPadNo9Work::flameMode`, which the flame-jet effect reads from its task's first payload
/// word.
enum {
    HELICOPTER_LANDING_PAD_NO_9_FLAME_OFF      = 0, // No flame; also the mode before the first ignition
    HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING  = 1, // Lit: the jet grows to full size and keeps throwing flame
    HELICOPTER_LANDING_PAD_NO_9_FLAME_BLAST    = 2, // Full-size burst from the first frame; the golem never sets it and only steps it back to burning
    HELICOPTER_LANDING_PAD_NO_9_FLAME_DYING    = 3, // Burn time over: the jet thins out
    HELICOPTER_LANDING_PAD_NO_9_FLAME_RELEASED = 4, // The golem has let go of the effect, which ends itself once room effects stop running
};

/// Work block of the Akropolis No. 9 golem, at `Task::work` of its body: its walk round the pad's square lap
/// (`lapDistance` from the first corner, sides `HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH`), and the latches its
/// children - the weapon, chest piece and prop on its parts, the stun grenade, the transformers and the blast
/// source - reach through `Task::parent`. The part-riding models borrow `color` and `light`.
/// Each frame the state handler picks the animation and the speed along the lap, and the common tail turns the body to
/// the side's heading, places it, plays the step cues and keeps the flame jet and the pad's collision faces up to date.
typedef struct {
    ActorAnimRig19        rig;                    // Playback storage of the nineteen-part body model; slots 1..18 are driven
    MATRIX                color;                  // Colour matrix lent to the body model and to the three part-riding models
    MATRIX                light;                  // Light matrix lent to the same models
    WorldCollisionBody    body;                   // Sphere of radius 0x1C2 at the chest on list 2, receiving attacks; pair tests go off when it dies or is put away
    WorldCollisionContact bodyContacts[3];        // Contacts of `body`, also lent to the enemy record; each frame's hits are read from them and cleared
    WorldCollisionBody    weaponAttack;           // Sphere on list 3 offset along the weapon model, carrying the current attack's key; enabled only while an attack or the flame can hit
    WorldCollisionBody    forearmAttack;          // Sphere on list 3 at the weapon arm's forearm; keyed and enabled together with `weaponAttack`
    WorldCollisionContact attackContacts[1];      // Contact shared by both attack spheres; a player contact latches `attackLanded`
    EffectSpawnArg        hitEffectArg;           // Argument of the effect a landed hit spawns at the chest
    MATRIX                propTossMtx;            // World transform of the off hand, taken as the prop leaves it; the prop model flies from it until it is caught
    Task*                 flameJetTask;           // Flame-jet effect on the weapon model, steered through its first payload word; `NULL` if it never spawned or once released
    Task*                 weaponTask;             // Task of the weapon model riding the weapon hand; the flame jet and `weaponAttack` follow its coordinate
    Task*                 chestModelTask;         // Task of the model riding the chest
    SVECTOR               hitTwist;               // Rotation a plain hit knocks the chest by (angle units of 4096 a turn; `vx` and `vy` only), walked back 0x20 a frame towards zero
    s32                   sparkSound;             // Sound looping while a spark reaction plays, kept so its pan can follow the golem and it can be stopped; 0 when silent
    s32                   flameSound;             // Flame sound started at ignition during the fight, stopped when the flame goes out; 0 when silent
    s32                   eventFlameSound;        // Flame sound started by the event animation that lights the flame, stopped the same way; 0 when silent
    s16                   hitTwistActive;         // Nonzero while `hitTwist` still has to be applied
    s16                   animationId;            // Animation the body should be playing; `ACTOR_MESSAGE_PLAY_ANIMATION` stores its id plus 0x1B
    s16                   seededAnimationId;      // Animation the slots were last started on; a difference from `animationId` restarts them
    s16                   animationFrame;         // Frames the current animation has been stepped since it was started
    s16                   hitCooldown;            // Frames left during which attack hits are ignored, set by the hit that landed
    s16                   state;                  // `STATE_*`
    s16                   subState;               // Step within `state`, numbered separately by each state
    s16                   present;                // 1 from spawn until death; the answer to `ACTOR_MESSAGE_IS_PRESENT`, and 0 tells the children to stop
    s16                   flameMode;              // `FLAME_*` the flame jet should be in
    s16                   sentFlameMode;          // `flameMode` as last written to the flame jet's task
    s16                   flameFrames;            // Frames the flame still burns; reaching zero puts it out and disables the attack spheres
    u16                   lastCueFlags;           // `ANIMATION_RECORD_CUE_MASK` bits of slot 1's record on the previous frame; a step sound plays when one drops
    s16                   stateCounter;           // Scratch of the current state: frames to wait, lap distance left to cover, slash cycles done or the lunge's speed
    s16                   shotCountdown;          // Frames until the next muzzle flash while the patrol walks firing
    s16                   yaw;                    // Heading of the body (4096 a turn), turned 0x1E a frame towards the heading of the lap side while the golem moves
    s16                   lapSpeed;               // Distance added to `lapDistance` this frame; negative when driven back
    s16                   activation;             // Set by message 2007 (0 put away: the frame handler does nothing, 1 shown, 2 fight started)
    u16                   lapDistance;            // Distance covered around the lap, clamped to 0xC8..0xB66C
    s16                   lapSide;                // Side of the lap the golem is on (0..3), `lapDistance / HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH`
    s16                   playerSide;             // Side whose strip of the pad last held the player
    s16                   playerDistance;         // Horizontal distance from the body to the player
    s16                   sideRemaining;          // Distance left to the end of the current side
    s16                   sideTravelled;          // Distance covered since the start of the current side
    s16                   attackLanded;           // Set when an attack sphere has touched the player; the state that reads it clears it
    s16                   lethalAttackPhase;      // Progress of the lethal attack (0 not begun, 1 winding up: hits neither flinch nor build up status, 2 struck: hit points stay at 1 or above)
    s16                   deathSoundLoadStep;     // Disc load after the lethal attack lands (0 idle, 1 queue the sound file, 2 wait for the drive, then play the death sound)
    s16                   buildupStunned;         // Nonzero during the build-up stun, when most hits cause no flinch or recoil
    s16                   grenadeLive;            // Nonzero from the throw until the grenade's task has finished; no second one is thrown meanwhile
    s16                   playerEscaped;          // Set by message 2014 while the player lives; ends the grenade's stun early, which clears it
    s16                   slashedTransformer;     // Transformer the slash is to break (0 none yet, 1 or 2 transformer 0 or 1, -1 once it has broken)
    s16                   transformerSlashStruck; // Set on the frame the slash reaches the transformer; never cleared
    s16                   blastTransformerStatus; // Display status the third transformer reports each frame (0 intact, 1 sparking, 2 spent, 3 stopped with the golem)
    s16                   blastTransformerState;  // State of the third transformer (0 intact, 1 sparking, 2 done); read by the blast source beside it
    byte                  pad_5C6[2];
} HelicopterLandingPadNo9Work;
STATIC_ASSERT_SIZEOF(HelicopterLandingPadNo9Work, 0x5C8);

/// Akropolis No. 9 GOLEM combat parameters, borrowed by its body enemy.
extern EnemyParams gHelicopterLandingPadNo9EnemyParams;

/// Animation-set bank borrowed by the nineteen-part No. 9 body rig.
extern AnimationSet* gHelicopterLandingPadNo9BodyAnimationSets[34];

extern TmdSource gGolemNo9BodyAkropolis;

extern TmdSource gGolemNo9AkropolisSword;

extern TmdSource gGolemNo9AkropolisProp;

extern TmdSource gGolemNo9AkropolisChestPiece;

extern TmdSource gGolemGrenade;

extern TmdSource gHelicopterLandingPadNo9TransformerCableModel;

extern AnimationSet gGolemNo9HelipadPatrolIdleClip;
extern AnimationSet gGolemNo9HelipadWalkStartClip;
extern AnimationSet gGolemNo9HelipadWalkClip;
extern AnimationSet gGolemNo9HelipadWalkFireClip;
extern AnimationSet gGolemNo9HelipadBackstepClip;
extern AnimationSet gGolemAdvanceClip;
extern AnimationSet gGolemNo9HelipadLethalStrikeClip;
extern AnimationSet gGolemNo9HelipadSlashFirstClip;
extern AnimationSet gGolemNo9HelipadSlashSecondClip;
extern AnimationSet gGolemNo9HelipadSlashWindupClip;
extern AnimationSet gGolemPawnBeamSwordChargeWindupClip;
extern AnimationSet gGolemPawnBeamSwordChargeRunClip;
extern AnimationSet gGolemPawnBeamSwordChargeSlashClip;
extern AnimationSet gGolemNo9HelipadFlameWindupClip;
extern AnimationSet gGolemNo9HelipadFlameLungeClip;
extern AnimationSet gGolemNo9HelipadRecoilClip;
extern AnimationSet gGolemRecoilFrontClip;
extern AnimationSet gGolemScreamRecoverClip;
extern AnimationSet gGolemBuildupClip;
extern AnimationSet gGolemRecoilBehindClip;
extern AnimationSet gGolemNo9HelipadShotgunResponseStartClip;
extern AnimationSet gGolemNo9HelipadShotgunResponseWalkClip;
extern AnimationSet gGolemNo9HelipadShotgunResponseEndClip;
extern AnimationSet gGolemNo9HelipadDeathClip;
extern AnimationSet gGolemNo9HelipadSlashRecoverClip;
extern AnimationSet gGolemNo9HelipadFightStartClip;
extern AnimationSet gGolemNo9HelipadGrenadeThrowClip;
extern AnimationSet gGolemNo9HelipadEvent1Clip;
extern AnimationSet gGolemNo9HelipadEvent2Clip;
extern AnimationSet gGolemNo9HelipadPropRevealClip;
extern AnimationSet gGolemNo9HelipadPropTossClip;
extern AnimationSet gGolemNo9HelipadEvent5Clip;
extern AnimationSet gGolemNo9HelipadEvent6Clip;

extern AnimationSet gGolemNo9HelipadPlayerGrenadeStunClip;

extern AnimationSet gGolemNo9HelipadPlayerGrenadeRecoverClip;

extern AnimationSet gGolemNo9HelipadCableIntactClip;

extern AnimationSet gGolemNo9HelipadCableBreakClip;

/// No. 9's six collision attacks, borrowed by its enemy parameters and attack bodies.
extern DamageAttack gHelicopterLandingPadNo9Attacks[6];

/// No. 9 body messages for grenade release, animation, placement, draw, activation and presence.
extern TaskMessageEntry gHelicopterLandingPadNo9BodyMessages[7];

/// Initializes the Akropolis No. 9 golem body, children and collision spheres.
void helicopterLandingPadNo9InitBody(Enemy* enemy, Task* task);

/// Advances No. 9 event playback and its frame-triggered flame presentation.
void helicopterLandingPadNo9TickEvent(Enemy* enemy, Task* task);

/// Installs or collapses the landing pad's extra collision-grid face.
void helicopterLandingPadNo9SetExtraGridFace(s32 enabled);

/// Restores the three landing-pad wall faces carried with the moving golem.
void helicopterLandingPadNo9RestoreGridFaces(Task* unusedTask);

/// Unlinks the body's receiving sphere and both attack spheres, then destroys it.
void helicopterLandingPadNo9ExitBody(Task* task);

/// Samples room lighting at the body's cached translation and applies its colour state.
void helicopterLandingPadNo9UpdateLighting(Task* task, const GfxCoord* coord);

/// Parts of No. 9's rig (`enemies/golem/no_9/rig`) the landing pad's code reaches: the chest carries his aim point,
/// hit effects, body sphere and chest piece, the right forearm his forearm strike, the hands his sword and his prop.
enum {
    PELVIS_PART        = 1,
    CHEST_PART         = 3,
    RIGHT_FOREARM_PART = 7,
    RIGHT_HAND_PART    = 8,
    LEFT_HAND_PART     = 12,
};

#endif // STAGES_AKROPOLIS_ACTORS_HELICOPTER_LANDING_PAD_NO_9_HELICOPTER_LANDING_PAD_NO_9_PRIVATE_H
