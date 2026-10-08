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

/// Sound shared by the reviewed arm and lunging-chain corpse fades.
enum { ACTOR_503500_CORPSE_BURN_SOUND = SOUND_COMMON(0x0D) };

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
static void _actor503500ArmApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount);
static void _actor503500LungingChainApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount);
static void _actor503500LungingChainDisableAttackOnPlayerContact(Task* unusedTask, WorldCollisionBody* attackBody, const WorldCollisionContact* contacts, s32 contactCount);
static void _actor503500ArmApplyAttackContacts(Task* task, const WorldCollisionContact* contacts, s32 contactCount);
static void _actor503500LargeOrbEmitterStepAttack(Task* task);
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
static void _actor503500ChainBaseStepState(Task* task);
static void _actor503500ChainBaseStepExposed(Task* task, s32 exposureLimitFrames);
static void _actor503500ChainBaseStepCovered(Task* task);
static void _actor503500YellowFlashEmitterStepState(Task* task);
static void _actor503500ArmStepState(Task* task);
static void _actor503500LargeOrbEmitterStepState(Task* task);
static void _actor503500SmallOrbEmitterStepAttack(Task* emitterTask);
static void _actor503500SmallOrbEmitterStepIdle(Task* task);
static void _actor503500SmallOrbEmitterClearReactions(Task* task);
static void _actor503500SmallOrbEmitterUpdateHits(Task* task);
static void _actor503500SmallOrbEmitterStepState(Task* task);
static void _actor503500YellowFlashEmitterExit(Task* task);
static void _actor503500YellowFlashEmitterClearReactions(Task* task);
static void _actor503500YellowFlashEmitterUpdateHits(Task* task);
static void _actor503500ChainBaseEnterState(Task* task, s32 state);
static void _actor503500YellowFlashEmitterStepAttack(Task* emitterTask);
static void _actor503500YellowFlashEmitterStepIdle(Task* task);
static void _actor503500YellowFlashEmitterStepDormant(Task* task);
static void _actor503500YellowFlashEmitterEnterState(Task* task, s32 state);
static void _actor503500LungingChainStepLunge(Task* task);
static void _actor503500LungingChainStepDying(Task* task);
static void _actor503500LungingChainReactToDamage(Task* task);
static void _actor503500LungingChainStepTip(Task* task);
static void _actor503500LungingChainUpdatePose(Task* task);
static void _actor503500LungingChainBlendPose(Task* task);
static void _actor503500LungingChainStepState(Task* task);
static void _actor503500LungingChainStepIdle(Task* task);
static void _actor503500LungingChainStepUnfolding(Task* task);
static void _actor503500LungingChainStepRegrowing(Task* task);
static void _actor503500LungingChainUpdateHits(Task* task);
static void _actor503500LungingChainUpdateColor(Task* task);
static void _actor503500LungingChainEnterState(Task* task, s32 state);
static void _actor503500ArmStepStrike(Task* task);
static void _actor503500ArmStepDying(Task* task);
static void _actor503500ArmFrameHook(Task* unusedTask);
static void _actor503500ArmUpdateHits(Task* task);
static void _actor503500ArmClearReactions(Task* task, s32 unusedActorControl, Enemy* unusedEnemy);
static void _actor503500ArmStepIdle(Task* task);
static void _actor503500ArmStepBecomeTarget(Task* task);
static void _actor503500ArmEnterState(Task* task, s32 state);
static void _actor503500BallisticShotStep(Task* task);
static void _actor503500BallisticShotReactToContacts(Task* task);
static void _actor503500LingeringShotStep(Task* task);
static void _actor503500LingeringShotClearContacts(Task* task);
static void _actor503500LargeOrbEmitterEnterState(Task* task, s32 state);
static void _actor503500LargeOrbEmitterLaunchOrb(Task* emitterTask, s32 side, s32 launchPointIndex);

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
static void _actor503500LargeOrbEmitterUpdate(Task* task);
static void _actor503500RearPartInit(Task* task);
static void _actor503500RearPartUpdate(Task* task);
static void _actor503500ChainBaseInit(Task* task);
static void _actor503500ChainBaseUpdate(Task* task);
static void _actor503500SmallOrbEmitterInit(Task* task);
static void _actor503500SmallOrbEmitterUpdate(Task* task);
static void _actor503500YellowFlashEmitterInit(Task* task);
static void _actor503500YellowFlashEmitterUpdate(Task* task);
static void _actor503500LungingChainInit(Task* task);
static void _actor503500LungingChainUpdate(Task* task);
static void _actor503500ArmInit(Task* task);
static void _actor503500ArmUpdate(Task* task);
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
        _actor503500LargeOrbEmitterUpdate,
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

static void _actor503500ChainBaseStepAttack(Task* emitterTask);

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

