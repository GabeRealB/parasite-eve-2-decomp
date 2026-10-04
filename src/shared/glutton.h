/* The Glutton (actor_403200 and actor_444000), fought in the Shelter B3
 * dumping hole / garbage incinerator (area 0x27). A large jointed host with seven escort models charges along the
 * arena's x axis, turns and charges back, pushing a collision wall rebuilt in
 * front of it. It yaws and pitches its neck parts toward the player and swings
 * a seven-segment limb whose extension is a shared reach counter. It cross-
 * fades three animation pairs. It flings sub-enemies: a chunk thrown forward
 * from escort 0 that falls with a ground shadow; a glob spat from escort 1
 * that bounces, stretches and, if the player is in reach, engulfs them by
 * installing a caught animation on the player; debris chunks from the owner's
 * part 3 that drop, slide and settle with smoke puffs; blobs launched high
 * off-screen that rain onto points on a ring around the host and splat flat;
 * and spinners that wait hidden, then spiral toward a target point. A shared
 * end flag makes every sub-enemy tear itself down when the fight ends. It uses
 * ActorContact_TurnJoint and ActorContact_PushContact from the existing
 * actor_contacts library.
 *
 * GLUTTON_ROOM configures the shared code for one encounter at compile time.
 * Each carrier must define it before including this header, using
 * GLUTTON_DUMPING_HOLE (actor_403200) or GLUTTON_INCINERATOR (actor_444000),
 * and retain that binding through every shared fragment. This header defines
 * the selector values and rejects unsupported values before selecting the
 * encounter's hit-effect dimensions. The fragments use the same selection
 * for damage, escort and shake behavior.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GLUTTON_H
#define SRC_SHARED_GLUTTON_H

/// Binds the shared shake helper to the current instance's borrowed host task.
///
/// Must expand to a side-effect-free `Task*` expression. The default selects
/// actor_403200's private pointer; actor_444000 overrides it before this header
/// with its storage record's `task` member. Both carriers declare the storage
/// before including the shake helper. Evaluated once per shake request; the
/// host and its `GluttonWork` must be alive. Spawn publishes the pointer,
/// and teardown leaves it stale, so requests require a successfully spawned,
/// still-live host. The binding neither owns the task nor checks for NULL.
#ifndef GLUTTON_HOST_TASK
#define GLUTTON_HOST_TASK (_gGluttonHostTask)
#endif

/// Compile-time `GLUTTON_ROOM` selector for the Shelter B3 dumping-hole Glutton.
///
/// `actor_403200` binds `GLUTTON_ROOM` to this value before including this
/// header and keeps that binding for the shared fragments. It selects the
/// encounter's hit, escort and shake behavior. This dimensionless integer must
/// remain a macro because the fragments compare it in `#if` directives.
#define GLUTTON_DUMPING_HOLE 1

/// Compile-time `GLUTTON_ROOM` selector for the Shelter B3 garbage-incinerator Glutton.
///
/// `actor_444000` binds `GLUTTON_ROOM` to this value before including this
/// header and keeps that binding for all shared fragments. It selects the
/// encounter's hit reactions, escort setup and shake handling. This
/// dimensionless integer must remain a macro because the fragments compare
/// `GLUTTON_ROOM` in `#if` directives.
#define GLUTTON_INCINERATOR 2
#ifndef GLUTTON_ROOM
#error "define GLUTTON_ROOM (GLUTTON_DUMPING_HOLE or GLUTTON_INCINERATOR) before including glutton.h"
#elif GLUTTON_ROOM != GLUTTON_DUMPING_HOLE && GLUTTON_ROOM != GLUTTON_INCINERATOR
#error "GLUTTON_ROOM must be GLUTTON_DUMPING_HOLE or GLUTTON_INCINERATOR"
#endif

/* Extent of the 0x6009C effect a group-0 hit spawns; the Incinerator's is
 * half the Dumping Hole's. */
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
#define GLUTTON_GROUP0_HIT_FX_Y 0x320
#define GLUTTON_GROUP0_HIT_FX_Z 0x3E8
#else
#define GLUTTON_GROUP0_HIT_FX_Y 0x190
#define GLUTTON_GROUP0_HIT_FX_Z 0x1F4
#endif

