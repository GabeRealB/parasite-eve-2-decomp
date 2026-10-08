#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/display.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
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

#include "rooms/shelter_r48.h"
#include "../../shared/bezier_curve.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

/// Boss animation presets and the attachment shared by its orange-flash sequences.
///
/// Presets are table indices; the flash delay uses actor-updating ticks.
enum {
    ACTOR_503500_ORANGE_FLASH_TASK                = 4,
    ACTOR_503500_ORANGE_FLASH_PARENT_PART         = 3,
    ACTOR_503500_ORANGE_FLASH_ANIMATION_WINDUP    = 4,
    ACTOR_503500_ORANGE_FLASH_ANIMATION_CHARGE    = 5,
    ACTOR_503500_ORANGE_FLASH_ANIMATION_STRIKE    = 6,
    ACTOR_503500_ORANGE_FLASH_ANIMATION_RECOVER   = 7,
    ACTOR_503500_ORANGE_FLASH_ANIMATION_FINISH    = 8,
    ACTOR_503500_ORANGE_FLASH_CHARGE_FRAMES       = 90,
    ACTOR_503500_ORANGE_FLASH_STRIKE_FRAMES       = 51,
    ACTOR_503500_ORANGE_FLASH_TARGET_REMOVE_DELAY = 14,
    ACTOR_503500_BOSS_IDLE_ANIMATION              = 0,
};

/// Phases and bearing limits used by the boss's slot attack callbacks.
///
/// Bearings use 4096 units per turn; the side-chain tests retain their inclusive
/// endpoints and one-unit gaps. The yaw offset makes the boss turn toward its rear.
enum {
    ACTOR_503500_ATTACK_RUNNING               = 0,
    ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES = 1,
    ACTOR_503500_ATTACK_PHASE_COMMAND         = 0,
    ACTOR_503500_ATTACK_PHASE_WAIT            = 1,
    ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING   = 1000,
    ACTOR_503500_SIDE_CHAIN_OUTER_BEARING     = 1700,
    ACTOR_503500_SIDE_CHAIN_REAR_BEARING      = 2000,
    ACTOR_503500_CHAIN_BASE_INNER_BEARING     = 1500,
    ACTOR_503500_CHAIN_BASE_OUTER_BEARING     = 1900,
    ACTOR_503500_ATTACK_REAR_YAW_OFFSET       = 2000,
};

/// One-time exposure commands and the room event selecting the later attack tables.
enum {
    ACTOR_503500_PROGRESS_ARM_0_EXPOSED        = 1 << 0,
    ACTOR_503500_PROGRESS_ARM_1_EXPOSED        = 1 << 1,
    ACTOR_503500_PROGRESS_YELLOW_FLASH_EXPOSED = 1 << 2,
    ACTOR_503500_PROGRESS_ROOM_EVENT_SENT      = 1 << 3,
    ACTOR_503500_PROGRESS_ARM_1_SHIFT          = 1,
    ACTOR_503500_PROGRESS_YELLOW_FLASH_SHIFT   = 2,
    ACTOR_503500_PROGRESS_ROOM_EVENT_SHIFT     = 3,
};

/// Units and identities shared by the boss motion and attached-target setup.
enum {
    ACTOR_503500_BOSS_FIXED_FRACTION_BITS     = 16,
    ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET = 14,
    ACTOR_503500_PART_BODY_KEY                = 0x30023,
    ACTOR_503500_ATTACHED_TARGET_RADIUS       = 800,
    ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG     = 3,
};

/// HP damage multiplier for this boss and its attached targets' critical hits.
enum { ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER = 4 };

/// Effect configuration shared by the emitter and large-chain death sequences.
///
/// The smoke word is the retained packed spawn configuration; the sound is
/// the resident corpse-burn loop stopped on exit or an active room event.
enum {
    ACTOR_503500_DYING_SMOKE_SPAWN_ARG = 0xB0008600U,
    ACTOR_503500_CORPSE_BURN_SOUND     = SOUND_COMMON(0x0D),
};

/// What the pink-flash emitter is doing, as held in `_Actor503500PinkFlashEmitterWork::state`.
enum {
    ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE   = 0, // A target; waits for the boss to command an attack
    ACTOR_503500_PINK_FLASH_EMITTER_STATE_ATTACK = 1, // Has the boss play its two attack animations and launches the pair of pink-flash attacks during the first
    ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING  = 2, // Health exhausted: tips over, comes off the boss, falls away and burns out
};

/// Frame counts of the pink-flash emitter, each measured on
/// `_Actor503500PinkFlashEmitterWork::stateFrames`.
enum {
    ACTOR_503500_PINK_FLASH_EMITTER_ATTACK_RECOVERY_FRAMES = 71, // Frames the attack counts after asking for the boss's second animation, before the emitter idles
    ACTOR_503500_PINK_FLASH_EMITTER_DYING_TIP_FRAMES       = 31, // Frames the dying emitter tips over while still on the boss
    ACTOR_503500_PINK_FLASH_EMITTER_DYING_FALL_FRAMES      = 31, // Frames it then falls before it starts to burn out
    ACTOR_503500_PINK_FLASH_EMITTER_DYING_FADE_FRAME       = 10, // Frame of the burn-out on which the model turns semi-transparent
    ACTOR_503500_PINK_FLASH_EMITTER_DYING_BLACKEN_FRAME    = 30, // Frame of the burn-out on which the model is lit black
    ACTOR_503500_PINK_FLASH_EMITTER_DYING_END_FRAME        = 40, // Frame of the burn-out on which the task moves on to its exit state
};

/// Work block of the pink-flash emitter, the slot enemy (slot 1) that launches
/// the boss's pink-flash attack.
///
/// The emitter is a one-part model hung from part 8 of the boss's model, with
/// a target sphere on it. Nothing animates it, and it is lit with the boss's
/// own light and colour matrices. It is a target from set-up on, though its
/// health is not shown.
///
/// Commanded by the boss, the emitter asks for the boss's animation 15 and
/// then 16, and a frame into the first spawns the pair of pink-flash attack
/// tasks on its own coordinate. It gives the attack up when the boss recoils
/// from a lost part, is stunned or is defeated.
///
/// A hit that exhausts its health empties the slot and sends the boss into
/// its part-lost recoil. The emitter tips over where it hangs, is then
/// re-parented onto the view in world space and thrown along its Z, and falls
/// and burns out. As it comes off, it spawns the yellow-flash emitter (slot
/// 12) on the part it leaves.
///
/// One static instance exists; the task's `Task::work` points at it.
typedef struct {
    byte                   unknown_0[0x40];     // No access found; role unproven
    WorldCollisionBody     body;                // Target sphere of radius 800 on the task's coordinate, 1000 units along its Z and 80 up; pair-tested from set-up until the emitter dies
    WorldCollisionContact  contacts[8];         // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg         hitEffect;           // Record hit effects on the emitter are spawned with, bound to the task's coordinate
    Actor503500FixedVector spin;                // Rotation applied to the dying emitter every frame: 16.16 Euler angles in 4096ths of a turn. Only X is ever changed
    Actor503500FixedVector velocity;            // Velocity of the falling emitter in world space, 16.16 units per frame: 16 along its Z as it comes off, then gravity on Y
    Actor503500FixedVector positionCarry;       // Travel of the falling emitter not yet applied to its coordinate; the integer halves are moved out every frame, leaving the fractions
    s16                    hitCooldown;         // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                    stateFrames;         // Frames counted by the current step of the state
    s8                     state;               // An `ACTOR_503500_PINK_FLASH_EMITTER_STATE_*` state
    s8                     stateStep;           // Step within the current state, restarted on every state change
    s8                     bufferFreeCountdown; // Frames until the model's buffers are freed after it is hidden; negative when idle
    s8                     smokePuffCount;      // Smoke puffs the dying emitter has shed; each takes the next of three offsets in turn
} _Actor503500PinkFlashEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500PinkFlashEmitterWork, 0x160);

/// Shape of a large chain's model.
enum {
    ACTOR_503500_LARGE_CHAIN_PART_COUNT = 9, // Model parts: the root and the eight links aimed along the curve
    ACTOR_503500_LARGE_CHAIN_TIP_PART   = 8, // Last part, which carries the target sphere and fires the shot
};

/// What a large chain is doing, as held in `_Actor503500LargeChainWork::state`.
///
/// Value 3 is not used.
enum {
    ACTOR_503500_LARGE_CHAIN_STATE_IDLE             = 0, // Circles its tip around the slot's rest offset; a command from the boss starts a shot, health under half starts the split
    ACTOR_503500_LARGE_CHAIN_STATE_SHOOT            = 1, // Brings its tip over the player, then fires a ballistic shot from it
    ACTOR_503500_LARGE_CHAIN_STATE_HOLD             = 2, // Waits out `holdFrames`, then idles; stepped, but nothing enters it
    ACTOR_503500_LARGE_CHAIN_STATE_DAMAGE_OVER_TIME = 4, // Held between the ticks of a damage-over-time reaction; has no step of its own
    ACTOR_503500_LARGE_CHAIN_STATE_DYING            = 5, // Health exhausted: drops its tip, leaves the boss, sinks, squashes flat and burns away
    ACTOR_503500_LARGE_CHAIN_STATE_SPLITTING        = 6, // Folds back into `bindPose`, then gives way to two lunging chains holding half its health each
};

/// Work block of a large chain, one of the two slot enemies (slots 2 and 3)
/// the boss starts the fight with.
///
/// The enemy is a nine-part model whose root hangs from part 1 of the boss
/// and whose last part, the tip, carries its target sphere. Nothing animates
/// it. Every frame the tip's position is stepped toward a target in the frame
/// of that boss part, cubic Beziers are drawn from the root to the tip, and
/// the links are aimed along samples of them, their lengths pulsing by a
/// sixty-fourth. Idle, the tip circles the slot's rest offset. Commanded by
/// the boss, the chain brings its tip over the player and fires a ballistic
/// shot from it.
///
/// Wearing a large chain down does not destroy it. Once its health is under
/// half it stops being a target, folds back into the pose its model was
/// created in and is hidden, and two lunging chains, each about half as wide,
/// are spawned on the same root with half its remaining health each. Only
/// health exhausted before the split begins makes it die in place.
///
/// One block per slot exists in a static array; the task's `Task::work`
/// points at its element.
typedef struct {
    MATRIX                lightMtx;                                        // Light matrix the model is lit with
    MATRIX                colorMtx;                                        // Colour matrix the model is lit with
    MATRIX                bindPose[ACTOR_503500_LARGE_CHAIN_PART_COUNT];   // Local matrix of each model part as the model was created; the laid-out pose gives way to it as `blendWeight` falls below 0x1000. Entry 0 is not used
    WorldCollisionBody    body;                                            // Target sphere of radius 800 on the tip; pair-tested until the chain dies or splits
    WorldCollisionContact contacts[8];                                     // Contact table of `body`, also the enemy's hit records
    EffectSpawnArg        hitEffect;                                       // Record hit effects on the chain are spawned with, bound to the tip
    void*                 field_248;                                       // Zeroed when the split's fold completes; only that zero word is ever stored and nothing reads it. Type and role unproven
    SVECTOR               linkPoints[ACTOR_503500_LARGE_CHAIN_PART_COUNT]; // World positions the links are aimed along, root end first, sampled from the Beziers
    SVECTOR               tipPosition;                                     // Where the chain ends, in the frame of the boss part the root hangs from
    SVECTOR               tipTarget;                                       // Position `tipPosition` is stepped toward, in the same frame
    SVECTOR               tipOrbitAngles;                                  // Euler angles, in 4096ths of a turn, of the idle tip target's 1000-unit offset from the rest offset; each advances every idle frame
    MATRIX                unscaledRootMatrix;                              // Root part's matrix saved once the dying chain has sunk; put back every frame before the collapse scale is applied
    s32                   tipSpeed;                                        // Speed of the tip toward `tipTarget`, 16.16 units per frame
    Fixed16               tipSpeedLimit;                                   // Top tip speed, 16.16: 128 normally, 48 once dying. A thirty-second of it is the per-frame acceleration; its integer half is the distance, summed over the axes, inside which the tip counts as arrived
    s16                   state;                                           // An `ACTOR_503500_LARGE_CHAIN_STATE_*` state
    s16                   holdFrames;                                      // Frames the hold state lasts; a stagger sets 5
    s16                   hitCooldown;                                     // Frames during which further hits are ignored; each hit that lands raises it to that attack's value
    s16                   slowFrames;                                      // Frames during which the tip moves at a quarter of its speed; a stagger or a damage-over-time tick sets 8
    s16                   collapseScaleY;                                  // Vertical scale of the dying chain, 0x1000 down to 0x200
    s16                   stateFrames;                                     // Frames counted by the current state's step
    s16                   pulsePhase;                                      // Phase of the link-length pulse in 4096ths of a turn, advanced 0x80 a frame
    s16                   blendWeight;                                     // Share of the laid-out pose in the model, from 0x1000 (all laid out) down to 0 (all `bindPose`); lowered only by the split
    s8                    stateStep;                                       // Step within the current state, restarted on every state change
    s8                    field_2E5;                                       // Cleared on every state change. Nothing reads it back; role unproven
    byte                  unknown_2E6[0x2];                                // No access found; role unproven
    s8                    tipArrived;                                      // 1 while `tipPosition` is within the arrival distance of `tipTarget`
    s8                    tipAdvancing;                                    // 1 while the tip accelerates toward its limit, 0 while it slows to a stop; set at set-up and never cleared
    s8                    detached;                                        // 1 once the dying chain's root has been re-parented onto the view; the chain is no longer steered or laid out
    s8                    bufferFreeCountdown;                             // Frames until the model's buffers are freed after it is hidden; negative when idle
} _Actor503500LargeChainWork;
STATIC_ASSERT_SIZEOF(_Actor503500LargeChainWork, 0x2EC);

/// Storage holding the boss's work block and the eight bytes after it.
///
/// `work` is the package's one `Actor503500Work`: the boss task's set-up
/// state clears exactly that member and parks it at `Task::work`, and the
/// accessors the slot enemies call address it directly. The eight bytes after
/// it lie between the work block and the table of slot counters. They are zero
/// in the image, neither neighbour's clear covers them and nothing in the
/// package addresses them, so they are not established as part of either
/// object; whether they are separate unreferenced variables is unproven. They
/// share this allocation only so the data after them keeps its address.
typedef struct {
    Actor503500Work work;           // The boss's work block
    byte            unknown_7E8[8]; // Zero in the image; no access established and role unproven
} _Actor503500WorkStorage;
STATIC_ASSERT_SIZEOF(_Actor503500WorkStorage, 0x7F0);

extern _Actor503500WorkStorage D_actor_503500_80176574;
/// 18-entry table of per-slot u16 counters, indexed by slot in
/// `_actor503500SetSlotEffectReservation` / `actor503500TryReserveSlotEffects` / `actor503500ReleaseSlotEffects`.
extern u16 D_actor_503500_80176D64[];
/// Main-executable globals with no module header yet: `gDisplayState.pendingMode` gates the
/// "everything is dead" message, `gPlayerStatus.hp` is the player's current HP and
/// `Gp_StateC08.mode` is 1 while the attachment wheel is open.
/// Main-executable flag byte cleared when the boss enters `ACTOR_503500_STATE_PART_LOST`; also written
/// by `mist_r18`, which has no module header for it either. Declared as an
/// array: `_actor503500StepPhaseTransitionState` needs the in-struct store, which keeps
/// the preceding `scriptedEffectTask` store ordered before it.
static s32  _actor503500CheckPartLossProgress(Task* task);
static void _actor503500ScheduleTargetable(Task* task, s8 targetable, s16 delayFrames);
static void _actor503500StepAttackState(Task* task);
static void _actor503500StepDefeatedState(Task* task);
static void _actor503500StepPhaseTransitionState(Task* task);
static void _actor503500StepCollapseState(Task* task);
static void _actor503500StepOrangeFlashAttack(Task* task);
static s32  _actor503500IsSlotAtRest(Actor503500Work* work, s32 slot);
static s32  _actor503500AttackPositiveSideChain(Task* task, Actor503500Work* work);
static s32  _actor503500AttackNegativeSideChain(Task* task, Actor503500Work* work);
static s32  _actor503500IsSlotReady(Actor503500Work* work, s32 slot);
static void _actor503500CommandSlot(Actor503500Work* work, s32 slot, s32 command, s32 cooldownFrames);

static void _actor503500StepIdleState(Task* task);
static void _actor503500StepPartLostState(Task* task);
static void _actor503500StepHeldState(Task* unusedTask);
static void _actor503500UpdateBodyCollisionGrid(Task* task, s32 initializeFaces, s32 moveAway);
static void _actor503500EnterCombatState(Task* task, s32 state);
static void _actor503500PinkFlashEmitterStepAttack(Task* task);
static void _actor503500LargeChainHandleReactions(Task* task);
static void _actor503500LargeChainLayoutLinks(Task* task);
static void func_actor_503500_8013A96C(Task* arg0);
static void _actor503500LargeChainProcessContacts(Task* task);
static void _actor503500LargeChainUpdateColor(Task* task);
static void _actor503500BossUpkeep(Task* task);
static void _actor503500TurnBoss(Task* task);
static void _actor503500WalkBoss(Task* task);
static void _actor503500UpdateSlotTargetEligibility(Task* task);
static void _actor503500HandleBossReactions(Task* task);
static void func_actor_503500_80136304(Task* arg0);
static void _actor503500ProcessBossContacts(Task* task);
static void _actor503500UpdateBossColor(Task* task);
static void _actor503500TickBossAnimation(Task* task);
static void _actor503500UpdateBossPartScales(Task* task);

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `_actor503500InitBoss`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

static void _actor503500ExitBoss(Task* task);

extern _Actor503500PinkFlashEmitterWork D_actor_503500_80176D88;

extern _Actor503500LargeChainWork D_actor_503500_80176EE8[2];

static void _actor503500LargeChainExit(Task* task);

static void _actor503500ApplyBossHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount);
static void _actor503500PinkFlashEmitterExit(Task* task);
static void _actor503500PinkFlashEmitterNoOp(Task* unusedTask);
static void _actor503500PinkFlashEmitterProcessContacts(Task* task);
static void _actor503500PinkFlashEmitterClearReactions(Task* task);
static void func_actor_503500_801383D0(Task* arg0);
static void _actor503500LargeChainEnterState(Task* task, s32 state);
static void _actor503500LargeChainStepIdle(Task* task);
static void _actor503500LargeChainStepShoot(Task* task);
static void _actor503500LargeChainStepSplitting(Task* task);
static void _actor503500LargeChainPlaceLinks(const SVECTOR* points, GfxCoord* coordinates, s32 pulsePhase);
static void _actor503500SetBossTrackRates(Task* task, s32 rate);

static void _actor503500InitBoss(Task* task);
static void func_actor_503500_80133270(Task* arg0);
static void _actor503500PinkFlashEmitterInit(Task* task);
static void func_actor_503500_8013815C(Task* arg0);
static void _actor503500LargeChainInit(Task* task);
static void func_actor_503500_80138898(Task* arg0);

static void _actor503500PinkFlashEmitterStepIdle(Task* task);
static void _actor503500PinkFlashEmitterEnterState(Task* task, s32 state);
static void _actor503500LargeChainSteerTip(Task* task);
static void _actor503500LargeChainBlendPose(Task* task);

/// `Task::state` handlers `actor503500BossTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131E44 = {
    {
        _actor503500InitBoss,
        func_actor_503500_80133270,
        _actor503500ExitBoss,
    },
};

Task* D_actor_503500_80176558 = NULL;

ActorTransform D_actor_503500_8017655C = { 0 };

_Actor503500WorkStorage D_actor_503500_80176574 = { 0 };

u16 D_actor_503500_80176D64[18] = { 0 };

_Actor503500PinkFlashEmitterWork D_actor_503500_80176D88;

_Actor503500LargeChainWork D_actor_503500_80176EE8[2];

static inline void _actor503500EnterBossState(Task* task, s16 state);
static void        _actor503500SetSlotEffectReservation(s32 slot, s16 effectCost);
static void        _actor503500PinkFlashEmitterStepDying(Task* task);
static void        _actor503500PinkFlashEmitterApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount);
static void        _actor503500LargeChainStepDying(Task* task);
static void        _actor503500LargeChainApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount);

/// Applies the boss placement's texture-page and CLUT offsets to a newly spawned slot.
///
/// Requires a live boss task, child model and loaded current-area placement.
/// The high nibble of the boss place key indexes that placement (0..15).
/// Rebuilds both existing packet halves and preserves their next-half selector.
static inline void _actor503500InheritBossSlotTextures(Task* task, Enemy* slotEnemy)
{
    GameLocationKey        location;
    const GameLocationKey* sessionLocation;
    u8                     viewId;
    AreaVariant*           areaVariant;
    const AreaPlacement*   placement;
    TmdObject*             slotModel;
    const Enemy*           placementOwner;
    u32                    placementKey;
    s32                    placementIndex;

    sessionLocation = &gGameSession->location.loc;
    placementOwner  = task->spawnArg2.pointer;
    placementKey    = placementOwner->placeKey;
    slotModel       = slotEnemy->task->extra.tmd;
    location.stage  = sessionLocation->stage;
    location.area   = sessionLocation->area;
    location.room   = sessionLocation->room;
    viewId          = sessionLocation->view;
    placementIndex  = placementKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view   = viewId;
    areaSyncLocationVariant(&location);
    areaVariant                  = areaGetVariant(&location);
    placement                    = gpAreaPlaceAt(areaVariant->placements, placementIndex);
    slotModel->texturePageOffset = placement->texturePageOffset;
    slotModel->clutRowOffset     = placement->clutRowOffset;
    if (slotModel->buffer != NULL) {
        tmdBuildBufferHalf(slotModel);
        tmdBuildBufferHalf(slotModel);
    }
}

/// Initializes the boss task, its target sphere and eleven starting slot enemies.
///
/// Requires a live body model, enemy and room collision grid. Borrows the
/// singleton boss work, clears its established work member, initializes full
/// contact storage and inherits textures from the current area placement
/// (index 0..15). Installs animation, message and exit handling, acquires a
/// battle reference and advances the task to its frame state.
static void _actor503500InitBoss(Task* task)
{
    enum {
        ACTOR_503500_BOSS_INIT_INACTIVE            = -1,
        ACTOR_503500_BOSS_INIT_WALK_SPEED_LIMIT    = 8 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_BOSS_INIT_ATTACK_DELAY_FRAMES = 90,
        ACTOR_503500_BOSS_INIT_OT_OFFSET           = 20,
        ACTOR_503500_BOSS_INIT_TARGET_PART         = 3,
        ACTOR_503500_BOSS_INIT_TARGET_RADIUS       = 600,
        ACTOR_503500_BOSS_INIT_HIT_EFFECT_ARG      = 0x600,
    };
    Enemy*                 slotEnemy;
    s32                    slotIndex;
    TmdObject*             bossModel;
    Enemy*                 enemy;
    GfxCoord*              rootCoord;
    GfxCoord*              targetCoord;
    WorldCollisionContact* contacts;
    // The boss owns this singleton work for the lifetime of its task.
    Actor503500Work* work = &D_actor_503500_80176574.work;

    bossModel = task->extra.tmd;
    enemy     = task->spawnArg2.pointer;
    rootCoord = bossModel->coords;
    memFillBytes(work, 0, sizeof(*work));
    task->work                 = work;
    work->animationId          = ACTOR_503500_BOSS_INIT_INACTIVE;
    work->animationSourceIndex = ACTOR_503500_BOSS_INIT_INACTIVE;
    work->bufferFreeCountdown  = ACTOR_503500_BOSS_INIT_INACTIVE;
    work->walkSpeedLimit       = ACTOR_503500_BOSS_INIT_WALK_SPEED_LIMIT;
    work->position.vx          = rootCoord->coord.t[0] << ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->position.vy          = rootCoord->coord.t[1] << ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->position.vz          = rootCoord->coord.t[2] << ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->advancing            = 1;
    work->attackDelay          = ACTOR_503500_BOSS_INIT_ATTACK_DELAY_FRAMES;
    bossModel->lightMtx        = &work->lightMtx;
    bossModel->colorMtx        = &work->colorMtx;
    bossModel->otOffset        = ACTOR_503500_BOSS_INIT_OT_OFFSET;
    rootCoord->composeStamp    = GRAPHICS_COORD_DIRTY;

    enemy->field_4                 = &rootCoord->coord;
    targetCoord                    = &rootCoord[ACTOR_503500_BOSS_INIT_TARGET_PART];
    enemy->field_48                = 0;
    enemy->coord                   = targetCoord;
    enemy->node.state.parts.flags |= (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    enemy->bodyPos.vx              = D_actor_503500_8016EC50.vx;
    enemy->bodyPos.vy              = D_actor_503500_8016EC50.vy;
    enemy->bodyPos.vz              = D_actor_503500_8016EC50.vz;
    contacts                       = work->contacts;
    enemy->param                   = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                    = contacts;
    enemy->hp                      = enemy->param->hpMax;

    work->body.coord            = targetCoord;
    work->body.context.contacts = contacts;
    work->body.key              = ACTOR_503500_PART_BODY_KEY;
    work->body.radius           = ACTOR_503500_BOSS_INIT_TARGET_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.pos.vx           = D_actor_503500_8016EC50.vx;
    work->body.pos.vy           = D_actor_503500_8016EC50.vy;
    work->body.pos.vz           = D_actor_503500_8016EC50.vz;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->hitEffect.spawnArgLo = ACTOR_503500_BOSS_INIT_HIT_EFFECT_ARG;
    work->hitEffect.coord      = targetCoord;
    work->hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    work->body.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    // Attach the starting targets and inherit the boss placement's textures.
    for (slotIndex = ACTOR_503500_SLOT_PINK_FLASH_EMITTER; slotIndex < ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER; slotIndex++) {
        slotEnemy = enemySpawnFromTable(D_actor_503500_8016E924, slotIndex, slotIndex, enemy);
        if (slotEnemy != NULL) {
            _actor503500InheritBossSlotTextures(task, slotEnemy);
            work->enemies[slotIndex] = slotEnemy;
        }
    }
    work->enemies[ACTOR_503500_SLOT_BODY] = enemy;
    (sceneAcquireBattleRef)(0x23);
    _actor503500UpdateBodyCollisionGrid(task, 1, 0);
    for (slotIndex = ARRAY_SIZE(D_actor_503500_80176D64) - 1; slotIndex >= 0; slotIndex--) {
        D_actor_503500_80176D64[slotIndex] = 0;
    }
    actor503500HandlePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, D_actor_503500_8016EAC0, 0);
    task->exitCallback = _actor503500ExitBoss;
    task->msgTable     = D_actor_503500_8016EA2C;
    task->state       += 1;
}