/// Steps the large-orb emitter's boss animation and bearing-selected launches.
///
/// Requires initialized emitter work and a live boss parent. At the launch
/// frame, the player's signed bearing (4096 per turn) chooses either or both
/// launch points; equality at a bearing boundary excludes that point.
/// Checks animation completion on that same update and idles when it finishes.
/// Boss interruption returns to idle and releases the slot's effects.
static void _actor503500LargeOrbEmitterStepAttack(Task* task)
{
    enum {
        ACTOR_503500_LARGE_ORB_ATTACK_START     = 0,
        ACTOR_503500_LARGE_ORB_ATTACK_LAUNCH    = 1,
        ACTOR_503500_LARGE_ORB_ATTACK_RECOVER   = 2,
        ACTOR_503500_LARGE_ORB_ATTACK_ANIMATION = 9,
        ACTOR_503500_LARGE_ORB_ATTACK_RATE      = 16,
        ACTOR_503500_LARGE_ORB_BEARING_OFFSET   = 300,
        ACTOR_503500_LARGE_ORB_QUARTER_TURN     = ONE / 4,
    };

    _Actor503500LargeOrbEmitterWork* work;
    s16                              playerBearing;
    s32                              sideBearingOffset;
    s32                              side;

    work = task->work;
    if (actor503500ShouldInterruptAttack(task->parent) != 0) {
        _actor503500LargeOrbEmitterEnterState(task, ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(task->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_LARGE_ORB_ATTACK_START:
            actor503500PlayAnimationPreset(task->parent, ACTOR_503500_LARGE_ORB_ATTACK_ANIMATION, ACTOR_503500_LARGE_ORB_ATTACK_RATE);
            work->stateStep++;
            break;
        case ACTOR_503500_LARGE_ORB_ATTACK_LAUNCH:
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_LARGE_ORB_EMITTER_ATTACK_LAUNCH_FRAME) {
                sideBearingOffset = -ACTOR_503500_LARGE_ORB_BEARING_OFFSET;
                playerBearing     = actor503500MeasurePlayerBearing(task->parent);
                side              = work->side;
                if (side != 0) {
                    sideBearingOffset = ACTOR_503500_LARGE_ORB_BEARING_OFFSET;
                }
                if (playerBearing > -ACTOR_503500_LARGE_ORB_QUARTER_TURN - sideBearingOffset && playerBearing < ACTOR_503500_LARGE_ORB_QUARTER_TURN - sideBearingOffset) {
                    _actor503500LargeOrbEmitterLaunchOrb(task, side, 0);
                }
                if (playerBearing < sideBearingOffset - ACTOR_503500_LARGE_ORB_QUARTER_TURN || sideBearingOffset + ACTOR_503500_LARGE_ORB_QUARTER_TURN < playerBearing) {
                    _actor503500LargeOrbEmitterLaunchOrb(task, side, 1);
                }
                work->stateStep++;
            }
            // Animation completion is checked on the launch update too.
        case ACTOR_503500_LARGE_ORB_ATTACK_RECOVER:
            if (actor503500HasAnimationFinished(task->parent, ACTOR_503500_LARGE_ORB_ATTACK_ANIMATION) != 0) {
                _actor503500LargeOrbEmitterEnterState(task, ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// Aims a large orb by postmultiplying its world rotation by the local launch rotation.
///
/// Both rotations use signed Q12 coefficients and must be live and disjoint.
/// `shotRotation` must be word-aligned; `launchRotation` must be halfword-aligned.
/// Writes only the nine rotation coefficients, preserving alignment bytes
/// and translation. Changes GTE rotation state; coordinate invalidation belongs
/// to the caller.
static inline void _actor503500LargeOrbEmitterComposeLaunchRotation(MATRIX* shotRotation, const MATRIX* launchRotation)
{
    gte_SetRotMatrix(shotRotation);
    gte_ldclmv(&launchRotation->m[0][0]);
    gte_rtir();
    gte_stclmv(&shotRotation->m[0][0]);
    gte_ldclmv(&launchRotation->m[0][1]);
    gte_rtir();
    gte_stclmv(&shotRotation->m[0][1]);
    gte_ldclmv(&launchRotation->m[0][2]);
    gte_rtir();
    gte_stclmv(&shotRotation->m[0][2]);
}

/// Launches one large orb from a mirrored point on the large-orb emitter.
///
/// Requires the emitter's live coordinate, `side` 0 or 1 and `launchPointIndex`
/// 0 or 1. The initial forward step is 160 in signed 16.16; the shot applies
/// its decayed step twice per update. Side 1 negates local X and launch yaw;
/// angles use 4096 per turn.
/// The spawned task owns its shot; allocation failure simply skips the launch.
/// The angle rows currently cross separately declared data storage; their
/// declaration bounds remain unproven. Uses GTE state and retains no pointer.
static void _actor503500LargeOrbEmitterLaunchOrb(Task* emitterTask, s32 side, s32 launchPointIndex)
{
    enum {
        ACTOR_503500_LARGE_ORB_LINGERING_TASK      = 1,
        ACTOR_503500_LARGE_ORB_KIND                = 0,
        ACTOR_503500_LARGE_ORB_LAUNCH_SPEED        = 160 * 0x10000,
        ACTOR_503500_LARGE_ORB_ROTATION_WORD_COUNT = 4,
    };

    SVECTOR    poseVector;
    SVECTOR    launchOffset;
    MATRIX     rotationMatrix;
    GfxCoord*  emitterCoord;
    GfxCoord*  shotCoord;
    Task*      shotTask;
    s32*       rotationDestinationWords;
    const s32* rotationSourceWords;
    s32        rotationWordIndex;
    s32        mirroredComponent;
    s16        offsetX;
    s16        launchYaw;

    emitterCoord = emitterTask->extra.coordBody->coord;
    shotTask     = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_LARGE_ORB_LINGERING_TASK, ACTOR_503500_LARGE_ORB_KIND, ACTOR_503500_LARGE_ORB_LAUNCH_SPEED);
    if (shotTask != NULL) {
        // Place the shot in world space; side 1 mirrors its local launch point.
        gfxComposeNodeWorldTransform(emitterCoord, &rotationMatrix, &poseVector);
        shotCoord       = shotTask->extra.coordBody->coord;
        launchOffset.vx = offsetX = D_actor_503500_8016F108[launchPointIndex].vx;
        launchOffset.vy           = D_actor_503500_8016F108[launchPointIndex].vy;
        launchOffset.vz           = D_actor_503500_8016F108[launchPointIndex].vz;
        if (side != 0) {
            mirroredComponent = -offsetX;
            launchOffset.vx   = mirroredComponent;
        }
        gte_SetRotMatrix(&rotationMatrix);
        gte_ldv0(&launchOffset);
        gte_rtv0();
        gte_stsv(&launchOffset);
        shotCoord->coord.t[0] = poseVector.vx + launchOffset.vx;
        shotCoord->coord.t[1] = poseVector.vy + launchOffset.vy;
        shotCoord->coord.t[2] = poseVector.vz + launchOffset.vz;
        // Copy 18 rotation bytes only, preserving matrix alignment and translation.
        rotationDestinationWords = (s32*)shotCoord->coord.m;
        rotationSourceWords      = (const s32*)rotationMatrix.m;
        for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_LARGE_ORB_ROTATION_WORD_COUNT; rotationWordIndex++) {
            *rotationDestinationWords++ = *rotationSourceWords++;
        }
        shotCoord->coord.m[2][2] = rotationMatrix.m[2][2];
        poseVector.vx            = D_actor_503500_8016F128[launchPointIndex][0].vx;
        poseVector.vy = launchYaw = D_actor_503500_8016F128[launchPointIndex][0].vy;
        poseVector.vz             = D_actor_503500_8016F128[launchPointIndex][0].vz;
        if (side != 0) {
            mirroredComponent = -launchYaw;
            poseVector.vy     = mirroredComponent;
        }
        // Compose the local launch angles onto the emitter's world rotation.
        RotMatrixZYX(&poseVector, &rotationMatrix);
        _actor503500LargeOrbEmitterComposeLaunchRotation(&shotCoord->coord, &rotationMatrix);
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

/// Updates a large-orb emitter's reactions, hit contacts and behavior state.
///
/// Requires initialized work, its live enemy and its single-coordinate body.
/// Paused updates do nothing; hidden updates only prohibit lock-on. Otherwise
/// invalidates the attached coordinate before processing damage and behavior.
static void _actor503500LargeOrbEmitterUpdate(Task* task)
{
    Enemy*    enemy;
    GfxCoord* rootCoord;

    enemy     = task->spawnArg2.pointer;
    rootCoord = task->extra.coordBody->coord;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500LargeOrbEmitterClearReactions(task);
    }
    _actor503500LargeOrbEmitterUpdateHits(task);
    _actor503500LargeOrbEmitterStepState(task);
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

/// Steps the large-orb emitter's idle, attack or dying behavior.
///
/// Requires initialized emitter work and a live boss parent. Dispatches the
/// work's behavior state, independently of `Task::state`; other values do nothing.
static void _actor503500LargeOrbEmitterStepState(Task* task)
{
    _Actor503500LargeOrbEmitterWork* work = task->work;

    switch (work->state) {
        case ACTOR_503500_LARGE_ORB_EMITTER_STATE_IDLE:
            _actor503500LargeOrbEmitterStepIdle(task);
            break;
        case ACTOR_503500_LARGE_ORB_EMITTER_STATE_ATTACK:
            _actor503500LargeOrbEmitterStepAttack(task);
            break;
        case ACTOR_503500_LARGE_ORB_EMITTER_STATE_DYING:
            _actor503500LargeOrbEmitterStepDying(task);
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

/// Retires the defeated rear part's target and reports its lost boss slot.
///
/// Requires the live slot task, its initialized work and coordinate, and its
/// enemy in `spawnArg2.pointer`. Disables pair tests, drops borrowed contacts,
/// unlinks the target, credits rewards and reports part loss in that order.
/// Starts the death-loop sound with signed-byte pan and half-depth, then
/// advances the death step. The collision body remains linked until task exit.
/// Initializes the rear part's Q12 shrink scale and its first shrink decrement.
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
        _actor503500ChainBaseUpdate,
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

/// Steps a chain base's attack animation and launches its lingering projectile.
///
/// Requires initialized work, a live boss and slot 7 or 8. At the launch frame,
/// spawns kind 1 with initial forward step 192 in signed 16.16, using the
/// base's world pose and mirrored local offset/yaw for slot 8. The shot applies
/// its decayed step twice per update. A failed spawn is skipped.
/// Returns to exposed when the boss animation finishes or is interrupted;
/// interruption also releases the slot's effects.
static void _actor503500ChainBaseStepAttack(Task* emitterTask)
{
    enum {
        ACTOR_503500_CHAIN_BASE_ATTACK_START        = 0,
        ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH       = 1,
        ACTOR_503500_CHAIN_BASE_ATTACK_RECOVER      = 2,
        ACTOR_503500_CHAIN_BASE_ATTACK_ANIMATION    = 10,
        ACTOR_503500_CHAIN_BASE_ATTACK_RATE         = 16,
        ACTOR_503500_CHAIN_BASE_LINGERING_TASK      = 1,
        ACTOR_503500_CHAIN_BASE_PROJECTILE_KIND     = 1,
        ACTOR_503500_CHAIN_BASE_LAUNCH_SPEED        = 192 * 0x10000,
        ACTOR_503500_CHAIN_BASE_ROTATION_WORD_COUNT = 4,
    };

    SVECTOR                    poseVector;
    SVECTOR                    launchOffset;
    MATRIX                     rotationMatrix;
    _Actor503500ChainBaseWork* work;
    GfxCoord*                  baseCoord;
    GfxCoord*                  shotCoord;
    Task*                      shotTask;
    const s32*                 rotationSourceWords;
    s32*                       rotationDestinationWords;
    s32                        rotationWordIndex;
    s32                        mirroredComponent;

    /// Composes the local Q12 rotation onto the shotCoord's current rotation.
    ///
    /// Captures rotationMatrix and shotCoord; requires live disjoint halfword-
    /// aligned matrices. Writes only the nine coefficients and changes GTE
    /// state. Expands to statements; invoke only in a braced block.
#define ACTOR_503500_CHAIN_BASE_COMPOSE_LAUNCH_ROTATION() \
    gte_SetRotMatrix(&shotCoord->coord);                  \
    gte_ldclmv(&rotationMatrix);                          \
    gte_rtir();                                           \
    gte_stclmv(&shotCoord->coord);                        \
    gte_ldclmv(&rotationMatrix.m[0][1]);                  \
    gte_rtir();                                           \
    gte_stclmv(&shotCoord->coord.m[0][1]);                \
    gte_ldclmv(&rotationMatrix.m[0][2]);                  \
    gte_rtir();                                           \
    gte_stclmv(&shotCoord->coord.m[0][2]);

    work      = emitterTask->work;
    baseCoord = emitterTask->extra.coordBody->coord;
    if (actor503500ShouldInterruptAttack(emitterTask->parent) != 0) {
        _actor503500ChainBaseEnterState(emitterTask, ACTOR_503500_CHAIN_BASE_STATE_EXPOSED);
        actor503500ReleaseSlotEffects(emitterTask->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_CHAIN_BASE_ATTACK_START:
            actor503500PlayAnimationPreset(emitterTask->parent, ACTOR_503500_CHAIN_BASE_ATTACK_ANIMATION, ACTOR_503500_CHAIN_BASE_ATTACK_RATE);
            work->stateStep++;
            break;
        case ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH:
            if (++work->stateFrames < ACTOR_503500_CHAIN_BASE_ATTACK_LAUNCH_FRAME) {
                break;
            }
            shotTask = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_CHAIN_BASE_LINGERING_TASK, ACTOR_503500_CHAIN_BASE_PROJECTILE_KIND, ACTOR_503500_CHAIN_BASE_LAUNCH_SPEED);
            if (shotTask != NULL) {
                // Place the shot in world space before composing its mirrored aim.
                gfxComposeNodeWorldTransform(baseCoord, &rotationMatrix, &poseVector);
                shotCoord       = shotTask->extra.coordBody->coord;
                launchOffset.vx = D_actor_503500_8016F258.vx;
                launchOffset.vy = D_actor_503500_8016F258.vy;
                launchOffset.vz = D_actor_503500_8016F258.vz;
                if (emitterTask->spawnArg1.value == ACTOR_503500_SLOT_CHAIN_BASE_1) {
                    mirroredComponent = launchOffset.vx;
                    launchOffset.vx   = -mirroredComponent;
                }
                gte_SetRotMatrix(&rotationMatrix);
                gte_ldv0(&launchOffset);
                gte_rtv0();
                gte_stsv(&launchOffset);
                shotCoord->coord.t[0] = poseVector.vx + launchOffset.vx;
                shotCoord->coord.t[1] = poseVector.vy + launchOffset.vy;
                shotCoord->coord.t[2] = poseVector.vz + launchOffset.vz;
                // Preserve the matrix alignment halfword and the new translation.
                rotationDestinationWords = (s32*)&shotCoord->coord;
                rotationSourceWords      = (const s32*)&rotationMatrix;
                for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_CHAIN_BASE_ROTATION_WORD_COUNT; rotationWordIndex++) {
                    *rotationDestinationWords++ = *rotationSourceWords++;
                }
                shotCoord->coord.m[2][2] = rotationMatrix.m[2][2];
                poseVector.vx            = D_actor_503500_8016F260.vx;
                poseVector.vy            = D_actor_503500_8016F260.vy;
                poseVector.vz            = D_actor_503500_8016F260.vz;
                if (emitterTask->spawnArg1.value == ACTOR_503500_SLOT_CHAIN_BASE_1) {
                    mirroredComponent = poseVector.vy;
                    poseVector.vy     = -mirroredComponent;
                }
                RotMatrixZYX(&poseVector, &rotationMatrix);
                ACTOR_503500_CHAIN_BASE_COMPOSE_LAUNCH_ROTATION();
            }
            work->stateStep++;
            break;
        case ACTOR_503500_CHAIN_BASE_ATTACK_RECOVER:
            if (actor503500HasAnimationFinished(emitterTask->parent, ACTOR_503500_CHAIN_BASE_ATTACK_ANIMATION) != 0) {
                _actor503500ChainBaseEnterState(emitterTask, ACTOR_503500_CHAIN_BASE_STATE_EXPOSED);
            }
            break;
    }
#undef ACTOR_503500_CHAIN_BASE_COMPOSE_LAUNCH_ROTATION
}

/// Retires the defeated chain base's target and reports its lost boss slot.
///
/// Requires the live slot task, its initialized work and coordinate, and its
/// enemy in `spawnArg2.pointer`. Disables pair tests, drops borrowed contacts,
/// unlinks the target, credits rewards and reports part loss in that order.
/// Starts the death-loop sound with signed-byte pan and half-depth, then
/// advances the death step. The collision body remains linked until task exit.
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

/// Updates a chain base's reactions, hit contacts and behavior state.
///
/// Requires initialized work, its live enemy and its single-coordinate body.
/// Paused updates do nothing; hidden updates only prohibit lock-on. Otherwise
/// invalidates the attached coordinate before processing damage and behavior.
static void _actor503500ChainBaseUpdate(Task* task)
{
    Enemy*    enemy;
    GfxCoord* rootCoord;

    enemy     = task->spawnArg2.pointer;
    rootCoord = task->extra.coordBody->coord;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500ChainBaseClearReactions(task);
    }
    _actor503500ChainBaseUpdateHits(task);
    _actor503500ChainBaseStepState(task);
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

/// Steps a chain base's covered, exposed, attack or dying behavior.
///
/// Requires initialized base work and a live boss parent. Exposed behavior
/// regrows once its signed-frame age exceeds 600; other state values do nothing.
static void _actor503500ChainBaseStepState(Task* task)
{
    _Actor503500ChainBaseWork* work = task->work;

    switch (work->state) {
        case ACTOR_503500_CHAIN_BASE_STATE_COVERED:
            _actor503500ChainBaseStepCovered(task);
            break;
        case ACTOR_503500_CHAIN_BASE_STATE_EXPOSED:
            _actor503500ChainBaseStepExposed(task, ACTOR_503500_CHAIN_BASE_EXPOSED_FRAMES);
            break;
        case ACTOR_503500_CHAIN_BASE_STATE_ATTACK:
            _actor503500ChainBaseStepAttack(task);
            break;
        case ACTOR_503500_CHAIN_BASE_STATE_DYING:
            _actor503500ChainBaseStepDying(task);
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
        _actor503500SmallOrbEmitterUpdate,
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

/// Steps the small-orb emitter's attack animation and six-shot ballistic volley.
///
/// Requires initialized work, the live boss and the player's coordinate.
/// Starting at the launch frame, one shot per active update uses index 0..5
/// into the launch-angle and speed tables. Speeds become signed 16.16, with
/// the retained signed shift/division correction for the player's negative Y.
/// Launches from local (0, 400, 2400); allocation failure skips only that shot.
/// Idles after animation completion, or immediately on boss interruption.
/// The volley phase is reached by fallthrough from the delay phase.
static void _actor503500SmallOrbEmitterStepAttack(Task* emitterTask)
{
    enum {
        ACTOR_503500_SMALL_ORB_ATTACK_START         = 0,
        ACTOR_503500_SMALL_ORB_ATTACK_DELAY         = 1,
        ACTOR_503500_SMALL_ORB_ATTACK_VOLLEY        = 2,
        ACTOR_503500_SMALL_ORB_ATTACK_RECOVER       = 3,
        ACTOR_503500_SMALL_ORB_ATTACK_ANIMATION     = 11,
        ACTOR_503500_SMALL_ORB_ATTACK_RATE          = 16,
        ACTOR_503500_SMALL_ORB_BALLISTIC_TASK       = 0,
        ACTOR_503500_SMALL_ORB_KIND                 = 1,
        ACTOR_503500_SMALL_ORB_SPEED_TABLE_SHIFT    = 12,
        ACTOR_503500_SMALL_ORB_HEIGHT_SPEED_SHIFT   = 24,
        ACTOR_503500_SMALL_ORB_HEIGHT_SPEED_DIVISOR = 1000,
        ACTOR_503500_SMALL_ORB_LAUNCH_Y             = 400,
        ACTOR_503500_SMALL_ORB_LAUNCH_Z             = 2400,
        ACTOR_503500_SMALL_ORB_ROTATION_WORD_COUNT  = 4,
    };

    SVECTOR                          worldPosition;
    SVECTOR                          launchOffset;
    MATRIX                           rotationMatrix;
    _Actor503500SmallOrbEmitterWork* work;
    GfxCoord*                        emitterCoord;
    GfxCoord*                        shotCoord;
    Task*                            shotTask;
    const s32*                       rotationSourceWords;
    s32*                             rotationDestinationWords;
    s32                              rotationWordIndex;
    s32                              launchSpeed;
    s16                              shotIndex;

    /// Composes the local Q12 rotation onto the shotCoord's current rotation.
    ///
    /// Captures rotationMatrix and shotCoord; requires live disjoint halfword-
    /// aligned matrices. Writes only the nine coefficients and changes GTE
    /// state. Expands to statements; invoke only in a braced block.
#define ACTOR_503500_SMALL_ORB_COMPOSE_LAUNCH_ROTATION() \
    gte_SetRotMatrix(&shotCoord->coord);                 \
    gte_ldclmv(&rotationMatrix);                         \
    gte_rtir();                                          \
    gte_stclmv(&shotCoord->coord);                       \
    gte_ldclmv(&rotationMatrix.m[0][1]);                 \
    gte_rtir();                                          \
    gte_stclmv(&shotCoord->coord.m[0][1]);               \
    gte_ldclmv(&rotationMatrix.m[0][2]);                 \
    gte_rtir();                                          \
    gte_stclmv(&shotCoord->coord.m[0][2]);

    work         = emitterTask->work;
    emitterCoord = emitterTask->extra.coordBody->coord;
    if (actor503500ShouldInterruptAttack(emitterTask->parent) != 0) {
        _actor503500SmallOrbEmitterEnterState(emitterTask, ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(emitterTask->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_SMALL_ORB_ATTACK_START:
            actor503500PlayAnimationPreset(emitterTask->parent, ACTOR_503500_SMALL_ORB_ATTACK_ANIMATION, ACTOR_503500_SMALL_ORB_ATTACK_RATE);
            work->stateStep++;
            break;
        case ACTOR_503500_SMALL_ORB_ATTACK_DELAY:
            if (++work->stateFrames < ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME) {
                break;
            }
        case ACTOR_503500_SMALL_ORB_ATTACK_VOLLEY:
            // The live delay step falls through and indexes the six-shot volley.
            shotIndex   = work->stateFrames - ACTOR_503500_SMALL_ORB_EMITTER_ATTACK_LAUNCH_FRAME;
            launchSpeed = (D_actor_503500_8016F2E0[shotIndex] << ACTOR_503500_SMALL_ORB_SPEED_TABLE_SHIFT) + (-gPlayerStatus.coordMtx->t[1] << ACTOR_503500_SMALL_ORB_HEIGHT_SPEED_SHIFT) / ACTOR_503500_SMALL_ORB_HEIGHT_SPEED_DIVISOR;
            shotTask    = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_SMALL_ORB_BALLISTIC_TASK, ACTOR_503500_SMALL_ORB_KIND, launchSpeed);
            if (shotTask != NULL) {
                gfxComposeNodeWorldTransform(emitterCoord, &rotationMatrix, &worldPosition);
                shotCoord       = shotTask->extra.coordBody->coord;
                launchOffset.vy = ACTOR_503500_SMALL_ORB_LAUNCH_Y;
                launchOffset.vx = 0;
                launchOffset.vz = ACTOR_503500_SMALL_ORB_LAUNCH_Z;
                gte_SetRotMatrix(&rotationMatrix);
                gte_ldv0(&launchOffset);
                gte_rtv0();
                gte_stsv(&launchOffset);
                shotCoord->coord.t[0] = worldPosition.vx + launchOffset.vx;
                shotCoord->coord.t[1] = worldPosition.vy + launchOffset.vy;
                shotCoord->coord.t[2] = worldPosition.vz + launchOffset.vz;
                // Copy rotation only; compose the volley aim one GTE column at a time.
                rotationSourceWords      = (const s32*)&rotationMatrix;
                rotationDestinationWords = (s32*)&shotCoord->coord;
                for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_SMALL_ORB_ROTATION_WORD_COUNT; rotationWordIndex++) {
                    *rotationDestinationWords++ = *rotationSourceWords++;
                }
                shotCoord->coord.m[2][2] = rotationMatrix.m[2][2];
                RotMatrix(&D_actor_503500_8016F2EC[shotIndex], &rotationMatrix);
                ACTOR_503500_SMALL_ORB_COMPOSE_LAUNCH_ROTATION();
            }
            if (shotIndex >= ACTOR_503500_SMALL_ORB_EMITTER_VOLLEY_SHOTS - 1) {
                work->stateStep += 2;
            }
            break;
        case ACTOR_503500_SMALL_ORB_ATTACK_RECOVER:
            if (actor503500HasAnimationFinished(emitterTask->parent, ACTOR_503500_SMALL_ORB_ATTACK_ANIMATION) != 0) {
                _actor503500SmallOrbEmitterEnterState(emitterTask, ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE);
            }
            break;
    }
#undef ACTOR_503500_SMALL_ORB_COMPOSE_LAUNCH_ROTATION
}

/// Retires the defeated small-orb emitter's target and reports its lost boss slot.
///
/// Requires the live slot task, its initialized work and coordinate, and its
/// enemy in `spawnArg2.pointer`. Disables pair tests, drops borrowed contacts,
/// unlinks the target, credits rewards and reports part loss in that order.
/// Starts the death-loop sound with signed-byte pan and half-depth, then
/// advances the death step. The collision body remains linked until task exit.
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

/// Updates the small-orb emitter's reactions, hit contacts and behavior state.
///
/// Requires initialized work, its live enemy and its single-coordinate body.
/// Paused updates do nothing; hidden updates only prohibit lock-on. Otherwise
/// invalidates the attached coordinate before processing damage and behavior.
static void _actor503500SmallOrbEmitterUpdate(Task* task)
{
    Enemy*    enemy;
    GfxCoord* rootCoord;

    enemy     = task->spawnArg2.pointer;
    rootCoord = task->extra.coordBody->coord;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500SmallOrbEmitterClearReactions(task);
    }
    _actor503500SmallOrbEmitterUpdateHits(task);
    _actor503500SmallOrbEmitterStepState(task);
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

/// Steps the small-orb emitter's idle, volley attack or dying behavior.
///
/// Requires initialized emitter work and a live boss parent. Dispatches the
/// work's behavior state, independently of `Task::state`; other values do nothing.
static void _actor503500SmallOrbEmitterStepState(Task* task)
{
    _Actor503500SmallOrbEmitterWork* work = task->work;

    switch (work->state) {
        case ACTOR_503500_SMALL_ORB_EMITTER_STATE_IDLE:
            _actor503500SmallOrbEmitterStepIdle(task);
            break;
        case ACTOR_503500_SMALL_ORB_EMITTER_STATE_ATTACK:
            _actor503500SmallOrbEmitterStepAttack(task);
            break;
        case ACTOR_503500_SMALL_ORB_EMITTER_STATE_DYING:
            _actor503500SmallOrbEmitterStepDying(task);
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
        _actor503500YellowFlashEmitterUpdate,
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

/// Steps the yellow-flash emitter's attack and recovery animations.
///
/// Requires initialized work and a live boss parent. The launch frame spawns
/// a yellow-flash attack at the emitter's origin; its coordinate borrows the
/// emitter's frame for the attack's lifetime. A failed spawn is skipped.
/// The attack interval finishes before animation completion starts recovery.
/// Boss interruption idles immediately and releases the slot's effects.
static void _actor503500YellowFlashEmitterStepAttack(Task* emitterTask)
{
    enum {
        ACTOR_503500_YELLOW_FLASH_ATTACK_START       = 0,
        ACTOR_503500_YELLOW_FLASH_ATTACK_DELAY       = 1,
        ACTOR_503500_YELLOW_FLASH_ATTACK_WAIT        = 2,
        ACTOR_503500_YELLOW_FLASH_ATTACK_RECOVER     = 3,
        ACTOR_503500_YELLOW_FLASH_ATTACK_ANIMATION   = 12,
        ACTOR_503500_YELLOW_FLASH_RECOVERY_ANIMATION = 13,
        ACTOR_503500_YELLOW_FLASH_ATTACK_RATE        = 8,
        ACTOR_503500_YELLOW_FLASH_RECOVERY_RATE      = 16,
        ACTOR_503500_YELLOW_FLASH_ATTACK_TASK        = 3,
    };

    _Actor503500YellowFlashEmitterWork* work = emitterTask->work;
    Task*                               attackTask;
    GfxCoord*                           attackCoord;

    if (actor503500ShouldInterruptAttack(emitterTask->parent) != 0) {
        _actor503500YellowFlashEmitterEnterState(emitterTask, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(emitterTask->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_YELLOW_FLASH_ATTACK_START:
            actor503500PlayAnimationPreset(emitterTask->parent, ACTOR_503500_YELLOW_FLASH_ATTACK_ANIMATION, ACTOR_503500_YELLOW_FLASH_ATTACK_RATE);
            work->stateStep++;
            break;
        case ACTOR_503500_YELLOW_FLASH_ATTACK_DELAY:
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_FRAMES) {
                work->stateFrames = 0;
                work->stateStep++;
            } else if (work->stateFrames == ACTOR_503500_YELLOW_FLASH_EMITTER_ATTACK_LAUNCH_FRAME) {
                attackTask = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_YELLOW_FLASH_ATTACK_TASK, 0, 0);
                if (attackTask != NULL) {
                    // The attack borrows this emitter's coordinate frame.
                    attackCoord             = attackTask->extra.coordBody->coord;
                    attackCoord->parent     = emitterTask->extra.coordBody->coord;
                    attackCoord->coord.t[0] = 0;
                    attackCoord->coord.t[1] = 0;
                    attackCoord->coord.t[2] = 0;
                }
            }
            break;
        case ACTOR_503500_YELLOW_FLASH_ATTACK_WAIT:
            if (actor503500HasAnimationFinished(emitterTask->parent, ACTOR_503500_YELLOW_FLASH_ATTACK_ANIMATION) != 0) {
                actor503500PlayAnimationPreset(emitterTask->parent, ACTOR_503500_YELLOW_FLASH_RECOVERY_ANIMATION, ACTOR_503500_YELLOW_FLASH_RECOVERY_RATE);
                work->stateStep++;
            }
            break;
        case ACTOR_503500_YELLOW_FLASH_ATTACK_RECOVER:
            if (actor503500HasAnimationFinished(emitterTask->parent, ACTOR_503500_YELLOW_FLASH_RECOVERY_ANIMATION) != 0) {
                _actor503500YellowFlashEmitterEnterState(emitterTask, ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// Retires the defeated yellow-flash emitter's target and reports its lost boss slot.
///
/// Requires the live slot task, its initialized work and coordinate, and its
/// enemy in `spawnArg2.pointer`. Disables pair tests, drops borrowed contacts,
/// unlinks the target, credits rewards and reports part loss in that order.
/// Starts the death-loop sound with signed-byte pan and half-depth, then
/// advances the death step. The collision body remains linked until task exit.
/// The additional death-only halfword write has no established meaning.
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

/// Updates the yellow-flash emitter's reactions, hit contacts and behavior state.
///
/// Requires initialized work, its live enemy and its single-coordinate body.
/// Paused updates do nothing; hidden updates only prohibit lock-on. Otherwise
/// invalidates the attached coordinate before processing damage and behavior.
static void _actor503500YellowFlashEmitterUpdate(Task* task)
{
    Enemy*    enemy;
    GfxCoord* rootCoord;

    enemy     = task->spawnArg2.pointer;
    rootCoord = task->extra.coordBody->coord;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        return;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_HIDDEN) {
        enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (enemy->reactionFlags != 0) {
        _actor503500YellowFlashEmitterClearReactions(task);
    }
    _actor503500YellowFlashEmitterUpdateHits(task);
    _actor503500YellowFlashEmitterStepState(task);
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

/// Ages the yellow-flash emitter's hit cooldown, applies contacts and clears them.
///
/// Requires initialized work with its eight-entry contact table marked at the
/// end. Cooldown decrements in signed halfword frames and floors at zero.
/// Defeat suppresses new hits; contacts are cleared in either case.
static void _actor503500YellowFlashEmitterUpdateHits(Task* task)
{
    _Actor503500YellowFlashEmitterWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500YellowFlashEmitterApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

/// Steps the yellow-flash emitter's idle, attack, dying or dormant behavior.
///
/// Requires initialized emitter work and a live boss parent. Dormant waits
/// for target exposure; other unrecognized state values do nothing.
static void _actor503500YellowFlashEmitterStepState(Task* task)
{
    _Actor503500YellowFlashEmitterWork* work = task->work;

    switch (work->state) {
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_IDLE:
            _actor503500YellowFlashEmitterStepIdle(task);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_ATTACK:
            _actor503500YellowFlashEmitterStepAttack(task);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DYING:
            _actor503500YellowFlashEmitterStepDying(task);
            break;
        case ACTOR_503500_YELLOW_FLASH_EMITTER_STATE_DORMANT:
            _actor503500YellowFlashEmitterStepDormant(task);
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
        _actor503500LungingChainInit,
        _actor503500LungingChainUpdate,
        _actor503500LungingChainExit,
    },
};

/// Initializes one of the boss's four lunging chains and its two tip spheres.
///
/// Requires slot 13..16, its enemy in `spawnArg2.pointer`, a live nine-part
/// model and the boss parent. Borrows that slot's static work block and boss
/// part 1; target and attack contacts remain in the work block until exit.
/// Seeds the rest tip position and per-link blend pose. Tip speed is signed
/// 16.16, angles are 4096 per turn and blend/sway weights use 4096 for 1.0.
/// Split chains unfold before targeting; regrown chains start hidden. Otherwise
/// HP and targeting are enabled immediately. Installs teardown and advances
/// the task from setup to updates.
static void _actor503500LungingChainInit(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_BUFFER_RELEASE_IDLE = -1,
        ACTOR_503500_LUNGING_CHAIN_DRAW_OFFSET         = 18,
        ACTOR_503500_LUNGING_CHAIN_TARGET_RADIUS       = 600,
        ACTOR_503500_LUNGING_CHAIN_ATTACK_RADIUS       = 500,
        ACTOR_503500_LUNGING_CHAIN_INITIAL_SPEED_LIMIT = 96 * 0x10000,
        ACTOR_503500_LUNGING_CHAIN_INITIAL_SWAY        = 64,
        ACTOR_503500_LUNGING_CHAIN_SWAY_PHASE_SHIFT    = 9,
    };

    Enemy*                        enemy;
    TmdObject*                    model;
    GfxCoord*                     rootCoord;
    GfxCoord*                     tipCoord;
    _Actor503500LungingChainWork* work;
    WorldCollisionContact*        targetContacts;
    WorldCollisionContact*        attackContacts;
    MATRIX                        rootRotation;
    s32                           chainIndex;
    s32                           partIndex;

    chainIndex = task->spawnArg1.value - ACTOR_503500_SLOT_LUNGING_CHAIN_0;
    enemy      = task->spawnArg2.pointer;
    work       = &D_actor_503500_80177B60[chainIndex];
    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    task->work = work;

    // Hang the root from the boss; targeting and attack spheres ride the tip.
    rootCoord->parent     = &task->parent->extra.tmd->coords[1];
    rootCoord->coord.t[0] = D_actor_503500_8016F3AC[chainIndex].vx;
    rootCoord->coord.t[1] = D_actor_503500_8016F3AC[chainIndex].vy;
    rootCoord->coord.t[2] = D_actor_503500_8016F3AC[chainIndex].vz;
    gfxSetRotIdentity(&rootRotation);
    RotMatrix(&D_actor_503500_8016F3CC[chainIndex], &rootRotation);
    MulMatrix0(&rootCoord->coord, &rootRotation, &rootCoord->coord);
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    work->bufferFreeCountdown = ACTOR_503500_LUNGING_CHAIN_BUFFER_RELEASE_IDLE;
    model->lightMtx           = &work->lightMtx;
    model->colorMtx           = &work->colorMtx;
    model->otOffset           = ACTOR_503500_LUNGING_CHAIN_DRAW_OFFSET;

    enemy->field_4                = &rootCoord->coord;
    tipCoord                      = &rootCoord[ACTOR_503500_LUNGING_CHAIN_TIP_PART];
    enemy->field_48               = 0;
    enemy->coord                  = tipCoord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F3EC.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F3EC.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F3EC.vz;
    targetContacts                = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = targetContacts;

    work->body.coord            = tipCoord;
    work->body.context.contacts = targetContacts;
    work->body.pos.vx           = D_actor_503500_8016F3EC.vx;
    work->body.pos.vy           = D_actor_503500_8016F3EC.vy;
    work->body.pos.vz           = D_actor_503500_8016F3EC.vz;
    work->body.key              = ACTOR_503500_PART_BODY_KEY;
    work->body.radius           = ACTOR_503500_LUNGING_CHAIN_TARGET_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(targetContacts, ARRAY_SIZE(work->contacts), 0);
    attackContacts                    = work->attackContacts;
    work->attackBody.coord            = tipCoord;
    work->attackBody.context.contacts = attackContacts;
    work->body.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.pos.vx           = D_actor_503500_8016F3F4[chainIndex].vx;
    work->attackBody.pos.vy           = D_actor_503500_8016F3F4[chainIndex].vy;
    work->attackBody.pos.vz           = D_actor_503500_8016F3F4[chainIndex].vz;
    work->attackBody.key              = damagePackAttackKey(enemy->param->attacks, 0);
    work->attackBody.radius           = ACTOR_503500_LUNGING_CHAIN_ATTACK_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->hitEffect.spawnArgLo = ACTOR_503500_PART_HIT_EFFECT_LOW_ARG;
    work->hitEffect.coord      = tipCoord;
    work->hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    work->attackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    // Seed the rest pose before selecting unfold, regrow or immediate targeting.
    copyVector(&work->tipTarget, &D_actor_503500_8016F414[task->spawnArg1.value - ACTOR_503500_SLOT_LUNGING_CHAIN_0]);
    copyVector(&work->tipPosition, &D_actor_503500_8016F414[task->spawnArg1.value - ACTOR_503500_SLOT_LUNGING_CHAIN_0]);
    work->tipSpeedLimit.word = ACTOR_503500_LUNGING_CHAIN_INITIAL_SPEED_LIMIT;
    work->swayAmplitude      = ACTOR_503500_LUNGING_CHAIN_INITIAL_SWAY;
    work->swayWeight         = ONE;
    work->tipAdvancing       = 1;
    for (partIndex = 1; partIndex < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; partIndex++) {
        work->swayPhase[partIndex]  = (partIndex << ACTOR_503500_LUNGING_CHAIN_SWAY_PHASE_SHIFT) & (ONE - 1);
        work->blendStart[partIndex] = rootCoord[partIndex].coord;
    }
    actorRenderComposeCoord(rootCoord);
    _actor503500LungingChainUpdateColor(task);
    switch (task->killCountdown) {
        case ACTOR_503500_SLOT_COMMAND_BECOME_TARGET:
            _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING);
            break;
        case ACTOR_503500_SLOT_COMMAND_REGROW:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING);
            break;
        default:
            worldTargetLinkNode(&enemy->node);
            enemy->hp         = D_actor_503500_8016E7EC[task->spawnArg1.value].hpMax;
            work->blendWeight = ONE;
            work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            break;
    }
    task->exitCallback = _actor503500LungingChainExit;
    task->state       += 1;
}

/// Ages a lunging chain's deferred primitive-buffer release and frees it on expiry.
///
/// Borrows initialized work and its live model. Negative signed-frame values
/// are inactive; zero releases the buffer, then becomes -1. Runs before the
/// model's draw policy can schedule another release.
static inline void _actor503500LungingChainReleaseExpiredBuffer(_Actor503500LungingChainWork* work, TmdObject* model)
{
    s8 bufferFreeCountdown = work->bufferFreeCountdown;

    if (bufferFreeCountdown >= 0) {
        if (bufferFreeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->bufferFreeCountdown--;
    }
}

/// Updates a lunging chain's model lifetime, contacts, movement and laid-out pose.
///
/// Requires initialized work in slot 13..16, its live nine-part model, enemy
/// and boss parent. Buffer-release updates continue while paused or hidden.
/// Paused visible models refresh only color; hidden models stop drawing and
/// prohibit lock-on. Active updates process damage and behavior before movement,
/// then color and pose blending; detached dying chains skip tip movement/layout.
static void _actor503500LungingChainUpdate(Task* task)
{
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    TmdObject*                    model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;
    // Release expired primitive buffers before synchronizing the attached model.
    _actor503500LungingChainReleaseExpiredBuffer(work, model);
    if (gGameSession->eventState != 0 &&
        actor503500IsSlotEmpty(task->parent, task->spawnArg1.value < ACTOR_503500_SLOT_LUNGING_CHAIN_2 ? ACTOR_503500_SLOT_ARM_0 : ACTOR_503500_SLOT_ARM_1) == 0) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        actor503500SyncAttachedModelDrawState(task, &work->bufferFreeCountdown);
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                _actor503500LungingChainUpdateColor(task);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            // Damage may change the state before the chain is moved and laid out.
            if (enemy->reactionFlags != 0) {
                _actor503500LungingChainReactToDamage(task);
            }
            _actor503500LungingChainUpdateHits(task);
            _actor503500LungingChainStepState(task);
            if (work->detached == 0) {
                _actor503500LungingChainStepTip(task);
                _actor503500LungingChainUpdatePose(task);
            }
            _actor503500LungingChainUpdateColor(task);
            _actor503500LungingChainBlendPose(task);
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

/// Retires a lunging chain, detaches it, then sinks, squashes and burns its model.
///
/// Requires initialized work/enemy and a live nine-part model with boss parent.
/// Waits for the tip's death target before detaching. Straightens the links in
/// 4096-per-turn Euler units while the root sinks in positive Y; past Y=1000
/// saves the full root matrix and applies an absolute Q12 squash down to 1/8.
/// Frame counters measure active updates. Smoke walks parts 8..1 every twelfth
/// display animation frame before squash; cue frame 40 advances to task exit.
/// A ready event forces task exit once death has begun.
static void _actor503500LungingChainStepDying(Task* task)
{
    enum {
        ACTOR_503500_LUNGING_CHAIN_DEATH_UNLINK           = 0,
        ACTOR_503500_LUNGING_CHAIN_DEATH_DETACH           = 1,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SINK             = 2,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SQUASH           = 3,
        ACTOR_503500_LUNGING_CHAIN_DEATH_TIP_Y            = 2000,
        ACTOR_503500_LUNGING_CHAIN_DEATH_ANGLE_STEP       = 2,
        ACTOR_503500_LUNGING_CHAIN_DEATH_ANGLE_COMPONENTS = 3,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SINK_STEP        = 10,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SQUASH_Y         = 1000,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SCALE_MIN        = ONE / 8,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SCALE_STEP       = 32,
        ACTOR_503500_LUNGING_CHAIN_DEATH_FADE_FRAME       = 10,
        ACTOR_503500_LUNGING_CHAIN_DEATH_BURN_FRAME       = 15,
        ACTOR_503500_LUNGING_CHAIN_DEATH_BLACK_FRAME      = 30,
        ACTOR_503500_LUNGING_CHAIN_DEATH_EXIT_FRAME       = 40,
        ACTOR_503500_LUNGING_CHAIN_DEATH_EFFECT_COST      = 4,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SMOKE_PERIOD     = 12,
        ACTOR_503500_LUNGING_CHAIN_DEATH_SMOKE_ARG        = 0xB0008600U,
        ACTOR_503500_LUNGING_CHAIN_DEATH_TASK_EXIT        = 2,
    };

    MATRIX                        worldRotation;
    VECTOR                        scale;
    SVECTOR                       poseVector;
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    GfxCoord*                     rootCoord;
    GfxCoord*                     linkCoord;
    s16*                          angleComponent;
    s32                           stateStep;
    s32                           linkIndex;
    s32                           elementIndex;

    /// Eases three signed Euler angles toward zero without overshoot.
    ///
    /// Captures poseVector, angleComponent and elementIndex and requires this
    /// function's angle-step/component constants. Units are 4096 per turn.
    /// Reuses elementIndex with the later smoke walk. Expands to statements;
    /// call only as a standalone statement in a braced block.
#define ACTOR_503500_LUNGING_CHAIN_EASE_DYING_ANGLES()                      \
    angleComponent = &poseVector.vx;                                        \
    elementIndex   = 1;                                                     \
    do {                                                                    \
        if (*angleComponent > 0) {                                          \
            *angleComponent -= ACTOR_503500_LUNGING_CHAIN_DEATH_ANGLE_STEP; \
            if (*angleComponent < 0) {                                      \
                *angleComponent = 0;                                        \
            }                                                               \
        } else {                                                            \
            *angleComponent += ACTOR_503500_LUNGING_CHAIN_DEATH_ANGLE_STEP; \
            if (*angleComponent > 0) {                                      \
                *angleComponent = 0;                                        \
            }                                                               \
        }                                                                   \
        angleComponent++;                                                   \
    } while (elementIndex++ < ACTOR_503500_LUNGING_CHAIN_DEATH_ANGLE_COMPONENTS);

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    stateStep = work->stateStep;
    rootCoord = task->extra.tmd->coords;
    switch (stateStep) {
        case ACTOR_503500_LUNGING_CHAIN_DEATH_UNLINK:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs       = NULL;
            worldTargetUnlinkNode(&enemy->node);
            actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
            work->hitCooldown = 0;
            (sceneAcquireBattleRef)(0);
            sceneReleaseBattleRefWithRewards(task, 0);
            actor503500EnterPartLostState(task->parent);
            enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            work->tipTarget.vy    = ACTOR_503500_LUNGING_CHAIN_DEATH_TIP_Y;
            work->stateStep++;
            break;
        case ACTOR_503500_LUNGING_CHAIN_DEATH_DETACH:
            // Preserve the full world matrix before removing the boss parent.
            if (work->tipArrived != 0) {
                gfxComposeNodeWorldTransform(rootCoord, &worldRotation, &poseVector);
                rootCoord->coord        = worldRotation;
                rootCoord->coord.t[0]   = poseVector.vx;
                rootCoord->coord.t[1]   = poseVector.vy;
                rootCoord->coord.t[2]   = poseVector.vz;
                rootCoord->parent       = &gGfxViewCoord;
                rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->detached          = stateStep;
                actorRenderComposeCoord(rootCoord);
                sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)(worldCoordGetOriginAudioDepth(rootCoord) / 2));
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_DEATH_SINK:
            // Straighten the links while positive Y sinks the root toward the floor.
            for (linkIndex = 1; linkIndex < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; linkIndex++) {
                linkCoord = &rootCoord[linkIndex];
                gfxExtractSmallestEuler(&poseVector, &linkCoord->coord);
                ACTOR_503500_LUNGING_CHAIN_EASE_DYING_ANGLES();
                gfxSetRotIdentity(&rootCoord[linkIndex].coord);
                RotMatrix(&poseVector, &linkCoord->coord);
                linkCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            rootCoord->coord.t[1]  += ACTOR_503500_LUNGING_CHAIN_DEATH_SINK_STEP;
            if (rootCoord->coord.t[1] > ACTOR_503500_LUNGING_CHAIN_DEATH_SQUASH_Y) {
                work->unscaledRootMatrix = rootCoord->coord;
                work->collapseScaleY     = ONE;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_DEATH_SQUASH:
            if (work->collapseScaleY > ACTOR_503500_LUNGING_CHAIN_DEATH_SCALE_MIN) {
                work->collapseScaleY -= ACTOR_503500_LUNGING_CHAIN_DEATH_SCALE_STEP;
            }
            // Restart scaling from the complete snapshot so squash does not compound.
            rootCoord->coord = work->unscaledRootMatrix;
            scale.vx         = ONE;
            scale.vy         = work->collapseScaleY;
            scale.vz         = ONE;
            ScaleMatrixL(&rootCoord->coord, &scale);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            switch (work->stateFrames) {
                case ACTOR_503500_LUNGING_CHAIN_DEATH_FADE_FRAME:
                    task->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(ACTOR_503500_CORPSE_BURN_SOUND, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                             (s8)(worldCoordGetOriginAudioDepth(rootCoord) / 2));
                    break;
                case ACTOR_503500_LUNGING_CHAIN_DEATH_BURN_FRAME:
                    effectSpawn(EFFECT_CORPSE_BURN, rootCoord, 1, NULL);
                    break;
                case ACTOR_503500_LUNGING_CHAIN_DEATH_BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_LUNGING_CHAIN_DEATH_EXIT_FRAME:
                    sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    task->state++;
                    break;
            }
            work->stateFrames++;
            break;
    }
    if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_LUNGING_CHAIN_DEATH_EFFECT_COST) != 0 && work->stateStep < ACTOR_503500_LUNGING_CHAIN_DEATH_SQUASH &&
        gDisplayState.animFrame % ACTOR_503500_LUNGING_CHAIN_DEATH_SMOKE_PERIOD == 0) {
        for (linkIndex = ACTOR_503500_LUNGING_CHAIN_TIP_PART, elementIndex = 0; linkIndex > 0; linkIndex--) {
            effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[linkIndex], ACTOR_503500_LUNGING_CHAIN_DEATH_SMOKE_ARG, &D_actor_503500_8016F448[elementIndex]);
            elementIndex++;
            elementIndex = (elementIndex < (s32)ARRAY_SIZE(D_actor_503500_8016F448)) ? elementIndex : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep > 0) {
        sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        task->state = ACTOR_503500_LUNGING_CHAIN_DEATH_TASK_EXIT;
    }
#undef ACTOR_503500_LUNGING_CHAIN_EASE_DYING_ANGLES
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

/// Applies eligible attack contacts to the lunging chain's tip target.
///
/// Requires initialized work, a live enemy/model and `contactCount` readable
/// contact elements. Earlier equal attack keys suppress duplicates; cooldown
/// blocks further hits. Fatal hits start dying only before unfolding states;
/// a surviving hit interrupts a lunge. Contacts are borrowed and unchanged.
/// `unusedBody` retains the collision-pass signature and is ignored.
static void _actor503500LungingChainApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    s32                           contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = &task->extra.tmd->coords[ACTOR_503500_LUNGING_CHAIN_TIP_PART];
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500LungingChainHandleHit(task, work, enemy, coord, contacts, contactIndex);
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

/// Steps a lunging chain's behavior and ages its temporary movement slowdown.
///
/// Requires initialized work. Hold returns to idle only after its signed-frame
/// countdown becomes negative. Damage-over-time and unknown states have no
/// behavior step; all states still decrement `slowFrames` and floor it at zero.
static void _actor503500LungingChainStepState(Task* task)
{
    _Actor503500LungingChainWork* work;

    work = task->work;
    switch (work->state) {
        case ACTOR_503500_LUNGING_CHAIN_STATE_IDLE:
            _actor503500LungingChainStepIdle(task);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE:
            _actor503500LungingChainStepLunge(task);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_HOLD:
            work->holdFrames--;
            if (work->holdFrames < 0) {
                _actor503500LungingChainEnterState(task, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_DYING:
            _actor503500LungingChainStepDying(task);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING:
            _actor503500LungingChainStepUnfolding(task);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING:
            _actor503500LungingChainStepRegrowing(task);
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

/// Ages a lunging chain's hit cooldown and handles its target and attack contacts.
///
/// Requires initialized work with marked eight-entry target and four-entry
/// attack tables. Cooldown decrements in signed halfword frames and floors at
/// zero. Unless the boss is defeated, applies hits before disabling an attack
/// that touched a player body. Both tables are cleared even after defeat.
static void _actor503500LungingChainUpdateHits(Task* task)
{
    _Actor503500LungingChainWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500LungingChainApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
        _actor503500LungingChainDisableAttackOnPlayerContact(task, &work->attackBody, work->attackContacts, ARRAY_SIZE(work->attackContacts));
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

/// Samples the lunging chain's lighting and colour at its cached root position.
///
/// Requires a live enemy/model and a current cached root transform. Borrows
/// its XYZ world translation in integer game units; the stack sample is used
/// only during the colour query. Does not compose the coordinate.
static void _actor503500LungingChainUpdateColor(Task* task)
{
    VECTOR worldPosition;

    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
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
        _actor503500ArmUpdate,
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

/// Retires an arm, detaches its model and spins it outward under gravity.
///
/// Requires live initialized work/enemy, boss parent and an arm model with
/// parts 0..3. Retires the target before reporting part loss, then disables
/// both attack spheres. After
/// 31 active updates, queues the side's resource load, takes the root's world
/// pose and boss forearm/hand rotations, allocates model buffers and hides the
/// boss limb. Rotation copies exclude alignment and translation. Velocity and
/// carries use signed 16.16; spin uses 16.16 angles at 4096 per turn.
/// Fall and burn counters are active updates; smoke uses display animation
/// frames. Burn cue 40 advances to exit. A ready event exits after detachment.
static void _actor503500ArmStepDying(Task* task)
{
    enum {
        ACTOR_503500_ARM_DEATH_UNLINK                    = 0,
        ACTOR_503500_ARM_DEATH_DETACH                    = 1,
        ACTOR_503500_ARM_DEATH_SOUND                     = 2,
        ACTOR_503500_ARM_DEATH_FALL                      = 3,
        ACTOR_503500_ARM_DEATH_BURN                      = 4,
        ACTOR_503500_ARM_DEATH_STAGE_FRAMES              = 31,
        ACTOR_503500_ARM_DEATH_ROTATION_WORD_COUNT       = 4,
        ACTOR_503500_ARM_DEATH_BOSS_COPY_BASE_SIDE_0     = 10,
        ACTOR_503500_ARM_DEATH_BOSS_COPY_BASE_SIDE_1     = 4,
        ACTOR_503500_ARM_DEATH_BOSS_ROOT_SIDE_0          = 11,
        ACTOR_503500_ARM_DEATH_BOSS_ROOT_SIDE_1          = 5,
        ACTOR_503500_ARM_DEATH_OUTWARD_SPEED             = 16 * 0x10000,
        ACTOR_503500_ARM_DEATH_GRAVITY                   = 0x8000,
        ACTOR_503500_ARM_DEATH_SPIN_ACCELERATION         = 0x2000,
        ACTOR_503500_ARM_DEATH_FRACTION_BITS             = 16,
        ACTOR_503500_ARM_DEATH_FADE_FRAME                = 10,
        ACTOR_503500_ARM_DEATH_BLACK_FRAME               = 30,
        ACTOR_503500_ARM_DEATH_EXIT_FRAME                = 40,
        ACTOR_503500_ARM_DEATH_SMOKE_PERIOD              = 4,
        ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_ARG         = 0x01101600,
        ACTOR_503500_ARM_DEATH_FALL_PUFF_ARG             = 0x01101C00,
        ACTOR_503500_ARM_DEATH_SMOKE_ARG                 = 0x81018A00U,
        ACTOR_503500_ARM_DEATH_FILE_STAGE                = 4,
        ACTOR_503500_ARM_DEATH_FILE_GROUP                = 48,
        ACTOR_503500_ARM_DEATH_FILE_SIDE_0               = 19,
        ACTOR_503500_ARM_DEATH_FILE_SIDE_1               = 20,
        ACTOR_503500_ARM_DEATH_VRAM_ROW_Y                = 253,
        ACTOR_503500_ARM_DEATH_DRAW_OFFSET               = 17,
        ACTOR_503500_ARM_DEATH_TASK_EXIT                 = 2,
        ACTOR_503500_ARM_DEATH_ATTACHED_EFFECT_COST      = 1,
        ACTOR_503500_ARM_DEATH_FALL_EFFECT_COST          = 2,
        ACTOR_503500_ARM_DEATH_SMOKE_EFFECT_COST         = 3,
        ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_X           = 300,
        ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_TRIG_SCALE  = 375,
        ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_TRIG_SHIFT  = 10,
        ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_PHASE_SHIFT = 7,
        ACTOR_503500_ARM_DEATH_FILE_FOLDER_SUFFIX        = 1,
        ACTOR_503500_ARM_DEATH_IMAGE_X_PAGE_OFFSET       = -1,
        ACTOR_503500_ARM_DEATH_IMAGE_Y_OFFSET_SIDE_0     = 2,
        ACTOR_503500_ARM_DEATH_IMAGE_Y_OFFSET_SIDE_1     = 4,
        ACTOR_503500_ARM_DEATH_FIRST_COPIED_PART         = 2,
        ACTOR_503500_ARM_DEATH_COPIED_PARTS              = 2,
        ACTOR_503500_ARM_DEATH_SMOKE_PARTS               = 2,
    };

    SVECTOR              poseVector;
    MATRIX               rotationMatrix;
    s8                   fileKeyBytes[4];
    s8                   loadArgBytes[4];
    _Actor503500ArmWork* work;
    Enemy*               enemy;
    GfxCoord*            bossCoords;
    GfxCoord*            armCoords;
    TmdObject*           model;
    const s32*           rotationSourceWords;
    s32*                 rotationDestinationWords;
    const s32*           partRotationSourceWords;
    s32*                 partRotationDestinationWords;
    s32                  side;
    s32                  partIndex;
    s32                  smokeOffsetIndex;
    s32                  rotationWordIndex;

    /// Composes the local Q12 rotation onto the armCoords's current rotation.
    ///
    /// Captures rotationMatrix and armCoords; requires live disjoint halfword-
    /// aligned matrices. Writes only the nine coefficients and changes GTE
    /// state. Expands to statements; invoke only in a braced block.
#define ACTOR_503500_ARM_COMPOSE_SPIN_ROTATION() \
    gte_SetRotMatrix(&armCoords->coord);         \
    gte_ldclmv(&rotationMatrix);                 \
    gte_rtir();                                  \
    gte_stclmv(&armCoords->coord);               \
    gte_ldclmv(&rotationMatrix.m[0][1]);         \
    gte_rtir();                                  \
    gte_stclmv(&armCoords->coord.m[0][1]);       \
    gte_ldclmv(&rotationMatrix.m[0][2]);         \
    gte_rtir();                                  \
    gte_stclmv(&armCoords->coord.m[0][2]);

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    armCoords = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_ARM_DEATH_UNLINK:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs       = NULL;
            worldTargetUnlinkNode(&enemy->node);
            actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
            work->hitCooldown = 0;
            (sceneAcquireBattleRef)(0);
            sceneReleaseBattleRefWithRewards(task, 0);
            actor503500EnterPartLostState(task->parent);
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
        case ACTOR_503500_ARM_DEATH_DETACH:
            side = work->side;
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_ARM_DEATH_ATTACHED_EFFECT_COST) != 0) {
                poseVector.vx = side != 0 ? -ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_X : ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_X;
                poseVector.vy = (u32)(rcos(work->stateFrames << ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_PHASE_SHIFT) * ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_TRIG_SCALE) >> ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_TRIG_SHIFT;
                poseVector.vz = (u32)(rsin(work->stateFrames << ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_PHASE_SHIFT) * ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_TRIG_SCALE) >> ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_TRIG_SHIFT;
                effectSpawn(EFFECT_HIT_PUFF, armCoords->parent->parent, ACTOR_503500_ARM_DEATH_ATTACHED_PUFF_ARG, &poseVector);
            }
            if (++work->stateFrames >= ACTOR_503500_ARM_DEATH_STAGE_FRAMES) {
                // Enqueue reads key bytes 3/2/0 and all four load arguments; key byte 1 is ignored.
                fileKeyBytes[3] = ACTOR_503500_ARM_DEATH_FILE_STAGE;
                fileKeyBytes[2] = ACTOR_503500_ARM_DEATH_FILE_GROUP;
                loadArgBytes[0] = ACTOR_503500_ARM_DEATH_FILE_FOLDER_SUFFIX;
                loadArgBytes[1] = CD_COMMAND_LOAD_DEFAULT;
                if (side != 0) {
                    fileKeyBytes[0] = ACTOR_503500_ARM_DEATH_FILE_SIDE_1;
                    loadArgBytes[2] = ACTOR_503500_ARM_DEATH_IMAGE_X_PAGE_OFFSET;
                    loadArgBytes[3] = ACTOR_503500_ARM_DEATH_IMAGE_Y_OFFSET_SIDE_1;
                    MoveImage(&D_actor_503500_8017155C, 0, ACTOR_503500_ARM_DEATH_VRAM_ROW_Y);
                } else {
                    fileKeyBytes[0] = ACTOR_503500_ARM_DEATH_FILE_SIDE_0;
                    loadArgBytes[2] = ACTOR_503500_ARM_DEATH_IMAGE_X_PAGE_OFFSET;
                    loadArgBytes[3] = ACTOR_503500_ARM_DEATH_IMAGE_Y_OFFSET_SIDE_0;
                }
                work->loadCommandSlot = cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgBytes);
                gfxComposeNodeWorldTransform(armCoords, &rotationMatrix, &poseVector);
                // Copy the nine coefficients as four words and a halfword; preserve the alignment halfword.
                rotationSourceWords      = (const s32*)&rotationMatrix;
                rotationDestinationWords = (s32*)&armCoords->coord;
                for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_ARM_DEATH_ROTATION_WORD_COUNT; rotationWordIndex++) {
                    *rotationDestinationWords++ = *rotationSourceWords++;
                }
                armCoords->coord.m[2][2] = rotationMatrix.m[2][2];
                armCoords->coord.t[0]    = poseVector.vx;
                armCoords->coord.t[1]    = poseVector.vy;
                armCoords->coord.t[2]    = poseVector.vz;
                // Copy the boss forearm/hand rotations into the detached model.
                bossCoords = task->parent->extra.tmd->coords;
                if (side != 0) {
                    bossCoords += ACTOR_503500_ARM_DEATH_BOSS_COPY_BASE_SIDE_1;
                } else {
                    bossCoords += ACTOR_503500_ARM_DEATH_BOSS_COPY_BASE_SIDE_0;
                }
                for (partIndex = ACTOR_503500_ARM_DEATH_FIRST_COPIED_PART; partIndex < ACTOR_503500_ARM_DEATH_FIRST_COPIED_PART + ACTOR_503500_ARM_DEATH_COPIED_PARTS; partIndex++) {
                    partRotationDestinationWords = (s32*)&armCoords[partIndex].coord;
                    partRotationSourceWords      = (const s32*)&bossCoords[partIndex].coord;
                    for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_ARM_DEATH_ROTATION_WORD_COUNT; rotationWordIndex++) {
                        *partRotationDestinationWords++ = *partRotationSourceWords++;
                    }
                    armCoords[partIndex].coord.m[2][2] = bossCoords[partIndex].coord.m[2][2];
                }
                model         = task->extra.tmd;
                model->flags &= (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                tmdAllocPrimitiveBuffer(model);
                poseVector.vx = 0;
                poseVector.vy = 0;
                poseVector.vz = 0;
                if (side != 0) {
                    actor503500SetBossPartScale(task->parent, ACTOR_503500_ARM_DEATH_BOSS_ROOT_SIDE_1, &poseVector);
                    work->velocity.fixed.vx.word = -ACTOR_503500_ARM_DEATH_OUTWARD_SPEED;
                    work->velocity.fixed.vy.word = 0;
                    work->velocity.fixed.vz.word = 0;
                } else {
                    actor503500SetBossPartScale(task->parent, ACTOR_503500_ARM_DEATH_BOSS_ROOT_SIDE_0, &poseVector);
                    work->velocity.fixed.vx.word = ACTOR_503500_ARM_DEATH_OUTWARD_SPEED;
                    work->velocity.fixed.vy.word = 0;
                    work->velocity.fixed.vz.word = 0;
                }
                ApplyMatrixLV(&armCoords->coord, &work->velocity.vector, &work->velocity.vector);
                armCoords->parent = &gGfxViewCoord;
                actorRenderComposeCoord(armCoords);
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_ARM_DEATH_SOUND:
            task->extra.tmd->otOffset = ACTOR_503500_ARM_DEATH_DRAW_OFFSET;
            sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(armCoords),
                                     (s8)(worldCoordGetOriginAudioDepth(armCoords) / 2));
            work->stateStep++;
            break;
        case ACTOR_503500_ARM_DEATH_FALL:
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_ARM_DEATH_FALL_EFFECT_COST) != 0) {
                if (work->side != 0) {
                    effectSpawn(EFFECT_HIT_PUFF, armCoords, ACTOR_503500_ARM_DEATH_FALL_PUFF_ARG, &D_actor_503500_80171594);
                    work->spin.fixed.vz.word -= ACTOR_503500_ARM_DEATH_SPIN_ACCELERATION;
                } else {
                    effectSpawn(EFFECT_HIT_PUFF, armCoords, ACTOR_503500_ARM_DEATH_FALL_PUFF_ARG, &D_actor_503500_8017158C);
                    work->spin.fixed.vz.word += ACTOR_503500_ARM_DEATH_SPIN_ACCELERATION;
                }
            }
            work->velocity.fixed.vy.word += ACTOR_503500_ARM_DEATH_GRAVITY;
            if (++work->stateFrames >= ACTOR_503500_ARM_DEATH_STAGE_FRAMES) {
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_ARM_DEATH_BURN:
            work->velocity.fixed.vy.word += ACTOR_503500_ARM_DEATH_GRAVITY;
            switch (work->stateFrames) {
                case ACTOR_503500_ARM_DEATH_FADE_FRAME:
                    task->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(ACTOR_503500_CORPSE_BURN_SOUND, (s8)worldCoordGetOriginAudioPan(armCoords),
                                             (s8)(worldCoordGetOriginAudioDepth(armCoords) / 2));
                    break;
                case ACTOR_503500_ARM_DEATH_BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_ARM_DEATH_EXIT_FRAME:
                    sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    task->state++;
                    break;
            }
            work->stateFrames++;
            break;
        default:
            task->state++;
            break;
    }
    // Apply accumulated angular velocity, then drain integer travel from 16.16 carries.
    poseVector.vx = work->spin.fixed.vx.word >> ACTOR_503500_ARM_DEATH_FRACTION_BITS;
    poseVector.vy = work->spin.fixed.vy.word >> ACTOR_503500_ARM_DEATH_FRACTION_BITS;
    poseVector.vz = work->spin.fixed.vz.word >> ACTOR_503500_ARM_DEATH_FRACTION_BITS;
    gfxSetRotIdentity(&rotationMatrix);
    RotMatrix(&poseVector, &rotationMatrix);
    ACTOR_503500_ARM_COMPOSE_SPIN_ROTATION();
    work->positionCarry.fixed.vx.word += work->velocity.fixed.vx.word;
    work->positionCarry.fixed.vy.word += work->velocity.fixed.vy.word;
    work->positionCarry.fixed.vz.word += work->velocity.fixed.vz.word;
    armCoords->coord.t[0]             += work->positionCarry.fixed.vx.halves.integer;
    armCoords->coord.t[1]             += work->positionCarry.fixed.vy.halves.integer;
    armCoords->coord.t[2]             += work->positionCarry.fixed.vz.halves.integer;
    work->positionCarry.fixed.vx.word  = work->positionCarry.fixed.vx.halves.fraction;
    work->positionCarry.fixed.vy.word  = work->positionCarry.fixed.vy.halves.fraction;
    work->positionCarry.fixed.vz.word  = work->positionCarry.fixed.vz.halves.fraction;
    armCoords->composeStamp            = GRAPHICS_COORD_DIRTY;
    if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_ARM_DEATH_SMOKE_EFFECT_COST) != 0) {
        if (!(gDisplayState.animFrame & (ACTOR_503500_ARM_DEATH_SMOKE_PERIOD - 1))) {
            for (partIndex = 0, smokeOffsetIndex = 0; partIndex < ACTOR_503500_ARM_DEATH_SMOKE_PARTS; partIndex++) {
                effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[partIndex], ACTOR_503500_ARM_DEATH_SMOKE_ARG, &D_actor_503500_80171564[smokeOffsetIndex]);
                smokeOffsetIndex++;
                smokeOffsetIndex = smokeOffsetIndex < (s32)ARRAY_SIZE(D_actor_503500_80171564) ? smokeOffsetIndex : 0;
            }
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep >= ACTOR_503500_ARM_DEATH_SOUND) {
        sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        task->state = ACTOR_503500_ARM_DEATH_TASK_EXIT;
    }
#undef ACTOR_503500_ARM_COMPOSE_SPIN_ROTATION
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

/// Applies eligible hits to an arm and advances its health-recovery countdown.
///
/// Requires initialized work, a live enemy/model and `contactCount` readable
/// contact elements. Earlier equal attack keys suppress duplicates; cooldown
/// blocks further hits. Depleted HP arms recovery; only destroying reactions
/// or flagged attachment attacks start death. Expiry restores one tenth of the
/// slot's maximum HP even when no contact lands. Contacts are borrowed and
/// unchanged; `unusedBody` retains the collision-pass signature and is ignored.
static void _actor503500ArmApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    enum { ACTOR_503500_ARM_RECOVERY_HP_DIVISOR = 10 };

    _Actor503500ArmWork* work;
    Enemy*               enemy;
    GfxCoord*            coord;
    s32                  contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500ArmHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
    if (--work->recoveryFrames == 0) {
        enemy->hp = D_actor_503500_8016E7EC[task->spawnArg1.value].hpMax / ACTOR_503500_ARM_RECOVERY_HP_DIVISOR;
    } else if (work->recoveryFrames < 0) {
        work->recoveryFrames = 0;
    }
}

/// Applies arm attack contacts to the player and starts the scripted knockback.
///
/// Requires live arm work/enemy, boss, player and `contactCount` readable
/// elements. Processes every player-body key, skipping scripted or input-held
/// players. Copies the boss root rotation, turns it by the arm side's +/-1500
/// (4096 per turn), applies damage and chooses a fall animation from a rotated
/// arm position without subtracting player translation. Locks attachments and
/// starts the hit sound even if spawning the knockback task fails.
/// The spawned task borrows `knockbackRotation` until its first active update;
/// the arm's work must remain live and unchanged until that copy. Contacts are
/// read only and are not consumed or deduplicated here.
static void _actor503500ArmApplyAttackContacts(Task* task, const WorldCollisionContact* contacts, s32 contactCount)
{
    enum {
        ACTOR_503500_ARM_KNOCKBACK_ROTATION_WORD_COUNT = 4,
        ACTOR_503500_ARM_KNOCKBACK_TURN                = 1500,
        ACTOR_503500_ARM_KNOCKBACK_PART_SIDE_0         = 11,
        ACTOR_503500_ARM_KNOCKBACK_PART_SIDE_1         = 5,
        ACTOR_503500_ARM_KNOCKBACK_SOUND               = 7,
    };

    SVECTOR              armPosition;
    MATRIX               worldRotation;
    MATRIX               inversePlayerRotation;
    Enemy*               enemy;
    _Actor503500ArmWork* work;
    GfxCoord*            bossCoord;
    Task*                playerTask;
    GfxCoord*            playerCoord;
    const s32*           rotationSourceWords;
    s32*                 rotationDestinationWords;
    s32                  contactIndex;
    s32                  rotationWordIndex;
    s32                  fallDirection;
    s32                  audioPan;

    /// Copies 18 Q12 rotation bytes without alignment or translation.
    ///
    /// Captures bossCoord, work, rotationSourceWords, rotationDestinationWords
    /// and rotationWordIndex; requires this function's four-word bound. Matrices must
    /// be live, disjoint and word-aligned. Invoke only in a braced block.
#define ACTOR_503500_ARM_COPY_KNOCKBACK_ROTATION()                                                                         \
    rotationSourceWords      = (const s32*)&bossCoord->coord;                                                              \
    rotationDestinationWords = (s32*)&work->knockbackRotation;                                                             \
    for (rotationWordIndex = 0; rotationWordIndex < ACTOR_503500_ARM_KNOCKBACK_ROTATION_WORD_COUNT; rotationWordIndex++) { \
        *rotationDestinationWords++ = *rotationSourceWords++;                                                              \
    }                                                                                                                      \
    work->knockbackRotation.m[2][2] = bossCoord->coord.m[2][2];

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            playerCoord = playerTask->extra.tmd->coords;
            if (((GameActor*)playerTask->work)->mode != GAME_ACTOR_MODE_SCRIPTED &&
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_503500_80171544, 0) == 0) {
                // Build the push direction from the boss, preserving translation.
                bossCoord = task->parent->extra.tmd->coords;
                ACTOR_503500_ARM_COPY_KNOCKBACK_ROTATION();
                if (work->side != 0) {
                    RotMatrixY(ACTOR_503500_ARM_KNOCKBACK_TURN, &work->knockbackRotation);
                    bossCoord = &task->parent->extra.tmd->coords[ACTOR_503500_ARM_KNOCKBACK_PART_SIDE_1];
                } else {
                    RotMatrixY(-ACTOR_503500_ARM_KNOCKBACK_TURN, &work->knockbackRotation);
                    bossCoord = &task->parent->extra.tmd->coords[ACTOR_503500_ARM_KNOCKBACK_PART_SIDE_0];
                }
                // Select a fall animation by the arm point rotated into player axes.
                gfxComposeNodeWorldTransform(bossCoord, &worldRotation, &armPosition);
                gte_TransposeMatrix(&playerCoord->coord, &inversePlayerRotation);
                gte_SetRotMatrix(&inversePlayerRotation);
                gte_ldv0(&armPosition);
                gte_rtv0();
                gte_stsv(&armPosition);
                fallDirection = armPosition.vz >= 0;
                taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0);
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_801714E0[fallDirection], 0);
                // The child copies this borrowed rotation on its first task update.
                taskSpawnFromTable(&D_actor_503500_8017146C, 0, fallDirection, &work->knockbackRotation);
                Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                audioPan           = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(SOUND_COMMON(ACTOR_503500_ARM_KNOCKBACK_SOUND), audioPan, (s8)(worldCoordGetOriginAudioDepth(playerCoord) / 2));
            }
        }
    }
#undef ACTOR_503500_ARM_COPY_KNOCKBACK_ROTATION
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

/// Updates an arm's frame hook, reactions, target/strike contacts and behavior.
///
/// Requires initialized arm work, its live model, enemy and boss parent.
/// Paused updates run only the hook when visible; hidden updates suppress
/// drawing and lock-on. Active updates clear reactions, run the hook, process
/// contacts and then step behavior. The hook currently has no effect.
static void _actor503500ArmUpdate(Task* task)
{
    Enemy*     enemy;
    TmdObject* model;
    s32        actorControl;

    enemy        = task->spawnArg2.pointer;
    actorControl = gSceneCombatState.actorControl;
    model        = task->extra.tmd;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                _actor503500ArmFrameHook(task);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                _actor503500ArmClearReactions(task, actorControl, enemy);
            }
            _actor503500ArmFrameHook(task);
            _actor503500ArmUpdateHits(task);
            _actor503500ArmStepState(task);
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

/// Ages an arm's hit cooldown and handles its target and shared strike contacts.
///
/// Requires initialized work with marked eight-entry target and four-entry
/// strike tables. Cooldown decrements in signed halfword frames and floors at
/// zero. Defeat suppresses hits and player knock-back; both tables are always
/// cleared after processing. The two strike spheres share the strike table.
static void _actor503500ArmUpdateHits(Task* task)
{
    _Actor503500ArmWork* work;

    work = task->work;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500ArmApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
        _actor503500ArmApplyAttackContacts(task, work->attackContacts, ARRAY_SIZE(work->attackContacts));
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

/// Steps an arm's idle, strike, dying or target-exposure behavior.
///
/// Requires initialized arm work and a live boss parent. Dispatches the work's
/// behavior state, independently of `Task::state`; other values do nothing.
static void _actor503500ArmStepState(Task* task)
{
    _Actor503500ArmWork* work = task->work;

    switch (work->state) {
        case ACTOR_503500_ARM_STATE_IDLE:
            _actor503500ArmStepIdle(task);
            break;
        case ACTOR_503500_ARM_STATE_STRIKE:
            _actor503500ArmStepStrike(task);
            break;
        case ACTOR_503500_ARM_STATE_DYING:
            _actor503500ArmStepDying(task);
            break;
        case ACTOR_503500_ARM_STATE_BECOME_TARGET:
            _actor503500ArmStepBecomeTarget(task);
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
