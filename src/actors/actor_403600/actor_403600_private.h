#ifndef SRC_ACTORS_ACTOR_403600_ACTOR_403600_PRIVATE_H
#define SRC_ACTORS_ACTOR_403600_ACTOR_403600_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/enemy_params.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

struct Enemy;

/// Top-level state of an `Actor403600Work`, kept in `mode`.
///
/// The values from `ACTOR_403600_MODE_FIGHT` up to, not including,
/// `ACTOR_403600_MODE_SCENE_POSE` are the fight: only in them does the boss
/// move, take hits and react. The `SCENE` values are set by actor commands
/// sent to the task.
enum {
    ACTOR_403600_MODE_PARKED         = 0,  // Boss held at a fixed point; also the mode a double or a scene figure is created in
    ACTOR_403600_MODE_FIGHT          = 1,  // Runs the action in `action`
    ACTOR_403600_MODE_STAGGER        = 2,  // Thrown back by a staggering hit, then back to the fight
    ACTOR_403600_MODE_STUN           = 3,  // Held for `stunFrames` by a status build-up
    ACTOR_403600_MODE_FREEZE         = 4,  // Animation stopped for `stunFrames` by a hit that caught a wind-up
    ACTOR_403600_MODE_WEAKEN         = 5,  // Sheds body parts and enters the weakened phase
    ACTOR_403600_MODE_RECOVER        = 6,  // Leaves the weakened phase under a white flash
    ACTOR_403600_MODE_SCENE_POSE     = 10, // Placed and posed by a scene command while the flash fades
    ACTOR_403600_MODE_SCENE_BRIGHTEN = 11, // `ambientBoost` rises every frame
    ACTOR_403600_MODE_SCENE_ASCEND   = 12, // Rises to its fight height and hovers there
    ACTOR_403600_MODE_SCENE_FADE     = 13, // Scene figure only: fades out
    ACTOR_403600_MODE_DYING          = 20, // Whites the screen out, loads a file and starts the task it carries
};

/// What the boss does while in `ACTOR_403600_MODE_FIGHT`, kept in `action`.
enum {
    ACTOR_403600_ACTION_CHOOSE       = 0,  // Waits `actionDelay` frames, then swipes at a close player or picks an attack
    ACTOR_403600_ACTION_RECHARGE     = 1,  // Flashes over the body after a dive attack
    ACTOR_403600_ACTION_RECHARGE_END = 2,  // Second half of that, back to `ACTOR_403600_ACTION_CHOOSE`
    ACTOR_403600_ACTION_SLAM         = 10, // Climbs high above the room and dives onto a fixed point; the shock hurts by distance
    ACTOR_403600_ACTION_VOLLEY       = 20, // Flies to a firing point and launches three projectiles
    ACTOR_403600_ACTION_SWOOP        = 30, // Climbs to one corner and dives across the room to another
    ACTOR_403600_ACTION_RUSH         = 40, // Spinning passes from changing sides, ended by a dive from above
    ACTOR_403600_ACTION_SUMMON       = 50, // Charges up and spawns the double
    ACTOR_403600_ACTION_DRAIN        = 60, // Distorts the screen while the player's MP climbs, then strikes and takes the MP
    ACTOR_403600_ACTION_MELEE        = 70, // Closes in on the player and swipes twice
};

/// What the double does, kept in its own `action`.
enum {
    ACTOR_403600_DOUBLE_ACTION_WAIT    = 0, // Hovers for 30 frames
    ACTOR_403600_DOUBLE_ACTION_CHASE   = 1, // Turns to the player and closes in at `chaseSpeed`
    ACTOR_403600_DOUBLE_ACTION_SWIPE_A = 3, // Swipes with the first attack of its kind
    ACTOR_403600_DOUBLE_ACTION_SWIPE_B = 4, // Swipes with the second attack of its kind
    ACTOR_403600_DOUBLE_ACTION_APPEAR  = 5, // Plays its entrance for 70 frames
};

