#include "actor_503500_private.h"

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
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/bezier_curve.h"

/// What an arm is doing, as held in `_Actor503500ArmWork::state`.
enum {
    ACTOR_503500_ARM_STATE_IDLE          = 0, // Waits for a command from the boss
    ACTOR_503500_ARM_STATE_STRIKE        = 1, // Has the boss play its side's strike animation, with the attack spheres live through the swing
    ACTOR_503500_ARM_STATE_DYING         = 2, // Destroyed: comes off the boss as a model of its own, falls away and burns out
    ACTOR_503500_ARM_STATE_BECOME_TARGET = 3, // Becomes a target, then idles
};

/// Constants of an arm.
enum {
    ACTOR_503500_ARM_STRIKE_RATE_SLOW = 0xC,  // Strike animation rate while the large-orb emitter riding the arm lives, in sixteenths
    ACTOR_503500_ARM_STRIKE_RATE_FAST = 0x12, // Strike animation rate once that emitter is destroyed
    ACTOR_503500_ARM_RECOVERY_FRAMES  = 600,  // Frames an arm stays at zero health before some is restored
};

/// Work block of an arm of the boss, one of the two slot enemies (slots 10
/// and 11) that strike with its upper limbs.
///
/// Each limb of the boss's model is a chain of three parts hanging from a
/// shoulder part: 11 to 13 for slot 10, 5 to 7 for slot 11. The arm enemy
/// rides on the first of them and animates nothing: the boss's own animation
/// moves the limb. The arm adds a target sphere, two attack spheres on the
/// limb's lower two parts, and a model of its own that stays hidden until
/// the arm is destroyed.
///
/// The boss commands a strike. The arm then asks the boss for its side's
/// strike animation and enables the attack spheres for the frames of the
/// swing; a sphere touching the player knocks the player back along the boss's
/// facing turned to one side. The arm cannot be hit until the boss tells it to
/// become a target, which it does once the large-orb emitter riding that arm
/// (slot 4 for slot 10, slot 5 for slot 11) is destroyed; from then on the
/// strike also plays faster.
///
/// Hits can empty the arm's health without destroying it: it then regains a
/// tenth of its maximum after `ACTOR_503500_ARM_RECOVERY_FRAMES`. Only certain
/// attacks destroy it: some whenever they hit, the others once its health is
/// empty. The boss's limb is then scaled away, the
/// arm's own model takes its pose in world space, and that model is thrown
/// outward, falls and burns out.
///
/// One block per arm exists in a static array; the task's `Task::work`
/// points at its element.
typedef struct {
    byte                   unknown_0[0x40];   // No access found; role unproven. The model is lit with the boss's light and colour matrices
    MATRIX                 knockbackRotation; // Rotation a knock-back pushes the player along the Z of: the boss's root rotation turned 1500/4096 of a turn about Y, one way for each arm. Only the rotation is written
    WorldCollisionBody     body;              // Target sphere of radius 1500, 500 units out along the arm's X; pair-tested only once the arm has become a target
    WorldCollisionContact  contacts[8];       // Contact table of `body`, also the enemy's hit records
    WorldCollisionBody     forearmAttackBody; // Attack sphere of radius 800 on the limb's middle part; pair-tested only during a strike's swing
    WorldCollisionBody     handAttackBody;    // Attack sphere of radius 1200 on the limb's last part, 400 units along its Y; pair-tested with `forearmAttackBody`
    WorldCollisionContact  attackContacts[4]; // Contact table shared by the two attack spheres, emptied every frame
    EffectSpawnArg         hitEffect;         // Record hit effects on the arm are spawned with, bound to the arm's coordinate
    Actor503500FixedVector spin;              // Rotation applied to the falling arm every frame: 16.16 Euler angles in 4096ths of a turn. Only Z is ever raised
    Actor503500FixedVector velocity;          // Velocity of the falling arm in world space, 16.16 units per frame: 16 outward along the limb's X, then gravity on Y
    Actor503500FixedVector positionCarry;     // Travel of the falling arm not yet applied to its coordinate; the integer halves are moved out every frame, leaving the fractions
    s16                    hitCooldown;       // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                    stateFrames;       // Frames counted by the current step of the state
    s16                    recoveryFrames;    // Frames until an arm at zero health regains a tenth of its maximum; 0 when not counting
    s16                    loadCommandSlot;   // CD command-ring slot of the file load queued when the arm comes off. Nothing reads it back
    s8                     side;              // Which arm (0 slot 10, on boss parts 11 to 13; 1 slot 11, on parts 5 to 7)
    s8                     state;             // An `ACTOR_503500_ARM_STATE_*` state
    s8                     stateStep;         // Step within the current state, restarted on every state change
    s8                     strikeRate;        // An `ACTOR_503500_ARM_STRIKE_RATE_*` rate, picked when a strike starts; the swing's frames depend on it
} _Actor503500ArmWork;
STATIC_ASSERT_SIZEOF(_Actor503500ArmWork, 0x224);

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

/// Shape of a lunging chain's model.
enum {
    ACTOR_503500_LUNGING_CHAIN_PART_COUNT = 9, // Model parts: the root and the eight links aimed along the curve
    ACTOR_503500_LUNGING_CHAIN_TIP_PART   = 8, // Last part, which carries the target and attack spheres
};

/// What a lunging chain is doing, as held in `_Actor503500LungingChainWork::state`.
///
/// Value 3 is not used.
enum {
    ACTOR_503500_LUNGING_CHAIN_STATE_IDLE             = 0, // Sways with its tip at the slot's rest offset; a command from the boss starts a lunge
    ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE            = 1, // Curls up over where the player stood, then throws its tip there
    ACTOR_503500_LUNGING_CHAIN_STATE_HOLD             = 2, // Waits out `holdFrames`, then idles; stepped, but nothing enters it
    ACTOR_503500_LUNGING_CHAIN_STATE_DAMAGE_OVER_TIME = 4, // Held between the ticks of a damage-over-time reaction; has no step of its own
    ACTOR_503500_LUNGING_CHAIN_STATE_DYING            = 5, // Health exhausted: leaves the boss, rises, squashes flat and burns away
    ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING        = 6, // Split off a large chain: eases out of `blendStart`, then becomes a target
    ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING        = 7, // Regrown: collapses `blendStart` onto the root and eases out of that at twice the rate
};

/// Work block of a lunging chain, one of the four slot enemies (slots 13 to
/// 16) that take the place of the boss's two large chains.
///
/// The enemy is a nine-part model whose root hangs from part 1 of the boss
/// and whose last part, the tip, carries its target sphere and its attack
/// sphere. Nothing animates it. Every frame the tip's position is stepped
/// toward a target in the frame of that boss part, a cubic Bezier is drawn
/// from the root to the tip, and the links are aimed along samples of it;
/// a sine sway and a curl are then added as pitch on links 2 to 7. Idle, the
/// tip stays at the slot's rest offset. Commanded by the boss, the chain
/// curls up over where the player stood and throws its tip there with the
/// attack sphere live.
///
/// The command waiting in the task at set-up says how the chain came to be.
/// Split off a large chain, it eases out of the pose its model was spawned
/// in; regrown later, it eases out from its root. Either way it becomes a target
/// only once that blend is complete. With neither command it is a target at
/// once.
///
/// One block per slot exists in a static array; the task's `Task::work`
/// points at its element.
typedef struct {
    MATRIX                lightMtx;                                          // Light matrix the model is lit with
    MATRIX                colorMtx;                                          // Colour matrix the model is lit with
    MATRIX                blendStart[ACTOR_503500_LUNGING_CHAIN_PART_COUNT]; // Local matrix of each model part that the laid-out pose is blended from while `blendWeight` is below 0x1000; entry 0 is not used
    WorldCollisionBody    body;                                              // Target sphere of radius 600 on the tip; pair-tested only while the chain is a target
    WorldCollisionContact contacts[8];                                       // Contact table of `body`, also the enemy's hit records
    WorldCollisionBody    attackBody;                                        // Attack sphere of radius 500 on the tip, delivering the slot's first attack; pair-tested from a lunge's throw until the tip lands or the sphere touches the player
    WorldCollisionContact attackContacts[4];                                 // Contact table of `attackBody`, emptied every frame
    EffectSpawnArg        hitEffect;                                         // Record hit effects on the chain are spawned with, bound to the tip
    SVECTOR               linkPoints[ACTOR_503500_LUNGING_CHAIN_PART_COUNT]; // World positions the links are aimed along, root end first: nine samples of the Bezier from the root to the tip
    SVECTOR               linkAngles[ACTOR_503500_LUNGING_CHAIN_PART_COUNT]; // Euler angles given to the links once aimed; only the pitch of entries 2 to 7 is applied, the sway plus the curl
    SVECTOR               tipPosition;                                       // Where the chain ends, in the frame of the boss part the root hangs from
    SVECTOR               previousTipPosition;                               // `tipPosition` before this frame's step. Nothing reads it back
    SVECTOR               tipTarget;                                         // Position `tipPosition` is stepped toward, in the same frame
    SVECTOR               lungeTarget;                                       // World position of the player, latched when a lunge starts
    MATRIX                unscaledRootMatrix;                                // Root part's matrix saved once the dying chain has risen; put back every frame before the collapse scale is applied
    s32                   tipSpeed;                                          // Speed of the tip toward `tipTarget`, 16.16 units per frame
    Fixed16               tipSpeedLimit;                                     // Top tip speed, 16.16: 96 normally, 1024 for a lunge's throw. A thirty-second of it is the per-frame acceleration; its integer half is the distance, summed over the axes, inside which the tip snaps onto the target
    byte                  unknown_3A0[0x4];                                  // No access found; role unproven
    s16                   state;                                             // An `ACTOR_503500_LUNGING_CHAIN_STATE_*` state
    s16                   holdFrames;                                        // Frames the hold state lasts; a stagger sets 5
    s16                   hitCooldown;                                       // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   slowFrames;                                        // Frames during which the tip moves at a quarter of its speed; a stagger or a damage-over-time tick sets 8
    s16                   collapseScaleY;                                    // Vertical scale of the dying chain, 0x1000 down to 0x200
    s16                   stateFrames;                                       // Frames counted by the current state's step
    s16                   stepFrames;                                        // Frames counted by the timed steps of a lunge
    s16                   blendWeight;                                       // Share of the laid-out pose in the model, from 0 (all `blendStart`) to 0x1000 (all laid out); raised while the chain unfolds or regrows
    s16                   swayAmplitude;                                     // Peak pitch of the sway, in 4096ths of a turn
    s16                   swayWeight;                                        // Scale of the sway, 0 to 0x1000; restored while idle, dropped during a lunge's throw
    s16                   swayPhase[ACTOR_503500_LUNGING_CHAIN_PART_COUNT];  // Sway phase of each link in 4096ths of a turn, seeded 0x200 apart; entries 2 to 8 advance 0x80 a frame
    byte                  unknown_3CA[0x2];                                  // No access found; role unproven
    s16                   curlWeight;                                        // Scale of the per-link curl pitch, 0x1000 being 1.0; wound up to 0x2000 before a lunge's throw and released after it
    byte                  unknown_3CE[0x2];                                  // No access found; role unproven
    s8                    stateStep;                                         // Step within the current state, restarted on every state change
    s8                    field_3D1;                                         // Cleared on every state change. Nothing reads it back; role unproven
    byte                  unknown_3D2[0x2];                                  // No access found; role unproven
    s8                    tipArrived;                                        // 1 while `tipPosition` sits on `tipTarget`
    s8                    tipAdvancing;                                      // 1 while the tip accelerates toward its limit, 0 while it slows to a stop; set at set-up and never cleared
    s8                    detached;                                          // 1 once the dying chain's root has been re-parented onto the view; the chain is no longer laid out
    s8                    bufferFreeCountdown;                               // Frames until the model's buffers are freed after it is hidden; negative when idle
} _Actor503500LungingChainWork;
STATIC_ASSERT_SIZEOF(_Actor503500LungingChainWork, 0x3D8);

/// Phase of a ballistic shot, as held in `_Actor503500BallisticShotWork::phase`.
enum {
    ACTOR_503500_BALLISTIC_SHOT_CUT_SHORT = -1, // Touched the player, or flew its whole time without landing; the task moves on to its exit
    ACTOR_503500_BALLISTIC_SHOT_FLYING    = 0,  // Falls along its arc until the room's geometry stops it
    ACTOR_503500_BALLISTIC_SHOT_BURSTING  = 1,  // Rests where it landed, its attack sphere doubled, for six frames
    ACTOR_503500_BALLISTIC_SHOT_SPENT     = 2,  // No longer collides; the task moves on to its exit
};

/// Work block of a ballistic shot, one of the attack tasks the boss's slot
/// enemies launch.
///
/// The launcher places and aims the task's coordinate and passes the launch
/// speed. The shot leaves along that facing and from then on only gravity
/// changes its velocity, so it falls in an arc, drawn by an
/// `EFFECT_BRAHMAN_SMALL_ORB` effect and carrying an attack sphere that is
/// tested against the room's geometry as well as against other bodies. Where
/// the geometry stops it, it rests with the sphere doubled for six frames,
/// then its effect is told to finish and the sphere stops colliding. Touching
/// the player, or flying 61 frames without landing, ends it at once.
/// `Task::spawnArg1` picks the attack the sphere delivers.
///
/// The block is allocated at launch and is the task's `Task::work`.
typedef struct {
    WorldCollisionBody     body;              // Attack sphere on the task's own coordinate, radius 300 in flight and 600 once landed; grid-tested throughout and pair-tested from launch until the burst ends
    WorldCollisionContact  contacts[4];       // Contact table of `body`, emptied every frame after the shot has reacted to it
    Task*                  effectTask;        // Effect drawing the shot, kept as a child of this task; told to finish when the shot touches the player or its burst ends
    Actor503500FixedVector position;          // World position in 16.16; its integer halves are what the coordinate's translation gets
    Actor503500FixedVector launchPosition;    // `position` as it was at launch, which the shot is put back to when the geometry holding it has opposed faces
    Actor503500FixedVector velocity;          // Step added onto `position` every frame, in 16.16: the launch speed along the launch facing, gaining 9.8 units of fall a frame, zeroed on landing
    s32                    gridContactResult; // What resolving the room contacts last reported (0 none, 1 pushed back, 2 opposed faces); any contact lands the shot
    s16                    field_B8;          // Set to 0x1000 at launch. Nothing reads it back; role unproven
    s16                    phaseFrames;       // Frames spent in `phase`
    s8                     phase;             // An `ACTOR_503500_BALLISTIC_SHOT_*` phase
    byte                   unknown_BD[1];     // No field access established; role unproven
    s8                     touchedPlayer;     // Set when a contact names the player's body; cuts the shot short on the next step
    s8                     landed;            // Set on landing; the shot no longer resolves its room contacts
} _Actor503500BallisticShotWork;
STATIC_ASSERT_SIZEOF(_Actor503500BallisticShotWork, 0xC0);

/// Phase of a lingering shot, as held in `_Actor503500LingeringShotWork::phase`.
enum {
    ACTOR_503500_LINGERING_SHOT_SLOWING   = 0, // Flies along its facing, shedding speed, until it has all but stopped
    ACTOR_503500_LINGERING_SHOT_LINGERING = 1, // Hangs where it stopped, still an attack, for as long as its kind allows
    ACTOR_503500_LINGERING_SHOT_SPENT     = 2, // No longer collides; the task moves on to its exit
};

/// Work block of a lingering shot, one of the attack tasks the boss's slot
/// enemies launch.
///
/// The launcher places and aims the task's coordinate and passes the launch
/// speed. The shot then flies along that facing, losing speed every frame,
/// while rising one unit a frame and held inside a fixed rectangle of the
/// ground plane. Once it has all but stopped, its effect stops drawing its
/// sprite and only sheds particles, and the shot hangs there as an attack
/// sphere. After a time set by its kind (`Task::spawnArg1`: 0 large orb,
/// 1 projectile) the sphere stops colliding and the task ends.
///
/// The block is allocated at launch and is the task's `Task::work`.
typedef struct {
    WorldCollisionBody    body;           // Attack sphere of radius 2200 on the task's own coordinate; pair-tested from launch until the linger ends
    WorldCollisionContact contacts[4];    // Contact table of `body`, emptied every frame; the shot does not react to what it touches
    Task*                 effectTask;     // Effect drawing the shot, kept as a child of this task; switched to shedding particles alone when the shot stops
    VECTOR                position;       // World position in 16.16; its integer halves are what the coordinate's translation gets
    VECTOR                launchPosition; // `position` as it was at launch. Nothing reads it back
    byte                  unknown_A4[4];  // No field access established; role unproven
    s32                   speed;          // Length of the forward step in 16.16, taken twice a frame; loses 8 a frame until under 8, then keeps what is left
    s16                   field_AC;       // Set to 0x1000 at launch. Nothing reads it back; role unproven
    s16                   lingerFrames;   // Frames spent in the lingering phase
    s8                    phase;          // An `ACTOR_503500_LINGERING_SHOT_*` phase
} _Actor503500LingeringShotWork;
STATIC_ASSERT_SIZEOF(_Actor503500LingeringShotWork, 0xB4);

/// Constants of an arm's knock-back.
enum {
    ACTOR_503500_KNOCKBACK_START_SPEED  = 0x1000000, // Speed the push starts at, 16.16: 256 units a frame
    ACTOR_503500_KNOCKBACK_SPEED_DECAY  = 0x30000,   // Speed lost every frame, 16.16
    ACTOR_503500_KNOCKBACK_REST_FRAMES  = 20,        // Frames the player is held once the push has stopped
    ACTOR_503500_KNOCKBACK_SHAKE_FRAMES = 8,         // Frames the view shakes for
    ACTOR_503500_KNOCKBACK_SOUND_FRAME  = 17,        // Frame of the task's life on which the knock-back sound plays
};

/// Work block of the knock-back an arm's strike gives the player.
///
/// The arm whose attack sphere touched the player spawns a task with no
/// body that pushes the player away, shakes the view, and then plays the
/// player's two follow-up animations before handing control back. The arm
/// hands over the rotation the push follows, which is copied here; the task
/// then moves the player along that rotation's Z by a speed that starts at
/// `ACTOR_503500_KNOCKBACK_START_SPEED` and falls every frame, or stops at
/// once when the move is refused.
///
/// One static instance exists. The task addresses it directly and leaves
/// its `Task::work` unset.
typedef struct {
    Actor503500FixedVector displacementCarry; // Travel not yet given to the player, 16.16; the integer halves are sent as each frame's move, leaving the fractions
    MATRIX                 rotation;          // Rotation the push follows the Z of, copied from the arm at the start. Only the rotation is written
    s32                    speed;             // Length of this frame's push, 16.16 units per frame
    s16                    restFrames;        // Frames counted since the push stopped
    s16                    shakeFrames;       // Frames of view shake left
} _Actor503500KnockbackWork;
STATIC_ASSERT_SIZEOF(_Actor503500KnockbackWork, 0x38);

/// What a yellow-flash emitter is doing, as held in `_Actor503500YellowFlashEmitterWork::state`.
enum {
    ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE    = 0, // A target; waits for the boss to command an attack
    ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_ATTACK  = 1, // Has the boss play its two attack animations and launches the yellow-flash attack during the first
    ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DYING   = 2, // Health exhausted: stops being a target, sheds hit effects and repaints a block of VRAM
    ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DORMANT = 3, // State after set-up: not yet hit; waits for the boss to tell it to become a target
};

/// Constants of a yellow-flash emitter.
enum {
    ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_LAUNCH_FRAME = 30,  // Frame of the attack's count on which the yellow-flash attack is launched
    ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_FRAMES       = 111, // Frames the attack counts before it waits on the boss's animation
    ACTOR_503500_YELLOW_FLASH_EMITTER_DYING_FRAMES        = 31,  // Frames the dying emitter sheds hit effects
};

/// Work block of the yellow-flash emitter, the slot enemy (slot 12) that
/// launches the boss's yellow-flash attack.
///
/// The emitter has no model: its task carries only a coordinate, hung with
/// no offset or rotation from part 8 of the boss's model, and a target
/// sphere 800 units along that part's Z. It is not among the slot enemies
/// the boss spawns at set-up. The pink-flash emitter (slot 1), which rides the same part,
/// spawns it when it comes off the boss, and the boss then tells it to
/// become a target; until then its sphere is not pair-tested.
///
/// Commanded by the boss, the emitter asks for the boss's animation 12 and
/// then 13, and during the first launches the yellow-flash attack task on its
/// own coordinate. It gives the attack up when the boss recoils from a lost
/// part, is stunned or is defeated.
///
/// A hit that exhausts its health empties the slot and sends the boss into
/// its part-lost recoil. The emitter then sheds hit effects for
/// `ACTOR_503500_YELLOW_FLASH_EMITTER_DYING_FRAMES`, copies a 31 by 40 block
/// of VRAM over the block 32 pixels to its left, and ends its task.
///
/// One static instance exists; the task's `Task::work` points at it.
typedef struct {
    WorldCollisionBody    body;          // Target sphere of radius 800 on the task's coordinate, 800 units along its Z; pair-tested only from the boss's command to become a target until the emitter dies
    WorldCollisionContact contacts[8];   // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        hitEffect;     // Record hit effects on the emitter are spawned with, bound to the task's coordinate
    s16                   hitCooldown;   // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   stateFrames;   // Frames counted by the current step of the state
    s16                   field_EC;      // Set to 2 when the dying state begins and cleared on every state change. Nothing reads it back; role unproven
    byte                  unknown_EE[2]; // No access found; role unproven
    s8                    state;         // An `ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_*` state
    s8                    stateStep;     // Step within the current state, restarted on every state change
    byte                  unknown_F2[2]; // No access found; role unproven
} _Actor503500YellowFlashEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500YellowFlashEmitterWork, 0xF4);

/// What a chain base is doing, as held in `_Actor503500ChainBaseWork::state`.
enum {
    ACTOR_503500_CHAIN_BASE_STATE_COVERED = 0, // State after set-up: not a target; waits until its side has neither its large chain nor a lunging chain
    ACTOR_503500_CHAIN_BASE_STATE_EXPOSED = 1, // A target; counts toward regrowing the side's lunging chains and takes the boss's command to attack
    ACTOR_503500_CHAIN_BASE_STATE_ATTACK  = 2, // Has the boss play its attack animation and launches a projectile partway through it
    ACTOR_503500_CHAIN_BASE_STATE_DYING   = 3, // Health exhausted: stops being a target and sheds effects
};

/// Constants of a chain base.
enum {
    ACTOR_503500_CHAIN_BASE_FIRST_SLOT          = 7,   // Slot of the side-0 base; the side-1 base has the next
    ACTOR_503500_CHAIN_BASE_EXPOSED_FRAMES      = 600, // Frames a base stays exposed before it regrows its chains
    ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH_FRAME = 20,  // Frames the attack counts before it launches its projectile
    ACTOR_503500_CHAIN_BASE_DYING_BURST_FRAME   = 5,   // Value of the dying frame count at which the final burst of effects is spawned
    ACTOR_503500_CHAIN_BASE_DYING_FRAMES        = 31,  // Value of the dying frame count at which the task ends
};

/// Work block of a chain base, one of the two slot enemies (slots 7 and 8)
/// that sit where a side's chains hang from the boss.
///
/// The base has no model: its task carries only a coordinate, hung with an
/// identity rotation and no offset from part 1 of the boss's model, and a
/// target sphere beside the root of its side's chains (slot 2 and then slots
/// 13 and 14 for slot 7; slot 3 and then slots 15 and 16 for slot 8). It is
/// not a target while any of those chains exists.
///
/// Once its side's large chain and both lunging chains are gone the base is
/// exposed: it becomes a target and reports itself at rest, so the boss may
/// command it. Commanded, it asks for the boss's animation 10 and,
/// `ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH_FRAME` frames on, launches a
/// lingering shot of the projectile kind from a fixed point of its
/// coordinate, mirrored for slot 8. It gives the attack up when the boss
/// recoils from a lost part, is stunned or is defeated.
///
/// A base left exposed for `ACTOR_503500_CHAIN_BASE_EXPOSED_FRAMES` regrows
/// both of its side's lunging chains, each with a fifth of the base's own
/// remaining health, and is covered again. A hit that exhausts its health
/// first empties the slot and sends the boss into its part-lost recoil; the
/// base then sheds effects and ends its task, and nothing else regrows that
/// side's chains.
///
/// One block per base exists in a static array; the task's `Task::work`
/// points at its element.
typedef struct {
    WorldCollisionBody    body;          // Target sphere of radius 400 on the task's coordinate, at the side's offset beside the chains' root; pair-tested from the base's exposure until it is covered again or dies
    WorldCollisionContact contacts[8];   // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        hitEffect;     // Record hit effects on the base are spawned with, bound to the task's coordinate
    s16                   hitCooldown;   // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   exposedFrames; // Frames spent in the exposed state since the chains last regrew; kept across an attack and a state change, zeroed only when the chains regrow
    s16                   stateFrames;   // Frames counted by the current step of the state
    s16                   field_EE;      // Cleared on every state change. Nothing reads it back; role unproven
    s8                    state;         // An `ACTOR_503500_CHAIN_BASE_STATE_*` state
    s8                    stateStep;     // Step within the current state, restarted on every state change
    byte                  unknown_F2[2]; // No access found; role unproven
} _Actor503500ChainBaseWork;
STATIC_ASSERT_SIZEOF(_Actor503500ChainBaseWork, 0xF4);

/// What the rear part is doing, as held in `_Actor503500RearPartWork::state`.
enum {
    ACTOR_503500_REAR_PART_STATE_IDLE  = 0, // A target; has no step of its own
    ACTOR_503500_REAR_PART_STATE_DYING = 1, // Health exhausted: stops being a target, sheds effects and shrinks the boss's rear part
};

/// Constants of the rear part.
enum {
    ACTOR_503500_REAR_PART_BOSS_PART            = 16,   // Part of the boss's model the enemy rides on and shrinks
    ACTOR_503500_REAR_PART_DYING_EFFECT_FRAMES  = 2000, // Dying frames during which effects may still be shed; the shrink ends long before
    ACTOR_503500_REAR_PART_SHRINK_STEP_START    = 0x80, // What the scale loses on the first frame of the shrink; 0x1000 is 1.0
    ACTOR_503500_REAR_PART_SHRINK_STEP_DECREASE = 4,    // How much less the scale loses on each following frame; the shrink ends when nothing is left
};

/// Work block of the rear part, the slot enemy (slot 6) that makes a target
/// of the rear of the boss.
///
/// The enemy has no model: its task carries only a coordinate, hung with an
/// identity rotation and no offset from part 16 of the boss's model, which
/// itself hangs behind part 1, and a target sphere further back along that
/// part. It is a target from set-up and never attacks: it reads no command
/// from the boss and reports nothing to it.
///
/// A hit that exhausts its health empties the slot, which lowers the boss's
/// top turning rate, and sends the boss into its part-lost recoil. The enemy
/// then sheds effects from random points around the part while the part is
/// scaled down along its Z, by a step that starts at
/// `ACTOR_503500_REAR_PART_SHRINK_STEP_START` and falls every frame, and
/// ends its task when the step runs out.
///
/// One static instance exists; the task's `Task::work` points at it.
typedef struct {
    WorldCollisionBody    body;          // Target sphere of radius 1000 on the task's coordinate, 500 units along its Y and 1500 back along its Z; pair-tested from set-up until the enemy dies
    WorldCollisionContact contacts[8];   // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        hitEffect;     // Record hit effects on the part are spawned with, bound to the task's coordinate
    s16                   hitCooldown;   // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   stateFrames;   // Frames counted by the current step of the state
    s16                   shrinkScale;   // Z scale given to the boss's part while dying; 0x1000 is 1.0
    s16                   shrinkStep;    // What `shrinkScale` loses this frame; the shrink ends once it is no longer positive
    s8                    state;         // An `ACTOR_503500_REAR_PART_STATE_*` state
    s8                    stateStep;     // Step within the current state, restarted on every state change
    byte                  unknown_F2[2]; // No access found; role unproven
} _Actor503500RearPartWork;
STATIC_ASSERT_SIZEOF(_Actor503500RearPartWork, 0xF4);

/// What a large-orb emitter is doing, as held in `_Actor503500LargeOrbEmitterWork::state`.
enum {
    ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE   = 0, // A target; waits for the boss to command an attack
    ACTOR_503500_LARGE_ORB_EMITTER_STATE_ATTACK = 1, // Has the boss play its attack animation and launches large orbs partway through it
    ACTOR_503500_LARGE_ORB_EMITTER_STATE_DYING  = 2, // Health exhausted: stops being a target, sheds hit and smoke effects and repaints two blocks of VRAM
};

