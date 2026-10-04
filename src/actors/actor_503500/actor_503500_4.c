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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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
    ACTOR_503500_ARM_STRIKE_RATE_SLOW = 0xC,  // Strike animation rate while the slot enemy shielding the arm lives, in sixteenths
    ACTOR_503500_ARM_STRIKE_RATE_FAST = 0x12, // Strike animation rate once that enemy is destroyed
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
/// become a target, which it does once the slot enemy shielding that arm
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

/// Work block of the knock-back task `func_actor_503500_801437D0` spawns
/// (`memFillBytes(_, 0, 0x38)` in `func_actor_503500_80143AC0`). `rot` is a copy of the
/// rotation handed over in `Task::spawnArg2`; every frame `speed` is pushed
/// through it by `ApplyMatrixLV` and added onto the 16.16 `pos`, whose integer
/// halves go to the player as `GAME_ACTOR_MESSAGE_MOVE_BY`. `field_34` counts frames spent at
/// zero speed and `field_36` the remaining camera-shake frames.
typedef struct Actor503500Work38 {
    /* 0x00 */ Actor503500FixedVector pos;
    /* 0x10 */ MATRIX                 rot;
    /* 0x30 */ s32                    speed;
    /* 0x34 */ s16                    field_34;
    /* 0x36 */ s16                    field_36;
} Actor503500Work38;
STATIC_ASSERT_SIZEOF(Actor503500Work38, 0x38);

/// Work block of the 0xF4 enemy whose state-0 init is
/// `func_actor_503500_8013ECBC` (`D_actor_503500_80177A6C`). It keeps a
/// halfword at 0xEC -- `func_actor_503500_8013F4A4` stores one there and
/// `func_actor_503500_8013F9D4` clears it -- where the 0xF0 blocks keep
/// bytes. Its sub-state index is `field_F0`, the one
/// `func_actor_503500_8013F8AC` dispatches on.
typedef struct Actor503500WorkF4 {
    /* 0x00 */ WorldCollisionBody    obj; // the collision body Gp_UnlinkObj takes
    /* 0x20 */ WorldCollisionContact rec[8];
    /* 0xE0 */ EffectSpawnArg        field_E0;
    /* 0xE8 */ s16                   field_E8; // per-frame countdown, clamped at 0
    /* 0xEA */ u16                   field_EA; // sub-state frame counter
    /* 0xEC */ s16                   field_EC;
    /* 0xEE */ byte                  pad_EE[0x2];
    /* 0xF0 */ s8                    field_F0; // sub-state index
    /* 0xF1 */ s8                    field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte                  pad_F2[0x2];
} Actor503500WorkF4;
STATIC_ASSERT_SIZEOF(Actor503500WorkF4, 0xF4);

/// The second 0xF4 block: the enemy whose state-0 init is
/// `func_actor_503500_8013CAE4` (`memFillBytes` over slot `spawnArg1 - 7` of
/// `D_actor_503500_80177794`). It is named after its array because size does
/// not tell the 0xF4 shapes apart: this one keeps its two sub-state
/// counters as halfwords at 0xEC and 0xEE -- `func_actor_503500_8013D1CC` and
/// `func_actor_503500_8013D558` step 0xEC and `func_actor_503500_8013DBA8`
/// clears both -- where the 0xF0 blocks keep the bytes they dispatch on, and
/// where `Actor503500WorkF4` puts its counter pair at 0xEA / 0xEC. Its sub-state index is `field_F0`,
/// the one `func_actor_503500_8013D990` dispatches on.
typedef struct Actor503500Work770E8 {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[8];   // Gp_InitRec18Table(rec, 8, 0)
    /* 0xE0 */ EffectSpawnArg        field_E0; // record the block's effects are spawned with
    /* 0xE8 */ s16                   field_E8; // per-frame countdown, clamped at 0
    /* 0xEA */ s16                   field_EA; // frames waited in sub-state 1 before the respawn
    /* 0xEC */ u16                   field_EC; // sub-state frame counter
    /* 0xEE */ s16                   field_EE;
    /* 0xF0 */ s8                    field_F0; // sub-state index
    /* 0xF1 */ s8                    field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte                  pad_F2[0x2];
} Actor503500Work770E8;
STATIC_ASSERT_SIZEOF(Actor503500Work770E8, 0xF4);

/// The first 0xF4 block: the enemy whose state-0 init is
/// `func_actor_503500_8013BEE4` (`memFillBytes` over `D_actor_503500_801776A0`).
/// Its death sub-state `func_actor_503500_8013C558` keeps three halfwords at 0xEA /
/// 0xEC / 0xEE -- a frame counter, a scale that shrinks from 0x1000 and the
/// step it shrinks by. Its sub-state index is `field_F0`, the one
/// `func_actor_503500_8013CA34` dispatches on.
typedef struct Actor503500Work776A0 {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[8];   // Gp_InitRec18Table(rec, 8, 0)
    /* 0xE0 */ EffectSpawnArg        field_E0; // record the block's effects are spawned with
    /* 0xE8 */ s16                   field_E8; // per-frame countdown, clamped at 0
    /* 0xEA */ s16                   field_EA; // sub-state frame counter
    /* 0xEC */ u16                   field_EC; // scale handed to func_actor_503500_80135E20
    /* 0xEE */ s16                   field_EE; // per-frame step of field_EC, stepped down by 4
    /* 0xF0 */ s8                    field_F0; // sub-state index
    /* 0xF1 */ s8                    field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte                  pad_F2[0x2];
} Actor503500Work776A0;
STATIC_ASSERT_SIZEOF(Actor503500Work776A0, 0xF4);

/// Element of `D_actor_503500_801774C0`, the two 0xF0 blocks
/// `func_actor_503500_8013AD64` clears for spawn slots 4 and 5; the task's
/// `Task::work` points at its element. `field_EC` holds
/// the slot (`spawnArg1 - 4`) that selects this enemy's parent part and
/// local offset.
typedef struct Actor503500Work774C0 {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[8];
    /* 0xE0 */ EffectSpawnArg        field_E0; // record the block's effects are spawned with
    /* 0xE8 */ s16                   field_E8; // per-frame countdown, clamped at 0
    /* 0xEA */ s16                   field_EA; // sub-state frame counter
    /* 0xEC */ s8                    field_EC; // slot, 0 or 1
    /* 0xED */ s8                    field_ED; // sub-state index
    /* 0xEE */ s8                    field_EE; // sub-state phase, cleared with field_ED
    /* 0xEF */ byte                  pad_EF[0x1];
} Actor503500Work774C0;
STATIC_ASSERT_SIZEOF(Actor503500Work774C0, 0xF0);

/// Work block of the 0xF0 enemy whose state-0 init is
/// `func_actor_503500_8013DD10` (`D_actor_503500_8017797C`). It is the size of
/// `Actor503500Work774C0` but keeps no slot byte: its sub-state index, the one
/// `func_actor_503500_8013EB60` dispatches on, sits at 0xEC and the phase
/// follows it.
typedef struct Actor503500Work7797C {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[8];   // Gp_InitRec18Table(rec, 8, 0)
    /* 0xE0 */ EffectSpawnArg        field_E0; // record the block's effects are spawned with
    /* 0xE8 */ s16                   field_E8; // per-frame countdown, clamped at 0
    /* 0xEA */ s16                   field_EA; // sub-state frame counter
    /* 0xEC */ s8                    field_EC; // sub-state index
    /* 0xED */ s8                    field_ED; // sub-state phase, cleared with field_EC
    /* 0xEE */ byte                  pad_EE[0x2];
} Actor503500Work7797C;
STATIC_ASSERT_SIZEOF(Actor503500Work7797C, 0xF0);

extern Actor503500WorkF4 D_actor_503500_80177A6C;

extern Actor503500Work776A0 D_actor_503500_801776A0;

static void func_actor_503500_80142220(SVECTOR* angles, GfxCoord* nodes);

extern Actor503500Work7797C D_actor_503500_8017797C;

extern Actor503500Work774C0 D_actor_503500_801774C0[2];

static void func_actor_503500_8013BC54(Task* arg0);

extern Actor503500Work770E8 D_actor_503500_80177794[2];
static void                 func_actor_503500_8013D85C(Task* arg0);

static void func_actor_503500_8013AF60(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* rec, s32 count);
static void func_actor_503500_8013CCBC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* rec, s32 count);
static void func_actor_503500_8013C088(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* rec, s32 count);
static void func_actor_503500_8013DEB4(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* rec, s32 count);
static void func_actor_503500_8013EA2C(Task* arg0);
static void func_actor_503500_8013EC20(Task* arg0, s32 arg1);
static void func_actor_503500_801431EC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void func_actor_503500_80140D38(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void func_actor_503500_8014215C(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void func_actor_503500_801437D0(Task* arg0, WorldCollisionContact* arg1, s32 arg2);
static void func_actor_503500_8013B460(Task* arg0);
static void func_actor_503500_8013B8D0(Task* arg0);
static void func_actor_503500_8013BE0C(Task* arg0);
static void func_actor_503500_8013BCB4(Task* arg0);
static void func_actor_503500_8013C900(Task* arg0);
static void func_actor_503500_8013C9DC(Task* arg0);
static void func_actor_503500_8013C960(Task* arg0);
static void func_actor_503500_8013CA34(Task* arg0);
static void func_actor_503500_8013CA74(Task* arg0, s8 arg1);
static void func_actor_503500_8013D8BC(Task* arg0);
static void func_actor_503500_8013D914(Task* arg0);
static void func_actor_503500_8013D990(Task* arg0);
static void func_actor_503500_8013DA2C(Task* arg0, s32 arg1);
static void func_actor_503500_8013DC4C(Task* arg0);
static void func_actor_503500_8013F8AC(Task* arg0);
static void func_actor_503500_801440F0(Task* arg0);
static void func_actor_503500_8013BD88(Task* arg0);
static void func_actor_503500_8013C558(Task* arg0);
static void func_actor_503500_8013E384(Task* arg0);
static void func_actor_503500_8013E740(Task* arg0);
static void func_actor_503500_8013EBE4(Task* arg0);
static void func_actor_503500_8013EA8C(Task* arg0);
static void func_actor_503500_8013EAE4(Task* arg0);
static void func_actor_503500_8013EB60(Task* arg0);
static void func_actor_503500_8013F778(Task* arg0);
static void func_actor_503500_8013F7D8(Task* arg0);
static void func_actor_503500_8013F830(Task* arg0);
static void func_actor_503500_8013DBA8(Task* arg0, s32 arg1);
static void func_actor_503500_8013F328(Task* arg0);
static void func_actor_503500_8013F4A4(Task* arg0);
static void func_actor_503500_8013F948(Task* arg0);
static void func_actor_503500_8013F984(Task* arg0);
static void func_actor_503500_8013F9D4(Task* arg0, s32 arg1);
static void func_actor_503500_801400A4(Task* arg0);
static void func_actor_503500_80140654(Task* arg0);
static void func_actor_503500_80140BE8(Task* arg0);
static void func_actor_503500_80141248(Task* arg0);
static void func_actor_503500_80141448(Task* arg0);
static void func_actor_503500_80141B94(Task* arg0);
static void func_actor_503500_80141D7C(Task* arg0);
static void func_actor_503500_80141E64(Task* arg0);
static void func_actor_503500_80141F48(Task* arg0);
static void func_actor_503500_80141FC8(Task* arg0);
static void func_actor_503500_801420C4(Task* arg0);
static void func_actor_503500_801421A8(Task* arg0);
static void func_actor_503500_80142310(Task* arg0, s32 arg1);
static void func_actor_503500_8014271C(Task* arg0);
static void func_actor_503500_80142980(Task* arg0);
static void func_actor_503500_80143FFC(Task* arg0);
static void func_actor_503500_80144004(Task* arg0);
static void func_actor_503500_80144098(Task* arg0, s32 arg1, Enemy* arg2);
static void func_actor_503500_8014418C(Task* arg0);
static void func_actor_503500_801441E8(Task* arg0);
static void func_actor_503500_80144238(Task* arg0, s32 arg1);
static void func_actor_503500_80144520(Task* arg0);
static void func_actor_503500_80144778(Task* arg0);
static void func_actor_503500_80144B40(Task* arg0);
static void func_actor_503500_80144E10(Task* arg0);
static void func_actor_503500_8013BE48(Task* arg0, s32 arg1);
static void func_actor_503500_8013B60C(Task* arg0, s32 side, s32 arg2);

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    _Actor503500ArmWork value[2];
    u8                  retained[8];
} Actor5035004Storage8AC0;
STATIC_ASSERT_SIZEOF(Actor5035004Storage8AC0, 1104);

static void func_actor_503500_80143F78(Task* arg0);
static void func_actor_503500_8014473C(Task* arg0);
static void func_actor_503500_80144DA8(Task* arg0);

static void func_actor_503500_80141D04(Task* arg0);

extern _Actor503500LungingChainWork D_actor_503500_80177B60[];

extern Actor5035004Storage8AC0 D_actor_503500_80178AC0;

/// Knock-back work block of `func_actor_503500_80143AC0`.
extern Actor503500Work38 D_actor_503500_80178F10;

static void func_actor_503500_8013AD64(Task* arg0);
static void func_actor_503500_8013BBCC(Task* arg0);
static void func_actor_503500_8013BEE4(Task* arg0);
static void func_actor_503500_8013C878(Task* arg0);
static void func_actor_503500_8013CAE4(Task* arg0);
static void func_actor_503500_8013D7D4(Task* arg0);
static void func_actor_503500_8013DD10(Task* arg0);
static void func_actor_503500_8013E9A4(Task* arg0);
static void func_actor_503500_8013ECBC(Task* arg0);
static void func_actor_503500_8013F6F0(Task* arg0);
static void func_actor_503500_8013FA74(Task* arg0);
static void func_actor_503500_8013FF0C(Task* arg0);
static void func_actor_503500_801423C8(Task* arg0);
static void func_actor_503500_80143EB4(Task* arg0);
static void func_actor_503500_80144300(Task* arg0);
static void func_actor_503500_801446E4(Task* arg0);
static void func_actor_503500_801448E8(Task* arg0);
static void func_actor_503500_80144D50(Task* arg0);

static void func_actor_503500_8013BD0C(Task* arg0);
static void func_actor_503500_8014176C(SVECTOR* pts, GfxCoord* coords);

/// `Task::state` handlers `func_actor_503500_8013BE8C` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131FF0 = {
    {
        func_actor_503500_8013AD64,
        func_actor_503500_8013BBCC,
        func_actor_503500_8013BC54,
    },
};

Actor503500Work774C0 D_actor_503500_801774C0[2] = { 0 };

Actor503500Work776A0 D_actor_503500_801776A0 = { 0 };

Actor503500Work770E8 D_actor_503500_80177794[2] = { 0 };

Actor503500Work7797C D_actor_503500_8017797C = { 0 };

Actor503500WorkF4 D_actor_503500_80177A6C = { 0 };

_Actor503500LungingChainWork D_actor_503500_80177B60[4];

Actor5035004Storage8AC0 D_actor_503500_80178AC0;

Actor503500Work38 D_actor_503500_80178F10;