#include "types.h"

#include "actors/actor.h"

#include "main/coord.h"
#include "main/gfx_types.h"
#include "main/task_types.h"

/// Graphics coordinate node with packed word access to its local rotation.
///
/// `node` is the transform node itself: what composition, parenting and every
/// `GfxCoord` consumer take. `packed` views the same storage with the local
/// matrix as a `GfxMatrix`, so its 3x3 can be written as whole words; an
/// identity is four word stores and one halfword store. Both views are live at
/// once and neither owns anything the other does not.
///
/// The Glutton uses this wherever it builds a node's rotation in place: the
/// host's free coordinate, the coordinate hung beneath its part 4, and the
/// frame-local node under a falling rain blob's ground shadow.
typedef union {
    GfxCoord node;
    struct {
        u32       composeStamp; // `node.composeStamp`
        GfxMatrix coord;        // `node.coord`, with word access to its coefficients
    } packed;
} GluttonCoord;
STATIC_ASSERT_SIZEOF(GluttonCoord, 0x50);

/// One hit sphere of the Glutton: a collision body and the contact table it
/// records into.
///
/// The body is a pair-tested sphere riding one model part's coordinate, of the
/// host or of an escort, and `contacts` is the table that body alone fills. A
/// hit handler scans a group's table for an attack contact and spawns the hit
/// effect on the body's coordinate. The boss enables and disables the bodies
/// with its state and resets every table's occupied entries each frame.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked into the world's body list; `coord` is the part it rides
    WorldCollisionContact contacts[5]; // The body's own contact table
} GluttonHitGroup;
STATIC_ASSERT_SIZEOF(GluttonHitGroup, 0x98);

/// 0x30-byte scratchpad frame the group-0 hit handler
/// `gluttonHitGroup0` carves off `SCRATCH_STACK_CURSOR` for the one hit it
/// takes this frame. `pos` is the contact point copied out of the `WorldCollisionContact`;
/// `delta` is the player-relative offset whose length is `dist`, the range
/// `Gp_ComputeDamage` scales `damage` by. `rot` doubles as `Gp_SpawnEff`'s
/// rotation argument and, afterwards, as the workspace for the contact point
/// relative to the part's world translation, which `angle` is the yaw of.
typedef struct GluttonHitScratch {
    VECTOR3 delta;
    byte    pad_C[0x4];
    SVECTOR rot;
    SVECTOR pos;
    s32     id;     // attack id of the hit that landed, 0 for none
    u32     damage; // HP taken off the enemy
    s32     dist;   // distance from the player, in world units
    s16     angle;  // yaw of the contact point, wrapped to +/-0x800
    byte    pad_2E[0x2];
} GluttonHitScratch;
STATIC_ASSERT_SIZEOF(GluttonHitScratch, 0x30);

/// 0xC-byte scratchpad frame `func_actor_403200_8013EF6C` carves off
/// the scratch-pad stack for the summon tick: `delta` is the player-relative
/// offset the tick yaws the host by, and `i` is the slot the loop and
/// the 0x7DB message both index `GluttonWork::summons` with.
typedef struct GluttonSpawnScratch {
    SVECTOR delta;
    byte    pad_8[0x2];
    s16     i; // `GluttonWork::summons` slot, 0 or 1
} GluttonSpawnScratch;
STATIC_ASSERT_SIZEOF(GluttonSpawnScratch, 0xC);

/// Scratchpad frame the hit-effect spawner `func_actor_403200_80134044` carves
/// off the scratch-pad stack to hand `func_800FDB18` an effect rotation together with
/// the `EffectSpawnArg` naming the coordinate the effect hangs off.
typedef struct GluttonEffScratch {
    SVECTOR        rot; // effect rotation, chosen from the attack's param 0
    EffectSpawnArg eff; // coordinate, 0x500, 3
} GluttonEffScratch;
STATIC_ASSERT_SIZEOF(GluttonEffScratch, 0x10);