/// Per-frame update. `gSceneCombatState.actorControl` 1 pauses the boss (buffers kept, only
/// `_actor503500UpdateBossColor` runs), 2 hides it; anything else runs the
/// normal chain. `bufferFreeCountdown` counts down to the frame the TMD buffers are freed.
static void func_actor_503500_80133270(Task* arg0)
{
    Actor503500Work* work;
    Enemy*           enemy;
    TmdObject*       tmd;
    s32              mode;

    enemy = arg0->spawnArg2.pointer;
    tmd   = arg0->extra.tmd;
    mode  = gSceneCombatState.actorControl;
    work  = arg0->work;

    switch (mode) {
        case 1:
            if (work->controlPaused == 0) {
                sndEvtRequestScriptMute(SOUND_BANK_TYPE_CHARACTER_ALL);
                tmdAllocPrimitiveBuffer(tmd);
                tmd->flags         &= (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                work->controlPaused = mode;
                work->controlHidden = 0;
            }
            _actor503500UpdateBossColor(arg0);
            return;
        case 2:
            if (work->bufferFreeCountdown >= 0) {
                if (work->bufferFreeCountdown == 0) {
                    tmdFreePrimitiveBuffer(tmd);
                }
                work->bufferFreeCountdown--;
            }
            if (work->controlHidden == 0) {
                sndEvtRequestScriptMute(SOUND_BANK_TYPE_CHARACTER_ALL);
                tmd->flags               |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                work->bufferFreeCountdown = 1;
                work->controlPaused       = 0;
                work->controlHidden       = 1;
            }
            return;
        default:
            if (work->controlPaused == 1 || work->controlHidden == 1 || work->mutedForMenu != 0) {
                sndEvtRequestScriptUnmute(SOUND_BANK_TYPE_CHARACTER_ALL);
                work->controlPaused = 0;
                work->controlHidden = 0;
                work->mutedForMenu  = 0;
            }
            if ((gDisplayState.pendingMode & DISPLAY_MODE_MENU_GROUP_MASK) == DISPLAY_MODE_GAME_MENU_GROUP) {
                sndEvtRequestScriptMute(SOUND_BANK_TYPE_CHARACTER_ALL);
                work->mutedForMenu = 1;
            }
            if (gGameSession->eventState == 0) {
                tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (work->bufferFreeCountdown >= 0) {
                if (work->bufferFreeCountdown == 0) {
                    tmdFreePrimitiveBuffer(tmd);
                }
                work->bufferFreeCountdown--;
            }
            if (enemy->reactionFlags != 0) {
                _actor503500HandleBossReactions(arg0);
            }
            _actor503500ProcessBossContacts(arg0);
            _actor503500UpdateBossColor(arg0);
            _actor503500UpdateBodyCollisionGrid(arg0, 0, 0);
            _actor503500BossUpkeep(arg0);
            func_actor_503500_80136304(arg0);
            _actor503500TurnBoss(arg0);
            _actor503500WalkBoss(arg0);
            _actor503500TickBossAnimation(arg0);
            _actor503500UpdateSlotTargetEligibility(arg0);
            _actor503500UpdateBossPartScales(arg0);
            break;
    }
}

/// Refreshes the boss's idle cooldowns, random roll, desired yaw and pending target.
///
/// Requires live boss work, its enemy, and player/root translations in a common
/// parent frame. Cooldowns reach zero only while idle. The X/Z offset narrows
/// to signed halfwords before measuring yaw; the offset yaw wraps to
/// [-2048, 2048) at 4096 units per turn. Decremented target countdowns apply
/// at zero; a negative result resets the countdown without applying.
static void _actor503500BossUpkeep(Task* task)
{
    Actor503500Work* work;
    GfxCoord*        rootCoord;
    Enemy*           enemy;
    SVECTOR          playerOffset;
    s16              targetYaw;
    s16              targetableFramesLeft;
    s16*             slotCooldown;
    s32              slotIndex;

    work         = task->work;
    slotCooldown = work->slotCooldown;
    if (work->state == ACTOR_503500_STATE_IDLE) {
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->slotCooldown); slotIndex++, slotCooldown++) {
            if (--*slotCooldown < 0) {
                *slotCooldown = 0;
            }
        }
    }
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->randomRoll = gRandomLcgState >> 16;
    rootCoord        = task->extra.tmd->coords;
    playerOffset.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerOffset.vy  = 0;
    playerOffset.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    targetYaw        = work->targetYawOffset + ratan2(playerOffset.vx, playerOffset.vz);
    while (targetYaw >= ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        targetYaw -= ACTOR_TRANSFORM_ANGLE_TURN;
    }
    while (targetYaw < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        targetYaw += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    work->targetYaw      = targetYaw;
    enemy                = task->spawnArg2.pointer;
    targetableFramesLeft = --work->targetableDelay;
    if (targetableFramesLeft < 0) {
        work->targetableDelay = 0;
    } else if (targetableFramesLeft == 0) {
        if (work->targetablePending != 0) {
            work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            worldTargetLinkNode(&enemy->node);
        } else {
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldTargetUnlinkNode(&enemy->node);
        }
    }
}

/// Exposes replacement targets after part loss and reports the later fight phase.
///
/// Returns 1 to defer idle attack selection while issuing an exposure command or
/// while a phase transition awaits its room event. The event is sent once, when
/// player control, health, attachment mode and display state allow it. Slots
/// 10/11 must remain live until their exposure commands have been sent.
static s32 _actor503500CheckPartLossProgress(Task* task)
{
    enum {
        ACTOR_503500_PROGRESS_LARGE_ORB_0_SLOT = 4,
        ACTOR_503500_PROGRESS_LARGE_ORB_1_SLOT = 5,
    };
    Actor503500Work* work;
    Enemy**          slotEnemies;
    Enemy*           pinkEmitter;
    s32              deferAttack;

    deferAttack = 0;
    work        = task->work;
    slotEnemies = work->enemies;

    if (work->state != ACTOR_503500_STATE_PART_LOST) {
        // Losing each outer target exposes the target it previously protected.
        if (!(work->progressFlags & ACTOR_503500_PROGRESS_ARM_0_EXPOSED) && (slotEnemies[ACTOR_503500_PROGRESS_LARGE_ORB_0_SLOT] == NULL)) {
            slotEnemies[ACTOR_503500_SLOT_ARM_0]->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
            deferAttack                                               = 1;
            work->progressFlags                                      |= ACTOR_503500_PROGRESS_ARM_0_EXPOSED;
        }
        if (!((work->progressFlags >> ACTOR_503500_PROGRESS_ARM_1_SHIFT) & 1) && (slotEnemies[ACTOR_503500_PROGRESS_LARGE_ORB_1_SLOT] == NULL)) {
            slotEnemies[ACTOR_503500_SLOT_ARM_1]->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
            deferAttack                                               = 1;
            work->progressFlags                                      |= ACTOR_503500_PROGRESS_ARM_1_EXPOSED;
        }
        if (!((work->progressFlags >> ACTOR_503500_PROGRESS_YELLOW_FLASH_SHIFT) & 1) && (slotEnemies[ACTOR_503500_SLOT_PINK_FLASH_EMITTER] == NULL)) {
            if (slotEnemies[ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER] != NULL) {
                slotEnemies[ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER]->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
                deferAttack                                                              = 1;
                work->progressFlags                                                     |= ACTOR_503500_PROGRESS_YELLOW_FLASH_EXPOSED;
            }
        }
    }

    // Keep deferring attacks even when presentation gates postpone the event.
    if (!((work->progressFlags >> ACTOR_503500_PROGRESS_ROOM_EVENT_SHIFT) & 1) &&
        ((((pinkEmitter = slotEnemies[ACTOR_503500_SLOT_PINK_FLASH_EMITTER], pinkEmitter == NULL)) && (slotEnemies[ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER] == NULL)) ||
         (slotEnemies[ACTOR_503500_SLOT_CHAIN_BASE_0] == NULL) || (slotEnemies[ACTOR_503500_SLOT_CHAIN_BASE_1] == NULL) || (slotEnemies[ACTOR_503500_SLOT_ARM_0] == NULL) ||
         (slotEnemies[ACTOR_503500_SLOT_ARM_1] == NULL) || ((slotEnemies[ACTOR_503500_SLOT_SMALL_ORB_EMITTER] == NULL) && (pinkEmitter == NULL)) ||
         ((slotEnemies[ACTOR_503500_PROGRESS_LARGE_ORB_0_SLOT]->hp == 0) && (slotEnemies[ACTOR_503500_PROGRESS_LARGE_ORB_1_SLOT]->hp == 0)))) {
        if ((((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode != GAME_ACTOR_MODE_SCRIPTED) &&
            (gPlayerStatus.hp > 0) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL)) {
            if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
                work->progressFlags |= ACTOR_503500_PROGRESS_ROOM_EVENT_SENT;
            }
        }
        deferAttack = 1;
    }
    return deferAttack;
}

/// Updates the attack picker's player-height band with asymmetric hysteresis.
///
/// Reads the saved previous band (0 upper, 1 middle, 2 lower) and live player
/// Y in game coordinates. Upper leaves at Y >= -2799; middle enters upper at
/// Y < -3200 or lower at Y >= -799; lower leaves at Y < -1200. Otherwise the
/// current band is retained. Initialization and attack selection keep both
/// bands in 0..2; Y is used directly without a coordinate conversion.
static inline void _actor503500ClassifyAttackHeight(Actor503500Work* work)
{
    enum {
        ACTOR_503500_HEIGHT_BAND_UPPER    = 0,
        ACTOR_503500_HEIGHT_BAND_MIDDLE   = 1,
        ACTOR_503500_HEIGHT_BAND_LOWER    = 2,
        ACTOR_503500_HEIGHT_UPPER_EXIT_Y  = -2799,
        ACTOR_503500_HEIGHT_UPPER_ENTER_Y = -3200,
        ACTOR_503500_HEIGHT_LOWER_ENTER_Y = -799,
        ACTOR_503500_HEIGHT_LOWER_EXIT_Y  = -1200,
    };
    switch (work->previousHeightBand) {
        case ACTOR_503500_HEIGHT_BAND_UPPER:
            if (gPlayerStatus.coordMtx->t[1] >= ACTOR_503500_HEIGHT_UPPER_EXIT_Y) {
                work->heightBand = ACTOR_503500_HEIGHT_BAND_MIDDLE;
            }
            break;
        case ACTOR_503500_HEIGHT_BAND_MIDDLE:
            if (gPlayerStatus.coordMtx->t[1] >= ACTOR_503500_HEIGHT_LOWER_ENTER_Y) {
                work->heightBand = ACTOR_503500_HEIGHT_BAND_LOWER;
            } else if (gPlayerStatus.coordMtx->t[1] < ACTOR_503500_HEIGHT_UPPER_ENTER_Y) {
                work->heightBand = ACTOR_503500_HEIGHT_BAND_UPPER;
            }
            break;
        case ACTOR_503500_HEIGHT_BAND_LOWER:
            if (gPlayerStatus.coordMtx->t[1] < ACTOR_503500_HEIGHT_LOWER_EXIT_Y) {
                work->heightBand = ACTOR_503500_HEIGHT_BAND_MIDDLE;
            }
            break;
    }
}

/// Picks a weighted boss attack, then runs it once per attack-state frame.
///
/// Refreshes the player's relative bearing in [-2048, 2048), at 4096 units per
/// turn. Picking selects one of two sets of three height bands and four bearing
/// bands; height changes have hysteresis. The low random byte selects the first
/// cumulative weight reaching it. An empty list waits 30 idle frames, and a roll
/// beyond its total waits 15. A picked attack runs immediately and thereafter
/// until its nonzero result supplies the next idle delay in frames.
static void _actor503500StepAttackState(Task* task)
{
    enum {
        ACTOR_503500_ATTACK_STATE_PICK                   = 0,
        ACTOR_503500_ATTACK_STATE_RUN                    = 1,
        ACTOR_503500_ATTACK_UNSELECTED_IDLE_DELAY_FRAMES = 15,
        ACTOR_503500_ATTACK_EMPTY_IDLE_DELAY_FRAMES      = 30,
    };
    Actor503500Work*               work;
    GfxCoord*                      coord;
    const Actor503500AttackChoice* choice;
    s32                            attackTableIndex;
    const s16*                     bearingThreshold;
    s32                            playerBearing;
    s32                            bearingMagnitude;
    u32                            cumulativeWeight;
    u32                            randomRoll;
    s32                            idleDelay;

    work          = task->work;
    coord         = task->extra.tmd->coords;
    playerBearing = ratan2(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0], gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]) - work->yaw;
    while (playerBearing >= ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        playerBearing -= ACTOR_TRANSFORM_ANGLE_TURN;
    }
    while (playerBearing < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        playerBearing += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    work->playerBearing = playerBearing;
    switch ((s8)work->stateStep) {
        case ACTOR_503500_ATTACK_STATE_PICK:
            // Classify the current player position only when choosing an attack.
            work->previousHeightBand  = work->heightBand;
            work->previousBearingBand = work->bearingBand;
            _actor503500ClassifyAttackHeight(work);
            bearingThreshold = D_actor_503500_8016EF28[(work->progressFlags >> ACTOR_503500_PROGRESS_ROOM_EVENT_SHIFT) & 1][work->heightBand];
            bearingMagnitude = abs(playerBearing);
            for (work->bearingBand = 0; *bearingThreshold++ < bearingMagnitude; work->bearingBand++) {
            }
            attackTableIndex = (work->progressFlags >> ACTOR_503500_PROGRESS_ROOM_EVENT_SHIFT) & 1;
            choice           = D_actor_503500_8016EF10[attackTableIndex][work->heightBand][work->bearingBand];
            randomRoll       = (u8)work->randomRoll;
            if (choice->attack != NULL) {
                cumulativeWeight = choice->weight;
                while (cumulativeWeight < randomRoll) {
                    if (choice[1].attack == NULL) {
                        _actor503500EnterCombatState(task, ACTOR_503500_STATE_IDLE);
                        work->attackDelay = ACTOR_503500_ATTACK_UNSELECTED_IDLE_DELAY_FRAMES;
                        return;
                    }
                    choice++;
                    cumulativeWeight += choice->weight;
                }
                work->runningAttack = choice->attack;
                work->stateStep++;
            } else {
                _actor503500EnterCombatState(task, ACTOR_503500_STATE_IDLE);
                work->attackDelay = ACTOR_503500_ATTACK_EMPTY_IDLE_DELAY_FRAMES;
                return;
            }
        case ACTOR_503500_ATTACK_STATE_RUN:
            idleDelay = work->runningAttack(task, work);
            if (idleDelay != 0) {
                _actor503500EnterCombatState(task, ACTOR_503500_STATE_IDLE);
                work->attackDelay = idleDelay;
            }
            break;
        default:
            _actor503500EnterCombatState(task, ACTOR_503500_STATE_IDLE);
            break;
    }
}

s32 actor503500AttackArmStrike(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_ARM_ATTACK_COOLDOWN_FRAMES = 120,
    };
    s32 idleDelay;
    s32 firstArmOffset;
    s32 slot;

    idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            // The forward band always enters the wait, retaining the old slot if neither starts.
            if (work->bearingBand == 0) {
                firstArmOffset = work->randomRoll & 1;
                slot           = firstArmOffset + ACTOR_503500_SLOT_ARM_0;
                if (_actor503500IsSlotReady(work, slot) != 0 ||
                    (slot = ACTOR_503500_SLOT_ARM_1 - firstArmOffset, _actor503500IsSlotReady(work, slot) != 0)) {
                    _actor503500CommandSlot(work, slot, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_ARM_ATTACK_COOLDOWN_FRAMES);
                    work->attackSlot = slot;
                }
                idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                work->targetYawOffset = 0;
                work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
            } else if (work->playerBearing > 0) {
                if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_ARM_0) != 0) {
                    _actor503500CommandSlot(work, ACTOR_503500_SLOT_ARM_0, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_ARM_ATTACK_COOLDOWN_FRAMES);
                    idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                    work->attackSlot      = ACTOR_503500_SLOT_ARM_0;
                    work->targetYawOffset = 0;
                    work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
                }
            } else if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_ARM_1) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_ARM_1, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_ARM_ATTACK_COOLDOWN_FRAMES);
                idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                work->attackSlot      = ACTOR_503500_SLOT_ARM_1;
                work->targetYawOffset = 0;
                work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            if (_actor503500IsSlotAtRest(work, work->attackSlot) == 0) {
                idleDelay = ACTOR_503500_ATTACK_RUNNING;
            }
            break;
    }
    return idleDelay;
}

/// Starts a selected side-chain attack and remembers its slot for completion.
///
/// `slot` is the selected large chain, lunging chain or chain-base slot (2/3,
/// 13..16 or 7/8). `cooldownFrames` counts boss idle upkeep ticks and narrows to
/// a signed halfword. The command starts the slot's busy period and the running
/// attack enters its wait phase; the live work block retains its attack timer.
static inline void _actor503500StartSideChainAttack(Actor503500Work* work, s16 slot, s32 cooldownFrames)
{
    _actor503500CommandSlot(work, slot, ACTOR_503500_SLOT_COMMAND_ATTACK, cooldownFrames);
    work->attackSlot  = slot;
    work->attackPhase = ACTOR_503500_ATTACK_PHASE_WAIT;
}

/// Tries the positive-bearing side's large chain, lunging chains and base.
///
/// Tries slots 2, 13, 14, 7 in descriptor-table order. Bearing gates use
/// 4096 units per turn, and each candidate takes the corresponding cooldown.
/// Returns 0 while running and 1 if no eligible slot starts. If called again
/// in the wait phase, stops at rest or after more than 30 timed calls and
/// returns 30 frames for a chain or 60 for the base; the base never advances
/// the timer. The combined side attack uses only this helper's command phase.
/// `task` and `work` are the boss task and its work block.
static s32 _actor503500AttackPositiveSideChain(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_SIDE_CHAIN_WAIT_LIMIT_FRAMES = 30,
        ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES = 30,
        ACTOR_503500_SIDE_BASE_IDLE_DELAY_FRAMES  = 60,
    };
    s32        idleDelay;
    s32        candidateIndex;
    const s16* candidateSlots;
    s16        playerBearing;
    s16        slot;

    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            idleDelay      = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
            candidateIndex = 0;
            candidateSlots = D_actor_503500_8016EF48;
            playerBearing  = work->playerBearing;
            // Choose the first ready candidate whose bearing and partner-presence gate passes.
            for (; candidateIndex < (s32)ARRAY_SIZE(D_actor_503500_8016EF48); candidateIndex++) {
                slot = *candidateSlots;
                if (_actor503500IsSlotReady(work, slot) != 0) {
                    switch (slot) {
                        case ACTOR_503500_SLOT_LARGE_CHAIN_0:
                            if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_OUTER_BEARING && playerBearing <= ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING)) {
                                idleDelay = ACTOR_503500_ATTACK_RUNNING;
                            }
                            break;
                        case ACTOR_503500_SLOT_LUNGING_CHAIN_0:
                            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_1) != 0) {
                                if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_REAR_BEARING && playerBearing <= ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING)) {
                                    idleDelay = ACTOR_503500_ATTACK_RUNNING;
                                }
                            } else if (playerBearing >= (ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING + 1) && playerBearing <= (ACTOR_503500_SIDE_CHAIN_OUTER_BEARING - 1)) {
                                idleDelay = ACTOR_503500_ATTACK_RUNNING;
                            }
                            break;
                        case ACTOR_503500_SLOT_LUNGING_CHAIN_1:
                            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_0) != 0) {
                                if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_REAR_BEARING && playerBearing <= ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING)) {
                                    idleDelay = ACTOR_503500_ATTACK_RUNNING;
                                }
                            } else if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_REAR_BEARING && playerBearing <= (ACTOR_503500_SIDE_CHAIN_OUTER_BEARING - 1))) {
                                idleDelay = ACTOR_503500_ATTACK_RUNNING;
                            }
                            break;
                        case ACTOR_503500_SLOT_CHAIN_BASE_0:
                            if (playerBearing >= (ACTOR_503500_CHAIN_BASE_INNER_BEARING + 1) && playerBearing <= (ACTOR_503500_CHAIN_BASE_OUTER_BEARING - 1)) {
                                idleDelay = ACTOR_503500_ATTACK_RUNNING;
                            }
                            break;
                    }
                    if (idleDelay == ACTOR_503500_ATTACK_RUNNING) {
                        _actor503500StartSideChainAttack(work, slot, D_actor_503500_8016EF40[candidateIndex]);
                        break;
                    }
                }
                candidateSlots++;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            if (work->attackSlot != ACTOR_503500_SLOT_CHAIN_BASE_0) {
                work->attackFrames++;
            }
            if (_actor503500IsSlotAtRest(work, work->attackSlot) != 0 || work->attackFrames > ACTOR_503500_SIDE_CHAIN_WAIT_LIMIT_FRAMES) {
                switch (work->attackSlot) {
                    case ACTOR_503500_SLOT_LARGE_CHAIN_0:
                        idleDelay = ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_0:
                        idleDelay = ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_1:
                        idleDelay = ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_CHAIN_BASE_0:
                        idleDelay = ACTOR_503500_SIDE_BASE_IDLE_DELAY_FRAMES;
                        break;
                }
            }
            break;
        default:
            idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
            break;
    }
    return idleDelay;
}

/// Tries the nonpositive-bearing side's large chain, lunging chains and base.
///
/// Tries slots 3, 16, 15, 8 in descriptor-table order, with reflected bearing
/// gates in 4096ths of a turn and per-candidate cooldowns. Returns 0 while
/// running and 1 if no eligible slot starts. If called again in the wait phase,
/// stops at rest or after more than 20 timed calls and returns 30 frames for
/// a chain or 60 for the base; the base never advances the timer. The combined
/// side attack uses only this helper's command phase. `task` and `work` are
/// the boss task and its work block.
static s32 _actor503500AttackNegativeSideChain(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_SIDE_CHAIN_WAIT_LIMIT_FRAMES = 20,
        ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES = 30,
        ACTOR_503500_SIDE_BASE_IDLE_DELAY_FRAMES  = 60,
    };
    s32        idleDelay;
    s32        candidateIndex;
    const s16* candidateSlots;
    s16        playerBearing;
    s16        slot;

    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            idleDelay      = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
            candidateIndex = 0;
            candidateSlots = D_actor_503500_8016EF50;
            playerBearing  = work->playerBearing;
            // Choose the first ready candidate whose bearing and partner-presence gate passes.
            for (; candidateIndex < (s32)ARRAY_SIZE(D_actor_503500_8016EF50); candidateIndex++) {
                slot = *candidateSlots;
                if (_actor503500IsSlotReady(work, slot) != 0) {
                    switch (slot) {
                        case ACTOR_503500_SLOT_LARGE_CHAIN_1:
                            if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING && playerBearing <= ACTOR_503500_SIDE_CHAIN_OUTER_BEARING)) {
                                idleDelay = ACTOR_503500_ATTACK_RUNNING;
                            }
                            break;
                        case ACTOR_503500_SLOT_LUNGING_CHAIN_2:
                            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_3) != 0) {
                                if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING && playerBearing <= ACTOR_503500_SIDE_CHAIN_REAR_BEARING)) {
                                    idleDelay = ACTOR_503500_ATTACK_RUNNING;
                                }
                            } else if (!(playerBearing >= -(ACTOR_503500_SIDE_CHAIN_OUTER_BEARING - 1) && playerBearing <= ACTOR_503500_SIDE_CHAIN_REAR_BEARING)) {
                                idleDelay = ACTOR_503500_ATTACK_RUNNING;
                            }
                            break;
                        case ACTOR_503500_SLOT_LUNGING_CHAIN_3:
                            if (actor503500IsSlotEmpty(task, ACTOR_503500_SLOT_LUNGING_CHAIN_2) != 0) {
                                if (!(playerBearing >= -ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING && playerBearing <= ACTOR_503500_SIDE_CHAIN_REAR_BEARING)) {
                                    idleDelay = ACTOR_503500_ATTACK_RUNNING;
                                }
                            } else if (playerBearing < -ACTOR_503500_SIDE_CHAIN_FORWARD_BEARING) {
                                if (playerBearing >= -(ACTOR_503500_SIDE_CHAIN_OUTER_BEARING - 1)) {
                                    idleDelay = ACTOR_503500_ATTACK_RUNNING;
                                }
                            }
                            break;
                        case ACTOR_503500_SLOT_CHAIN_BASE_1:
                            if (playerBearing < -ACTOR_503500_CHAIN_BASE_INNER_BEARING) {
                                if (playerBearing >= -(ACTOR_503500_CHAIN_BASE_OUTER_BEARING - 1)) {
                                    idleDelay = ACTOR_503500_ATTACK_RUNNING;
                                }
                            }
                            break;
                    }
                    if (idleDelay == ACTOR_503500_ATTACK_RUNNING) {
                        _actor503500StartSideChainAttack(work, slot, D_actor_503500_8016EF40[candidateIndex]);
                        break;
                    }
                }
                candidateSlots++;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            if (work->attackSlot != ACTOR_503500_SLOT_CHAIN_BASE_1) {
                work->attackFrames++;
            }
            if (_actor503500IsSlotAtRest(work, work->attackSlot) != 0 || work->attackFrames > ACTOR_503500_SIDE_CHAIN_WAIT_LIMIT_FRAMES) {
                switch (work->attackSlot) {
                    case ACTOR_503500_SLOT_LARGE_CHAIN_1:
                        idleDelay = ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_2:
                        idleDelay = ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_3:
                        idleDelay = ACTOR_503500_SIDE_CHAIN_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_CHAIN_BASE_1:
                        idleDelay = ACTOR_503500_SIDE_BASE_IDLE_DELAY_FRAMES;
                        break;
                }
            }
            break;
        default:
            idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
            break;
    }
    return idleDelay;
}