static void func_actor_503500_8013D1CC(Task* arg0);
static void func_actor_503500_8013D558(Task* arg0);
static void func_actor_503500_8013EE5C(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);

/// State-0 init of the 0xF0 enemies in spawn slots 4 and 5: clears the slot's
/// block in `D_actor_503500_801774C0`, hangs the task's coordinate off the
/// parent part the slot names, links the enemy and its display node, and
/// hands the task to its exit callback.
static void func_actor_503500_8013AD64(Task* arg0)
{
    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    MATRIX*                mtx;
    WorldCollisionContact* rec;
    Actor503500Work774C0*  work;
    s32                    idx;

    idx    = arg0->spawnArg1.value - 4;
    enemy  = arg0->spawnArg2.pointer;
    parent = arg0->parent;
    work   = &D_actor_503500_801774C0[idx];
    coord  = arg0->extra.tmd->coords;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work     = work;
    work->field_EC = idx;

    coord->parent                    = &parent->extra.tmd->coords[D_actor_503500_8016F0E8[idx]];
    MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
    mtx                              = &coord->coord;
    MATRIX_PAIR(mtx, 0, 2)           = 0;
    MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
    MATRIX_PAIR(mtx, 2, 0)           = 0;
    mtx->m[2][2]                     = 0x1000;
    enemy->field_4                   = mtx;
    enemy->field_48                  = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                   = coord;
    enemy->node.state.parts.flags |= (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    enemy->bodyPos.vx              = D_actor_503500_8016F0F0[idx].vx;
    enemy->bodyPos.vy              = D_actor_503500_8016F0F0[idx].vy;
    enemy->bodyPos.vz              = D_actor_503500_8016F0F0[idx].vz;
    rec                            = work->rec;
    enemy->param                   = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                    = rec;
    enemy->hp                      = enemy->param->hpMax;

    work->obj.coord            = coord;
    work->obj.context.contacts = rec;
    work->obj.pos.vx           = D_actor_503500_8016F0F0[idx].vx;
    work->obj.pos.vy           = D_actor_503500_8016F0F0[idx].vy;
    work->obj.pos.vz           = D_actor_503500_8016F0F0[idx].vz;
    work->obj.key              = 0x30023;
    work->obj.radius           = 0x5DC;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_E0.spawnArgLo = 0x600;
    work->field_E0.coord      = coord;
    work->field_E0.spawnArgHi = 3;
    work->obj.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    MoveImage(&D_actor_503500_8016F100, 0, 0x105);
    arg0->exitCallback = func_actor_503500_8013BC54;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `rec[0..count)`,
/// like `func_actor_503500_80137C90`: each attack id is taken once, only
/// type-2 ids land while the `field_E8` countdown is clear, and a hit that
/// empties `field_40` starts state 2. The hit effect is pulled to 1600 units
/// along the contact offset and placed at the `field_EC` sub-state's offset.
/// `arg1` is passed by the caller but unused.
static void func_actor_503500_8013AF60(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* rec, s32 count)
{
    MATRIX                mtx;
    MATRIX                rot;
    VECTOR                d;
    SVECTOR               pos;
    Actor503500Work774C0* work;
    Enemy*                enemy;
    GfxCoord*             coord;
    GfxCoord*             src;
    s16                   stun;
    u32                   id;
    s32                   dmg;
    s32                   crit;
    s32                   scale;
    s32                   i;
    s32                   j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < count; i++) {
        id = rec[i].key.value;
        for (j = 0; j < i; j++) {
            if (rec[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013BE48(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                Gp_SetObjFlag2(enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4(enemy, id, 0);
                break;
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = rec[i].point.vx - coord->workm.t[0];
        pos.vy = rec[i].point.vy - coord->workm.t[1];
        pos.vz = rec[i].point.vz - coord->workm.t[2];
        scale  = 0x640000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F0F0[work->field_EC].vx;
        pos.vy += D_actor_503500_8016F0F0[work->field_EC].vy;
        pos.vz += D_actor_503500_8016F0F0[work->field_EC].vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

static void func_actor_503500_8013B460(Task* arg0)
{
    Actor503500Work774C0* work;
    s16                   angle;
    s32                   offset;
    s32                   side;

    work = arg0->work;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013BE48(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1.value);
        return;
    }
    switch (work->field_EE) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 9, 0x10);
            work->field_EE++;
            break;
        case 1:
            work->field_EA++;
            if (work->field_EA >= 0x80) {
                offset = -0x12C;
                angle  = func_actor_503500_80136134(arg0->parent);
                side   = work->field_EC;
                if (side != 0) {
                    offset = 0x12C;
                }
                if (angle > -0x400 - offset && angle < 0x400 - offset) {
                    func_actor_503500_8013B60C(arg0, side, 0);
                }
                if (angle < offset - 0x400 || offset + 0x400 < angle) {
                    func_actor_503500_8013B60C(arg0, side, 1);
                }
                work->field_EE++;
            }
        case 2:
            if (func_actor_503500_80136014(arg0->parent, 9) != 0) {
                func_actor_503500_8013BE48(arg0, 0);
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
    task = Task_SpawnFromTable(D_actor_503500_8016E9F0, 1, 0, 0xA00000);
    if (task != NULL) {
        Gp_ComposeParentWorld(src, &m, &pos);
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

/// Death state of the 0xF0 block, stepped by `field_EE`: phase 0 is the death
/// setup shared with `func_actor_503500_8013F4A4`; phase 1 spawns a mirrored
/// pair of effects per frame from `D_actor_503500_8016F168`, drifting with the
/// frame count, moves the side's VRAM rects on frame 0x14 and leaves at 0x1F.
static void func_actor_503500_8013B8D0(Task* arg0)
{
    Enemy*                enemy;
    Actor503500Work774C0* work;
    GfxCoord*             coord;
    SVECTOR               vec;
    s32                   pan;
    s32                   side;
    s32                   n;
    s32                   i;
    s32                   t;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_EE) {
        case 0:
            work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs      = 0;
            worldTargetUnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->field_E8 = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            pan                   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(SOUND_BRAHMAN_DEATH_LOOP, pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->field_EE++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1.value, 3) != 0) {
                i       = (s16)(work->field_EA % 9);
                vec.vx  = D_actor_503500_8016F168[i].vx;
                vec.vy  = D_actor_503500_8016F168[i].vy;
                vec.vz  = D_actor_503500_8016F168[i].vz;
                vec.vx -= work->field_EA * 10;
                vec.vy -= work->field_EA * 20;
                if (work->field_EC != 0) {
                    t      = vec.vx;
                    vec.vx = -t;
                }
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x1800, &vec);
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x80008600, &vec);
                t      = vec.vz;
                vec.vz = -t;
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x1800, &vec);
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x80008600, &vec);
            }
            work->field_EA++;
            if (work->field_EA >= 0x1F) {
                SndEvt_EnqueueType7(SOUND_BRAHMAN_DEATH_LOOP, 0x2D);
                work->field_EE++;
            } else if (work->field_EA == 0x14) {
                side = work->field_EC;
                n    = 2;
                if (side != 0) {
                    n = 4;
                }
                MoveImage(&D_actor_503500_8016F148[side][0], (n << 6) + 0x140, 0x100);
                MoveImage(&D_actor_503500_8016F148[side][1], 0, n + 0xF7);
            }
            break;
        default:
            func_actor_503500_8013611C(arg0->spawnArg1.value);
            arg0->state++;
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
        func_actor_503500_8013BCB4(arg0);
    }
    func_actor_503500_8013BD0C(arg0);
    func_actor_503500_8013BD88(arg0);
}

static void func_actor_503500_8013BC54(Task* arg0)
{
    Enemy* enemy;

    enemy                           = arg0->spawnArg2.pointer;
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work774C0*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_8013BCB4(Task* arg0)
{
    Enemy* enemy;

    enemy = arg0->spawnArg2.pointer;
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

static void func_actor_503500_8013BD0C(Task* arg0)
{
    Actor503500Work774C0* work;
    s16                   timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013AF60(arg0, &work->obj, work->rec, 8);
    }
    Gp_ClearRec18Occupied(work->rec);
}

static void func_actor_503500_8013BD88(Task* arg0)
{
    switch (((Actor503500Work774C0*)arg0->work)->field_ED) {
        case 0:
            func_actor_503500_8013BE0C(arg0);
            break;
        case 1:
            func_actor_503500_8013B460(arg0);
            break;
        case 2:
            func_actor_503500_8013B8D0(arg0);
            break;
    }
}

/// The 0xF0 block's counterpart of `func_actor_503500_80138454`: when a kill is
/// pending, hands the block to sub-state 1 and cancels the countdown.
static void func_actor_503500_8013BE0C(Task* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013BE48(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// The 0xF0 block's counterpart of `func_actor_503500_80138490`: puts the block
/// into sub-state `arg1`, clears the phase and frame counter that go with it,
/// cancels a pending kill, and reports the slot busy to the boss when the
/// sub-state is non-zero.
static void func_actor_503500_8013BE48(Task* arg0, s32 arg1)
{
    Actor503500Work774C0* work = arg0->work;

    work->field_ED      = arg1;
    work->field_EE      = 0;
    work->field_EA      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1.value, arg1 != 0);
}

void func_actor_503500_8013BE8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131FF0;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013CA8C` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132028 = {
    {
        func_actor_503500_8013BEE4,
        func_actor_503500_8013C878,
        func_actor_503500_8013C900,
    },
};

/// State-0 init of the 0xF4 enemy at `D_actor_503500_801776A0`, the twin of
/// `func_actor_503500_8013ECBC`: clears the block, resets the task's own
/// coordinate to a plain 4096 identity, parents it to part 16 of the parent
/// task's model, links the enemy node and its display node, and starts the
/// block in sub-state 0.
static void func_actor_503500_8013BEE4(Task* arg0)
{
    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    GfxCoord*              parts;
    MATRIX*                mtx;
    WorldCollisionContact* rec;

    coord  = arg0->extra.tmd->coords;
    enemy  = arg0->spawnArg2.pointer;
    parent = arg0->parent;
    memFillBytes(&D_actor_503500_801776A0, 0, sizeof(D_actor_503500_801776A0));
    arg0->work = &D_actor_503500_801776A0;

    parts                            = parent->extra.tmd->coords;
    MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
    coord->parent                    = &parts[16];
    mtx                              = &coord->coord;
    MATRIX_PAIR(mtx, 0, 2)           = 0;
    MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
    MATRIX_PAIR(mtx, 2, 0)           = 0;
    mtx->m[2][2]                     = 0x1000;
    enemy->field_4                   = mtx;
    enemy->field_48                  = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F1B0.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F1B0.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F1B0.vz;
    rec                           = D_actor_503500_801776A0.rec;
    enemy->param                  = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                   = rec;
    enemy->hp                     = enemy->param->hpMax;

    D_actor_503500_801776A0.obj.coord            = coord;
    D_actor_503500_801776A0.obj.context.contacts = rec;
    D_actor_503500_801776A0.obj.key              = 0x30023;
    D_actor_503500_801776A0.obj.radius           = 0x3E8;
    D_actor_503500_801776A0.obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    D_actor_503500_801776A0.obj.pos.vx           = D_actor_503500_8016F1B0.vx;
    D_actor_503500_801776A0.obj.pos.vy           = D_actor_503500_8016F1B0.vy;
    D_actor_503500_801776A0.obj.pos.vz           = D_actor_503500_8016F1B0.vz;
    Gp_LinkObj(2, &D_actor_503500_801776A0.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_801776A0.field_E0.spawnArgLo = 0x600;
    D_actor_503500_801776A0.field_E0.coord      = coord;
    D_actor_503500_801776A0.field_E0.spawnArgHi = 3;
    D_actor_503500_801776A0.obj.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    func_actor_503500_8013CA74(arg0, 0);
    arg0->exitCallback = func_actor_503500_8013C900;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy - the same body as `func_actor_503500_80137C90` on this block's
/// fields: each attack id is taken once, only type-2 ids land while the
/// `field_E8` countdown is clear, and a hit that empties `field_40` starts the
/// death sub-state. The hit effect is pulled to 1400 units along the contact
/// offset. `arg1` is passed by the caller but unused.
static void func_actor_503500_8013C088(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    VECTOR                d;
    SVECTOR               pos;
    MATRIX                mtx;
    MATRIX                rot;
    Actor503500Work776A0* work;
    Enemy*                enemy;
    GfxCoord*             coord;
    GfxCoord*             src;
    s16                   stun;
    u32                   id;
    s32                   dmg;
    s32                   crit;
    s32                   scale;
    s32                   i;
    s32                   j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key.value;
        for (j = 0; j < i; j++) {
            if (arg2[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013CA74(arg0, 1);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                Gp_SetObjFlag2(enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4(enemy, id, 0);
                break;
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x578000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F1B0.vx;
        pos.vy += D_actor_503500_8016F1B0.vy;
        pos.vz += D_actor_503500_8016F1B0.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Death sub-state of the first 0xF4 block, stepped by `field_F1`: phase 0
/// is the death setup shared with `func_actor_503500_8013D558` and seeds the
/// shrinking scale; phase 1 spawns three effects at random offsets for up to
/// 2000 frames while the scale shrinks by an ever smaller step, and leaves
/// once that step runs out.
static void func_actor_503500_8013C558(Task* arg0)
{
    Actor503500Work776A0* work;
    GfxCoord*             coord;
    SVECTOR               vec;
    s32                   pan;

    work  = (Actor503500Work776A0*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_F1) {
        case 0:
            work->obj.flags                        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            ((Enemy*)arg0->spawnArg2.pointer)->recs = 0;
            worldTargetUnlinkNode(&((Enemy*)arg0->spawnArg2.pointer)->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->field_E8 = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            ((Enemy*)arg0->spawnArg2.pointer)->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            pan                                               = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(SOUND_BRAHMAN_DEATH_LOOP, pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->field_EC = 0x1000;
            work->field_EE = 0x80;
            work->field_F1++;
            break;
        case 1:
            if (++work->field_EA <= 2000) {
                if (func_actor_503500_801360BC(arg0->spawnArg1.value, 5) != 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01001900,
                                &D_actor_503500_8016F1B8[(u16)((gRandomLcgState >> 16) % 18)]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01001700,
                                &D_actor_503500_8016F1B8[(u16)((gRandomLcgState >> 16) % 18)]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, 0x01404600,
                                &D_actor_503500_8016F1B8[(u16)((gRandomLcgState >> 16) % 18)]);
                }
            }
            vec.vx = 0x1000;
            vec.vy = 0x1000;
            vec.vz = work->field_EC;
            func_actor_503500_80135E20(arg0->parent, 0x10, &vec);
            work->field_EC -= work->field_EE;
            work->field_EE -= 4;
            if (work->field_EE <= 0) {
                SndEvt_EnqueueType7(SOUND_BRAHMAN_DEATH_LOOP, 0x2D);
                func_actor_503500_8013611C(arg0->spawnArg1.value);
                work->field_F1++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// Per-frame tick of the first 0xF4 block, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
static void func_actor_503500_8013C878(Task* arg0)
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
        func_actor_503500_8013C9DC(arg0);
    }
    func_actor_503500_8013C960(arg0);
    func_actor_503500_8013CA34(arg0);
}

static void func_actor_503500_8013C900(Task* arg0)
{
    Enemy* enemy;

    enemy                           = arg0->spawnArg2.pointer;
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work776A0*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_8013C960(Task* arg0)
{
    Actor503500Work776A0* work;
    s16                   timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013C088(arg0, &work->obj, work->rec, 8);
    }
    Gp_ClearRec18Occupied(work->rec);
}

static void func_actor_503500_8013C9DC(Task* arg0)
{
    Enemy* enemy = arg0->spawnArg2.pointer;

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

static void func_actor_503500_8013CA34(Task* arg0)
{
    switch (((Actor503500Work776A0*)arg0->work)->field_F0) {
        case 0:
            break;
        case 1:
            func_actor_503500_8013C558(arg0);
            break;
    }
}

static void func_actor_503500_8013CA74(Task* arg0, s8 arg1)
{
    Actor503500Work776A0* work;

    work           = arg0->work;
    work->field_F0 = arg1;
    work->field_F1 = 0;
    work->field_EA = 0;
}

void func_actor_503500_8013CA8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132028;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013DBF4` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132060 = {
    {
        func_actor_503500_8013CAE4,
        func_actor_503500_8013D7D4,
        func_actor_503500_8013D85C,
    },
};

/// State-0 init of the 0xF4 enemy in slot `spawnArg1` of
/// `D_actor_503500_80177794`, the same shape as `func_actor_503500_8013BEE4`
/// but without linking the enemy node.
static void func_actor_503500_8013CAE4(Task* arg0)
{
    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    MATRIX*                mtx;
    GfxCoord*              parts;
    WorldCollisionContact* rec;
    Actor503500Work770E8*  work;
    SVECTOR*               pos;
    s32                    idx;

    idx    = arg0->spawnArg1.value;
    enemy  = arg0->spawnArg2.pointer;
    work   = &D_actor_503500_80177794[idx - 7];
    pos    = &D_actor_503500_8016F248[idx - 7];
    coord  = arg0->extra.tmd->coords;
    parent = arg0->parent;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work = work;

    parts                            = parent->extra.tmd->coords;
    MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
    coord->parent                    = &parts[1];
    mtx                              = &coord->coord;
    MATRIX_PAIR(mtx, 0, 2)           = 0;
    MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
    MATRIX_PAIR(mtx, 2, 0)           = 0;
    mtx->m[2][2]                     = 0x1000;
    enemy->field_4                   = mtx;
    enemy->field_48                  = 0;
    enemy->coord                     = coord;
    enemy->node.state.parts.flags    = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx                = pos->vx;
    enemy->bodyPos.vy                = pos->vy;
    enemy->bodyPos.vz                = pos->vz;
    rec                              = work->rec;
    enemy->param                     = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                      = rec;
    enemy->hp                        = enemy->param->hpMax;

    work->obj.coord            = coord;
    work->obj.context.contacts = rec;
    work->obj.pos.vx           = pos->vx;
    work->obj.pos.vy           = pos->vy;
    work->obj.pos.vz           = pos->vz;
    work->obj.key              = 0x30023;
    work->obj.radius           = 0x190;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_E0.spawnArgLo = 0x600;
    work->field_E0.coord      = coord;
    work->field_E0.spawnArgHi = 3;
    work->obj.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    func_actor_503500_8013DBA8(arg0, 0);
    arg0->exitCallback = func_actor_503500_8013D85C;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// this form - the same pass as `func_actor_503500_8013C088`: each attack id
/// is taken once, only type-2 ids land while the `field_E8` countdown is
/// clear, and a hit that empties `field_40` starts death sub-state 3. The hit
/// effect is pulled to 200 units along the contact offset and shifted by this
/// slot's `D_actor_503500_8016F248` entry. `arg1` is passed but unused.
static void func_actor_503500_8013CCBC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    VECTOR                d;
    SVECTOR               pos;
    MATRIX                mtx;
    MATRIX                rot;
    Actor503500Work770E8* work;
    Enemy*                enemy;
    GfxCoord*             coord;
    GfxCoord*             src;
    s16                   stun;
    u32                   id;
    s32                   dmg;
    s32                   crit;
    s32                   scale;
    s32                   i;
    s32                   j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key.value;
        for (j = 0; j < i; j++) {
            if (arg2[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013DBA8(arg0, 3);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                Gp_SetObjFlag2(enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4(enemy, id, 0);
                break;
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0xC8000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        {
            SVECTOR* offset = &D_actor_503500_8016F248[arg0->spawnArg1.value - 7];
            pos.vx         += offset->vx;
        }
        {
            SVECTOR* offset = &D_actor_503500_8016F248[arg0->spawnArg1.value - 7];
            pos.vy         += offset->vy;
        }
        {
            SVECTOR* offset = &D_actor_503500_8016F248[arg0->spawnArg1.value - 7];
            pos.vz         += offset->vz;
        }
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Sub-state 2 of the second 0xF4 block, stepped by `field_F1`: phase 0
/// applies the boss's preset 0xA; from frame 0x14 phase 1 spawns one child
/// from `D_actor_503500_8016E9F0`, placed at the offset
/// `D_actor_503500_8016F258` in the actor's frame and turned by
/// `D_actor_503500_8016F260` (both mirrored for slot 8); phase 2 hands off
/// once `func_actor_503500_80136014` reports preset 0xA flagged.
static void func_actor_503500_8013D1CC(Task* arg0)
{
    SVECTOR               pos;
    SVECTOR               ofs;
    MATRIX                m;
    Actor503500Work770E8* work;
    GfxCoord*             coord;
    GfxCoord*             dst;
    Task*                 task;
    s32*                  src;
    s32*                  out;
    s32                   i;
    s32                   t;

    work  = (Actor503500Work770E8*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013DBA8(arg0, 1);
        func_actor_503500_8013611C(arg0->spawnArg1.value);
        return;
    }
    switch (work->field_F1) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 0xA, 0x10);
            work->field_F1++;
            break;
        case 1:
            if ((s16)++work->field_EC < 0x14) {
                break;
            }
            task = Task_SpawnFromTable(D_actor_503500_8016E9F0, 1, 1, 0xC00000);
            if (task != NULL) {
                Gp_ComposeParentWorld(coord, &m, &pos);
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
            work->field_F1++;
            break;
        case 2:
            if (func_actor_503500_80136014(arg0->parent, 0xA) != 0) {
                func_actor_503500_8013DBA8(arg0, 1);
            }
            break;
    }
}

/// Sub-state 3 of the second 0xF4 block, stepped by `field_F1`: phase 0 is
/// the death setup shared with `func_actor_503500_8013F4A4`; phase 1 spawns an
/// effect per frame from a slot-dependent offset table for 6 frames, then a
/// final burst of three; the last phase keeps counting to 0x1F and leaves.
static void func_actor_503500_8013D558(Task* arg0)
{
    Actor503500Work770E8* work;
    GfxCoord*             coord;
    SVECTOR*              vec;
    s32                   pan;

    work  = (Actor503500Work770E8*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_F1) {
        case 0:
            work->obj.flags                        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            ((Enemy*)arg0->spawnArg2.pointer)->recs = 0;
            worldTargetUnlinkNode(&((Enemy*)arg0->spawnArg2.pointer)->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->field_E8 = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            ((Enemy*)arg0->spawnArg2.pointer)->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            pan                                               = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(SOUND_BRAHMAN_DEATH_LOOP, pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->field_F1++;
            break;
        case 1:
            if (arg0->spawnArg1.value == 7) {
                vec = D_actor_503500_8016F278;
            } else {
                vec = D_actor_503500_8016F290;
            }
            if (func_actor_503500_801360BC(arg0->spawnArg1.value, 2) != 0) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01001C00, &vec[(s16)((s16)work->field_EC % 3)]);
            }
            if ((s16)work->field_EC++ >= 5) {
                if (func_actor_503500_801360BC(arg0->spawnArg1.value, 6) != 0) {
                    Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, 0x04404600, &vec[0]);
                    Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, 0x05404600, &vec[1]);
                    Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, 0x06404600, &vec[2]);
                }
                SndEvt_EnqueueType7(SOUND_BRAHMAN_DEATH_LOOP, 0x2D);
                work->field_F1++;
            }
            break;
        default:
            work->field_EC++;
            if ((s16)work->field_EC >= 0x1F) {
                func_actor_503500_8013611C(arg0->spawnArg1.value);
                arg0->state++;
            }
            break;
    }
}

/// Per-frame tick of the second 0xF4 block, the same shape as
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
        func_actor_503500_8013D8BC(arg0);
    }
    func_actor_503500_8013D914(arg0);
    func_actor_503500_8013D990(arg0);
}

static void func_actor_503500_8013D85C(Task* arg0)
{
    Enemy* enemy;

    enemy                           = arg0->spawnArg2.pointer;
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work770E8*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_8013D8BC(Task* arg0)
{
    Enemy* enemy = arg0->spawnArg2.pointer;

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

static void func_actor_503500_8013D914(Task* arg0)
{
    Actor503500Work770E8* work;
    s16                   timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013CCBC(arg0, &work->obj, work->rec, 8);
    }
    Gp_ClearRec18Occupied(work->rec);
}

static void func_actor_503500_8013D990(Task* arg0)
{
    s8 temp_v1;

    temp_v1 = ((Actor503500Work770E8*)arg0->work)->field_F0;
    switch (temp_v1) {
        case 0:
            func_actor_503500_8013DC4C(arg0);
            break;
        case 1:
            func_actor_503500_8013DA2C(arg0, 0x258);
            break;
        case 2:
            func_actor_503500_8013D1CC(arg0);
            break;
        case 3:
            func_actor_503500_8013D558(arg0);
            break;
    }
}

/// Sub-state 1 of the second 0xF4 block: counts `field_EA` up to `arg1`
/// frames, then - if slot `kind` is free - respawns the two slot enemies,
/// each primed to die in 9 frames with a fifth of this enemy's HP (at least
/// 1), and drops back to sub-state 0 with the node unlinked. A pending
/// kill (`killCountdown == 2`) moves it to sub-state 2 instead.
static void func_actor_503500_8013DA2C(Task* arg0, s32 arg1)
{
    Actor503500Work770E8* work;
    Enemy*                enemy;
    Enemy*                child;
    s32                   kind;
    s32                   slotA;
    s32                   slotB;
    s32                   hp;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (arg1 < ++work->field_EA) {
        kind = 3;
        if (arg0->spawnArg1.value == 7) {
            kind  = 2;
            slotA = 0xD;
            slotB = 0xE;
        } else {
            slotA = 0xF;
            slotB = 0x10;
        }
        if (func_actor_503500_80135E04(arg0->parent, kind) != 0) {
            child = func_actor_503500_80135D00(arg0->parent, slotA);
            hp    = (s16)(enemy->hp / 5);
            if (hp <= 0) {
                hp = 1;
            }
            if (child != NULL) {
                child->task->killCountdown = 9;
                child->hp                  = hp;
            }
            child = func_actor_503500_80135D00(arg0->parent, slotB);
            if (child != NULL) {
                child->task->killCountdown = 9;
                child->hp                  = hp;
            }
            work->field_EA = 0;
        }
        func_actor_503500_8013DBA8(arg0, 0);
        work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldTargetUnlinkNode(&enemy->node);
        work->field_E8 = 0;
        return;
    }
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013DBA8(arg0, 2);
        arg0->killCountdown = 0;
    }
}

/// The second 0xF4 block's counterpart of `func_actor_503500_8013BE48`: puts
/// the block into sub-state `arg1` (the one `func_actor_503500_8013D990`
/// dispatches on), clears the phase and the two counters that go with it,
/// cancels a pending kill, and reports the slot busy to the boss for every
/// sub-state but 1.
static void func_actor_503500_8013DBA8(Task* arg0, s32 arg1)
{
    Actor503500Work770E8* work = (Actor503500Work770E8*)arg0->work;

    work->field_F0      = arg1;
    work->field_F1      = 0;
    work->field_EC      = 0;
    work->field_EE      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1.value, arg1 != 1);
}

void func_actor_503500_8013DBF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132060;
    sp.funcs[task->state](task);
}

static void func_actor_503500_8013DC4C(Task* arg0)
{
    Actor503500Work770E8* work;
    s32                   kind;
    s32                   slotA;
    s32                   slotB;

    kind = 3;
    if (arg0->spawnArg1.value == 7) {
        kind  = 2;
        slotA = 0xD;
        slotB = 0xE;
    } else {
        slotA = 0xF;
        slotB = 0x10;
    }
    if ((func_actor_503500_80135E04(arg0->parent, kind) != 0) &&
        (func_actor_503500_80135E04(arg0->parent, slotA) != 0) &&
        (func_actor_503500_80135E04(arg0->parent, slotB) != 0)) {
        func_actor_503500_8013DBA8(arg0, 1);
        work             = arg0->work;
        work->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        Gp_LinkNode(&((Enemy*)arg0->spawnArg2.pointer)->node);
    }
}

/// `Task::state` handlers `func_actor_503500_8013EC64` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132098 = {
    {
        func_actor_503500_8013DD10,
        func_actor_503500_8013E9A4,
        func_actor_503500_8013EA2C,
    },
};

/// State-0 init of the 0xF0 enemy at `D_actor_503500_8017797C`, the same shape
/// as `func_actor_503500_8013BEE4`: clears the block, resets the task's own
/// coordinate to a plain 4096 identity, parents it to part 1 of the parent
/// task's model, links the enemy node and its display node, and starts the
/// block in sub-state 0.
static void func_actor_503500_8013DD10(Task* arg0)
{
    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    GfxCoord*              parts;
    MATRIX*                mtx;
    WorldCollisionContact* rec;

    coord  = arg0->extra.tmd->coords;
    enemy  = arg0->spawnArg2.pointer;
    parent = arg0->parent;
    memFillBytes(&D_actor_503500_8017797C, 0, sizeof(D_actor_503500_8017797C));
    arg0->work = &D_actor_503500_8017797C;

    parts                            = parent->extra.tmd->coords;
    MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
    coord->parent                    = &parts[1];
    mtx                              = &coord->coord;
    MATRIX_PAIR(mtx, 0, 2)           = 0;
    MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
    MATRIX_PAIR(mtx, 2, 0)           = 0;
    mtx->m[2][2]                     = 0x1000;
    enemy->field_4                   = mtx;
    enemy->field_48                  = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F2D8.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F2D8.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F2D8.vz;
    rec                           = D_actor_503500_8017797C.rec;
    enemy->param                  = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                   = rec;
    enemy->hp                     = enemy->param->hpMax;

    D_actor_503500_8017797C.obj.coord            = coord;
    D_actor_503500_8017797C.obj.context.contacts = rec;
    D_actor_503500_8017797C.obj.key              = 0x30023;
    D_actor_503500_8017797C.obj.radius           = 0x3E8;
    D_actor_503500_8017797C.obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    D_actor_503500_8017797C.obj.pos.vx           = D_actor_503500_8016F2D8.vx;
    D_actor_503500_8017797C.obj.pos.vy           = D_actor_503500_8016F2D8.vy;
    D_actor_503500_8017797C.obj.pos.vz           = D_actor_503500_8016F2D8.vz;
    Gp_LinkObj(2, &D_actor_503500_8017797C.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_8017797C.field_E0.spawnArgLo = 0x600;
    D_actor_503500_8017797C.field_E0.coord      = coord;
    D_actor_503500_8017797C.field_E0.spawnArgHi = 3;
    D_actor_503500_8017797C.obj.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    func_actor_503500_8013EC20(arg0, 0);
    arg0->exitCallback = func_actor_503500_8013EA2C;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)`, the
/// same pass as `func_actor_503500_80137C90` for this form: each attack id is
/// taken once, only type-2 ids land while the `field_E8` countdown is clear,
/// and a hit that empties `field_40` starts state 2. The hit effect is pulled
/// to 600 units along the contact offset. `arg1` is passed but unused.
static void func_actor_503500_8013DEB4(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    VECTOR                d;
    SVECTOR               pos;
    MATRIX                mtx;
    MATRIX                rot;
    Actor503500Work7797C* work;
    Enemy*                enemy;
    GfxCoord*             coord;
    GfxCoord*             src;
    s16                   stun;
    u32                   id;
    s32                   dmg;
    s32                   crit;
    s32                   scale;
    s32                   i;
    s32                   j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key.value;
        for (j = 0; j < i; j++) {
            if (arg2[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013EC20(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                Gp_SetObjFlag2(enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4(enemy, id, 0);
                break;
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x258000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F2D8.vx;
        pos.vy += D_actor_503500_8016F2D8.vy;
        pos.vz += D_actor_503500_8016F2D8.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Sub-state 1 of the third 0xF0 block, stepped by `field_ED`: step 0 applies
/// the boss's preset 0xB; from frame 0x42 step 1 spawns six children from
/// `D_actor_503500_8016E9F0`, one a frame, each placed at a fixed offset in the
/// actor's frame and turned by its row of `D_actor_503500_8016F2EC`; step 3
/// hands off once `func_actor_503500_80136014` reports preset 0xB flagged.
static void func_actor_503500_8013E384(Task* arg0)
{
    SVECTOR               pos;
    SVECTOR               ofs;
    MATRIX                m;
    Actor503500Work7797C* work;
    GfxCoord*             coord;
    GfxCoord*             dst;
    Task*                 task;
    s32*                  src;
    s32*                  out;
    s32                   i;
    s32                   arg;
    s16                   idx;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013EC20(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1.value);
        return;
    }
    switch (work->field_ED) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 0xB, 0x10);
            work->field_ED++;
            break;
        case 1:
            if (++work->field_EA < 0x42) {
                break;
            }
        case 2:
            idx  = work->field_EA - 0x42;
            arg  = (D_actor_503500_8016F2E0[idx] << 12) + (-gPlayerStatus.coordMtx->t[1] << 24) / 1000;
            task = Task_SpawnFromTable(D_actor_503500_8016E9F0, 0, 1, arg);
            if (task != NULL) {
                Gp_ComposeParentWorld(coord, &m, &pos);
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
            if (idx >= 5) {
                work->field_ED += 2;
            }
            break;
        case 3:
            if (func_actor_503500_80136014(arg0->parent, 0xB) != 0) {
                func_actor_503500_8013EC20(arg0, 0);
            }
            break;
    }
}

/// Sub-state 2 of the third 0xF0 block, stepped by `field_ED`: the same
/// death sequence as `func_actor_503500_8013F4A4`, except the effects come
/// from `D_actor_503500_8016F31C` - the odd-frame one at a random row - and
/// the VRAM rect is restored on frame 8 rather than at the end.
static void func_actor_503500_8013E740(Task* arg0)
{
    Enemy*                enemy;
    Actor503500Work7797C* work;
    GfxCoord*             coord;
    s32                   pan;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_ED) {
        case 0:
            work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs      = 0;
            worldTargetUnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->field_E8 = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            pan                   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(SOUND_BRAHMAN_DEATH_LOOP, pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->field_ED++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1.value, 3) != 0) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01001C00,
                            &D_actor_503500_8016F31C[(s16)(work->field_EA % 9)]);
                if (work->field_EA & 1) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, 0x04404600,
                                &D_actor_503500_8016F31C[(u16)((gRandomLcgState >> 16) % 9)]);
                }
            }
            if (work->field_EA == 8) {
                MoveImage(&D_actor_503500_8016F364, 0x141, 0x12A);
            }
            if (work->field_EA++ >= 0x1F) {
                func_actor_503500_8013611C(arg0->spawnArg1.value);
                SndEvt_EnqueueType7(SOUND_BRAHMAN_DEATH_LOOP, 0x2D);
                work->field_ED++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// The third 0xF0 block's per-frame tick, the same shape as
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
        func_actor_503500_8013EA8C(arg0);
    }
    func_actor_503500_8013EAE4(arg0);
    func_actor_503500_8013EB60(arg0);
}

static void func_actor_503500_8013EA2C(Task* arg0)
{
    Enemy* enemy;

    enemy                           = arg0->spawnArg2.pointer;
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work7797C*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_8013EA8C(Task* arg0)
{
    Enemy* enemy;

    enemy = arg0->spawnArg2.pointer;
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

static void func_actor_503500_8013EAE4(Task* arg0)
{
    Actor503500Work7797C* work;
    s16                   timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013DEB4(arg0, &work->obj, work->rec, 8);
    }
    Gp_ClearRec18Occupied(work->rec);
}

static void func_actor_503500_8013EB60(Task* arg0)
{
    switch (((Actor503500Work7797C*)arg0->work)->field_EC) {
        case 0:
            func_actor_503500_8013EBE4(arg0);
            break;
        case 1:
            func_actor_503500_8013E384(arg0);
            break;
        case 2:
            func_actor_503500_8013E740(arg0);
            break;
    }
}

/// The third 0xF0 block's counterpart of `func_actor_503500_80138454`: when a
/// kill is pending, hands the block to sub-state 1 and cancels the countdown.
static void func_actor_503500_8013EBE4(Task* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013EC20(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// The third 0xF0 block's counterpart of `func_actor_503500_8013BE48`: puts the
/// block into sub-state `arg1` (the one `func_actor_503500_8013EB60`
/// dispatches on), clears the phase and frame counter that go with it, cancels
/// a pending kill, and reports the slot busy to the boss when the sub-state
/// is non-zero.
static void func_actor_503500_8013EC20(Task* arg0, s32 arg1)
{
    Actor503500Work7797C* work = arg0->work;

    work->field_EC      = arg1;
    work->field_ED      = 0;
    work->field_EA      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1.value, arg1 != 0);
}

void func_actor_503500_8013EC64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132098;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013FA1C` dispatches through.
static const TaskFuncTable3 D_actor_503500_801320D0 = {
    {
        func_actor_503500_8013ECBC,
        func_actor_503500_8013F6F0,
        func_actor_503500_8013F778,
    },
};

/// State-0 init of the 0xF4 enemy at `D_actor_503500_80177A6C`: clears the
/// block, resets the task's own coordinate to a plain 4096 identity, parents
/// it to part 8 of the parent task's model, links the enemy node and its
/// display node, and hands the block to sub-state 3.
static void func_actor_503500_8013ECBC(Task* arg0)
{
    Enemy*                 enemy;
    Task*                  parent;
    GfxCoord*              coord;
    GfxCoord*              parts;
    MATRIX*                mtx;
    WorldCollisionContact* rec;

    coord  = arg0->extra.tmd->coords;
    enemy  = arg0->spawnArg2.pointer;
    parent = arg0->parent;
    memFillBytes(&D_actor_503500_80177A6C, 0, sizeof(D_actor_503500_80177A6C));
    arg0->work = &D_actor_503500_80177A6C;

    parts                            = parent->extra.tmd->coords;
    MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
    coord->parent                    = &parts[8];
    mtx                              = &coord->coord;
    MATRIX_PAIR(mtx, 0, 2)           = 0;
    MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
    MATRIX_PAIR(mtx, 2, 0)           = 0;
    mtx->m[2][2]                     = 0x1000;
    enemy->field_4                   = mtx;
    enemy->field_48                  = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                   = coord;
    enemy->node.state.parts.flags |= (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    enemy->bodyPos.vx              = D_actor_503500_8016F36C.vx;
    enemy->bodyPos.vy              = D_actor_503500_8016F36C.vy;
    enemy->bodyPos.vz              = D_actor_503500_8016F36C.vz;
    rec                            = D_actor_503500_80177A6C.rec;
    enemy->param                   = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                    = rec;
    enemy->hp                      = enemy->param->hpMax;

    D_actor_503500_80177A6C.obj.coord            = coord;
    D_actor_503500_80177A6C.obj.context.contacts = rec;
    D_actor_503500_80177A6C.obj.pos.vx           = D_actor_503500_8016F36C.vx;
    D_actor_503500_80177A6C.obj.pos.vy           = D_actor_503500_8016F36C.vy;
    D_actor_503500_80177A6C.obj.pos.vz           = D_actor_503500_8016F36C.vz;
    D_actor_503500_80177A6C.obj.key              = 0x30023;
    D_actor_503500_80177A6C.obj.radius           = 0x320;
    D_actor_503500_80177A6C.obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &D_actor_503500_80177A6C.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_80177A6C.field_E0.spawnArgLo = 0x600;
    D_actor_503500_80177A6C.field_E0.coord      = coord;
    D_actor_503500_80177A6C.field_E0.spawnArgHi = 3;
    D_actor_503500_80177A6C.obj.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    func_actor_503500_8013F9D4(arg0, 3);
    arg0->exitCallback = func_actor_503500_8013F778;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the 0xF4 block's enemy, like `func_actor_503500_80139A20`: each attack id is
/// taken once, only type-2 ids land while the `field_E8` countdown is clear,
/// and a hit that empties `field_40` starts sub-state 2 but still applies the
/// id's status effect. The hit effect is pulled to 400 units along the contact
/// offset. `arg1` is passed by the caller but unused.
static void func_actor_503500_8013EE5C(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    VECTOR             d;
    SVECTOR            pos;
    MATRIX             mtx;
    MATRIX             rot;
    Actor503500WorkF4* work;
    Enemy*             enemy;
    GfxCoord*          coord;
    GfxCoord*          src;
    s16                stun;
    u32                id;
    s32                dmg;
    s32                crit;
    s32                scale;
    s32                i;
    s32                j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key.value;
        for (j = 0; j < i; j++) {
            if (arg2[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013F9D4(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                Gp_SetObjFlag2(enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4(enemy, id, 0);
                break;
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x190000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F36C.vx;
        pos.vy += D_actor_503500_8016F36C.vy;
        pos.vz += D_actor_503500_8016F36C.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// A sub-state of the 0xF4 block, stepped by `field_F1`: applies the boss's
/// animation preset 0xC, counts 0x6F frames (spawning table entry 3 at frame
/// 0x1E with its coordinate parented to this task's), applies preset 0xD once
/// the boss reports sub-state 0xC done, and leaves when 0xD is done too.
static void func_actor_503500_8013F328(Task* arg0)
{
    Actor503500WorkF4* work = (Actor503500WorkF4*)arg0->work;
    Task*              task;
    GfxCoord*          coord;

    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013F9D4(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1.value);
        return;
    }
    switch (work->field_F1) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 0xC, 8);
            work->field_F1++;
            break;
        case 1:
            work->field_EA++;
            if ((s16)work->field_EA >= 0x6F) {
                work->field_EA = 0;
                work->field_F1++;
            } else if ((s16)work->field_EA == 0x1E) {
                task = Task_SpawnFromTable(D_actor_503500_8016E9F0, 3, 0, 0);
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
            if (func_actor_503500_80136014(arg0->parent, 0xC) != 0) {
                func_actor_503500_80135FB4(arg0->parent, 0xD, 0x10);
                work->field_F1++;
            }
            break;
        case 3:
            if (func_actor_503500_80136014(arg0->parent, 0xD) != 0) {
                func_actor_503500_8013F9D4(arg0, 0);
            }
            break;
    }
}

/// Sub-state 2 of the 0xF4 block, stepped by `field_F1`: phase 0 hides the
/// display node, unlinks the enemy node, releases its state-F0 reference and
/// plays the death sound; phase 1 spawns effects from `D_actor_503500_8016F374`
/// for 0x1F frames, then restores the VRAM rect and moves on.
static void func_actor_503500_8013F4A4(Task* arg0)
{
    Enemy*             enemy;
    Actor503500WorkF4* work;
    GfxCoord*          coord;
    s32                pan;

    enemy = arg0->spawnArg2.pointer;
    work  = (Actor503500WorkF4*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_F1) {
        case 0:
            work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs      = 0;
            worldTargetUnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->field_E8 = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            pan                   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(SOUND_BRAHMAN_DEATH_LOOP, pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->field_EC = 2;
            work->field_F1++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1.value, 3) != 0) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01001A00,
                            &D_actor_503500_8016F374[(s16)((s16)work->field_EA % 6)]);
                if (work->field_EA & 1) {
                    Gp_SpawnEff(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord, 0x04404600,
                                &D_actor_503500_8016F374[(s16)((s16)work->field_EA % 3) + 3]);
                }
            }
            work->field_EA++;
            if ((s16)work->field_EA >= 0x1F) {
                MoveImage(&D_actor_503500_8016F3A4, 0x140, 0x100);
                SndEvt_EnqueueType7(SOUND_BRAHMAN_DEATH_LOOP, 0x2D);
                func_actor_503500_8013611C(arg0->spawnArg1.value);
                work->field_F1++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// The 0xF4 block's per-frame tick, the same shape as
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
        func_actor_503500_8013F7D8(arg0);
    }
    func_actor_503500_8013F830(arg0);
    func_actor_503500_8013F8AC(arg0);
}

static void func_actor_503500_8013F778(Task* arg0)
{
    Enemy* enemy;

    enemy                           = arg0->spawnArg2.pointer;
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500WorkF4*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_8013F7D8(Task* arg0)
{
    Enemy* enemy;

    enemy = arg0->spawnArg2.pointer;
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
    Actor503500WorkF4* work;
    s16                timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013EE5C(arg0, &work->obj, work->rec, 8);
    }
    Gp_ClearRec18Occupied(work->rec);
}

static void func_actor_503500_8013F8AC(Task* arg0)
{
    switch (((Actor503500WorkF4*)arg0->work)->field_F0) {
        case 0:
            func_actor_503500_8013F948(arg0);
            break;
        case 1:
            func_actor_503500_8013F328(arg0);
            break;
        case 2:
            func_actor_503500_8013F4A4(arg0);
            break;
        case 3:
            func_actor_503500_8013F984(arg0);
            break;
    }
}

/// The 0xF4 block's counterpart of `func_actor_503500_80138454`: when a kill is
/// pending, hands the block to sub-state 1 and cancels the countdown.
static void func_actor_503500_8013F948(Task* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013F9D4(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// The 0xF4 block's sub-state 3 handler: when a kill is pending, hands the
/// block to sub-state 0 and hides its display node.
static void func_actor_503500_8013F984(Task* arg0)
{
    Actor503500WorkF4* work;

    if (arg0->killCountdown == 8) {
        func_actor_503500_8013F9D4(arg0, 0);
        work             = arg0->work;
        work->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// The 0xF4 block's counterpart of `func_actor_503500_80138490`: puts the block
/// into sub-state `arg1`, clears the phase and the two counters that go with
/// it, cancels a pending kill, and reports the slot busy to the boss when the
/// sub-state is non-zero.
static void func_actor_503500_8013F9D4(Task* arg0, s32 arg1)
{
    Actor503500WorkF4* work = (Actor503500WorkF4*)arg0->work;

    work->field_F0      = arg1;
    work->field_F1      = 0;
    work->field_EA      = 0;
    work->field_EC      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1.value, arg1 != 0);
}

void func_actor_503500_8013FA1C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801320D0;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80142370` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132108 = {
    {
        func_actor_503500_8013FA74,
        func_actor_503500_8013FF0C,
        func_actor_503500_80141D04,
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
    GfxMatrix                     m;
    GfxRotationWords*             ident;
    s32                           idx;
    s32                           i;

    idx   = arg0->spawnArg1.value - 0xD;
    enemy = arg0->spawnArg2.pointer;
    work  = &D_actor_503500_80177B60[idx];
    coord = arg0->extra.tmd->coords;
    tmd   = arg0->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work = work;

    coord->parent          = &arg0->parent->extra.tmd->coords[1];
    coord->coord.t[0]      = D_actor_503500_8016F3AC[idx].vx;
    coord->coord.t[1]      = D_actor_503500_8016F3AC[idx].vy;
    coord->coord.t[2]      = D_actor_503500_8016F3AC[idx].vz;
    m.rotationWords.m00M01 = ONE;
    ident                  = &m.rotationWords;
    ident->m02M10          = 0;
    ident->m11M12          = ONE;
    ident->m20M21          = 0;
    ident->m22             = ONE;
    RotMatrix(&D_actor_503500_8016F3CC[idx], &m.mat);
    MulMatrix0(&coord->coord, &m.mat, &coord->coord);
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
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(rec, ARRAY_SIZE(work->contacts), 0);
    rec2                              = work->attackContacts;
    work->attackBody.coord            = part;
    work->attackBody.context.contacts = rec2;
    work->body.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.pos.vx           = D_actor_503500_8016F3F4[idx].vx;
    work->attackBody.pos.vy           = D_actor_503500_8016F3F4[idx].vy;
    work->attackBody.pos.vz           = D_actor_503500_8016F3F4[idx].vz;
    work->attackBody.key              = Gp_PackPair(enemy->param->attacks, 0);
    work->attackBody.radius           = 0x1F4;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->attackBody);
    Gp_InitRec18Table(rec2, ARRAY_SIZE(work->attackContacts), 0);
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
    Gp_UpdateCoord(coord);
    func_actor_503500_801421A8(arg0);
    switch (arg0->killCountdown) {
        case 8:
            func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING);
            break;
        case 9:
            tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING);
            break;
        default:
            Gp_LinkNode(&enemy->node);
            enemy->hp         = D_actor_503500_8016E7EC[arg0->spawnArg1.value].hpMax;
            work->blendWeight = 0x1000;
            work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            break;
    }
    arg0->exitCallback = func_actor_503500_80141D04;
    arg0->state       += 1;
}

static void func_actor_503500_8013FF0C(Task* arg0)
{
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    TmdObject*                    tmd;
    s8                            countdown;
    s32                           slot;

    work      = arg0->work;
    enemy     = arg0->spawnArg2.pointer;
    countdown = work->bufferFreeCountdown;
    tmd       = arg0->extra.tmd;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(tmd);
        }
        work->bufferFreeCountdown--;
    }
    if (gGameSession->eventState != 0) {
        slot = 0xB;
        if (arg0->spawnArg1.value < 0xF) {
            slot = 0xA;
        }
        if (func_actor_503500_80135E04(arg0->parent, slot) == 0) {
            tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        } else {
            goto tick;
        }
    } else {
    tick:
        func_actor_503500_80135828(arg0, &work->bufferFreeCountdown);
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
                func_actor_503500_80140BE8(arg0);
            }
            func_actor_503500_801420C4(arg0);
            func_actor_503500_80141D7C(arg0);
            if (work->detached == 0) {
                func_actor_503500_80141248(arg0);
                func_actor_503500_80141448(arg0);
            }
            func_actor_503500_801421A8(arg0);
            func_actor_503500_80141B94(arg0);
            break;
    }
}

/// Lunge state of the lunging chains. Step 0 latches the position behind
/// `gPlayerStatus.coordMtx` in `lungeTarget` and rotates its offset from the
/// parent coordinate into `tipTarget`; step 1 ramps `curlWeight` to 0x2000 and
/// re-aims once `tipArrived` is set; steps 2..4 ramp it back to 0. While in
/// steps 0..1, `func_actor_503500_80142310` ends the state after 120 frames
/// or when `func_actor_503500_80136218`'s reading leaves the window the slot
/// (and whether its partner slot is empty) allows.
static void func_actor_503500_801400A4(Task* arg0)
{
    SVECTOR                       v;
    SVECTOR                       pos;
    MATRIX                        mtx;
    MATRIX                        rot;
    _Actor503500LungingChainWork* work;
    GfxCoord*                     coord;
    s32                           keep;
    s32                           dist;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
        func_actor_503500_8013611C(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            work->lungeTarget.vx = gPlayerStatus.coordMtx->t[0];
            work->lungeTarget.vy = gPlayerStatus.coordMtx->t[1];
            work->lungeTarget.vz = gPlayerStatus.coordMtx->t[2];
            Gp_ComposeParentWorld(coord->parent, &mtx, &pos);
            v.vx = work->lungeTarget.vx - pos.vx;
            v.vy = work->lungeTarget.vy - pos.vy - 5000;
            v.vz = work->lungeTarget.vz - pos.vz;
            gte_TransposeMatrix(&mtx, &rot);
            gte_SetRotMatrix(&rot);
            gte_ldv0(&v);
            gte_rtv0();
            gte_stsv(&work->tipTarget);
            func_actor_503500_80135FB4(arg0->parent, 0x12, 0x10);
            work->stateStep++;
            break;
        case 1:
            work->curlWeight += 0x88;
            if (work->curlWeight > 0x2000) {
                work->curlWeight = 0x2000;
            }
            if (work->tipArrived != 0 && work->swayPhase[ACTOR_503500_LUNGING_CHAIN_TIP_PART] > 2000) {
                Gp_ComposeParentWorld(coord->parent, &mtx, &pos);
                v.vx = work->lungeTarget.vx - pos.vx;
                v.vy = work->lungeTarget.vy - pos.vy + 500;
                v.vz = work->lungeTarget.vz - pos.vz;
                gte_TransposeMatrix(&mtx, &rot);
                gte_SetRotMatrix(&rot);
                gte_ldv0(&v);
                gte_rtv0();
                gte_stsv(&work->tipTarget);
                work->tipSpeedLimit.word = 0x4000000;
                work->attackBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->stateStep++;
            }
            break;
        case 2:
            work->curlWeight -= 0x200;
            if (work->curlWeight < 0) {
                work->curlWeight = 0;
            }
            work->swayWeight -= 0x80;
            if (work->swayWeight < 0) {
                work->swayWeight = 0;
            }
            work->stepFrames++;
            if (work->stepFrames > 30) {
                work->stepFrames = 0;
                work->stateStep++;
            }
            break;
        case 3:
            work->curlWeight -= 0x2AA;
            if (work->curlWeight < 0) {
                work->curlWeight = 0;
            }
            if (work->tipArrived != 0) {
                work->stepFrames        = 0;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->stateStep++;
            }
            break;
        case 4:
            work->curlWeight -= 0x400;
            if (work->curlWeight < 0) {
                work->curlWeight = 0;
            }
            work->stepFrames++;
            if (work->stepFrames > 30) {
                func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            }
            break;
    }
    keep = 0;
    dist = func_actor_503500_80136218();
    switch (arg0->spawnArg1.value) {
        case 13:
            if (func_actor_503500_80135E04(arg0, 0xE) != 0) {
                if (dist < -1900 || dist > 1000) {
                    keep = 1;
                }
            } else if (dist > 1000 && dist < 1700) {
                keep = 1;
            }
            break;
        case 14:
            if (func_actor_503500_80135E04(arg0, 0xD) != 0) {
                if (dist < -1900 || dist > 1000) {
                    keep = 1;
                }
            } else if (dist < -1900 || dist > 1699) {
                keep = 1;
            }
            break;
        case 15:
            if (func_actor_503500_80135E04(arg0, 0x10) != 0) {
                if (dist < -1000 || dist > 1900) {
                    keep = 1;
                }
            } else if (dist < -1699 || dist > 1900) {
                keep = 1;
            }
            break;
        case 16:
            if (func_actor_503500_80135E04(arg0, 0xF) != 0) {
                if (dist < -1000 || dist > 1900) {
                    keep = 1;
                }
            } else if (dist < -1000) {
                if (dist >= -1699) {
                    keep = 1;
                }
            }
            break;
    }
    work->stateFrames++;
    if (work->stateStep < 2 && (work->stateFrames > 120 || keep == 0)) {
        func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
    }
}

/// Death state of the lunging chains, the same body as
/// `func_actor_503500_80139014` at this block's offsets: unlinks the enemy
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
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->hitCooldown = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= ENEMY_REACTION_LOW_CLEAR;
            work->tipTarget.vy    = 0x7D0;
            work->stateStep++;
            break;
        case 1:
            if (work->tipArrived != 0) {
                Gp_ComposeParentWorld(coord, &m, &rot);
                coord->coord        = m;
                coord->coord.t[0]   = rot.vx;
                coord->coord.t[1]   = rot.vy;
                coord->coord.t[2]   = rot.vz;
                coord->parent       = &gGfxViewCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->detached      = phase;
                Gp_UpdateCoord(coord);
                SndEvt_EnqueueType6(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
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
                func_actor_503500_SetRotIdentity(&coord[i].coord);
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
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    SndEvt_EnqueueType6(SOUND_COMMON(0x0D), (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case 15:
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 1, NULL);
                    break;
                case 30:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 40:
                    SndEvt_EnqueueType7(SOUND_COMMON(0x0D), 1);
                    arg0->state++;
                    break;
            }
            work->stateFrames++;
            break;
    }
    if (func_actor_503500_801360BC(arg0->spawnArg1.value, 4) != 0 && work->stateStep < 3 &&
        gDisplayState.animFrame % 12 == 0) {
        for (i = ACTOR_503500_LUNGING_CHAIN_TIP_PART, j = 0; i > 0; i--) {
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[i], 0xB0008600, &D_actor_503500_8016F448[j]);
            j++;
            j = (j < 3) ? j : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep > 0) {
        SndEvt_EnqueueType7(SOUND_COMMON(0x0D), 1);
        arg0->state = 2;
    }
}

static void func_actor_503500_80140BE8(Task* arg0)
{
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    s32                           dmg;
    u8                            flags;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if ((func_actor_503500_80136208() == 0) && (gGameSession->eventState == 0)) {
        flags = enemy->reactionFlags;
        if (flags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
            func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            work->holdFrames = 5;
            work->slowFrames = 8;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_DAMAGE_OVER_TIME);
            if (Gp_ObjFlag4Expired(arg0->spawnArg2.pointer) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            } else {
                dmg = Gp_TickObjFlag4(enemy);
                if (dmg != 0) {
                    enemy->hp -= dmg;
                    func_800DA6E8(&enemy->node, dmg, 0);
                    work->slowFrames = 8;
                    if (enemy->hp <= 0) {
                        enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                        func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_DYING);
                    } else {
                        func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
                    }
                }
            }
        }
    }
}

/// Applies this frame's hits from `body`'s collision records `arg2[0..arg3)`
/// to the lunging chain's enemy, like `func_actor_503500_8013EE5C`, but at model
/// part 8: each attack id is taken once, only type-2 ids land while the
/// `hitCooldown` countdown is clear, and a hit that empties `hp` enters
/// state 5 (unless already past it) instead of applying the id's status effect.
/// The hit effect is pulled to 500 units along the contact offset, and a hit
/// landing in state 1 drops back to state 0. `arg1` is passed but unused.
static void func_actor_503500_80140D38(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    SVECTOR                       pos;
    MATRIX                        rot;
    MATRIX                        mtx;
    VECTOR                        d;
    _Actor503500LungingChainWork* work;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    GfxCoord*                     src;
    s16                           stun;
    u32                           id;
    s32                           dmg;
    s32                           crit;
    s32                           scale;
    s32                           i;
    s32                           j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[ACTOR_503500_LUNGING_CHAIN_TIP_PART];
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key.value;
        for (j = 0; j < i; j++) {
            if (arg2[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->hitCooldown != 0) {
            continue;
        }
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        src  = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            if (work->state < ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING) {
                func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_DYING);
            }
        } else {
            switch (Gp_GetIdParam0(id) & 0xFFFF) {
                case 0:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    break;
                case 1:
                    Gp_SetObjFlag1(enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2(enemy, id, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4(enemy, id, 0);
                    break;
            }
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x1F4000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F3EC.vx;
        pos.vy += D_actor_503500_8016F3EC.vy;
        pos.vz += D_actor_503500_8016F3EC.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->hitEffect);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->hitCooldown < stun) {
            work->hitCooldown = stun;
        }
        if (work->state == ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE) {
            func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
        }
    next:;
    }
}

/// The lunging chain's copy of `func_actor_503500_80139EFC`: saves
/// `tipPosition` into `previousTipPosition`, then steers it toward `tipTarget`.
/// Inside the arrival distance (the integer half of `tipSpeedLimit`) it snaps
/// onto the target and sets `tipArrived`; otherwise the speed `tipSpeed`
/// accelerates toward +/-`tipSpeedLimit` while `tipAdvancing` is set, or decays
/// to 0, and moves `tipPosition` along the normalized offset (at a quarter
/// speed while `slowFrames` runs).
static void func_actor_503500_80141248(Task* arg0)
{
    SVECTOR                       d;
    SVECTOR                       n;
    VECTOR                        step;
    _Actor503500LungingChainWork* work;
    s32                           lim;
    s32                           speed;

    work                         = arg0->work;
    work->previousTipPosition.vx = work->tipPosition.vx;
    work->previousTipPosition.vy = work->tipPosition.vy;
    work->previousTipPosition.vz = work->tipPosition.vz;
    d.vx                         = work->tipTarget.vx - work->tipPosition.vx;
    d.vy                         = work->tipTarget.vy - work->tipPosition.vy;
    d.vz                         = work->tipTarget.vz - work->tipPosition.vz;
    if (ABS(d.vx) + ABS(d.vy) + ABS(d.vz) < work->tipSpeedLimit.halves.integer) {
        work->tipArrived     = 1;
        work->tipPosition.vx = work->tipTarget.vx;
        work->tipPosition.vy = work->tipTarget.vy;
        work->tipPosition.vz = work->tipTarget.vz;
        return;
    }
    lim              = work->tipSpeedLimit.word;
    work->tipArrived = 0;
    if (work->tipAdvancing != 0) {
        speed = work->tipSpeed + lim / 32;
        if (speed > 0) {
            if (speed > lim) {
                speed = lim;
            }
        } else if (speed < -lim) {
            speed = -lim;
        }
    } else {
        speed = work->tipSpeed - lim / 32;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->tipSpeed = speed;
    VectorNormalSS(&d, &n);
    if (work->slowFrames != 0) {
        speed >>= 2;
    }
    step.vx               = n.vx * (speed >> 12);
    step.vy               = n.vy * (speed >> 12);
    step.vz               = n.vz * (speed >> 12);
    work->tipPosition.vx += step.vx >> 16;
    work->tipPosition.vy += step.vy >> 16;
    work->tipPosition.vz += step.vz >> 16;
}

/// Lays the lunging chain's nine points along a cubic Bezier from the root
/// coordinate's world position to its parent's, with the near control point
/// offset 1000 units in the root's frame (X mirrored for spawn slots 15 / 16)
/// and both far control points at the parent plus its rotated `tipPosition`. The
/// samples land in `linkPoints`, `func_actor_503500_8014176C` re-aims the links along
/// them, and links 2..8 get a pitch of a fading sine sway plus the scaled rest
/// pitch before `func_actor_503500_80142220` applies it.
static void func_actor_503500_80141448(Task* arg0)
{
    SVECTOR                       ctrl[4];
    SVECTOR                       ofs;
    SVECTOR                       tmp;
    VECTOR                        out[ACTOR_503500_LUNGING_CHAIN_PART_COUNT];
    MATRIX                        m;
    GfxCoord*                     coord;
    _Actor503500LungingChainWork* work;
    s32                           i;
    s32                           v;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    Gp_ComposeParentWorld(coord, &m, &ctrl[0]);
    work->linkPoints[0].vx = ctrl[0].vx;
    work->linkPoints[0].vy = ctrl[0].vy;
    work->linkPoints[0].vz = ctrl[0].vz;
    if ((u32)(arg0->spawnArg1.value - 15) < 2) {
        ofs.vx = -1000;
        ofs.vy = 0;
        ofs.vz = 1000;
    } else {
        ofs.vx = 1000;
        ofs.vy = 0;
        ofs.vz = 1000;
    }
    gte_SetRotMatrix(&m);
    gte_ldv0(&ofs);
    gte_rtv0();
    gte_stsv(&ctrl[1]);
    ctrl[1].vx += ctrl[0].vx;
    ctrl[1].vy += ctrl[0].vy;
    ctrl[1].vz += ctrl[0].vz;
    Gp_ComposeParentWorld(coord->parent, &m, &tmp);
    gte_SetRotMatrix(&m);
    gte_ldv0(&work->tipPosition);
    gte_rtv0();
    gte_stsv(&ofs);
    tmp.vx    += ofs.vx;
    tmp.vy    += ofs.vy;
    tmp.vz    += ofs.vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy;
    ctrl[3].vz = tmp.vz;
    for (i = ACTOR_503500_LUNGING_CHAIN_TIP_PART; i >= 0; i--) {
        bezierCurveEvaluate(ctrl, &ctrl[3], ACTOR_503500_LUNGING_CHAIN_PART_COUNT, i, &out[i].vx);
        copyVector(&work->linkPoints[ACTOR_503500_LUNGING_CHAIN_TIP_PART - i], &out[i]);
    }
    func_actor_503500_8014176C(work->linkPoints, arg0->extra.tmd->coords);
    for (i = ACTOR_503500_LUNGING_CHAIN_TIP_PART; i >= 2; i--) {
        v                       = ((work->swayAmplitude * work->swayWeight >> 12) * rsin(work->swayPhase[i])) >> 12;
        work->linkAngles[i].vx  = v;
        work->linkAngles[i].vx += D_actor_503500_8016F434[i] * work->curlWeight >> 12;
        work->linkAngles[i].vy  = 0;
        work->linkAngles[i].vz  = 0;
        work->swayPhase[i]      = (work->swayPhase[i] + 0x80) & 0xFFF;
    }
    func_actor_503500_80142220(work->linkAngles, arg0->extra.tmd->coords);
}

/// Re-aims a chain of eight child coordinates along the polyline `pts[0..8]`.
/// `world` starts as the chain root's world rotation and accumulates each
/// link's local rotation; the segment `pts[i + 1] - pts[i]` is taken into that
/// frame, and the resulting direction becomes the next link's basis
/// (`Gfx_OrthonormalBasis`, up hint +Y) with the local segment as its
/// translation. Works in an `Actor503500ChainScratch` on the scratchpad stack.
static void func_actor_503500_8014176C(SVECTOR* pts, GfxCoord* coords)
{
    Actor503500ChainScratch* s;
    MATRIX*                  inv;
    SVECTOR*                 dir;
    s32                      i;
    s32                      j;

    s        = (Actor503500ChainScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor503500ChainScratch));
    s->up.vx = 0;
    s->up.vy = 0x1000;
    s->up.vz = 0;
    Gp_ComposeParentWorld(coords->parent, &s->world, &s->rot);
    for (i = 0, j = 1; i < 8; i++, j++) {
        s->diff.vx = pts[j].vx - pts[i].vx;
        s->diff.vy = pts[j].vy - pts[i].vy;
        s->diff.vz = pts[j].vz - pts[i].vz;
        gte_SetRotMatrix(&s->world);
        inv = &s->inv;
        dir = &s->dir;
        gte_ldclmv(&coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->world);
        gte_ldclmv((char*)&coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->world + 2);
        gte_ldclmv((char*)&coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->world + 4);
        gte_TransposeMatrix(&s->world, inv);
        gte_SetRotMatrix(inv);
        gte_ldv0(&s->diff);
        gte_rtv0();
        gte_stlvnl(&s->pos);
        VectorNormalS(&s->pos, dir);
        Gfx_OrthonormalBasis(&coords[j].coord, dir, &s->up);
        coords[j].coord.t[0] = s->pos.vx;
        coords[j].coord.t[1] = s->pos.vy;
        coords[j].coord.t[2] = s->pos.vz;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor503500ChainScratch));
}

#include "../../shared/bezier_curve_evaluate.inc.c"

/// The lunging chain's counterpart of `func_actor_503500_8013AB38`: while
/// `blendWeight` is below 0x1000, blends parts 1..8 toward `blendStart`,
/// copying the lerped rotation back word-wise and keeping a
/// `blendWeight / 0x1000` share of each part's offset from its `blendStart`
/// entry.
static void func_actor_503500_80141B94(Task* arg0)
{
    MATRIX                        m;
    VECTOR                        d;
    _Actor503500LungingChainWork* work;
    GfxCoord*                     coord;
    MATRIX*                       mat;
    s32*                          src;
    s32*                          dst;
    s32                           t;
    s32                           i;
    s32                           j;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords + 1;
    if (work->blendWeight < 0x1000) {
        mat = &work->blendStart[1];
        t   = work->blendWeight;
        for (i = 1; i < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; i++) {
            Gp_LerpOrthonormal(mat, &coord->coord, &m, t);
            dst = (s32*)&coord->coord;
            src = (s32*)&m;
            for (j = 0; j < 4; j++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = m.m[2][2];
            d.vx                 = ((coord->coord.t[0] - mat->t[0]) * t) >> 12;
            d.vy                 = ((coord->coord.t[1] - mat->t[1]) * t) >> 12;
            d.vz                 = ((coord->coord.t[2] - mat->t[2]) * t) >> 12;
            coord->coord.t[0]    = mat->t[0] + d.vx;
            coord->coord.t[1]    = mat->t[1] + d.vy;
            coord->coord.t[2]    = mat->t[2] + d.vz;
            mat++;
            coord++;
        }
    }
}

static void func_actor_503500_80141D04(Task* arg0)
{
    Enemy* enemy;

    enemy = arg0->spawnArg2.pointer;
    func_actor_503500_8013611C(arg0->spawnArg1.value);
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&((_Actor503500LungingChainWork*)arg0->work)->body);
    Gp_UnlinkObj(&((_Actor503500LungingChainWork*)arg0->work)->attackBody);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_80141D7C(Task* arg0)
{
    _Actor503500LungingChainWork* work;

    work = arg0->work;
    switch (work->state) {
        case ACTOR_503500_LUNGING_CHAIN_STATE_IDLE:
            func_actor_503500_80141E64(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE:
            func_actor_503500_801400A4(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_HOLD:
            work->holdFrames--;
            if (work->holdFrames < 0) {
                func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
            }
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_DYING:
            func_actor_503500_80140654(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_UNFOLDING:
            func_actor_503500_80141F48(arg0);
            break;
        case ACTOR_503500_LUNGING_CHAIN_STATE_REGROWING:
            func_actor_503500_80141FC8(arg0);
            break;
    }
    work->slowFrames--;
    if (work->slowFrames < 0) {
        work->slowFrames = 0;
    }
}

static void func_actor_503500_80141E64(Task* arg0)
{
    _Actor503500LungingChainWork* work;

    work = arg0->work;
    if (arg0->killCountdown == 2) {
        func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_LUNGE);
        arg0->killCountdown = 0;
    }
    work->swayWeight += 0x20;
    if (work->swayWeight > 0x1000) {
        work->swayWeight = 0x1000;
    }
    work->curlWeight -= 0x111;
    if (work->curlWeight < 0) {
        work->curlWeight = 0;
    }
    work->tipTarget.vx = D_actor_503500_8016F3AC[arg0->spawnArg1.value].vx;
    work->tipTarget.vy = D_actor_503500_8016F3AC[arg0->spawnArg1.value].vy;
    work->tipTarget.vz = D_actor_503500_8016F3AC[arg0->spawnArg1.value].vz;
}

static void func_actor_503500_80141F48(Task* arg0)
{
    _Actor503500LungingChainWork* work;

    work               = arg0->work;
    work->blendWeight += 0x10;
    if (work->blendWeight > 0x1000) {
        Gp_LinkNode(&((Enemy*)arg0->spawnArg2.pointer)->node);
        work->blendWeight = 0x1000;
        work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
    }
}

/// Step 0 resets `blendStart` to identity; step 1 raises `blendWeight` by 0x20
/// a frame and, once it passes 0x1000, relinks the display node and moves on
/// like `func_actor_503500_80141F48`.
static void func_actor_503500_80141FC8(Task* arg0)
{
    _Actor503500LungingChainWork* work;
    s32                           i;
    long*                         t;

    work = arg0->work;
    switch (work->stateStep) {
        case 0:
            for (i = 1; i < ACTOR_503500_LUNGING_CHAIN_PART_COUNT; i++) {
                func_actor_503500_SetRotIdentity(&work->blendStart[i]);
                // The view shifted by i matrices puts blendStart[i] at
                // blendStart[0]; this `(work + i) + offset` association is
                // what lets the pointer derive from the giv the indexed store
                // below uses.
                t                        = ((_Actor503500LungingChainWork*)((MATRIX*)work + i))->blendStart[0].t;
                work->blendStart[i].t[0] = 0;
                t[1]                     = 0;
                t[2]                     = 0;
            }
            work->blendWeight = 0;
            work->stateStep++;
        case 1:
            work->blendWeight += 0x20;
            if (work->blendWeight > 0x1000) {
                Gp_LinkNode(&((Enemy*)arg0->spawnArg2.pointer)->node);
                work->blendWeight = 0x1000;
                work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                func_actor_503500_80142310(arg0, ACTOR_503500_LUNGING_CHAIN_STATE_IDLE);
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
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_80140D38(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
        func_actor_503500_8014215C(arg0, &work->attackBody, work->attackContacts, ARRAY_SIZE(work->attackContacts));
    }
    Gp_ClearRec18Occupied(work->contacts);
    Gp_ClearRec18Occupied(work->attackContacts);
}

/// Scans `count` `WorldCollisionContact` slots and clears bit 0x8000 of `obj->flags` for
/// every slot whose `key` high half is 1.
static void func_actor_503500_8014215C(Task* arg0, WorldCollisionBody* obj, WorldCollisionContact* rec, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++, rec++) {
        if ((rec->key.value & 0xFFFF0000) == 0x10000) {
            obj->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
}

/// Copies the actor's attach-coordinate world position into a stack `VECTOR`
/// and hands it to `Gp_UpdateActorColor` with no blend parameters. Same body as
/// `func_actor_503500_80136AEC`.
static void func_actor_503500_801421A8(Task* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra.tmd->coords->workm.t[0];
    vec.vy = arg0->extra.tmd->coords->workm.t[1];
    vec.vz = arg0->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Re-aims coordinate nodes 2..7 of the model: each node's rotation is read
/// back as Euler angles, its pitch replaced with the caller's per-node angle,
/// and the matrix rebuilt from the result. The identity splat before
/// `RotMatrixZYX` clears the node's rotation with five aligned stores, the same
/// idiom `func_800B0928` uses.
static void func_actor_503500_80142220(SVECTOR* angles, GfxCoord* nodes)
{
    SVECTOR ang;
    MATRIX* m;
    s32     i;

    for (i = 2; i < 8; i++) {
        m = &nodes[i].coord;
        gfxExtractSmallestEuler(&ang, m);
        ang.vx                 = angles[i].vx;
        *(s32*)&nodes[i].coord = ONE;
        MATRIX_PAIR(m, 0, 2)   = 0;
        MATRIX_PAIR(m, 1, 1)   = ONE;
        MATRIX_PAIR(m, 2, 0)   = 0;
        m->m[2][2]             = ONE;
        RotMatrixZYX(&ang, m);
    }
}

#include "../../shared/bezier_curve_coefficients.inc.c"

static void func_actor_503500_80142310(Task* arg0, s32 arg1)
{
    _Actor503500LungingChainWork* work;

    work                     = arg0->work;
    work->state              = arg1;
    work->stateStep          = 0;
    work->field_3D1          = 0;
    work->stateFrames        = 0;
    work->stepFrames         = 0;
    work->tipSpeedLimit.word = 0x600000;
    work->attackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg0->killCountdown      = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1.value, arg1 != 0);
}

void func_actor_503500_80142370(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132108;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801442A8` dispatches through.
static const TaskFuncTable3 D_actor_503500_80132178 = {
    {
        func_actor_503500_801423C8,
        func_actor_503500_80143EB4,
        func_actor_503500_80143F78,
    },
};

/// State-0 init of the arm at `D_actor_503500_80178AC0.value[spawnArg1 - 0xA]`:
/// clears the block, hangs the task's coordinate off the parent part picked by
/// `D_actor_503500_80171464`, republishes the parent's light and colour
/// matrices, and links three collision bodies - `body` on the task's own
/// coordinate, `forearmAttackBody` / `handAttackBody` on parent parts 6 / 7
/// (side 1) or 12 / 13 (side 0) sharing `attackContacts` - before starting
/// `ACTOR_503500_ARM_STATE_IDLE`.
static void func_actor_503500_801423C8(Task* arg0)
{
    Enemy*                 enemy;
    TmdObject*             tmd;
    Task*                  parent;
    s32                    slot;
    _Actor503500ArmWork*   work;
    GfxCoord*              coord;
    TmdObject*             parentTmd;
    SVECTOR*               ofs;
    WorldCollisionContact* rec;
    GfxCoord*              parts;
    GfxCoord*              parts2;

    enemy     = arg0->spawnArg2.pointer;
    tmd       = arg0->extra.tmd;
    parent    = arg0->parent;
    slot      = arg0->spawnArg1.value - 0xA;
    work      = &D_actor_503500_80178AC0.value[slot];
    coord     = tmd->coords;
    parentTmd = parent->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work = work;

    coord->parent       = &parent->extra.tmd->coords[D_actor_503500_80171464[slot]];
    coord->coord.t[0]   = D_actor_503500_80171478.vx;
    coord->coord.t[1]   = D_actor_503500_80171478.vy;
    coord->coord.t[2]   = D_actor_503500_80171478.vz;
    work->side          = arg0->spawnArg1.value - 0xA;
    tmd->lightMtx       = parentTmd->lightMtx;
    tmd->colorMtx       = parentTmd->colorMtx;
    tmd->otOffset       = 0x13;
    tmd->flags         |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    enemy->field_4                = &coord->coord;
    ofs                           = &D_actor_503500_80171480[slot];
    enemy->field_48               = 0;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = ofs->vx;
    enemy->bodyPos.vy             = ofs->vy;
    enemy->bodyPos.vz             = ofs->vz;
    rec                           = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                   = rec;
    enemy->hp                     = enemy->param->hpMax;

    work->body.coord            = coord;
    work->body.context.contacts = rec;
    work->body.pos.vx           = ofs->vx;
    work->body.pos.vy           = ofs->vy;
    work->body.pos.vz           = ofs->vz;
    work->body.key              = 0x30023;
    work->body.radius           = 0x5DC;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(rec, ARRAY_SIZE(work->contacts), 0);
    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    parts = parent->extra.tmd->coords;
    if (slot != 0) {
        work->forearmAttackBody.coord = &parts[6];
    } else {
        work->forearmAttackBody.coord = &parts[12];
    }
    work->forearmAttackBody.context.contacts = work->attackContacts;
    work->forearmAttackBody.pos.vx           = 0;
    work->forearmAttackBody.pos.vy           = 0;
    work->forearmAttackBody.pos.vz           = 0;
    work->forearmAttackBody.key              = 0x30023;
    work->forearmAttackBody.radius           = 0x320;
    work->forearmAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->forearmAttackBody);
    Gp_InitRec18Table(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    parts2 = parent->extra.tmd->coords;
    if (slot != 0) {
        work->handAttackBody.coord = &parts2[7];
    } else {
        work->handAttackBody.coord = &parts2[13];
    }
    work->handAttackBody.context.contacts = work->attackContacts;
    work->handAttackBody.pos.vx           = 0;
    work->handAttackBody.pos.vy           = 0x190;
    work->handAttackBody.pos.vz           = 0;
    work->handAttackBody.key              = 0x30023;
    work->handAttackBody.radius           = 0x4B0;
    work->handAttackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->handAttackBody);
    Gp_InitRec18Table(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->hitEffect.spawnArgLo  = 0x600;
    work->hitEffect.coord       = coord;
    work->hitEffect.spawnArgHi  = 3;
    work->handAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_IDLE);
    arg0->exitCallback = func_actor_503500_80143F78;
    arg0->state       += 1;
}

/// `ACTOR_503500_ARM_STATE_STRIKE` step of the arm. Step 0 hands the parent
/// rate 0xC, or 0x12 when `func_actor_503500_80135E04` accepts slot 4/5, and
/// keeps the pick in `strikeRate`. Step 1 counts frames in `stateFrames`: on
/// frame 0x7A (0xC) or 0x51 (0x12) it enables pair tests on
/// `forearmAttackBody` / `handAttackBody` and plays `SOUND_BRAHMAN_ARM_STRIKE`
/// at parent coordinate 6 or 12 (by `side`); on 0x90 / 0x60 it disables them,
/// and it moves on once `func_actor_503500_80136014` reports done.
static void func_actor_503500_8014271C(Task* arg0)
{
    _Actor503500ArmWork* work;
    GfxCoord*            coords;
    GfxCoord*            coord;
    s32                  side;
    s32                  anim;
    s16                  frame;

    work = arg0->work;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_IDLE);
        func_actor_503500_8013611C(arg0->spawnArg1.value);
        return;
    }
    side = 3;
    if (work->side != 0) {
        side = 2;
    }
    switch (work->stateStep) {
        case 0:
            anim = ACTOR_503500_ARM_STRIKE_RATE_SLOW;
            if (func_actor_503500_80135E04(arg0->parent, side == 2 ? 5 : 4) != 0) {
                anim = ACTOR_503500_ARM_STRIKE_RATE_FAST;
            }
            func_actor_503500_80135FB4(arg0->parent, side, anim);
            work->strikeRate = anim;
            work->stateStep++;
            break;
        case 1:
            frame = ++work->stateFrames;
            if (work->strikeRate == ACTOR_503500_ARM_STRIKE_RATE_SLOW) {
                switch (frame) {
                    case 0x7A:
                        work->forearmAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                        work->handAttackBody.flags    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                        coords                         = arg0->parent->extra.tmd->coords;
                        if (work->side != 0) {
                            coord = &coords[6];
                        } else {
                            coord = &coords[12];
                        }
                        SndEvt_EnqueueType6(SOUND_BRAHMAN_ARM_STRIKE, (s8)worldCoordGetOriginAudioPan(coord),
                                            (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                        break;
                    case 0x90:
                        work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                }
            } else {
                switch (frame) {
                    case 0x51:
                        work->forearmAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                        work->handAttackBody.flags    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                        coords                         = arg0->parent->extra.tmd->coords;
                        if (work->side != 0) {
                            coord = &coords[6];
                        } else {
                            coord = &coords[12];
                        }
                        SndEvt_EnqueueType6(SOUND_BRAHMAN_ARM_STRIKE, (s8)worldCoordGetOriginAudioPan(coord),
                                            (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                        break;
                    case 0x60:
                        work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                }
            }
            if (func_actor_503500_80136014(arg0->parent, side) != 0) {
                work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->stateStep++;
            }
            break;
        default:
            func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_IDLE);
            break;
    }
}

/// `ACTOR_503500_ARM_STATE_DYING` step of the arm, the counterpart of the 0x160 enemy's
/// `func_actor_503500_80137678`: step 0 unlinks the enemy node and clears the
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
    GfxMatrix            m;
    s8                   param1[8];
    s8                   param2[8];
    GfxRotationWords*    ident;
    _Actor503500ArmWork* work;
    Enemy*               enemy;
    GfxCoord*            coord;
    GfxCoord*            src;
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
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1.value);
            work->hitCooldown = 0;
            (Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add(arg0, 0);
            func_actor_503500_80136048(arg0->parent);
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
            if (func_actor_503500_801360BC(arg0->spawnArg1.value, 1) != 0) {
                rot.vx = side != 0 ? -300 : 300;
                rot.vy = (u32)(rcos(work->stateFrames << 7) * 375) >> 10;
                rot.vz = (u32)(rsin(work->stateFrames << 7) * 375) >> 10;
                Gp_SpawnEff(EFFECT_HIT_PUFF, coord->parent->parent, 0x01101600, &rot);
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
                work->loadCommandSlot = CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, (u8*)param1, (u8*)param2);
                Gp_ComposeParentWorld(coord, &m.mat, &rot);
                // Copy the nine coefficients as four words and a halfword; preserve the alignment halfword.
                in  = (s32*)&m;
                out = (s32*)&coord->coord;
                for (k = 0; k < 4; k++) {
                    *out++ = *in++;
                }
                coord->coord.m[2][2] = m.mat.m[2][2];
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
                Tmd_AllocBuffers(tmd);
                rot.vx = 0;
                rot.vy = 0;
                rot.vz = 0;
                if (side != 0) {
                    func_actor_503500_80135E20(arg0->parent, 5, &rot);
                    work->velocity.fixed.vx.word = -0x100000;
                    work->velocity.fixed.vy.word = 0;
                    work->velocity.fixed.vz.word = 0;
                } else {
                    func_actor_503500_80135E20(arg0->parent, 0xB, &rot);
                    work->velocity.fixed.vx.word = 0x100000;
                    work->velocity.fixed.vy.word = 0;
                    work->velocity.fixed.vz.word = 0;
                }
                ApplyMatrixLV(&coord->coord, &work->velocity.vector, &work->velocity.vector);
                coord->parent = &gGfxViewCoord;
                Gp_UpdateCoord(coord);
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 2:
            arg0->extra.tmd->otOffset = 0x11;
            SndEvt_EnqueueType6(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            work->stateStep++;
            break;
        case 3:
            if (func_actor_503500_801360BC(arg0->spawnArg1.value, 2) != 0) {
                if (work->side != 0) {
                    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01101C00, &D_actor_503500_80171594);
                    work->spin.fixed.vz.word -= 0x2000;
                } else {
                    Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x01101C00, &D_actor_503500_8017158C);
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
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    SndEvt_EnqueueType6(SOUND_COMMON(0x0D), (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case 30:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 40:
                    SndEvt_EnqueueType7(SOUND_COMMON(0x0D), 1);
                    arg0->state++;
                    break;
            }
            work->stateFrames++;
            break;
        default:
            arg0->state++;
            break;
    }
    rot.vx                 = work->spin.fixed.vx.word >> 16;
    rot.vy                 = work->spin.fixed.vy.word >> 16;
    rot.vz                 = work->spin.fixed.vz.word >> 16;
    m.rotationWords.m00M01 = ONE;
    m.rotationWords.m02M10 = 0;
    ident                  = &m.rotationWords;
    ident->m11M12          = ONE;
    m.rotationWords.m20M21 = 0;
    ident->m22             = ONE;
    RotMatrix(&rot, &m.mat);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&m.mat);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv((char*)&m.mat + 2);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 2);
    gte_ldclmv((char*)&m.mat + 4);
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
    if (func_actor_503500_801360BC(arg0->spawnArg1.value, 3) != 0) {
        if (!(gDisplayState.animFrame & 3)) {
            for (i = 0, j = 0; i < 2; i++) {
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[i], 0x81018A00, &D_actor_503500_80171564[j]);
                j++;
                j = j < 5 ? j : 0;
            }
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep >= 2) {
        SndEvt_EnqueueType7(SOUND_COMMON(0x0D), 1);
        arg0->state = 2;
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
    VECTOR               d;
    SVECTOR              pos;
    MATRIX               mtx;
    MATRIX               rot;
    _Actor503500ArmWork* work;
    Enemy*               enemy;
    GfxCoord*            coord;
    GfxCoord*            src;
    s16                  stun;
    s16                  hp;
    u32                  id;
    s32                  dmg;
    s32                  crit;
    s32                  scale;
    s32                  i;
    s32                  j;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key.value;
        for (j = 0; j < i; j++) {
            if (arg2[j].key.value == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->hitCooldown != 0) {
            continue;
        }
        src = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78(enemy, id, dmg, 0);
        hp        = enemy->hp - dmg;
        enemy->hp = hp;
        if (hp <= 0) {
            enemy->hp = 0;
            dmg      += hp;
            if (work->recoveryFrames == 0) {
                work->recoveryFrames = ACTOR_503500_ARM_RECOVERY_FRAMES;
            }
        }
        func_800DA6E8(&enemy->node, dmg, 0);
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 5:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                Gp_SetObjFlag2(enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4(enemy, id, 0);
                break;
            case 4:
            case 6:
                if (enemy->hp <= 0) {
                    func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_DYING);
                    crit = 2;
                }
                break;
        }
        if ((id & 0x8000) && D_actor_503500_80171490[id & 0x7F] != 0) {
            func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_DYING);
            crit = 2;
        }
        gte_TransposeMatrix(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x5DC000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_80171480[work->side].vx;
        pos.vy += D_actor_503500_80171480[work->side].vy;
        pos.vz += D_actor_503500_80171480[work->side].vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->hitEffect);
        if (crit != 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, (crit == 2) * 2, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->hitCooldown < stun) {
            work->hitCooldown = stun;
        }
    next:;
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
                Gp_ComposeParentWorld(coord, &world, &vec);
                gte_TransposeMatrix(&pcoord->coord, &rot);
                gte_SetRotMatrix(&rot);
                gte_ldv0(&vec);
                gte_rtv0();
                gte_stsv(&vec);
                side = vec.vz >= 0;
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 0), 0);
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_801714E0[side], 0);
                Task_SpawnFromTable(&D_actor_503500_8017146C, 0, side, &work->knockbackRotation);
                Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
                pan                = (s8)worldCoordGetOriginAudioPan(pcoord);
                SndEvt_EnqueueType6(SOUND_COMMON(7), pan, (s8)(worldCoordGetOriginAudioDepth(pcoord) / 2));
            }
        }
    }
}

/// Knock-back task spawned from `D_actor_503500_8017146C` by
/// `func_actor_503500_801437D0`, with the hit side in `spawnArg1` and the
/// enemy's turned rotation in `spawnArg2`. State 0 copies that rotation, starts
/// the push at speed 0x1000000 and shakes the camera for 8 frames; state 1 moves
/// the player by the rotated speed through `GAME_ACTOR_MESSAGE_MOVE_BY`, decaying it by
/// 0x30000 a frame, and after 20 frames at rest moves on (or ends at -1 when the
/// player has no HP left). States 2-4 wait out message 0x3ED between the two
/// 0x3FF payloads and the closing 0x3F1. Frame 0x11 plays sound 0x54300002 at
/// the player.
void func_actor_503500_80143AC0(Task* arg0)
{
    VECTOR             vec;
    GameActorMoveBy    msg;
    Actor503500Work38* work;
    Task*              player;
    GfxCoord*          coord;
    s32*               src;
    s32*               dst;
    s32                i;
    s32                pan;
    s32                next;
    s32                shake;

    work   = &D_actor_503500_80178F10;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (gPlayerStatus.hp <= 0) {
                taskKill(arg0);
                return;
            }
            memFillBytes(work, 0, sizeof(Actor503500Work38));
            work->speed             = 0x1000000;
            work->pos.fixed.vx.word = 0;
            work->pos.fixed.vy.word = 0;
            work->pos.fixed.vz.word = 0;
            src                     = (s32*)arg0->spawnArg2.pointer;
            dst                     = (s32*)&work->rot;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->rot.m[2][2] = ((MATRIX*)arg0->spawnArg2.pointer)->m[2][2];
            Gp_SpawnScript18(D_actor_503500_8017159C, D_actor_503500_801715A4);
            work->field_36 = 8;
            // An s32 temp: passed straight to the s8 parameter, the masked
            // expression is shortened into a byte load of the frame counter.
            shake = (gDisplayState.animFrame ^ 1) & 1;
            displaySetShakeY(shake);
            arg0->state++;
        case 1:
            vec.vx = 0;
            vec.vy = 0;
            vec.vz = work->speed;
            ApplyMatrixLV(&work->rot, &vec, &vec);
            work->pos.fixed.vx.word += vec.vx;
            work->pos.fixed.vy.word += vec.vy;
            work->pos.fixed.vz.word += vec.vz;
            msg.collisionRequests    = 1;
            msg.keepControl          = 1;
            msg.displacement.vx      = work->pos.fixed.vx.halves.integer;
            msg.displacement.vy      = work->pos.fixed.vy.halves.integer;
            msg.displacement.vz      = work->pos.fixed.vz.halves.integer;
            if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &msg, 0) != 0) {
                work->speed = 0;
            }
            work->pos.fixed.vx.word = work->pos.fixed.vx.halves.fraction;
            work->pos.fixed.vy.word = work->pos.fixed.vy.halves.fraction;
            work->pos.fixed.vz.word = work->pos.fixed.vz.halves.fraction;
            work->speed            -= 0x30000;
            if (work->speed < 0) {
                work->speed = 0;
                if (++work->field_34 > 20) {
                    // The -1 arm first: reorg inverts the branch around it and
                    // leaves the `li` in the delay slot, sharing $v0 with the load.
                    if (gPlayerStatus.hp <= 0) {
                        next = -1;
                    } else {
                        next = arg0->state + 1;
                    }
                    arg0->state = next;
                }
            }
            if (work->field_36 > 0) {
                shake = (gDisplayState.animFrame ^ 1) & 1;
                displaySetShakeY(shake);
                work->field_36--;
            } else {
                displaySetShakeY(0);
            }
            break;
        case 2:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_80171508[arg0->spawnArg1.value], 0);
                arg0->state++;
            }
            break;
        case 3:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                D_actor_503500_801714DC =
                    Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]
                        ->table.words[7];
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_503500_80171530, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                taskKill(arg0);
            }
            break;
    }
    if (++arg0->killCountdown == 0x11) {
        coord = player->extra.tmd->coords;
        pan   = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(SOUND_SHELTER_R48_PLAYER_KNOCKBACK, pan, (s8)worldCoordGetOriginAudioDepth(coord));
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
                func_actor_503500_80143FFC(arg0);
            }
            break;
        case 2:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                func_actor_503500_80144098(arg0, mode, enemy);
            }
            func_actor_503500_80143FFC(arg0);
            func_actor_503500_80144004(arg0);
            func_actor_503500_801440F0(arg0);
            break;
    }
}