/// Constants of a large-orb emitter.
enum {
    ACTOR_503500_LARGE_ORB_EMITTER_FIRST_SLOT          = 4,   // Slot of the side-0 emitter; the side-1 emitter has the next
    ACTOR_503500_LARGE_ORB_EMITTER_ATTACK_LAUNCH_FRAME = 128, // Frames the attack counts before it launches its orbs
    ACTOR_503500_LARGE_ORB_EMITTER_DYING_REPAINT_FRAME = 20,  // Frame of the dying count on which the VRAM blocks are copied
    ACTOR_503500_LARGE_ORB_EMITTER_DYING_FRAMES        = 31,  // Frames the dying emitter sheds effects
};

/// Work block of a large-orb emitter, one of the two slot enemies (slots 4
/// and 5) that ride the boss's upper limbs and launch its large orbs.
///
/// The emitter has no model: its task carries only a coordinate, hung with
/// an identity rotation from the first part of a limb of the boss's model
/// (part 11 for slot 4, part 5 for slot 5), and a target sphere beside that
/// part. It is a target from set-up. The arm enemy riding the same part
/// (slot 10 or 11) is not one until the emitter is destroyed.
///
/// Commanded by the boss, the emitter asks for the boss's animation 9 and,
/// `ACTOR_503500_LARGE_ORB_EMITTER_ATTACK_LAUNCH_FRAME` frames on, launches
/// lingering shots of the large-orb kind from two fixed points of its
/// coordinate: one while the player is within about a quarter turn of the
/// boss's facing, the other while the player is further round than that, and
/// both where the two ranges overlap. It gives the attack up when the boss
/// recoils from a lost part, is stunned or is defeated.
///
/// A hit that exhausts its health empties the slot and sends the boss into
/// its part-lost recoil. The emitter then sheds hit and smoke effects for
/// `ACTOR_503500_LARGE_ORB_EMITTER_DYING_FRAMES`, copying two blocks of VRAM
/// picked by its side on the way, and ends its task.
///
/// One block per emitter exists in a static array; the task's `Task::work`
/// points at its element.
typedef struct {
    WorldCollisionBody    body;          // Target sphere of radius 1500 on the task's coordinate, 300 units along its X, mirrored for side 1; pair-tested from set-up until the emitter dies
    WorldCollisionContact contacts[8];   // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        hitEffect;     // Record hit effects on the emitter are spawned with, bound to the task's coordinate
    s16                   hitCooldown;   // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   stateFrames;   // Frames counted by the current step of the state
    s8                    side;          // Which emitter (0 slot 4, on boss part 11; 1 slot 5, on boss part 5); side 1 mirrors the X of what the emitter places
    s8                    state;         // An `ACTOR_503500_LARGE_ORB_EMITTER_STATE_*` state
    s8                    stateStep;     // Step within the current state, restarted on every state change
    byte                  unknown_EF[1]; // No access found; role unproven
} _Actor503500LargeOrbEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500LargeOrbEmitterWork, 0xF0);

/// What the small-orb emitter is doing, as held in `_Actor503500SmallOrbEmitterWork::state`.
enum {
    ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE   = 0, // A target; waits for the boss to command an attack
    ACTOR_503500_SMALL_ORB_EMITTER_STATE_ATTACK = 1, // Has the boss play its attack animation and launches a volley of small orbs partway through it
    ACTOR_503500_SMALL_ORB_EMITTER_STATE_DYING  = 2, // Health exhausted: stops being a target, sheds effects and repaints a block of VRAM
};

/// Constants of the small-orb emitter.
enum {
    ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME = 66, // Value of the attack's frame count at which the first orb of the volley is launched
    ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS        = 6,  // Orbs in a volley, launched one a frame
    ACTOR_503500_SMALL_ORB_EMITTER_DYING_REPAINT_FRAME = 8,  // Value of the dying frame count at which the VRAM block is copied
    ACTOR_503500_SMALL_ORB_EMITTER_DYING_FRAMES        = 31, // Value of the dying frame count after which the emitter stops shedding effects
};

/// Work block of the small-orb emitter, the slot enemy (slot 9) that
/// launches the boss's volley of small orbs.
///
/// The emitter has no model: its task carries only a coordinate, hung with
/// an identity rotation and no offset from part 1 of the boss's model, and a
/// target sphere offset along that part's Y and Z. It is a target from
/// set-up.
///
/// Commanded by the boss, the emitter asks for the boss's animation 11 and,
/// `ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME` frames on, launches
/// `ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS` ballistic shots of kind 1,
/// one a frame, from one fixed point of its coordinate. Each has its own
/// launch speed and its own aim off the emitter's facing, and every speed is
/// raised with the player's height (negative Y). It gives the attack up when
/// the boss recoils from a lost part, is stunned or is defeated.
///
/// A hit that exhausts its health empties the slot and sends the boss into
/// its part-lost recoil. The emitter then sheds effects until its count
/// passes `ACTOR_503500_SMALL_ORB_EMITTER_DYING_FRAMES`, copying a block of
/// VRAM on the way, and ends its task.
///
/// One static instance exists; the task's `Task::work` points at it.
typedef struct {
    WorldCollisionBody    body;          // Target sphere of radius 1000 on the task's coordinate, 800 units along its Y and 2000 along its Z; pair-tested from set-up until the emitter dies
    WorldCollisionContact contacts[8];   // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        hitEffect;     // Record hit effects on the emitter are spawned with, bound to the task's coordinate
    s16                   hitCooldown;   // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   stateFrames;   // Frames counted by the current step of the state
    s8                    state;         // An `ACTOR_503500_SMALL_ORB_EMITTER_STATE_*` state
    s8                    stateStep;     // Step within the current state, restarted on every state change
    byte                  unknown_EE[2]; // No access found; role unproven
} _Actor503500SmallOrbEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500SmallOrbEmitterWork, 0xF0);

extern _Actor503500YellowFlashEmitterWork D_actor_503500_80177A6C;

extern _Actor503500RearPartWork D_actor_503500_801776A0;

/// Shared collision identity and packed hit-effect arguments of these boss parts.
enum {
    ACTOR_503500_PART_BODY_KEY            = 0x30023,
    ACTOR_503500_PART_HIT_EFFECT_LOW_ARG  = 0x600,
    ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG = 3,
};

/// Damage multiplier and low-halfword selectors used by these part-hit handlers.
enum {
    ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER = 4,
    ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT      = 7, // Bit 7 selects player (0) or companion (1)
    ACTOR_503500_ATTACK_SOURCE_INDEX_MASK       = 1,
    ACTOR_503500_ATTACHMENT_ATTACK_FLAG         = 0x8000,
    ACTOR_503500_ATTACK_ROW_MASK                = 0x7F,
};

static void _actor503500LungingChainApplyPitch(const SVECTOR* linkAngles, GfxCoord* coordinates);

extern _Actor503500SmallOrbEmitterWork D_actor_503500_8017797C;

extern _Actor503500LargeOrbEmitterWork D_actor_503500_801774C0[2];

static void _actor503500LargeOrbEmitterExit(Task* task);

extern _Actor503500ChainBaseWork D_actor_503500_80177794[2];
static void                      _actor503500ChainBaseExit(Task* task);

static void _actor503500LargeOrbEmitterApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount);
static void _actor503500RearPartApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* rec, s32 count);
static void _actor503500SmallOrbEmitterExit(Task* task);
static void _actor503500SmallOrbEmitterEnterState(Task* task, s32 state);
static void func_actor_503500_801431EC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void func_actor_503500_80140D38(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void _actor503500LungingChainDisableAttackOnPlayerContact(Task* unusedTask, WorldCollisionBody* attackBody, const WorldCollisionContact* contacts, s32 contactCount);
static void func_actor_503500_801437D0(Task* arg0, WorldCollisionContact* arg1, s32 arg2);
static void func_actor_503500_8013B460(Task* arg0);
static void _actor503500LargeOrbEmitterStepDying(Task* task);
static void _actor503500LargeOrbEmitterStepIdle(Task* task);
static void _actor503500LargeOrbEmitterClearReactions(Task* task);
static void _actor503500RearPartExit(Task* task);
static void _actor503500RearPartClearReactions(Task* task);
static void _actor503500RearPartUpdateHits(Task* task);
static void _actor503500RearPartStepState(Task* task);
static void _actor503500RearPartEnterState(Task* task, s8 state);
static void _actor503500ChainBaseClearReactions(Task* task);
static void _actor503500ChainBaseUpdateHits(Task* task);
static void func_actor_503500_8013D990(Task* arg0);
static void _actor503500ChainBaseStepExposed(Task* task, s32 exposureLimitFrames);
static void _actor503500ChainBaseStepCovered(Task* task);
static void func_actor_503500_8013F8AC(Task* arg0);
static void func_actor_503500_801440F0(Task* arg0);
static void func_actor_503500_8013BD88(Task* arg0);
static void func_actor_503500_8013E384(Task* arg0);
static void _actor503500SmallOrbEmitterStepIdle(Task* task);
static void _actor503500SmallOrbEmitterClearReactions(Task* task);
static void _actor503500SmallOrbEmitterUpdateHits(Task* task);
static void func_actor_503500_8013EB60(Task* arg0);
static void _actor503500YellowFlashEmitterExit(Task* task);
static void _actor503500YellowFlashEmitterClearReactions(Task* task);
static void func_actor_503500_8013F830(Task* arg0);
static void _actor503500ChainBaseEnterState(Task* task, s32 state);
static void func_actor_503500_8013F328(Task* arg0);
static void _actor503500YellowFlashEmitterStepIdle(Task* task);
static void _actor503500YellowFlashEmitterStepDormant(Task* task);
static void _actor503500YellowFlashEmitterEnterState(Task* task, s32 state);
static void _actor503500LungingChainStepLunge(Task* task);
static void func_actor_503500_80140654(Task* arg0);
static void _actor503500LungingChainReactToDamage(Task* task);
static void _actor503500LungingChainStepTip(Task* task);
static void _actor503500LungingChainUpdatePose(Task* task);
static void _actor503500LungingChainBlendPose(Task* task);
static void func_actor_503500_80141D7C(Task* arg0);
static void _actor503500LungingChainStepIdle(Task* task);
static void _actor503500LungingChainStepUnfolding(Task* task);
static void _actor503500LungingChainStepRegrowing(Task* task);
static void func_actor_503500_801420C4(Task* arg0);
static void func_actor_503500_801421A8(Task* arg0);
static void _actor503500LungingChainEnterState(Task* task, s32 state);
static void _actor503500ArmStepStrike(Task* task);
static void func_actor_503500_80142980(Task* arg0);
static void _actor503500ArmFrameHook(Task* unusedTask);
static void func_actor_503500_80144004(Task* arg0);
static void _actor503500ArmClearReactions(Task* task, s32 unusedActorControl, Enemy* unusedEnemy);
static void _actor503500ArmStepIdle(Task* task);
static void _actor503500ArmStepBecomeTarget(Task* task);
static void _actor503500ArmEnterState(Task* task, s32 state);
static void _actor503500BallisticShotStep(Task* task);
static void _actor503500BallisticShotReactToContacts(Task* task);
static void _actor503500LingeringShotStep(Task* task);
static void _actor503500LingeringShotClearContacts(Task* task);
static void _actor503500LargeOrbEmitterEnterState(Task* task, s32 state);
static void func_actor_503500_8013B60C(Task* arg0, s32 side, s32 arg2);

/// Storage holding the two arms' work blocks and the eight bytes after them.
///
/// `arms` is indexed by side, the arm's slot less 10: an arm's set-up clears
/// exactly its own element and parks it at `Task::work`, and nothing else
/// addresses the array. The eight bytes after it lie between the second arm's
/// block and the knock-back's work block. They are zero in the image, no
/// clear covers them and nothing in the package addresses them; both
/// neighbours are already aligned, so they are not padding, and whether they
/// are separate unreferenced variables is unproven. They share this
/// allocation only so the data after them keeps its address.
typedef struct {
    _Actor503500ArmWork arms[2];        // Work block of each arm, by side (0 slot 10, 1 slot 11)
    byte                unknown_448[8]; // Zero in the image; no access established and role unproven
} _Actor503500ArmStorage;
STATIC_ASSERT_SIZEOF(_Actor503500ArmStorage, 0x450);

static void _actor503500ArmExit(Task* task);
static void _actor503500BallisticShotExit(Task* task);
static void _actor503500LingeringShotExit(Task* task);

static void _actor503500LungingChainExit(Task* task);

extern _Actor503500LungingChainWork D_actor_503500_80177B60[];

extern _Actor503500ArmStorage D_actor_503500_80178AC0;

/// Work block of the arms' knock-back task.
extern _Actor503500KnockbackWork D_actor_503500_80178F10;

static void _actor503500LargeOrbEmitterInit(Task* task);
static void func_actor_503500_8013BBCC(Task* arg0);
static void _actor503500RearPartInit(Task* task);
static void _actor503500RearPartUpdate(Task* task);
static void _actor503500ChainBaseInit(Task* task);
static void func_actor_503500_8013D7D4(Task* arg0);
static void _actor503500SmallOrbEmitterInit(Task* task);
static void func_actor_503500_8013E9A4(Task* arg0);
static void _actor503500YellowFlashEmitterInit(Task* task);
static void func_actor_503500_8013F6F0(Task* arg0);
static void func_actor_503500_8013FA74(Task* arg0);
static void func_actor_503500_8013FF0C(Task* arg0);
static void _actor503500ArmInit(Task* task);
static void func_actor_503500_80143EB4(Task* arg0);
static void _actor503500BallisticShotInit(Task* task);
static void _actor503500BallisticShotUpdate(Task* task);
static void _actor503500LingeringShotInit(Task* task);
static void _actor503500LingeringShotUpdate(Task* task);

static void _actor503500LargeOrbEmitterUpdateHits(Task* task);
static void _actor503500LungingChainPlaceLinks(const SVECTOR* points, GfxCoord* coordinates);

/// Integrates a shot's signed 16.16 velocity and publishes its integer position.
///
/// Updates XYZ only. The coordinate receives the signed upper halfwords, so
/// negative fractional positions round down and integer overflow narrows to
/// 16 bits. Both objects must be live and in the same world frame; coordinate
/// invalidation belongs to the caller.
static inline void _actor503500BallisticShotIntegratePosition(_Actor503500BallisticShotWork* work, GfxCoord* coord)
{
    work->position.fixed.vx.word += work->velocity.fixed.vx.word;
    work->position.fixed.vy.word += work->velocity.fixed.vy.word;
    work->position.fixed.vz.word += work->velocity.fixed.vz.word;
    coord->coord.t[0]             = work->position.fixed.vx.halves.integer;
    coord->coord.t[1]             = work->position.fixed.vy.halves.integer;
    coord->coord.t[2]             = work->position.fixed.vz.halves.integer;
}

/// Aims the next link's Z axis along a world-space segment and places its origin at the segment's end.
///
/// `scratch` holds the current link's orthonormal world rotation, a signed
/// halfword segment in world axes and a +Y hint in the current link's frame.
/// The segment must normalize to a nonzero direction independent of the hint.
/// Borrows both live objects, uses the scratch stack through the basis builder,
/// and changes GTE state. Writes the next link's local rotation and integer
/// translation; the caller manages its composition stamp.
static inline void _actor503500LungingChainAimNextLink(Actor503500ChainScratch* scratch, GfxCoord* nextLink)
{
    MATRIX*  inverseRotation;
    SVECTOR* direction;

    inverseRotation = &scratch->inverseRotation;
    direction       = &scratch->direction;
    gte_TransposeMatrix(&scratch->worldRotation, inverseRotation);
    gte_SetRotMatrix(inverseRotation);
    gte_ldv0(&scratch->segment);
    gte_rtv0();
    gte_stlvnl(&scratch->localSegment);
    VectorNormalS(&scratch->localSegment, direction);
    gfxBuildOrthonormalBasis(&nextLink->coord, direction, &scratch->up);
    nextLink->coord.t[0] = scratch->localSegment.vx;
    nextLink->coord.t[1] = scratch->localSegment.vy;
    nextLink->coord.t[2] = scratch->localSegment.vz;
}

/// Opens the strike interval and starts its sound at the boss's forearm.
///
/// Requires initialized arm work and a live boss parent. Side selects part
/// 12 or 6 of the boss model; both attack spheres become pair-testable.
static inline void _actor503500ArmBeginStrike(Task* task, _Actor503500ArmWork* work)
{
    enum {
        ACTOR_503500_ARM_STRIKE_SOUND_PART_SIDE_0 = 12,
        ACTOR_503500_ARM_STRIKE_SOUND_PART_SIDE_1 = 6,
    };
    GfxCoord* bossCoords;
    GfxCoord* strikeSoundCoord;

    work->forearmAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->handAttackBody.flags    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    bossCoords                     = task->parent->extra.tmd->coords;
    if (work->side != 0) {
        strikeSoundCoord = &bossCoords[ACTOR_503500_ARM_STRIKE_SOUND_PART_SIDE_1];
    } else {
        strikeSoundCoord = &bossCoords[ACTOR_503500_ARM_STRIKE_SOUND_PART_SIDE_0];
    }
    sndEvtRequestScriptStart(SOUND_BRAHMAN_ARM_STRIKE, (s8)worldCoordGetOriginAudioPan(strikeSoundCoord),
                             (s8)(worldCoordGetOriginAudioDepth(strikeSoundCoord) / 2));
}

/// `Task::state` handlers `actor503500LargeOrbEmitterTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131FF0 = {
    {
        _actor503500LargeOrbEmitterInit,
        func_actor_503500_8013BBCC,
        _actor503500LargeOrbEmitterExit,
    },
};

_Actor503500LargeOrbEmitterWork D_actor_503500_801774C0[2] = { 0 };

_Actor503500RearPartWork D_actor_503500_801776A0 = { 0 };

_Actor503500ChainBaseWork D_actor_503500_80177794[2] = { 0 };

_Actor503500SmallOrbEmitterWork D_actor_503500_8017797C = { 0 };

_Actor503500YellowFlashEmitterWork D_actor_503500_80177A6C = { 0 };

_Actor503500LungingChainWork D_actor_503500_80177B60[4];

_Actor503500ArmStorage D_actor_503500_80178AC0;

_Actor503500KnockbackWork D_actor_503500_80178F10;

static void func_actor_503500_8013D1CC(Task* arg0);

/// Initializes an idle large-orb emitter on the boss's upper limb.
///
/// Requires slot 4 or 5 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows the side's static work block and boss part
/// 11 or 5, links its target sphere, and copies a stored VRAM row to (0, 261).
static void _actor503500LargeOrbEmitterInit(Task* task)
{
    enum { ACTOR_503500_LARGE_ORB_EMITTER_VRAM_ROW_Y = 261 };

    Enemy*                           enemy;
    Task*                            parent;
    GfxCoord*                        coord;
    WorldCollisionContact*           contacts;
    _Actor503500LargeOrbEmitterWork* work;
    s32                              side;

    side   = task->spawnArg1.value - ACTOR_503500_LARGE_ORB_EMITTER_FIRST_SLOT;
    enemy  = task->spawnArg2.pointer;
    parent = task->parent;
    work   = &D_actor_503500_801774C0[side];
    coord  = task->extra.tmd->coords;
    memFillBytes(work, 0, sizeof(*work));
    task->work = work;
    work->side = side;

    // Bind the target to the same boss limb as this side's arm.
    coord->parent = &parent->extra.tmd->coords[D_actor_503500_8016F0E8[side]];
    gfxSetRotIdentity(&coord->coord);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                   = coord;
    enemy->node.state.parts.flags |= (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    enemy->bodyPos.vx              = D_actor_503500_8016F0F0[side].vx;
    enemy->bodyPos.vy              = D_actor_503500_8016F0F0[side].vy;
    enemy->bodyPos.vz              = D_actor_503500_8016F0F0[side].vz;
    contacts                       = work->contacts;
    enemy->param                   = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                    = contacts;
    enemy->hp                      = enemy->param->hpMax;

    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = D_actor_503500_8016F0F0[side].vx;
    work->body.pos.vy           = D_actor_503500_8016F0F0[side].vy;
    work->body.pos.vz           = D_actor_503500_8016F0F0[side].vz;
    work->body.key              = ACTOR_503500_PART_BODY_KEY;
    work->body.radius           = 1500;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->hitEffect.spawnArgLo = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    work->hitEffect.coord      = coord;
    work->hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    work->body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    MoveImage(&D_actor_503500_8016F100, 0, ACTOR_503500_LARGE_ORB_EMITTER_VRAM_ROW_Y);
    task->exitCallback = _actor503500LargeOrbEmitterExit;
    task->state       += 1;
}

/// Applies one eligible attack contact to a large-orb emitter.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// Fatal hits start its dying state before applying status reactions.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 1600 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500LargeOrbEmitterHandleHit(Task* task, _Actor503500LargeOrbEmitterWork* work, Enemy* enemy, GfxCoord* coord,
                                                        const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// `work` selects the side's local sphere centre.
#define ACTOR_503500_LARGE_ORB_EMITTER_PROJECT_HIT_POINT()                                                                                                                                                     \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                      \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                                   \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                                   \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                                   \
    radialScale       = (ACTOR_503500_LARGE_ORB_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                                 \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                                 \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                                 \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                        \
    gte_ldv0(&effectPosition);                                                                                                                                                                                 \
    gte_rtv0();                                                                                                                                                                                                \
    gte_stsv(&effectPosition);                                                                                                                                                                                 \
    effectPosition.vx += D_actor_503500_8016F0F0[work->side].vx;                                                                                                                                               \
    effectPosition.vy += D_actor_503500_8016F0F0[work->side].vy;                                                                                                                                               \
    effectPosition.vz += D_actor_503500_8016F0F0[work->side].vz;
    enum { ACTOR_503500_LARGE_ORB_HIT_EFFECT_RADIUS = 1600 };
    MATRIX    worldRotation;
    MATRIX    inverseRotation;
    VECTOR    attackerOffset;
    SVECTOR   effectPosition;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       criticalHit;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    attackerCoord = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    criticalHit       = 0;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage     *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        criticalHit = 1;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        _actor503500LargeOrbEmitterEnterState(task, ACTOR_503500_LARGE_ORB_EMITTER_STATE_DYING);
    }
    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

        case DAMAGE_PLAYER_REACTION_NONE:
        case 4:
        case 5:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
        case 8:
        case 9:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            damageStartEnemyStagger(enemy);
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            damageStartEnemyBuildup(enemy, attackKey, 0);
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
            break;
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_LARGE_ORB_EMITTER_PROJECT_HIT_POINT();
#undef ACTOR_503500_LARGE_ORB_EMITTER_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to a large-orb emitter.
///
/// Requires initialized work, its live enemy/model and `contactCount` readable
/// elements. Processes earlier equal keys only once and honors hit cooldown.
/// Borrows contacts without changing them; `unusedBody` retains the original
/// four-argument collision-pass interface and is ignored.
static void _actor503500LargeOrbEmitterApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500LargeOrbEmitterWork* work;
    Enemy*                           enemy;
    GfxCoord*                        coord;
    s32                              contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500LargeOrbEmitterHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// `ACTOR_503500_LARGE_ORB_EMITTER_STATE_ATTACK` step of a large-orb emitter,
/// stepped by `stateStep`: applies the boss's animation preset 9, counts
/// `ACTOR_503500_LARGE_ORB_EMITTER_ATTACK_LAUNCH_FRAME` in `stateFrames`, then
/// launches by the player's bearing from the boss, and idles once the boss
/// reports preset 9 done. Orb 0 goes when the bearing is within a quarter
/// turn of a heading 300 to one side of the boss's facing, orb 1 when it is
/// more than a quarter turn from the heading 300 to the other side; `side`
/// picks which side is which, and both go where the ranges overlap. Idles at
/// once while the boss is recoiling, stunned or defeated.
static void func_actor_503500_8013B460(Task* arg0)
{
    _Actor503500LargeOrbEmitterWork* work;
    s16                              angle;
    s32                              offset;
    s32                              side;

    work = arg0->work;
    if (actor503500ShouldInterruptAttack(arg0->parent) != 0) {
        _actor503500LargeOrbEmitterEnterState(arg0, ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0->parent, 9, 0x10);
            work->stateStep++;
            break;
        case 1:
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_LARGE_ORB_EMITTER_ATTACK_LAUNCH_FRAME) {
                offset = -0x12C;
                angle  = actor503500MeasurePlayerBearing(arg0->parent);
                side   = work->side;
                if (side != 0) {
                    offset = 0x12C;
                }
                if (angle > -0x400 - offset && angle < 0x400 - offset) {
                    func_actor_503500_8013B60C(arg0, side, 0);
                }
                if (angle < offset - 0x400 || offset + 0x400 < angle) {
                    func_actor_503500_8013B60C(arg0, side, 1);
                }
                work->stateStep++;
            }
        case 2:
            if (actor503500HasAnimationFinished(arg0->parent, 9) != 0) {
                _actor503500LargeOrbEmitterEnterState(arg0, ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// Spawns effect slot `arg2` of `D_actor_503500_8016E9F0` on the task's own
/// coordinate: the child's translation is the parent world position plus the
/// slot offset rotated into that frame, and its rotation is the parent's world
/// rotation times `RotMatrixZYX` of the slot angles. A nonzero `side` mirrors
/// the slot: the offset's `vx` and the angles' `vy` are negated.
static void func_actor_503500_8013B60C(Task* arg0, s32 side, s32 arg2)
{
    SVECTOR   pos;
    SVECTOR   ofs;
    MATRIX    m;
    GfxCoord* src;
    GfxCoord* coord;
    Task*     task;
    s32*      dst;
    s32*      p;
    s32       i;
    s32       t;
    s16       vx;
    s16       vy;

    src  = arg0->extra.tmd->coords;
    task = taskSpawnFromTable(D_actor_503500_8016E9F0, 1, 0, 0xA00000);
    if (task != NULL) {
        gfxComposeNodeWorldTransform(src, &m, &pos);
        coord  = task->extra.tmd->coords;
        ofs.vx = vx = D_actor_503500_8016F108[arg2].vx;
        ofs.vy      = D_actor_503500_8016F108[arg2].vy;
        ofs.vz      = D_actor_503500_8016F108[arg2].vz;
        if (side != 0) {
            t      = -vx;
            ofs.vx = t;
        }
        gte_SetRotMatrix(&m);
        gte_ldv0(&ofs);
        gte_rtv0();
        gte_stsv(&ofs);
        coord->coord.t[0] = pos.vx + ofs.vx;
        coord->coord.t[1] = pos.vy + ofs.vy;
        coord->coord.t[2] = pos.vz + ofs.vz;
        dst               = (s32*)coord->coord.m;
        p                 = (s32*)m.m;
        for (i = 0; i < 4; i++) {
            *dst++ = *p++;
        }
        coord->coord.m[2][2] = m.m[2][2];
        pos.vx               = D_actor_503500_8016F128[arg2][0].vx;
        pos.vy = vy = D_actor_503500_8016F128[arg2][0].vy;
        pos.vz      = D_actor_503500_8016F128[arg2][0].vz;
        if (side != 0) {
            t      = -vy;
            pos.vy = t;
        }
        RotMatrixZYX(&pos, &m);
        gte_SetRotMatrix(&coord->coord);
        gte_ldclmv(&m);
        gte_rtir();
        gte_stclmv(&coord->coord);
        gte_ldclmv((char*)&m + 2);
        gte_rtir();
        gte_stclmv((char*)&coord->coord + 2);
        gte_ldclmv((char*)&m + 4);
        gte_rtir();
        gte_stclmv((char*)&coord->coord + 4);
    }
}

/// Removes the dying emitter's target and reports its lost slot to the boss.
///
/// Requires live initialized task, work, enemy and root coordinate. Stops pair
/// tests and detaches the enemy's borrowed contacts before reporting part loss.
/// A balanced scene battle hold credits the enemy's rewards. Starts the death
/// sound with signed-byte pan/depth and advances the state step. The collision
/// body remains linked until task exit.
static inline void _actor503500LargeOrbEmitterBeginDeath(Task* task, _Actor503500LargeOrbEmitterWork* work, Enemy* enemy, GfxCoord* partCoord)
{
    s32 audioPan;
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->recs       = NULL;
    worldTargetUnlinkNode(&enemy->node);
    actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
    work->hitCooldown = 0;
    sceneAcquireBattleRef(0);
    sceneReleaseBattleRefWithRewards(task, 0);
    actor503500EnterPartLostState(task->parent);
    enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
    audioPan              = (s8)worldCoordGetOriginAudioPan(partCoord);
    sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, audioPan, (s8)(worldCoordGetOriginAudioDepth(partCoord) / 2));
    work->stateStep++;
}

/// Removes a defeated large-orb emitter and runs its smoke and texture transition.
///
/// Requires live emitter work, enemy and boss parent. Step 0 removes its target
/// and enemy slot and reports part loss; step 1 sheds mirrored effects for
/// 31 active updates and repaints the side's texture on update 20. The following
/// step releases its effect reservation and advances the task to exit.
/// Frame counts are active updates; effect positions narrow to signed halfwords.
static void _actor503500LargeOrbEmitterStepDying(Task* task)
{
    enum {
        ACTOR_503500_LARGE_ORB_DEATH_BEGIN          = 0,
        ACTOR_503500_LARGE_ORB_DEATH_EFFECTS        = 1,
        ACTOR_503500_LARGE_ORB_DEATH_EFFECT_COST    = 3,
        ACTOR_503500_LARGE_ORB_DEATH_HIT_PUFF_ARG   = 0x1800,
        ACTOR_503500_LARGE_ORB_DEATH_SMOKE_PUFF_ARG = 0x80008600,
        ACTOR_503500_LARGE_ORB_DEATH_SOUND_FADE     = 45, // Audio updates, subject to integer fade rounding
        ACTOR_503500_LARGE_ORB_REPAINT_INDEX_SIDE_0 = 2,
        ACTOR_503500_LARGE_ORB_REPAINT_INDEX_SIDE_1 = 4,
        ACTOR_503500_LARGE_ORB_REPAINT_BASE_X       = 320,
        ACTOR_503500_LARGE_ORB_REPAINT_Y            = 256,
        ACTOR_503500_LARGE_ORB_REPAINT_ROW_BASE     = 247,
    };
    Enemy*                           enemy;
    _Actor503500LargeOrbEmitterWork* work;
    GfxCoord*                        partCoord;
    SVECTOR                          effectPosition;
    s32                              side;
    s32                              repaintIndex;
    s32                              effectOffsetIndex;
    s32                              mirroredComponent;

    enemy     = task->spawnArg2.pointer;
    work      = task->work;
    partCoord = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_LARGE_ORB_DEATH_BEGIN:
            // Remove the target before notifying the boss of the lost part.
            _actor503500LargeOrbEmitterBeginDeath(task, work, enemy, partCoord);
            break;
        case ACTOR_503500_LARGE_ORB_DEATH_EFFECTS:
            // Shed mirrored effects while the side's two texture regions are replaced.
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_LARGE_ORB_DEATH_EFFECT_COST) != 0) {
                effectOffsetIndex  = (s16)(work->stateFrames % (s32)ARRAY_SIZE(D_actor_503500_8016F168));
                effectPosition.vx  = D_actor_503500_8016F168[effectOffsetIndex].vx;
                effectPosition.vy  = D_actor_503500_8016F168[effectOffsetIndex].vy;
                effectPosition.vz  = D_actor_503500_8016F168[effectOffsetIndex].vz;
                effectPosition.vx -= work->stateFrames * 10;
                effectPosition.vy -= work->stateFrames * 20;
                if (work->side != 0) {
                    mirroredComponent = effectPosition.vx;
                    effectPosition.vx = -mirroredComponent;
                }
                effectSpawn(EFFECT_HIT_PUFF, partCoord, ACTOR_503500_LARGE_ORB_DEATH_HIT_PUFF_ARG, &effectPosition);
                effectSpawn(EFFECT_SMOKE_PUFF, partCoord, ACTOR_503500_LARGE_ORB_DEATH_SMOKE_PUFF_ARG, &effectPosition);
                mirroredComponent = effectPosition.vz;
                effectPosition.vz = -mirroredComponent;
                effectSpawn(EFFECT_HIT_PUFF, partCoord, ACTOR_503500_LARGE_ORB_DEATH_HIT_PUFF_ARG, &effectPosition);
                effectSpawn(EFFECT_SMOKE_PUFF, partCoord, ACTOR_503500_LARGE_ORB_DEATH_SMOKE_PUFF_ARG, &effectPosition);
            }
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_LARGE_ORB_EMITTER_DYING_FRAMES) {
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, ACTOR_503500_LARGE_ORB_DEATH_SOUND_FADE);
                work->stateStep++;
            } else if (work->stateFrames == ACTOR_503500_LARGE_ORB_EMITTER_DYING_REPAINT_FRAME) {
                side         = work->side;
                repaintIndex = ACTOR_503500_LARGE_ORB_REPAINT_INDEX_SIDE_0;
                if (side != 0) {
                    repaintIndex = ACTOR_503500_LARGE_ORB_REPAINT_INDEX_SIDE_1;
                }
                MoveImage(&D_actor_503500_8016F148[side][0], (repaintIndex << 6) + ACTOR_503500_LARGE_ORB_REPAINT_BASE_X, ACTOR_503500_LARGE_ORB_REPAINT_Y);
                MoveImage(&D_actor_503500_8016F148[side][1], 0, repaintIndex + ACTOR_503500_LARGE_ORB_REPAINT_ROW_BASE);
            }
            break;
        default:
            actor503500ReleaseSlotEffects(task->spawnArg1.value);
            task->state++;
            break;
    }
}