s32 actor503500AttackSideChain(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_SIDE_ATTACK_WAIT_LIMIT_FRAMES          = 90,
        ACTOR_503500_LARGE_CHAIN_ATTACK_IDLE_DELAY_FRAMES   = 15,
        ACTOR_503500_LUNGING_CHAIN_ATTACK_IDLE_DELAY_FRAMES = 60,
        ACTOR_503500_BASE_ATTACK_IDLE_DELAY_FRAMES          = 90,
        ACTOR_503500_SIDE_ATTACK_DEFAULT_IDLE_DELAY_FRAMES  = 30,
    };
    s32 idleDelay;
    s32 playerBearing;

    playerBearing = work->playerBearing;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            // Preserve the original opposite-side fallback tests, although the sign gate excludes them.
            if (playerBearing > 0) {
                idleDelay = _actor503500AttackPositiveSideChain(task, work);
                if (idleDelay == ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES && playerBearing < -ACTOR_503500_CHAIN_BASE_OUTER_BEARING) {
                    idleDelay = _actor503500AttackNegativeSideChain(task, work);
                }
            } else {
                idleDelay = _actor503500AttackNegativeSideChain(task, work);
                if (idleDelay == ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES && playerBearing > ACTOR_503500_CHAIN_BASE_OUTER_BEARING) {
                    idleDelay = _actor503500AttackPositiveSideChain(task, work);
                }
            }
            if (idleDelay == ACTOR_503500_ATTACK_RUNNING) {
                work->targetYawOffset = ACTOR_503500_ATTACK_REAR_YAW_OFFSET;
            } else {
                work->targetYawOffset = 0;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            // Bases wait for rest without advancing the timeout.
            if ((u16)work->attackSlot - ACTOR_503500_SLOT_CHAIN_BASE_0 >= 2U) {
                work->attackFrames++;
            }
            if (_actor503500IsSlotAtRest(work, work->attackSlot) != 0 || work->attackFrames > ACTOR_503500_SIDE_ATTACK_WAIT_LIMIT_FRAMES) {
                switch (work->attackSlot) {
                    case ACTOR_503500_SLOT_LARGE_CHAIN_0:
                    case ACTOR_503500_SLOT_LARGE_CHAIN_1:
                        idleDelay = ACTOR_503500_LARGE_CHAIN_ATTACK_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_0:
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_1:
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_2:
                    case ACTOR_503500_SLOT_LUNGING_CHAIN_3:
                        idleDelay = ACTOR_503500_LUNGING_CHAIN_ATTACK_IDLE_DELAY_FRAMES;
                        break;
                    case ACTOR_503500_SLOT_CHAIN_BASE_0:
                    case ACTOR_503500_SLOT_CHAIN_BASE_1:
                        idleDelay = ACTOR_503500_BASE_ATTACK_IDLE_DELAY_FRAMES;
                        break;
                    default:
                        idleDelay = ACTOR_503500_SIDE_ATTACK_DEFAULT_IDLE_DELAY_FRAMES;
                        break;
                }
            }
            break;
        default:
            idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
            break;
    }
    return idleDelay;
}

/// Removes the defeated boss's target and reports its defeat to the room.
///
/// Cancels room effects, releases the body's effect reservation, snapshots
/// slots 1..16's remaining HP as a signed halfword sum, and plays recoil at
/// twice normal rate. After 31 state updates, sends the room actor event once
/// player control, health, attachment mode and display permit it, and fades
/// the death sound over 45 frames. The model and attached slots remain live.
static void _actor503500StepDefeatedState(Task* task)
{
    enum {
        ACTOR_503500_DEFEATED_STEP_REMOVE_TARGET = 0,
        ACTOR_503500_DEFEATED_STEP_WAIT_EVENT    = 1,
        ACTOR_503500_DEFEATED_EVENT_DELAY_FRAMES = 31,
        ACTOR_503500_DEFEATED_SOUND_FADE_FRAMES  = 45,
    };
    Actor503500Work* work;
    Enemy*           enemy;
    s32              slotIndex;
    s16              partsHpSum;
    s32              audioPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    switch ((s8)work->stateStep) {
        case ACTOR_503500_DEFEATED_STEP_REMOVE_TARGET:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            actor503500ReleaseSlotEffects(task->spawnArg1.value);
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = NULL;
            work->hitCooldown = 0;
            roomEffectRequestCancelAll();
            // Retain the 16-bit health sum used by the successor encounter.
            partsHpSum = 0;
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->enemies); slotIndex++) {
                if (work->enemies[slotIndex] != NULL) {
                    partsHpSum += work->enemies[slotIndex]->hp;
                }
            }
            gGameSession->bossPartsHpSum = partsHpSum;
            work->selfAttackCommand      = ACTOR_503500_SLOT_COMMAND_NONE;
            actor503500PlayAnimationPreset(task, ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET, 2 * ANIMATION_RATE_ONE);
            audioPan = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[3]);
            sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, audioPan,
                                     (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[3]) / 2));
            work->stateStep = work->stateStep + 1;
            break;
        case ACTOR_503500_DEFEATED_STEP_WAIT_EVENT:
            if (++work->stateFrames >= ACTOR_503500_DEFEATED_EVENT_DELAY_FRAMES &&
                ((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode != GAME_ACTOR_MODE_SCRIPTED &&
                gPlayerStatus.hp > 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, ACTOR_503500_DEFEATED_SOUND_FADE_FRAMES);
                work->stateStep = work->stateStep + 1;
            }
            break;
    }
}

/// Spawns and attaches an orange flash to the boss's emitting part.
///
/// Requires a live boss model with part 3; chargeFrames is evaluated once and
/// counts updating ticks. Captures task, flashTask and flashCoord; leaves NULL
/// in flashTask on failure. The coordinate borrows its parent without joining
/// the boss's task tree. Expands to multiple statements; use in a braced block.
#define ACTOR_503500_SPAWN_ATTACHED_ORANGE_FLASH(chargeFrames)                                                     \
    flashTask = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_ORANGE_FLASH_TASK, (chargeFrames), task); \
    if (flashTask != NULL) {                                                                                       \
        flashCoord             = flashTask->extra.coordBody->coord;                                                \
        flashCoord->parent     = &task->extra.tmd->coords[ACTOR_503500_ORANGE_FLASH_PARENT_PART];                  \
        flashCoord->coord.t[0] = D_actor_503500_8016EC50.vx;                                                       \
        flashCoord->coord.t[1] = D_actor_503500_8016EC50.vy;                                                       \
        flashCoord->coord.t[2] = D_actor_503500_8016EC50.vz;                                                       \
    }

/// Runs the boss's phase-transition spray, held flash and return to combat.
///
/// Requires initialized boss work, model part 3 and the room's phase-transition
/// script. The script saves/repositions the root and restores it with command 5;
/// this handler waits for that restoration before replacing its first orange
/// flash with a 90-tick charge. Spawn failures still advance the sequence.
/// The first flash is borrowed through `scriptedEffectTask`; its exit callback
/// must be installed before restoration. Animation completion returns to idle
/// and restores the ordinary ordering-table depth scale.
static void _actor503500StepPhaseTransitionState(Task* task)
{
    enum {
        ACTOR_503500_PHASE_STEP_BEGIN                = 0,
        ACTOR_503500_PHASE_STEP_SPRAY                = 1,
        ACTOR_503500_PHASE_STEP_WAIT_ROOT            = 2,
        ACTOR_503500_PHASE_STEP_CHARGE               = 3,
        ACTOR_503500_PHASE_STEP_STRIKE               = 4,
        ACTOR_503500_PHASE_STEP_RECOVER              = 5,
        ACTOR_503500_PHASE_STEP_FINISH               = 6,
        ACTOR_503500_PHASE_ANIMATION_BEGIN           = 19,
        ACTOR_503500_PHASE_PUFF_FRAMES               = 120,
        ACTOR_503500_PHASE_SOUND                     = 0x12,
        ACTOR_503500_PHASE_SOUND_START_FRAME         = 2,
        ACTOR_503500_PHASE_SOUND_FADE_FRAMES         = 60,
        ACTOR_503500_PHASE_FLASH_SPAWN_FRAME         = 151,
        ACTOR_503500_PHASE_FIRST_FLASH_CHARGE_FRAMES = 100,
        ACTOR_503500_PHASE_PUFF_SPAWN_ARG            = (0x01000000 | 0x1000 | 0x800), // Scaled direction, size 2048, one tick per cell
    };
    Actor503500Work* work;
    GfxCoord*        flashCoord;
    Task*            flashTask;
    s32              pan;

    work = task->work;
    switch ((s8)work->stateStep) {
        case ACTOR_503500_PHASE_STEP_BEGIN:
            actor503500PlayAnimationPreset(task, ACTOR_503500_PHASE_ANIMATION_BEGIN, ANIMATION_RATE_ONE);
            actor503500ReleaseSlotEffects(task->spawnArg1.value);
            work->scriptedEffectTask = NULL;
            work->stateStep          = work->stateStep + 1;
            break;
        case ACTOR_503500_PHASE_STEP_SPRAY:
            // Spray cyclic and random directions while the room scene repositions the boss.
            if (work->stateFrames < ACTOR_503500_PHASE_PUFF_FRAMES) {
                effectSpawn(EFFECT_HIT_PUFF, &task->extra.tmd->coords[ACTOR_503500_ORANGE_FLASH_PARENT_PART], ACTOR_503500_PHASE_PUFF_SPAWN_ARG,
                            &D_actor_503500_8016EF58[(s16)(work->stateFrames % (s32)ARRAY_SIZE(D_actor_503500_8016EF58))]);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_HIT_PUFF, &task->extra.tmd->coords[ACTOR_503500_ORANGE_FLASH_PARENT_PART], ACTOR_503500_PHASE_PUFF_SPAWN_ARG,
                            &D_actor_503500_8016EF58[(u16)((gRandomLcgState >> 16) % ARRAY_SIZE(D_actor_503500_8016EF58))]);
            }
            if (work->stateFrames == ACTOR_503500_PHASE_PUFF_FRAMES) {
                sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_PHASE_SOUND), ACTOR_503500_PHASE_SOUND_FADE_FRAMES);
            }
            if (work->stateFrames == ACTOR_503500_PHASE_SOUND_START_FRAME) {
                flashCoord = &task->extra.tmd->coords[ACTOR_503500_ORANGE_FLASH_PARENT_PART];
                pan        = (s8)worldCoordGetOriginAudioPan(flashCoord);
                sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, ACTOR_503500_PHASE_SOUND), pan, (s8)(worldCoordGetOriginAudioDepth(flashCoord) / 2));
            }
            if (++work->stateFrames >= ACTOR_503500_PHASE_FLASH_SPAWN_FRAME) {
                ACTOR_503500_SPAWN_ATTACHED_ORANGE_FLASH(ACTOR_503500_PHASE_FIRST_FLASH_CHARGE_FRAMES);
                work->scriptedEffectTask   = flashTask;
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_2X;
                work->stateStep            = work->stateStep + 1;
            }
            break;
        case ACTOR_503500_PHASE_STEP_WAIT_ROOT:
            // The restore-root command releases the held flash and resumes the attack sequence.
            if (work->rootCoordRestored != 0) {
                flashTask = work->scriptedEffectTask;
                if (flashTask != NULL) {
                    flashTask->exitCallback(flashTask);
                }
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_CHARGE, ANIMATION_RATE_ONE);
                ACTOR_503500_SPAWN_ATTACHED_ORANGE_FLASH(ACTOR_503500_ORANGE_FLASH_CHARGE_FRAMES);
                _actor503500ScheduleTargetable(task, 1, 1);
                work->stateStep = work->stateStep + 1;
            }
            break;
        case ACTOR_503500_PHASE_STEP_CHARGE:
            if (actor503500HasAnimationFinished(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_CHARGE) != 0) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_STRIKE, ANIMATION_RATE_ONE);
                work->stateFrames = 0;
                work->stateStep   = work->stateStep + 1;
            }
            break;
        case ACTOR_503500_PHASE_STEP_STRIKE:
            if (++work->stateFrames >= ACTOR_503500_ORANGE_FLASH_STRIKE_FRAMES) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_RECOVER, 0);
                work->stateStep = work->stateStep + 1;
            }
            break;
        case ACTOR_503500_PHASE_STEP_RECOVER:
            if (actor503500HasAnimationFinished(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_RECOVER) != 0) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_FINISH, 0);
                _actor503500ScheduleTargetable(task, 0, ACTOR_503500_ORANGE_FLASH_TARGET_REMOVE_DELAY);
                work->stateStep = work->stateStep + 1;
            }
            break;
        case ACTOR_503500_PHASE_STEP_FINISH:
            if (actor503500HasAnimationFinished(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_FINISH) != 0) {
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
                actor503500PlayAnimationPreset(task, ACTOR_503500_BOSS_IDLE_ANIMATION, 0);
                work->targetYawOffset = 0;
                _actor503500EnterCombatState(task, ACTOR_503500_STATE_IDLE);
            }
            break;
    }
}

/// Steps the boss's scripted collapse, flattening its root and burning it away.
///
/// Requires live boss work and model storage. Saves the nine Q12 rotation
/// coefficients, preserving translation and alignment bytes. After 31 updates
/// it repeats the recoil animation at rate 32 while Y scale falls from ONE to
/// ONE/8. Squash updates 60, 70 and 100 select translucency, corpse burn and
/// black lighting; this handler does not advance to teardown.
static void _actor503500StepCollapseState(Task* task)
{
    enum {
        ACTOR_503500_BOSS_COLLAPSE_STEP_SAVE      = 0,
        ACTOR_503500_BOSS_COLLAPSE_STEP_WAIT      = 1,
        ACTOR_503500_BOSS_COLLAPSE_STEP_SQUASH    = 2,
        ACTOR_503500_BOSS_COLLAPSE_WAIT_FRAMES    = 31,
        ACTOR_503500_BOSS_COLLAPSE_SCALE_MIN      = ONE / 8,
        ACTOR_503500_BOSS_COLLAPSE_SCALE_STEP     = 16,
        ACTOR_503500_BOSS_COLLAPSE_ANIMATION_RATE = 32,
        ACTOR_503500_BOSS_COLLAPSE_FADE_FRAME     = 60,
        ACTOR_503500_BOSS_COLLAPSE_BURN_FRAME     = 70,
        ACTOR_503500_BOSS_COLLAPSE_BLACK_FRAME    = 100,
    };
    Actor503500Work* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        coord;
    const s32*       sourceWords;
    s32*             destinationWords;
    s32              wordIndex;
    VECTOR           scale;

    /// Copies exactly nine rotation halfwords, preserving translation and alignment bytes.
    ///
    /// Arguments must be word-aligned MATRIX::m arrays without side effects;
    /// each is evaluated twice. Captures destinationWords, sourceWords and
    /// wordIndex. Expands to multiple statements; invoke within a braced block.
#define ACTOR_503500_BOSS_COPY_ROTATION(destinationRotation, sourceRotation)                                  \
    destinationWords = (s32*)(destinationRotation);                                                           \
    sourceWords      = (const s32*)(sourceRotation);                                                          \
    for (wordIndex = 0; wordIndex < (s32)(sizeof(destinationRotation) / sizeof(*sourceWords)); wordIndex++) { \
        *destinationWords++ = *sourceWords++;                                                                 \
    }                                                                                                         \
    (destinationRotation)[2][2] = (sourceRotation)[2][2];

    coord = task->extra.tmd->coords;
    model = task->extra.tmd;
    work  = task->work;
    enemy = task->spawnArg2.pointer;
    switch ((s8)work->stateStep) {
        case ACTOR_503500_BOSS_COLLAPSE_STEP_SAVE:
            work->stateFrames    = 0;
            work->collapseScaleY = ONE;
            ACTOR_503500_BOSS_COPY_ROTATION(work->unscaledRotation.m, coord->coord.m);
            work->stateStep = work->stateStep + 1;
            break;
        case ACTOR_503500_BOSS_COLLAPSE_STEP_WAIT:
            if (++work->stateFrames >= ACTOR_503500_BOSS_COLLAPSE_WAIT_FRAMES) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET, ACTOR_503500_BOSS_COLLAPSE_ANIMATION_RATE);
                work->stateFrames = 0;
                work->stateStep   = work->stateStep + 1;
            }
            break;
        case ACTOR_503500_BOSS_COLLAPSE_STEP_SQUASH:
            if (work->collapseScaleY > ACTOR_503500_BOSS_COLLAPSE_SCALE_MIN) {
                work->collapseScaleY -= ACTOR_503500_BOSS_COLLAPSE_SCALE_STEP;
            }
            // Restore the snapshot so scaling does not compound.
            ACTOR_503500_BOSS_COPY_ROTATION(coord->coord.m, work->unscaledRotation.m);
            scale.vx = ONE;
            scale.vy = work->collapseScaleY;
            scale.vz = ONE;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            switch (++work->stateFrames) {
                case ACTOR_503500_BOSS_COLLAPSE_FADE_FRAME:
                    model->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;
                case ACTOR_503500_BOSS_COLLAPSE_BURN_FRAME:
                    effectSpawn(EFFECT_CORPSE_BURN, coord, 1, NULL);
                    break;
                case ACTOR_503500_BOSS_COLLAPSE_BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
            }
            break;
    }
    if ((s8)work->stateStep >= ACTOR_503500_BOSS_COLLAPSE_STEP_SQUASH && actor503500HasAnimationFinished(task, ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET) != 0) {
        actor503500PlayAnimationPreset(task, ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET, ACTOR_503500_BOSS_COLLAPSE_ANIMATION_RATE);
    }
#undef ACTOR_503500_BOSS_COPY_ROTATION
}

/// Advances the boss's slot-0 orange-flash attack alongside its main state.
///
/// Requires initialized boss work and model part 3. Part loss, stun or defeat
/// clears the slot command immediately. Windup enables targeting after 75 ticks;
/// a 90-tick orange flash is followed by strike and recovery animations.
/// Spawn failure still advances. Completion clears the command and busy latch
/// and restores idle animation and ordinary ordering-table depth.
static void _actor503500StepOrangeFlashAttack(Task* task)
{
    enum {
        ACTOR_503500_SELF_ATTACK_STEP_WINDUP        = 0,
        ACTOR_503500_SELF_ATTACK_STEP_CHARGE        = 1,
        ACTOR_503500_SELF_ATTACK_STEP_WAIT_CHARGE   = 2,
        ACTOR_503500_SELF_ATTACK_STEP_STRIKE        = 3,
        ACTOR_503500_SELF_ATTACK_STEP_RECOVER       = 4,
        ACTOR_503500_SELF_ATTACK_STEP_FINISH        = 5,
        ACTOR_503500_SELF_ATTACK_TARGET_DELAY       = 75,
        ACTOR_503500_SELF_ATTACK_CHARGE_WAIT_FRAMES = 91,
    };
    Actor503500Work* work;
    Task*            flashTask;
    GfxCoord*        flashCoord;

    work = task->work;
    if ((u16)(work->state - ACTOR_503500_STATE_PART_LOST) < (u32)(ACTOR_503500_STATE_DEFEATED - ACTOR_503500_STATE_PART_LOST + 1)) {
        _actor503500CommandSlot(work, ACTOR_503500_SLOT_BODY, ACTOR_503500_SLOT_COMMAND_NONE, 0);
        return;
    }
    switch (work->selfAttackPhase) {
        case ACTOR_503500_SELF_ATTACK_STEP_WINDUP:
            actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_WINDUP, ANIMATION_RATE_ONE);
            _actor503500ScheduleTargetable(task, 1, ACTOR_503500_SELF_ATTACK_TARGET_DELAY);
            work->selfAttackPhase++;
            break;
        case ACTOR_503500_SELF_ATTACK_STEP_CHARGE:
            // Attach the orange flash to the boss and expand OT depth for its charge.
            if (actor503500HasAnimationFinished(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_WINDUP) != 0) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_CHARGE, ANIMATION_RATE_ONE);
                ACTOR_503500_SPAWN_ATTACHED_ORANGE_FLASH(ACTOR_503500_ORANGE_FLASH_CHARGE_FRAMES);
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_2X;
                work->selfAttackFrames     = 0;
                work->selfAttackPhase++;
            }
            break;
        case ACTOR_503500_SELF_ATTACK_STEP_WAIT_CHARGE:
            if (++work->selfAttackFrames >= ACTOR_503500_SELF_ATTACK_CHARGE_WAIT_FRAMES) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_STRIKE, ANIMATION_RATE_ONE);
                work->selfAttackFrames = 0;
                work->selfAttackPhase++;
            }
            break;
        case ACTOR_503500_SELF_ATTACK_STEP_STRIKE:
            if (++work->selfAttackFrames >= ACTOR_503500_ORANGE_FLASH_STRIKE_FRAMES) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_RECOVER, 0);
                work->selfAttackPhase++;
            }
            break;
        case ACTOR_503500_SELF_ATTACK_STEP_RECOVER:
            if (actor503500HasAnimationFinished(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_RECOVER) != 0) {
                actor503500PlayAnimationPreset(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_FINISH, 0);
                _actor503500ScheduleTargetable(task, 0, ACTOR_503500_ORANGE_FLASH_TARGET_REMOVE_DELAY);
                work->selfAttackPhase++;
            }
            break;
        case ACTOR_503500_SELF_ATTACK_STEP_FINISH:
            if (actor503500HasAnimationFinished(task, ACTOR_503500_ORANGE_FLASH_ANIMATION_FINISH) != 0) {
                work->selfAttackCommand    = ACTOR_503500_SLOT_COMMAND_NONE;
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
                actor503500SetSlotBusy(task, ACTOR_503500_SLOT_BODY, 0);
                actor503500PlayAnimationPreset(task, ACTOR_503500_BOSS_IDLE_ANIMATION, 0);
            }
            break;
    }
}

#undef ACTOR_503500_SPAWN_ATTACHED_ORANGE_FLASH