/// What the boss turns towards, kept in `aimMode`. The yaw-only turn knows
/// just the first two.
enum {
    ACTOR_403600_AIM_PLAYER       = 0, // The player
    ACTOR_403600_AIM_TARGET       = 1, // `targetPos`
    ACTOR_403600_AIM_PLAYER_LEVEL = 2, // The player's direction on the ground plane, which levels the body
    ACTOR_403600_AIM_TARGET_SNAP  = 3, // `targetPos`, taken at once instead of at `turnRate`
};

/// Work block of the three enemy tasks this package runs: the boss, the
/// translucent double it summons, and the figure a scene command spawns.
///
/// Each allocates one zeroed at its full size and keeps it at `Task::work`.
/// The double uses the rig, the model matrices, `worldCoord`, the hit and
/// attack bodies, the animation and movement fields and its own lifetime
/// fields. The scene figure uses only `worldCoord`, `mode`, `phaseFrame`,
/// `hitCooldown`, `ambientBoost` and `sceneFrame`. Nothing in this package
/// reads or writes the `pad` runs, whose role is unproven. Angles are 4096 to
/// a turn and speeds are world units a frame.
typedef struct {
    ActorAnimRig20        rig;                // Playback storage of the model's twenty parts; slots 1 to 19 are driven
    MATRIX                color;              // Colour matrix the model is lit with
    MATRIX                light;              // Light matrix the model is lit with
    struct Enemy*         childEnemy;         // Enemy the boss spawned beside itself (the double, or the scene figure), NULL when none
    GfxCoord              worldCoord;         // Coordinate the model's root hangs under: the task's position and facing in the room
    WorldCollisionBody    hitBody;            // Sphere that takes hits and is pushed out of the room's geometry
    WorldCollisionContact hitContacts[4];     // Contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    attackBody;         // Sphere in front of the body that deals the swipes; pairs only on a swipe's hit frames
    WorldCollisionContact attackContacts[1];  // The one contact of `attackBody`; a contact there ends the swipe's hit frames
    WorldCollisionBody    gridBody;           // Boss only: small sphere whose grid test runs during the rush
    byte                  pad_5E0[0x18];
    WorldCollisionContact gridContacts[4];    // Contacts of `gridBody`; a room-grid contact there is how the rush notices the scenery
    EffectSpawnArg        hitEffectArg;       // Boss only: argument of the effect a hit spawns, hung off `worldCoord`
    byte                  pad_660[0x50];
    VECTOR                prevPos;            // Position before this frame's movement, restored per axis once `targetPos` is near
    GfxCoord*             field_6C0;          // Double only: set to its body part's coordinate at spawn; never read, role unproven
    s16                   field_6C4;          // Double only: set to 0x100 at spawn; never read, role unproven
    s16                   field_6C6;          // Double only: set to 1 at spawn; never read, role unproven
    byte                  pad_6C8[0x20];
    SVECTOR               hitEffectOffset;    // Offset from `worldCoord` the hit effect spawns at
    VECTOR                targetPos;          // Room position the task flies to or turns towards under `ACTOR_403600_AIM_TARGET`
    SVECTOR               flinchRot;          // Turn given to the model's part 2 after a hit; only `vx` is set, easing back to 0 by 0x20 a frame
    s16                   screenDistortion;   // Strength of the screen distortion grid, 0 (not drawn) to 0x1000 (tinted over)
    s16                   chainSweep;         // Weight, 4096 = 1, of the pull sweeping the limb chains back; 0x7000 during the rush, else 0
    s16                   chainPullExtra;     // Added to the pull the body chain hangs by; nothing in this package writes it
    s16                   limbPullExtra;      // Added to the pull the limb chains hang by; nothing in this package writes it
    Task*                 fxTask;             // The boss's display task, which draws the distortion grid and owns the chain joints
    byte                  pad_714[0x1C];
    s16                   mode;               // Top-level state, an `ACTOR_403600_MODE_` value
    s16                   step;               // Step within the running action or mode, counted from 0
    s16                   actionParam;        // Value the running action keeps: a wind-up length in frames, the rush's pass count (0xFF once its last dive is set), or a played-once latch
    s16                   animId;             // Animation requested, an index into the package's animation table
    s16                   appliedAnimId;      // Animation the slots were last seeded with; a different `animId` reseeds them
    s16                   phaseFrame;         // Frames since the animation last changed or an action reset the count
    s16                   forwardSpeed;       // Speed along the facing; negative backs away
    s16                   action;             // `ACTOR_403600_ACTION_` value of the boss, `ACTOR_403600_DOUBLE_ACTION_` value of the double
    byte                  pad_740[2];
    s16                   defeated;           // 1 once the task's enemy is defeated: the double then fades and the boss's effect tasks end
    s16                   hitCooldown;        // Frames further hits are ignored for. The scene figure keeps its start delay, then its model scale (0x1000 = 1), here
    s16                   aimMode;            // What the turn follows, an `ACTOR_403600_AIM_` value
    u16                   yaw;                // Heading of `worldCoord`
    s16                   verticalSpeed;      // Speed added to the height while `committed` is clear
    s16                   colorRefreshFrames; // Double only: frames since its lighting colour was last sampled, redone every 10
    s16                   age;                // Double only: frames it has lived
    s16                   chaseSpeed;         // Double only: forward speed of its chase, 20 rising by 1 every 42 frames to 100
    byte                  pad_752[2];
    s16                   lifetime;           // Double only: `age` at which it fades out, 3000 to 3049
    s16                   animBlendFrames;    // Blend length the next animation change is seeded with
    s16                   damageTaken;        // Damage summed since an action last cleared it; 200 breaks the drain and ends `exposed`
    s16                   projectileKind;     // Kind of the volley's projectiles (0 to 3), drawn when the volley starts
    byte                  pad_75C[2];
    s16                   roll;               // Roll added to the facing, advanced each frame of a spinning dive
    s16                   knockbackFrame;     // Frames the player has spent in the current stage of the knock-back sequence
    s16                   knockbackSpeed;     // Speed the player is moved along its own facing while knocked back, decaying to 0
    s16                   shakeFrames;        // Frames of screen shake left
    s16                   shakeFadeFrames;    // Last frames of the shake, run at half strength; 0 when no shake is running
    s16                   drainPuffFrames;    // Frames since the drain last spawned a puff on the player
    s16                   drainPuffArg;       // Spawn argument of the next puff, 0x100 rising to 0x400
    s16                   drainPuffInterval;  // Frames between puffs, shrinking to 2; -1 puffs every part every third frame
    s16                   turnRate;           // Most each angle of the facing changes in a frame
    u16                   swoopCorners;       // Swoop: bit 0 is the high corner nearer the player, bit 1 the low corner nearer the player
    s16                   playerZone;         // Where the player stood when the attack was picked (0 not classified, 1 middle height inside the centre, 2 floor height, 3 middle height outside)
    s16                   ignorePushOut;      // Nonzero keeps the room's geometry from pushing the boss out, for the flights
    s16                   actionDelay;        // Frames the boss waits before choosing; the ascent and the rush reuse it as a bob speed and a frame count
    s16                   animRate;           // Rate the slots tick at (0x10 normal, 0x20 double, 0 stopped)
    s16                   ambientBoost;       // Level added to all three channels of the model's ambient colour; 0 leaves it alone
    s16                   sceneFrame;         // Scene figure only: frames it has run, the index of its rotation sample and view key, held at 0x2BB
    byte                  pad_77E[2];
    s16                   rushAngle;          // Rush: bearing from the room's centre of the point the next odd pass starts from
    s16                   rushPasses;         // Rush: passes to make before the last dive (0, 2, 4 or 6)
    s16                   committed;          // 1 while a move is under way: the boss moves along its whole facing, status hits do not register and its face texture is swapped
    s16                   gridHitLatched;     // Rush: set once a room-grid contact of the pass has been handled
    byte                  pad_788[2];
    s16                   hpMax;              // HP the boss started with
    s16                   weakPhase;          // Weakened phase (0 none, 1 weakened: slower, bobbing and puffing, 2 recovering)
    byte                  pad_78E[2];
    s16                   stunFrames;         // Frames `ACTOR_403600_MODE_STUN` or `ACTOR_403600_MODE_FREEZE` has left
    s16                   actionTimer;        // Countdown or count the running action keeps (a delay, the wait of a rush pass, the drain's MP tick)
    s16                   actionCounter;      // Second count of the running action (the reload of `actionTimer`, the bobs left in the ascent)
    s16                   recoilSpeed;        // Speed taken off `forwardSpeed` after a hit, sized by the damage; negative for a hit from behind
    s16                   hpAt60Percent;      // 60% of `hpMax`; above it the volley and the swoop are not chosen
    s16                   hpAt35Percent;      // 35% of `hpMax`; the double is summoned only at or below it
    s16                   drainStartMp;       // Player's MP when the drain began; stored, never read
    u16                   weakFrames;         // Frames spent weakened; recovery starts after 900 once HP is back above a tenth
    u16                   recoilHold;         // Frames `recoilSpeed` holds before it decays
    s16                   whiteout;           // Brightness of the additive white overlay, 0 to 0xFF
    s16                   diving;             // 1 during a dive, when three particular attacks knock the boss into the weakened phase
    s16                   repositioning;      // 1 while the rush moves the boss, untargetable, to its next side
    s16                   appliedFace;        // Value of `committed` the face texture was last set for
    byte                  pad_7AA[2];
    s16                   pauseSoundSent;     // 1 once sound event 8 went to every area bank for a pause or menu; cleared when event 9 follows
    s16                   exposed;            // 1 after a dive lands: hits do double damage until enough is taken or a reaction ends it
    s16                   summonCount;        // Times the double has been summoned; no more after 10
    u16                   meleeChaseFrames;   // Frames the melee has spent closing in; it gives up at 90
    s16                   meleeGaveUp;        // 1 after a melee gave up, barring the next one until another attack is picked
    byte                  pad_7B6[2];
} Actor403600Work;
STATIC_ASSERT_SIZEOF(Actor403600Work, 0x7B8);