/// Work block shared by three of the projectiles the Glutton flings: the
/// thrown hit sphere, the glob and the debris chunk.
///
/// Each is an enemy task of its own running a table of state handlers. Its
/// spawn state allocates the block zeroed at this size and keeps it at
/// `Task::work`; the states after it time themselves on `stateTicks` and set
/// themselves up on the tick `stateChanged` is raised. The glob and the chunk
/// are lobbed at where the player stood: the model drops `fallStep` a tick
/// while it covers an equal share of `travel`, so it lands on that spot.
///
/// No projectile uses every member. The thrown sphere has the attack body and
/// the ground shadow, the glob the lighting matrices and the player request,
/// and the chunk both bodies and the lighting matrices.
typedef struct {
    VECTOR3               travel;            // Horizontal offset from the launch point to the player, covered in equal shares as the model falls; a settling chunk cuts it to one share and halves it each tick as its slide. A wall contact zeroes it; `vy` stays 0
    byte                  unknown_C[0x54];   // Never accessed; role unproven
    GfxCoord              shadowCoord;       // Node under the view coordinate, kept on the floor below the thrown sphere; its ground shadow is drawn there
    WorldCollisionBody    attackBody;        // Pair-tested sphere riding the model, keyed with one of the owner's attacks
    WorldCollisionBody    gridBody;          // Sphere riding the model that is tested against the room grid, so a wall stops the chunk
    WorldCollisionContact attackContacts[1]; // The attack body's own contact table
    WorldCollisionContact gridContacts[3];   // The grid body's own contact table
    MATRIX                colorMtx;          // Light-colour matrix lent to the model
    MATRIX                lightMtx;          // Light-direction matrix lent to the model
    byte                  unknown_190[0x4];  // Never accessed; role unproven
    AnimationPlayRequest  playerAnim;        // Request sent to the player task to play its caught clips
    s16                   stateChanged;      // 1 on the tick a state is entered, otherwise 0. The glob's and the chunk's dispatchers derive it from `prevState`; the thrown sphere's states raise and clear it themselves
    s16                   fallStep;          // Launch height divided by the ticks the fall takes (15 for the glob, 9 for the chunk); its magnitude is added to the model's y each tick
    s16                   stateTicks;        // Ticks spent in the current state; cleared on entry
    byte                  unknown_1AE[0x2];  // Never accessed; role unproven
    s16                   shadowGrowth;      // How far the ground shadow has spread, in eighths of a size unit on top of its base 0x100; starts at 0x400 and gains 0x60 a tick
    s16                   playerCaught;      // 1 while the caught clip this glob installed is on the player, so only that glob sends the release
    s16                   prevState;         // Task state as of the previous tick, kept by the glob's and the chunk's dispatchers
    byte                  unknown_1B6[0xA];  // Never accessed; role unproven
} GluttonProjectileWork;
STATIC_ASSERT_SIZEOF(GluttonProjectileWork, 0x1C0);

/// Work block of the enemy dispatched through `D_actor_403200_80131F14`, the one
/// that rises out of view and slams back down onto the floor. Its spawn state
/// allocates it with `memCalloc(0x1C0, 0)` and parks it in the task's
/// `Task::work` slot, so the size is the allocation.
///
/// `target` is the landing point the spawn state picks; `coord` is the
/// coordinate its shadow marker is drawn at, kept on the floor directly under
/// the model and refreshed every step; `obj` is its collision node, whose
/// `radius` is the marker size, carrying the one-entry `rec` table. `timer` is
/// the step counter of the current state.
typedef struct GluttonDropWork {
    VECTOR3               target;
    byte                  pad_C[0x4];
    GfxCoord              coord;
    byte                  pad_60[0x50];
    WorldCollisionBody    obj;
    byte                  pad_D0[0x20];
    WorldCollisionContact rec;
    byte                  pad_108[0x88];
    /// The effect the spawn state starts, reparented onto the task so it dies
    /// with it; the landing state tells it to finish.
    EffectWork* eff;
    byte        pad_194[0x16];
    s16         field_1AA;
    u16         timer;
    /// Per-step bias of the rise and fall, rolled off the LCG.
    s16  field_1AE;
    byte pad_1B0[0x10];
} GluttonDropWork;
STATIC_ASSERT_SIZEOF(GluttonDropWork, 0x1C0);