/// Applies one unique player/companion attack contact to the boss while hits are enabled.
///
/// `contacts[0..contactIndex]` must be readable; earlier equal keys suppress this
/// record. `task`, `work` and `enemy` belong to the live boss, and `coord` is its
/// model root. Damage uses the attacker's distance in game units. Death replaces
/// status reactions; a landed hit raises the cooldown to the attack's frame count.
static inline void _actor503500HandleHit(Task* task, Actor503500Work* work, Enemy* enemy, GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactIndex)
{
    VECTOR    attackerOffset;
    SVECTOR   worldPosition;
    MATRIX    worldRotation;
    GfxCoord* attackerCoord;
    s16       hitCooldownFrames;
    u32       attackKey;
    s32       damage;
    s32       previousContactIndex;

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
    attackerCoord = gPlayerActorTasks[(attackKey >> 7) & 1]->extra.tmd->coords;
    gfxComposeNodeWorldTransform(coord, &worldRotation, &worldPosition);
    attackerOffset.vx = attackerCoord->coord.t[0] - worldPosition.vx;
    attackerOffset.vy = attackerCoord->coord.t[1] - worldPosition.vy;
    attackerOffset.vz = attackerCoord->coord.t[2] - worldPosition.vz;
    damage            = damageComputePlayerAttack(attackKey, SquareRoot0(attackerOffset.vx * attackerOffset.vx + attackerOffset.vy * attackerOffset.vy + attackerOffset.vz * attackerOffset.vz), 0, 0);
    if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
        damage *= ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER;
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, NULL);
    }
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    enemy->hp -= damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    if (enemy->hp <= 0) {
        _actor503500EnterCombatState(task, ACTOR_503500_STATE_DEFEATED);
        work->defeated = 1;
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
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, NULL, &work->hitEffect);
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to the boss.
///
/// Requires initialized work, a live enemy/model and `contactCount` readable
/// contact elements. Earlier equal keys suppress duplicates; an active hit
/// cooldown suppresses damage. Hit effects appear at its target coordinate.
/// Contacts are borrowed and left intact. `unusedBody` is retained in the
/// collision-handler signature and is not read.
static void _actor503500ApplyBossHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    Actor503500Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500HandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// Turns the idle or attacking boss toward its desired yaw with acceleration.
///
/// Animation choice and missing parts lower the signed 16.16 angular speed
/// limit. Brakes near the target using the unwrapped yaw difference; farther
/// away chooses the shorter turn, retaining both half-turn endpoints. Rebuilds
/// the root with yaw only and retains translation. Angles use 4096 per turn;
/// other boss states retain their rotation and turn speed. Requires live boss
/// work and a current animation ID in 0..19 for the limit table.
static void _actor503500TurnBoss(Task* task)
{
    enum {
        ACTOR_503500_TURN_RATE_REDUCED   = 1,
        ACTOR_503500_TURN_RATE_MINIMUM   = 2,
        ACTOR_503500_TURN_LIMIT_REDUCED  = 6 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_TURN_LIMIT_MINIMUM  = 1 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_TURN_LIMIT_NORMAL   = 7 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_TURN_REAR_PART_SLOT = 6,
    };
    SVECTOR          rootAngles;
    Actor503500Work* work;
    GfxCoord*        rootCoord;
    Enemy**          slotEnemies;
    s32              turnSpeedLimit;
    s32              yawStep;
    s32              missingPartPenalty;
    s32              yawError;
    s32              yawDistance;
    s16              currentYaw;
    s16              nextYaw;

    work        = task->work;
    rootCoord   = task->extra.tmd->coords;
    slotEnemies = work->enemies;
    if (work->state < ACTOR_503500_STATE_PART_LOST) {
        if (work->state >= ACTOR_503500_STATE_IDLE) {
            switch (D_actor_503500_8016E8FC[work->animationId]) {
                case ACTOR_503500_TURN_RATE_REDUCED:
                    turnSpeedLimit = ACTOR_503500_TURN_LIMIT_REDUCED;
                    break;
                case ACTOR_503500_TURN_RATE_MINIMUM:
                    turnSpeedLimit = ACTOR_503500_TURN_LIMIT_MINIMUM;
                    break;
                default:
                    turnSpeedLimit = ACTOR_503500_TURN_LIMIT_NORMAL;
                    break;
            }
            // Missing targets reduce turning capacity before acceleration.
            missingPartPenalty = 0;
            if (slotEnemies[ACTOR_503500_SLOT_ARM_0] == NULL) {
                missingPartPenalty = -(turnSpeedLimit / 25);
            }
            if (slotEnemies[ACTOR_503500_SLOT_ARM_1] == NULL) {
                missingPartPenalty -= turnSpeedLimit / 25;
            }
            if (work->enemies[ACTOR_503500_TURN_REAR_PART_SLOT] == NULL) {
                missingPartPenalty -= (turnSpeedLimit * 12) / 100;
            }
            if (work->enemies[ACTOR_503500_SLOT_SMALL_ORB_EMITTER] == NULL) {
                missingPartPenalty -= turnSpeedLimit / 4;
            }
            turnSpeedLimit += missingPartPenalty;
            currentYaw      = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]);
            yawStep         = work->turnSpeed;
            yawError        = work->targetYaw - currentYaw;
            yawDistance     = ABS(yawError);
            // The braking test deliberately precedes shortest-turn wrapping.
            if ((turnSpeedLimit * 8) >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS >= yawDistance) {
                if (yawStep < 0) {
                    yawStep += turnSpeedLimit / 16;
                    if (yawStep > 0) {
                        yawStep = 0;
                    }
                } else {
                    yawStep -= turnSpeedLimit / 16;
                    if (yawStep < 0) {
                        yawStep = 0;
                    }
                }
            } else {
                if (yawDistance > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                    if (yawError < 0) {
                        yawError += ACTOR_TRANSFORM_ANGLE_TURN;
                    } else {
                        yawError -= ACTOR_TRANSFORM_ANGLE_TURN;
                    }
                }
                if (yawError > 0) {
                    yawStep += turnSpeedLimit / 32;
                    if (yawStep > turnSpeedLimit) {
                        yawStep = turnSpeedLimit;
                    }
                } else {
                    yawStep -= turnSpeedLimit / 32;
                    if (yawStep < -turnSpeedLimit) {
                        yawStep = -turnSpeedLimit;
                    }
                }
            }
            nextYaw         = currentYaw + (yawStep >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS);
            work->turnSpeed = yawStep;
            rootAngles.vx   = 0;
            rootAngles.vy   = nextYaw;
            rootAngles.vz   = 0;
            RotMatrix(&rootAngles, &rootCoord->coord);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->yaw               = nextYaw;
        }
    }
}

/// Clamps the boss's signed 16.16 X/Z position to its fight box.
///
/// Requires the live boss work and model root in the same parent frame.
/// X is restricted to 7000..9000 and Z to 6000..8000 game units, inclusive;
/// publishes their integer halves and invalidates composition. Y, velocity
/// and walk speed are retained, including when a boundary stops translation.
static inline void _actor503500ClampBossWalkPosition(Actor503500Work* work, GfxCoord* rootCoord)
{
    enum {
        ACTOR_503500_WALK_MIN_X = 7000 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_WALK_MAX_X = 9000 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_WALK_MIN_Z = 6000 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_WALK_MAX_Z = 8000 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
    };
    long* positionX;
    long* positionZ;

    positionX = &work->position.vx;
    positionZ = &work->position.vz;
    if (*positionX > ACTOR_503500_WALK_MAX_X) {
        *positionX = ACTOR_503500_WALK_MAX_X;
    } else if (*positionX < ACTOR_503500_WALK_MIN_X) {
        *positionX = ACTOR_503500_WALK_MIN_X;
    }
    if (*positionZ > ACTOR_503500_WALK_MAX_Z) {
        *positionZ = ACTOR_503500_WALK_MAX_Z;
    } else if (*positionZ < ACTOR_503500_WALK_MIN_Z) {
        *positionZ = ACTOR_503500_WALK_MIN_Z;
    }
    rootCoord->coord.t[0]   = *positionX >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    rootCoord->coord.t[2]   = *positionZ >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Steps the boss's fixed-point root position along its yaw within the fight box.
///
/// Records the old translation in whole parent-frame units on every call.
/// Idle and late phase-transition steps accelerate; other moving states brake.
/// A yaw error above 600 units reverses the effective limit at one-quarter
/// speed. Position and speed use signed 16.16 units; X clamps to 7000..9000,
/// Z to 6000..8000. Defeated, held, collapse and early scripted steps only
/// record the old translation. Y is retained.
static void _actor503500WalkBoss(Task* task)
{
    enum {
        ACTOR_503500_WALK_REVERSE_YAW_ERROR    = 600,
        ACTOR_503500_WALK_MATRIX_FRACTION_BITS = 12,
        ACTOR_503500_WALK_FIRST_MOVING_PHASE   = 3,
    };
    MATRIX           facingRotation;
    Actor503500Work* work;
    GfxCoord*        rootCoord;
    s32              effectiveSpeedLimit;
    s32              walkSpeed;
    s32              speedInMatrixUnits;

    work                         = task->work;
    rootCoord                    = task->extra.tmd->coords;
    work->previousTranslation.vx = rootCoord->coord.t[0];
    work->previousTranslation.vy = rootCoord->coord.t[1];
    work->previousTranslation.vz = rootCoord->coord.t[2];
    switch (work->state) {
        case ACTOR_503500_STATE_DEFEATED:
        case ACTOR_503500_STATE_HELD:
        case ACTOR_503500_STATE_COLLAPSE:
            return;
        case ACTOR_503500_STATE_SCRIPTED:
            if ((s8)work->stateStep < ACTOR_503500_WALK_FIRST_MOVING_PHASE) {
                return;
            }
        case ACTOR_503500_STATE_IDLE:
            work->advancing = 1;
            break;
        default:
            work->advancing = 0;
            break;
    }
    effectiveSpeedLimit = work->walkSpeedLimit;
    if (ABS(work->targetYaw - work->yaw) > ACTOR_503500_WALK_REVERSE_YAW_ERROR) {
        effectiveSpeedLimit = -effectiveSpeedLimit >> 2;
    }
    if (work->advancing != 0) {
        walkSpeed = work->walkSpeed + effectiveSpeedLimit / 32;
        if (walkSpeed > 0) {
            if (effectiveSpeedLimit < walkSpeed) {
                walkSpeed = effectiveSpeedLimit;
            }
        } else if (walkSpeed < -effectiveSpeedLimit) {
            walkSpeed = -effectiveSpeedLimit;
        }
    } else {
        walkSpeed = work->walkSpeed - effectiveSpeedLimit / 32;
        if (walkSpeed < 0) {
            walkSpeed = 0;
        }
    }
    work->walkSpeed = walkSpeed;
    // Integrate the facing-axis step in fixed point before clamping the root.
    gfxSetRotIdentity(&facingRotation);
    RotMatrixY(work->yaw, &facingRotation);
    speedInMatrixUnits = walkSpeed >> ACTOR_503500_WALK_MATRIX_FRACTION_BITS;
    work->velocity.vx  = facingRotation.m[0][2] * speedInMatrixUnits;
    work->velocity.vz  = facingRotation.m[2][2] * speedInMatrixUnits;
    work->position.vx += work->velocity.vx;
    work->position.vz += work->velocity.vz;
    _actor503500ClampBossWalkPosition(work, rootCoord);
}

/// Updates attached targets' lock-on eligibility from the player's bearing and height.
///
/// Requires live boss work and player/root translations in a common frame.
/// The per-slot mask selects upper/middle/lower Y and forward/rear/side bearing
/// bands, using 4096 angle units per turn. A bearing miss or active event
/// blocks locking and clears scan retention; a height-only miss blocks locking
/// but retains scanning. Full eligibility clears only the lock exclusion,
/// retaining the previous scan flag. Empty slots are skipped.
static void _actor503500UpdateSlotTargetEligibility(Task* task)
{
    enum {
        ACTOR_503500_TARGET_HEIGHT_UPPER          = 1 << 0,
        ACTOR_503500_TARGET_HEIGHT_MIDDLE         = 1 << 1,
        ACTOR_503500_TARGET_HEIGHT_LOWER          = 1 << 2,
        ACTOR_503500_TARGET_BEARING_FORWARD       = 1 << 3,
        ACTOR_503500_TARGET_BEARING_REAR          = 1 << 4,
        ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE = 1 << 5,
        ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE = 1 << 6,
        ACTOR_503500_TARGET_FORWARD_LIMIT         = 1024,
        ACTOR_503500_TARGET_SIDE_MIN              = 768,
        ACTOR_503500_TARGET_SIDE_MAX              = 1152,
        ACTOR_503500_TARGET_UPPER_BOUNDARY_Y      = -3000,
        ACTOR_503500_TARGET_LOWER_BOUNDARY_Y      = -999,
    };
    Actor503500Work* work;
    GfxCoord*        rootCoord;
    Enemy*           enemy;
    s16              playerBearing;
    s32              playerY;
    s16              bearingMask;
    s16              heightMask;
    s32              slotIndex;
    s16              eligibilityMask;

    work          = task->work;
    rootCoord     = task->extra.tmd->coords;
    playerBearing = ratan2(gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0], gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2]) - work->yaw;
    while (playerBearing >= ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        playerBearing -= ACTOR_TRANSFORM_ANGLE_TURN;
    }
    while (playerBearing < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        playerBearing += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    bearingMask = ACTOR_503500_TARGET_BEARING_REAR;
    if (ABS(playerBearing) < ACTOR_503500_TARGET_FORWARD_LIMIT) {
        bearingMask = ACTOR_503500_TARGET_BEARING_FORWARD;
    }
    if (playerBearing > ACTOR_503500_TARGET_SIDE_MIN && playerBearing < ACTOR_503500_TARGET_SIDE_MAX) {
        bearingMask = ACTOR_503500_TARGET_BEARING_POSITIVE_SIDE;
    } else if (playerBearing < -ACTOR_503500_TARGET_SIDE_MIN && playerBearing > -ACTOR_503500_TARGET_SIDE_MAX) {
        bearingMask = ACTOR_503500_TARGET_BEARING_NEGATIVE_SIDE;
    }
    playerY    = gPlayerStatus.coordMtx->t[1];
    heightMask = ACTOR_503500_TARGET_HEIGHT_LOWER;
    if (playerY < ACTOR_503500_TARGET_LOWER_BOUNDARY_Y) {
        heightMask = ACTOR_503500_TARGET_HEIGHT_MIDDLE;
        if (playerY < ACTOR_503500_TARGET_UPPER_BOUNDARY_Y) {
            heightMask = ACTOR_503500_TARGET_HEIGHT_UPPER;
        }
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->enemies); slotIndex++) {
        enemy = work->enemies[slotIndex];
        if (enemy != NULL) {
            eligibilityMask = D_actor_503500_8016E910[slotIndex];
            if ((eligibilityMask & bearingMask) != bearingMask || gGameSession->eventState != 0) {
                enemy->node.state.parts.flags                    |= WORLD_TARGET_NOT_LOCKABLE;
                work->enemies[slotIndex]->node.state.parts.flags &= ~WORLD_TARGET_KEEP_SCANNED;
            } else if ((eligibilityMask & heightMask) != heightMask) {
                enemy->node.state.parts.flags                    |= WORLD_TARGET_NOT_LOCKABLE;
                work->enemies[slotIndex]->node.state.parts.flags |= WORLD_TARGET_KEEP_SCANNED;
            } else {
                enemy->node.state.parts.flags &= ~WORLD_TARGET_NOT_LOCKABLE;
            }
        }
    }
}

void actor503500SyncAttachedModelDrawState(Task* task, s8* bufferFreeCountdown)
{
    enum { ACTOR_503500_ATTACHED_BUFFER_FREE_DELAY_FRAMES = 2 };
    TmdObject* model;
    TmdObject* parentModel;
    u16        drawFlags;
    u16        policyFlags;

    model = task->extra.tmd;
    if (model->coords->parent != &gGfxViewCoord) {
        drawFlags   = model->flags;
        parentModel = task->parent->extra.tmd;
        if (drawFlags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            if (!(parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                model->flags = drawFlags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        } else if (parentModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            model->flags = drawFlags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
        policyFlags = model->flags;
        if (policyFlags & TMD_OBJECT_SEMI_TRANS) {
            if (!(parentModel->flags & TMD_OBJECT_SEMI_TRANS)) {
                model->flags = policyFlags & ~TMD_OBJECT_SEMI_TRANS;
            }
        } else if (parentModel->flags & TMD_OBJECT_SEMI_TRANS) {
            model->flags = policyFlags | TMD_OBJECT_SEMI_TRANS;
        }
        policyFlags = model->flags;
        if (policyFlags & TMD_OBJECT_SKIP_AUTO_BUFFER) {
            if (!(parentModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
                model->flags = policyFlags & ~TMD_OBJECT_SKIP_AUTO_BUFFER;
                tmdAllocPrimitiveBuffer(model);
            }
        } else if (parentModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER) {
            model->flags         = policyFlags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            *bufferFreeCountdown = ACTOR_503500_ATTACHED_BUFFER_FREE_DELAY_FRAMES;
        }
    }
}

/// Applies the enabled scales to the boss's current model pose.
///
/// `task` must have the boss work block and its twenty-part model. Scale vectors
/// use 4096 for 1.0. Parts 5 and 11 borrow private parents refreshed from parts
/// 4 and 10; their child-parent links must already point at those copies.
/// Part 16 is scaled in place, so its animation pose must be refreshed before
/// reapplication to avoid multiplying a previously scaled pose again.
static inline void _actor503500ApplyPartScales(Task* task)
{
    Actor503500Work* scaleWork;

    scaleWork = task->work;
    if (scaleWork->scaledParts & (1 << 5)) {
        scaleWork->part5Parent = task->extra.tmd->coords[4];
        ScaleMatrix(&scaleWork->part5Parent.coord, &scaleWork->part5Scale);
    }
    if (scaleWork->scaledParts & (1 << 11)) {
        scaleWork->part11Parent = task->extra.tmd->coords[10];
        ScaleMatrix(&scaleWork->part11Parent.coord, &scaleWork->part11Scale);
    }
    if (scaleWork->scaledParts & (1 << 16)) {
        ScaleMatrix(&task->extra.tmd->coords[16].coord, &scaleWork->part16Scale);
    }
}

s32 actor503500HandlePlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    Actor503500Work* work;
    TmdObject*       model;
    s32              trackIndex;

    work  = task->work;
    model = task->extra.tmd;
    // Bind a changed source before reseeding the driven tracks.
    if (request->source.index != work->animationSourceIndex) {
        work->animationSourceIndex = request->source.index;
        animationInitContext(&work->rig.anim, D_actor_503500_8016EAB8[work->animationSourceIndex], model, work->rig.poses, work->rig.slots);
        work->animationStarted = 0;
    }
    work->animationId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->animationStarted != 0) {
        for (trackIndex = 1; trackIndex < ARRAY_SIZE(work->rig.slots); trackIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, trackIndex, work->animationId, 0, request->blendFrames);
        }
    } else {
        for (trackIndex = 1; trackIndex < ARRAY_SIZE(work->rig.slots); trackIndex++) {
            animationResetSlot(&work->rig.anim, trackIndex, work->animationId);
        }
    }
    // Publish the first pose immediately, then restore the scaled parts.
    for (trackIndex = 1; trackIndex < ARRAY_SIZE(work->rig.slots); trackIndex++) {
        animationTickSlot(&work->rig.anim, trackIndex);
    }
    work->animationStarted = 1;
    _actor503500ApplyPartScales(task);
    return 0;
}

/// Restarts boss state progress and schedules targetability three upkeep ticks later.
///
/// Clears the state/attack phases and frame counters and restores ordinary OT
/// depth. The selected attack slot and idle delay are retained. `state` narrows
/// to a signed halfword; the separate signed-byte `targetable` preserves combat
/// entry's full-width state comparison before that narrowing.
static inline void _actor503500ResetBossState(Task* task, s32 state, s8 targetable)
{
    enum { ACTOR_503500_STATE_TARGETABLE_DELAY_FRAMES = 3 };
    Actor503500Work* work;

    work               = task->work;
    work->state        = state;
    work->stateStep    = 0;
    work->attackPhase  = ACTOR_503500_ATTACK_PHASE_COMMAND;
    work->stateFrames  = 0;
    work->attackFrames = 0;
    _actor503500ScheduleTargetable(task, targetable, ACTOR_503500_STATE_TARGETABLE_DELAY_FRAMES);
    gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
}

/// Enters a boss state, restarting progress and scheduling target removal.
///
/// `task` is the boss task; `state` is an `Actor503500State`. State and attack
/// phases and frame counters restart; the selected slot and attack delay remain.
/// Target removal takes effect after three upkeep ticks and ordinary OT depth
/// is restored immediately. Scene commands and part-loss recoil use this entry.
static inline void _actor503500EnterBossState(Task* task, s16 state)
{
    _actor503500ResetBossState(task, state, 0);
}

s32 actor503500HandleBossCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_503500_BOSS_COMMAND_RESUME                 = 0,
        ACTOR_503500_BOSS_COMMAND_HOLD                   = 1,
        ACTOR_503500_BOSS_COMMAND_COLLAPSE               = 2,
        ACTOR_503500_BOSS_COMMAND_EXIT                   = 3,
        ACTOR_503500_BOSS_COMMAND_BEGIN_PHASE_TRANSITION = 4,
        ACTOR_503500_BOSS_COMMAND_RESTORE_ROOT           = 5,
    };
    Actor503500Work* work;
    GfxCoord*        rootCoord;

    switch (command->command) {
        case ACTOR_503500_BOSS_COMMAND_RESUME:
            _actor503500EnterBossState(task, ACTOR_503500_STATE_IDLE);
            break;
        case ACTOR_503500_BOSS_COMMAND_HOLD:
            _actor503500EnterBossState(task, ACTOR_503500_STATE_HELD);
            break;
        case ACTOR_503500_BOSS_COMMAND_COLLAPSE:
            _actor503500EnterBossState(task, ACTOR_503500_STATE_COLLAPSE);
            break;
        case ACTOR_503500_BOSS_COMMAND_EXIT:
            task->state++;
            break;
        case ACTOR_503500_BOSS_COMMAND_BEGIN_PHASE_TRANSITION:
            work                    = task->work;
            rootCoord               = task->extra.tmd->coords;
            work->savedYaw          = work->yaw;
            work->savedRootCoord    = *rootCoord;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            _actor503500EnterBossState(task, ACTOR_503500_STATE_SCRIPTED);
            break;
        case ACTOR_503500_BOSS_COMMAND_RESTORE_ROOT:
            work                    = task->work;
            rootCoord               = task->extra.tmd->coords;
            work->yaw               = work->savedYaw;
            *rootCoord              = work->savedRootCoord;
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->rootCoordRestored = 1;
            break;
    }
    return 0;
}

void actor503500ClearSlotEnemy(Task* unusedTask, s32 slot)
{
    D_actor_503500_80176574.work.enemies[slot] = NULL;
}

Enemy* actor503500SpawnSlotEnemy(Task* task, s32 slot)
{
    GameLocationKey        location;
    const GameLocationKey* sessionLocation;
    u8                     viewId;
    AreaVariant*           areaVariant;
    const AreaPlacement*   placement;
    Enemy*                 slotEnemy;
    TmdObject*             slotModel;
    s32                    placementIndex;
    u32                    placementKey;
    const Enemy*           parentEnemy;
    // Keep the boss slot storage live across spawning and placement lookup.
    Actor503500Work* bossWork = &D_actor_503500_80176574.work;

    slotEnemy = enemySpawnFromTable(D_actor_503500_8016E924, slot, slot, task->spawnArg2.pointer);
    if (slotEnemy != NULL) {
        sessionLocation = &gGameSession->location.loc;
        parentEnemy     = task->spawnArg2.pointer;
        placementKey    = parentEnemy->placeKey;
        slotModel       = slotEnemy->task->extra.tmd;
        location.stage  = sessionLocation->stage;
        location.area   = sessionLocation->area;
        location.room   = sessionLocation->room;
        viewId          = sessionLocation->view;
        placementIndex  = placementKey >> ENEMY_PLACE_INDEX_SHIFT;
        location.view   = viewId;
        areaSyncLocationVariant(&location);
        areaVariant                  = areaGetVariant(&location);
        placement                    = gpAreaPlaceAt(areaVariant->placements, placementIndex);
        slotModel->texturePageOffset = placement->texturePageOffset;
        slotModel->clutRowOffset     = placement->clutRowOffset;
        // Rebuild both halves with the inherited texture offsets, preserving the half selector.
        if (slotModel->buffer != NULL) {
            tmdBuildBufferHalf(slotModel);
            tmdBuildBufferHalf(slotModel);
        }
        bossWork->enemies[slot] = slotEnemy;
    }
    return slotEnemy;
}

s32 actor503500IsSlotEmpty(Task* unusedTask, s32 slot)
{
    return D_actor_503500_80176574.work.enemies[slot] == NULL;
}

void actor503500SetBossPartScale(Task* task, s32 partIndex, const SVECTOR* scale)
{
    Actor503500Work* work = &D_actor_503500_80176574.work;

    // Insert a scaled parent for the arms; the rear part uses its animated pose.
    switch (partIndex) {
        case 5:
            work->part5Scale.vx               = scale->vx;
            work->part5Scale.vy               = scale->vy;
            work->part5Scale.vz               = scale->vz;
            work->part5Parent                 = task->extra.tmd->coords[4];
            task->extra.tmd->coords[5].parent = &work->part5Parent;
            break;
        case 11:
            work->part11Scale.vx               = scale->vx;
            work->part11Scale.vy               = scale->vy;
            work->part11Scale.vz               = scale->vz;
            work->part11Parent                 = task->extra.tmd->coords[10];
            task->extra.tmd->coords[11].parent = &work->part11Parent;
            break;
        case 16:
            work->part16Scale.vx = scale->vx;
            work->part16Scale.vy = scale->vy;
            work->part16Scale.vz = scale->vz;
            break;
    }
    work->scaledParts |= 1 << partIndex;
}

void actor503500SetSlotBusy(Task* unusedTask, s32 slot, s16 busy)
{
    D_actor_503500_80176574.work.slotBusy[slot] = busy;
}

void actor503500PlayAnimationPreset(Task* task, s32 presetIndex, s32 rate)
{
    enum { ACTOR_503500_ANIMATION_RATE_SLOT_COUNT = 16 };
    Actor503500Work* work;
    AnimationSlot*   slot;
    s32              tracksLeft;

    work = task->work;
    slot = &work->rig.slots[1];
    if (rate == 0) {
        rate = ANIMATION_RATE_ONE;
    }
    for (tracksLeft = ACTOR_503500_ANIMATION_RATE_SLOT_COUNT - 1; tracksLeft >= 0; tracksLeft--) {
        slot->rate = rate;
        slot++;
    }
    actor503500HandlePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_503500_8016EAC0[presetIndex], 0);
}

s32 actor503500HasAnimationFinished(Task* unusedTask, s32 animationId)
{
    enum { ACTOR_503500_ANIMATION_NOT_CURRENT = -1 };

    if (animationId != D_actor_503500_80176574.work.animationId) {
        return ACTOR_503500_ANIMATION_NOT_CURRENT;
    }
    return (D_actor_503500_80176574.work.rig.slots[1].status.fields.flags & (ANIMATION_SLOT_SETTLED | ANIMATION_SLOT_FOLLOWED_JUMP)) != 0;
}

void actor503500EnterPartLostState(Task* task)
{
    _actor503500EnterBossState(task, ACTOR_503500_STATE_PART_LOST);
}

s32 actor503500ShouldInterruptAttack(Task* unusedTask)
{
    return (u32)((u16)D_actor_503500_80176574.work.state - ACTOR_503500_STATE_PART_LOST) < (ACTOR_503500_STATE_DEFEATED - ACTOR_503500_STATE_PART_LOST + 1U);
}

/// Replaces a boss slot's effect-cost reservation without checking the budget.
///
/// The enemy slot domain is 0..16; indexing is unchecked. Stores the signed
/// halfword's bit pattern in unsigned-halfword storage. Budget summation
/// interprets each stored cost as signed; zero releases a reservation.
/// The projectile aggregate at entry 17 has separate lifetime add/subtract APIs.
static void _actor503500SetSlotEffectReservation(s32 slot, s16 effectCost)
{
    D_actor_503500_80176D64[slot] = effectCost;
}