/// Work block of the boss's effect task, a child of the boss task that
/// `Actor403600Work::fxTask` points back to.
///
/// It keeps the two things that task carries from one frame to the next: the
/// seed of the screen distortion grid, and the points the loose parts of the
/// boss's model swing after. Those parts are the chain of model parts 9 to 11,
/// which hangs from part 8, and parts 15 and 19, each turning about its own
/// origin. A point is a room position cut to 16 bits; it is pulled along at a
/// fixed distance by the part it hangs from, so it lags while the model moves
/// and the part is turned to face it. Nothing in this package reads or writes
/// the `pad` runs, whose role is unproven.
typedef struct {
    byte    pad_0[4];
    s32     gridSeed;     // Random state the grid's jitter was drawn from on the last running frame; a held frame restarts from it and redraws the same grid
    SVECTOR chain[4];     // Joints of the chain, root first: element 0 is model part 8's position and each later one trails its predecessor at 0x485
    byte    pad_28[0xE0];
    SVECTOR limbTips[2];  // Point model parts 15 and 19 each face, trailing 0x898 from the part's origin
    s32     chainsPlaced; // 0 until the first running frame has laid the joints out straight from the model's pose, 1 from then on
} Actor403600FxWork;
STATIC_ASSERT_SIZEOF(Actor403600FxWork, 0x11C);