static void func_actor_503500_8013BBCC(Task* arg0)
{
    Enemy*    enemy;
    GfxCoord* coord;

    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500LargeOrbEmitterClearReactions(arg0);
    }
    _actor503500LargeOrbEmitterUpdateHits(arg0);
    func_actor_503500_8013BD88(arg0);
}

/// Detaches a large-orb emitter and releases its collision body and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500LargeOrbEmitterExit(Task* task)
{
    Enemy*                           enemy;
    _Actor503500LargeOrbEmitterWork* work;

    enemy                           = task->spawnArg2.pointer;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Discards stagger, buildup and damage-over-time reactions on a large-orb emitter.
///
/// Requires the emitter's live enemy in `spawnArg2.pointer`; other reaction
/// bits and all health values are retained.
static void _actor503500LargeOrbEmitterClearReactions(Task* task)
{
    Enemy* enemy;

    enemy = task->spawnArg2.pointer;
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ~ENEMY_REACTION_STAGGER;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags &= ~ENEMY_REACTION_DAMAGE_OVER_TIME_BITS;
    }
}

/// Ticks a large-orb emitter's hit cooldown, applies contacts and empties the table.
///
/// Requires initialized work. A nonzero cooldown decrements as a signed
/// halfword and floors at zero. Defeat suppresses new damage, but contacts
/// are cleared on every call, including the defeat frame.
static void _actor503500LargeOrbEmitterUpdateHits(Task* task)
{
    _Actor503500LargeOrbEmitterWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500LargeOrbEmitterApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

static void func_actor_503500_8013BD88(Task* arg0)
{
    _Actor503500LargeOrbEmitterWork* work = arg0->work;

    switch (work->state) {
        case ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE:
            _actor503500LargeOrbEmitterStepIdle(arg0);
            break;
        case ACTOR_503500_LARGE_ORB_EMITTER_STATE_ATTACK:
            func_actor_503500_8013B460(arg0);
            break;
        case ACTOR_503500_LARGE_ORB_EMITTER_STATE_DYING:
            _actor503500LargeOrbEmitterStepDying(arg0);
            break;
    }
}

/// Starts a large-orb emitter attack when its idle task receives the boss's attack command.
///
/// Requires initialized emitter work. Consumes `ACTOR_503500_SLOT_COMMAND_ATTACK`
/// from `killCountdown`; other requests remain pending.
static void _actor503500LargeOrbEmitterStepIdle(Task* task)
{
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500LargeOrbEmitterEnterState(task, ACTOR_503500_LARGE_ORB_EMITTER_STATE_ATTACK);
        task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    }
}

/// Starts a large-orb emitter state and reports its availability to the boss.
///
/// `state` is an `ACTOR_503500_LARGE_ORB_EMITTER_STATE_*` value. Restarts
/// the state step and frame count, consumes any pending boss command, and
/// marks the slot busy unless the new state is idle.
static void _actor503500LargeOrbEmitterEnterState(Task* task, s32 state)
{
    _Actor503500LargeOrbEmitterWork* work = task->work;

    work->state         = state;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE);
}

void actor503500LargeOrbEmitterTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_80131FF0;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500RearPartTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132028 = {
    {
        _actor503500RearPartInit,
        _actor503500RearPartUpdate,
        _actor503500RearPartExit,
    },
};

/// Initializes the boss's rear part as an idle, hittable target.
///
/// Requires slot 6 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows singleton work and boss part 16; the enemy
/// and collision sphere share the fixed local target offset.
static void _actor503500RearPartInit(Task* task)
{
    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    WorldCollisionContact* contacts;

    coord  = task->extra.tmd->coords;
    enemy  = task->spawnArg2.pointer;
    parent = task->parent;
    memFillBytes(&D_actor_503500_801776A0, 0, sizeof(D_actor_503500_801776A0));
    task->work = &D_actor_503500_801776A0;

    coord->parent = &parent->extra.tmd->coords[ACTOR_503500_REAR_PART_BOSS_PART];
    gfxSetRotIdentity(&coord->coord);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F1B0.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F1B0.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F1B0.vz;
    contacts                      = D_actor_503500_801776A0.contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = contacts;
    enemy->hp                     = enemy->param->hpMax;

    D_actor_503500_801776A0.body.coord            = coord;
    D_actor_503500_801776A0.body.context.contacts = contacts;
    D_actor_503500_801776A0.body.key              = ACTOR_503500_PART_BODY_KEY;
    D_actor_503500_801776A0.body.radius           = 1000;
    D_actor_503500_801776A0.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    D_actor_503500_801776A0.body.pos.vx           = D_actor_503500_8016F1B0.vx;
    D_actor_503500_801776A0.body.pos.vy           = D_actor_503500_8016F1B0.vy;
    D_actor_503500_801776A0.body.pos.vz           = D_actor_503500_8016F1B0.vz;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_503500_801776A0.body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(D_actor_503500_801776A0.contacts), 0);
    D_actor_503500_801776A0.hitEffect.spawnArgLo = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    D_actor_503500_801776A0.hitEffect.coord      = coord;
    D_actor_503500_801776A0.hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    D_actor_503500_801776A0.body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    _actor503500RearPartEnterState(task, ACTOR_503500_REAR_PART_STATE_IDLE);
    task->exitCallback = _actor503500RearPartExit;
    task->state       += 1;
}

/// Applies one eligible attack contact to the rear part.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// Fatal hits start its dying state before applying status reactions.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 1400 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500RearPartHandleHit(Task* task, _Actor503500RearPartWork* work, Enemy* enemy, GfxCoord* coord,
                                                 const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// Adds the part's fixed local sphere centre.
#define ACTOR_503500_REAR_PART_PROJECT_HIT_POINT()                                                                                                                                                             \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                      \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                                   \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                                   \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                                   \
    radialScale       = (ACTOR_503500_REAR_PART_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                                 \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                                 \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                                 \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                        \
    gte_ldv0(&effectPosition);                                                                                                                                                                                 \
    gte_rtv0();                                                                                                                                                                                                \
    gte_stsv(&effectPosition);                                                                                                                                                                                 \
    effectPosition.vx += D_actor_503500_8016F1B0.vx;                                                                                                                                                           \
    effectPosition.vy += D_actor_503500_8016F1B0.vy;                                                                                                                                                           \
    effectPosition.vz += D_actor_503500_8016F1B0.vz;
    enum { ACTOR_503500_REAR_PART_HIT_EFFECT_RADIUS = 1400 };
    VECTOR    attackerOffset;
    SVECTOR   effectPosition;
    MATRIX    worldRotation;
    MATRIX    inverseRotation;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       criticalHit;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    attackerCoord = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    criticalHit       = 0;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage     *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        criticalHit = 1;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        _actor503500RearPartEnterState(task, ACTOR_503500_REAR_PART_STATE_DYING);
    }
    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

        case DAMAGE_PLAYER_REACTION_NONE:
        case 4:
        case 5:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
        case 8:
        case 9:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            damageStartEnemyStagger(enemy);
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            damageStartEnemyBuildup(enemy, attackKey, 0);
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
            break;
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_REAR_PART_PROJECT_HIT_POINT();
#undef ACTOR_503500_REAR_PART_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to the rear part.
///
/// Requires initialized work, its live enemy/model and `contactCount` readable
/// elements. Processes earlier equal keys only once and honors hit cooldown.
/// Borrows contacts without changing them; `unusedBody` retains the original
/// four-argument collision-pass interface and is ignored.
static void _actor503500RearPartApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500RearPartWork* work;
    Enemy*                    enemy;
    GfxCoord*                 coord;
    s32                       contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500RearPartHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// Removes the dying part's target, credits rewards and reports its lost boss slot.
///
/// Requires live initialized task, work, enemy and coordinate. Stops pair tests
/// and detaches borrowed contacts before notifying the boss. Starts the death
/// sound and advances the state step; collision storage stays linked until exit.
static inline void _actor503500RearPartBeginDeath(Task* task, _Actor503500RearPartWork* work, GfxCoord* coord)
{
    s32 audioPan;

    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    {
        Enemy* enemy = task->spawnArg2.pointer;
        enemy->recs  = NULL;
    }
    {
        Enemy* enemy = task->spawnArg2.pointer;
        worldTargetUnlinkNode(&enemy->node);
    }
    actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
    work->hitCooldown = 0;
    sceneAcquireBattleRef(0);
    sceneReleaseBattleRefWithRewards(task, 0);
    actor503500EnterPartLostState(task->parent);
    {
        Enemy* enemy          = task->spawnArg2.pointer;
        enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
    }
    audioPan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, audioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    work->shrinkScale = ONE;
    work->shrinkStep  = ACTOR_503500_REAR_PART_SHRINK_STEP_START;
    work->stateStep++;
}

/// Removes the defeated rear target and shrinks the boss's rear model part.
///
/// Requires initialized work, its live enemy and boss parent. Death entry
/// clears the slot, credits rewards and reports part loss. Subsequent active
/// updates shed effects at random local offsets and reduce the part's Z scale
/// (4096 is full size) by a decreasing step. When the step reaches zero,
/// releases the effect reservation; the next update advances the task to exit.
static void _actor503500RearPartStepDying(Task* task)
{
    enum {
        ACTOR_503500_REAR_PART_DEATH_BEGIN           = 0,
        ACTOR_503500_REAR_PART_DEATH_SHRINK          = 1,
        ACTOR_503500_REAR_PART_DEATH_EFFECT_COST     = 5,
        ACTOR_503500_REAR_PART_DEATH_FIRST_PUFF_ARG  = 0x01001900,
        ACTOR_503500_REAR_PART_DEATH_SECOND_PUFF_ARG = 0x01001700,
        ACTOR_503500_REAR_PART_DEATH_DRIFT_ARG       = 0x01404600,
        ACTOR_503500_REAR_PART_DEATH_SOUND_FADE      = 45, // Audio updates; integer fade rounding may extend this
    };
    _Actor503500RearPartWork* work;
    GfxCoord*                 coord;
    SVECTOR                   partScale;

    work  = task->work;
    coord = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_REAR_PART_DEATH_BEGIN:
            // Remove the target before reporting the lost boss part.
            _actor503500RearPartBeginDeath(task, work, coord);
            break;
        case ACTOR_503500_REAR_PART_DEATH_SHRINK:
            // Shed local effects while the rear model part contracts along Z.
            if (++work->stateFrames <= ACTOR_503500_REAR_PART_DYING_EFFECT_FRAMES) {
                if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_REAR_PART_DEATH_EFFECT_COST) != 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_REAR_PART_DEATH_FIRST_PUFF_ARG,
                                &D_actor_503500_8016F1B8[(u16)((gRandomLcgState >> 16) % ARRAY_SIZE(D_actor_503500_8016F1B8))]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_REAR_PART_DEATH_SECOND_PUFF_ARG,
                                &D_actor_503500_8016F1B8[(u16)((gRandomLcgState >> 16) % ARRAY_SIZE(D_actor_503500_8016F1B8))]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, ACTOR_503500_REAR_PART_DEATH_DRIFT_ARG,
                                &D_actor_503500_8016F1B8[(u16)((gRandomLcgState >> 16) % ARRAY_SIZE(D_actor_503500_8016F1B8))]);
                }
            }
            partScale.vx = ONE;
            partScale.vy = ONE;
            partScale.vz = work->shrinkScale;
            actor503500SetBossPartScale(task->parent, ACTOR_503500_REAR_PART_BOSS_PART, &partScale);
            work->shrinkScale -= work->shrinkStep;
            work->shrinkStep  -= ACTOR_503500_REAR_PART_SHRINK_STEP_DECREASE;
            if (work->shrinkStep <= 0) {
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, ACTOR_503500_REAR_PART_DEATH_SOUND_FADE);
                actor503500ReleaseSlotEffects(task->spawnArg1.value);
                work->stateStep++;
            }
            break;
        default:
            task->state++;
            break;
    }
}

/// Updates the rear part's contacts and behavior when scene control permits.
///
/// Requires initialized work, enemy and coordinate. Paused control skips all
/// work; hidden control marks the target not lockable and skips the update.
/// Otherwise dirties the transform, discards supported reactions, processes
/// contacts and steps the part's behavior in that order.
static void _actor503500RearPartUpdate(Task* task)
{
    Enemy*    enemy;
    GfxCoord* coord;

    enemy = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500RearPartClearReactions(task);
    }
    _actor503500RearPartUpdateHits(task);
    _actor503500RearPartStepState(task);
}

/// Detaches the rear-part target and releases its collision body and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500RearPartExit(Task* task)
{
    Enemy*                    enemy;
    _Actor503500RearPartWork* work;

    enemy                           = task->spawnArg2.pointer;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Ages the rear part's hit cooldown, applies contacts and empties their table.
///
/// Requires initialized work and the marked eight-entry contact table. Cooldown
/// uses signed halfword frames and is decremented before hits are considered.
/// A defeated boss suppresses hits; contacts are cleared in either case.
static void _actor503500RearPartUpdateHits(Task* task)
{
    _Actor503500RearPartWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500RearPartApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

/// Discards the rear part's stagger, buildup and damage-over-time reactions.
///
/// Requires its live enemy in `spawnArg2.pointer`. Retains other reaction bits,
/// health and behavior state, including the damage-over-time counters.
static void _actor503500RearPartClearReactions(Task* task)
{
    Enemy* enemy = task->spawnArg2.pointer;

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ~ENEMY_REACTION_STAGGER;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags &= ~ENEMY_REACTION_DAMAGE_OVER_TIME_BITS;
    }
}

/// Steps the rear part's behavior, leaving its idle target unchanged.
///
/// Requires initialized rear-part work. The dying state owns target removal,
/// shrink and the transition to task exit; other values have no step.
static void _actor503500RearPartStepState(Task* task)
{
    _Actor503500RearPartWork* work = task->work;

    switch (work->state) {
        case ACTOR_503500_REAR_PART_STATE_IDLE:
            break;
        case ACTOR_503500_REAR_PART_STATE_DYING:
            _actor503500RearPartStepDying(task);
            break;
    }
}

/// Starts a rear-part state with its step and frame count reset.
///
/// `state` is an `ACTOR_503500_REAR_PART_STATE_*` value. This target has no
/// boss command or slot-availability handshake.
static void _actor503500RearPartEnterState(Task* task, s8 state)
{
    _Actor503500RearPartWork* work;

    work              = task->work;
    work->state       = state;
    work->stateStep   = 0;
    work->stateFrames = 0;
}

void actor503500RearPartTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_80132028;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500ChainBaseTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132060 = {
    {
        _actor503500ChainBaseInit,
        func_actor_503500_8013D7D4,
        _actor503500ChainBaseExit,
    },
};

/// Initializes a covered chain base beside its side's chain roots.
///
/// Requires slot 7 or 8 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows the side's static work and boss part 1.
/// Registers the sphere with pair tests disabled and leaves the target node
/// unlinked until the side's chains are gone.
static void _actor503500ChainBaseInit(Task* task)
{
    enum { ACTOR_503500_CHAIN_BASE_BOSS_PART = 1 };

    Enemy*                     enemy;
    Task*                      parent;
    GfxCoord*                  coord;
    WorldCollisionContact*     contacts;
    _Actor503500ChainBaseWork* work;
    SVECTOR*                   bodyOffset;
    s32                        slot;

    slot       = task->spawnArg1.value;
    enemy      = task->spawnArg2.pointer;
    work       = &D_actor_503500_80177794[slot - ACTOR_503500_CHAIN_BASE_FIRST_SLOT];
    bodyOffset = &D_actor_503500_8016F248[slot - ACTOR_503500_CHAIN_BASE_FIRST_SLOT];
    coord      = task->extra.tmd->coords;
    parent     = task->parent;
    memFillBytes(work, 0, sizeof(*work));
    task->work = work;

    coord->parent = &parent->extra.tmd->coords[ACTOR_503500_CHAIN_BASE_BOSS_PART];
    gfxSetRotIdentity(&coord->coord);
    enemy->field_4                = &coord->coord;
    enemy->field_48               = 0;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = bodyOffset->vx;
    enemy->bodyPos.vy             = bodyOffset->vy;
    enemy->bodyPos.vz             = bodyOffset->vz;
    contacts                      = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = contacts;
    enemy->hp                     = enemy->param->hpMax;

    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = bodyOffset->vx;
    work->body.pos.vy           = bodyOffset->vy;
    work->body.pos.vz           = bodyOffset->vz;
    work->body.key              = ACTOR_503500_PART_BODY_KEY;
    work->body.radius           = 400;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->hitEffect.spawnArgLo = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    work->hitEffect.coord      = coord;
    work->hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    work->body.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    _actor503500ChainBaseEnterState(task, ACTOR_503500_CHAIN_BASE_STATE_COVERED);
    task->exitCallback = _actor503500ChainBaseExit;
    task->state       += 1;
}

/// Applies one eligible attack contact to a chain base.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// Fatal hits start its dying state before applying status reactions.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 200 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500ChainBaseHandleHit(Task* task, _Actor503500ChainBaseWork* work, Enemy* enemy, GfxCoord* coord,
                                                  const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// `task` selects the local centre for slot 7 or 8.
#define ACTOR_503500_CHAIN_BASE_PROJECT_HIT_POINT()                                                                                                                                                             \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                       \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                                    \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                                    \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                                    \
    radialScale       = (ACTOR_503500_CHAIN_BASE_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                                  \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                                  \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                                  \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                         \
    gte_ldv0(&effectPosition);                                                                                                                                                                                  \
    gte_rtv0();                                                                                                                                                                                                 \
    gte_stsv(&effectPosition);                                                                                                                                                                                  \
    {                                                                                                                                                                                                           \
        const SVECTOR* targetCenter = &D_actor_503500_8016F248[task->spawnArg1.value - ACTOR_503500_CHAIN_BASE_FIRST_SLOT];                                                                                     \
        effectPosition.vx          += targetCenter->vx;                                                                                                                                                         \
    }                                                                                                                                                                                                           \
    {                                                                                                                                                                                                           \
        const SVECTOR* targetCenter = &D_actor_503500_8016F248[task->spawnArg1.value - ACTOR_503500_CHAIN_BASE_FIRST_SLOT];                                                                                     \
        effectPosition.vy          += targetCenter->vy;                                                                                                                                                         \
    }                                                                                                                                                                                                           \
    {                                                                                                                                                                                                           \
        const SVECTOR* targetCenter = &D_actor_503500_8016F248[task->spawnArg1.value - ACTOR_503500_CHAIN_BASE_FIRST_SLOT];                                                                                     \
        effectPosition.vz          += targetCenter->vz;                                                                                                                                                         \
    }
    enum { ACTOR_503500_CHAIN_BASE_HIT_EFFECT_RADIUS = 200 };
    VECTOR    attackerOffset;
    SVECTOR   effectPosition;
    MATRIX    worldRotation;
    MATRIX    inverseRotation;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       criticalHit;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    attackerCoord = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    criticalHit       = 0;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage     *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        criticalHit = 1;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        _actor503500ChainBaseEnterState(task, ACTOR_503500_CHAIN_BASE_STATE_DYING);
    }
    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

        case DAMAGE_PLAYER_REACTION_NONE:
        case 4:
        case 5:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
        case 8:
        case 9:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            damageStartEnemyStagger(enemy);
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            damageStartEnemyBuildup(enemy, attackKey, 0);
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
            break;
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_CHAIN_BASE_PROJECT_HIT_POINT();
#undef ACTOR_503500_CHAIN_BASE_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to a chain base.
///
/// Requires initialized work, its live enemy/coordinate and `contactCount`
/// readable elements. Earlier equal keys suppress duplicates; only attack
/// contacts land with a clear cooldown. Fatal hits start the dying state
/// before status reactions are applied. Contacts are borrowed and unchanged.
/// `unusedBody` retains the collision-pass interface and is ignored.
static void _actor503500ChainBaseApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500ChainBaseWork* work;
    Enemy*                     enemy;
    GfxCoord*                  coord;
    s32                        contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500ChainBaseHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// `ACTOR_503500_CHAIN_BASE_STATE_ATTACK` step of a chain base, stepped by
/// `stateStep`: step 0 asks for the boss's animation 10; step 1 counts
/// `ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH_FRAME` frames, then launches a
/// lingering shot of the projectile kind from `D_actor_503500_8016E9F0`,
/// placed at the offset `D_actor_503500_8016F258` in the base's frame and
/// turned by `D_actor_503500_8016F260` (both mirrored for slot 8); step 2
/// returns to `ACTOR_503500_CHAIN_BASE_STATE_EXPOSED` once the animation has
/// run out. A boss that is recoiling, stunned or defeated ends the attack at
/// once.
static void func_actor_503500_8013D1CC(Task* arg0)
{
    SVECTOR                    pos;
    SVECTOR                    ofs;
    MATRIX                     m;
    _Actor503500ChainBaseWork* work;
    GfxCoord*                  coord;
    GfxCoord*                  dst;
    Task*                      task;
    s32*                       src;
    s32*                       out;
    s32                        i;
    s32                        t;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (actor503500ShouldInterruptAttack(arg0->parent) != 0) {
        _actor503500ChainBaseEnterState(arg0, ACTOR_503500_CHAIN_BASE_STATE_EXPOSED);
        actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0->parent, 0xA, 0x10);
            work->stateStep++;
            break;
        case 1:
            if (++work->stateFrames < ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH_FRAME) {
                break;
            }
            task = taskSpawnFromTable(D_actor_503500_8016E9F0, 1, 1, 0xC00000);
            if (task != NULL) {
                gfxComposeNodeWorldTransform(coord, &m, &pos);
                dst    = task->extra.tmd->coords;
                ofs.vx = D_actor_503500_8016F258.vx;
                ofs.vy = D_actor_503500_8016F258.vy;
                ofs.vz = D_actor_503500_8016F258.vz;
                if (arg0->spawnArg1.value == 8) {
                    t      = ofs.vx;
                    ofs.vx = -t;
                }
                gte_SetRotMatrix(&m);
                gte_ldv0(&ofs);
                gte_rtv0();
                gte_stsv(&ofs);
                dst->coord.t[0] = pos.vx + ofs.vx;
                dst->coord.t[1] = pos.vy + ofs.vy;
                dst->coord.t[2] = pos.vz + ofs.vz;
                out             = (s32*)&dst->coord;
                src             = (s32*)&m;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                dst->coord.m[2][2] = m.m[2][2];
                pos.vx             = D_actor_503500_8016F260.vx;
                pos.vy             = D_actor_503500_8016F260.vy;
                pos.vz             = D_actor_503500_8016F260.vz;
                if (arg0->spawnArg1.value == 8) {
                    t      = pos.vy;
                    pos.vy = -t;
                }
                RotMatrixZYX(&pos, &m);
                gte_SetRotMatrix(&dst->coord);
                gte_ldclmv(&m);
                gte_rtir();
                gte_stclmv(&dst->coord);
                gte_ldclmv((char*)&m + 2);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 2);
                gte_ldclmv((char*)&m + 4);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 4);
            }
            work->stateStep++;
            break;
        case 2:
            if (actor503500HasAnimationFinished(arg0->parent, 0xA) != 0) {
                _actor503500ChainBaseEnterState(arg0, ACTOR_503500_CHAIN_BASE_STATE_EXPOSED);
            }
            break;
    }
}

/// Removes the dying part's target, credits rewards and reports its lost boss slot.
///
/// Requires live initialized task, work, enemy and coordinate. Stops pair tests
/// and detaches borrowed contacts before notifying the boss. Starts the death
/// sound and advances the state step; collision storage stays linked until exit.
static inline void _actor503500ChainBaseBeginDeath(Task* task, _Actor503500ChainBaseWork* work, GfxCoord* coord)
{
    s32 audioPan;

    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    {
        Enemy* enemy = task->spawnArg2.pointer;
        enemy->recs  = NULL;
    }
    {
        Enemy* enemy = task->spawnArg2.pointer;
        worldTargetUnlinkNode(&enemy->node);
    }
    actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
    work->hitCooldown = 0;
    sceneAcquireBattleRef(0);
    sceneReleaseBattleRefWithRewards(task, 0);
    actor503500EnterPartLostState(task->parent);
    {
        Enemy* enemy          = task->spawnArg2.pointer;
        enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
    }
    audioPan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, audioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    work->stateStep++;
}