s32 actor503500TryReserveSlotEffects(s32 slot, s32 effectCost)
{
    enum { ACTOR_503500_EFFECT_BUDGET_LIMIT = 9 };
    s32  accepted;
    s32  totalCost;
    s32  budgetIndex;
    u16* slotCost;

    accepted    = 0;
    totalCost   = effectCost;
    budgetIndex = 0;
    slotCost    = D_actor_503500_80176D64;
    // Replace this slot's reservation rather than adding to it.
    do {
        if (budgetIndex != slot) {
            totalCost += (s16)*slotCost;
        }
        budgetIndex++;
        slotCost++;
    } while (budgetIndex < (s32)ARRAY_SIZE(D_actor_503500_80176D64));

    if (totalCost < ACTOR_503500_EFFECT_BUDGET_LIMIT) {
        D_actor_503500_80176D64[slot] = (u16)effectCost;
        accepted                      = 1;
    }
    return accepted;
}

void actor503500ReleaseSlotEffects(s32 slot)
{
    D_actor_503500_80176D64[slot] = 0;
}

s16 actor503500MeasurePlayerBearing(Task* task)
{
    GfxCoord* coord;
    SVECTOR   playerOffset;
    s16       playerBearing;

    coord           = task->extra.tmd->coords;
    playerOffset.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    playerOffset.vy = 0;
    playerOffset.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    playerBearing   = ratan2(playerOffset.vx, playerOffset.vz) - ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    while (playerBearing >= ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        playerBearing -= ACTOR_TRANSFORM_ANGLE_TURN;
    }
    while (playerBearing < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        playerBearing += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    return playerBearing;
}

s32 actor503500IsDefeated(void)
{
    return D_actor_503500_80176574.work.defeated;
}

s16 actor503500GetPlayerBearing(void)
{
    return D_actor_503500_80176574.work.playerBearing;
}

/// Removes the boss's live collision geometry and target before destroying its task.
///
/// The static work storage is retained. The room's body mesh is moved 8000 units
/// along Y before the target sphere is unlinked and its contact pointer cleared.
static void _actor503500ExitBoss(Task* task)
{
    Actor503500Work* work;
    Enemy*           enemy;

    enemy = task->spawnArg2.pointer;
    _actor503500UpdateBodyCollisionGrid(task, 0, 1);
    work = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Consumes boss reaction flags, entering stun only for a build-up reaction.
///
/// Clears stagger and damage-over-time flags without applying their effects.
/// Build-up restarts the stunned state and seeds its animation countdown to 3;
/// other reaction bits remain intact. The live enemy and boss work are required.
static void _actor503500HandleBossReactions(Task* task)
{
    enum { ACTOR_503500_BUILDUP_ANIMATION_DELAY_FRAMES = 3 };
    Actor503500Work* work;
    Enemy*           enemy;
    u8               reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        _actor503500EnterCombatState(task, ACTOR_503500_STATE_STUNNED);
        work->stunAnimationTimer = ACTOR_503500_BUILDUP_ANIMATION_DELAY_FRAMES;
    }
    reactionFlags = enemy->reactionFlags;
    if (reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

static void func_actor_503500_80136304(Task* arg0)
{
    Actor503500Work* work = arg0->work;
    u16              timer;

    switch (work->state) {
        case ACTOR_503500_STATE_IDLE:
            if (_actor503500CheckPartLossProgress(arg0) == 0) {
                _actor503500StepIdleState(arg0);
            }
            break;
        case ACTOR_503500_STATE_ATTACK:
            _actor503500StepAttackState(arg0);
            break;
        case ACTOR_503500_STATE_PART_LOST:
            _actor503500StepPartLostState(arg0);
            break;
        case ACTOR_503500_STATE_STUNNED:
            timer                    = work->stunAnimationTimer - 1;
            work->stunAnimationTimer = timer;
            if ((s16)timer < 0) {
                actor503500PlayAnimationPreset(arg0, 6, 0x10);
                work->stunAnimationTimer = 3;
            }
            if (damageTickEnemyBuildup(arg0->spawnArg2.pointer) != 0) {
                _actor503500EnterCombatState(arg0, ACTOR_503500_STATE_IDLE);
                work->attackDelay = 0x3C;
            }
            break;
        case ACTOR_503500_STATE_DEFEATED:
            _actor503500StepDefeatedState(arg0);
            break;
        case ACTOR_503500_STATE_HELD:
            _actor503500StepHeldState(arg0);
            break;
        case ACTOR_503500_STATE_SCRIPTED:
            _actor503500StepPhaseTransitionState(arg0);
            break;
        case ACTOR_503500_STATE_COLLAPSE:
            _actor503500StepCollapseState(arg0);
            break;
    }
    if (work->selfAttackCommand != 0) {
        _actor503500StepOrangeFlashAttack(arg0);
    }
}

/// Keeps the boss's idle animation and counts down to its next attack pick.
///
/// Requires initialized boss work and animation. Step 0 blends back to idle
/// only when a later animation is current. The signed-halfword delay decrements
/// through zero; the first negative result enters attack, so zero waits one call.
static void _actor503500StepIdleState(Task* task)
{
    enum {
        ACTOR_503500_IDLE_FIRST_NON_IDLE_ANIMATION = 2,
        ACTOR_503500_IDLE_BLEND_PRESET             = 1,
    };
    Actor503500Work* work = task->work;
    u16              delayFramesLeft;

    if ((s8)work->stateStep == 0 && work->animationId >= ACTOR_503500_IDLE_FIRST_NON_IDLE_ANIMATION) {
        actor503500PlayAnimationPreset(task, ACTOR_503500_IDLE_BLEND_PRESET, ANIMATION_RATE_ONE);
    }
    delayFramesLeft   = work->attackDelay - 1;
    work->attackDelay = delayFramesLeft;
    if ((s16)delayFramesLeft < 0) {
        _actor503500EnterCombatState(task, ACTOR_503500_STATE_ATTACK);
    }
}

s32 actor503500AttackBody(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_BODY_ATTACK_COOLDOWN_FRAMES   = 60,
        ACTOR_503500_BODY_ATTACK_IDLE_DELAY_FRAMES = 30,
    };
    s32 idleDelay;

    idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_BODY) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_BODY, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_BODY_ATTACK_COOLDOWN_FRAMES);
                work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
                idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                work->targetYawOffset = 0;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            if (_actor503500IsSlotAtRest(work, ACTOR_503500_SLOT_BODY) != 0) {
                idleDelay = ACTOR_503500_BODY_ATTACK_IDLE_DELAY_FRAMES;
            }
            break;
    }
    return idleDelay;
}

s32 actor503500AttackChainBases(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_BASES_ATTACK_RETRY_DELAY_FRAMES = 30,
        ACTOR_503500_BASES_ATTACK_COOLDOWN_FRAMES    = 150,
        ACTOR_503500_BASES_ATTACK_WAIT_LIMIT_FRAMES  = 90,
        ACTOR_503500_BASES_ATTACK_IDLE_DELAY_FRAMES  = 240,
    };
    s32 idleDelay;

    idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            idleDelay             = ACTOR_503500_BASES_ATTACK_RETRY_DELAY_FRAMES;
            work->targetYawOffset = 0;
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_CHAIN_BASE_0) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_CHAIN_BASE_0, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_BASES_ATTACK_COOLDOWN_FRAMES);
                idleDelay = ACTOR_503500_ATTACK_RUNNING;
            }
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_CHAIN_BASE_1) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_CHAIN_BASE_1, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_BASES_ATTACK_COOLDOWN_FRAMES);
                idleDelay = ACTOR_503500_ATTACK_RUNNING;
            }
            if (idleDelay == ACTOR_503500_ATTACK_RUNNING) {
                work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
                work->targetYawOffset = ACTOR_503500_ATTACK_REAR_YAW_OFFSET;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            // Either rest report ends this attack; count time only when both are busy.
            if (_actor503500IsSlotAtRest(work, ACTOR_503500_SLOT_CHAIN_BASE_0) != 0 || _actor503500IsSlotAtRest(work, ACTOR_503500_SLOT_CHAIN_BASE_1) != 0 ||
                ++work->attackFrames > ACTOR_503500_BASES_ATTACK_WAIT_LIMIT_FRAMES) {
                idleDelay = ACTOR_503500_BASES_ATTACK_IDLE_DELAY_FRAMES;
            }
            break;
    }
    return idleDelay;
}

s32 actor503500AttackPinkOrYellowFlash(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_PINK_FLASH_ATTACK_BEARING_LIMIT = 500,
        ACTOR_503500_FLASH_ATTACK_COOLDOWN_FRAMES    = 150,
        ACTOR_503500_FLASH_ATTACK_IDLE_DELAY_FRAMES  = 90,
    };
    s32 idleDelay;

    idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_PINK_FLASH_EMITTER) != 0) {
                if (__builtin_abs(work->playerBearing) < ACTOR_503500_PINK_FLASH_ATTACK_BEARING_LIMIT) {
                    _actor503500CommandSlot(work, ACTOR_503500_SLOT_PINK_FLASH_EMITTER, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_FLASH_ATTACK_COOLDOWN_FRAMES);
                    work->attackSlot      = ACTOR_503500_SLOT_PINK_FLASH_EMITTER;
                    work->targetYawOffset = 0;
                    work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
                    idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                    break;
                }
            }
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_FLASH_ATTACK_COOLDOWN_FRAMES);
                idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                work->attackSlot      = ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER;
                work->targetYawOffset = 0;
                work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            if (_actor503500IsSlotAtRest(work, work->attackSlot) != 0) {
                idleDelay = ACTOR_503500_FLASH_ATTACK_IDLE_DELAY_FRAMES;
            }
            break;
    }
    return idleDelay;
}

s32 actor503500AttackYellowFlash(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_YELLOW_FLASH_ATTACK_COOLDOWN_FRAMES   = 150,
        ACTOR_503500_YELLOW_FLASH_ATTACK_IDLE_DELAY_FRAMES = 150,
    };
    s32 idleDelay;

    idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_YELLOW_FLASH_ATTACK_COOLDOWN_FRAMES);
                work->attackPhase     = ACTOR_503500_ATTACK_PHASE_WAIT;
                idleDelay             = ACTOR_503500_ATTACK_RUNNING;
                work->targetYawOffset = 0;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            if (_actor503500IsSlotAtRest(work, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER) != 0) {
                idleDelay = ACTOR_503500_YELLOW_FLASH_ATTACK_IDLE_DELAY_FRAMES;
            }
            break;
    }
    return idleDelay;
}

s32 actor503500AttackLargeOrbPair(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_LARGE_ORB_PAIR_PHASE_COMMAND      = 0,
        ACTOR_503500_LARGE_ORB_PAIR_PHASE_WAIT         = 1,
        ACTOR_503500_LARGE_ORB_PAIR_FIRST_SLOT         = 4,
        ACTOR_503500_LARGE_ORB_PAIR_SECOND_SLOT        = 5,
        ACTOR_503500_LARGE_ORB_PAIR_COOLDOWN_FRAMES    = 180,
        ACTOR_503500_LARGE_ORB_PAIR_RETRY_DELAY_FRAMES = 1,
        ACTOR_503500_LARGE_ORB_PAIR_IDLE_DELAY_FRAMES  = 10,
        ACTOR_503500_LARGE_ORB_PAIR_SIDE_BAND_INNER    = 0x200,
        ACTOR_503500_LARGE_ORB_PAIR_SIDE_BAND_OUTER    = 0x600,
    };
    s32 idleDelay;
    s16 playerBearing;

    playerBearing = work->playerBearing;
    idleDelay     = ACTOR_503500_LARGE_ORB_PAIR_RETRY_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_LARGE_ORB_PAIR_PHASE_COMMAND:
            // Either eligible emitter commands both; preserve each leading-slot order.
            if (playerBearing < -(ACTOR_503500_LARGE_ORB_PAIR_SIDE_BAND_OUTER - 1) || work->playerBearing >= -ACTOR_503500_LARGE_ORB_PAIR_SIDE_BAND_INNER) {
                if (_actor503500IsSlotReady(work, ACTOR_503500_LARGE_ORB_PAIR_FIRST_SLOT) != 0) {
                    _actor503500CommandSlot(work, ACTOR_503500_LARGE_ORB_PAIR_FIRST_SLOT, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_LARGE_ORB_PAIR_COOLDOWN_FRAMES);
                    _actor503500CommandSlot(work, ACTOR_503500_LARGE_ORB_PAIR_SECOND_SLOT, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_LARGE_ORB_PAIR_COOLDOWN_FRAMES);
                    idleDelay = 0;
                }
            }
            if (playerBearing < ACTOR_503500_LARGE_ORB_PAIR_SIDE_BAND_INNER + 1 || playerBearing >= ACTOR_503500_LARGE_ORB_PAIR_SIDE_BAND_OUTER) {
                if (_actor503500IsSlotReady(work, ACTOR_503500_LARGE_ORB_PAIR_SECOND_SLOT) != 0) {
                    _actor503500CommandSlot(work, ACTOR_503500_LARGE_ORB_PAIR_SECOND_SLOT, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_LARGE_ORB_PAIR_COOLDOWN_FRAMES);
                    _actor503500CommandSlot(work, ACTOR_503500_LARGE_ORB_PAIR_FIRST_SLOT, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_LARGE_ORB_PAIR_COOLDOWN_FRAMES);
                    idleDelay = 0;
                }
            }
            if (idleDelay == 0) {
                work->attackPhase = ACTOR_503500_LARGE_ORB_PAIR_PHASE_WAIT;
            }
            break;
        case ACTOR_503500_LARGE_ORB_PAIR_PHASE_WAIT:
            idleDelay = 0;
            if (_actor503500IsSlotAtRest(work, ACTOR_503500_LARGE_ORB_PAIR_FIRST_SLOT) != 0) {
                if (_actor503500IsSlotAtRest(work, ACTOR_503500_LARGE_ORB_PAIR_SECOND_SLOT) != 0) {
                    idleDelay = ACTOR_503500_LARGE_ORB_PAIR_IDLE_DELAY_FRAMES;
                }
            }
            break;
    }
    return idleDelay;
}

s32 actor503500AttackSmallOrbVolley(Task* task, Actor503500Work* work)
{
    enum {
        ACTOR_503500_SMALL_ORB_ATTACK_COOLDOWN_FRAMES   = 60,
        ACTOR_503500_SMALL_ORB_ATTACK_IDLE_DELAY_FRAMES = 30,
    };
    s32 idleDelay;

    idleDelay = ACTOR_503500_ATTACK_MIN_IDLE_DELAY_FRAMES;
    switch ((s8)work->attackPhase) {
        case ACTOR_503500_ATTACK_PHASE_COMMAND:
            work->targetYawOffset = 0;
            if (_actor503500IsSlotReady(work, ACTOR_503500_SLOT_SMALL_ORB_EMITTER) != 0) {
                _actor503500CommandSlot(work, ACTOR_503500_SLOT_SMALL_ORB_EMITTER, ACTOR_503500_SLOT_COMMAND_ATTACK, ACTOR_503500_SMALL_ORB_ATTACK_COOLDOWN_FRAMES);
                work->attackPhase = ACTOR_503500_ATTACK_PHASE_WAIT;
                idleDelay         = ACTOR_503500_ATTACK_RUNNING;
            }
            break;
        case ACTOR_503500_ATTACK_PHASE_WAIT:
            idleDelay = ACTOR_503500_ATTACK_RUNNING;
            if (_actor503500IsSlotAtRest(work, ACTOR_503500_SLOT_SMALL_ORB_EMITTER) != 0) {
                idleDelay = ACTOR_503500_SMALL_ORB_ATTACK_IDLE_DELAY_FRAMES;
            }
            break;
    }
    return idleDelay;
}

/// Plays the boss's recoil after losing a part, then returns it to idle.
///
/// Releases the body's effect reservation on entry and starts normal-rate
/// recoil. A finished clip or a superseding animation ends the wait; the
/// target yaw offset is cleared and the existing idle attack delay is retained.
static void _actor503500StepPartLostState(Task* task)
{
    enum {
        ACTOR_503500_PART_LOST_STEP_START_RECOIL = 0,
        ACTOR_503500_PART_LOST_STEP_WAIT_RECOIL  = 1,
    };
    Actor503500Work* work;

    work = task->work;
    switch ((s8)work->stateStep) {
        case ACTOR_503500_PART_LOST_STEP_START_RECOIL:
            actor503500PlayAnimationPreset(task, ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET, ANIMATION_RATE_ONE);
            actor503500ReleaseSlotEffects(task->spawnArg1.value);
            work->stateStep = work->stateStep + 1;
            break;
        case ACTOR_503500_PART_LOST_STEP_WAIT_RECOIL:
            if (actor503500HasAnimationFinished(task, ACTOR_503500_BOSS_RECOIL_ANIMATION_PRESET) != 0) {
                work->targetYawOffset = 0;
                _actor503500EnterCombatState(task, ACTOR_503500_STATE_IDLE);
            }
            break;
    }
}

/// Performs no state work while the boss is held.
///
/// The task argument is unused; this step has no side effects.
static void _actor503500StepHeldState(Task* unusedTask)
{
}

/// Decrements the boss hit cooldown, applies its eight contacts and clears them.
///
/// Requires initialized boss work and a live enemy/model. Negative cooldowns
/// clamp to zero. The boss processes hits whenever this frame path is called;
/// its contact table is cleared after damage and reactions have been applied.
static void _actor503500ProcessBossContacts(Task* task)
{
    Actor503500Work*       work = task->work;
    WorldCollisionContact* contacts;

    if (work->hitCooldown != 0) {
        work->hitCooldown -= 1;
        if (work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }

    contacts = work->contacts;
    _actor503500ApplyBossHits(task, &work->body, contacts, ARRAY_SIZE(work->contacts));
    worldCollisionClearContacts(contacts);
}

/// Updates the boss's lighting and colour mode at its cached root position.
///
/// Requires a live enemy/model and a composed root cache in the lighting
/// query's coordinate frame. Copies XYZ in whole game units before calling
/// the resident colour updater; this handler does not compose the root.
static void _actor503500UpdateBossColor(Task* task)
{
    VECTOR lightingPosition;

    lightingPosition.vx = task->extra.tmd->coords->workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &lightingPosition, 0, 0);
}

/// Updates the boss body's four-face region in the loaded room collision grid.
///
/// Both grid descriptors and their pools must be live. `initializeFaces` copies
/// the four topology records at set-up; every call transforms four normals and
/// eight vertices by boss model part 1. `moveAway` adds 8000 game units along Y
/// during teardown. The rest of the room mesh and its cell lists are retained.
static void _actor503500UpdateBodyCollisionGrid(Task* task, s32 initializeFaces, s32 moveAway)
{
    enum {
        ACTOR_503500_BODY_GRID_FACE_COUNT    = 4,
        ACTOR_503500_BODY_GRID_NORMAL_COUNT  = 4,
        ACTOR_503500_BODY_GRID_VERTEX_COUNT  = 8,
        ACTOR_503500_BODY_GRID_EXIT_Y_OFFSET = 8000,
    };
    MATRIX                    worldRotation;
    SVECTOR                   worldPosition;
    s32                       elementIndex;
    const SVECTOR*            sourceVector;
    SVECTOR*                  destinationVector;
    WorldCollisionGrid*       roomGrid = D_shelter_r48_80183EEC;
    const WorldCollisionGrid* bodyGrid = &D_actor_503500_8016F03C;

    if (initializeFaces != 0) {
        for (elementIndex = 0; elementIndex < ACTOR_503500_BODY_GRID_FACE_COUNT; elementIndex++) {
            roomGrid->faces[elementIndex] = bodyGrid->faces[elementIndex];
        }
    }
    gfxComposeNodeWorldTransform(&task->extra.tmd->coords[1], &worldRotation, &worldPosition);
    if (moveAway != 0) {
        worldPosition.vy += ACTOR_503500_BODY_GRID_EXIT_Y_OFFSET;
    }
    gte_SetRotMatrix(&worldRotation);
    destinationVector = roomGrid->normals;
    sourceVector      = bodyGrid->normals;
    for (elementIndex = 0; elementIndex < ACTOR_503500_BODY_GRID_NORMAL_COUNT; elementIndex++, destinationVector++, sourceVector++) {
        gte_ldv0(sourceVector);
        gte_rtv0();
        gte_stsv(destinationVector);
    }
    destinationVector = roomGrid->vertices;
    sourceVector      = bodyGrid->vertices;
    for (elementIndex = 0; elementIndex < ACTOR_503500_BODY_GRID_VERTEX_COUNT; elementIndex++, destinationVector++, sourceVector++) {
        gte_ldv0(sourceVector);
        gte_rtv0();
        gte_stsv(destinationVector);
        destinationVector->vx += worldPosition.vx;
        destinationVector->vy += worldPosition.vy;
        destinationVector->vz += worldPosition.vz;
    }
}

/// Advances the boss's nineteen driven animation tracks while track 1 is running.
///
/// Requires initialized boss work and a live twenty-part model. An unstarted
/// rig is untouched. A settled track 1 stops all ticking; in idle it instead
/// restores tracks 1..16 to normal rate and requests the twenty-frame idle
/// blend, whose handler immediately ticks the new pose. Other states hold it.
static void _actor503500TickBossAnimation(Task* task)
{
    enum { ACTOR_503500_BOSS_ANIMATION_DEFAULT_RATE = 0 };
    Actor503500Work* work;
    s32              trackIndex;

    work = task->work;
    if (work->animationStarted != 0) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            if (work->state == ACTOR_503500_STATE_IDLE) {
                _actor503500SetBossTrackRates(task, ACTOR_503500_BOSS_ANIMATION_DEFAULT_RATE);
                actor503500HandlePlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_503500_8016EAD4, 0);
            }
        } else {
            for (trackIndex = 1; trackIndex < ARRAY_SIZE(work->rig.slots); trackIndex++) {
                animationTickSlot(&work->rig.anim, trackIndex);
            }
        }
    }
}

/// Reapplies the boss's configured part scales after its frame animation phase.
///
/// Requires live boss work and its twenty-part model; scale vectors use
/// 4096 for 1.0. The private parents of parts 5 and 11 are refreshed from
/// parts 4 and 10. Part 16 scales its current matrix in place, including
/// frames on which animation did not refresh that pose.
static void _actor503500UpdateBossPartScales(Task* task)
{
    _actor503500ApplyPartScales(task);
}

/// Enters a combat state, restarting progress and scheduling its targetability.
///
/// `task` is the boss task; `state` is an `Actor503500State`, stored as a signed
/// halfword. State and attack phases and frame counters restart. Only stunned
/// entry enables the boss's target; all others remove it after three upkeep
/// ticks. The enable test uses the full state argument before storage truncation.
/// Ordinary OT depth is restored immediately.
static void _actor503500EnterCombatState(Task* task, s32 state)
{
    _actor503500ResetBossState(task, state, state == ACTOR_503500_STATE_STUNNED);
}

/// Delivers a command to boss slot 0..16 and starts its busy/cooldown period.
///
/// Slot 0 resets the boss's own attack phase and timer. Other occupied slots
/// receive the command through their task's signed-halfword `killCountdown`.
/// An empty slot only has its busy flag cleared. `cooldownFrames` is 0..32767
/// upkeep ticks and counts down during the boss's idle state. Slot 0 stores
/// the command as a signed byte; other slots store it as a signed halfword.
static void _actor503500CommandSlot(Actor503500Work* work, s32 slot, s32 command, s32 cooldownFrames)
{
    Enemy* enemy;

    if (slot == 0) {
        work->selfAttackCommand = command;
        work->selfAttackPhase   = 0;
        work->selfAttackFrames  = 0;
        work->slotBusy[0]       = 1;
        work->slotCooldown[0]   = cooldownFrames;
        return;
    }
    enemy = work->enemies[slot];
    if (enemy != NULL) {
        enemy->task->killCountdown = command;
        work->slotBusy[slot]       = 1;
        work->slotCooldown[slot]   = cooldownFrames;
        return;
    }
    work->slotBusy[slot] = 0;
}

/// Returns 1 when slot 0..16 is empty or has reported itself at rest.
///
/// Cooldown is ignored; this is the completion test for a commanded attack.
static s32 _actor503500IsSlotAtRest(Actor503500Work* work, s32 slot)
{
    s32 atRest;

    atRest = 1;
    if (work->enemies[slot] != NULL) {
        atRest = work->slotBusy[slot] == 0;
    }
    return atRest;
}

/// Returns 1 when slot 0..16 can accept a new attack command.
///
/// A nonzero slot must have an enemy, no busy report and zero cooldown. Slot
/// 0 instead tests the boss's own pending command and cooldown.
static s32 _actor503500IsSlotReady(Actor503500Work* work, s32 slot)
{
    s32 ready;

    ready = 0;
    if (slot != 0) {
        if (work->enemies[slot] != NULL) {
            if (work->slotCooldown[slot] == 0) {
                ready = work->slotBusy[slot] == 0;
            }
        }
    } else if ((work->selfAttackCommand == ACTOR_503500_SLOT_COMMAND_NONE) && (work->slotCooldown[0] == 0)) {
        ready = 1;
    }
    return ready;
}