static void func_actor_503500_80143F78(Task* arg0)
{
    Enemy*               enemy;
    _Actor503500ArmWork* work;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    func_actor_503500_8013611C(arg0->spawnArg1.value);
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    Gp_UnlinkObj(&work->body);
    Gp_UnlinkObj(&work->forearmAttackBody);
    Gp_UnlinkObj(&work->handAttackBody);
    enemy->recs = 0;
    arg0->work  = NULL;
    enemyDestroy(enemy, arg0);
}

static void func_actor_503500_80143FFC(Task* arg0)
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
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_801431EC(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
        func_actor_503500_801437D0(arg0, work->attackContacts, ARRAY_SIZE(work->attackContacts));
    }
    Gp_ClearRec18Occupied(work->contacts);
    Gp_ClearRec18Occupied(work->attackContacts);
}

static void func_actor_503500_80144098(Task* arg0, s32 arg1, Enemy* arg2)
{
    Enemy* enemy = arg0->spawnArg2.pointer;

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
            func_actor_503500_8014418C(arg0);
            break;
        case ACTOR_503500_ARM_STATE_STRIKE:
            func_actor_503500_8014271C(arg0);
            break;
        case ACTOR_503500_ARM_STATE_DYING:
            func_actor_503500_80142980(arg0);
            break;
        case ACTOR_503500_ARM_STATE_BECOME_TARGET:
            func_actor_503500_801441E8(arg0);
            break;
    }
}