/// Removes a defeated chain base and runs its death burst before task exit.
///
/// Requires initialized work, its live enemy and boss parent. Death entry
/// clears the target and slot, credits rewards and reports part loss. Six
/// subsequent active updates cycle over three local effect offsets on the
/// base's side, ending with a three-sprite burst. The remaining updates count
/// to 31 before releasing the effect reservation and advancing to task exit.
static void _actor503500ChainBaseStepDying(Task* task)
{
    enum {
        ACTOR_503500_CHAIN_BASE_DEATH_BEGIN             = 0,
        ACTOR_503500_CHAIN_BASE_DEATH_EFFECTS           = 1,
        ACTOR_503500_CHAIN_BASE_DEATH_EFFECT_COST       = 2,
        ACTOR_503500_CHAIN_BASE_DEATH_BURST_EFFECT_COST = 6,
        ACTOR_503500_CHAIN_BASE_DEATH_OFFSET_COUNT      = 3,
        ACTOR_503500_CHAIN_BASE_DEATH_HIT_PUFF_ARG      = 0x01001C00,
        ACTOR_503500_CHAIN_BASE_DEATH_FIRST_DRIFT_ARG   = 0x04404600,
        ACTOR_503500_CHAIN_BASE_DEATH_SECOND_DRIFT_ARG  = 0x05404600,
        ACTOR_503500_CHAIN_BASE_DEATH_THIRD_DRIFT_ARG   = 0x06404600,
        ACTOR_503500_CHAIN_BASE_DEATH_SOUND_FADE        = 45, // Audio updates; integer fade rounding may extend this
    };
    _Actor503500ChainBaseWork* work;
    GfxCoord*                  coord;
    SVECTOR*                   effectOffsets;

    work  = task->work;
    coord = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_CHAIN_BASE_DEATH_BEGIN:
            // Remove the target before reporting the lost boss part.
            _actor503500ChainBaseBeginDeath(task, work, coord);
            break;
        case ACTOR_503500_CHAIN_BASE_DEATH_EFFECTS:
            // Each side uses its first three offsets; the second table has other rows.
            if (task->spawnArg1.value == ACTOR_503500_CHAIN_BASE_FIRST_SLOT) {
                effectOffsets = D_actor_503500_8016F278;
            } else {
                effectOffsets = D_actor_503500_8016F290;
            }
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_CHAIN_BASE_DEATH_EFFECT_COST) != 0) {
                effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_CHAIN_BASE_DEATH_HIT_PUFF_ARG, &effectOffsets[work->stateFrames % ACTOR_503500_CHAIN_BASE_DEATH_OFFSET_COUNT]);
            }
            if (work->stateFrames++ >= ACTOR_503500_CHAIN_BASE_DYING_BURST_FRAME) {
                if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_CHAIN_BASE_DEATH_BURST_EFFECT_COST) != 0) {
                    effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, ACTOR_503500_CHAIN_BASE_DEATH_FIRST_DRIFT_ARG, &effectOffsets[0]);
                    effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, ACTOR_503500_CHAIN_BASE_DEATH_SECOND_DRIFT_ARG, &effectOffsets[1]);
                    effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, ACTOR_503500_CHAIN_BASE_DEATH_THIRD_DRIFT_ARG, &effectOffsets[2]);
                }
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, ACTOR_503500_CHAIN_BASE_DEATH_SOUND_FADE);
                work->stateStep++;
            }
            break;
        default:
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_CHAIN_BASE_DYING_FRAMES) {
                actor503500ReleaseSlotEffects(task->spawnArg1.value);
                task->state++;
            }
            break;
    }
}

/// Per-frame tick of a chain base, the same shape as
/// `func_actor_503500_8013BBCC`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
static void func_actor_503500_8013D7D4(Task* arg0)
{
    Enemy*    enemy;
    GfxCoord* coord;

    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500ChainBaseClearReactions(arg0);
    }
    _actor503500ChainBaseUpdateHits(arg0);
    func_actor_503500_8013D990(arg0);
}

/// Detaches a chain base and releases its collision body and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500ChainBaseExit(Task* task)
{
    Enemy*                     enemy;
    _Actor503500ChainBaseWork* work;

    enemy                           = task->spawnArg2.pointer;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Discards a chain base's stagger, buildup and damage-over-time reactions.
///
/// Requires its live enemy in `spawnArg2.pointer`. Retains other reaction bits,
/// health and behavior state, including the damage-over-time counters.
static void _actor503500ChainBaseClearReactions(Task* task)
{
    Enemy* enemy = task->spawnArg2.pointer;

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ~ENEMY_REACTION_STAGGER;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags &= ~ENEMY_REACTION_DAMAGE_OVER_TIME_BITS;
    }
}

/// Ages a chain base's hit cooldown, applies contacts and empties their table.
///
/// Requires initialized work and the marked eight-entry contact table. Cooldown
/// uses signed halfword frames and is decremented before hits are considered.
/// A defeated boss suppresses hits; contacts are cleared in either case.
static void _actor503500ChainBaseUpdateHits(Task* task)
{
    _Actor503500ChainBaseWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500ChainBaseApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

static void func_actor_503500_8013D990(Task* arg0)
{
    s8 temp_v1;

    temp_v1 = ((_Actor503500ChainBaseWork*)arg0->work)->state;
    switch (temp_v1) {
        case ACTOR_503500_CHAIN_BASE_STATE_COVERED:
            _actor503500ChainBaseStepCovered(arg0);
            break;
        case ACTOR_503500_CHAIN_BASE_STATE_EXPOSED:
            _actor503500ChainBaseStepExposed(arg0, ACTOR_503500_CHAIN_BASE_EXPOSED_FRAMES);
            break;
        case ACTOR_503500_CHAIN_BASE_STATE_ATTACK:
            func_actor_503500_8013D1CC(arg0);
            break;
        case ACTOR_503500_CHAIN_BASE_STATE_DYING:
            _actor503500ChainBaseStepDying(arg0);
            break;
    }
}

/// Regrows an exposed base's side chains after its exposure limit, or takes an attack command.
///
/// Requires initialized work, its live enemy and boss parent, in slot 7 or 8.
/// `exposureLimitFrames` counts active exposed updates; the caller supplies 600.
/// The incremented signed-halfword age must strictly exceed the limit. If the
/// side's large-chain slot is empty, attempts both lunging-chain spawns with
/// one fifth of the base's current HP (at least 1) and the regrow command.
/// Restarts the exposure age even when a spawn fails. Either way covers and
/// removes the target. Before expiry, consumes only the boss's attack command.
static void _actor503500ChainBaseStepExposed(Task* task, s32 exposureLimitFrames)
{
    _Actor503500ChainBaseWork* work;
    Enemy*                     enemy;
    Enemy*                     regrownChain;
    s32                        largeChainSlot;
    s32                        firstLungingChainSlot;
    s32                        secondLungingChainSlot;
    s32                        regrownHp;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (exposureLimitFrames < ++work->exposedFrames) {
        // Select the large-chain slot and its two lunging replacements by side.
        largeChainSlot = ACTOR_503500_SLOT_LARGE_CHAIN_1;
        if (task->spawnArg1.value == ACTOR_503500_CHAIN_BASE_FIRST_SLOT) {
            largeChainSlot         = ACTOR_503500_SLOT_LARGE_CHAIN_0;
            firstLungingChainSlot  = ACTOR_503500_SLOT_LUNGING_CHAIN_0;
            secondLungingChainSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_1;
        } else {
            firstLungingChainSlot  = ACTOR_503500_SLOT_LUNGING_CHAIN_2;
            secondLungingChainSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_3;
        }
        if (actor503500IsSlotEmpty(task->parent, largeChainSlot) != 0) {
            regrownChain = actor503500SpawnSlotEnemy(task->parent, firstLungingChainSlot);
            regrownHp    = (s16)(enemy->hp / 5);
            if (regrownHp <= 0) {
                regrownHp = 1;
            }
            if (regrownChain != NULL) {
                regrownChain->task->killCountdown = ACTOR_503500_SLOT_COMMAND_REGROW;
                regrownChain->hp                  = regrownHp;
            }
            regrownChain = actor503500SpawnSlotEnemy(task->parent, secondLungingChainSlot);
            if (regrownChain != NULL) {
                regrownChain->task->killCountdown = ACTOR_503500_SLOT_COMMAND_REGROW;
                regrownChain->hp                  = regrownHp;
            }
            // Both spawn attempts consume this exposure cycle, including failures.
            work->exposedFrames = 0;
        }
        _actor503500ChainBaseEnterState(task, ACTOR_503500_CHAIN_BASE_STATE_COVERED);
        work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldTargetUnlinkNode(&enemy->node);
        work->hitCooldown = 0;
        return;
    }
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500ChainBaseEnterState(task, ACTOR_503500_CHAIN_BASE_STATE_ATTACK);
        task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    }
}

/// Starts a chain-base state and reports its availability to the boss.
///
/// `state` is an `ACTOR_503500_CHAIN_BASE_STATE_*` value. Restarts the state
/// step and frame count, consumes any pending boss command, and marks the
/// slot busy unless exposed. The exposure age survives attacks and state changes.
static void _actor503500ChainBaseEnterState(Task* task, s32 state)
{
    _Actor503500ChainBaseWork* work = task->work;

    work->state         = state;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    work->field_EE      = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_CHAIN_BASE_STATE_EXPOSED);
}

void actor503500ChainBaseTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_80132060;
    stateHandlers.funcs[task->state](task);
}

/// Exposes a covered chain base once all three chain slots on its side are empty.
///
/// Requires an initialized base in slot 7 or 8 and its live enemy. Enters the
/// exposed state, enables the target sphere's pair tests and links the target
/// node. A surviving large chain or either lunging chain keeps the base covered.
static void _actor503500ChainBaseStepCovered(Task* task)
{
    _Actor503500ChainBaseWork* work;
    Enemy*                     enemy;
    s32                        largeChainSlot;
    s32                        firstLungingChainSlot;
    s32                        secondLungingChainSlot;

    largeChainSlot = ACTOR_503500_SLOT_LARGE_CHAIN_1;
    if (task->spawnArg1.value == ACTOR_503500_CHAIN_BASE_FIRST_SLOT) {
        largeChainSlot         = ACTOR_503500_SLOT_LARGE_CHAIN_0;
        firstLungingChainSlot  = ACTOR_503500_SLOT_LUNGING_CHAIN_0;
        secondLungingChainSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_1;
    } else {
        firstLungingChainSlot  = ACTOR_503500_SLOT_LUNGING_CHAIN_2;
        secondLungingChainSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_3;
    }
    if ((actor503500IsSlotEmpty(task->parent, largeChainSlot) != 0) &&
        (actor503500IsSlotEmpty(task->parent, firstLungingChainSlot) != 0) &&
        (actor503500IsSlotEmpty(task->parent, secondLungingChainSlot) != 0)) {
        _actor503500ChainBaseEnterState(task, ACTOR_503500_CHAIN_BASE_STATE_EXPOSED);
        work              = task->work;
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        enemy             = task->spawnArg2.pointer;
        worldTargetLinkNode(&enemy->node);
    }
}

/// `Task::state` handlers `actor503500SmallOrbEmitterTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132098 = {
    {
        _actor503500SmallOrbEmitterInit,
        func_actor_503500_8013E9A4,
        _actor503500SmallOrbEmitterExit,
    },
};

/// Initializes the small-orb emitter as an idle target on the boss's main part.
///
/// Requires slot 9 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows singleton work and boss part 1, with the
/// target sphere and enemy using the same local offset.
static void _actor503500SmallOrbEmitterInit(Task* task)
{
    enum { ACTOR_503500_SMALL_ORB_EMITTER_BOSS_PART = 1 };

    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    WorldCollisionContact* contacts;

    coord  = task->extra.tmd->coords;
    enemy  = task->spawnArg2.pointer;
    parent = task->parent;
    memFillBytes(&D_actor_503500_8017797C, 0, sizeof(D_actor_503500_8017797C));
    task->work = &D_actor_503500_8017797C;

    coord->parent = &parent->extra.tmd->coords[ACTOR_503500_SMALL_ORB_EMITTER_BOSS_PART];
    gfxSetRotIdentity(&coord->coord);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F2D8.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F2D8.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F2D8.vz;
    contacts                      = D_actor_503500_8017797C.contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = contacts;
    enemy->hp                     = enemy->param->hpMax;

    D_actor_503500_8017797C.body.coord            = coord;
    D_actor_503500_8017797C.body.context.contacts = contacts;
    D_actor_503500_8017797C.body.key              = ACTOR_503500_PART_BODY_KEY;
    D_actor_503500_8017797C.body.radius           = 1000;
    D_actor_503500_8017797C.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    D_actor_503500_8017797C.body.pos.vx           = D_actor_503500_8016F2D8.vx;
    D_actor_503500_8017797C.body.pos.vy           = D_actor_503500_8016F2D8.vy;
    D_actor_503500_8017797C.body.pos.vz           = D_actor_503500_8016F2D8.vz;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_503500_8017797C.body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(D_actor_503500_8017797C.contacts), 0);
    D_actor_503500_8017797C.hitEffect.spawnArgLo = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    D_actor_503500_8017797C.hitEffect.coord      = coord;
    D_actor_503500_8017797C.hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    D_actor_503500_8017797C.body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    _actor503500SmallOrbEmitterEnterState(task, ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE);
    task->exitCallback = _actor503500SmallOrbEmitterExit;
    task->state       += 1;
}

/// Applies one eligible attack contact to the small-orb emitter.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// Fatal hits start its dying state before applying status reactions.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 600 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500SmallOrbEmitterHandleHit(Task* task, _Actor503500SmallOrbEmitterWork* work, Enemy* enemy, GfxCoord* coord,
                                                        const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// Adds the part's fixed local sphere centre.
#define ACTOR_503500_SMALL_ORB_EMITTER_PROJECT_HIT_POINT()                                                                                                                                                     \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                      \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                                   \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                                   \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                                   \
    radialScale       = (ACTOR_503500_SMALL_ORB_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                                 \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                                 \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                                 \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                        \
    gte_ldv0(&effectPosition);                                                                                                                                                                                 \
    gte_rtv0();                                                                                                                                                                                                \
    gte_stsv(&effectPosition);                                                                                                                                                                                 \
    effectPosition.vx += D_actor_503500_8016F2D8.vx;                                                                                                                                                           \
    effectPosition.vy += D_actor_503500_8016F2D8.vy;                                                                                                                                                           \
    effectPosition.vz += D_actor_503500_8016F2D8.vz;
    enum { ACTOR_503500_SMALL_ORB_HIT_EFFECT_RADIUS = 600 };
    VECTOR    attackerOffset;
    SVECTOR   effectPosition;
    MATRIX    worldRotation;
    MATRIX    inverseRotation;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       criticalHit;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    attackerCoord = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    criticalHit       = 0;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage     *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        criticalHit = 1;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        _actor503500SmallOrbEmitterEnterState(task, ACTOR_503500_SMALL_ORB_EMITTER_STATE_DYING);
    }
    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

        case DAMAGE_PLAYER_REACTION_NONE:
        case 4:
        case 5:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
        case 8:
        case 9:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            damageStartEnemyStagger(enemy);
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            damageStartEnemyBuildup(enemy, attackKey, 0);
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
            break;
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_SMALL_ORB_EMITTER_PROJECT_HIT_POINT();
#undef ACTOR_503500_SMALL_ORB_EMITTER_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to the small-orb emitter.
///
/// Requires initialized work, its live enemy/coordinate and `contactCount`
/// readable elements. Earlier equal keys suppress duplicates; only attack
/// contacts land with a clear cooldown. Fatal hits start the dying state
/// before status reactions are applied. Contacts are borrowed and unchanged.
/// `unusedBody` retains the collision-pass interface and is ignored.
static void _actor503500SmallOrbEmitterApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500SmallOrbEmitterWork* work;
    Enemy*                           enemy;
    GfxCoord*                        coord;
    s32                              contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500SmallOrbEmitterHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// `ACTOR_503500_SMALL_ORB_EMITTER_STATE_ATTACK` step of the small-orb
/// emitter, stepped by `stateStep`: step 0 applies the boss's animation
/// preset 11; step 1 counts `stateFrames` up to
/// `ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME` and from then on
/// launches one ballistic shot of kind 1 a frame,
/// `ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS` in all; step 3 idles once
/// the boss reports preset 11 done. Step 2 is only ever reached by falling
/// through from step 1. Shot `idx` leaves a fixed offset in the emitter's
/// frame, turned off the emitter's facing by its row of
/// `D_actor_503500_8016F2EC`, at its speed from `D_actor_503500_8016F2E0`
/// raised with the player's height. Idles at once while the boss is
/// recoiling, stunned or defeated.
static void func_actor_503500_8013E384(Task* arg0)
{
    SVECTOR                          pos;
    SVECTOR                          ofs;
    MATRIX                           m;
    _Actor503500SmallOrbEmitterWork* work;
    GfxCoord*                        coord;
    GfxCoord*                        dst;
    Task*                            task;
    s32*                             src;
    s32*                             out;
    s32                              i;
    s32                              arg;
    s16                              idx;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (actor503500ShouldInterruptAttack(arg0->parent) != 0) {
        _actor503500SmallOrbEmitterEnterState(arg0, ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0->parent, 0xB, 0x10);
            work->stateStep++;
            break;
        case 1:
            if (++work->stateFrames < ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME) {
                break;
            }
        case 2:
            idx  = work->stateFrames - ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME;
            arg  = (D_actor_503500_8016F2E0[idx] << 12) + (-gPlayerStatus.coordMtx->t[1] << 24) / 1000;
            task = taskSpawnFromTable(D_actor_503500_8016E9F0, 0, 1, arg);
            if (task != NULL) {
                gfxComposeNodeWorldTransform(coord, &m, &pos);
                dst    = task->extra.tmd->coords;
                ofs.vy = 0x190;
                ofs.vx = 0;
                ofs.vz = 0x960;
                gte_SetRotMatrix(&m);
                gte_ldv0(&ofs);
                gte_rtv0();
                gte_stsv(&ofs);
                dst->coord.t[0] = pos.vx + ofs.vx;
                dst->coord.t[1] = pos.vy + ofs.vy;
                dst->coord.t[2] = pos.vz + ofs.vz;
                src             = (s32*)&m;
                out             = (s32*)&dst->coord;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                dst->coord.m[2][2] = m.m[2][2];
                RotMatrix(&D_actor_503500_8016F2EC[idx], &m);
                gte_SetRotMatrix(&dst->coord);
                gte_ldclmv(&m);
                gte_rtir();
                gte_stclmv(&dst->coord);
                gte_ldclmv((char*)&m + 2);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 2);
                gte_ldclmv((char*)&m + 4);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 4);
            }
            if (idx >= ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS - 1) {
                work->stateStep += 2;
            }
            break;
        case 3:
            if (actor503500HasAnimationFinished(arg0->parent, 0xB) != 0) {
                _actor503500SmallOrbEmitterEnterState(arg0, ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// Removes the dying part's target, credits rewards and reports its lost boss slot.
///
/// Requires live initialized task, work, enemy and coordinate. Stops pair tests
/// and detaches borrowed contacts before notifying the boss. Starts the death
/// sound and advances the state step; collision storage stays linked until exit.
static inline void _actor503500SmallOrbEmitterBeginDeath(Task* task, _Actor503500SmallOrbEmitterWork* work, Enemy* enemy, GfxCoord* coord)
{
    s32 audioPan;

    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->recs       = NULL;
    worldTargetUnlinkNode(&enemy->node);
    actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
    work->hitCooldown = 0;
    sceneAcquireBattleRef(0);
    sceneReleaseBattleRefWithRewards(task, 0);
    actor503500EnterPartLostState(task->parent);
    enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
    audioPan              = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, audioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    work->stateStep++;
}

/// Removes the defeated small-orb emitter and runs its effects and texture transition.
///
/// Requires initialized work, its live enemy and boss parent. Death entry
/// clears the target and slot, credits rewards and reports part loss. The next
/// 32 active updates cycle over nine local hit-effect offsets, with a random
/// drifting sprite on odd counts. Count 8 replaces its texture region; count
/// 31 releases the effect reservation and stops the death sound. The following
/// update advances the task to exit.
static void _actor503500SmallOrbEmitterStepDying(Task* task)
{
    enum {
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_BEGIN        = 0,
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_EFFECTS      = 1,
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_EFFECT_COST  = 3,
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_HIT_PUFF_ARG = 0x01001C00,
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_DRIFT_ARG    = 0x04404600,
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_SOUND_FADE   = 45, // Audio updates; integer fade rounding may extend this
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_REPAINT_X    = 321,
        ACTOR_503500_SMALL_ORB_EMITTER_DEATH_REPAINT_Y    = 298,
    };
    Enemy*                           enemy;
    _Actor503500SmallOrbEmitterWork* work;
    GfxCoord*                        coord;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_SMALL_ORB_EMITTER_DEATH_BEGIN:
            // Remove the target before reporting the lost boss part.
            _actor503500SmallOrbEmitterBeginDeath(task, work, enemy, coord);
            break;
        case ACTOR_503500_SMALL_ORB_EMITTER_DEATH_EFFECTS:
            // Shed local effects while replacing the defeated emitter's texture.
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_SMALL_ORB_EMITTER_DEATH_EFFECT_COST) != 0) {
                effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_SMALL_ORB_EMITTER_DEATH_HIT_PUFF_ARG,
                            &D_actor_503500_8016F31C[work->stateFrames % (s32)ARRAY_SIZE(D_actor_503500_8016F31C)]);
                if (work->stateFrames & 1) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, ACTOR_503500_SMALL_ORB_EMITTER_DEATH_DRIFT_ARG,
                                &D_actor_503500_8016F31C[(u16)((gRandomLcgState >> 16) % ARRAY_SIZE(D_actor_503500_8016F31C))]);
                }
            }
            if (work->stateFrames == ACTOR_503500_SMALL_ORB_EMITTER_DYING_REPAINT_FRAME) {
                MoveImage(&D_actor_503500_8016F364, ACTOR_503500_SMALL_ORB_EMITTER_DEATH_REPAINT_X, ACTOR_503500_SMALL_ORB_EMITTER_DEATH_REPAINT_Y);
            }
            if (work->stateFrames++ >= ACTOR_503500_SMALL_ORB_EMITTER_DYING_FRAMES) {
                actor503500ReleaseSlotEffects(task->spawnArg1.value);
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, ACTOR_503500_SMALL_ORB_EMITTER_DEATH_SOUND_FADE);
                work->stateStep++;
            }
            break;
        default:
            task->state++;
            break;
    }
}

/// The small-orb emitter's per-frame tick, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
static void func_actor_503500_8013E9A4(Task* arg0)
{
    Enemy*    enemy;
    GfxCoord* coord;

    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500SmallOrbEmitterClearReactions(arg0);
    }
    _actor503500SmallOrbEmitterUpdateHits(arg0);
    func_actor_503500_8013EB60(arg0);
}

/// Detaches the small-orb emitter and releases its collision body and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500SmallOrbEmitterExit(Task* task)
{
    Enemy*                           enemy;
    _Actor503500SmallOrbEmitterWork* work;

    enemy                           = task->spawnArg2.pointer;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Discards the small-orb emitter's stagger, buildup and damage-over-time reactions.
///
/// Requires its live enemy in `spawnArg2.pointer`. Retains other reaction bits,
/// health and behavior state, including the damage-over-time counters.
static void _actor503500SmallOrbEmitterClearReactions(Task* task)
{
    Enemy* enemy;

    enemy = task->spawnArg2.pointer;
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ~ENEMY_REACTION_STAGGER;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags &= ~ENEMY_REACTION_DAMAGE_OVER_TIME_BITS;
    }
}

/// Ages the small-orb emitter's hit cooldown, applies contacts and empties their table.
///
/// Requires initialized work and the marked eight-entry contact table. Cooldown
/// uses signed halfword frames and is decremented before hits are considered.
/// A defeated boss suppresses hits; contacts are cleared in either case.
static void _actor503500SmallOrbEmitterUpdateHits(Task* task)
{
    _Actor503500SmallOrbEmitterWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500SmallOrbEmitterApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

static void func_actor_503500_8013EB60(Task* arg0)
{
    _Actor503500SmallOrbEmitterWork* work = arg0->work;

    switch (work->state) {
        case ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE:
            _actor503500SmallOrbEmitterStepIdle(arg0);
            break;
        case ACTOR_503500_SMALL_ORB_EMITTER_STATE_ATTACK:
            func_actor_503500_8013E384(arg0);
            break;
        case ACTOR_503500_SMALL_ORB_EMITTER_STATE_DYING:
            _actor503500SmallOrbEmitterStepDying(arg0);
            break;
    }
}

/// Starts the small-orb emitter's attack when its idle task receives the boss's attack command.
///
/// Requires initialized emitter work. Consumes `ACTOR_503500_SLOT_COMMAND_ATTACK`
/// from `killCountdown`; other requests remain pending.
static void _actor503500SmallOrbEmitterStepIdle(Task* task)
{
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500SmallOrbEmitterEnterState(task, ACTOR_503500_SMALL_ORB_EMITTER_STATE_ATTACK);
        task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    }
}

/// Starts a small-orb emitter state and reports its availability to the boss.
///
/// `state` is an `ACTOR_503500_SMALL_ORB_EMITTER_STATE_*` value. Restarts
/// the state step and frame count, consumes any pending boss command, and
/// marks the slot busy unless the new state is idle.
static void _actor503500SmallOrbEmitterEnterState(Task* task, s32 state)
{
    _Actor503500SmallOrbEmitterWork* work = task->work;

    work->state         = state;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE);
}

void actor503500SmallOrbEmitterTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_80132098;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500YellowFlashEmitterTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_801320D0 = {
    {
        _actor503500YellowFlashEmitterInit,
        func_actor_503500_8013F6F0,
        _actor503500YellowFlashEmitterExit,
    },
};

/// Initializes the dormant yellow-flash emitter on the boss's flash mount.
///
/// Requires slot 12 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows singleton work and boss part 8. The target
/// node is linked but not lockable, and its sphere's pair tests remain off
/// until the boss commands exposure.
static void _actor503500YellowFlashEmitterInit(Task* task)
{
    enum { ACTOR_503500_YELLOW_FLASH_EMITTER_BOSS_PART = 8 };

    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    WorldCollisionContact* contacts;

    coord  = task->extra.tmd->coords;
    enemy  = task->spawnArg2.pointer;
    parent = task->parent;
    memFillBytes(&D_actor_503500_80177A6C, 0, sizeof(D_actor_503500_80177A6C));
    task->work = &D_actor_503500_80177A6C;

    coord->parent = &parent->extra.tmd->coords[ACTOR_503500_YELLOW_FLASH_EMITTER_BOSS_PART];
    gfxSetRotIdentity(&coord->coord);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                   = coord;
    enemy->node.state.parts.flags |= (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    enemy->bodyPos.vx              = D_actor_503500_8016F36C.vx;
    enemy->bodyPos.vy              = D_actor_503500_8016F36C.vy;
    enemy->bodyPos.vz              = D_actor_503500_8016F36C.vz;
    contacts                       = D_actor_503500_80177A6C.contacts;
    enemy->param                   = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                    = contacts;
    enemy->hp                      = enemy->param->hpMax;

    D_actor_503500_80177A6C.body.coord            = coord;
    D_actor_503500_80177A6C.body.context.contacts = contacts;
    D_actor_503500_80177A6C.body.pos.vx           = D_actor_503500_8016F36C.vx;
    D_actor_503500_80177A6C.body.pos.vy           = D_actor_503500_8016F36C.vy;
    D_actor_503500_80177A6C.body.pos.vz           = D_actor_503500_8016F36C.vz;
    D_actor_503500_80177A6C.body.key              = ACTOR_503500_PART_BODY_KEY;
    D_actor_503500_80177A6C.body.radius           = 800;
    D_actor_503500_80177A6C.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_503500_80177A6C.body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(D_actor_503500_80177A6C.contacts), 0);
    D_actor_503500_80177A6C.hitEffect.spawnArgLo = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    D_actor_503500_80177A6C.hitEffect.coord      = coord;
    D_actor_503500_80177A6C.hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    D_actor_503500_80177A6C.body.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    _actor503500YellowFlashEmitterEnterState(task, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DORMANT);
    task->exitCallback = _actor503500YellowFlashEmitterExit;
    task->state       += 1;
}

/// Applies one eligible attack contact to the yellow-flash emitter.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// Fatal hits start its dying state before applying status reactions.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 400 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500YellowFlashEmitterHandleHit(Task* task, _Actor503500YellowFlashEmitterWork* work, Enemy* enemy, GfxCoord* coord,
                                                           const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// Adds the part's fixed local sphere centre.
