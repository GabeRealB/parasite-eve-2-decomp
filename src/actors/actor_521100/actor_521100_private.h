#ifndef SRC_ACTORS_ACTOR_521100_ACTOR_521100_PRIVATE_H
#define SRC_ACTORS_ACTOR_521100_ACTOR_521100_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/message.h"

#include "main/session_types.h"
#include "main/task_types.h"

/// Values of `Actor521100Work::state`: the behaviour the fighting golem runs
/// each frame. Each one steps through its own `subState` values.
enum {
    ACTOR_521100_STATE_APPROACH   = 0, // Stands, then walks at the player; tries the grab on the way and attacks once the player is in reach
    ACTOR_521100_STATE_ATTACK     = 1, // Picks one of the three gunblade attacks and plays it
    ACTOR_521100_STATE_GRAB       = 2, // Seizes and holds the player until they struggle free, are killed, or kill the golem
    ACTOR_521100_STATE_STAGGER    = 3, // Reaction to a guardable hit that comes from more than 0x300 off its heading or lands during a live attack
    ACTOR_521100_STATE_GUARD      = 4, // Reaction to a guardable hit from within 0x300 of its heading, which does a quarter or an eighth of its damage
    ACTOR_521100_STATE_FLINCH     = 5, // Reaction to a plain hit while approaching, limited by `flinchCooldown`
    ACTOR_521100_STATE_WALK_ROUTE = 6, // Walks the room's fixed waypoints, one leg per `subState`, turning to attack when the player comes near
};

/// Values of `Actor521100Work::attackChoice`: the attack the golem plays, which
/// is also the index of its entry in the package's damage table.
enum {
    ACTOR_521100_ATTACK_NONE       = -1, // Nothing chosen yet
    ACTOR_521100_ATTACK_SLASH      = 0,  // Swing with a short step forward
    ACTOR_521100_ATTACK_LONG_SLASH = 1,  // Swing that turns with the player and steps further; never chosen at close range
    ACTOR_521100_ATTACK_STANCE     = 2,  // Takes a stance, holds it, then swings with the weapon sphere alone; always chosen while the player's attachment is armed or casting
};