/// Work block of the spinner enemy dispatched through `D_actor_403200_80131F28`:
/// its spawn state allocates it with `memCalloc(0xA0, 0)` and parks it in the
/// task's `Task::work` slot. `spin` counts down while the model only yaws in
/// place and is also the phase that yaw follows; `field_98` is the homing
/// speed and radius and `field_96` the step count that accelerates it.
typedef struct GluttonSpinnerWork {
    byte   pad_0[0x50];
    MATRIX colorMtx;
    MATRIX lightMtx;
    /// Set when the dispatcher sees the state change, cleared when it has not.
    s16  field_90;
    byte pad_92[0x2];
    /// The state the dispatcher last ran, so it can spot the change.
    s16  field_94;
    s16  field_96;
    s16  field_98;
    byte pad_9A[0x2];
    u8   spin;
    byte pad_9D[0x3];
} GluttonSpinnerWork;
STATIC_ASSERT_SIZEOF(GluttonSpinnerWork, 0xA0);

/// Parts of the limb the pose driver walks: coordinates 0 to 6 of escort 4's
/// model, each with a pitch and a target in `GluttonWork`.
enum { GLUTTON_LIMB_PARTS = 7 };

/// `GluttonWork::animStep`: what the animation tick does with `animId` next.
enum {
    GLUTTON_ANIM_STEP_BLEND   = 1, // Seek the driving rigs to `animId`, blending from their pose, unless it is already applied
    GLUTTON_ANIM_STEP_RESTART = 2, // Reset the driving rigs to the start of `animId`
    GLUTTON_ANIM_STEP_PLAYING = 3, // Seeded: the slots only tick
};

/// `GluttonWork::shakeLevel`: the screen shake a state asks for.
///
/// Each level is a fixed run of frames with its own vertical pattern; a level
/// outside this set is never armed.
enum {
    GLUTTON_SHAKE_NONE   = 0, // No shake
    GLUTTON_SHAKE_SHORT  = 1, // 5 frames alternating 0 and 2 pixels
    GLUTTON_SHAKE_MEDIUM = 2, // 10 frames of a four-frame 0, 2, 3, 2 pattern
    GLUTTON_SHAKE_LONG   = 3, // 22 frames of an eight-frame ramp peaking at 4 pixels
};