#define ACTOR_503500_YELLOW_FLASH_EMITTER_PROJECT_HIT_POINT()                                                                                                                                                     \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                         \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                                      \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                                      \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                                      \
    radialScale       = (ACTOR_503500_YELLOW_FLASH_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                                    \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                                    \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                                    \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                           \
    gte_ldv0(&effectPosition);                                                                                                                                                                                    \
    gte_rtv0();                                                                                                                                                                                                   \
    gte_stsv(&effectPosition);                                                                                                                                                                                    \
    effectPosition.vx += D_actor_503500_8016F36C.vx;                                                                                                                                                              \
    effectPosition.vy += D_actor_503500_8016F36C.vy;                                                                                                                                                              \
    effectPosition.vz += D_actor_503500_8016F36C.vz;
    enum { ACTOR_503500_YELLOW_FLASH_HIT_EFFECT_RADIUS = 400 };
    VECTOR    attackerOffset;
    SVECTOR   effectPosition;
    MATRIX    worldRotation;
    MATRIX    inverseRotation;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       criticalHit;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    attackerCoord = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    criticalHit       = 0;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage     *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        criticalHit = 1;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        _actor503500YellowFlashEmitterEnterState(task, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DYING);
    }
    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

        case DAMAGE_PLAYER_REACTION_NONE:
        case 4:
        case 5:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
        case 8:
        case 9:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            damageStartEnemyStagger(enemy);
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            damageStartEnemyBuildup(enemy, attackKey, 0);
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
            break;
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_YELLOW_FLASH_EMITTER_PROJECT_HIT_POINT();
#undef ACTOR_503500_YELLOW_FLASH_EMITTER_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to the yellow-flash emitter.
///
/// Requires initialized work, its live enemy/coordinate and `contactCount`
/// readable elements. Earlier equal keys suppress duplicates; only attack
/// contacts land with a clear cooldown. Fatal hits start the dying state
/// before status reactions are applied. Contacts are borrowed and unchanged.
/// `unusedBody` retains the collision-pass interface and is ignored.
static void _actor503500YellowFlashEmitterApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500YellowFlashEmitterWork* work;
    Enemy*                              enemy;
    GfxCoord*                           coord;
    s32                                 contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500YellowFlashEmitterHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// `ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_ATTACK` step of the yellow-flash
/// emitter, stepped by `stateStep`: applies the boss's animation preset 0xC,
/// counts `ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_FRAMES` in `stateFrames`
/// (spawning table entry 3 on frame
/// `ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_LAUNCH_FRAME` with its coordinate
/// parented to this task's), applies preset 0xD once the boss reports preset
/// 0xC done, and idles when 0xD is done too.
static void func_actor_503500_8013F328(Task* arg0)
{
    _Actor503500YellowFlashEmitterWork* work = arg0->work;
    Task*                               task;
    GfxCoord*                           coord;

    if (actor503500ShouldInterruptAttack(arg0->parent) != 0) {
        _actor503500YellowFlashEmitterEnterState(arg0, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0->parent, 0xC, 8);
            work->stateStep++;
            break;
        case 1:
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_FRAMES) {
                work->stateFrames = 0;
                work->stateStep++;
            } else if (work->stateFrames == ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_LAUNCH_FRAME) {
                task = taskSpawnFromTable(D_actor_503500_8016E9F0, 3, 0, 0);
                if (task != NULL) {
                    coord             = task->extra.tmd->coords;
                    coord->parent     = arg0->extra.tmd->coords;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                }
            }
            break;
        case 2:
            if (actor503500HasAnimationFinished(arg0->parent, 0xC) != 0) {
                actor503500PlayAnimationPreset(arg0->parent, 0xD, 0x10);
                work->stateStep++;
            }
            break;
        case 3:
            if (actor503500HasAnimationFinished(arg0->parent, 0xD) != 0) {
                _actor503500YellowFlashEmitterEnterState(arg0, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// Removes the dying part's target, credits rewards and reports its lost boss slot.
///
/// Requires live initialized task, work, enemy and coordinate. Stops pair tests
/// and detaches borrowed contacts before notifying the boss. Starts the death
/// sound and advances the state step; collision storage stays linked until exit.
static inline void _actor503500YellowFlashEmitterBeginDeath(Task* task, _Actor503500YellowFlashEmitterWork* work, Enemy* enemy, GfxCoord* coord)
{
    s32 audioPan;

    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->recs       = NULL;
    worldTargetUnlinkNode(&enemy->node);
    actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
    work->hitCooldown = 0;
    sceneAcquireBattleRef(0);
    sceneReleaseBattleRefWithRewards(task, 0);
    actor503500EnterPartLostState(task->parent);
    enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
    audioPan              = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, audioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    // This death-only halfword has no established reader or meaning.
    work->field_EC = 2;
    work->stateStep++;
}

/// Removes the defeated yellow-flash emitter and runs its effects and texture transition.
///
/// Requires initialized work, its live enemy and boss parent. Death entry
/// clears the target and slot, credits rewards and reports part loss. The next
/// 31 active updates cycle over six local hit-effect offsets; odd counts add
/// a drifting sprite from the final three offsets. Replaces the texture,
/// stops the death sound and releases the effect reservation before the next
/// update advances the task to exit. The death-only halfword's role is unproven.
static void _actor503500YellowFlashEmitterStepDying(Task* task)
{
    enum {
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_BEGIN              = 0,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_EFFECTS            = 1,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_EFFECT_COST        = 3,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_HIT_PUFF_ARG       = 0x01001A00,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_DRIFT_ARG          = 0x04404600,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_DRIFT_OFFSET_FIRST = 3,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_DRIFT_OFFSET_COUNT = 3,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_SOUND_FADE         = 45, // Audio updates; integer fade rounding may extend this
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_REPAINT_X          = 320,
        ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_REPAINT_Y          = 256,
    };
    Enemy*                              enemy;
    _Actor503500YellowFlashEmitterWork* work;
    GfxCoord*                           coord;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_BEGIN:
            // Remove the target before reporting the lost boss part.
            _actor503500YellowFlashEmitterBeginDeath(task, work, enemy, coord);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_EFFECTS:
            // Shed local effects while replacing the defeated emitter's texture.
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_EFFECT_COST) != 0) {
                effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_HIT_PUFF_ARG,
                            &D_actor_503500_8016F374[work->stateFrames % (s32)ARRAY_SIZE(D_actor_503500_8016F374)]);
                if (work->stateFrames & 1) {
                    effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_DRIFT_ARG,
                                &D_actor_503500_8016F374[work->stateFrames % ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_DRIFT_OFFSET_COUNT + ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_DRIFT_OFFSET_FIRST]);
                }
            }
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_YELLOW_FLASH_EMITTER_DYING_FRAMES) {
                MoveImage(&D_actor_503500_8016F3A4, ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_REPAINT_X, ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_REPAINT_Y);
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, ACTOR_503500_YELLOW_FLASH_EMITTER_DEATH_SOUND_FADE);
                actor503500ReleaseSlotEffects(task->spawnArg1.value);
                work->stateStep++;
            }
            break;
        default:
            task->state++;
            break;
    }
}

/// The yellow-flash emitter's per-frame tick, the same shape as
/// `func_actor_503500_8013E9A4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
static void func_actor_503500_8013F6F0(Task* arg0)
{
    Enemy*    enemy;
    GfxCoord* coord;

    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500YellowFlashEmitterClearReactions(arg0);
    }
    func_actor_503500_8013F830(arg0);
    func_actor_503500_8013F8AC(arg0);
}

/// Detaches the yellow-flash emitter and releases its collision body and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500YellowFlashEmitterExit(Task* task)
{
    Enemy*                              enemy;
    _Actor503500YellowFlashEmitterWork* work;

    enemy                           = task->spawnArg2.pointer;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Discards the yellow-flash emitter's stagger, buildup and damage-over-time reactions.
///
/// Requires its live enemy in `spawnArg2.pointer`. Retains other reaction bits,
/// health and behavior state, including the damage-over-time counters.
static void _actor503500YellowFlashEmitterClearReactions(Task* task)
{
    Enemy* enemy;

    enemy = task->spawnArg2.pointer;
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ~ENEMY_REACTION_STAGGER;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags &= ~ENEMY_REACTION_DAMAGE_OVER_TIME_BITS;
    }
}

static void func_actor_503500_8013F830(Task* arg0)
{
    _Actor503500YellowFlashEmitterWork* work;
    s16                                 timer;

    work = arg0->work;
    if (work->hitCooldown != 0) {
        timer             = (u16)work->hitCooldown - 1;
        work->hitCooldown = timer;
        if (timer < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500YellowFlashEmitterApplyHits(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

static void func_actor_503500_8013F8AC(Task* arg0)
{
    _Actor503500YellowFlashEmitterWork* work = arg0->work;

    switch (work->state) {
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE:
            _actor503500YellowFlashEmitterStepIdle(arg0);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_ATTACK:
            func_actor_503500_8013F328(arg0);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DYING:
            _actor503500YellowFlashEmitterStepDying(arg0);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DORMANT:
            _actor503500YellowFlashEmitterStepDormant(arg0);
            break;
    }
}

/// Starts the yellow-flash emitter's attack when its idle task receives the boss's attack command.
///
/// Requires initialized emitter work. Consumes `ACTOR_503500_SLOT_COMMAND_ATTACK`
/// from `killCountdown`; other requests remain pending.
static void _actor503500YellowFlashEmitterStepIdle(Task* task)
{
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500YellowFlashEmitterEnterState(task, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_ATTACK);
        task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    }
}

/// Makes the dormant yellow-flash emitter targetable on the boss's expose command.
///
/// Requires initialized emitter work. `ACTOR_503500_SLOT_COMMAND_BECOME_TARGET`
/// enters idle, consumes the request and enables the target sphere's pair tests.
/// The target node is already linked at set-up; other requests remain pending.
static void _actor503500YellowFlashEmitterStepDormant(Task* task)
{
    _Actor503500YellowFlashEmitterWork* work;

    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_BECOME_TARGET) {
        _actor503500YellowFlashEmitterEnterState(task, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE);
        work              = task->work;
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Starts a yellow-flash emitter state and reports its availability to the boss.
///
/// `state` is an `ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_*` value. Restarts
/// the state step and frame count, consumes any pending boss command, and
/// marks the slot busy unless idle, including while dormant.
static void _actor503500YellowFlashEmitterEnterState(Task* task, s32 state)
{
    _Actor503500YellowFlashEmitterWork* work = task->work;

    work->state         = state;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    work->field_EC      = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE);
}

void actor503500YellowFlashEmitterTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_801320D0;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500LungingChainTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132108 = {
    {
        func_actor_503500_8013FA74,
        func_actor_503500_8013FF0C,
        _actor503500LungingChainExit,
    },
};

static void func_actor_503500_8013FA74(Task* arg0)
{
    Enemy*                        enemy;
    TmdObject*                    tmd;
    GfxCoord*                     coord;
    GfxCoord*                     part;
    _Actor503500LungingChainWork* work;
    WorldCollisionContact*        rec;
    WorldCollisionContact*        rec2;
    MATRIX                        m;
    s32                           idx;
    s32                           i;

    idx   = arg0->spawnArg1.value - 0xD;
    enemy = arg0->spawnArg2.pointer;
    work  = &D_actor_503500_80177B60[idx];
    coord = arg0->extra.tmd->coords;
    tmd   = arg0->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work = work;

    coord->parent     = &arg0->parent->extra.tmd->coords[1];
    coord->coord.t[0] = D_actor_503500_8016F3AC[idx].vx;
    coord->coord.t[1] = D_actor_503500_8016F3AC[idx].vy;
    coord->coord.t[2] = D_actor_503500_8016F3AC[idx].vz;
    gfxSetRotIdentity(&m);
    RotMatrix(&D_actor_503500_8016F3CC[idx], &m);
    MulMatrix0(&coord->coord, &m, &coord->coord);
    coord->composeStamp       = GRAPHICS_COORD_DIRTY;
    work->bufferFreeCountdown = -1;
    tmd->lightMtx             = &work->lightMtx;
    tmd->colorMtx             = &work->colorMtx;
    tmd->otOffset             = 0x12;

    enemy->field_4                = &coord->coord;
    part                          = &coord[ACTOR_503500_LUNGING_CHAIN_TIP_PART];
    enemy->field_48               = 0;
    enemy->coord                  = part;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F3EC.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F3EC.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F3EC.vz;
    rec                           = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                   = rec;

    work->body.coord            = part;
    work->body.context.contacts = rec;
    work->body.pos.vx           = D_actor_503500_8016F3EC.vx;
    work->body.pos.vy           = D_actor_503500_8016F3EC.vy;
    work->body.pos.vz           = D_actor_503500_8016F3EC.vz;
    work->body.key              = 0x30023;
    work->body.radius           = 0x258;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(rec, ARRAY_SIZE(work->contacts), 0);
    rec2                              = work->attackContacts;
    work->attackBody.coord            = part;
    work->attackBody.context.contacts = rec2;
    work->body.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.pos.vx           = D_actor_503500_8016F3F4[idx].vx;
    work->attackBody.pos.vy           = D_actor_503500_8016F3F4[idx].vy;
    work->attackBody.pos.vz           = D_actor_503500_8016F3F4[idx].vz;
    work->attackBody.key              = damagePackAttackKey(enemy->param->attacks, 0);
    work->attackBody.radius           = 0x1F4;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(rec2, ARRAY_SIZE(work->attackContacts), 0);
    work->hitEffect.spawnArgLo = 0x600;
    work->hitEffect.coord      = part;
    work->hitEffect.spawnArgHi = 3;
    work->attackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    copyVector(&work->tipTarget, &D_actor_503500_8016F414[arg0->spawnArg1.value - 0xD]);
    copyVector(&work->tipPosition, &D_actor_503500_8016F414[arg0->spawnArg1.value - 0xD]);
    work->tipSpeedLimit.word = 0x600000;
    work->swayAmplitude      = 0x40;
    work->swayWeight         = 0x1000;
    work->tipAdvancing       = 1;
    for (i = 1; i < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; i++) {
        work->swayPhase[i]  = (i << 9) & 0xFFF;
        work->blendStart[i] = coord[i].coord;
    }
    actorRenderComposeCoord(coord);
    func_actor_503500_801421A8(arg0);
    switch (arg0->killCountdown) {
        case ACTOR_503500_SLOT_COMMAND_BECOME_TARGET:
            _actor503500LungingChainEnterState(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING);
            break;
        case ACTOR_503500_SLOT_COMMAND_REGROW:
            tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor503500LungingChainEnterState(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING);
            break;
        default:
            worldTargetLinkNode(&enemy->node);
            enemy->hp         = D_actor_503500_8016E7EC[arg0->spawnArg1.value].hpMax;
            work->blendWeight = 0x1000;
            work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            _actor503500LungingChainEnterState(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            break;
    }
    arg0->exitCallback = _actor503500LungingChainExit;
    arg0->state       += 1;
}

static void func_actor_503500_8013FF0C(Task* arg0)
{
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    TmdObject*                    tmd;
    s8                            countdown;

    work      = arg0->work;
    enemy     = arg0->spawnArg2.pointer;
    countdown = work->bufferFreeCountdown;
    tmd       = arg0->extra.tmd;
    if (countdown >= 0) {
        if (countdown == 0) {
            tmdFreePrimitiveBuffer(tmd);
        }
        work->bufferFreeCountdown--;
    }
    if (gGameSession->eventState != 0 &&
        actor503500IsSlotEmpty(arg0->parent, arg0->spawnArg1.value < 0xF ? 0xA : 0xB) == 0) {
        tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        actor503500SyncAttachedModelDrawState(arg0, &work->bufferFreeCountdown);
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (!(tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                func_actor_503500_801421A8(arg0);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                _actor503500LungingChainReactToDamage(arg0);
            }
            func_actor_503500_801420C4(arg0);
            func_actor_503500_80141D7C(arg0);
            if (work->detached == 0) {
                _actor503500LungingChainStepTip(arg0);
                _actor503500LungingChainUpdatePose(arg0);
            }
            func_actor_503500_801421A8(arg0);
            _actor503500LungingChainBlendPose(arg0);
            break;
    }
}

/// Winds a lunging chain above the player's latched position, throws its tip and recovers.
///
/// Requires initialized work, a live nine-part model in slot 13..16 and its
/// boss parent. World positions use integer game units; the target is converted
/// into the root parent's frame. Weights use 4096 for 1.0 and tip speed is 16.16.
/// The throw enables the attack sphere until arrival or interruption. Wind-up
/// expires once its counter exceeds 120 or outside the slot's allowed bearing
/// window; throw and recovery each wait until their counters exceed 30.
static void _actor503500LungingChainStepLunge(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_LUNGE_AIM          = 0,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_WIND_UP      = 1,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW        = 2,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_WAIT_FOR_TIP = 3,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_RECOVER      = 4,
    };
    enum {
        ACTOR_503500_LUNGING_CHAIN_LUNGE_WINDUP_HEIGHT         = 5000,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_TARGET_Y_OFFSET       = 500,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_ANIMATION_PRESET      = 18,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_TIP_SPEED             = 1024 * 0x10000,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_MAX_CURL              = 2 * ONE,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_WINDUP_CURL_STEP      = 0x88,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW_CURL_STEP       = 0x200,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_LANDING_CURL_STEP     = 0x2AA,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_RECOVERY_CURL_STEP    = 0x400,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_SWAY_STEP             = 0x80,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_RELEASE_SWAY_PHASE    = 2000,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW_FRAMES          = 30,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_RECOVERY_FRAMES       = 30,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_WINDUP_TIMEOUT_FRAMES = 120,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING         = 1900,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING         = 1000,
        ACTOR_503500_LUNGING_CHAIN_LUNGE_SPLIT_BEARING         = 1700,
    };

    SVECTOR                       targetOffset;
    SVECTOR                       parentWorldPosition;
    MATRIX                        parentWorldRotation;
    MATRIX                        worldToParentRotation;
    _Actor503500LungingChainWork* work;
    GfxCoord*                     rootCoord;
    s32                           withinAttackArc;
    s32                           playerBearing;

    // Convert the latched world target into the root parent's frame, after a
    // world-Y offset. Captures work, rootCoord and the four matrix/vector locals;
    // worldYOffset is evaluated once. Inputs narrow to signed halfwords before
    // inverse rotation. Writes tipTarget's XYZ and changes GTE state. This
    // binding exists only in the lunge step. This is a statement sequence;
    // never use it as an unbraced if/loop body.
#define ACTOR_503500_LUNGING_CHAIN_AIM_AT_WORLD_TARGET(worldYOffset)                             \
    gfxComposeNodeWorldTransform(rootCoord->parent, &parentWorldRotation, &parentWorldPosition); \
    targetOffset.vx = work->lungeTarget.vx - parentWorldPosition.vx;                             \
    targetOffset.vy = work->lungeTarget.vy - parentWorldPosition.vy + (worldYOffset);            \
    targetOffset.vz = work->lungeTarget.vz - parentWorldPosition.vz;                             \
    gte_TransposeMatrix(&parentWorldRotation, &worldToParentRotation);                           \
    gte_SetRotMatrix(&worldToParentRotation);                                                    \
    gte_ldv0(&targetOffset);                                                                     \
    gte_rtv0();                                                                                  \
    gte_stsv(&work->tipTarget)

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (actor503500ShouldInterruptAttack(task->parent) != 0) {
        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
        actor503500ReleaseSlotEffects(task->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_LUNGING_CHAIN_LUNGE_AIM:
            // Latch the player in world space, then aim above that point in the root parent's frame.
            work->lungeTarget.vx = gPlayerStatus.coordMtx->t[0];
            work->lungeTarget.vy = gPlayerStatus.coordMtx->t[1];
            work->lungeTarget.vz = gPlayerStatus.coordMtx->t[2];
            ACTOR_503500_LUNGING_CHAIN_AIM_AT_WORLD_TARGET(-ACTOR_503500_LUNGING_CHAIN_LUNGE_WINDUP_HEIGHT);
            actor503500PlayAnimationPreset(task->parent, ACTOR_503500_LUNGING_CHAIN_LUNGE_ANIMATION_PRESET, ANIMATION_RATE_ONE);
            work->stateStep++;
            break;
        case ACTOR_503500_LUNGING_CHAIN_LUNGE_WIND_UP:
            work->curlWeight += ACTOR_503500_LUNGING_CHAIN_LUNGE_WINDUP_CURL_STEP;
            if (work->curlWeight > ACTOR_503500_LUNGING_CHAIN_LUNGE_MAX_CURL) {
                work->curlWeight = ACTOR_503500_LUNGING_CHAIN_LUNGE_MAX_CURL;
            }
            if (work->tipArrived != 0 && work->swayPhase[ACTOR_503500_LUNGING_CHAIN_TIP_PART] > ACTOR_503500_LUNGING_CHAIN_LUNGE_RELEASE_SWAY_PHASE) {
                // Release toward the latched point when the tip and sway phase are ready.
                ACTOR_503500_LUNGING_CHAIN_AIM_AT_WORLD_TARGET(ACTOR_503500_LUNGING_CHAIN_LUNGE_TARGET_Y_OFFSET);
                work->tipSpeedLimit.word = ACTOR_503500_LUNGING_CHAIN_LUNGE_TIP_SPEED;
                work->attackBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW:
            work->curlWeight -= ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW_CURL_STEP;
            if (work->curlWeight < 0) {
                work->curlWeight = 0;
            }
            work->swayWeight -= ACTOR_503500_LUNGING_CHAIN_LUNGE_SWAY_STEP;
            if (work->swayWeight < 0) {
                work->swayWeight = 0;
            }
            work->stepFrames++;
            if (work->stepFrames > ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW_FRAMES) {
                work->stepFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_LUNGE_WAIT_FOR_TIP:
            work->curlWeight -= ACTOR_503500_LUNGING_CHAIN_LUNGE_LANDING_CURL_STEP;
            if (work->curlWeight < 0) {
                work->curlWeight = 0;
            }
            if (work->tipArrived != 0) {
                work->stepFrames        = 0;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_LUNGE_RECOVER:
            work->curlWeight -= ACTOR_503500_LUNGING_CHAIN_LUNGE_RECOVERY_CURL_STEP;
            if (work->curlWeight < 0) {
                work->curlWeight = 0;
            }
            work->stepFrames++;
            if (work->stepFrames > ACTOR_503500_LUNGING_CHAIN_LUNGE_RECOVERY_FRAMES) {
                _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            }
            break;
    }
    // Wind-up is allowed only in this slot's bearing window; a missing partner widens it.
    withinAttackArc = 0;
    playerBearing   = actor503500GetPlayerBearing();
    switch (task->spawnArg1.value) {
        case ACTOR_503500_SLOT_LUNGING_CHAIN_0:
            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_1) != 0) {
                if (playerBearing < -ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING || playerBearing > ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING) {
                    withinAttackArc = 1;
                }
            } else if (playerBearing > ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING && playerBearing < ACTOR_503500_LUNGING_CHAIN_LUNGE_SPLIT_BEARING) {
                withinAttackArc = 1;
            }
            break;
        case ACTOR_503500_SLOT_LUNGING_CHAIN_1:
            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_0) != 0) {
                if (playerBearing < -ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING || playerBearing > ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING) {
                    withinAttackArc = 1;
                }
            } else if (playerBearing < -ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING || playerBearing > (ACTOR_503500_LUNGING_CHAIN_LUNGE_SPLIT_BEARING - 1)) {
                withinAttackArc = 1;
            }
            break;
        case ACTOR_503500_SLOT_LUNGING_CHAIN_2:
            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_3) != 0) {
                if (playerBearing < -ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING || playerBearing > ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING) {
                    withinAttackArc = 1;
                }
            } else if (playerBearing < (1 - ACTOR_503500_LUNGING_CHAIN_LUNGE_SPLIT_BEARING) || playerBearing > ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING) {
                withinAttackArc = 1;
            }
            break;
        case ACTOR_503500_SLOT_LUNGING_CHAIN_3:
            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_2) != 0) {
                if (playerBearing < -ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING || playerBearing > ACTOR_503500_LUNGING_CHAIN_LUNGE_OUTER_BEARING) {
                    withinAttackArc = 1;
                }
            } else if (playerBearing < -ACTOR_503500_LUNGING_CHAIN_LUNGE_INNER_BEARING) {
                if (playerBearing >= (1 - ACTOR_503500_LUNGING_CHAIN_LUNGE_SPLIT_BEARING)) {
                    withinAttackArc = 1;
                }
            }
            break;
    }
    work->stateFrames++;
    if (work->stateStep < ACTOR_503500_LUNGING_CHAIN_LUNGE_THROW && (work->stateFrames > ACTOR_503500_LUNGING_CHAIN_LUNGE_WINDUP_TIMEOUT_FRAMES || withinAttackArc == 0)) {
        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
    }
#undef ACTOR_503500_LUNGING_CHAIN_AIM_AT_WORLD_TARGET
}

/// Death state of the lunging chains, the same body as
/// `_actor503500LargeChainStepDying` at this block's offsets: unlinks the enemy
/// node, waits for `tipArrived`, re-parents the root coordinate onto the view
/// and plays 0x40230004 at it. Phase 2 eases every part back to rest while the
/// body rises; past 1000 the pose is saved in `unscaledRootMatrix` and phase 3
/// squashes it vertically (`collapseScaleY`), firing the cues on frames
/// 10/15/30/40.
/// Every twelfth frame of phases 0..2 sprays effects along parts 8..1.
static void func_actor_503500_80140654(Task* arg0)
{
    MATRIX                        m;
    VECTOR                        scale;
    SVECTOR                       rot;
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    GfxCoord*                     part;
    s16*                          p;
    s32                           phase;
    s32                           i;
    s32                           j;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    phase = work->stateStep;
    coord = arg0->extra.tmd->coords;
    switch (phase) {
        case 0:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs       = 0;
            worldTargetUnlinkNode(&enemy->node);
            actor503500ClearSlotEnemy(arg0->parent, arg0->spawnArg1.value);
            work->hitCooldown = 0;
            (sceneAcquireBattleRef)(0);
            sceneReleaseBattleRefWithRewards(arg0, 0);
            actor503500EnterPartLostState(arg0->parent);
            enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            work->tipTarget.vy    = 0x7D0;
            work->stateStep++;
            break;
        case 1:
            if (work->tipArrived != 0) {
                gfxComposeNodeWorldTransform(coord, &m, &rot);
                coord->coord        = m;
                coord->coord.t[0]   = rot.vx;
                coord->coord.t[1]   = rot.vy;
                coord->coord.t[2]   = rot.vz;
                coord->parent       = &gGfxViewCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->detached      = phase;
                actorRenderComposeCoord(coord);
                sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->stateStep++;
            }
            break;
        case 2:
            for (i = 1; i < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; i++) {
                part = &coord[i];
                gfxExtractSmallestEuler(&rot, &part->coord);
                p = &rot.vx;
                j = 1;
                do {
                    if (*p > 0) {
                        *p -= 2;
                        if (*p < 0) {
                            *p = 0;
                        }
                    } else {
                        *p += 2;
                        if (*p > 0) {
                            *p = 0;
                        }
                    }
                    p++;
                } while (j++ < 3);
                gfxSetRotIdentity(&coord[i].coord);
                RotMatrix(&rot, &part->coord);
                part->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += 10;
            if (coord->coord.t[1] > 1000) {
                work->unscaledRootMatrix = coord->coord;
                work->collapseScaleY     = 0x1000;
                work->stateStep++;
            }
            break;
        case 3:
            if (work->collapseScaleY > 0x200) {
                work->collapseScaleY -= 0x20;
            }
            coord->coord = work->unscaledRootMatrix;
            scale.vx     = 0x1000;
            scale.vy     = work->collapseScaleY;
            scale.vz     = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            switch (work->stateFrames) {
                case 10:
                    arg0->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(SOUND_COMMON(0x0D), (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case 15:
                    effectSpawn(EFFECT_CORPSE_BURN, coord, 1, NULL);
                    break;
                case 30:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 40:
                    sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    arg0->state++;
                    break;
            }
            work->stateFrames++;
            break;
    }
    if (actor503500TryReserveSlotEffects(arg0->spawnArg1.value, 4) != 0 && work->stateStep < 3 &&
        gDisplayState.animFrame % 12 == 0) {
        for (i = ACTOR_503500_LUNGING_CHAIN_TIP_PART, j = 0; i > 0; i--) {
            effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[i], 0xB0008600, &D_actor_503500_8016F448[j]);
            j++;
            j = (j < 3) ? j : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep > 0) {
        sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        arg0->state = 2;
    }
}

/// Handles a lunging chain's stagger and damage-over-time reactions while discarding buildup.
///
/// Requires live initialized work and enemy parameters. Does nothing after boss
/// defeat or during an event. Stagger resumes idle, writes a five-frame hold
/// count and slows tip movement; damage-over-time pulses reduce HP, show damage
/// and slow the tip. Between pulses the chain remains in the damage-over-time
/// state; expiry resumes idle and a lethal pulse starts dying. State changes
/// disable its attack sphere. The dispatcher decrements the slow counter later
/// in the same update; the stagger's hold count does not enter the hold state.
static void _actor503500LungingChainReactToDamage(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_STAGGER_HOLD_FRAMES  = 5,
        ACTOR_503500_LUNGING_CHAIN_REACTION_SLOW_FRAMES = 8,
    };

    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    s32                           damage;
    u8                            reactionFlags;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if ((actor503500IsDefeated() == 0) && (gGameSession->eventState == 0)) {
        reactionFlags = enemy->reactionFlags;
        if (reactionFlags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
            _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            work->holdFrames = ACTOR_503500_LUNGING_CHAIN_STAGGER_HOLD_FRAMES;
            work->slowFrames = ACTOR_503500_LUNGING_CHAIN_REACTION_SLOW_FRAMES;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        }
        // Hold between damage pulses; a pulse resumes idle or starts dying.
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_DAMAGE_OVER_TIME);
            if (damageIsEnemyDamageOverTimeExpired(task->spawnArg2.pointer) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            } else {
                damage = damageTickEnemyDamageOverTime(enemy);
                if (damage != 0) {
                    enemy->hp -= damage;
                    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                    work->slowFrames = ACTOR_503500_LUNGING_CHAIN_REACTION_SLOW_FRAMES;
                    if (enemy->hp <= 0) {
                        enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_DYING);
                    } else {
                        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
                    }
                }
            }
        }
    }
}