static void func_actor_503500_8014418C(Task* arg0)
{
    switch (arg0->killCountdown) {
        case 2:
            func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_STRIKE);
            arg0->killCountdown = 0;
            break;
        case 8:
            func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_BECOME_TARGET);
            arg0->killCountdown = 0;
            break;
    }
}

static void func_actor_503500_801441E8(Task* arg0)
{
    _Actor503500ArmWork* work;

    work              = arg0->work;
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkNode(&((Enemy*)arg0->spawnArg2.pointer)->node);
    func_actor_503500_80144238(arg0, ACTOR_503500_ARM_STATE_IDLE);
}

static void func_actor_503500_80144238(Task* arg0, s32 arg1)
{
    _Actor503500ArmWork* work;

    work                = arg0->work;
    work->state         = arg1;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1.value, arg1 != 0);
    work->forearmAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->handAttackBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    SndEvt_EnqueueType7(SOUND_BRAHMAN_ARM_STRIKE, 1);
}

void func_actor_503500_801442A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132178;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80144890` dispatches through.
static const TaskFuncTable3 D_actor_503500_801321DC = {
    {
        func_actor_503500_80144300,
        func_actor_503500_801446E4,
        func_actor_503500_8014473C,
    },
};

static void func_actor_503500_80144300(Task* arg0)
{
    _Actor503500BallisticShotWork* work;
    GfxCoord*                      coord;
    WorldCollisionContact*         contacts;
    EffectWork*                    eff;
    Task*                          child;
    GfxRotationWords*              m;
    VECTOR                         v;
    s32                            pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;

    work->position.fixed.vx.word       = coord->coord.t[0] << 16;
    work->position.fixed.vy.word       = coord->coord.t[1] << 16;
    work->position.fixed.vz.word       = coord->coord.t[2] << 16;
    work->launchPosition.fixed.vx.word = work->position.fixed.vx.word;
    work->field_B8                     = 0x1000;
    work->launchPosition.fixed.vy.word = work->position.fixed.vy.word;
    work->launchPosition.fixed.vz.word = work->position.fixed.vz.word;

    if (arg0->spawnArg2.pointer != NULL) {
        v.vx = 0;
        v.vy = 0;
        v.vz = arg0->spawnArg2.value;
        ApplyMatrixLV(&coord->coord, &v, &work->velocity.vector);
    } else {
        m         = (GfxRotationWords*)&coord->coord;
        m->m00M01 = ONE;
        m->m02M10 = 0;
        m->m11M12 = ONE;
        m->m20M21 = 0;
        m->m22    = ONE;
    }
    contacts = work->contacts;

    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = D_actor_503500_801715AC.vx;
    work->body.pos.vy           = D_actor_503500_801715AC.vy;
    work->body.pos.vz           = D_actor_503500_801715AC.vz;
    work->body.key              = Gp_PackPair(D_actor_503500_8016E7CC[0], arg0->spawnArg1.value);
    work->body.radius           = 0x12C;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    eff = Gp_SpawnEff(EFFECT_BRAHMAN_SMALL_ORB, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_8014473C(arg0);
        return;
    }
    child            = eff->task;
    work->effectTask = child;
    taskReparent(arg0, child);
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 5), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    func_actor_503500_80137290(1);
    arg0->exitCallback = func_actor_503500_8014473C;
    arg0->state       += 1;
}

static void func_actor_503500_80144520(Task* arg0)
{
    _Actor503500BallisticShotWork* work;
    GfxCoord*                      coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->touchedPlayer != 0) {
        work->effectTask->spawnArg1.value = 2;
        work->phase                       = ACTOR_503500_BALLISTIC_SHOT_CUT_SHORT;
    }
    switch (work->phase) {
        case ACTOR_503500_BALLISTIC_SHOT_FLYING:
            work->velocity.fixed.vy.word += 9.8 * 0x10000;
            if (work->gridContactResult != 0) {
                work->body.radius            = 0x258;
                work->velocity.fixed.vx.word = 0;
                work->velocity.fixed.vy.word = 0;
                work->velocity.fixed.vz.word = 0;
                work->landed                 = 1;
                work->gridContactResult      = 0;
                work->phaseFrames            = 0;
                work->phase++;
            } else {
                work->phaseFrames++;
                if (work->phaseFrames >= 0x3D) {
                    work->phase = ACTOR_503500_BALLISTIC_SHOT_CUT_SHORT;
                }
            }
            break;
        case ACTOR_503500_BALLISTIC_SHOT_BURSTING:
            work->phaseFrames++;
            if (work->phaseFrames >= 6) {
                work->effectTask->spawnArg1.value = 2;
                work->phaseFrames                 = 0;
                work->body.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->phase++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    work->position.fixed.vx.word += work->velocity.fixed.vx.word;
    work->position.fixed.vy.word += work->velocity.fixed.vy.word;
    work->position.fixed.vz.word += work->velocity.fixed.vz.word;
    coord->coord.t[0]             = work->position.fixed.vx.halves.integer;
    coord->coord.t[1]             = work->position.fixed.vy.halves.integer;
    coord->coord.t[2]             = work->position.fixed.vz.halves.integer;
}

static void func_actor_503500_801446E4(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    coord = arg0->extra.tmd->coords;
    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_80144778(arg0);
    func_actor_503500_80144520(arg0);
}

static void func_actor_503500_8014473C(Task* arg0)
{
    func_actor_503500_801372AC(1);
    Gp_UnlinkObj(&((_Actor503500BallisticShotWork*)arg0->work)->body);
    taskKill(arg0);
}

static void func_actor_503500_80144778(Task* arg0)
{
    WorldCollisionDelta            delta;
    _Actor503500BallisticShotWork* work;
    WorldCollisionContact*         contacts;
    s32                            result;
    s32                            i;

    work     = arg0->work;
    contacts = work->contacts;
    if (work->landed == 0) {
        result                  = func_800E0C10(contacts, &delta, ARRAY_SIZE(work->contacts), NULL);
        work->gridContactResult = result;
        switch (result) {
            case 0:
                break;
            case 1:
                work->position.fixed.vx.word += delta.fixed.vx.word;
                work->position.fixed.vy.word += delta.fixed.vy.word;
                work->position.fixed.vz.word += delta.fixed.vz.word;
                break;
            case 2:
                work->position.fixed.vx.word = work->launchPosition.fixed.vx.word;
                work->position.fixed.vy.word = work->launchPosition.fixed.vy.word;
                work->position.fixed.vz.word = work->launchPosition.fixed.vz.word;
                break;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        if ((contacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
            work->touchedPlayer = 1;
        }
    }
    Gp_ClearRec18Occupied(work->contacts);
}

void func_actor_503500_80144890(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321DC;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80144E34` dispatches through.
static const TaskFuncTable3 D_actor_503500_801321E8 = {
    {
        func_actor_503500_801448E8,
        func_actor_503500_80144D50,
        func_actor_503500_80144DA8,
    },
};