/// Work block of one projectile of the boss's volley.
///
/// The projectile gathers on one of its owner's model parts, is released
/// along the owner's facing with a random spread, flies straight, steers
/// towards the player for a while and then flies straight again until it hits
/// or its flight runs out, when it fades from the head of its trail back. It
/// is drawn as a glow at its position and a trail of quads behind it, one per
/// remembered position, and carries an attack capsule that covers each step
/// it takes. The spawn argument is its kind: the low four bits pick the glow's
/// tint and the attack. Kinds from 0x1000 up stay on the owner's part, link no
/// body and do not time out; nothing in this package spawns one.
typedef struct {
    SVECTOR               trail[32];         // Room positions, cut to 16 bits, of the last 32 running frames, newest first; `pad` is a random draw giving the angle the point's quad is turned by and, in bit 5, which of two textures it shows
    SVECTOR               direction;         // Direction of flight, 0x1000 = 1; a step is 100 units along it, taken twice a frame
    WorldCollisionBody    attackBody;        // Capsule body riding the projectile's coordinate and carrying the attack of its kind; set up and linked only for kinds below 0x1000
    WorldCollisionCapsule attackCapsule;     // Its shape: radius 200, from the projectile back over the step it last took
    WorldCollisionContact attackContacts[1]; // The one contact of `attackBody`; a contact there ends the flight
    s32                   life;              // Frames of flight left, counted down from 300; a contact forces it negative, which starts the fade-out and parks it at 0x7FFFFFFF
} Actor403600ProjectileWork;
STATIC_ASSERT_SIZEOF(Actor403600ProjectileWork, 0x15C);