/// Applies one eligible attack contact to a lunging chain.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// Fatal hits skip status reactions and start dying only before the unfolding states.
/// A surviving lunge returns to idle after the hit.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 500 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500LungingChainHandleHit(Task* task, _Actor503500LungingChainWork* work, Enemy* enemy, GfxCoord* coord,
                                                     const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// Adds the part's fixed local sphere centre.
#define ACTOR_503500_LUNGING_CHAIN_PROJECT_HIT_POINT()                                                                                                                                                             \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                          \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                                       \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                                       \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                                       \
    radialScale       = (ACTOR_503500_LUNGING_CHAIN_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                                     \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                                     \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                                     \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                            \
    gte_ldv0(&effectPosition);                                                                                                                                                                                     \
    gte_rtv0();                                                                                                                                                                                                    \
    gte_stsv(&effectPosition);                                                                                                                                                                                     \
    effectPosition.vx += D_actor_503500_8016F3EC.vx;                                                                                                                                                               \
    effectPosition.vy += D_actor_503500_8016F3EC.vy;                                                                                                                                                               \
    effectPosition.vz += D_actor_503500_8016F3EC.vz;
    enum { ACTOR_503500_LUNGING_CHAIN_HIT_EFFECT_RADIUS = 500 };
    SVECTOR   effectPosition;
    MATRIX    inverseRotation;
    MATRIX    worldRotation;
    VECTOR    attackerOffset;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       criticalHit;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerCoord     = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    criticalHit       = 0;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage     *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        criticalHit = 1;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        if (work->state < ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING) {
            _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_DYING);
        }
    } else {
        switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

            case DAMAGE_PLAYER_REACTION_NONE:
            case 4:
            case 5:
            case DAMAGE_PLAYER_REACTION_EXPLOSION:
            case DAMAGE_PLAYER_REACTION_INCENDIARY:
            case 8:
            case 9:
                break;
            case DAMAGE_PLAYER_REACTION_STAGGER:
                damageStartEnemyStagger(enemy);
                break;
            case DAMAGE_PLAYER_REACTION_BUILDUP:
                damageStartEnemyBuildup(enemy, attackKey, 0);
                break;
            case DAMAGE_PLAYER_REACTION_POISON:
                damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
                break;
        }
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_LUNGING_CHAIN_PROJECT_HIT_POINT();
#undef ACTOR_503500_LUNGING_CHAIN_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
    if (work->state == ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE) {
        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
    }
}

/// Applies this frame's hits from `body`'s collision records `arg2[0..arg3)`
/// to the lunging chain's enemy, like `_actor503500YellowFlashEmitterApplyHits`, but at model
/// part 8: each attack id is taken once, only type-2 ids land while the
/// `hitCooldown` countdown is clear, and a hit that empties `hp` enters
/// state 5 (unless already past it) instead of applying the id's status effect.
/// The hit effect is pulled to 500 units along the contact offset, and a hit
/// landing in state 1 drops back to state 0. `arg1` is passed but unused.
static void func_actor_503500_80140D38(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    s32                           i;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[ACTOR_503500_LUNGING_CHAIN_TIP_PART];
    for (i = 0; i < arg3; i++) {
        _actor503500LungingChainHandleHit(arg0, work, enemy, coord, arg2, i);
    }
}

/// Advances the lunging chain's tip toward its target in the root parent's frame.
///
/// Requires initialized work. Positions and offsets narrow to signed halfword
/// game units; speed and its limit use signed 16.16. A strict Manhattan-distance
/// test against the signed integer half of the limit snaps to the target and
/// retains speed. Otherwise speed changes by one thirty-second of the limit
/// and the normalized direction advances the tip, at quarter speed while slow.
/// Saves the previous position, updates arrival and leaves countdowns untouched.
static void _actor503500LungingChainStepTip(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_TIP_ACCELERATION_DIVISOR = 32,
        ACTOR_503500_LUNGING_CHAIN_DIRECTION_FRACTION_BITS  = 12,
        ACTOR_503500_LUNGING_CHAIN_POSITION_FRACTION_BITS   = 16,
    };

    SVECTOR                       targetOffset;
    SVECTOR                       direction;
    VECTOR                        positionStep;
    _Actor503500LungingChainWork* work;
    s32                           speedLimit;
    s32                           speed;

    work                         = task->work;
    work->previousTipPosition.vx = work->tipPosition.vx;
    work->previousTipPosition.vy = work->tipPosition.vy;
    work->previousTipPosition.vz = work->tipPosition.vz;
    targetOffset.vx              = work->tipTarget.vx - work->tipPosition.vx;
    targetOffset.vy              = work->tipTarget.vy - work->tipPosition.vy;
    targetOffset.vz              = work->tipTarget.vz - work->tipPosition.vz;
    if (ABS(targetOffset.vx) + ABS(targetOffset.vy) + ABS(targetOffset.vz) < work->tipSpeedLimit.halves.integer) {
        work->tipArrived     = 1;
        work->tipPosition.vx = work->tipTarget.vx;
        work->tipPosition.vy = work->tipTarget.vy;
        work->tipPosition.vz = work->tipTarget.vz;
        return;
    }
    speedLimit       = work->tipSpeedLimit.word;
    work->tipArrived = 0;
    if (work->tipAdvancing != 0) {
        speed = work->tipSpeed + speedLimit / ACTOR_503500_LUNGING_CHAIN_TIP_ACCELERATION_DIVISOR;
        if (speed > 0) {
            if (speed > speedLimit) {
                speed = speedLimit;
            }
        } else if (speed < -speedLimit) {
            speed = -speedLimit;
        }
    } else {
        speed = work->tipSpeed - speedLimit / ACTOR_503500_LUNGING_CHAIN_TIP_ACCELERATION_DIVISOR;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->tipSpeed = speed;
    // Reduce fixed-point speed before the Q12 product, then discard position fractions.
    VectorNormalSS(&targetOffset, &direction);
    if (work->slowFrames != 0) {
        speed >>= 2;
    }
    positionStep.vx       = direction.vx * (speed >> ACTOR_503500_LUNGING_CHAIN_DIRECTION_FRACTION_BITS);
    positionStep.vy       = direction.vy * (speed >> ACTOR_503500_LUNGING_CHAIN_DIRECTION_FRACTION_BITS);
    positionStep.vz       = direction.vz * (speed >> ACTOR_503500_LUNGING_CHAIN_DIRECTION_FRACTION_BITS);
    work->tipPosition.vx += positionStep.vx >> ACTOR_503500_LUNGING_CHAIN_POSITION_FRACTION_BITS;
    work->tipPosition.vy += positionStep.vy >> ACTOR_503500_LUNGING_CHAIN_POSITION_FRACTION_BITS;
    work->tipPosition.vz += positionStep.vz >> ACTOR_503500_LUNGING_CHAIN_POSITION_FRACTION_BITS;
}

/// Lays out the lunging chain along a world-space cubic and adds its sway and curl pitch.
///
/// Requires live initialized work and nine root-first model coordinates with
/// an orthonormal parent hierarchy. Control points use signed halfword game
/// units: the root, its rotated (1000, 0, 1000) tangent (X mirrored for slots
/// 15/16), then the tip twice. Nine samples run from approximately t=1/9 to
/// just below t=1; no t=0 sample is used. The segment and basis requirements of
/// `_actor503500LungingChainPlaceLinks` must hold. Pitch is in 4096ths of a turn
/// and weights use 4096 for 1.0. Advances sway phases 2..8, applies pitch only
/// to links 2..7, and leaves composition stamps to the caller. Changes GTE state.
static void _actor503500LungingChainUpdatePose(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_CURVE_ROOT_OFFSET         = 1000,
        ACTOR_503500_LUNGING_CHAIN_SWAY_PHASE_STEP           = 0x80,
        ACTOR_503500_LUNGING_CHAIN_POSE_WEIGHT_FRACTION_BITS = 12,
    };

    SVECTOR                       controlPoints[4];
    SVECTOR                       offset;
    SVECTOR                       tipWorldPosition;
    VECTOR                        samples[ACTOR_503500_LUNGING_CHAIN_PART_COUNT];
    MATRIX                        worldRotation;
    GfxCoord*                     rootCoord;
    _Actor503500LungingChainWork* work;
    s32                           linkIndex;
    s32                           swayPitch;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    // Build the cubic in world space: root, outward tangent, tip, tip.
    gfxComposeNodeWorldTransform(rootCoord, &worldRotation, &controlPoints[0]);
    work->linkPoints[0].vx = controlPoints[0].vx;
    work->linkPoints[0].vy = controlPoints[0].vy;
    work->linkPoints[0].vz = controlPoints[0].vz;
    if ((u32)(task->spawnArg1.value - ACTOR_503500_SLOT_LUNGING_CHAIN_2) < 2) {
        offset.vx = -ACTOR_503500_LUNGING_CHAIN_CURVE_ROOT_OFFSET;
        offset.vy = 0;
        offset.vz = ACTOR_503500_LUNGING_CHAIN_CURVE_ROOT_OFFSET;
    } else {
        offset.vx = ACTOR_503500_LUNGING_CHAIN_CURVE_ROOT_OFFSET;
        offset.vy = 0;
        offset.vz = ACTOR_503500_LUNGING_CHAIN_CURVE_ROOT_OFFSET;
    }
    gte_SetRotMatrix(&worldRotation);
    gte_ldv0(&offset);
    gte_rtv0();
    gte_stsv(&controlPoints[1]);
    controlPoints[1].vx += controlPoints[0].vx;
    controlPoints[1].vy += controlPoints[0].vy;
    controlPoints[1].vz += controlPoints[0].vz;
    gfxComposeNodeWorldTransform(rootCoord->parent, &worldRotation, &tipWorldPosition);
    gte_SetRotMatrix(&worldRotation);
    gte_ldv0(&work->tipPosition);
    gte_rtv0();
    gte_stsv(&offset);
    tipWorldPosition.vx += offset.vx;
    tipWorldPosition.vy += offset.vy;
    tipWorldPosition.vz += offset.vz;
    controlPoints[2].vx  = tipWorldPosition.vx;
    controlPoints[2].vy  = tipWorldPosition.vy;
    controlPoints[2].vz  = tipWorldPosition.vz;
    controlPoints[3].vx  = tipWorldPosition.vx;
    controlPoints[3].vy  = tipWorldPosition.vy;
    controlPoints[3].vz  = tipWorldPosition.vz;
    // Reverse the backward sampler into root-to-tip order without adding an endpoint sample.
    for (linkIndex = ACTOR_503500_LUNGING_CHAIN_TIP_PART; linkIndex >= 0; linkIndex--) {
        _bezierCurveEvaluate(controlPoints, &controlPoints[3], ARRAY_SIZE(samples), linkIndex, &samples[linkIndex].vx);
        copyVector(&work->linkPoints[ACTOR_503500_LUNGING_CHAIN_TIP_PART - linkIndex], &samples[linkIndex]);
    }
    _actor503500LungingChainPlaceLinks(work->linkPoints, task->extra.tmd->coords);
    // Advance sway even on the tip, whose pitch is excluded by the pitch applicator.
    for (linkIndex = ACTOR_503500_LUNGING_CHAIN_TIP_PART; linkIndex >= 2; linkIndex--) {
        swayPitch                       = ((work->swayAmplitude * work->swayWeight >> ACTOR_503500_LUNGING_CHAIN_POSE_WEIGHT_FRACTION_BITS) * rsin(work->swayPhase[linkIndex])) >> ACTOR_503500_LUNGING_CHAIN_POSE_WEIGHT_FRACTION_BITS;
        work->linkAngles[linkIndex].vx  = swayPitch;
        work->linkAngles[linkIndex].vx += D_actor_503500_8016F434[linkIndex] * work->curlWeight >> ACTOR_503500_LUNGING_CHAIN_POSE_WEIGHT_FRACTION_BITS;
        work->linkAngles[linkIndex].vy  = 0;
        work->linkAngles[linkIndex].vz  = 0;
        work->swayPhase[linkIndex]      = (work->swayPhase[linkIndex] + ACTOR_503500_LUNGING_CHAIN_SWAY_PHASE_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
    _actor503500LungingChainApplyPitch(work->linkAngles, task->extra.tmd->coords);
}

/// Places the lunging chain's eight links along nine world-space points.
///
/// `points` and writable `coordinates` each contain the model's nine parts,
/// root first. Each segment must fit signed halfwords, be nonzero, and not be
/// parallel to its current frame's +Y hint. The root and its parent chain must
/// be live; accumulated rotations are assumed orthonormal for transposition.
/// Borrows scratch-stack storage and changes GTE state; writes local link
/// matrices without invalidating their composition stamps.
static void _actor503500LungingChainPlaceLinks(const SVECTOR* points, GfxCoord* coordinates)
{
    Actor503500ChainScratch* scratch;
    s32                      i;
    s32                      j;

    // Accumulate the link's rotation with the current world rotation already
    // loaded into the GTE. Arguments are live, halfword-aligned MATRIX pointers
    // with no side effects: each is evaluated three times. Writes rotation only
    // and changes GTE arithmetic state. This binding exists only in this walk.
#define ACTOR_503500_LUNGING_CHAIN_COMPOSE_LINK_ROTATION(linkRotation, worldRotation) \
    do {                                                                              \
        gte_ldclmv((linkRotation));                                                   \
        gte_rtir();                                                                   \
        gte_stclmv((worldRotation));                                                  \
        gte_ldclmv(&(linkRotation)->m[0][1]);                                         \
        gte_rtir();                                                                   \
        gte_stclmv(&(worldRotation)->m[0][1]);                                        \
        gte_ldclmv(&(linkRotation)->m[0][2]);                                         \
        gte_rtir();                                                                   \
        gte_stclmv(&(worldRotation)->m[0][2]);                                        \
    } while (0)

    scratch        = SCRATCH_STACK_RESERVE_BLOCK(Actor503500ChainScratch);
    scratch->up.vx = 0;
    scratch->up.vy = ONE;
    scratch->up.vz = 0;
    gfxComposeNodeWorldTransform(coordinates->parent, &scratch->worldRotation, &scratch->parentTranslation);
    // Carry the current link's world rotation forward, then undo it for the next segment.
    for (i = 0, j = 1; i < ACTOR_503500_LUNGING_CHAIN_PART_COUNT - 1; i++, j++) {
        scratch->segment.vx = points[j].vx - points[i].vx;
        scratch->segment.vy = points[j].vy - points[i].vy;
        scratch->segment.vz = points[j].vz - points[i].vz;
        gte_SetRotMatrix(&scratch->worldRotation);
        ACTOR_503500_LUNGING_CHAIN_COMPOSE_LINK_ROTATION(&coordinates[i].coord, &scratch->worldRotation);
        _actor503500LungingChainAimNextLink(scratch, &coordinates[j]);
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor503500ChainScratch);
#undef ACTOR_503500_LUNGING_CHAIN_COMPOSE_LINK_ROTATION
}

#include "../../shared/bezier_curve_evaluate.inc.c"

/// Blends the laid-out links from their stored starting pose during unfolding or regrowth.
///
/// Requires initialized work and nine root-first model coordinates.
/// `blendWeight` is 0..4096: zero selects `blendStart`, 4096 leaves the laid-out
/// pose unchanged. Blends rotations through an orthonormal basis and translations
/// with signed 12-fractional-bit products, rounding down. Only links 1..8 change;
/// the root, matrix alignment bytes and composition stamps are retained. Rotation
/// word views require word alignment and transfer exactly 18 bytes, without
/// reading the unwritten translation in the temporary matrix. Changes GTE state.
static void _actor503500LungingChainBlendPose(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_BLEND_WEIGHT_FRACTION_BITS = 12,
        ACTOR_503500_LUNGING_CHAIN_ROTATION_WORD_COUNT        = 4,
    };

    MATRIX                        blendedRotation;
    VECTOR                        translationOffset;
    _Actor503500LungingChainWork* work;
    GfxCoord*                     linkCoord;
    const MATRIX*                 blendStart;
    const s32*                    sourceWords;
    s32*                          destinationWords;
    s32                           blendWeight;
    s32                           linkIndex;
    s32                           rotationWordIndex;

    // Copy exactly 18 rotation bytes from live, word-aligned matrices, leaving
    // alignment bytes and translation untouched. Captures destinationWords,
    // sourceWords and rotationWordIndex. Both pointer arguments are evaluated
    // twice and must have no side effects; only identical-pointer overlap is
    // supported. This binding exists only in the pose blend.
#define ACTOR_503500_LUNGING_CHAIN_COPY_ROTATION(sourceMatrix, destinationMatrix)                                              \
    do {                                                                                                                       \
        destinationWords = (s32*)(destinationMatrix);                                                                          \
        sourceWords      = (const s32*)(sourceMatrix);                                                                         \
        for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_LUNGING_CHAIN_ROTATION_WORD_COUNT; rotationWordIndex++) { \
            *destinationWords++ = *sourceWords++;                                                                              \
        }                                                                                                                      \
        (destinationMatrix)->m[2][2] = (sourceMatrix)->m[2][2];                                                                \
    } while (0)

    work      = task->work;
    linkCoord = task->extra.tmd->coords + 1;
    if (work->blendWeight < ONE) {
        blendStart  = &work->blendStart[1];
        blendWeight = work->blendWeight;
        for (linkIndex = 1; linkIndex < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; linkIndex++) {
            gfxBlendOrthonormalRotation(blendStart, &linkCoord->coord, &blendedRotation, blendWeight);
            ACTOR_503500_LUNGING_CHAIN_COPY_ROTATION(&blendedRotation, &linkCoord->coord);
            translationOffset.vx  = ((linkCoord->coord.t[0] - blendStart->t[0]) * blendWeight) >> ACTOR_503500_LUNGING_CHAIN_BLEND_WEIGHT_FRACTION_BITS;
            translationOffset.vy  = ((linkCoord->coord.t[1] - blendStart->t[1]) * blendWeight) >> ACTOR_503500_LUNGING_CHAIN_BLEND_WEIGHT_FRACTION_BITS;
            translationOffset.vz  = ((linkCoord->coord.t[2] - blendStart->t[2]) * blendWeight) >> ACTOR_503500_LUNGING_CHAIN_BLEND_WEIGHT_FRACTION_BITS;
            linkCoord->coord.t[0] = blendStart->t[0] + translationOffset.vx;
            linkCoord->coord.t[1] = blendStart->t[1] + translationOffset.vy;
            linkCoord->coord.t[2] = blendStart->t[2] + translationOffset.vz;
            blendStart++;
            linkCoord++;
        }
    }
#undef ACTOR_503500_LUNGING_CHAIN_COPY_ROTATION
}