/// Sets the boss's sixteen rate-controlled tracks without changing playback.
///
/// Requires initialized boss work. `rate` is a signed step in sixteenths of
/// a frame per tick, narrowed to each slot's signed byte; zero selects
/// `ANIMATION_RATE_ONE`. Only tracks 1..16 change; tracks 0 and 17..19 retain
/// their rates. A subsequent playback reset can replace the written rates.
static void _actor503500SetBossTrackRates(Task* task, s32 rate)
{
    enum { ACTOR_503500_RATE_CONTROLLED_TRACK_COUNT = 16 };
    Actor503500Work* work;
    AnimationSlot*   slot;
    s32              tracksLeft;

    work = task->work;
    slot = &work->rig.slots[1];
    if (rate == 0) {
        rate = ANIMATION_RATE_ONE;
    }
    for (tracksLeft = ACTOR_503500_RATE_CONTROLLED_TRACK_COUNT - 1; tracksLeft >= 0; tracksLeft--) {
        slot->rate = rate;
        slot++;
    }
}

/// Schedules the boss's target to be linked or unlinked after upkeep ticks.
///
/// `targetable` is 0 to remove the target, 1 to enable it. A `delayFrames`
/// value in 1..32767 applies the change when upkeep decrements it to zero.
/// A later call replaces the pending change; zero or negative cancels it.
static void _actor503500ScheduleTargetable(Task* task, s8 targetable, s16 delayFrames)
{
    Actor503500Work* work;

    work                    = task->work;
    work->targetablePending = targetable;
    work->targetableDelay   = delayFrames;
}

s32 actor503500HandlePlaceBoss(Task* task, s32 messageId, const ActorTransform* transform, s32 unusedArg)
{
    Actor503500Work* work;
    GfxCoord*        rootCoord;

    work                    = task->work;
    rootCoord               = task->extra.tmd->coords;
    rootCoord->coord.t[0]   = transform->pos.vx;
    rootCoord->coord.t[1]   = transform->pos.vy;
    rootCoord->coord.t[2]   = transform->pos.vz;
    rootCoord->param.rot.vx = transform->rot.vx;
    rootCoord->param.rot.vy = transform->rot.vy;
    rootCoord->param.rot.vz = transform->rot.vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->yaw               = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]);
    work->position.vx       = transform->pos.vx << ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->position.vy       = transform->pos.vy << ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->position.vz       = transform->pos.vz << ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    return 0;
}