/// Work block of the Dryfield No. 9 golem, kept at `Task::work` of the body
/// model's task.
///
/// Each frame of the fight the hit handler reads the frame's contacts, the
/// state handler picks the animation, the speed and the heading to turn to,
/// and the common tail turns the body by `turnSpeed`, moves it along its
/// facing by `forwardSpeed`, plays the step cues and steps the animation.
/// While an event runs the block only animates, and burns when the event asks
/// for it.
///
/// The gunblade is a second model whose task is a child of the body's. It
/// reaches this block through `Task::parent`, borrows `color` and `light`,
/// and during an event copies the body's draw mode.
///
/// The block is cleared at allocation. No access to `pad_658` has been
/// observed; what those bytes hold is unproven. The fourth halfword of
/// `prevRootPos` is never accessed either.
typedef struct {
    ActorAnimRig19        rig;                  // Playback storage of the nineteen-part body model; slots 1..18 are driven
    MATRIX                color;                // Colour matrix lent to the body model and to the gunblade model
    MATRIX                light;                // Light matrix lent to the same models
    WorldCollisionBody    groundBody;           // Sphere of radius 0x190 resting on the root on list 2, grid-tested with a floor query to keep the golem on the floor and out of walls
    WorldCollisionContact groundContacts[5];    // Contacts of `groundBody`; each frame's push-out is read from them and they are cleared
    WorldCollisionBody    body;                 // Sphere of radius 0x190 at the chest on list 2, receiving attacks
    WorldCollisionContact bodyContacts[3];      // Contacts of `body`, also lent to the enemy record; each frame's hits are read from them and cleared
    WorldCollisionBody    weaponAttack;         // Sphere on list 3 offset along the gunblade model, carrying the current attack's key; enabled only while an attack can hit
    WorldCollisionBody    forearmAttack;        // Sphere on list 3 at the weapon arm's forearm; keyed with `weaponAttack` and enabled with it for the two slashes
    WorldCollisionContact attackContacts[1];    // Contact shared by both attack spheres; a contact disables them and latches `attackLanded`
    WorldCollisionBody    grabPathProbe;        // Capsule on list 3 riding the root, grid-tested only: the line from the golem to the spot it holds a grabbed player at
    WorldCollisionBody    grabSpotProbe;        // Sphere of radius 0x1C2 on list 3, grid-tested only, 0x4E2 ahead of the root where a grabbed player is held
    WorldCollisionCapsule grabPathCapsule;      // Shape of `grabPathProbe` in the root's frame: `ends[1]` on the body, `ends[0]` 0x5DC ahead, radius 1
    WorldCollisionContact grabProbeContacts[1]; // Contact shared by both probes; read into `grabBlocked` and cleared every frame
    EffectSpawnArg        hitEffectArg;         // Argument of the effects spawned on the body: the one a landed hit spawns at the chest and the event's fire
    SVECTOR               prevRootPos;          // Root translation before this frame's move, put back when the ground contacts push in opposing directions
    Task*                 weaponTask;           // Task of the gunblade model riding the weapon hand; `weaponAttack` follows its coordinate
    byte                  pad_658[0x20];
    SVECTOR               hitTwist;             // Rotation a plain hit knocks the chest by (angle units of 4096 a turn; `vx` and `vy` only), walked back 0x20 a frame towards zero
    s16                   hitTwistActive;       // Nonzero while `hitTwist` still has to be applied
    s16                   inEvent;              // Refreshed every frame: 1 while an event runs, when the gunblade model copies the body's draw mode
    s16                   hitCooldown;          // Frames left during which attack hits are ignored, set by the hit that landed
    s16                   animationId;          // Animation the body should be playing; `ACTOR_MESSAGE_PLAY_ANIMATION` stores its id plus 0x14 or 0x1D, by the request's source
    s16                   seededAnimationId;    // Animation the slots were last started on; a difference from `animationId` restarts them with that animation's blend
    s16                   animationFrame;       // Frames the current animation has been stepped since it was started
    s16                   eventBurnStage;       // Fire the body sheds during an event (0 none, 1 alight: a burst every 7 frames on the chest and on a random part, 2 and 3 dying down: every 14, then 28)
    s16                   stateCounter;         // Scratch of the current state: frames to stand, walk or hold the stance, frames to the hold's next damage, the death sound's load step, or frames since the last fire burst
    s16                   stateElapsed;         // Second counter: frames the hold has lasted without a struggle, or frames the event's fire has burnt
    s16                   modelDrawFlags;       // First argument of the last `ACTOR_MESSAGE_SET_MODEL_DRAW` (bit 0 draw the model, bit 1 skip its automatic buffer); the gunblade model follows it during an event
    s16                   weaponHidden;         // Set by actor command 1 and never cleared; the gunblade model is not drawn during an event
    s16                   yaw;                  // Heading of the body (4096 a turn), turned up to `turnSpeed` a frame towards `targetYaw`
    s16                   targetYaw;            // Heading to turn to (0..0xFFF): the bearing of the player, or of a waypoint on the route
    s16                   forwardSpeed;         // Distance the root moves along its facing this frame; negative backs away
    s16                   turnSpeed;            // Most `yaw` may change this frame; 0 holds the heading
    s16                   state;                // `ACTOR_521100_STATE_*`
    s16                   subState;             // Step within `state`, numbered separately by each state
    s16                   attackStep;           // Step of the stance attack (0 taking the stance, 1 holding it, 2 swinging, 3 recovering)
    s16                   grabFromFront;        // Taken as the grab begins: 1 when the player faces the golem, 0 when seized from behind; picks the player's animations and the throw's path
    s16                   attackLanded;         // Set when an attack sphere has touched its target, cleared when the attack's spheres go off; not read
    s16                   playerEscaped;        // Set by message 2014 while the player lives; ends the hold, which clears it
    s16                   playerDistance;       // Horizontal distance from the body to the player, measured by the approach and the attack
    s16                   flinchCooldown;       // Frames before a plain hit may start the flinch again; rolled as 0x96..0x195 by each flinch
    s16                   attackLive;           // 1 from the frame an attack's spheres are enabled until the attack ends; a guardable hit taken meanwhile staggers
    s16                   activated;            // Set by message 2007; until then the frame handler takes no hits and runs no state
    s16                   present;              // 1 from spawn until its hit points run out while the player lives; the answer to `ACTOR_MESSAGE_IS_PRESENT`
    u16                   lastCueFlags;         // `ANIMATION_RECORD_CUE_MASK` bits of slot 1's record on the previous frame; a step sound plays when one drops
    s16                   attackChoice;         // `ACTOR_521100_ATTACK_*` chosen last
    s16                   prevAttackChoice;     // The choice before it; when both agree the next one is drawn from a table that excludes a third repeat
    s16                   resumeRoute;          // Nonzero sends a finished hit reaction back to the route, at `resumeRouteLeg`, instead of to the approach
    s16                   resumeRouteLeg;       // `subState` the route is resumed at
    s16                   grabBlocked;          // Refreshed every frame: 1 when the room's collision grid touches a grab probe, which rules the grab out
} Actor521100Work;
STATIC_ASSERT_SIZEOF(Actor521100Work, 0x6C0);