extern TmdSource gActor403600EveBody;

extern TmdSource gActor403600Model199B8;

extern DamageAttack D_actor_403600_80150E9C;

extern u16 D_actor_403600_80150EA4;

extern u16 D_actor_403600_80150EAC;

extern DamageAttack D_actor_403600_80150EB0;

extern EnemyParams D_actor_403600_80150EC8;

extern EnemyParams D_actor_403600_80150ED8;

extern AnimationSet gActor403600Animation1FEB0;

extern AnimationSet gActor403600Animation208EC;

extern AnimationSet gActor403600Animation2139C;

extern AnimationSet gActor403600Animation22424;

extern AnimationSet gActor403600Animation22904;

extern AnimationSet gActor403600Animation237A8;

extern AnimationSet gActor403600Animation241DC;

extern AnimationSet gActor403600Animation24C54;

extern AnimationSet gActor403600Animation25670;

extern AnimationSet gActor403600Animation26650;

extern AnimationSet gActor403600Animation27184;

extern AnimationSet gActor403600Animation27A60;

extern AnimationSet gActor403600Animation283F8;

extern AnimationSet gActor403600Animation29234;

extern AnimationSet gActor403600Animation29674;

extern AnimationSet gActor403600Animation29CB4;

extern AnimationSet gActor403600Animation2A708;

extern AnimationSet gActor403600Animation2A8EC;

extern AnimationSet gActor403600Animation2B364;

extern AnimationSet gActor403600Animation2BF8C;

extern AnimationSet gActor403600Animation2C0C4;

extern AnimationSet gActor403600Animation2C90C;

extern AnimationSet gActor403600Animation2D0A8;

extern AnimationSet gActor403600Animation2D8B4;

extern AnimationSet gActor403600Animation2E0C8;

extern AnimationSet gActor403600Animation2E6BC;

extern TaskDesc D_actor_403600_801421A0[4];

extern s32 D_actor_403600_80160698;

extern GfxCoord* D_actor_403600_801606A0;

void func_actor_403600_80138C68(Task* arg0);

void func_actor_403600_80132E40(Task* arg0, Actor403600Work* work, Actor403600FxWork* fx);

void func_actor_403600_80138C34(Task* task);

#endif // SRC_ACTORS_ACTOR_403600_ACTOR_403600_PRIVATE_H