static void func_actor_503500_801448E8(Task* arg0)
{
    _Actor503500LingeringShotWork* work;
    GfxCoord*                      coord;
    WorldCollisionContact*         contacts;
    EffectWork*                    eff;
    Task*                          child;
    GfxRotationWords*              m;
    s32                            pan;
    s32                            pan2;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;

    work->position.vx       = coord->coord.t[0] << 16;
    work->position.vy       = coord->coord.t[1] << 16;
    work->position.vz       = coord->coord.t[2] << 16;
    work->launchPosition.vx = work->position.vx;
    work->field_AC          = 0x1000;
    work->launchPosition.vy = work->position.vy;
    work->launchPosition.vz = work->position.vz;

    if (arg0->spawnArg2.pointer != NULL) {
        work->speed = arg0->spawnArg2.value;
    } else {
        m           = (GfxRotationWords*)&coord->coord;
        m->m00M01   = ONE;
        m->m02M10   = 0;
        m->m11M12   = ONE;
        m->m20M21   = 0;
        m->m22      = ONE;
        work->speed = 0x100000;
    }
    contacts = work->contacts;

    work->body.coord            = coord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = D_actor_503500_801715B4.vx;
    work->body.pos.vy           = D_actor_503500_801715B4.vy;
    work->body.pos.vz           = D_actor_503500_801715B4.vz;
    work->body.key              = Gp_PackPair(D_actor_503500_8016E7D0[0], arg0->spawnArg1.value);
    work->body.radius           = 0x898;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->body);
    Gp_InitRec18Table(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    if (arg0->spawnArg1.value == 0) {
        eff = Gp_SpawnEff(EFFECT_BRAHMAN_LARGE_ORB, coord, 0, NULL);
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 8), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    } else {
        eff  = Gp_SpawnEff(EFFECT_BRAHMAN_PROJECTILE, coord, 0, NULL);
        pan2 = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 7), pan2, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
    }
    if (eff == NULL) {
        func_actor_503500_80144DA8(arg0);
        return;
    }
    child            = eff->task;
    work->effectTask = child;
    taskReparent(arg0, child);
    func_actor_503500_80137290(3);
    arg0->exitCallback = func_actor_503500_80144DA8;
    arg0->state       += 1;
}