/// One stretch of the grab's throw: how far the held player is carried on each of its frames.
///
/// The throw's travel is a list of these in ascending `endFrame`, one list for
/// each side the player was seized from. A frame of the throw takes the first
/// span it falls short of the end of, so the last span of a list ends on the
/// frame the carrying stops.
typedef struct {
    s16 endFrame;     // Frame of the golem's throw animation this span ends before
    s16 sidewaysStep; // Distance the player is moved on each frame of the span, along the X axis of the golem's root; added to where the player then stands
} Actor521100ThrowSpan;
STATIC_ASSERT_SIZEOF(Actor521100ThrowSpan, 4);

extern AnimationSet gActor521100Animation10EAC;

extern AnimationSet gActor521100Animation11614;

extern AnimationSet gActor521100Animation12204;

extern AnimationSet gActor521100Animation1271C;

extern AnimationSet gActor521100Animation13454;

extern AnimationSet gActor521100Animation13B8C;

extern AnimationSet gActor521100Animation143F0;

extern AnimationSet gActor521100Animation14918;

extern AnimationSet gActor521100Animation14C38;

extern AnimationSet gActor521100Animation15168;

extern AnimationSet gActor521100Animation152F8;

extern AnimationSet gActor521100Animation15904;

extern AnimationSet gActor521100Animation15E28;

extern AnimationSet gActor521100Animation16628;

extern AnimationSet gActor521100Animation16F90;

extern AnimationSet gActor521100Animation18B20;

extern AnimationSet gActor521100Animation1A8E4;

extern AnimationSet gActor521100Animation1B8F8;

extern AnimationSet gActor521100Animation1C104;

extern AnimationSet gActor521100Animation1C58C;

extern AnimationSet gActor521100Animation20880;

extern AnimationSet gActor521100Animation20F94;

extern AnimationSet gActor521100Animation2177C;

extern AnimationSet gActor521100Animation220B8;

extern AnimationSet gActor521100Animation229A0;

extern AnimationSet gActor521100Animation2318C;

extern AnimationSet gActor521100Animation239C0;

extern AnimationSet gActor521100Animation24F5C;

extern AnimationSet gActor521100Animation25ACC;

extern AnimationSet gActor521100Animation26024;

extern AnimationSet gActor521100Animation26E3C;

extern AnimationSet gActor521100Animation26FCC;

extern AnimationSet gActor521100Animation271A8;

extern AnimationSet gActor521100Animation27A78;

extern AnimationSet gActor521100Animation27DB0;

extern AnimationSet gActor521100Animation28380;

extern AnimationSet gActor521100Animation28DD8;

extern AnimationSet gActor521100Animation291F8;

extern AnimationSet gActor521100Animation29534;

extern AnimationSet gActor521100Animation29A88;

extern AnimationSet gActor521100Animation2A998;

extern AnimationSet gActor521100Animation2B89C;

extern AnimationSet gActor521100Animation2C378;

extern AnimationSet gActor521100Animation2CCA8;

extern AnimationSet gActor521100Animation2D708;

extern AnimationSet* D_actor_521100_8015F73C[36];

extern AnimationSet* D_actor_521100_8015F7CC[14];

extern EffectSpawnArg D_actor_521100_8015F804;

extern s16 D_actor_521100_8015F894[20];

extern s16 D_actor_521100_8015F8BC[8];

extern s16 D_actor_521100_8015F8CC[4];

extern Actor521100ThrowSpan D_actor_521100_8015F80C[2][17];

s32 func_actor_521100_80135D10(Task*, s32, s32, s32);

s32 func_actor_521100_80135D58(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

s32 func_actor_521100_80135D9C(Task*, s32, s32, s32);

s32 func_actor_521100_80135DC8(Task*, s32, s32, s32);

#endif // SRC_ACTORS_ACTOR_521100_ACTOR_521100_PRIVATE_H