/// Releases a lunging chain's effect reservation, collision bodies and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500LungingChainExit(Task* task)
{
    Enemy*                        enemy;
    _Actor503500LungingChainWork* work;

    enemy = task->spawnArg2.pointer;
    actor503500ReleaseSlotEffects(task->spawnArg1.value);
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    work = task->work;
    worldCollisionUnlinkBody(&work->attackBody);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

static void func_actor_503500_80141D7C(Task* arg0)
{
    _Actor503500LungingChainWork* work;

    work = arg0->work;
    switch (work->state) {
        case ACTOR_503500_LUNGING_CHAIN_STATE_IDLE:
            _actor503500LungingChainStepIdle(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE:
            _actor503500LungingChainStepLunge(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_HOLD:
            work->holdFrames--;
            if (work->holdFrames < 0) {
                _actor503500LungingChainEnterState(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_DYING:
            func_actor_503500_80140654(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING:
            _actor503500LungingChainStepUnfolding(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING:
            _actor503500LungingChainStepRegrowing(arg0);
            break;
    }
    work->slowFrames--;
    if (work->slowFrames < 0) {
        work->slowFrames = 0;
    }
}

/// Restores a lunging chain's resting target and sway while accepting the boss's attack command.
///
/// Requires initialized work in slot 13..16. Consumes the attack command and
/// enters lunge; other requests remain pending. On that same update it still
/// restores sway toward 4096, releases curl toward zero and reloads the slot's
/// rest tip offset in the root parent's frame. Tip movement belongs to the later
/// movement step. The rest table is indexed by slot minus the first chain slot.
static void _actor503500LungingChainStepIdle(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_IDLE_SWAY_STEP = 0x20,
        ACTOR_503500_LUNGING_CHAIN_IDLE_CURL_STEP = 0x111,
    };

    _Actor503500LungingChainWork* work;

    work = task->work;
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE);
        task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    }
    // Restore the resting shape on this update even if it just accepted a lunge.
    work->swayWeight += ACTOR_503500_LUNGING_CHAIN_IDLE_SWAY_STEP;
    if (work->swayWeight > ONE) {
        work->swayWeight = ONE;
    }
    work->curlWeight -= ACTOR_503500_LUNGING_CHAIN_IDLE_CURL_STEP;
    if (work->curlWeight < 0) {
        work->curlWeight = 0;
    }
    copyVector(&work->tipTarget, &D_actor_503500_8016F414[task->spawnArg1.value - ACTOR_503500_SLOT_LUNGING_CHAIN_0]);
}

/// Blends a newly split chain into its laid-out pose, then exposes its target.
///
/// Requires initialized work and a live enemy. The weight uses 4096 for 1.0;
/// it rises by 16 per active update. Equality alone does not finish the blend:
/// the following update clamps it, links the target, enables pair tests and idles.
static void _actor503500LungingChainStepUnfolding(Task* task)
{
    enum { ACTOR_503500_LUNGING_CHAIN_UNFOLD_BLEND_STEP = 16 };

    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;

    work               = task->work;
    work->blendWeight += ACTOR_503500_LUNGING_CHAIN_UNFOLD_BLEND_STEP;
    if (work->blendWeight > ONE) {
        enemy = task->spawnArg2.pointer;
        worldTargetLinkNode(&enemy->node);
        work->blendWeight = ONE;
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
    }
}

/// Grows a chain's laid-out pose from coincident identity links, then exposes it.
///
/// Requires initialized work, a live enemy and state step 0 or 1. Resets the
/// eight link matrices in `blendStart[1..8]`, leaving root entry 0 intact.
/// The weight uses 4096 for 1.0 and rises by 32 per active update, including
/// the reset update. Only a weight above 4096 links the target and enables
/// pair tests before entering idle.
static void _actor503500LungingChainStepRegrowing(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_REGROW_STEP_RESET = 0,
        ACTOR_503500_LUNGING_CHAIN_REGROW_STEP_BLEND = 1,
        ACTOR_503500_LUNGING_CHAIN_REGROW_BLEND_STEP = 32,
    };

    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    s32                           partIndex;
    long*                         translation;

    work = task->work;
    switch (work->stateStep) {
        case ACTOR_503500_LUNGING_CHAIN_REGROW_STEP_RESET:
            // Collapse all links onto the root before the first blend update.
            for (partIndex = 1; partIndex < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; partIndex++) {
                gfxSetRotIdentity(&work->blendStart[partIndex]);
                // This address association keeps the translation stores on the
                // indexed store's induction pointer. The storage is blendStart[partIndex].t.
                translation                      = ((_Actor503500LungingChainWork*)((MATRIX*)work + partIndex))->blendStart[0].t;
                work->blendStart[partIndex].t[0] = 0;
                translation[1]                   = 0;
                translation[2]                   = 0;
            }
            work->blendWeight = 0;
            work->stateStep++;
            // Fall through so regrowth starts on the reset update itself.
        case ACTOR_503500_LUNGING_CHAIN_REGROW_STEP_BLEND:
            work->blendWeight += ACTOR_503500_LUNGING_CHAIN_REGROW_BLEND_STEP;
            if (work->blendWeight > ONE) {
                enemy = task->spawnArg2.pointer;
                worldTargetLinkNode(&enemy->node);
                work->blendWeight = ONE;
                work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            }
            break;
    }
}

/// Steps the lunging chain's `hitCooldown` down to zero, then, unless the
/// global freeze is on, runs both display nodes through their record tables
/// before releasing the tables. Same shape as `func_actor_503500_80144004`.
static void func_actor_503500_801420C4(Task* arg0)
{
    _Actor503500LungingChainWork* work;

    work = arg0->work;
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        func_actor_503500_80140D38(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
        _actor503500LungingChainDisableAttackOnPlayerContact(arg0, &work->attackBody, work->attackContacts, ARRAY_SIZE(work->attackContacts));
    }
    worldCollisionClearContacts(work->contacts);
    worldCollisionClearContacts(work->attackContacts);
}

/// Stops a lunging tip's pair tests after any player or companion body contact.
///
/// Reads exactly `contactCount` elements, including slots after a zero key.
/// Supply a nonnegative count and that many readable contacts; the caller
/// supplies the attack body's four-element table. Contact keys are unchanged.
/// Other body flags are retained; `unusedTask` is ignored.
static void _actor503500LungingChainDisableAttackOnPlayerContact(Task* unusedTask, WorldCollisionBody* attackBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    s32 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++, contacts++) {
        if ((contacts->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            attackBody->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
}

/// Copies the actor's attach-coordinate world position into a stack `VECTOR`
/// and hands it to `worldCoordUpdateActorColor` with zero for the unused arguments. Same body as
/// `_actor503500UpdateBossColor`.
static void func_actor_503500_801421A8(Task* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra.tmd->coords->workm.t[0];
    vec.vy = arg0->extra.tmd->coords->workm.t[1];
    vec.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Applies sway and curl pitch to the lunging chain's interior links 2 through 7.
///
/// Arrays use model-part indices and provide at least eight elements. Only
/// `linkAngles[].vx` is read, in 4096ths of a turn. Extracts XYZ Euler angles
/// from each laid-out basis, replaces X, then rebuilds with the retained ZYX
/// product; this is not an XYZ round trip. Translation and composition stamps
/// are retained. The root, first link and tip are unchanged.
static void _actor503500LungingChainApplyPitch(const SVECTOR* linkAngles, GfxCoord* coordinates)
{
    SVECTOR eulerAngles;
    MATRIX* rotation;
    s32     partIndex;

    for (partIndex = 2; partIndex < ACTOR_503500_LUNGING_CHAIN_TIP_PART; partIndex++) {
        rotation = &coordinates[partIndex].coord;
        gfxExtractSmallestEuler(&eulerAngles, rotation);
        eulerAngles.vx = linkAngles[partIndex].vx;
        gfxSetRotIdentity(&coordinates[partIndex].coord);
        RotMatrixZYX(&eulerAngles, rotation);
    }
}

#include "../../shared/bezier_curve_coefficients.inc.c"

/// Starts a lunging-chain state with normal tip speed and its attack disabled.
///
/// `state` is an `ACTOR_503500_LUNGING_CHAIN_STATE_*` value. Restarts the
/// state step and both frame counters, consumes any pending boss command,
/// and marks the slot busy unless idle. Target-sphere eligibility is retained.
static void _actor503500LungingChainEnterState(Task* task, s32 state)
{
    // Signed 16.16 world units per update.
    enum { ACTOR_503500_LUNGING_CHAIN_REST_SPEED_LIMIT = 96 * 0x10000 };

    _Actor503500LungingChainWork* work;

    work                     = task->work;
    work->state              = state;
    work->stateStep          = 0;
    work->field_3D1          = 0;
    work->stateFrames        = 0;
    work->stepFrames         = 0;
    work->tipSpeedLimit.word = ACTOR_503500_LUNGING_CHAIN_REST_SPEED_LIMIT;
    work->attackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->killCountdown      = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
}

void actor503500LungingChainTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_80132108;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500ArmTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132178 = {
    {
        _actor503500ArmInit,
        func_actor_503500_80143EB4,
        _actor503500ArmExit,
    },
};

/// Initializes an idle arm with a hidden detached model and three disabled spheres.
///
/// Requires slot 10 or 11 in `spawnArg1.value`, its enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows the side's static work and boss lighting.
/// The target rides boss part 11 or 5; the forearm and hand attack spheres
/// follow parts 12/13 or 6/7 and share a four-element contact table.
static void _actor503500ArmInit(Task* task)
{
    enum {
        ACTOR_503500_ARM_SIDE_0_FOREARM_PART = 12,
        ACTOR_503500_ARM_SIDE_0_HAND_PART    = 13,
        ACTOR_503500_ARM_SIDE_1_FOREARM_PART = 6,
        ACTOR_503500_ARM_SIDE_1_HAND_PART    = 7,
    };

    Enemy*                 enemy;
    TmdObject*             model;
    Task*                  parent;
    s32                    side;
    _Actor503500ArmWork*   work;
    GfxCoord*              coord;
    TmdObject*             bossModel;
    SVECTOR*               bodyOffset;
    WorldCollisionContact* contacts;
    GfxCoord*              forearmCoords;
    GfxCoord*              handCoords;

    enemy     = task->spawnArg2.pointer;
    model     = task->extra.tmd;
    parent    = task->parent;
    side      = task->spawnArg1.value - ACTOR_503500_SLOT_ARM_0;
    work      = &D_actor_503500_80178AC0.arms[side];
    coord     = model->coords;
    bossModel = parent->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    task->work = work;

    // Keep the detached model hidden while the boss's own limb supplies the pose.
    coord->parent       = &parent->extra.tmd->coords[D_actor_503500_80171464[side]];
    coord->coord.t[0]   = D_actor_503500_80171478.vx;
    coord->coord.t[1]   = D_actor_503500_80171478.vy;
    coord->coord.t[2]   = D_actor_503500_80171478.vz;
    work->side          = task->spawnArg1.value - ACTOR_503500_SLOT_ARM_0;
    model->lightMtx     = bossModel->lightMtx;
    model->colorMtx     = bossModel->colorMtx;
    model->otOffset     = 19;
    model->flags       |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    enemy->field_4                = &coord->coord;
    bodyOffset                    = &D_actor_503500_80171480[side];
    enemy->field_48               = 0;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = bodyOffset->vx;
    enemy->bodyPos.vy             = bodyOffset->vy;
    enemy->bodyPos.vz             = bodyOffset->vz;
    contacts                      = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = contacts;
    enemy->hp                     = enemy->param->hpMax;

    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = bodyOffset->vx;
    work->body.pos.vy           = bodyOffset->vy;
    work->body.pos.vz           = bodyOffset->vz;
    work->body.key              = ACTOR_503500_PART_BODY_KEY;
    work->body.radius           = 1500;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    // Both strike spheres start inactive and feed the same contact table.
    forearmCoords = parent->extra.tmd->coords;
    if (side != 0) {
        work->forearmAttackBody.coord = &forearmCoords[ACTOR_503500_ARM_SIDE_1_FOREARM_PART];
    } else {
        work->forearmAttackBody.coord = &forearmCoords[ACTOR_503500_ARM_SIDE_0_FOREARM_PART];
    }
    work->forearmAttackBody.context.contacts = work->attackContacts;
    work->forearmAttackBody.pos.vx           = 0;
    work->forearmAttackBody.pos.vy           = 0;
    work->forearmAttackBody.pos.vz           = 0;
    work->forearmAttackBody.key              = ACTOR_503500_PART_BODY_KEY;
    work->forearmAttackBody.radius           = 800;
    work->forearmAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->forearmAttackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    handCoords = parent->extra.tmd->coords;
    if (side != 0) {
        work->handAttackBody.coord = &handCoords[ACTOR_503500_ARM_SIDE_1_HAND_PART];
    } else {
        work->handAttackBody.coord = &handCoords[ACTOR_503500_ARM_SIDE_0_HAND_PART];
    }
    work->handAttackBody.context.contacts = work->attackContacts;
    work->handAttackBody.pos.vx           = 0;
    work->handAttackBody.pos.vy           = 400;
    work->handAttackBody.pos.vz           = 0;
    work->handAttackBody.key              = ACTOR_503500_PART_BODY_KEY;
    work->handAttackBody.radius           = 1200;
    work->handAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->handAttackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->hitEffect.spawnArgLo  = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    work->hitEffect.coord       = coord;
    work->hitEffect.spawnArgHi  = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    work->handAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_IDLE);
    task->exitCallback = _actor503500ArmExit;
    task->state       += 1;
}

/// Plays the boss's arm-strike animation with a timed damaging interval.
///
/// Requires initialized arm work and the live boss parent. Animation rates
/// are in sixteenths of a frame per tick: 12 while the side's large-orb emitter
/// lives, 18 after it is destroyed. Active-update counts 122..143 or 81..95
/// enable the forearm and hand spheres. Animation completion also closes the
/// interval; interruption returns to idle and releases the slot effect budget.
static void _actor503500ArmStepStrike(Task* task)
{
    enum {
        ACTOR_503500_ARM_STRIKE_STEP_START       = 0,
        ACTOR_503500_ARM_STRIKE_STEP_SWING       = 1,
        ACTOR_503500_ARM_STRIKE_ANIMATION_SIDE_0 = 3,
        ACTOR_503500_ARM_STRIKE_ANIMATION_SIDE_1 = 2,
        ACTOR_503500_ARM_STRIKE_SLOW_START_FRAME = 122,
        ACTOR_503500_ARM_STRIKE_SLOW_END_FRAME   = 144,
        ACTOR_503500_ARM_STRIKE_FAST_START_FRAME = 81,
        ACTOR_503500_ARM_STRIKE_FAST_END_FRAME   = 96,
    };

    _Actor503500ArmWork* work;
    s32                  animationPreset;
    s32                  strikeRate;
    s16                  strikeFrame;

    work = task->work;
    if (actor503500ShouldInterruptAttack(task->parent) != 0) {
        _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_IDLE);
        actor503500ReleaseSlotEffects(task->spawnArg1.value);
        return;
    }
    animationPreset = ACTOR_503500_ARM_STRIKE_ANIMATION_SIDE_0;
    if (work->side != 0) {
        animationPreset = ACTOR_503500_ARM_STRIKE_ANIMATION_SIDE_1;
    }
    switch (work->stateStep) {
        case ACTOR_503500_ARM_STRIKE_STEP_START:
            // Losing the emitter accelerates this side's next strike.
            strikeRate = ACTOR_503500_ARM_STRIKE_RATE_SLOW;
            if (actor503500IsSlotEmpty(task->parent, animationPreset == ACTOR_503500_ARM_STRIKE_ANIMATION_SIDE_1
                                                         ? ACTOR_503500_LARGE_ORB_EMITTER_FIRST_SLOT + 1
                                                         : ACTOR_503500_LARGE_ORB_EMITTER_FIRST_SLOT) != 0) {
                strikeRate = ACTOR_503500_ARM_STRIKE_RATE_FAST;
            }
            actor503500PlayAnimationPreset(task->parent, animationPreset, strikeRate);
            work->strikeRate = strikeRate;
            work->stateStep++;
            break;
        case ACTOR_503500_ARM_STRIKE_STEP_SWING:
            // Pair tests cover only the striking portion of the chosen playback.
            strikeFrame = ++work->stateFrames;
            if (work->strikeRate == ACTOR_503500_ARM_STRIKE_RATE_SLOW) {
                switch (strikeFrame) {
                    case ACTOR_503500_ARM_STRIKE_SLOW_START_FRAME:
                        _actor503500ArmBeginStrike(task, work);
                        break;
                    case ACTOR_503500_ARM_STRIKE_SLOW_END_FRAME:
                        work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                }
            } else {
                switch (strikeFrame) {
                    case ACTOR_503500_ARM_STRIKE_FAST_START_FRAME:
                        _actor503500ArmBeginStrike(task, work);
                        break;
                    case ACTOR_503500_ARM_STRIKE_FAST_END_FRAME:
                        work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                }
            }
            if (actor503500HasAnimationFinished(task->parent, animationPreset) != 0) {
                work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->stateStep++;
            }
            break;
        default:
            _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_IDLE);
            break;
    }
}

/// `ACTOR_503500_ARM_STATE_DYING` step of the arm, the counterpart of the pink-flash emitter's
/// `_actor503500PinkFlashEmitterStepDying`: step 0 unlinks the enemy node and clears the
/// 16.16 `spin` / `velocity` / `positionCarry`; step 1 sprays effects for 31 frames, then
/// queues the side's CD load, re-parents the coordinate onto the view in world
/// space, copies the parent's parts 6/7 (or 12/13, by `side`) into its own
/// parts 2/3 and points `velocity` along the coordinate; step 2 plays `SOUND_BRAHMAN_PART_DEATH`;
/// steps 3/4 accelerate `velocity.fixed.vy`, and
/// step 4 fires the light and sound cues on frames 10/30 and leaves on frame
/// 40. Every frame the spin and the travel are applied to the coordinate, and
/// every fourth frame sprays two effects from `D_actor_503500_80171564`.
static void func_actor_503500_80142980(Task* arg0)
{
    SVECTOR              rot;
    MATRIX               m;
    s8                   param1[8];
    s8                   param2[8];
    _Actor503500ArmWork* work;
    Enemy*               enemy;
    GfxCoord*            src;
    GfxCoord*            coord;
    TmdObject*           tmd;
    s32*                 in;
    s32*                 out;
    s32*                 in2;
    s32*                 out2;
    s32                  side;
    s32                  i;
    s32                  j;
    s32                  k;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    switch (work->stateStep) {
        case 0:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs       = 0;
            worldTargetUnlinkNode(&enemy->node);
            actor503500ClearSlotEnemy(arg0->parent, arg0->spawnArg1.value);
            work->hitCooldown = 0;
            (sceneAcquireBattleRef)(0);
            sceneReleaseBattleRefWithRewards(arg0, 0);
            actor503500EnterPartLostState(arg0->parent);
            enemy->reactionFlags             &= ENEMY_REACTION_LOW_CLEAR;
            work->spin.fixed.vx.word          = 0;
            work->spin.fixed.vy.word          = 0;
            work->spin.fixed.vz.word          = 0;
            work->velocity.fixed.vx.word      = 0;
            work->velocity.fixed.vy.word      = 0;
            work->velocity.fixed.vz.word      = 0;
            work->positionCarry.fixed.vx.word = 0;
            work->positionCarry.fixed.vy.word = 0;
            work->positionCarry.fixed.vz.word = 0;
            work->forearmAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->handAttackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->stateStep++;
            break;
        case 1:
            side = work->side;
            if (actor503500TryReserveSlotEffects(arg0->spawnArg1.value, 1) != 0) {
                rot.vx = side != 0 ? -300 : 300;
                rot.vy = (u32)(rcos(work->stateFrames << 7) * 375) >> 10;
                rot.vz = (u32)(rsin(work->stateFrames << 7) * 375) >> 10;
                effectSpawn(EFFECT_HIT_PUFF, coord->parent->parent, 0x01101600, &rot);
            }
            if (++work->stateFrames >= 0x1F) {
                param1[3] = 4;
                param1[2] = 0x30;
                param2[0] = 1;
                param2[1] = 0;
                if (side != 0) {
                    param1[0] = 0x14;
                    param2[2] = -1;
                    param2[3] = 4;
                    MoveImage(&D_actor_503500_8017155C, 0, 0xFD);
                } else {
                    param1[0] = 0x13;
                    param2[2] = -1;
                    param2[3] = 2;
                }
                work->loadCommandSlot = cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
                gfxComposeNodeWorldTransform(coord, &m, &rot);
                // Copy the nine coefficients as four words and a halfword; preserve the alignment halfword.
                in  = (s32*)&m;
                out = (s32*)&coord->coord;
                for (k = 0; k < 4; k++) {
                    *out++ = *in++;
                }
                coord->coord.m[2][2] = m.m[2][2];
                coord->coord.t[0]    = rot.vx;
                coord->coord.t[1]    = rot.vy;
                coord->coord.t[2]    = rot.vz;
                src                  = arg0->parent->extra.tmd->coords;
                if (side != 0) {
                    src += 4;
                } else {
                    src += 10;
                }
                for (i = 2; i < 4; i++) {
                    out2 = (s32*)&coord[i].coord;
                    in2  = (s32*)&src[i].coord;
                    for (k = 0; k < 4; k++) {
                        *out2++ = *in2++;
                    }
                    coord[i].coord.m[2][2] = src[i].coord.m[2][2];
                }
                tmd         = arg0->extra.tmd;
                tmd->flags &= (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                tmdAllocPrimitiveBuffer(tmd);
                rot.vx = 0;
                rot.vy = 0;
                rot.vz = 0;
                if (side != 0) {
                    actor503500SetBossPartScale(arg0->parent, 5, &rot);
                    work->velocity.fixed.vx.word = -0x100000;
                    work->velocity.fixed.vy.word = 0;
                    work->velocity.fixed.vz.word = 0;
                } else {
                    actor503500SetBossPartScale(arg0->parent, 0xB, &rot);
                    work->velocity.fixed.vx.word = 0x100000;
                    work->velocity.fixed.vy.word = 0;
                    work->velocity.fixed.vz.word = 0;
                }
                ApplyMatrixLV(&coord->coord, &work->velocity.vector, &work->velocity.vector);
                coord->parent = &gGfxViewCoord;
                actorRenderComposeCoord(coord);
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 2:
            arg0->extra.tmd->otOffset = 0x11;
            sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                     (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->stateStep++;
            break;
        case 3:
            if (actor503500TryReserveSlotEffects(arg0->spawnArg1.value, 2) != 0) {
                if (work->side != 0) {
                    effectSpawn(EFFECT_HIT_PUFF, coord, 0x01101C00, &D_actor_503500_80171594);
                    work->spin.fixed.vz.word -= 0x2000;
                } else {
                    effectSpawn(EFFECT_HIT_PUFF, coord, 0x01101C00, &D_actor_503500_8017158C);
                    work->spin.fixed.vz.word += 0x2000;
                }
            }
            work->velocity.fixed.vy.word += 0x8000;
            if (++work->stateFrames >= 0x1F) {
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 4:
            work->velocity.fixed.vy.word += 0x8000;
            switch (work->stateFrames) {
                case 10:
                    arg0->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(SOUND_COMMON(0x0D), (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case 30:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 40:
                    sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    arg0->state++;
                    break;
            }
            work->stateFrames++;
            break;
        default:
            arg0->state++;
            break;
    }
    rot.vx = work->spin.fixed.vx.word >> 16;
    rot.vy = work->spin.fixed.vy.word >> 16;
    rot.vz = work->spin.fixed.vz.word >> 16;
    gfxSetRotIdentity(&m);
    RotMatrix(&rot, &m);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&m);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv((char*)&m + 2);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 2);
    gte_ldclmv((char*)&m + 4);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 4);
    work->positionCarry.fixed.vx.word += work->velocity.fixed.vx.word;
    work->positionCarry.fixed.vy.word += work->velocity.fixed.vy.word;
    work->positionCarry.fixed.vz.word += work->velocity.fixed.vz.word;
    coord->coord.t[0]                 += work->positionCarry.fixed.vx.halves.integer;
    coord->coord.t[1]                 += work->positionCarry.fixed.vy.halves.integer;
    coord->coord.t[2]                 += work->positionCarry.fixed.vz.halves.integer;
    work->positionCarry.fixed.vx.word  = work->positionCarry.fixed.vx.halves.fraction;
    work->positionCarry.fixed.vy.word  = work->positionCarry.fixed.vy.halves.fraction;
    work->positionCarry.fixed.vz.word  = work->positionCarry.fixed.vz.halves.fraction;
    coord->composeStamp                = GRAPHICS_COORD_DIRTY;
    if (actor503500TryReserveSlotEffects(arg0->spawnArg1.value, 3) != 0) {
        if (!(gDisplayState.animFrame & 3)) {
            for (i = 0, j = 0; i < 2; i++) {
                effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[i], 0x81018A00, &D_actor_503500_80171564[j]);
                j++;
                j = j < 5 ? j : 0;
            }
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep >= 2) {
        sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        arg0->state = 2;
    }
}

/// Applies one eligible attack contact to an arm.
///
/// Borrows live task, work, enemy, coordinate and read-only contacts. Requires
/// `contactIndex` within the caller's table; earlier keys are readable. Only
/// category-2 attacks land with a clear cooldown; bit 7 selects a live player
/// or companion task. Attack rows must satisfy the damage APIs' bounds.
/// HP loss narrows to s16 before flooring at zero and starting recovery.
/// Reaction 4 or explosion destroys a depleted arm; flagged attachment rows
/// destroy it at any HP. Destruction selects critical-hit effect variant 2 instead of variant 0.
/// The cached world transform must be current and the contact offset nonzero
/// after halfword narrowing. Effects use radius 1500 in game units, then the
/// target sphere's local offset. The cooldown narrows to signed halfword frames.
static inline void _actor503500ArmHandleHit(Task* task, _Actor503500ArmWork* work, Enemy* enemy, GfxCoord* coord,
                                            const WorldCollisionContact* contacts, s32 contactIndex)
{
/// Projects a contact into this part's local hit-effect point.
///
/// Captures coord, contacts, contactIndex, inverseRotation, effectPosition,
/// radialScale and the radius constant below. No arguments. Requires a current
/// cached transform and a nonzero narrowed offset; changes GTE state and
/// preserves signed-halfword narrowing after each position step.
/// `work` selects the side's local sphere centre.
#define ACTOR_503500_ARM_PROJECT_HIT_POINT()                                                                                                                                                             \
    gte_TransposeMatrix(&coord->workm, &inverseRotation);                                                                                                                                                \
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];                                                                                                                             \
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];                                                                                                                             \
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];                                                                                                                             \
    radialScale       = (ACTOR_503500_ARM_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz); \
    effectPosition.vx = effectPosition.vx * radialScale / ONE;                                                                                                                                           \
    effectPosition.vy = effectPosition.vy * radialScale / ONE;                                                                                                                                           \
    effectPosition.vz = effectPosition.vz * radialScale / ONE;                                                                                                                                           \
    gte_SetRotMatrix(&inverseRotation);                                                                                                                                                                  \
    gte_ldv0(&effectPosition);                                                                                                                                                                           \
    gte_rtv0();                                                                                                                                                                                          \
    gte_stsv(&effectPosition);                                                                                                                                                                           \
    effectPosition.vx += D_actor_503500_80171480[work->side].vx;                                                                                                                                         \
    effectPosition.vy += D_actor_503500_80171480[work->side].vy;                                                                                                                                         \
    effectPosition.vz += D_actor_503500_80171480[work->side].vz;
    enum {
        ACTOR_503500_ARM_HIT_EFFECT_RADIUS            = 1500,
        ACTOR_503500_ARM_HIT_NORMAL                   = 0,
        ACTOR_503500_ARM_HIT_CRITICAL                 = 1,
        ACTOR_503500_ARM_HIT_DESTROYING               = 2,
        ACTOR_503500_ARM_DESTROY_IF_DEPLETED_REACTION = 4,
        ACTOR_503500_ARM_DESTROYING_EFFECT_ARG        = 2,
    };
    VECTOR    attackerOffset;
    SVECTOR   effectPosition;
    MATRIX    worldRotation;
    MATRIX    inverseRotation;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    s16       remainingHp;
    u32       attackKey;
    s32       damage;
    s32       hitKind;
    s32       radialScale;
    s32       previousContactIndex;

    // Earlier equal keys suppress repeated contacts from the same attack.
    attackKey = contacts[contactIndex].key.value;
    for (previousContactIndex = 0; previousContactIndex < contactIndex; previousContactIndex++) {
        if (contacts[previousContactIndex].key.value == attackKey) {
            return;
        }
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
        return;
    }
    if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
        return;
    }
    if (work->hitCooldown != 0) {
        return;
    }
    attackerCoord = gPlayerActorTasks[(attackKey >> ACTOR_503500_ATTACK_SOURCE_INDEX_SHIFT) & ACTOR_503500_ATTACK_SOURCE_INDEX_MASK]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &effectPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - effectPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - effectPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - effectPosition.vz;
    hitKind           = ACTOR_503500_ARM_HIT_NORMAL;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        hitKind = ACTOR_503500_ARM_HIT_CRITICAL;
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    remainingHp = enemy->hp - damage;
    enemy->hp   = remainingHp;
    if (remainingHp <= 0) {
        enemy->hp = 0;
        damage   += remainingHp;
        if (work->recoveryFrames == 0) {
            work->recoveryFrames = ACTOR_503500_ARM_RECOVERY_FRAMES;
        }
    }
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {

        case DAMAGE_PLAYER_REACTION_NONE:
        case 5:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
        case 8:
        case 9:
            break;
        case DAMAGE_PLAYER_REACTION_STAGGER:
            damageStartEnemyStagger(enemy);
            break;
        case DAMAGE_PLAYER_REACTION_BUILDUP:
            damageStartEnemyBuildup(enemy, attackKey, 0);
            break;
        case DAMAGE_PLAYER_REACTION_POISON:
            damageTryStartEnemyDamageOverTime(enemy, attackKey, 0);
            break;
        case ACTOR_503500_ARM_DESTROY_IF_DEPLETED_REACTION:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
            if (enemy->hp <= 0) {
                _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_DYING);
                hitKind = ACTOR_503500_ARM_HIT_DESTROYING;
            }
            break;
    }
    if ((attackKey & ACTOR_503500_ATTACHMENT_ATTACK_FLAG) && D_actor_503500_80171490[attackKey & ACTOR_503500_ATTACK_ROW_MASK] != 0) {
        _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_DYING);
        hitKind = ACTOR_503500_ARM_HIT_DESTROYING;
    }
    // Project the contact radially, then move the effect point into the part frame.
    ACTOR_503500_ARM_PROJECT_HIT_POINT();
#undef ACTOR_503500_ARM_PROJECT_HIT_POINT
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (hitKind != ACTOR_503500_ARM_HIT_NORMAL) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, (hitKind == ACTOR_503500_ARM_HIT_DESTROYING) * ACTOR_503500_ARM_DESTROYING_EFFECT_ARG, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Hit handler of the arm, one pass over `count` records. Duplicate
/// ids and anything but a type-2 hit are skipped, as is the whole record while
/// `hitCooldown` runs. Damage is scaled by distance to the
/// attacker, quadrupled on a critical roll, and clamped so the health floors at
/// 0 - which also arms the `ACTOR_503500_ARM_RECOVERY_FRAMES` countdown in
/// `recoveryFrames`. Id kinds 4/6 on a
/// dead enemy, and ids flagged in `D_actor_503500_80171490`, enter
/// `ACTOR_503500_ARM_STATE_DYING` and mark the hit as kind 2. The hit effect is placed
/// 0x5DC along the impact direction in the model's frame, offset by the side
/// vector. Once `recoveryFrames` runs out the health is refilled to a tenth of the
/// spawn record's maximum.
static void func_actor_503500_801431EC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    _Actor503500ArmWork* work;
    Enemy*               enemy;
    GfxCoord*            coord;
    s32                  i;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        _actor503500ArmHandleHit(arg0, work, enemy, coord, arg2, i);
    }
    if (--work->recoveryFrames == 0) {
        enemy->hp = D_actor_503500_8016E7EC[arg0->spawnArg1.value].hpMax / 10;
    } else if (work->recoveryFrames < 0) {
        work->recoveryFrames = 0;
    }
}

/// Scans the arm's `attackContacts`. For each record whose
/// `key` high half is 1 - unless the player task (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`)
/// is in mode 2 or answers message 0x3F8 - copies the parent's root rotation
/// into `knockbackRotation` and turns it by +/-0x5DC with `RotMatrixY` (sign from
/// `side`), then takes the world position of parent coordinate 5 or 11
/// into the player's frame. The sign of its z picks the 0x3FF payload and is
/// passed to the task spawned from `D_actor_503500_8017146C`; message 0x3F9
/// carries the enemy's packed pair, and sound 7 plays at the player.
static void func_actor_503500_801437D0(Task* arg0, WorldCollisionContact* rec, s32 count)
{
    SVECTOR              vec;
    MATRIX               world;
    MATRIX               rot;
    Enemy*               enemy;
    _Actor503500ArmWork* work;
    GfxCoord*            coord;
    Task*                player;
    GfxCoord*            pcoord;
    s32*                 src;
    s32*                 dst;
    s32                  i;
    s32                  j;
    s32                  side;
    s32                  pan;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    for (i = 0; i < count; i++) {
        if ((rec[i].key.value & 0xFFFF0000) == 0x10000) {
            player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            pcoord = player->extra.tmd->coords;
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED &&
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_503500_80171544, 0) == 0) {
                coord = arg0->parent->extra.tmd->coords;
                src   = (s32*)&coord->coord;
                dst   = (s32*)&work->knockbackRotation;
                for (j = 0; j < 4; j++) {
                    *dst++ = *src++;
                }
                work->knockbackRotation.m[2][2] = coord->coord.m[2][2];
                if (work->side != 0) {
                    RotMatrixY(0x5DC, &work->knockbackRotation);
                    coord = &arg0->parent->extra.tmd->coords[5];
                } else {
                    RotMatrixY(-0x5DC, &work->knockbackRotation);
                    coord = &arg0->parent->extra.tmd->coords[11];
                }
                gfxComposeNodeWorldTransform(coord, &world, &vec);
                gte_TransposeMatrix(&pcoord->coord, &rot);
                gte_SetRotMatrix(&rot);
                gte_ldv0(&vec);
                gte_rtv0();
                gte_stsv(&vec);
                side = vec.vz >= 0;
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0);
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_801714E0[side], 0);
                taskSpawnFromTable(&D_actor_503500_8017146C, 0, side, &work->knockbackRotation);
                Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                pan                = (s8)worldCoordGetOriginAudioPan(pcoord);
                sndEvtRequestScriptStart(SOUND_COMMON(7), pan, (s8)(worldCoordGetOriginAudioDepth(pcoord) / 2));
            }
        }
    }
}

void actor503500KnockbackTask(Task* task)
{
/// Copies 18 Q12 rotation bytes, excluding matrix alignment and translation.
///
/// Captures task, work, rotationSourceWords, rotationDestinationWords,
/// rotationWordIndex and the four-word bound. No arguments; requires live,
/// word-aligned disjoint matrices and borrows spawnArg2 only during the copy.
#define ACTOR_503500_KNOCKBACK_COPY_ROTATION()                                                                         \
    rotationSourceWords      = (const s32*)task->spawnArg2.pointer;                                                    \
    rotationDestinationWords = (s32*)&work->rotation;                                                                  \
    for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_KNOCKBACK_ROTATION_WORD_COUNT; rotationWordIndex++) { \
        *rotationDestinationWords++ = *rotationSourceWords++;                                                          \
    }                                                                                                                  \
    work->rotation.m[2][2] = ((const MATRIX*)task->spawnArg2.pointer)->m[2][2];
    enum {
        ACTOR_503500_KNOCKBACK_PLAYER_DEAD         = -1,
        ACTOR_503500_KNOCKBACK_INIT                = 0,
        ACTOR_503500_KNOCKBACK_PUSH                = 1,
        ACTOR_503500_KNOCKBACK_RECOVER             = 2,
        ACTOR_503500_KNOCKBACK_RESTORE_POSE        = 3,
        ACTOR_503500_KNOCKBACK_FINISH              = 4,
        ACTOR_503500_KNOCKBACK_ENABLE_FIRST_BODY   = 1,
        ACTOR_503500_KNOCKBACK_WEAPON_BANK_WORD    = 7,
        ACTOR_503500_KNOCKBACK_ROTATION_WORD_COUNT = 4, // First 16 bytes; final coefficient copied separately
    };
    VECTOR                     velocityStep;
    GameActorMoveBy            moveRequest;
    _Actor503500KnockbackWork* work;
    Task*                      player;
    GfxCoord*                  playerCoord;
    const s32*                 rotationSourceWords;
    s32*                       rotationDestinationWords;
    s32                        rotationWordIndex;
    s32                        audioPan;
    s32                        nextState;
    s32                        cameraShake;

    work   = &D_actor_503500_80178F10;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    switch (task->state) {
        case ACTOR_503500_KNOCKBACK_INIT:
            if (gPlayerStatus.hp <= 0) {
                taskKill(task);
                return;
            }
            memFillBytes(work, 0, sizeof(*work));
            work->speed                           = ACTOR_503500_KNOCKBACK_START_SPEED;
            work->displacementCarry.fixed.vx.word = 0;
            work->displacementCarry.fixed.vy.word = 0;
            work->displacementCarry.fixed.vz.word = 0;
            ACTOR_503500_KNOCKBACK_COPY_ROTATION();
#undef ACTOR_503500_KNOCKBACK_COPY_ROTATION
            padScriptSpawn(D_actor_503500_8017159C, D_actor_503500_801715A4);
            work->shakeFrames = ACTOR_503500_KNOCKBACK_SHAKE_FRAMES;
            // An s32 temp: passed straight to the s8 parameter, the masked
            // expression is shortened into a byte load of the frame counter.
            cameraShake = (gDisplayState.animFrame ^ 1) & 1;
            displaySetShakeY(cameraShake);
            task->state++;
            // Initialization also performs the first push on this update.
        case ACTOR_503500_KNOCKBACK_PUSH:
            // Send integer travel and retain only each axis's fractional carry.
            velocityStep.vx = 0;
            velocityStep.vy = 0;
            velocityStep.vz = work->speed;
            ApplyMatrixLV(&work->rotation, &velocityStep, &velocityStep);
            work->displacementCarry.fixed.vx.word += velocityStep.vx;
            work->displacementCarry.fixed.vy.word += velocityStep.vy;
            work->displacementCarry.fixed.vz.word += velocityStep.vz;
            moveRequest.collisionRequests          = ACTOR_503500_KNOCKBACK_ENABLE_FIRST_BODY;
            moveRequest.keepControl                = true;
            moveRequest.displacement.vx            = work->displacementCarry.fixed.vx.halves.integer;
            moveRequest.displacement.vy            = work->displacementCarry.fixed.vy.halves.integer;
            moveRequest.displacement.vz            = work->displacementCarry.fixed.vz.halves.integer;
            if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &moveRequest, 0) != 0) {
                work->speed = 0;
            }
            work->displacementCarry.fixed.vx.word = work->displacementCarry.fixed.vx.halves.fraction;
            work->displacementCarry.fixed.vy.word = work->displacementCarry.fixed.vy.halves.fraction;
            work->displacementCarry.fixed.vz.word = work->displacementCarry.fixed.vz.halves.fraction;
            work->speed                          -= ACTOR_503500_KNOCKBACK_SPEED_DECAY;
            if (work->speed < 0) {
                work->speed = 0;
                if (++work->restFrames > ACTOR_503500_KNOCKBACK_REST_FRAMES) {
                    // The -1 arm first: reorg inverts the branch around it and
                    // leaves the `li` in the delay slot, sharing $v0 with the load.
                    if (gPlayerStatus.hp <= 0) {
                        nextState = ACTOR_503500_KNOCKBACK_PLAYER_DEAD;
                    } else {
                        nextState = task->state + 1;
                    }
                    task->state = nextState;
                }
            }
            if (work->shakeFrames > 0) {
                cameraShake = (gDisplayState.animFrame ^ 1) & 1;
                displaySetShakeY(cameraShake);
                work->shakeFrames--;
            } else {
                displaySetShakeY(0);
            }
            break;
        case ACTOR_503500_KNOCKBACK_RECOVER:
            // Finish the fall, recover by hit side, then restore the weapon pose.
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_80171508[task->spawnArg1.value], 0);
                task->state++;
            }
            break;
        case ACTOR_503500_KNOCKBACK_RESTORE_POSE:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                D_actor_503500_801714DC =
                    Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]
                        ->table.words[ACTOR_503500_KNOCKBACK_WEAPON_BANK_WORD];
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_80171530, 0);
                task->state++;
            }
            break;
        case ACTOR_503500_KNOCKBACK_FINISH:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
                taskKill(task);
            }
            break;
    }
    if (++task->killCountdown == ACTOR_503500_KNOCKBACK_SOUND_FRAME) {
        playerCoord = player->extra.tmd->coords;
        audioPan    = (s8)worldCoordGetOriginAudioPan(playerCoord);
        sndEvtRequestScriptStart(SOUND_SHELTER_R48_PLAYER_KNOCKBACK, audioPan, (s8)worldCoordGetOriginAudioDepth(playerCoord));
    }
}