/// Work block of the Glutton itself, allocated zeroed at this size by its
/// spawn state and kept at `Task::work`.
///
/// The boss is one host model and seven escort models parented to its parts,
/// each escort an enemy task of its own. The host and escorts 0 and 1 are
/// animated together: every clip id is played on all three, each through a
/// driving rig and a second rig whose pose can be mixed into the first. Parts 3
/// and 4 of the host are its neck, turned and pitched toward the player on top
/// of the clip, and escort 4 is the seven-part limb, posed part by part.
///
/// A state machine drives the fight. The per-frame tick runs the handler
/// `state` indexes, after flagging a change of state and counting the ticks
/// spent in it; handlers start their clip and arm their flags on the tick the
/// change is flagged. A handler forces its own entry again by writing -1 to
/// `prevState`.
///
/// Damage arrives through nine hit spheres in four groups, each with its own
/// handler and cooldown. All of it comes off the host's HP, which is mirrored
/// onto the escorts that stand in as lock-on targets.
typedef struct {
    s16           state;                               // Index of the state handler the tick runs
    s16           prevState;                           // `state` as of the previous tick; -1 makes the next tick flag a change
    s16           stateChanged;                        // 1 on the tick `state` differs from `prevState`, otherwise 0
    s16           stateTicks;                          // Ticks since the change was flagged, saturating at 0x7FFF; handlers fire cues at fixed counts
    byte          unknown_8[0x4];                      // Never accessed; role unproven
    ActorAnimRig8 hostRig;                             // Drives the host model; slots 1 to 7 are played
    ActorAnimRig8 hostBlendRig;                        // Second pose source for the host, mixed into `hostRig` while `blending`
    ActorAnimRig4 escort0Rig;                          // Drives escort 0's model
    ActorAnimRig4 escort0BlendRig;                     // Second pose source for escort 0
    ActorAnimRig4 escort1Rig;                          // Drives escort 1's model
    ActorAnimRig4 escort1BlendRig;                     // Second pose source for escort 1
    s16           limbPitchTarget[GLUTTON_LIMB_PARTS]; // Pitch each part of the limb is driven to, by part; the pose picks entries 1 to 6
    byte          unknown_792[0x2];                    // Never accessed; role unproven
    s16           limbPitch[GLUTTON_LIMB_PARTS];       // Pitch each part of the limb is at, walked toward its target and applied as the part's X rotation
    byte          unknown_7A2[0x2];                    // Never accessed; role unproven
    s16           limbPose;                            // Limb pose the targets are picked from (0 to 5); poses 0, 1, 3 and 5 curl the limb by the shared reach counter
    s16           limbPitchStep;                       // Most a `limbPitch` entry moves in one tick, set by the pose
    s32           prevSlot3Cue;                        // Cue index of `hostRig.slots[3]` as of the previous tick, so a cue fires once on arrival
    s32           prevSwipeCue;                        // Cue index of the slot the swipe watches (1, then 2) as of the previous tick
    s8            animStep;                            // `GLUTTON_ANIM_STEP_*`; a state requests BLEND or RESTART and the tick answers with PLAYING
    s8            blending;                            // Nonzero while the tick mixes the blend rigs in; cleared when `hostBlendRig.slots[1]` reaches its boundary
    s8            appliedAnimId;                       // Clip the driving rigs were last seeded with
    s8            animId;                              // Clip a state requests, on the host and escorts 0 and 1 alike
    u16           animTicks;                           // Ticks since the driving rigs were last seeded
    s16           animRate;                            // Slot rate the driving rigs tick at (0x10 normally; raised to fast-forward a clip)
    s16           field_7B8;                           // Seeded with the same 0x10 as `animRate` and never read; role unproven
    s16           blendStep;                           // 2 seeds the blend rigs on the next tick, which then stores 3
    s16           blendAnimId;                         // Clip the blend rigs are seeded with
    s16           blendRate;                           // Slot rate the blend rigs tick at
    s16           blendWeight;                         // Share of the blend rig's pose in the mix, out of 0x1000
    byte          unknown_7C2[0x2];                    // Never accessed; role unproven
    s16           neckYawTarget;                       // Yaw from the host's facing to the player, wrapped to +/-0x800; the neck follows it within +/-0x200
    byte          unknown_7C6[0x2];                    // Never accessed; role unproven
    s16           neckYaw;                             // Yaw the neck is turned by, walked toward the target 0x71 a tick; `swipeCoord` takes the same yaw
    u16           caughtTicks;                         // Ticks since `playerAnim` was last sent to the caught player; bounds the resends
    byte          unknown_7CC[0x4];                    // Never accessed; role unproven
    struct {
        byte unknown_0[0x8];                           // Only ever cleared; role unproven
        s32  prevSlot2Cue;                             // Cue index of `hostRig.slots[2]` as of the previous tick, so a cue fires once on arrival
        byte unknown_C[0x14];                          // Only ever cleared; role unproven
    } clip;                                            // Cleared as one block whenever the driving rigs are seeded
    byte                  unknown_7F0[0x2];            // Never accessed; role unproven
    s8                    spinnersSpawned;             // Dumping hole only: set once the nine spinners have been placed for this round; cleared when the inhale ends
    u8                    freeCountdown;               // Frames until the host's and escorts' model buffers are freed, hiding them meanwhile; 0 disables it
    GluttonHitGroup       hits[9];                     // Hit spheres: 0 and 1 ride the host's part 4 and 2 its part 1; 3 to 5 ride parts 1 to 3 of escort 0, 6 to 8 those of escort 1
    WorldCollisionBody    swipeBody;                   // Capsule body of the limb's swipe, riding `swipeCoord`; pair-tested only on the swipe's strike frame
    WorldCollisionCapsule swipeCapsule;                // Its shape: 0x1B58 forward from the host, radius 0x258
    WorldCollisionContact swipeContacts[5];            // Its contact table; a player contact starts the catch
    MATRIX                lightMtx;                    // Light-direction matrix lent to the host's and every escort's model
    MATRIX                colorMtx;                    // Light-colour matrix lent to the same models
    GluttonCoord          swipeCoord;                  // Node under the host's root that `swipeBody` rides, re-yawed to `neckYaw` as a swipe starts
    s16                   group0Cooldown;              // Ticks before the group-0 hit handler runs again
    s16                   groups3To5Cooldown;          // Ticks before the handler of groups 3 to 5 runs again
    s16                   groups6To8Cooldown;          // Ticks before the handler of groups 6 to 8 runs again
    s16                   groups1To2Cooldown;          // Ticks before the handler of groups 1 and 2 runs again
    s16                   wallDistance;                // How far ahead of the host its collision wall is built; also the least lead the player keeps
    s16                   wallDistanceTarget;          // Distance a state asks for; `wallDistance` closes on it 0x32 a tick
    s16                   wallDrop;                    // How far the wall's lower edge sits below its upper one
    byte                  unknown_E9A[0x12];           // Never accessed; role unproven
    u8                    shakeLevel;                  // `GLUTTON_SHAKE_*` requested; cleared when the shake runs out
    u8                    armedShakeLevel;             // Level the running shake was started for; a request that differs starts a new one
    u8                    shakeFramesRemaining;        // Frames of the running shake left
    s8                    shakeY;                      // Vertical display offset applied this frame, in pixels
    AnimationPlayRequest  playerAnim;                  // Request sent to the player task to play its caught and release clips
    u8                    lastCommandStage;            // Location stage of the last actor command received; never read
    u8                    lastCommandArea;             // Location area of that command; never read
    u8                    lastCommand;                 // Low byte of that command's code; never read
    byte                  pad_EC7;
    s16                   playerCaught;                // 1 while the player is under the boss's scripted animation, from the catch to its release
    s16                   swipeDamageReply;            // Reply to the swipe's damage message; 1 keeps the player in the caught clip instead of releasing it
    Enemy*                escorts[7];                  // The models making up the rest of the body, by escort index; NULL where the spawn failed. 0, 1 and 3 are lock-on targets, 4 is the limb, and 6 is forgotten once spawned
    Enemy*                summons[2];                  // Enemies the boss has called in, dropped as their HP runs out
    Enemy*                lastSpawned;                 // Projectile or spinner spawned most recently; read only while placing it
    s16                   neckPitchEnabled;            // Nonzero: the tick pitches the neck toward `neckPitchTarget`
    s16                   neckYawEnabled;              // Nonzero: the tick turns the neck toward `neckYawTarget`
    s16                   limbPoseEnabled;             // Nonzero: the tick poses the limb
    s16                   hostExposed;                 // 1 while the host is the lock-on target and group 0 takes hits; 0 while escort 3 and groups 1 and 2 stand in
    s16                   prevHostExposed;             // `hostExposed` as last seen by the tick, which relights both targets on a change
    s16                   neckPitchTarget;             // Pitch a state asks of the neck, 0 to 0x500
    s16                   neckPitch;                   // Pitch the neck is at, walked toward the target 0x10 a tick
    s16                   field_F02;                   // Set to 1 as the inhale catches the player and never read; role unproven
    s16                   viewLocked;                  // Nonzero: the tick leaves the camera view alone
    s16                   viewSelector;                // Index of the view-picking function the tick calls with `phase`
    s16                   phase;                       // Step of the fight, advanced as the boss moves on through the arena; attacks and views are picked by it
    s16                   groups3To5Pool;              // Damage groups 3 to 5 absorb before the boss is staggered; refilled then
    s16                   groups6To8Pool;              // Damage groups 6 to 8 absorb before the boss is staggered; refilled then
    s16                   groups1To2Pool;              // Seeded like the other pools and drawn down by groups 1 and 2; never tested
    s16                   attackDelay;                 // Ticks the boss waits in its deciding state before picking an attack
    s16                   deathTicks;                  // Ticks since the fight ended, to 0x100; the death handoff runs at a fixed count
    s16                   collapseSkip;                // How far into the scripted collapse to fast-forward, set by the command that starts it
    s16                   deathDelay;                  // Ticks the host is kept at 1 HP: held up while summons live, then counted down
    byte                  unknown_F18[0x2];            // Never accessed; role unproven
    u8                    pendingHeals;                // Heals granted and not yet played out; a positive count sends the boss to its healing state
    s8                    summonsSpawned;              // Summons called in so far; no more after 8
    s8                    summonsAlive;                // Summons still alive
    s8                    lastAttack;                  // State of the attack picked last, so the same one is not picked twice running
    byte                  unknown_F1E[0x6];            // Never accessed; role unproven
} GluttonWork;
STATIC_ASSERT_SIZEOF(GluttonWork, 0xF24);