static void func_actor_503500_80144B40(Task* arg0)
{
    _Actor503500LingeringShotWork* work;
    GfxCoord*                      coord;
    VECTOR                         v;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->phase) {
        case ACTOR_503500_LINGERING_SHOT_SLOWING:
            work->speed -= 0x80000;
            if (work->speed < 0x80000) {
                work->effectTask->spawnArg1.value = 2;
                work->phase++;
            }
            break;
        case ACTOR_503500_LINGERING_SHOT_LINGERING:
            work->lingerFrames++;
            if (D_actor_503500_801715BC[arg0->spawnArg1.value] < work->lingerFrames) {
                work->lingerFrames = 0;
                work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->phase++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    v.vx = 0;
    v.vy = 0;
    v.vz = work->speed;
    ApplyMatrixLV(&coord->coord, &v, &v);
    work->position.vx += v.vx;
    work->position.vy += v.vy - 0x10000;
    work->position.vz += v.vz;
    work->position.vx += v.vx;
    work->position.vy += v.vy;
    work->position.vz += v.vz;
    if (work->position.vx > 0x36B00000) {
        work->position.vx = 0x36B00000;
    } else if (work->position.vx < 0x7D00000) {
        work->position.vx = 0x7D00000;
    }
    if (work->position.vz > 0x32C80000) {
        work->position.vz = 0x32C80000;
    } else if (work->position.vz < 0x3E80000) {
        work->position.vz = 0x3E80000;
    }
    coord->coord.t[0] = work->position.vx >> 16;
    coord->coord.t[1] = work->position.vy >> 16;
    coord->coord.t[2] = work->position.vz >> 16;
}

static void func_actor_503500_80144D50(Task* arg0)
{
    GfxCoord* coord;
    s32       state;

    coord = arg0->extra.tmd->coords;
    state = gSceneCombatState.actorControl;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_503500_80144E10(arg0);
    func_actor_503500_80144B40(arg0);
}

static void func_actor_503500_80144DA8(Task* arg0)
{
    func_actor_503500_801372AC(3);
    if (arg0->spawnArg1.value == 0) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 8), 1);
    } else {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 7), 1);
    }
    Gp_UnlinkObj(&((_Actor503500LingeringShotWork*)arg0->work)->body);
    taskKill(arg0);
}

static void func_actor_503500_80144E10(Task* arg0)
{
    Gp_ClearRec18Occupied(((_Actor503500LingeringShotWork*)arg0->work)->contacts);
}

void func_actor_503500_80144E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321E8;
    sp.funcs[task->state](task);
}