s32 actor503500HandleSetBossModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum {
        ACTOR_503500_BOSS_DRAW_HIDE_AUTO_BUFFER     = 0,
        ACTOR_503500_BOSS_DRAW_SHOW_ALLOCATE        = 1,
        ACTOR_503500_BOSS_DRAW_HIDE_RELEASE         = 2,
        ACTOR_503500_BOSS_DRAW_SHOW_EXISTING_BUFFER = 3,
    };
    TmdObject* model;
    s32        result;

    model  = task->extra.tmd;
    result = 0;
    switch (drawMode) {
        case ACTOR_503500_BOSS_DRAW_HIDE_AUTO_BUFFER:
            model->flags = (model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_503500_BOSS_DRAW_SHOW_ALLOCATE:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_503500_BOSS_DRAW_HIDE_RELEASE: {
            Actor503500Work* work;

            model->flags             |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                      = task->work;
            work->bufferFreeCountdown = drawMode;
            model->flags             |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        }
        case ACTOR_503500_BOSS_DRAW_SHOW_EXISTING_BUFFER:
            model->flags = (model->flags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

void actor503500BossTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_503500_80131E44;
    handlers.funcs[task->state](task);
}

void actor503500AcquireProjectileEffectCost(s32 effectCost)
{
    enum { ACTOR_503500_PROJECTILE_EFFECT_COST_INDEX = ACTOR_503500_SLOT_COUNT };
    D_actor_503500_80176D64[ACTOR_503500_PROJECTILE_EFFECT_COST_INDEX] += effectCost;
}

void actor503500ReleaseProjectileEffectCost(s32 effectCost)
{
    enum { ACTOR_503500_PROJECTILE_EFFECT_COST_INDEX = ACTOR_503500_SLOT_COUNT };
    D_actor_503500_80176D64[ACTOR_503500_PROJECTILE_EFFECT_COST_INDEX] -= effectCost;
}

/// `Task::state` handlers `actor503500PinkFlashEmitterTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131F4C = {
    {
        _actor503500PinkFlashEmitterInit,
        func_actor_503500_8013815C,
        _actor503500PinkFlashEmitterExit,
    },
};

/// Initializes the pink-flash emitter as an idle, targetable boss attachment.
///
/// Requires slot 1 in `spawnArg1.value`, its live enemy in `spawnArg2.pointer`,
/// and the boss as parent. Borrows the singleton work block and boss lighting;
/// attaches the model to boss part 8 and its 800-unit target sphere to the
/// model root. Hides the HP readout, seeds full health and eight contact slots,
/// installs teardown, then advances the task to its update state.
static void _actor503500PinkFlashEmitterInit(Task* task)
{
    enum {
        ACTOR_503500_PINK_FLASH_EMITTER_PARENT_PART         = 8,
        ACTOR_503500_PINK_FLASH_EMITTER_OT_OFFSET           = 19,
        ACTOR_503500_PINK_FLASH_EMITTER_BUFFER_RELEASE_IDLE = -1,
        ACTOR_503500_PINK_FLASH_EMITTER_HIT_EFFECT_LOW_ARG  = 0x400,
    };
    Enemy*                 enemy;
    Task*                  parent;
    TmdObject*             model;
    TmdObject*             parentModel;
    GfxCoord*              rootCoord;
    WorldCollisionContact* contacts;

    enemy       = task->spawnArg2.pointer;
    model       = task->extra.tmd;
    parent      = task->parent;
    rootCoord   = model->coords;
    parentModel = parent->extra.tmd;
    memFillBytes(&D_actor_503500_80176D88, 0, sizeof(D_actor_503500_80176D88));
    task->work = &D_actor_503500_80176D88;

    rootCoord->parent       = &parent->extra.tmd->coords[ACTOR_503500_PINK_FLASH_EMITTER_PARENT_PART];
    rootCoord->coord.t[0]   = D_actor_503500_8016F060.vx;
    rootCoord->coord.t[1]   = D_actor_503500_8016F060.vy;
    rootCoord->coord.t[2]   = D_actor_503500_8016F060.vz;
    model->lightMtx         = parentModel->lightMtx;
    model->colorMtx         = parentModel->colorMtx;
    model->otOffset         = ACTOR_503500_PINK_FLASH_EMITTER_OT_OFFSET;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;

    D_actor_503500_80176D88.bufferFreeCountdown = ACTOR_503500_PINK_FLASH_EMITTER_BUFFER_RELEASE_IDLE;
    enemy->field_4                              = &rootCoord->coord;
    enemy->field_48                             = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F068.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F068.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F068.vz;
    contacts                      = D_actor_503500_80176D88.contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = contacts;
    enemy->hp                     = enemy->param->hpMax;

    D_actor_503500_80176D88.body.coord            = rootCoord;
    D_actor_503500_80176D88.body.context.contacts = contacts;
    D_actor_503500_80176D88.body.key              = ACTOR_503500_PART_BODY_KEY;
    D_actor_503500_80176D88.body.radius           = ACTOR_503500_ATTACHED_TARGET_RADIUS;
    D_actor_503500_80176D88.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    D_actor_503500_80176D88.body.pos.vx           = D_actor_503500_8016F068.vx;
    D_actor_503500_80176D88.body.pos.vy           = D_actor_503500_8016F068.vy;
    D_actor_503500_80176D88.body.pos.vz           = D_actor_503500_8016F068.vz;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_503500_80176D88.body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(D_actor_503500_80176D88.contacts), 0);
    D_actor_503500_80176D88.hitEffect.spawnArgLo = ACTOR_503500_PINK_FLASH_EMITTER_HIT_EFFECT_LOW_ARG;
    D_actor_503500_80176D88.hitEffect.coord      = rootCoord;
    D_actor_503500_80176D88.hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    D_actor_503500_80176D88.body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    _actor503500PinkFlashEmitterEnterState(task, ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
    task->exitCallback = _actor503500PinkFlashEmitterExit;
    task->state       += 1;
}

/// Starts both sides of the pink-flash sweep, then waits through the boss's recovery.
///
/// Requires initialized emitter work, a live boss parent and emitter root.
/// Child spawn argument 1 selects side 0 (negative sweep, owns the charge effect)
/// or side 1 (positive sweep, collision enabled 21 ticks later). Both roots
/// borrow the emitter coordinate; task parenting keeps it live until they exit.
/// Failed sides are skipped independently. Boss interruption returns to idle
/// and releases the slot reservation; normal recovery takes 71 updating ticks.
static void _actor503500PinkFlashEmitterStepAttack(Task* task)
{
    enum {
        ACTOR_503500_PINK_EMITTER_STEP_BEGIN          = 0,
        ACTOR_503500_PINK_EMITTER_STEP_SPAWN          = 1,
        ACTOR_503500_PINK_EMITTER_STEP_WAIT_ANIMATION = 2,
        ACTOR_503500_PINK_EMITTER_STEP_RECOVER        = 3,
        ACTOR_503500_PINK_EMITTER_ANIMATION_ATTACK    = 15,
        ACTOR_503500_PINK_EMITTER_ANIMATION_RECOVER   = 16,
        ACTOR_503500_PINK_FLASH_TASK                  = 2,
        ACTOR_503500_PINK_FLASH_SIDE_COUNT            = 2,
    };
    _Actor503500PinkFlashEmitterWork* work;
    Task*                             attackTask;
    GfxCoord*                         attackCoord;
    s32                               side;

    work = task->work;
    if (actor503500ShouldInterruptAttack(task->parent) != 0) {
        _actor503500PinkFlashEmitterEnterState(task, ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(task->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_PINK_EMITTER_STEP_BEGIN:
            actor503500PlayAnimationPreset(task->parent, ACTOR_503500_PINK_EMITTER_ANIMATION_ATTACK, ANIMATION_RATE_ONE);
            work->stateStep++;
            break;
        case ACTOR_503500_PINK_EMITTER_STEP_SPAWN:
            if (++work->stateFrames > 0) {
                // Each side owns its capsule; side zero also owns the shared charge effect.
                for (side = 0; side < ACTOR_503500_PINK_FLASH_SIDE_COUNT; side++) {
                    attackTask = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_PINK_FLASH_TASK, side, 0);
                    if (attackTask != NULL) {
                        attackCoord             = attackTask->extra.coordBody->coord;
                        attackCoord->parent     = task->extra.tmd->coords;
                        attackCoord->coord.t[0] = D_actor_503500_8016F070.vx;
                        attackCoord->coord.t[1] = D_actor_503500_8016F070.vy;
                        attackCoord->coord.t[2] = D_actor_503500_8016F070.vz;
                        taskReparent(task, attackTask);
                    }
                }
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_PINK_EMITTER_STEP_WAIT_ANIMATION:
            if (actor503500HasAnimationFinished(task->parent, ACTOR_503500_PINK_EMITTER_ANIMATION_ATTACK) != 0) {
                actor503500PlayAnimationPreset(task->parent, ACTOR_503500_PINK_EMITTER_ANIMATION_RECOVER, ANIMATION_RATE_ONE);
                work->stateStep++;
            }
            break;
        case ACTOR_503500_PINK_EMITTER_STEP_RECOVER:
            if (++work->stateFrames >= ACTOR_503500_PINK_FLASH_EMITTER_ATTACK_RECOVERY_FRAMES) {
                _actor503500PinkFlashEmitterEnterState(task, ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// Detaches the dying pink-flash emitter, throws it into a fall and burns it out.
///
/// Requires initialized emitter work, a live boss parent and model root. Stops
/// targeting and reports the lost part before tipping for 31 updates. Detaches
/// in world space, launches at 16 units/frame along its Z and spawns the yellow
/// emitter. Spin and velocity use signed 16.16; gravity adds half a unit/frame
/// each update. Fractional travel is retained. Fade completion or a ready room
/// event advances to exit; smoke offsets cycle through all three entries.
static void _actor503500PinkFlashEmitterStepDying(Task* task)
{
    enum {
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_UNLINK  = 0,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_WAIT    = 1,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_TIP     = 2,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_FALL    = 3,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_FADE    = 4,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_SPIN_STEP    = (1 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS) / 4,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_LAUNCH_SPEED = 16 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_GRAVITY      = (1 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS) / 2,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_EFFECT_COST  = 3,
        ACTOR_503500_PINK_FLASH_EMITTER_DYING_SMOKE_CYCLE  = 6,
        ACTOR_503500_PINK_FLASH_EMITTER_TASK_EXIT          = 2,
    };
    SVECTOR                           spinAngles;
    MATRIX                            rotationMatrix;
    _Actor503500PinkFlashEmitterWork* work;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    const s32*                        sourceWords;
    s32*                              destinationWords;
    s32                               wordIndex;

    /// Detaches the root onto the view while preserving its composed world pose.
    ///
    /// Requires the task's live model root. Captures task, coord, rotationMatrix,
    /// spinAngles (temporarily the composed world position), sourceWords,
    /// destinationWords and wordIndex. The word-aligned rotation transfer writes
    /// exactly 18 bytes, preserving alignment bytes. Expands to multiple statements;
    /// invoke once as a standalone statement within a braced block.
#define ACTOR_503500_PINK_FLASH_EMITTER_DETACH_ROOT()                                                    \
    sourceWords = (const s32*)rotationMatrix.m;                                                          \
    coord       = task->extra.tmd->coords;                                                               \
    gfxComposeNodeWorldTransform(coord, &rotationMatrix, &spinAngles);                                   \
    destinationWords = (s32*)coord->coord.m;                                                             \
    for (wordIndex = 0; wordIndex < (s32)(sizeof(coord->coord.m) / sizeof(*sourceWords)); wordIndex++) { \
        *destinationWords++ = *sourceWords++;                                                            \
    }                                                                                                    \
    coord->coord.m[2][2] = rotationMatrix.m[2][2];                                                       \
    coord->coord.t[0]    = spinAngles.vx;                                                                \
    coord->coord.t[1]    = spinAngles.vy;                                                                \
    coord->coord.t[2]    = spinAngles.vz;                                                                \
    coord->parent        = &gGfxViewCoord;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    switch (work->stateStep) {
        case ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_UNLINK:
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
            work->stateStep++;
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_WAIT:
            work->stateStep++;
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_TIP:
            work->spin.fixed.vx.word -= ACTOR_503500_PINK_FLASH_EMITTER_DYING_SPIN_STEP;
            if (++work->stateFrames >= ACTOR_503500_PINK_FLASH_EMITTER_DYING_TIP_FRAMES) {
                ACTOR_503500_PINK_FLASH_EMITTER_DETACH_ROOT();
                work->velocity.fixed.vx.word = 0;
                work->velocity.fixed.vy.word = 0;
                work->velocity.fixed.vz.word = ACTOR_503500_PINK_FLASH_EMITTER_DYING_LAUNCH_SPEED;
                ApplyMatrixLV(&rotationMatrix, &work->velocity.vector, &work->velocity.vector);
                actor503500SpawnSlotEnemy(task->parent, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER);
                actorRenderComposeCoord(coord);
                sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_FALL:
            work->velocity.fixed.vy.word += ACTOR_503500_PINK_FLASH_EMITTER_DYING_GRAVITY;
            if (++work->stateFrames >= ACTOR_503500_PINK_FLASH_EMITTER_DYING_FALL_FRAMES) {
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_FADE:
            work->velocity.fixed.vy.word += ACTOR_503500_PINK_FLASH_EMITTER_DYING_GRAVITY;
            switch (work->stateFrames) {
                case ACTOR_503500_PINK_FLASH_EMITTER_DYING_FADE_FRAME:
                    task->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(ACTOR_503500_CORPSE_BURN_SOUND, (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case ACTOR_503500_PINK_FLASH_EMITTER_DYING_BLACKEN_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_PINK_FLASH_EMITTER_DYING_END_FRAME:
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
    // Compose incremental spin column by column, then apply whole-unit travel.
    spinAngles.vx = work->spin.fixed.vx.word >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    spinAngles.vy = work->spin.fixed.vy.word >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    spinAngles.vz = work->spin.fixed.vz.word >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    gfxSetRotIdentity(&rotationMatrix);
    RotMatrix(&spinAngles, &rotationMatrix);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&rotationMatrix.m[0][0]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][0]);
    gte_ldclmv(&rotationMatrix.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&rotationMatrix.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);
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
    if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_PINK_FLASH_EMITTER_DYING_EFFECT_COST) != 0) {
        switch (gDisplayState.animFrame % ACTOR_503500_PINK_FLASH_EMITTER_DYING_SMOKE_CYCLE) {
            case 0:
            case 2:
            case 4:
                effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_503500_DYING_SMOKE_SPAWN_ARG,
                            &D_actor_503500_8016F078[work->smokePuffCount++ % (s32)ARRAY_SIZE(D_actor_503500_8016F078)]);
                break;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep >= ACTOR_503500_PINK_FLASH_EMITTER_DYING_STEP_FALL) {
        sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        task->state = ACTOR_503500_PINK_FLASH_EMITTER_TASK_EXIT;
    }
#undef ACTOR_503500_PINK_FLASH_EMITTER_DETACH_ROOT
}

/// Applies one unique attack contact and places its effects on the pink emitter's sphere.
///
/// `contacts[0..contactIndex]` must be readable; earlier equal keys and an active
/// hit cooldown suppress damage. All task, work, enemy and model pointers must
/// remain live. Death starts the dying state and still applies status reactions.
/// The nonzero contact offset is pulled to radius 800 in game units, then taken
/// into `coord`'s local frame. Its cached world transform must already be current.
static inline void _actor503500PinkFlashEmitterHandleHit(Task* task, _Actor503500PinkFlashEmitterWork* work, Enemy* enemy, GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactIndex)
{
    enum { ACTOR_503500_PINK_FLASH_HIT_EFFECT_RADIUS = 800 };
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
    attackerCoord = gPlayerActorTasks[(attackKey >> 7) & 1]->extra.tmd->coords;
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
        _actor503500PinkFlashEmitterEnterState(task, ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING);
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
    // Project the contact onto the sphere, then express the effect point locally.
    gte_TransposeMatrix(&coord->workm, &inverseRotation);
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];
    radialScale       = (ACTOR_503500_PINK_FLASH_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz);
    effectPosition.vx = effectPosition.vx * radialScale / ONE;
    effectPosition.vy = effectPosition.vy * radialScale / ONE;
    effectPosition.vz = effectPosition.vz * radialScale / ONE;
    gte_SetRotMatrix(&inverseRotation);
    gte_ldv0(&effectPosition);
    gte_rtv0();
    gte_stsv(&effectPosition);
    effectPosition.vx += D_actor_503500_8016F068.vx;
    effectPosition.vy += D_actor_503500_8016F068.vy;
    effectPosition.vz += D_actor_503500_8016F068.vz;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to the pink-flash emitter.
///
/// Requires initialized work, a live enemy/model and `contactCount` readable
/// contact elements. Earlier equal keys suppress duplicates; an active hit
/// cooldown suppresses damage. Hit effects appear on its 800-unit target sphere.
/// Contacts are borrowed and left intact. `unusedBody` is retained in the
/// collision-handler signature and is not read.
static void _actor503500PinkFlashEmitterApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500PinkFlashEmitterWork* work;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    s32                               contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500PinkFlashEmitterHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

static void func_actor_503500_8013815C(Task* arg0)
{
    _Actor503500PinkFlashEmitterWork* work;
    Enemy*                            enemy;
    TmdObject*                        tmd;
    s8                                countdown;

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
    if (work->state != ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING) {
        actor503500SyncAttachedModelDrawState(arg0, &work->bufferFreeCountdown);
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (!(tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                _actor503500PinkFlashEmitterNoOp(arg0);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                _actor503500PinkFlashEmitterClearReactions(arg0);
            }
            _actor503500PinkFlashEmitterNoOp(arg0);
            _actor503500PinkFlashEmitterProcessContacts(arg0);
            func_actor_503500_801383D0(arg0);
            break;
    }
}

/// Releases the pink emitter's effect reservation, detaches its model and destroys it.
///
/// The static work block remains allocated; the sphere is unlinked and the
/// enemy's contact pointer is cleared before enemy/task teardown.
static void _actor503500PinkFlashEmitterExit(Task* task)
{
    _Actor503500PinkFlashEmitterWork* work;
    Enemy*                            enemy;

    enemy = task->spawnArg2.pointer;
    actor503500ReleaseSlotEffects(task->spawnArg1.value);
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

/// Performs no work during the pink-flash emitter's paused or active frame path.
///
/// The task argument is unused; this retained hook has no side effects.
static void _actor503500PinkFlashEmitterNoOp(Task* unusedTask)
{
}

/// Advances the pink-flash emitter's hit cooldown, applies contacts and clears the table.
///
/// Requires initialized work and a live enemy/model. Cooldown arithmetic
/// narrows to signed 16 bits before clamping negative values to zero. Boss
/// defeat suppresses hit application; all eight contacts are cleared either way.
static void _actor503500PinkFlashEmitterProcessContacts(Task* task)
{
    _Actor503500PinkFlashEmitterWork* work;
    s16                               remainingCooldown;

    work = task->work;
    if (work->hitCooldown != 0) {
        remainingCooldown = work->hitCooldown - 1;
        work->hitCooldown = remainingCooldown;
        if (remainingCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500PinkFlashEmitterApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

/// Consumes the pink-flash emitter's stagger, build-up and damage-over-time flags.
///
/// Requires its live enemy in the task's second spawn argument. Applies no
/// reaction damage or state change; any other reaction bits are retained.
static void _actor503500PinkFlashEmitterClearReactions(Task* task)
{
    Enemy* enemy;
    u8     reactionFlags;
    u8     damageOverTimeFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
    }
    damageOverTimeFlags = enemy->reactionFlags;
    if (damageOverTimeFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = damageOverTimeFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

static void func_actor_503500_801383D0(Task* arg0)
{
    switch (((_Actor503500PinkFlashEmitterWork*)arg0->work)->state) {
        case ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE:
            _actor503500PinkFlashEmitterStepIdle(arg0);
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_STATE_ATTACK:
            _actor503500PinkFlashEmitterStepAttack(arg0);
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING:
            _actor503500PinkFlashEmitterStepDying(arg0);
            break;
    }
}

/// Starts the pink-flash emitter's attack when the boss commands it.
///
/// Requires initialized emitter work. Consumes the attack command carried
/// in the task's signed-halfword kill countdown; other values are retained.
static void _actor503500PinkFlashEmitterStepIdle(Task* task)
{
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500PinkFlashEmitterEnterState(task, ACTOR_503500_PINK_FLASH_EMITTER_STATE_ATTACK);
        task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    }
}

/// Restarts the pink-flash emitter in a requested state and reports its occupancy.
///
/// `task` must own the emitter work block; `state` is an
/// `ACTOR_503500_PINK_FLASH_EMITTER_STATE_*` value, stored as a signed byte.
/// Clears the state's step and frame counter and consumes any pending boss
/// command. Slot 1 reports at rest only for idle; every other state reports busy.
static void _actor503500PinkFlashEmitterEnterState(Task* task, s32 state)
{
    _Actor503500PinkFlashEmitterWork* work = task->work;

    work->state         = state;
    work->stateStep     = 0;
    work->stateFrames   = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
}

void actor503500PinkFlashEmitterTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_503500_80131F4C;
    handlers.funcs[task->state](task);
}

/// `Task::state` handlers `actor503500LargeChainTask` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131F9C = {
    {
        _actor503500LargeChainInit,
        func_actor_503500_80138898,
        _actor503500LargeChainExit,
    },
};

/// Initializes one of the two large chains with a target on its tip.
///
/// Requires boss slot 2 or 3 in `spawnArg1.value`, its live enemy in
/// `spawnArg2.pointer`, and the boss as parent. Borrows the slot's static work,
/// attaches the nine-part root to boss part 1, and saves child bind matrices.
/// Seeds full health, an 800-unit tip sphere, eight contact slots and a tip
/// speed limit of 128 parent-frame units per update in signed 16.16. Installs
/// teardown before advancing the task to its update state.
static void _actor503500LargeChainInit(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_PARENT_PART             = 1,
        ACTOR_503500_LARGE_CHAIN_OT_OFFSET               = 18,
        ACTOR_503500_LARGE_CHAIN_BUFFER_RELEASE_IDLE     = -1,
        ACTOR_503500_LARGE_CHAIN_HIT_EFFECT_LOW_ARG      = 0x600,
        ACTOR_503500_LARGE_CHAIN_INITIAL_TIP_SPEED_LIMIT = 128 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
    };
    Enemy*                      enemy;
    TmdObject*                  model;
    GfxCoord*                   rootCoord;
    GfxCoord*                   tipCoord;
    _Actor503500LargeChainWork* work;
    WorldCollisionContact*      contacts;
    MATRIX                      rootRotation;
    s32                         chainIndex;
    s32                         partIndex;

    chainIndex = task->spawnArg1.value - ACTOR_503500_SLOT_LARGE_CHAIN_0;
    enemy      = task->spawnArg2.pointer;
    work       = &D_actor_503500_80176EE8[chainIndex];
    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    task->work = work;

    rootCoord->parent     = &task->parent->extra.tmd->coords[ACTOR_503500_LARGE_CHAIN_PARENT_PART];
    tipCoord              = &rootCoord[ACTOR_503500_LARGE_CHAIN_TIP_PART];
    rootCoord->coord.t[0] = D_actor_503500_8016F090[chainIndex].vx;
    rootCoord->coord.t[1] = D_actor_503500_8016F090[chainIndex].vy;
    rootCoord->coord.t[2] = D_actor_503500_8016F090[chainIndex].vz;
    gfxSetRotIdentity(&rootRotation);
    RotMatrix(&D_actor_503500_8016F0A0[chainIndex], &rootRotation);
    MulMatrix0(&rootCoord->coord, &rootRotation, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    for (partIndex = 1; partIndex < ACTOR_503500_LARGE_CHAIN_PART_COUNT; partIndex++) {
        work->bindPose[partIndex] = rootCoord[partIndex].coord;
    }
    work->blendWeight       = ONE;
    model->colorMtx         = &work->colorMtx;
    model->otOffset         = ACTOR_503500_LARGE_CHAIN_OT_OFFSET;
    model->lightMtx         = &work->lightMtx;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;

    work->bufferFreeCountdown = ACTOR_503500_LARGE_CHAIN_BUFFER_RELEASE_IDLE;
    enemy->field_4            = &rootCoord->coord;
    enemy->field_48           = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = tipCoord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F0B0.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F0B0.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F0B0.vz;
    contacts                      = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[task->spawnArg1.value];
    enemy->recs                   = contacts;
    enemy->hp                     = enemy->param->hpMax;

    work->body.coord            = tipCoord;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = D_actor_503500_8016F0B0.vx;
    work->body.pos.vy           = D_actor_503500_8016F0B0.vy;
    work->body.pos.vz           = D_actor_503500_8016F0B0.vz;
    work->body.key              = ACTOR_503500_PART_BODY_KEY;
    work->body.radius           = ACTOR_503500_ATTACHED_TARGET_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->hitEffect.spawnArgLo = ACTOR_503500_LARGE_CHAIN_HIT_EFFECT_LOW_ARG;
    work->hitEffect.coord      = tipCoord;
    work->hitEffect.spawnArgHi = ACTOR_503500_PART_HIT_EFFECT_HIGH_ARG;
    work->body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->tipTarget.vx         = D_actor_503500_8016F0A8[task->spawnArg1.value].vx;
    work->tipTarget.vy         = D_actor_503500_8016F0A8[task->spawnArg1.value].vy;
    work->tipTarget.vz         = D_actor_503500_8016F0A8[task->spawnArg1.value].vz;
    work->tipPosition.vx       = D_actor_503500_8016F0A8[task->spawnArg1.value].vx;
    work->tipPosition.vy       = D_actor_503500_8016F0A8[task->spawnArg1.value].vy;
    work->tipPosition.vz       = D_actor_503500_8016F0A8[task->spawnArg1.value].vz;
    work->tipSpeedLimit.word   = ACTOR_503500_LARGE_CHAIN_INITIAL_TIP_SPEED_LIMIT;
    work->tipAdvancing         = 1;
    task->exitCallback         = _actor503500LargeChainExit;
    task->state               += 1;
}

static void func_actor_503500_80138898(Task* arg0)
{
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    TmdObject*                  tmd;
    s8                          countdown;

    work      = arg0->work;
    enemy     = arg0->spawnArg2.pointer;
    countdown = work->bufferFreeCountdown;
    /* `Task::extra` is a `TmdObject`: the model instance and the actor-ext
     * record documented in `main/session.h` are the same object. */
    tmd = arg0->extra.tmd;
    if (countdown >= 0) {
        if (countdown == 0) {
            tmdFreePrimitiveBuffer(tmd);
        }
        work->bufferFreeCountdown = (s8)((u8)work->bufferFreeCountdown - 1);
    }
    if (gGameSession->eventState != 0 &&
        actor503500IsSlotEmpty(arg0->parent, arg0->spawnArg1.value < 3 ? 0xA : 0xB) == 0) {
        tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        actor503500SyncAttachedModelDrawState(arg0, &work->bufferFreeCountdown);
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (!(tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                _actor503500LargeChainUpdateColor(arg0);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                _actor503500LargeChainHandleReactions(arg0);
            }
            _actor503500LargeChainProcessContacts(arg0);
            func_actor_503500_8013A96C(arg0);
            if (work->detached == 0) {
                _actor503500LargeChainSteerTip(arg0);
                _actor503500LargeChainLayoutLinks(arg0);
            }
            _actor503500LargeChainUpdateColor(arg0);
            _actor503500LargeChainBlendPose(arg0);
            break;
    }
}

/// Orbits an idle large chain's tip target and accepts shooting or splitting.
///
/// Requires slot 2 or 3, initialized chain work and its live enemy. Target
/// positions use the boss attachment's parent frame. Positive health below
/// half maximum starts splitting before an attack command can start a shot.
/// Otherwise rotates a 1000-unit offset about the slot's rest position;
/// Euler angles advance by 32/64/128 units per update, 4096 per turn.
static void _actor503500LargeChainStepIdle(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_IDLE_STEP_SEED_TARGET = 0,
        ACTOR_503500_LARGE_CHAIN_ORBIT_STEP_X          = 32,
        ACTOR_503500_LARGE_CHAIN_ORBIT_STEP_Y          = 64,
        ACTOR_503500_LARGE_CHAIN_ORBIT_STEP_Z          = 128,
    };
    _Actor503500LargeChainWork* work;
    MATRIX                      orbitRotation;
    SVECTOR                     orbitOffset;
    s32                         chainIndex;
    s16                         health;
    Enemy*                      enemy;

    work       = task->work;
    chainIndex = task->spawnArg1.value - ACTOR_503500_SLOT_LARGE_CHAIN_0;
    if (work->stateStep == ACTOR_503500_LARGE_CHAIN_IDLE_STEP_SEED_TARGET) {
        work->tipTarget.vx = D_actor_503500_8016F0B8[chainIndex].vx;
        work->tipTarget.vy = D_actor_503500_8016F0B8[chainIndex].vy;
        work->tipTarget.vz = D_actor_503500_8016F0B8[chainIndex].vz;
        work->stateStep++;
    }
    enemy  = task->spawnArg2.pointer;
    health = enemy->hp;
    if (health < (D_actor_503500_8016E7EC[task->spawnArg1.value].hpMax >> 1) && health > 0) {
        _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_SPLITTING);
        return;
    }
    if (task->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_SHOOT);
        return;
    }
    gfxSetRotIdentity(&orbitRotation);
    RotMatrix(&work->tipOrbitAngles, &orbitRotation);
    gte_SetRotMatrix(&orbitRotation);
    gte_ldv0(&D_actor_503500_8016F0C8);
    gte_rtv0();
    gte_stsv(&orbitOffset);
    work->tipTarget.vx       = D_actor_503500_8016F0B8[chainIndex].vx + orbitOffset.vx;
    work->tipTarget.vy       = D_actor_503500_8016F0B8[chainIndex].vy + orbitOffset.vy;
    work->tipTarget.vz       = D_actor_503500_8016F0B8[chainIndex].vz + orbitOffset.vz;
    work->tipOrbitAngles.vx += ACTOR_503500_LARGE_CHAIN_ORBIT_STEP_X;
    work->tipOrbitAngles.vy += ACTOR_503500_LARGE_CHAIN_ORBIT_STEP_Y;
    work->tipOrbitAngles.vz += ACTOR_503500_LARGE_CHAIN_ORBIT_STEP_Z;
}

/// Aims the large-chain tip above the player and fires a ballistic shot from it.
///
/// Requires initialized chain work, nine model coordinates, a live boss parent
/// and player world matrix. Target Y is 1000 game units above the player, in
/// the root attachment frame. Arrival advances to firing; 91 aim ticks abort.
/// The firing gate uses the accumulated aim count, then adds a tick: it is not
/// a fresh 11-tick wait. A ground-plane distance below 3000 spawns an independent
/// shot 1600 units along tip Z with signed 16.16 speed `distance * 3000`.
/// Spawn failure still enters the 11-tick recovery. Boss interruption returns
/// to idle and releases the slot reservation. Halfword coordinate narrowing is retained.
static void _actor503500LargeChainStepShoot(Task* task)
{
    enum {
        ACTOR_503500_CHAIN_SHOOT_STEP_BEGIN         = 0,
        ACTOR_503500_CHAIN_SHOOT_STEP_AIM           = 1,
        ACTOR_503500_CHAIN_SHOOT_STEP_FIRE          = 2,
        ACTOR_503500_CHAIN_SHOOT_STEP_RECOVER       = 3,
        ACTOR_503500_CHAIN_SHOOT_ANIMATION          = 17,
        ACTOR_503500_CHAIN_SHOOT_TIMEOUT_FRAMES     = 91,
        ACTOR_503500_CHAIN_SHOOT_FIRE_MIN_FRAME     = 11,
        ACTOR_503500_CHAIN_SHOOT_RECOVERY_FRAMES    = 11,
        ACTOR_503500_CHAIN_SHOOT_TARGET_HEIGHT      = 1000,
        ACTOR_503500_CHAIN_SHOOT_RANGE              = 3000,
        ACTOR_503500_CHAIN_SHOOT_SPEED_PER_DISTANCE = 3000, // Signed 16.16 velocity per integer distance unit
        ACTOR_503500_CHAIN_SHOOT_MUZZLE_OFFSET      = 1600,
        ACTOR_503500_CHAIN_SHOOT_BALLISTIC_TASK     = 0,
    };
    SVECTOR                     position;
    SVECTOR                     offset;
    MATRIX                      worldMatrix;
    MATRIX                      attachmentInverse;
    MATRIX*                     attachmentMatrix;
    _Actor503500LargeChainWork* work;
    GfxCoord*                   chainCoords;
    GfxCoord*                   shotCoord;
    Task*                       shotTask;
    const s32*                  sourceWords;
    s32*                        destinationWords;
    s32                         playerDistance;
    s32                         wordIndex;

    /// Places the ballistic shot beyond the chain tip and copies its Q12 rotation.
    ///
    /// Requires the live chain tip and spawned coordinate-body shot. Captures
    /// chainCoords, shotTask, shotCoord, worldMatrix, position, offset, sourceWords,
    /// destinationWords and wordIndex. Reuses the aim scratch; the word view
    /// transfers exactly 18 rotation bytes and preserves alignment and translation.
    /// Expands to multiple statements; invoke once inside a braced block.
#define ACTOR_503500_LARGE_CHAIN_PLACE_SHOT()                                                               \
    gfxComposeNodeWorldTransform(&chainCoords[ACTOR_503500_LARGE_CHAIN_TIP_PART], &worldMatrix, &position); \
    sourceWords = (const s32*)worldMatrix.m;                                                                \
    shotCoord   = shotTask->extra.coordBody->coord;                                                         \
    offset.vx   = 0;                                                                                        \
    offset.vy   = 0;                                                                                        \
    offset.vz   = ACTOR_503500_CHAIN_SHOOT_MUZZLE_OFFSET;                                                   \
    gte_SetRotMatrix(sourceWords);                                                                          \
    gte_ldv0(&offset);                                                                                      \
    gte_rtv0();                                                                                             \
    gte_stsv(&offset);                                                                                      \
    shotCoord->coord.t[0] = position.vx + offset.vx;                                                        \
    shotCoord->coord.t[1] = position.vy + offset.vy;                                                        \
    shotCoord->coord.t[2] = position.vz + offset.vz;                                                        \
    destinationWords      = (s32*)shotCoord->coord.m;                                                       \
    for (wordIndex = 0; wordIndex < (s32)(sizeof(worldMatrix.m) / sizeof(*sourceWords)); wordIndex++) {     \
        *destinationWords++ = *sourceWords++;                                                               \
    }                                                                                                       \
    shotCoord->coord.m[2][2] = worldMatrix.m[2][2];

    work        = task->work;
    chainCoords = task->extra.tmd->coords;
    if (actor503500ShouldInterruptAttack(task->parent) != 0) {
        _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
        actor503500ReleaseSlotEffects(task->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case ACTOR_503500_CHAIN_SHOOT_STEP_BEGIN:
            work->tipArrived = 0;
            actor503500PlayAnimationPreset(task->parent, ACTOR_503500_CHAIN_SHOOT_ANIMATION, ANIMATION_RATE_ONE);
            work->stateStep++;
            // Fall through to track the player on the first shot tick.
        case ACTOR_503500_CHAIN_SHOOT_STEP_AIM:
            if (work->tipArrived != 0) {
                work->stateStep++;
                return;
            }
            if (++work->stateFrames >= ACTOR_503500_CHAIN_SHOOT_TIMEOUT_FRAMES) {
                _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
                return;
            }
            // Convert the elevated player position from world into the attachment frame.
            attachmentMatrix = &worldMatrix;
            gfxComposeNodeWorldTransform(chainCoords->parent, attachmentMatrix, &offset);
            position.vx = gPlayerStatus.coordMtx->t[0] - offset.vx;
            position.vy = gPlayerStatus.coordMtx->t[1] - offset.vy - ACTOR_503500_CHAIN_SHOOT_TARGET_HEIGHT;
            position.vz = gPlayerStatus.coordMtx->t[2] - offset.vz;
            gte_TransposeMatrix(attachmentMatrix, &attachmentInverse);
            gte_SetRotMatrix(&attachmentInverse);
            gte_ldv0(&position);
            gte_rtv0();
            gte_stsv(&work->tipTarget);
            break;
        case ACTOR_503500_CHAIN_SHOOT_STEP_FIRE:
            if (++work->stateFrames >= ACTOR_503500_CHAIN_SHOOT_FIRE_MIN_FRAME) {
                position.vx    = gPlayerStatus.coordMtx->t[0] - work->linkPoints[ACTOR_503500_LARGE_CHAIN_TIP_PART].vx;
                position.vz    = gPlayerStatus.coordMtx->t[2] - work->linkPoints[ACTOR_503500_LARGE_CHAIN_TIP_PART].vz;
                playerDistance = SquareRoot0(position.vx * position.vx + position.vz * position.vz);
                if (playerDistance < ACTOR_503500_CHAIN_SHOOT_RANGE) {
                    shotTask = taskSpawnFromTable(D_actor_503500_8016E9F0, ACTOR_503500_CHAIN_SHOOT_BALLISTIC_TASK, 0, playerDistance * ACTOR_503500_CHAIN_SHOOT_SPEED_PER_DISTANCE);
                    if (shotTask != NULL) {
                        ACTOR_503500_LARGE_CHAIN_PLACE_SHOT();
                    }
                }
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_CHAIN_SHOOT_STEP_RECOVER:
            if (++work->stateFrames >= ACTOR_503500_CHAIN_SHOOT_RECOVERY_FRAMES) {
                _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            }
            break;
    }
#undef ACTOR_503500_LARGE_CHAIN_PLACE_SHOT
}

/// Drops, detaches and collapses a dying large chain before task teardown.
///
/// Requires initialized chain work, nine model coordinates and a live boss
/// parent. Stops targeting and reports the lost slot, lowers the tip target
/// to Y 5000 in the attachment frame, then detaches the root in world space.
/// Link angles ease toward zero in 4096ths of a turn as the root sinks ten
/// units/update. Past Y 1000 its full matrix is saved and Y scale falls to
/// ONE/8. Timed fade, burn and blackening precede exit; a ready room event can
/// request exit after the first step. Smoke covers links 8 down to 1.
static void _actor503500LargeChainStepDying(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_DYING_STEP_UNLINK      = 0,
        ACTOR_503500_LARGE_CHAIN_DYING_STEP_DETACH      = 1,
        ACTOR_503500_LARGE_CHAIN_DYING_STEP_SINK        = 2,
        ACTOR_503500_LARGE_CHAIN_DYING_STEP_SQUASH      = 3,
        ACTOR_503500_LARGE_CHAIN_DYING_TIP_Y            = 5000,
        ACTOR_503500_LARGE_CHAIN_DYING_TIP_SPEED_LIMIT  = 48 << ACTOR_503500_BOSS_FIXED_FRACTION_BITS,
        ACTOR_503500_LARGE_CHAIN_DYING_ANGLE_STEP       = 2,
        ACTOR_503500_LARGE_CHAIN_DYING_ANGLE_COMPONENTS = 3,
        ACTOR_503500_LARGE_CHAIN_DYING_SINK_STEP        = 10,
        ACTOR_503500_LARGE_CHAIN_DYING_SQUASH_Y         = 1000,
        ACTOR_503500_LARGE_CHAIN_DYING_SCALE_MIN        = ONE / 8,
        ACTOR_503500_LARGE_CHAIN_DYING_SCALE_STEP       = 32,
        ACTOR_503500_LARGE_CHAIN_DYING_FADE_FRAME       = 10,
        ACTOR_503500_LARGE_CHAIN_DYING_BURN_FRAME       = 15,
        ACTOR_503500_LARGE_CHAIN_DYING_BLACK_FRAME      = 30,
        ACTOR_503500_LARGE_CHAIN_DYING_EXIT_FRAME       = 40,
        ACTOR_503500_LARGE_CHAIN_DYING_EFFECT_COST      = 4,
        ACTOR_503500_LARGE_CHAIN_DYING_SMOKE_PERIOD     = 12,
        ACTOR_503500_LARGE_CHAIN_DYING_TASK_EXIT        = 2,
    };
    MATRIX                      worldRotation;
    VECTOR                      scale;
    SVECTOR                     linkAngles;
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    GfxCoord*                   coord;
    GfxCoord*                   linkCoord;
    s16*                        angleComponent;
    s32                         stateStep;
    s32                         linkIndex;
    s32                         elementIndex;

    /// Eases the link's three signed Euler angles toward zero without overshooting.
    ///
    /// Captures linkAngles, angleComponent and elementIndex, and requires this
    /// handler's angle-step/component constants. Angles use 4096 units per turn;
    /// only the SDK vector's XYZ halfwords are visited. Reuses elementIndex with
    /// the later smoke-offset walk. Expands to multiple statements; invoke as a
    /// standalone statement within a braced block.
#define ACTOR_503500_LARGE_CHAIN_EASE_DYING_ANGLES()                      \
    angleComponent = &linkAngles.vx;                                      \
    elementIndex   = 1;                                                   \
    do {                                                                  \
        if (*angleComponent > 0) {                                        \
            *angleComponent -= ACTOR_503500_LARGE_CHAIN_DYING_ANGLE_STEP; \
            if (*angleComponent < 0) {                                    \
                *angleComponent = 0;                                      \
            }                                                             \
        } else {                                                          \
            *angleComponent += ACTOR_503500_LARGE_CHAIN_DYING_ANGLE_STEP; \
            if (*angleComponent > 0) {                                    \
                *angleComponent = 0;                                      \
            }                                                             \
        }                                                                 \
        angleComponent++;                                                 \
    } while (elementIndex++ < ACTOR_503500_LARGE_CHAIN_DYING_ANGLE_COMPONENTS);

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    stateStep = work->stateStep;
    coord     = task->extra.tmd->coords;
    switch (stateStep) {
        case ACTOR_503500_LARGE_CHAIN_DYING_STEP_UNLINK:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->recs       = NULL;
            worldTargetUnlinkNode(&enemy->node);
            actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
            work->hitCooldown = 0;
            (sceneAcquireBattleRef)(0);
            sceneReleaseBattleRefWithRewards(task, 0);
            actor503500EnterPartLostState(task->parent);
            enemy->reactionFlags    &= ENEMY_REACTION_LOW_CLEAR;
            work->tipTarget.vy       = ACTOR_503500_LARGE_CHAIN_DYING_TIP_Y;
            work->tipSpeedLimit.word = ACTOR_503500_LARGE_CHAIN_DYING_TIP_SPEED_LIMIT;
            work->tipArrived         = 0;
            work->stateStep++;
            break;
        case ACTOR_503500_LARGE_CHAIN_DYING_STEP_DETACH:
            if (work->tipArrived != 0) {
                gfxComposeNodeWorldTransform(coord, &worldRotation, &linkAngles);
                coord->coord        = worldRotation;
                coord->coord.t[0]   = linkAngles.vx;
                coord->coord.t[1]   = linkAngles.vy;
                coord->coord.t[2]   = linkAngles.vz;
                coord->parent       = &gGfxViewCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->detached      = 1;
                actorRenderComposeCoord(coord);
                sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LARGE_CHAIN_DYING_STEP_SINK:
            // Straighten links while the detached root sinks toward its squash height.
            for (linkIndex = 1; linkIndex < ACTOR_503500_LARGE_CHAIN_PART_COUNT; linkIndex++) {
                linkCoord = &coord[linkIndex];
                gfxExtractSmallestEuler(&linkAngles, &linkCoord->coord);
                ACTOR_503500_LARGE_CHAIN_EASE_DYING_ANGLES();
                gfxSetRotIdentity(&coord[linkIndex].coord);
                RotMatrix(&linkAngles, &linkCoord->coord);
                linkCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += ACTOR_503500_LARGE_CHAIN_DYING_SINK_STEP;
            if (coord->coord.t[1] > ACTOR_503500_LARGE_CHAIN_DYING_SQUASH_Y) {
                work->unscaledRootMatrix = coord->coord;
                work->collapseScaleY     = ONE;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LARGE_CHAIN_DYING_STEP_SQUASH:
            if (work->collapseScaleY > ACTOR_503500_LARGE_CHAIN_DYING_SCALE_MIN) {
                work->collapseScaleY -= ACTOR_503500_LARGE_CHAIN_DYING_SCALE_STEP;
            }
            // Restart from the snapshot so scaling does not compound.
            coord->coord = work->unscaledRootMatrix;
            scale.vx     = ONE;
            scale.vy     = work->collapseScaleY;
            scale.vz     = ONE;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            switch (work->stateFrames) {
                case ACTOR_503500_LARGE_CHAIN_DYING_FADE_FRAME:
                    task->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(ACTOR_503500_CORPSE_BURN_SOUND, (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case ACTOR_503500_LARGE_CHAIN_DYING_BURN_FRAME:
                    effectSpawn(EFFECT_CORPSE_BURN, coord, 1, NULL);
                    break;
                case ACTOR_503500_LARGE_CHAIN_DYING_BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_LARGE_CHAIN_DYING_EXIT_FRAME:
                    sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    task->state++;
                    break;
            }
            work->stateFrames++;
            break;
    }
    if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_LARGE_CHAIN_DYING_EFFECT_COST) != 0 && work->stateStep < ACTOR_503500_LARGE_CHAIN_DYING_STEP_SQUASH &&
        gDisplayState.animFrame % ACTOR_503500_LARGE_CHAIN_DYING_SMOKE_PERIOD == 0) {
        for (linkIndex = ACTOR_503500_LARGE_CHAIN_TIP_PART, elementIndex = 0; linkIndex > 0; linkIndex--) {
            effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[linkIndex], ACTOR_503500_DYING_SMOKE_SPAWN_ARG, &D_actor_503500_8016F0D0[elementIndex]);
            elementIndex++;
            elementIndex = (elementIndex < (s32)ARRAY_SIZE(D_actor_503500_8016F0D0)) ? elementIndex : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep > ACTOR_503500_LARGE_CHAIN_DYING_STEP_UNLINK) {
        sndEvtRequestScriptStop(ACTOR_503500_CORPSE_BURN_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        task->state = ACTOR_503500_LARGE_CHAIN_DYING_TASK_EXIT;
    }
#undef ACTOR_503500_LARGE_CHAIN_EASE_DYING_ANGLES
}

/// Replaces a folded large chain with two lunging chains sharing its remaining HP.
///
/// Requires initialized chain work, nine model coordinates and a live boss
/// parent. Disables targeting and decreases the Q12 pose weight to zero. The
/// next 26 updates emit two puffs on parts 0..8 (three updates per part).
/// Slot 2 produces slots 13/14; slot 3 produces 15/16. Each successful spawn
/// receives half the remaining HP, truncated, and a become-target command.
/// Hides the old model, defers buffer release three frames and exits 91 updates
/// later. Failed spawns are retained behavior: they are not retried.
static void _actor503500LargeChainStepSplitting(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_UNLINK        = 0,
        ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_FOLD          = 1,
        ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_REPLACE       = 2,
        ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_HIDE          = 3,
        ACTOR_503500_LARGE_CHAIN_SPLIT_BLEND_STEP         = 32,
        ACTOR_503500_LARGE_CHAIN_SPLIT_PART_FRAMES        = 3,
        ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_Y             = 700,
        ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_X_STEP        = 33,
        ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_ARG           = 0x11101800,
        ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_FRAMES        = 26,
        ACTOR_503500_LARGE_CHAIN_SPLIT_EXIT_FRAMES        = 91,
        ACTOR_503500_LARGE_CHAIN_SPLIT_EFFECT_COST        = 4,
        ACTOR_503500_LARGE_CHAIN_SPLIT_BUFFER_FREE_FRAMES = 3,
    };
    SVECTOR                     puffOffset;
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    Enemy*                      splitEnemy;
    GfxCoord*                   coord;
    s32                         stateStep;
    s32                         firstChildSlot;
    s32                         secondChildSlot;

    work      = task->work;
    stateStep = work->stateStep;
    enemy     = task->spawnArg2.pointer;
    switch (stateStep) {
        case ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_UNLINK:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = NULL;
            work->hitCooldown = 0;
            work->stateStep++;
            // Begin folding on the update that removes the target.
        case ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_FOLD:
            work->blendWeight -= ACTOR_503500_LARGE_CHAIN_SPLIT_BLEND_STEP;
            if (work->blendWeight <= 0) {
                work->blendWeight = 0;
                work->field_248   = NULL;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_REPLACE:
            if (actor503500TryReserveSlotEffects(task->spawnArg1.value, ACTOR_503500_LARGE_CHAIN_SPLIT_EFFECT_COST) != 0) {
                coord = &task->extra.tmd->coords[(s16)(work->stateFrames / ACTOR_503500_LARGE_CHAIN_SPLIT_PART_FRAMES)];
                // Z is not initialized here in the original sequence.
                puffOffset.vx = 0;
                puffOffset.vy = -ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_Y;
                puffOffset.vx = (s16)(work->stateFrames % ACTOR_503500_LARGE_CHAIN_SPLIT_PART_FRAMES) * ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_X_STEP;
                effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_ARG, &puffOffset);
                puffOffset.vy = ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_Y;
                effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_ARG, &puffOffset);
            }
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_LARGE_CHAIN_SPLIT_PUFF_FRAMES) {
                firstChildSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_2;
                // At this step, stateStep is also slot 2's identifier.
                if (task->spawnArg1.value == stateStep) {
                    firstChildSlot  = ACTOR_503500_SLOT_LUNGING_CHAIN_0;
                    secondChildSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_1;
                } else {
                    secondChildSlot = ACTOR_503500_SLOT_LUNGING_CHAIN_3;
                }
                splitEnemy = actor503500SpawnSlotEnemy(task->parent, firstChildSlot);
                if (splitEnemy != NULL) {
                    splitEnemy->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
                    splitEnemy->hp                  = enemy->hp / 2;
                }
                splitEnemy = actor503500SpawnSlotEnemy(task->parent, secondChildSlot);
                if (splitEnemy != NULL) {
                    splitEnemy->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
                    splitEnemy->hp                  = enemy->hp / 2;
                }
                actor503500ClearSlotEnemy(task->parent, task->spawnArg1.value);
                work->bufferFreeCountdown = ACTOR_503500_LARGE_CHAIN_SPLIT_BUFFER_FREE_FRAMES;
                work->stateFrames         = 0;
                work->stateStep++;
            }
            break;
        case ACTOR_503500_LARGE_CHAIN_SPLIT_STEP_HIDE:
            task->extra.tmd->flags |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work->stateFrames++;
            if (work->stateFrames >= ACTOR_503500_LARGE_CHAIN_SPLIT_EXIT_FRAMES) {
                actor503500ReleaseSlotEffects(task->spawnArg1.value);
                task->state++;
            }
            break;
    }
}

/// Processes a large chain's stagger, build-up and damage-over-time reactions.
///
/// Requires initialized chain work and its live enemy. Defeat or an active
/// event leaves all reactions pending. Stagger returns to idle and seeds
/// a five-frame hold counter and eight-frame slow counter; it does not enter
/// the hold state. Build-up is discarded. A pending damage-over-time tick
/// holds the chain in that state until expiry or nonzero damage; damage
/// returns a survivor to idle and an exhausted enemy to dying.
static void _actor503500LargeChainHandleReactions(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_STAGGER_HOLD_FRAMES  = 5,
        ACTOR_503500_LARGE_CHAIN_REACTION_SLOW_FRAMES = 8,
    };
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    s32                         damage;
    u8                          reactionFlags;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if ((actor503500IsDefeated() == 0) && (gGameSession->eventState == 0)) {
        reactionFlags = enemy->reactionFlags;
        if (reactionFlags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
            _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            work->holdFrames = ACTOR_503500_LARGE_CHAIN_STAGGER_HOLD_FRAMES;
            work->slowFrames = ACTOR_503500_LARGE_CHAIN_REACTION_SLOW_FRAMES;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_DAMAGE_OVER_TIME);
            if (damageIsEnemyDamageOverTimeExpired(task->spawnArg2.pointer) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            } else {
                damage = damageTickEnemyDamageOverTime(enemy);
                if (damage != 0) {
                    enemy->hp -= damage;
                    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                    work->slowFrames = ACTOR_503500_LARGE_CHAIN_REACTION_SLOW_FRAMES;
                    if (enemy->hp <= 0) {
                        enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                        _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_DYING);
                    } else {
                        _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
                    }
                }
            }
        }
    }
}

/// Applies one unique attack contact and places its effects on the large chain's tip sphere.
///
/// `contacts[0..contactIndex]` must be readable; earlier equal keys and an active
/// hit cooldown suppress damage. The pointers belong to a live large-chain task;
/// `coord` is its tip. Death replaces status reactions. The nonzero contact
/// offset is pulled to radius 800 in game units, then taken into the tip's local
/// frame; its cached world transform must already be current.
static inline void _actor503500LargeChainHandleHit(Task* task, _Actor503500LargeChainWork* work, Enemy* enemy, GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactIndex)
{
    enum { ACTOR_503500_LARGE_CHAIN_HIT_EFFECT_RADIUS = 800 };
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
    attackerCoord     = gPlayerActorTasks[(attackKey >> 7) & 1]->extra.tmd->coords;
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
        _actor503500LargeChainEnterState(task, ACTOR_503500_LARGE_CHAIN_STATE_DYING);
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
    // Project the contact onto the sphere, then express the effect point locally.
    gte_TransposeMatrix(&coord->workm, &inverseRotation);
    effectPosition.vx = contacts[contactIndex].point.vx - coord->workm.t[0];
    effectPosition.vy = contacts[contactIndex].point.vy - coord->workm.t[1];
    effectPosition.vz = contacts[contactIndex].point.vz - coord->workm.t[2];
    radialScale       = (ACTOR_503500_LARGE_CHAIN_HIT_EFFECT_RADIUS * ONE) / SquareRoot0(effectPosition.vx * effectPosition.vx + effectPosition.vy * effectPosition.vy + effectPosition.vz * effectPosition.vz);
    effectPosition.vx = effectPosition.vx * radialScale / ONE;
    effectPosition.vy = effectPosition.vy * radialScale / ONE;
    effectPosition.vz = effectPosition.vz * radialScale / ONE;
    gte_SetRotMatrix(&inverseRotation);
    gte_ldv0(&effectPosition);
    gte_rtv0();
    gte_stsv(&effectPosition);
    effectPosition.vx += D_actor_503500_8016F0B0.vx;
    effectPosition.vy += D_actor_503500_8016F0B0.vy;
    effectPosition.vz += D_actor_503500_8016F0B0.vz;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &effectPosition, &work->hitEffect);
    if (criticalHit != 0) {
        effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, &effectPosition);
    }
    hitCooldownFrames = damageGetPlayerAttackHitCooldown(attackKey);
    if (work->hitCooldown < hitCooldownFrames) {
        work->hitCooldown = hitCooldownFrames;
    }
}

/// Applies eligible attack contacts to the large chain.
///
/// Requires initialized work, a live enemy/model and `contactCount` readable
/// contact elements. Earlier equal keys suppress duplicates; an active hit
/// cooldown suppresses damage. Hit effects appear on its 800-unit tip sphere.
/// Contacts are borrowed and left intact. `unusedBody` is retained in the
/// collision-handler signature and is not read.
static void _actor503500LargeChainApplyHits(Task* task, WorldCollisionBody* unusedBody, const WorldCollisionContact* contacts, s32 contactCount)
{
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    GfxCoord*                   coord;
    s32                         contactIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    coord = &task->extra.tmd->coords[ACTOR_503500_LARGE_CHAIN_TIP_PART];
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        _actor503500LargeChainHandleHit(task, work, enemy, coord, contacts, contactIndex);
    }
}

/// Steps a large chain's tip toward its target in the boss attachment frame.
///
/// Requires initialized chain work. XYZ differences narrow to signed
/// halfwords before their absolute values are summed. A sum strictly below
/// the speed limit's integer half sets arrival without changing speed or
/// position. Otherwise speed accelerates by limit/32, or brakes to zero,
/// in signed 16.16 units per update. A nonzero slow counter quarters only
/// this update's travel; normalization uses Q12 and per-axis motion drops
/// its fractional units rather than carrying them into the next update.
static void _actor503500LargeChainSteerTip(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_DIRECTION_FRACTION_BITS = 12,
        ACTOR_503500_LARGE_CHAIN_SLOW_SPEED_SHIFT        = 2,
    };
    SVECTOR                     targetOffset;
    SVECTOR                     direction;
    VECTOR                      stepFixed;
    _Actor503500LargeChainWork* work;
    s16                         offsetX;
    s16                         offsetY;
    s16                         offsetZ;
    s32                         speedLimit;
    s32                         tipSpeed;
    s32                         speedInDirectionUnits;

    work            = task->work;
    offsetX         = work->tipTarget.vx - work->tipPosition.vx;
    targetOffset.vx = offsetX;
    offsetY         = work->tipTarget.vy - work->tipPosition.vy;
    targetOffset.vy = offsetY;
    offsetZ         = work->tipTarget.vz - work->tipPosition.vz;
    targetOffset.vz = offsetZ;
    if (ABS(offsetX) + ABS(offsetY) + ABS(offsetZ) < work->tipSpeedLimit.halves.integer) {
        work->tipArrived = 1;
        return;
    }
    // Arrival preserves the stored speed; only a travelling tip accelerates.
    speedLimit       = work->tipSpeedLimit.word;
    work->tipArrived = 0;
    if (work->tipAdvancing != 0) {
        tipSpeed = work->tipSpeed + speedLimit / 32;
        if (tipSpeed > 0) {
            if (tipSpeed > speedLimit) {
                tipSpeed = speedLimit;
            }
        } else if (tipSpeed < -speedLimit) {
            tipSpeed = -speedLimit;
        }
    } else {
        tipSpeed = work->tipSpeed - speedLimit / 32;
        if (tipSpeed < 0) {
            tipSpeed = 0;
        }
    }
    work->tipSpeed = tipSpeed;
    VectorNormalSS(&targetOffset, &direction);
    if (work->slowFrames != 0) {
        tipSpeed >>= ACTOR_503500_LARGE_CHAIN_SLOW_SPEED_SHIFT;
    }
    speedInDirectionUnits = tipSpeed >> ACTOR_503500_LARGE_CHAIN_DIRECTION_FRACTION_BITS;
    stepFixed.vx          = direction.vx * speedInDirectionUnits;
    stepFixed.vy          = direction.vy * speedInDirectionUnits;
    stepFixed.vz          = direction.vz * speedInDirectionUnits;
    work->tipPosition.vx += stepFixed.vx >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->tipPosition.vy += stepFixed.vy >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
    work->tipPosition.vz += stepFixed.vz >> ACTOR_503500_BOSS_FIXED_FRACTION_BITS;
}

/// Lays out a large chain's eight links along two world-space cubic curves.
///
/// Requires nine model coordinates, initialized chain work and the live
/// boss attachment parent. Transforms the parent-local tip into world space,
/// then samples a guide curve to seed a five-point root arc and a three-point
/// tip arc. Coordinate calculations narrow to signed halfwords and the
/// Bezier coefficients and products must fit their signed types. Aims the
/// links along the nine root-first points and advances their length pulse
/// by 128 modulo 4096 angle units. Changes GTE and scratch-stack state.
static void _actor503500LargeChainLayoutLinks(Task* task)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_ROOT_ARC_POINTS    = 5,
        ACTOR_503500_LARGE_CHAIN_TIP_ARC_POINTS     = 3,
        ACTOR_503500_LARGE_CHAIN_TIP_ARC_STEPS      = 16,
        ACTOR_503500_LARGE_CHAIN_TIP_ARC_FIRST_STEP = 12,
        ACTOR_503500_LARGE_CHAIN_PULSE_STEP         = 128,
    };
    SVECTOR                     controlPoints[4];
    SVECTOR                     rotatedOffset;
    SVECTOR                     tipWorldPosition;
    VECTOR                      guideSamples[ACTOR_503500_LARGE_CHAIN_PART_COUNT];
    VECTOR                      curvePoint;
    MATRIX                      worldRotation;
    GfxCoord*                   rootCoord;
    _Actor503500LargeChainWork* work;
    s32                         sampleIndex;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    // Build a guide from the root tangent to the raised parent-local tip.
    gfxComposeNodeWorldTransform(rootCoord, &worldRotation, &controlPoints[0]);
    work->linkPoints[0].vx = controlPoints[0].vx;
    work->linkPoints[0].vy = controlPoints[0].vy;
    work->linkPoints[0].vz = controlPoints[0].vz;
    rotatedOffset.vx       = 0;
    rotatedOffset.vy       = 0;
    rotatedOffset.vz       = 1000;
    gte_SetRotMatrix(&worldRotation);
    gte_ldv0(&rotatedOffset);
    gte_rtv0();
    gte_stsv(&controlPoints[1]);
    controlPoints[1].vx += controlPoints[0].vx;
    controlPoints[1].vy += controlPoints[0].vy;
    controlPoints[1].vz += controlPoints[0].vz;
    gfxComposeNodeWorldTransform(rootCoord->parent, &worldRotation, &tipWorldPosition);
    gte_SetRotMatrix(&worldRotation);
    gte_ldv0(&work->tipPosition);
    gte_rtv0();
    gte_stsv(&rotatedOffset);
    tipWorldPosition.vx += rotatedOffset.vx;
    tipWorldPosition.vy += rotatedOffset.vy;
    tipWorldPosition.vz += rotatedOffset.vz;
    controlPoints[2].vx  = tipWorldPosition.vx;
    controlPoints[2].vy  = tipWorldPosition.vy - 3000;
    controlPoints[2].vz  = tipWorldPosition.vz;
    controlPoints[3].vx  = tipWorldPosition.vx;
    controlPoints[3].vy  = tipWorldPosition.vy - 2000;
    controlPoints[3].vz  = tipWorldPosition.vz;
    for (sampleIndex = ARRAY_SIZE(guideSamples) - 1; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(controlPoints, &controlPoints[3], ARRAY_SIZE(guideSamples), sampleIndex, &guideSamples[sampleIndex].vx);
    }
    // Re-seed the root arc from the guide, then join the tip arc at sample 4.
    controlPoints[0].vx = guideSamples[8].vx;
    controlPoints[0].vy = guideSamples[8].vy;
    controlPoints[0].vz = guideSamples[8].vz;
    controlPoints[1].vx = guideSamples[6].vx;
    controlPoints[1].vy = guideSamples[6].vy + 1000;
    controlPoints[1].vz = guideSamples[6].vz;
    controlPoints[2].vx = guideSamples[5].vx;
    controlPoints[2].vy = guideSamples[5].vy - 2000;
    controlPoints[2].vz = guideSamples[5].vz;
    controlPoints[3].vx = guideSamples[4].vx;
    controlPoints[3].vy = guideSamples[4].vy - 2000;
    controlPoints[3].vz = guideSamples[4].vz;
    for (sampleIndex = ACTOR_503500_LARGE_CHAIN_ROOT_ARC_POINTS - 1; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(controlPoints, &controlPoints[3], ACTOR_503500_LARGE_CHAIN_ROOT_ARC_POINTS, sampleIndex, &curvePoint.vx);
        copyVector(&work->linkPoints[ACTOR_503500_LARGE_CHAIN_ROOT_ARC_POINTS - sampleIndex], &curvePoint);
    }
    controlPoints[0].vx = guideSamples[4].vx;
    controlPoints[0].vy = guideSamples[4].vy - 2000;
    controlPoints[0].vz = guideSamples[4].vz;
    controlPoints[1].vx = guideSamples[3].vx;
    controlPoints[1].vy = guideSamples[3].vy - 2000;
    controlPoints[1].vz = guideSamples[3].vz;
    controlPoints[2].vx = tipWorldPosition.vx;
    controlPoints[2].vy = tipWorldPosition.vy - 1000;
    controlPoints[2].vz = tipWorldPosition.vz;
    controlPoints[3].vx = tipWorldPosition.vx;
    controlPoints[3].vy = tipWorldPosition.vy;
    controlPoints[3].vz = tipWorldPosition.vz;
    for (sampleIndex = ACTOR_503500_LARGE_CHAIN_TIP_ARC_POINTS - 1; sampleIndex >= 0; sampleIndex--) {
        _bezierCurveEvaluate(controlPoints, &controlPoints[3], ACTOR_503500_LARGE_CHAIN_TIP_ARC_STEPS, sampleIndex + ACTOR_503500_LARGE_CHAIN_TIP_ARC_FIRST_STEP, &curvePoint.vx);
        copyVector(&work->linkPoints[ACTOR_503500_LARGE_CHAIN_TIP_PART - sampleIndex], &curvePoint);
    }
    _actor503500LargeChainPlaceLinks(work->linkPoints, task->extra.tmd->coords, work->pulsePhase);
    work->pulsePhase = (work->pulsePhase + ACTOR_503500_LARGE_CHAIN_PULSE_STEP) & (ACTOR_TRANSFORM_ANGLE_TURN - 1);
}

/// Aims the large chain's eight links along nine world-space points with a length pulse.
///
/// `points` and writable `coordinates` contain nine root-first entries. Each
/// segment must fit signed halfwords, be nonzero, and not be parallel to its
/// current frame's +Y hint. The root's live parent chain supplies an orthonormal
/// rotation for inverse transposition. Normalizes each new basis, and scales
/// translations for links 2..8 by 1 + sin(pulsePhase)/64; link 1's translation
/// remains intact. Phase uses 4096 per turn. Borrows scratch-stack storage,
/// changes GTE state, and retains all composition stamps.
static void _actor503500LargeChainPlaceLinks(const SVECTOR* points, GfxCoord* coordinates, s32 pulsePhase)
{
    enum {
        ACTOR_503500_LARGE_CHAIN_MATRIX_FRACTION_BITS = 12,
        ACTOR_503500_LARGE_CHAIN_PULSE_SHIFT          = 6,
    };
    Actor503500ChainScratch* scratch;
    s32                      linkScale;
    s32                      segmentIndex;
    s32                      partIndex;

    scratch        = SCRATCH_STACK_RESERVE_BLOCK(Actor503500ChainScratch);
    scratch->up.vx = 0;
    scratch->up.vy = ONE;
    scratch->up.vz = 0;
    gfxComposeNodeWorldTransform(coordinates->parent, &scratch->worldRotation, &scratch->parentTranslation);
    linkScale = ((rsin(pulsePhase) << ACTOR_503500_LARGE_CHAIN_PULSE_SHIFT) >> ACTOR_503500_LARGE_CHAIN_MATRIX_FRACTION_BITS) + ONE;
    for (segmentIndex = 0, partIndex = 1; segmentIndex < ACTOR_503500_LARGE_CHAIN_PART_COUNT - 1; segmentIndex++, partIndex++) {
        scratch->segment.vx = points[partIndex].vx - points[segmentIndex].vx;
        scratch->segment.vy = points[partIndex].vy - points[segmentIndex].vy;
        scratch->segment.vz = points[partIndex].vz - points[segmentIndex].vz;
        // Carry the current link rotation forward, then undo it for the next segment.
        gte_SetRotMatrix(&scratch->worldRotation);
        gte_ldclmv(&coordinates[segmentIndex].coord);
        gte_rtir();
        gte_stclmv(&scratch->worldRotation);
        gte_ldclmv(&coordinates[segmentIndex].coord.m[0][1]);
        gte_rtir();
        gte_stclmv(&scratch->worldRotation.m[0][1]);
        gte_ldclmv(&coordinates[segmentIndex].coord.m[0][2]);
        gte_rtir();
        gte_stclmv(&scratch->worldRotation.m[0][2]);
        gte_TransposeMatrix(&scratch->worldRotation, &scratch->inverseRotation);
        gte_SetRotMatrix(&scratch->inverseRotation);
        gte_ldv0(&scratch->segment);
        gte_rtv0();
        gte_stlvnl(&scratch->localSegment);
        VectorNormalS(&scratch->localSegment, &scratch->direction);
        gfxBuildOrthonormalBasis(&scratch->basis, &scratch->direction, &scratch->up);
        MatrixNormal(&scratch->basis, &coordinates[partIndex].coord);
        if (segmentIndex != 0) {
            coordinates[partIndex].coord.t[0] = (scratch->localSegment.vx * linkScale) >> ACTOR_503500_LARGE_CHAIN_MATRIX_FRACTION_BITS;
            coordinates[partIndex].coord.t[1] = (scratch->localSegment.vy * linkScale) >> ACTOR_503500_LARGE_CHAIN_MATRIX_FRACTION_BITS;
            coordinates[partIndex].coord.t[2] = (scratch->localSegment.vz * linkScale) >> ACTOR_503500_LARGE_CHAIN_MATRIX_FRACTION_BITS;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor503500ChainScratch);
}

#include "../../shared/bezier_curve_evaluate.inc.c"

/// Releases the large chain's effect reservation, detaches its model and destroys it.
///
/// The static per-slot work block remains allocated; the tip sphere is unlinked
/// and the enemy's contact pointer is cleared before enemy/task teardown.
static void _actor503500LargeChainExit(Task* task)
{
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;

    enemy = task->spawnArg2.pointer;
    actor503500ReleaseSlotEffects(task->spawnArg1.value);
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work                            = task->work;
    worldCollisionUnlinkBody(&work->body);
    enemy->recs = NULL;
    task->work  = NULL;
    enemyDestroy(enemy, task);
}

static void func_actor_503500_8013A96C(Task* arg0)
{
    _Actor503500LargeChainWork* work;
    s16                         timer;

    work = arg0->work;
    switch (work->state) {
        case ACTOR_503500_LARGE_CHAIN_STATE_IDLE:
            _actor503500LargeChainStepIdle(arg0);
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_SHOOT:
            _actor503500LargeChainStepShoot(arg0);
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_HOLD:
            timer            = (u16)work->holdFrames - 1;
            work->holdFrames = timer;
            if (timer < 0) {
                _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            }
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_DYING:
            _actor503500LargeChainStepDying(arg0);
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_SPLITTING:
            _actor503500LargeChainStepSplitting(arg0);
            break;
    }
    timer            = (u16)work->slowFrames - 1;
    work->slowFrames = timer;
    if (timer < 0) {
        work->slowFrames = 0;
    }
}

/// Advances the large chain's hit cooldown, applies contacts and clears the table.
///
/// Requires initialized work and a live enemy/model. Cooldown arithmetic
/// narrows to signed 16 bits before clamping negative values to zero. Boss
/// defeat suppresses hit application; all eight contacts are cleared either way.
static void _actor503500LargeChainProcessContacts(Task* task)
{
    _Actor503500LargeChainWork* work;
    s16                         remainingCooldown;

    work = task->work;
    if (work->hitCooldown != 0) {
        remainingCooldown = work->hitCooldown - 1;
        work->hitCooldown = remainingCooldown;
        if (remainingCooldown < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        _actor503500LargeChainApplyHits(task, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

/// Updates the large chain's lighting and colour mode at its cached root position.
///
/// Requires a live enemy/model and a composed root cache in the lighting
/// query's coordinate frame. Copies XYZ in whole game units before calling
/// the resident colour updater; this handler does not compose the root.
static void _actor503500LargeChainUpdateColor(Task* task)
{
    VECTOR lightingPosition;

    lightingPosition.vx = task->extra.tmd->coords->workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &lightingPosition, 0, 0);
}

/// Keeps a Q12 share of a link's translation offset from its bind pose.
///
/// Both matrices must be live; weight is 0..4096. Products must fit signed
/// 32 bits, as must their input differences. Calculates offsets before writing
/// XYZ, rounds down and leaves
/// rotation and matrix alignment bytes intact.
static inline void _actor503500LargeChainBlendTranslation(const MATRIX* bindPose, MATRIX* pose, s32 weight)
{
    enum { ACTOR_503500_LARGE_CHAIN_BLEND_FRACTION_BITS = 12 };
    VECTOR translationOffset;

    translationOffset.vx = ((pose->t[0] - bindPose->t[0]) * weight) >> ACTOR_503500_LARGE_CHAIN_BLEND_FRACTION_BITS;
    translationOffset.vy = ((pose->t[1] - bindPose->t[1]) * weight) >> ACTOR_503500_LARGE_CHAIN_BLEND_FRACTION_BITS;
    translationOffset.vz = ((pose->t[2] - bindPose->t[2]) * weight) >> ACTOR_503500_LARGE_CHAIN_BLEND_FRACTION_BITS;
    pose->t[0]           = bindPose->t[0] + translationOffset.vx;
    pose->t[1]           = bindPose->t[1] + translationOffset.vy;
    pose->t[2]           = bindPose->t[2] + translationOffset.vz;
}

/// Blends a large chain's laid-out links back toward their saved bind pose.
///
/// Requires initialized chain work and nine live model coordinates. The Q12
/// weight is 0..4096: zero selects bind pose, 4096 leaves the laid-out pose
/// unchanged. Only links 1..8 change; root and composition stamps remain
/// intact. Rotation is blended in place through an orthonormal basis;
/// translation products must fit signed 32 bits and round down on shifting.
static void _actor503500LargeChainBlendPose(Task* task)
{
    _Actor503500LargeChainWork* work;
    GfxCoord*                   linkCoord;
    const MATRIX*               bindPose;
    s32                         blendWeight;
    s32                         linkIndex;

    work      = task->work;
    linkCoord = task->extra.tmd->coords + 1;
    if (work->blendWeight < ONE) {
        bindPose    = &work->bindPose[1];
        blendWeight = work->blendWeight;
        for (linkIndex = 1; linkIndex < ACTOR_503500_LARGE_CHAIN_PART_COUNT; linkIndex++) {
            gfxBlendOrthonormalRotation(bindPose, &linkCoord->coord, &linkCoord->coord, blendWeight);
            _actor503500LargeChainBlendTranslation(bindPose, &linkCoord->coord, blendWeight);
            bindPose++;
            linkCoord++;
        }
    }
}

#include "../../shared/bezier_curve_coefficients.inc.c"

/// Restarts a large chain in a requested state and reports its occupancy.
///
/// `task` must own the large-chain work block for slot 2 or 3; `state` is an
/// `ACTOR_503500_LARGE_CHAIN_STATE_*` value, stored as a signed halfword.
/// Clears the state's step, frame counter and the unread `field_2E5` byte,
/// consumes a pending boss command, and reports at rest only for idle.
static void _actor503500LargeChainEnterState(Task* task, s32 state)
{
    _Actor503500LargeChainWork* work = task->work;

    work->state         = state;
    work->stateStep     = 0;
    work->field_2E5     = 0;
    work->stateFrames   = 0;
    task->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
    actor503500SetSlotBusy(task->parent, task->spawnArg1.value, state != ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
}

void actor503500LargeChainTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_503500_80131F9C;
    handlers.funcs[task->state](task);
}