static void func_actor_503500_80143EB4(Task* arg0)
{
    Enemy*     enemy;
    TmdObject* tmd;
    s32        mode;

    enemy = arg0->spawnArg2.pointer;
    mode  = gSceneCombatState.actorControl;
    tmd   = arg0->extra.tmd;
    switch (mode) {
        case 1:
            if (!(tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                _actor503500ArmFrameHook(arg0);
            }
            break;
        case 2:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                _actor503500ArmClearReactions(arg0, mode, enemy);
            }
            _actor503500ArmFrameHook(arg0);
            func_actor_503500_80144004(arg0);
            func_actor_503500_801440F0(arg0);
            break;
    }
}

/// Releases an arm's effect reservation, collision bodies and enemy.
///
/// Requires an initialized task with its enemy in `spawnArg2.pointer`. The
/// root coordinate is reparented to the view before teardown. Clearing
/// `task->work` retains the static work block instead of freeing it.
static void _actor503500ArmExit(Task* task)
{
    Enemy*               enemy;
    _Actor503500ArmWork* work;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    actor503500ReleaseSlotEffects(task->spawnArg1.value);
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->forearmAttackBody);
    worldCollisionUnlinkBody(&work->handAttackBody);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Empty arm frame hook, called on active updates and visible paused updates.
///
/// Retains the hook present in the task's update sequence; `unusedTask` is ignored.
static void _actor503500ArmFrameHook(Task* unusedTask)
{
}

static void func_actor_503500_80144004(Task* arg0)
{
    _Actor503500ArmWork* work;
    s16                  timer;

    work = arg0->work;
    if (work->hitCooldown != 0) {
        timer             = (u16)work->hitCooldown - 1;
        work->hitCooldown = timer;
        if (timer < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        func_actor_503500_801431EC(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
        func_actor_503500_801437D0(arg0, work->attackContacts, ARRAY_SIZE(work->attackContacts));
    }
    worldCollisionClearContacts(work->contacts);
    worldCollisionClearContacts(work->attackContacts);
}

/// Discards an arm's stagger, buildup and damage-over-time reaction bits.
///
/// Requires the live enemy in `task->spawnArg2.pointer`. Retains health,
/// counters, behavior and other reaction bits. `unusedActorControl` and
/// `unusedEnemy` are ignored; the enemy is always taken from the task.
static void _actor503500ArmClearReactions(Task* task, s32 unusedActorControl, Enemy* unusedEnemy)
{
    Enemy* enemy = task->spawnArg2.pointer;

    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ~ENEMY_REACTION_STAGGER;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags &= ~ENEMY_REACTION_DAMAGE_OVER_TIME_BITS;
    }
}

static void func_actor_503500_801440F0(Task* arg0)
{
    _Actor503500ArmWork* work = arg0->work;

    switch (work->state) {
        case ACTOR_503500_ARM_STATE_IDLE:
            _actor503500ArmStepIdle(arg0);
            break;
        case ACTOR_503500_ARM_STATE_STRIKE:
            _actor503500ArmStepStrike(arg0);
            break;
        case ACTOR_503500_ARM_STATE_DYING:
            func_actor_503500_80142980(arg0);
            break;
        case ACTOR_503500_ARM_STATE_BECOME_TARGET:
            _actor503500ArmStepBecomeTarget(arg0);
            break;
    }
}

/// Accepts the boss's strike or target-exposure command while the arm is idle.
///
/// Requires initialized work and the boss parent. Recognized commands enter
/// the requested state and are consumed; other commands remain pending.
static void _actor503500ArmStepIdle(Task* task)
{
    switch (task->killCountdown) {
        case ACTOR_503500_SLOT_COMMAND_ATTACK:
            _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_STRIKE);
            task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
            break;
        case ACTOR_503500_SLOT_COMMAND_BECOME_TARGET:
            _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_BECOME_TARGET);
            task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
            break;
    }
}

/// Exposes an arm's target sphere and target node, then returns it to idle.
///
/// Requires initialized work and a live enemy. The idle transition also
/// disables both strike spheres and consumes the boss's command.
static void _actor503500ArmStepBecomeTarget(Task* task)
{
    _Actor503500ArmWork* work;
    Enemy*               enemy;

    work              = task->work;
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    enemy             = task->spawnArg2.pointer;
    worldTargetLinkNode(&enemy->node);
    _actor503500ArmEnterState(task, ACTOR_503500_ARM_STATE_IDLE);
}

/// Starts an arm state with both strike spheres disabled and its sound stopped.
///
/// `state` is an `ACTOR_503500_ARM_STATE_*` value. Restarts the state step
/// and frame count, consumes any pending boss command, and marks the slot
/// busy unless idle. Stopping the strike sound retains its release phase.
static void _actor503500ArmEnterState(Task* task, s32 state)
{
    _Actor503500ArmWork* work;

    work                = task->work;
    work->state         = state;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_ARM_STATE_IDLE);
    // Every state starts outside the damaging interval of a strike.
    work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    sndEvtRequestScriptStop(SOUND_BRAHMAN_ARM_STRIKE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
}

void actor503500ArmTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_80132178;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500BallisticShotTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_801321DC = {
    {
        _actor503500BallisticShotInit,
        _actor503500BallisticShotUpdate,
        _actor503500BallisticShotExit,
    },
};

/// Registers the ballistic shot's attack sphere for room-grid and body tests.
///
/// Requires an unlinked body in live work, a live shot coordinate and four
/// writable contact elements. `spawnArg1.value` selects attack 0 or 1. Uses
/// the local sphere offset and a 300-unit game-coordinate radius. Initializes
/// the contact table's final-entry marker before enabling either pass. The
/// body borrows the coordinate and contact storage until exit unlinks it.
static inline void _actor503500BallisticShotLinkSphere(Task* task, _Actor503500BallisticShotWork* work, GfxCoord* coord, WorldCollisionContact* contacts)
{
    enum { ACTOR_503500_BALLISTIC_SHOT_FLIGHT_RADIUS = 300 };
    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = D_actor_503500_801715AC.vx;
    work->body.pos.vy           = D_actor_503500_801715AC.vy;
    work->body.pos.vz           = D_actor_503500_801715AC.vz;
    work->body.key              = damagePackAttackKey(D_actor_503500_8016E7CC[0], task->spawnArg1.value);
    work->body.radius           = ACTOR_503500_BALLISTIC_SHOT_FLIGHT_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Launches a ballistic small-orb shot with an owned work block and child effect.
///
/// Requires a world-space coordinate body; `spawnArg1.value` selects attack 0
/// or 1. `spawnArg2.value` is forward speed in signed 16.16 units per update;
/// zero resets rotation and leaves velocity zero. The 300-unit attack sphere
/// participates in grid and pair tests. Work and effect live until task exit.
/// Effect creation failure exits before budget acquisition, retaining its
/// unconditional subtraction and possible budget wrap.
static void _actor503500BallisticShotInit(Task* task)
{
    enum {
        ACTOR_503500_BALLISTIC_SHOT_POSITION_FRACTION_BITS = 16,

        ACTOR_503500_BALLISTIC_SHOT_LAUNCH_SOUND = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 5),
    };
    _Actor503500BallisticShotWork* work;
    GfxCoord*                      coord;
    WorldCollisionContact*         contacts;
    EffectWork*                    effectWork;
    Task*                          effectTask;
    VECTOR                         launchVelocity;
    s32                            audioPan;
    coord = task->extra.coordBody->coord;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work = work;

    work->position.fixed.vx.word       = coord->coord.t[0] << ACTOR_503500_BALLISTIC_SHOT_POSITION_FRACTION_BITS;
    work->position.fixed.vy.word       = coord->coord.t[1] << ACTOR_503500_BALLISTIC_SHOT_POSITION_FRACTION_BITS;
    work->position.fixed.vz.word       = coord->coord.t[2] << ACTOR_503500_BALLISTIC_SHOT_POSITION_FRACTION_BITS;
    work->launchPosition.fixed.vx.word = work->position.fixed.vx.word;
    work->field_B8                     = 0x1000;
    work->launchPosition.fixed.vy.word = work->position.fixed.vy.word;
    work->launchPosition.fixed.vz.word = work->position.fixed.vz.word;

    if (task->spawnArg2.value != 0) {
        launchVelocity.vx = 0;
        launchVelocity.vy = 0;
        launchVelocity.vz = task->spawnArg2.value;
        ApplyMatrixLV(&coord->coord, &launchVelocity, &work->velocity.vector);
    } else {
        gfxSetRotIdentity(&coord->coord);
    }
    contacts = work->contacts;

    // Link the sphere before enabling the passes used by this shot.
    _actor503500BallisticShotLinkSphere(task, work, coord, contacts);

    effectWork = effectSpawn(EFFECT_BRAHMAN_SMALL_ORB, coord, 0, NULL);
    if (effectWork == NULL) {
        // Exit subtracts the effect cost even though acquisition has not run.
        _actor503500BallisticShotExit(task);
        return;
    }
    effectTask       = effectWork->task;
    work->effectTask = effectTask;
    taskReparent(task, effectTask);
    audioPan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(ACTOR_503500_BALLISTIC_SHOT_LAUNCH_SOUND, audioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    actor503500AcquireProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_BALLISTIC);
    task->exitCallback = _actor503500BallisticShotExit;
    task->state       += 1;
}

/// Advances a ballistic shot's flight or landing burst by one active update.
///
/// Requires initialized work and a live child effect. Contacts from the
/// collision pass land the shot or cut it short on touching the player.
/// Position and velocity are signed 16.16 world units; positive Y is downward.
/// A cut-short or spent phase advances the task to its exit state when stepped.
static void _actor503500BallisticShotStep(Task* task)
{
    enum {
        ACTOR_503500_BALLISTIC_SHOT_FIXED_ONE     = 0x10000, // One world unit in signed 16.16
        ACTOR_503500_BALLISTIC_SHOT_BURST_RADIUS  = 600,     // World-coordinate units
        ACTOR_503500_BALLISTIC_SHOT_FLIGHT_LIMIT  = 61,      // Active updates without landing
        ACTOR_503500_BALLISTIC_SHOT_BURST_FRAMES  = 6,       // Active updates with the enlarged sphere
        ACTOR_503500_BALLISTIC_SHOT_EFFECT_FINISH = 2,       // Child effect command: emit its final burst, then end
    };

    _Actor503500BallisticShotWork* work;
    GfxCoord*                      coord;

    work  = task->work;
    coord = task->extra.tmd->coords;
    if (work->touchedPlayer != 0) {
        work->effectTask->spawnArg1.value = ACTOR_503500_BALLISTIC_SHOT_EFFECT_FINISH;
        work->phase                       = ACTOR_503500_BALLISTIC_SHOT_CUT_SHORT;
    }
    switch (work->phase) {
        case ACTOR_503500_BALLISTIC_SHOT_FLYING:
            // Keep the floating-point addition: conversion truncates the updated velocity.
            work->velocity.fixed.vy.word += 9.8 * ACTOR_503500_BALLISTIC_SHOT_FIXED_ONE;
            if (work->gridContactResult != 0) {
                // Rest at the resolved position with a larger attack sphere.
                work->body.radius            = ACTOR_503500_BALLISTIC_SHOT_BURST_RADIUS;
                work->velocity.fixed.vx.word = 0;
                work->velocity.fixed.vy.word = 0;
                work->velocity.fixed.vz.word = 0;
                work->landed                 = 1;
                work->gridContactResult      = 0;
                work->phaseFrames            = 0;
                work->phase++;
            } else {
                work->phaseFrames++;
                if (work->phaseFrames >= ACTOR_503500_BALLISTIC_SHOT_FLIGHT_LIMIT) {
                    work->phase = ACTOR_503500_BALLISTIC_SHOT_CUT_SHORT;
                }
            }
            break;
        case ACTOR_503500_BALLISTIC_SHOT_BURSTING:
            // Finish the visual burst and stop pair tests when its interval ends.
            work->phaseFrames++;
            if (work->phaseFrames >= ACTOR_503500_BALLISTIC_SHOT_BURST_FRAMES) {
                work->effectTask->spawnArg1.value = ACTOR_503500_BALLISTIC_SHOT_EFFECT_FINISH;
                work->phaseFrames                 = 0;
                work->body.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->phase++;
            }
            break;
        default:
            task->state++;
            break;
    }
    // Advance even on the update that hands the task to its exit state.
    _actor503500BallisticShotIntegratePosition(work, coord);
}

/// Reacts to a ballistic shot's contacts and advances its flight or landing burst.
///
/// Requires initialized work, coordinate and child effect. Actor controls 1
/// and 2 pause the whole update, retaining contacts; 0 and values above 2 run.
/// Marks the coordinate dirty before resolving contacts and moving the shot.
static void _actor503500BallisticShotUpdate(Task* task)
{
    GfxCoord* coord;
    s32       actorControl;

    coord        = task->extra.tmd->coords;
    actorControl = gSceneCombatState.actorControl;
    if (actorControl <= SCENE_COMBAT_ACTORS_HIDDEN) {
        if (actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor503500BallisticShotReactToContacts(task);
    _actor503500BallisticShotStep(task);
}

/// Unlinks a ballistic shot and releases its task and projectile effect budget.
///
/// Requires allocated work with a linked collision body. Generic task teardown
/// frees that work and exits any child effect. The effect-creation failure path
/// also calls this before acquiring the budget cost; the subtraction is retained.
static void _actor503500BallisticShotExit(Task* task)
{
    _Actor503500BallisticShotWork* work;

    actor503500ReleaseProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_BALLISTIC);
    work = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// Resolves a ballistic shot's room contacts and latches player-body contact.
///
/// Requires initialized work and its four-element contact table. Before
/// landing, applies signed 16.16 grid pushback or restores the launch position
/// for opposed faces. After landing, only the player-body scan remains.
/// Records the grid result for the flight step and empties contacts afterward.
static void _actor503500BallisticShotReactToContacts(Task* task)
{
    WorldCollisionDelta            pushback;
    _Actor503500BallisticShotWork* work;
    WorldCollisionContact*         contacts;
    s32                            pushbackResult;
    s32                            contactIndex;

    work     = task->work;
    contacts = work->contacts;
    if (work->landed == 0) {
        // Apply a correction once; opposed geometry sends the shot back to launch.
        pushbackResult          = worldCollisionResolvePushback(contacts, &pushback, ARRAY_SIZE(work->contacts), NULL);
        work->gridContactResult = pushbackResult;
        switch (pushbackResult) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                work->position.fixed.vx.word += pushback.fixed.vx.word;
                work->position.fixed.vy.word += pushback.fixed.vy.word;
                work->position.fixed.vz.word += pushback.fixed.vz.word;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                work->position.fixed.vx.word = work->launchPosition.fixed.vx.word;
                work->position.fixed.vy.word = work->launchPosition.fixed.vy.word;
                work->position.fixed.vz.word = work->launchPosition.fixed.vz.word;
                break;
        }
    }
    // Any player or companion body cuts the shot short on its next phase step.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            work->touchedPlayer = 1;
        }
    }
    worldCollisionClearContacts(work->contacts);
}

void actor503500BallisticShotTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_801321DC;
    stateHandlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500LingeringShotTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_801321E8 = {
    {
        _actor503500LingeringShotInit,
        _actor503500LingeringShotUpdate,
        _actor503500LingeringShotExit,
    },
};

/// Registers the lingering shot's attack sphere for body tests.
///
/// Requires an unlinked body in live work, a live shot coordinate and four
/// writable contact elements. `spawnArg1.value` selects attack 0 or 1. Uses
/// the local sphere offset and a 2200-unit game-coordinate radius. Initializes
/// the contact table's final-entry marker before enabling pair tests; this
/// sphere has no grid pass. The body borrows the coordinate and contact storage
/// until exit unlinks it.
static inline void _actor503500LingeringShotLinkSphere(Task* task, _Actor503500LingeringShotWork* work, GfxCoord* coord, WorldCollisionContact* contacts)
{
    enum { ACTOR_503500_LINGERING_SHOT_RADIUS = 2200 };
    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = D_actor_503500_801715B4.vx;
    work->body.pos.vy           = D_actor_503500_801715B4.vy;
    work->body.pos.vz           = D_actor_503500_801715B4.vz;
    work->body.key              = damagePackAttackKey(D_actor_503500_8016E7D0[0], task->spawnArg1.value);
    work->body.radius           = ACTOR_503500_LINGERING_SHOT_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
}

/// Launches a lingering large orb or projectile with owned work and a child effect.
///
/// Requires a world-space coordinate body. `spawnArg1.value` is 0 (large orb)
/// or 1 (projectile); it selects the attack and the later linger duration.
/// `spawnArg2.value` is forward speed in signed 16.16 units per update. Zero
/// resets rotation and chooses 16 units per update; movement later takes two
/// forward steps each update. Its 2200-unit sphere uses pair tests alone.
/// Work and effect live until exit. Effect creation failure exits before budget
/// acquisition, retaining the unconditional subtraction and possible wrap.
static void _actor503500LingeringShotInit(Task* task)
{
    enum {
        ACTOR_503500_LINGERING_SHOT_POSITION_FRACTION_BITS = 16,

        ACTOR_503500_LINGERING_SHOT_DEFAULT_SPEED    = 16 * 0x10000,
        ACTOR_503500_LINGERING_SHOT_LARGE_ORB_KIND   = 0,
        ACTOR_503500_LINGERING_SHOT_LARGE_ORB_SOUND  = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 8),
        ACTOR_503500_LINGERING_SHOT_PROJECTILE_SOUND = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 7),
    };
    _Actor503500LingeringShotWork* work;
    GfxCoord*                      coord;
    WorldCollisionContact*         contacts;
    EffectWork*                    effectWork;
    Task*                          effectTask;
    s32                            orbAudioPan;
    s32                            projectileAudioPan;
    coord = task->extra.coordBody->coord;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work = work;

    work->position.vx       = coord->coord.t[0] << ACTOR_503500_LINGERING_SHOT_POSITION_FRACTION_BITS;
    work->position.vy       = coord->coord.t[1] << ACTOR_503500_LINGERING_SHOT_POSITION_FRACTION_BITS;
    work->position.vz       = coord->coord.t[2] << ACTOR_503500_LINGERING_SHOT_POSITION_FRACTION_BITS;
    work->launchPosition.vx = work->position.vx;
    work->field_AC          = 0x1000;
    work->launchPosition.vy = work->position.vy;
    work->launchPosition.vz = work->position.vz;

    if (task->spawnArg2.value != 0) {
        work->speed = task->spawnArg2.value;
    } else {
        gfxSetRotIdentity(&coord->coord);
        work->speed = ACTOR_503500_LINGERING_SHOT_DEFAULT_SPEED;
    }
    contacts = work->contacts;

    // Link the sphere before enabling the passes used by this shot.
    _actor503500LingeringShotLinkSphere(task, work, coord, contacts);

    if (task->spawnArg1.value == ACTOR_503500_LINGERING_SHOT_LARGE_ORB_KIND) {
        effectWork  = effectSpawn(EFFECT_BRAHMAN_LARGE_ORB, coord, 0, NULL);
        orbAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(ACTOR_503500_LINGERING_SHOT_LARGE_ORB_SOUND, orbAudioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    } else {
        effectWork         = effectSpawn(EFFECT_BRAHMAN_PROJECTILE, coord, 0, NULL);
        projectileAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(ACTOR_503500_LINGERING_SHOT_PROJECTILE_SOUND, projectileAudioPan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    }
    if (effectWork == NULL) {
        // Exit subtracts the effect cost even though acquisition has not run.
        _actor503500LingeringShotExit(task);
        return;
    }
    effectTask       = effectWork->task;
    work->effectTask = effectTask;
    taskReparent(task, effectTask);
    actor503500AcquireProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_LINGERING);
    task->exitCallback = _actor503500LingeringShotExit;
    task->state       += 1;
}

/// Advances a lingering shot's deceleration, lingering interval and world position.
///
/// Requires initialized work, coordinate and live child effect. Kind 0 is a
/// large orb, kind 1 a projectile; `spawnArg1.value` indexes a two-entry frame
/// limit table. Position and speed are signed 16.16. Each update moves twice
/// along local Z, rises one world unit, and clamps X to 2000..14000 and Z to
/// 1000..13000. Residual speed remains after the slowing phase. The linger
/// ends only after its count exceeds the kind's limit; a later update exits.
static void _actor503500LingeringShotStep(Task* task)
{
    enum {
        ACTOR_503500_LINGERING_SHOT_SPEED_DECAY      = 8 * 0x10000,
        ACTOR_503500_LINGERING_SHOT_RISE             = 0x10000,
        ACTOR_503500_LINGERING_SHOT_EFFECT_PARTICLES = 2,
        ACTOR_503500_LINGERING_SHOT_MIN_X            = 2000 * 0x10000,
        ACTOR_503500_LINGERING_SHOT_MAX_X            = 14000 * 0x10000,
        ACTOR_503500_LINGERING_SHOT_MIN_Z            = 1000 * 0x10000,
        ACTOR_503500_LINGERING_SHOT_MAX_Z            = 13000 * 0x10000,
    };

    _Actor503500LingeringShotWork* work;
    GfxCoord*                      coord;
    VECTOR                         step;

    work  = task->work;
    coord = task->extra.tmd->coords;
    switch (work->phase) {
        case ACTOR_503500_LINGERING_SHOT_SLOWING:
            // Shed speed, then leave the visual in its particle-only mode.
            work->speed -= ACTOR_503500_LINGERING_SHOT_SPEED_DECAY;
            if (work->speed < ACTOR_503500_LINGERING_SHOT_SPEED_DECAY) {
                work->effectTask->spawnArg1.value = ACTOR_503500_LINGERING_SHOT_EFFECT_PARTICLES;
                work->phase++;
            }
            break;
        case ACTOR_503500_LINGERING_SHOT_LINGERING:
            // Keep the sphere live through the kind's interval, then stop pair tests.
            work->lingerFrames++;
            if (D_actor_503500_801715BC[task->spawnArg1.value] < work->lingerFrames) {
                work->lingerFrames = 0;
                work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->phase++;
            }
            break;
        default:
            task->state++;
            break;
    }
    // Retain both forward steps and the single rise, including on the exit update.
    step.vx = 0;
    step.vy = 0;
    step.vz = work->speed;
    ApplyMatrixLV(&coord->coord, &step, &step);
    work->position.vx += step.vx;
    work->position.vy += step.vy - ACTOR_503500_LINGERING_SHOT_RISE;
    work->position.vz += step.vz;
    work->position.vx += step.vx;
    work->position.vy += step.vy;
    work->position.vz += step.vz;
    // Hold the lingering hazard inside the encounter's ground-plane rectangle.
    if (work->position.vx > ACTOR_503500_LINGERING_SHOT_MAX_X) {
        work->position.vx = ACTOR_503500_LINGERING_SHOT_MAX_X;
    } else if (work->position.vx < ACTOR_503500_LINGERING_SHOT_MIN_X) {
        work->position.vx = ACTOR_503500_LINGERING_SHOT_MIN_X;
    }
    if (work->position.vz > ACTOR_503500_LINGERING_SHOT_MAX_Z) {
        work->position.vz = ACTOR_503500_LINGERING_SHOT_MAX_Z;
    } else if (work->position.vz < ACTOR_503500_LINGERING_SHOT_MIN_Z) {
        work->position.vz = ACTOR_503500_LINGERING_SHOT_MIN_Z;
    }
    coord->coord.t[0] = work->position.vx >> 16;
    coord->coord.t[1] = work->position.vy >> 16;
    coord->coord.t[2] = work->position.vz >> 16;
}

/// Clears a lingering shot's contacts and advances its phase and movement.
///
/// Requires initialized work, coordinate and child effect. Actor controls 1
/// and 2 pause the whole update, retaining contacts; 0 and values above 2 run.
/// The shot delivers attacks but does not react to its own contact records.
static void _actor503500LingeringShotUpdate(Task* task)
{
    GfxCoord* coord;
    s32       actorControl;

    coord        = task->extra.tmd->coords;
    actorControl = gSceneCombatState.actorControl;
    if (actorControl <= SCENE_COMBAT_ACTORS_HIDDEN) {
        if (actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor503500LingeringShotClearContacts(task);
    _actor503500LingeringShotStep(task);
}

/// Releases a lingering shot's effect budget, sound, collision body and task.
///
/// Requires allocated work and a linked sphere. Kind 0 stops the large-orb
/// sound; other values stop the projectile sound, preserving their release
/// phases. Generic teardown frees work and exits the child effect. The effect
/// creation failure path calls this before acquiring its budget cost; that
/// subtraction is retained.
static void _actor503500LingeringShotExit(Task* task)
{
    enum {
        ACTOR_503500_LINGERING_SHOT_LARGE_ORB_KIND   = 0,
        ACTOR_503500_LINGERING_SHOT_LARGE_ORB_SOUND  = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 8),
        ACTOR_503500_LINGERING_SHOT_PROJECTILE_SOUND = SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 7),
    };
    _Actor503500LingeringShotWork* work;

    actor503500ReleaseProjectileEffectCost(ACTOR_503500_PROJECTILE_EFFECT_COST_LINGERING);
    if (task->spawnArg1.value == ACTOR_503500_LINGERING_SHOT_LARGE_ORB_KIND) {
        sndEvtRequestScriptStop(ACTOR_503500_LINGERING_SHOT_LARGE_ORB_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    } else {
        sndEvtRequestScriptStop(ACTOR_503500_LINGERING_SHOT_PROJECTILE_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    work = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// Empties the initialized lingering shot's contact table without reacting to it.
static void _actor503500LingeringShotClearContacts(Task* task)
{
    _Actor503500LingeringShotWork* work;

    work = task->work;
    worldCollisionClearContacts(work->contacts);
}

void actor503500LingeringShotTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_503500_801321E8;
    stateHandlers.funcs[task->state](task);
}