void gluttonBuildWall(Task* task, s16 scale, s16 drop, s16 index);
void gluttonPoseLimb(Task* task);
void gluttonTurnNeck(Task* task, s16 arg1);
void gluttonPitchNeck(Task* task, s16 arg1);
void gluttonSeedBlend(Task* task);
void gluttonSwitchAnim(Task* arg0);
void gluttonTickBlended(Task* arg0);
void gluttonTickAnim(Task* arg0);
void gluttonHitEffect(GfxCoord* coord, s32 id);
void gluttonThrowSpawn(Enemy* enemy, Task* task);
void gluttonThrowFly(Enemy* enemy, Task* task);
void gluttonGlobSpawn(Enemy* enemy, Task* task);
void gluttonGlobFall(Enemy* enemy, Task* task);
void gluttonGlobEngulf(Enemy* enemy, Task* task);
void gluttonGlobHold(Enemy* enemy, Task* task);
void gluttonChunkSpawn(Enemy* enemy, Task* task);
void gluttonChunkFall(Enemy* enemy, Task* task);
void gluttonChunkSettle(Enemy* enemy, Task* task);
void gluttonRainSpawn(Enemy* enemy, Task* task);
void gluttonRainRise(Enemy* enemy, Task* task);
void gluttonRainFall(Enemy* enemy, Task* task);
void gluttonRainSplat(Enemy* enemy, Task* task);
void gluttonSpinnerSpawn(Enemy* enemy, Task* task);
void gluttonSpinnerChase(Enemy* enemy, Task* task);
void gluttonExit(Task* arg0);
void gluttonPropSetup(Enemy* enemy, Task* task);
void gluttonPropTick(Enemy* enemy, Task* arg1);
void gluttonSpinnerWait(Enemy* arg0, Task* arg1);

static inline void gluttonShrinkRotation(GfxCoord* coord);
static inline void gluttonScaleRotation(GfxCoord* coord, s16 xz, s32 y);
static inline void gluttonGapToCamera(GfxCoord* coord, SVECTOR* out);

void gluttonGlobTask(Task* arg0);
void gluttonChunkTask(Task* arg0);
void gluttonSpinnerTask(Task* arg0);
void gluttonRainTask(Task* arg0);
void gluttonThrowTask(Task* arg0);
void gluttonPropTask(Task* arg0);
void gluttonSetQuadHeights(s32 arg0, s16 arg1);
void gluttonSetShakeLevel(s8 arg0);
void gluttonSetSpinnersReleased(s16 arg0);
s16  gluttonGetSpinnersReleased(void);

/// Gives the escort the texture page and palette of the current area's
/// third placement, and refreshes its existing model stream.
static __inline__ void gluttonTintEscort(TmdObject* model)
{
    AreaPlacement* entry;

    entry                    = &(actorGetCurrentAreaRec()->placements)[2];
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

#endif /* SRC_SHARED_GLUTTON_H */
