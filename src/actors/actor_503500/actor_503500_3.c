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

/// HP damage multiplier for this boss and its attached targets' critical hits.
enum { ACTOR_503500_CRITICAL_HIT_DAMAGE_MULTIPLIER = 4 };

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
/// `func_actor_503500_801360A4` / `actor503500TryReserveSlotEffects` / `actor503500ReleaseSlotEffects`.
extern u16 D_actor_503500_80176D64[];
/// Main-executable globals with no module header yet: `gDisplayState.pendingMode` gates the
/// "everything is dead" message, `gPlayerStatus.hp` is the player's current HP and
/// `Gp_StateC08.mode` is 1 while the attachment wheel is open.
/// Main-executable flag byte cleared when the boss enters `ACTOR_503500_STATE_PART_LOST`; also written
/// by `mist_r18`, which has no module header for it either. Declared as an
/// array: `func_actor_503500_801345F4` needs the in-struct store, which keeps
/// the preceding `scriptedEffectTask` store ordered before it.
static s32  _actor503500CheckPartLossProgress(Task* task);
static void _actor503500ScheduleTargetable(Task* task, s8 targetable, s16 delayFrames);
static void _actor503500StepAttackState(Task* task);
static void func_actor_503500_80134408(Task* arg0);
static void func_actor_503500_801345F4(Task* arg0);
static void func_actor_503500_80134A24(Task* arg0);
static void func_actor_503500_80134C68(Task* arg0);
static s32  _actor503500IsSlotAtRest(Actor503500Work* work, s32 slot);
static s32  _actor503500AttackPositiveSideChain(Task* task, Actor503500Work* work);
static s32  _actor503500AttackNegativeSideChain(Task* task, Actor503500Work* work);
static s32  _actor503500IsSlotReady(Actor503500Work* work, s32 slot);
static void _actor503500CommandSlot(Actor503500Work* work, s32 slot, s32 command, s32 cooldownFrames);

static void func_actor_503500_80136450(Task* arg0);
static void func_actor_503500_801369E4(Task* arg0);
static void func_actor_503500_80136A80(Task* arg0);
static void _actor503500UpdateBodyCollisionGrid(Task* task, s32 initializeFaces, s32 moveAway);
static void _actor503500EnterCombatState(Task* task, s32 state);
static void func_actor_503500_801374BC(Task* arg0);
static void func_actor_503500_801398D0(Task* arg0);
static void func_actor_503500_8013A0D0(Task* arg0);
static void func_actor_503500_8013A96C(Task* arg0);
static void func_actor_503500_8013AA44(Task* arg0);
static void func_actor_503500_8013AAC0(Task* arg0);
static void func_actor_503500_801334CC(Task* arg0);
static void func_actor_503500_80135178(Task* arg0);
static void func_actor_503500_801353F0(Task* arg0);
static void func_actor_503500_80135644(Task* arg0);
static void func_actor_503500_80136280(Task* arg0);
static void func_actor_503500_80136304(Task* arg0);
static void func_actor_503500_80136A88(Task* arg0);
static void func_actor_503500_80136AEC(Task* arg0);
static void func_actor_503500_80136D30(Task* arg0);
static void func_actor_503500_80136DDC(Task* arg0);

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `func_actor_503500_80132F64`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

static void _actor503500ExitBoss(Task* task);

extern _Actor503500PinkFlashEmitterWork D_actor_503500_80176D88;

extern _Actor503500LargeChainWork D_actor_503500_80176EE8[2];

static void _actor503500LargeChainExit(Task* task);

/// Re-places a display node and its record table: `arg2` is the node's
/// `WorldCollisionContact` table and `arg3` the record count.
static void func_actor_503500_80134EAC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void _actor503500PinkFlashEmitterExit(Task* task);
static void func_actor_503500_801382F4(Task* arg0);
static void func_actor_503500_801382FC(Task* arg0);
static void func_actor_503500_80138378(Task* arg0);
static void func_actor_503500_801383D0(Task* arg0);
static void _actor503500LargeChainEnterState(Task* task, s32 state);
static void func_actor_503500_80138A30(Task* arg0);
static void func_actor_503500_80138C08(Task* arg0);
static void func_actor_503500_801395BC(Task* arg0);
static void func_actor_503500_8013A470(SVECTOR* pts, GfxCoord* coords, s32 phase);
static void func_actor_503500_80137048(Task* arg0, s32 rate);

static void func_actor_503500_80132F64(Task* arg0);
static void func_actor_503500_80133270(Task* arg0);
static void func_actor_503500_801372C8(Task* arg0);
static void func_actor_503500_8013815C(Task* arg0);
static void func_actor_503500_8013852C(Task* arg0);
static void func_actor_503500_80138898(Task* arg0);

static void func_actor_503500_80138454(Task* arg0);
static void _actor503500PinkFlashEmitterEnterState(Task* task, s32 state);
static void func_actor_503500_80139EFC(Task* arg0);
static void func_actor_503500_8013AB38(Task* arg0);

/// `Task::state` handlers `func_actor_503500_80137238` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131E44 = {
    {
        func_actor_503500_80132F64,
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
static void        func_actor_503500_801360A4(s32 arg0, s16 arg1);
static void        func_actor_503500_80137678(Task* arg0);
static void        func_actor_503500_80137C90(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);
static void        func_actor_503500_80139014(Task* arg0);
static void        func_actor_503500_80139A20(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3);

/// State-0 init of the boss: clears and seeds its work block, links the
/// second body part's display node, spawns slot enemies 1..11 from
/// `D_actor_503500_8016E924` (tinting each from the current area record, as
/// `actor503500SpawnSlotEnemy` does) and applies preset 0x7D3.
static void func_actor_503500_80132F64(Task* arg0)
{
    GameLocationKey        key;
    GameLocationKey*       sessionKey;
    u8                     areaByte0;
    AreaVariant*           layout;
    AreaPlacement*         entry;
    Enemy*                 child;
    TmdObject*             model;
    u32                    raw;
    s32                    idx;
    s32                    i;
    TmdObject*             tmd;
    Enemy*                 enemy;
    GfxCoord*              coord;
    GfxCoord*              part;
    WorldCollisionContact* recs;
    /* Kept in a register across the spawn loop: the ROM stores enemies[0]
       through the same base rather than rebuilding the address. */
    Actor503500Work* work = &D_actor_503500_80176574.work;

    tmd   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    coord = tmd->coords;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work                 = work;
    work->animationId          = -1;
    work->animationSourceIndex = -1;
    work->bufferFreeCountdown  = -1;
    work->walkSpeedLimit       = 0x80000;
    work->position.vx          = coord->coord.t[0] << 16;
    work->position.vy          = coord->coord.t[1] << 16;
    work->position.vz          = coord->coord.t[2] << 16;
    work->advancing            = 1;
    work->attackDelay          = 0x5A;
    tmd->lightMtx              = &work->lightMtx;
    tmd->colorMtx              = &work->colorMtx;
    tmd->otOffset              = 0x14;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;

    enemy->field_4                 = &coord->coord;
    part                           = &coord[3];
    enemy->field_48                = 0;
    enemy->coord                   = part;
    enemy->node.state.parts.flags |= (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    enemy->bodyPos.vx              = D_actor_503500_8016EC50.vx;
    enemy->bodyPos.vy              = D_actor_503500_8016EC50.vy;
    enemy->bodyPos.vz              = D_actor_503500_8016EC50.vz;
    recs                           = work->contacts;
    enemy->param                   = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                    = recs;
    enemy->hp                      = enemy->param->hpMax;

    work->body.coord            = part;
    work->body.context.contacts = recs;
    work->body.key              = 0x30023;
    work->body.radius           = 0x258;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.pos.vx           = D_actor_503500_8016EC50.vx;
    work->body.pos.vy           = D_actor_503500_8016EC50.vy;
    work->body.pos.vz           = D_actor_503500_8016EC50.vz;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(recs, ARRAY_SIZE(work->contacts), 0);
    work->hitEffect.spawnArgLo = 0x600;
    work->hitEffect.coord      = part;
    work->hitEffect.spawnArgHi = 3;
    work->body.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    for (i = 1; i < 12; i++) {
        child = enemySpawnFromTable(D_actor_503500_8016E924, i, i, enemy);
        if (child != NULL) {
            sessionKey = &gGameSession->location.loc;
            raw        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
            model      = child->task->extra.tmd;
            key.stage  = sessionKey->stage;
            key.area   = sessionKey->area;
            key.room   = sessionKey->room;
            areaByte0  = sessionKey->view;
            idx        = raw >> 12;
            key.view   = areaByte0;
            areaSyncLocationVariant(&key);
            layout                   = areaGetVariant(&key);
            entry                    = gpAreaPlaceAt(layout->placements, idx);
            model->texturePageOffset = entry->texturePageOffset;
            model->clutRowOffset     = entry->clutRowOffset;
            if (model->buffer != NULL) {
                tmdBuildBufferHalf(model);
                tmdBuildBufferHalf(model);
            }
            work->enemies[i] = child;
        }
    }
    work->enemies[0] = enemy;
    (sceneAcquireBattleRef)(0x23);
    _actor503500UpdateBodyCollisionGrid(arg0, 1, 0);
    for (i = 17; i >= 0; i--) {
        D_actor_503500_80176D64[i] = 0;
    }
    actor503500HandlePlayAnimation(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, D_actor_503500_8016EAC0, 0);
    arg0->exitCallback = _actor503500ExitBoss;
    arg0->msgTable     = D_actor_503500_8016EA2C;
    arg0->state       += 1;
}

/// Per-frame update. `gSceneCombatState.actorControl` 1 pauses the boss (buffers kept, only
/// `func_actor_503500_80136AEC` runs), 2 hides it; anything else runs the
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
                SndEvt_EnqueueType8(SOUND_BANK_TYPE_CHARACTER_ALL);
                tmdAllocPrimitiveBuffer(tmd);
                tmd->flags         &= (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                work->controlPaused = mode;
                work->controlHidden = 0;
            }
            func_actor_503500_80136AEC(arg0);
            return;
        case 2:
            if (work->bufferFreeCountdown >= 0) {
                if (work->bufferFreeCountdown == 0) {
                    tmdFreePrimitiveBuffer(tmd);
                }
                work->bufferFreeCountdown--;
            }
            if (work->controlHidden == 0) {
                SndEvt_EnqueueType8(SOUND_BANK_TYPE_CHARACTER_ALL);
                tmd->flags               |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                work->bufferFreeCountdown = 1;
                work->controlPaused       = 0;
                work->controlHidden       = 1;
            }
            return;
        default:
            if (work->controlPaused == 1 || work->controlHidden == 1 || work->mutedForMenu != 0) {
                SndEvt_EnqueueType9(SOUND_BANK_TYPE_CHARACTER_ALL);
                work->controlPaused = 0;
                work->controlHidden = 0;
                work->mutedForMenu  = 0;
            }
            if ((gDisplayState.pendingMode & DISPLAY_MODE_MENU_GROUP_MASK) == DISPLAY_MODE_GAME_MENU_GROUP) {
                SndEvt_EnqueueType8(SOUND_BANK_TYPE_CHARACTER_ALL);
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
                func_actor_503500_80136280(arg0);
            }
            func_actor_503500_80136A88(arg0);
            func_actor_503500_80136AEC(arg0);
            _actor503500UpdateBodyCollisionGrid(arg0, 0, 0);
            func_actor_503500_801334CC(arg0);
            func_actor_503500_80136304(arg0);
            func_actor_503500_80135178(arg0);
            func_actor_503500_801353F0(arg0);
            func_actor_503500_80136D30(arg0);
            func_actor_503500_80135644(arg0);
            func_actor_503500_80136DDC(arg0);
            break;
    }
}

/// Per-frame upkeep: ticks the `slotCooldown` slot counters down to 0 while the
/// boss is in `ACTOR_503500_STATE_IDLE`, rolls `randomRoll` from `gRandomLcgState`, stores the yaw to
/// `gPlayerStatus.coordMtx` (offset by `targetYawOffset`, wrapped into [-0x800, 0x800)) in
/// `targetYaw`, and when `targetableDelay` runs out links or unlinks the
/// boss's target per `targetablePending`.
static void func_actor_503500_801334CC(Task* arg0)
{
    Actor503500Work* work;
    GfxCoord*        coord;
    Enemy*           enemy;
    SVECTOR          vec;
    s16              angle;
    s16              count;
    s16*             p;
    s32              i;

    work = arg0->work;
    p    = work->slotCooldown;
    if (work->state == ACTOR_503500_STATE_IDLE) {
        for (i = 0; i < ARRAY_SIZE(work->slotCooldown); i++, p++) {
            if (--*p < 0) {
                *p = 0;
            }
        }
    }
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->randomRoll = gRandomLcgState >> 16;
    coord            = arg0->extra.tmd->coords;
    vec.vx           = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    vec.vy           = 0;
    vec.vz           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    angle            = work->targetYawOffset + ratan2(vec.vx, vec.vz);
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    work->targetYaw = angle;
    enemy           = arg0->spawnArg2.pointer;
    count           = --work->targetableDelay;
    if (count < 0) {
        work->targetableDelay = 0;
    } else if (count == 0) {
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
/// Uses the saved previous band and the live player Y in game coordinates.
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

/// Two-step state of the boss block. Step 0 hides the second body part,
/// unlinks the enemy node, stores the summed `hp` of occupied slots
/// 1..16 in `gGameSession->bossPartsHpSum` and plays sound 0x40230010 at the
/// part's position. Step 1 counts 0x1F frames, then posts message 0x13F4
/// under the same gates as `_actor503500CheckPartLossProgress`.
static void func_actor_503500_80134408(Task* arg0)
{
    Actor503500Work* work;
    Enemy*           enemy;
    s32              i;
    s16              sum;
    s32              pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    switch ((s8)work->stateStep) {
        case 0:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = 0;
            work->hitCooldown = 0;
            roomEffectRequestCancelAll();
            sum = 0;
            for (i = 1; i < ARRAY_SIZE(work->enemies); i++) {
                if (work->enemies[i] != NULL) {
                    sum += work->enemies[i]->hp;
                }
            }
            gGameSession->bossPartsHpSum = sum;
            work->selfAttackCommand      = ACTOR_503500_SLOT_COMMAND_NONE;
            actor503500PlayAnimationPreset(arg0, 0xE, 0x20);
            pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[3]);
            sndEvtRequestScriptStart(SOUND_BRAHMAN_DEATH_LOOP, pan,
                                     (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[3]) / 2));
            work->stateStep = work->stateStep + 1;
            break;
        case 1:
            if (++work->stateFrames >= 0x1F &&
                ((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode != GAME_ACTOR_MODE_SCRIPTED &&
                gPlayerStatus.hp > 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
                sndEvtRequestScriptStop(SOUND_BRAHMAN_DEATH_LOOP, 0x2D);
                work->stateStep = work->stateStep + 1;
            }
            break;
    }
}

/// Seven-step state of the boss block. Step 0 applies preset 0x13 and clears
/// `scriptedEffectTask`; step 1 sprays 0x60055 effects for 0x78 frames (one from the
/// frame count, one from `gRandomLcgState`), plays 0x40230012, and at frame 0x97
/// spawns the attached effect task into `scriptedEffectTask`. Step 2 waits for
/// `rootCoordRestored`, kills that task and spawns a fresh one; steps 3..6 walk
/// presets 6, 7 and 8 and finally return the boss to `ACTOR_503500_STATE_IDLE`.
static void func_actor_503500_801345F4(Task* arg0)
{
    Actor503500Work* work;
    GfxCoord*        coord;
    Task*            task;
    s32              pan;

    work = arg0->work;
    switch ((s8)work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0, 0x13, 0x10);
            actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
            work->scriptedEffectTask = NULL;
            work->stateStep          = work->stateStep + 1;
            break;
        case 1:
            if (work->stateFrames < 0x78) {
                effectSpawn(EFFECT_HIT_PUFF, &arg0->extra.tmd->coords[3], 0x01001800,
                            &D_actor_503500_8016EF58[(s16)(work->stateFrames % 7)]);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_HIT_PUFF, &arg0->extra.tmd->coords[3], 0x01001800,
                            &D_actor_503500_8016EF58[(u16)((gRandomLcgState >> 16) % 7)]);
            }
            if (work->stateFrames == 0x78) {
                sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x12), 0x3C);
            }
            if (work->stateFrames == 2) {
                coord = &arg0->extra.tmd->coords[3];
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_BRAHMAN, 0x12), pan, (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
            }
            if (++work->stateFrames >= 0x97) {
                task = taskSpawnFromTable(D_actor_503500_8016E9F0, 4, 0x64, arg0);
                if (task != NULL) {
                    coord             = task->extra.tmd->coords;
                    coord->parent     = &arg0->extra.tmd->coords[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                work->scriptedEffectTask   = task;
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_2X;
                work->stateStep            = work->stateStep + 1;
            }
            break;
        case 2:
            if (work->rootCoordRestored != 0) {
                task = work->scriptedEffectTask;
                if (task != NULL) {
                    task->exitCallback(task);
                }
                actor503500PlayAnimationPreset(arg0, 5, 0x10);
                task = taskSpawnFromTable(D_actor_503500_8016E9F0, 4, 0x5A, arg0);
                if (task != NULL) {
                    coord             = task->extra.tmd->coords;
                    coord->parent     = &arg0->extra.tmd->coords[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                _actor503500ScheduleTargetable(arg0, 1, 1);
                work->stateStep = work->stateStep + 1;
            }
            break;
        case 3:
            if (actor503500HasAnimationFinished(arg0, 5) != 0) {
                actor503500PlayAnimationPreset(arg0, 6, 0x10);
                work->stateFrames = 0;
                work->stateStep   = work->stateStep + 1;
            }
            break;
        case 4:
            if (++work->stateFrames >= 0x33) {
                actor503500PlayAnimationPreset(arg0, 7, 0);
                work->stateStep = work->stateStep + 1;
            }
            break;
        case 5:
            if (actor503500HasAnimationFinished(arg0, 7) != 0) {
                actor503500PlayAnimationPreset(arg0, 8, 0);
                _actor503500ScheduleTargetable(arg0, 0, 0xE);
                work->stateStep = work->stateStep + 1;
            }
            break;
        case 6:
            if (actor503500HasAnimationFinished(arg0, 8) != 0) {
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
                actor503500PlayAnimationPreset(arg0, 0, 0);
                work->targetYawOffset = 0;
                _actor503500EnterCombatState(arg0, ACTOR_503500_STATE_IDLE);
            }
            break;
    }
}

/// Three-step state of the boss block. Step 0 saves the coordinate's rotation
/// and seeds the Y scale to 0x1000; step 1 counts 0x1F frames, then applies
/// animation preset 0xE; step 2 restores the rotation every frame, squashes it
/// in Y down to 0x200 and fires the light / effect cues at frames 0x3C, 0x46
/// and 0x64. From step 2 on, preset 0xE is reapplied whenever
/// `actor503500HasAnimationFinished` reports it.
static void func_actor_503500_80134A24(Task* arg0)
{
    Actor503500Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32*             src;
    s32*             dst;
    s32              i;
    VECTOR           scale;

    coord = arg0->extra.tmd->coords;
    obj   = arg0->extra.tmd;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    switch ((s8)work->stateStep) {
        case 0:
            work->stateFrames    = 0;
            work->collapseScaleY = 0x1000;
            dst                  = (s32*)work->unscaledRotation.m;
            src                  = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->unscaledRotation.m[2][2] = coord->coord.m[2][2];
            work->stateStep                = work->stateStep + 1;
            break;
        case 1:
            if (++work->stateFrames >= 0x1F) {
                actor503500PlayAnimationPreset(arg0, 0xE, 0x20);
                work->stateFrames = 0;
                work->stateStep   = work->stateStep + 1;
            }
            break;
        case 2:
            if (work->collapseScaleY > 0x200) {
                work->collapseScaleY -= 0x10;
            }
            dst = (s32*)coord->coord.m;
            src = (s32*)work->unscaledRotation.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->unscaledRotation.m[2][2];
            scale.vx             = 0x1000;
            scale.vy             = work->collapseScaleY;
            scale.vz             = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            switch (++work->stateFrames) {
                case 0x3C:
                    obj->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;
                case 0x46:
                    effectSpawn(EFFECT_CORPSE_BURN, coord, 1, NULL);
                    break;
                case 0x64:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
            }
            break;
    }
    if ((s8)work->stateStep >= 2 && actor503500HasAnimationFinished(arg0, 0xE) != 0) {
        actor503500PlayAnimationPreset(arg0, 0xE, 0x20);
    }
}

static void func_actor_503500_80134C68(Task* arg0)
{
    Actor503500Work* work;
    Task*            task;
    GfxCoord*        coord;

    work = arg0->work;
    if ((u16)(work->state - ACTOR_503500_STATE_PART_LOST) < 3U) {
        _actor503500CommandSlot(work, 0, ACTOR_503500_SLOT_COMMAND_NONE, 0);
        return;
    }
    switch (work->selfAttackPhase) {
        case 0:
            actor503500PlayAnimationPreset(arg0, 4, 0x10);
            _actor503500ScheduleTargetable(arg0, 1, 0x4B);
            work->selfAttackPhase++;
            break;
        case 1:
            if (actor503500HasAnimationFinished(arg0, 4) != 0) {
                actor503500PlayAnimationPreset(arg0, 5, 0x10);
                task = taskSpawnFromTable(D_actor_503500_8016E9F0, 4, 0x5A, arg0);
                if (task != NULL) {
                    coord             = task->extra.tmd->coords;
                    coord->parent     = &arg0->extra.tmd->coords[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_2X;
                work->selfAttackFrames     = 0;
                work->selfAttackPhase++;
            }
            break;
        case 2:
            if (++work->selfAttackFrames >= 0x5B) {
                actor503500PlayAnimationPreset(arg0, 6, 0x10);
                work->selfAttackFrames = 0;
                work->selfAttackPhase++;
            }
            break;
        case 3:
            if (++work->selfAttackFrames >= 0x33) {
                actor503500PlayAnimationPreset(arg0, 7, 0);
                work->selfAttackPhase++;
            }
            break;
        case 4:
            if (actor503500HasAnimationFinished(arg0, 7) != 0) {
                actor503500PlayAnimationPreset(arg0, 8, 0);
                _actor503500ScheduleTargetable(arg0, 0, 0xE);
                work->selfAttackPhase++;
            }
            break;
        case 5:
            if (actor503500HasAnimationFinished(arg0, 8) != 0) {
                work->selfAttackCommand    = ACTOR_503500_SLOT_COMMAND_NONE;
                gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
                actor503500SetSlotBusy(arg0, 0, 0);
                actor503500PlayAnimationPreset(arg0, 0, 0);
            }
            break;
    }
}

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

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the boss. Each attack id is taken once, and only type-2 ids land while the
/// `hitCooldown` countdown is clear: the damage scales with the attacker's
/// distance, `damageRollCriticalHit` can quadruple it, and a hit that empties
/// `hp` starts the death state instead of the id's status effect. `arg1`
/// is passed by the caller but unused.
static void func_actor_503500_80134EAC(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    Actor503500Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              i;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        _actor503500HandleHit(arg0, work, enemy, coord, arg2, i);
    }
}

static void func_actor_503500_80135178(Task* arg0)
{
    SVECTOR          rot;
    Actor503500Work* work;
    GfxCoord*        coord;
    Enemy**          enemies;
    s32              speed;
    s32              turn;
    s32              diff;
    s32              absDiff;
    s16              yaw;
    s16              newYaw;

    work    = arg0->work;
    coord   = arg0->extra.tmd->coords;
    enemies = work->enemies;
    if (work->state < ACTOR_503500_STATE_PART_LOST) {
        if (work->state >= ACTOR_503500_STATE_IDLE) {
            switch (D_actor_503500_8016E8FC[work->animationId]) {
                case 1:
                    speed = 0x60000;
                    break;
                case 2:
                    speed = 0x10000;
                    break;
                default:
                    speed = 0x70000;
                    break;
            }
            turn = 0;
            if (enemies[10] == NULL) {
                turn = -(speed / 25);
            }
            if (enemies[11] == NULL) {
                turn -= speed / 25;
            }
            if (work->enemies[6] == NULL) {
                turn -= (speed * 12) / 100;
            }
            if (work->enemies[9] == NULL) {
                turn -= speed / 4;
            }
            speed  += turn;
            yaw     = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
            turn    = work->turnSpeed;
            diff    = work->targetYaw - yaw;
            absDiff = ABS(diff);
            if ((speed * 8) >> 16 >= absDiff) {
                if (turn < 0) {
                    turn += speed / 16;
                    if (turn > 0) {
                        turn = 0;
                    }
                } else {
                    turn -= speed / 16;
                    if (turn < 0) {
                        turn = 0;
                    }
                }
            } else {
                if (absDiff > 0x800) {
                    if (diff < 0) {
                        diff += 0x1000;
                    } else {
                        diff -= 0x1000;
                    }
                }
                if (diff > 0) {
                    turn += speed / 32;
                    if (turn > speed) {
                        turn = speed;
                    }
                } else {
                    turn -= speed / 32;
                    if (turn < -speed) {
                        turn = -speed;
                    }
                }
            }
            newYaw          = yaw + (turn >> 16);
            work->turnSpeed = turn;
            rot.vx          = 0;
            rot.vy          = newYaw;
            rot.vz          = 0;
            RotMatrix(&rot, &coord->coord);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->yaw           = newYaw;
        }
    }
}

/// Steps the boss along its yaw: ramps the walk speed `walkSpeed` by
/// `walkSpeedLimit` / 32 (reversed at a quarter rate when `targetYaw` is more than
/// 0x258 off `yaw`), moves `position` by it inside a fixed box and drops
/// the integer part into the coordinate. States 4, 5 and 7 (and 6 before step
/// 3) only record the previous translation in `previousTranslation`.
static void func_actor_503500_801353F0(Task* arg0)
{
    MATRIX           mat;
    Actor503500Work* work;
    GfxCoord*        coord;
    s32              step;
    s32              speed;
    s32              scale;
    long*            px;
    long*            pz;

    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    work->previousTranslation.vx = coord->coord.t[0];
    work->previousTranslation.vy = coord->coord.t[1];
    work->previousTranslation.vz = coord->coord.t[2];
    switch (work->state) {
        case ACTOR_503500_STATE_DEFEATED:
        case ACTOR_503500_STATE_HELD:
        case ACTOR_503500_STATE_COLLAPSE:
            return;
        case ACTOR_503500_STATE_SCRIPTED:
            if ((s8)work->stateStep < 3) {
                return;
            }
        case ACTOR_503500_STATE_IDLE:
            work->advancing = 1;
            break;
        default:
            work->advancing = 0;
            break;
    }
    step = work->walkSpeedLimit;
    if (ABS(work->targetYaw - work->yaw) > 0x258) {
        step = -step >> 2;
    }
    if (work->advancing != 0) {
        speed = work->walkSpeed + step / 32;
        if (speed > 0) {
            if (step < speed) {
                speed = step;
            }
        } else if (speed < -step) {
            speed = -step;
        }
    } else {
        speed = work->walkSpeed - step / 32;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->walkSpeed = speed;
    gfxSetRotIdentity(&mat);
    RotMatrixY(work->yaw, &mat);
    scale              = speed >> 12;
    work->velocity.vx  = mat.m[0][2] * scale;
    work->velocity.vz  = mat.m[2][2] * scale;
    work->position.vx += work->velocity.vx;
    work->position.vz += work->velocity.vz;
    px                 = &work->position.vx;
    pz                 = &work->position.vz;
    if (*px > 0x23280000) {
        *px = 0x23280000;
    } else if (*px < 0x1B580000) {
        *px = 0x1B580000;
    }
    if (*pz > 0x1F400000) {
        *pz = 0x1F400000;
    } else if (*pz < 0x17700000) {
        *pz = 0x17700000;
    }
    coord->coord.t[0]   = *px >> 16;
    coord->coord.t[2]   = *pz >> 16;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Clears each slot enemy's not-lockable flag only when its
/// `D_actor_503500_8016E910` entry covers both the camera's yaw sector
/// (relative to `yaw`) and its height band and `gGameSession->eventState`
/// is 0; otherwise sets it. Keep-scanned is set on a height-only miss and
/// cleared when the yaw sector or the event state excludes the slot.
static void func_actor_503500_80135644(Task* arg0)
{
    Actor503500Work* work;
    GfxCoord*        coord;
    Enemy*           enemy;
    s16              angle;
    s32              y;
    s16              dirMask;
    s16              heightMask;
    s32              i;
    s16              bits;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    angle = ratan2(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0], gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]) - work->yaw;
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    dirMask = 0x10;
    if (ABS(angle) < 0x400) {
        dirMask = 8;
    }
    if (angle > 0x300 && angle < 0x480) {
        dirMask = 0x20;
    } else if (angle < -0x300 && angle > -0x480) {
        dirMask = 0x40;
    }
    y          = gPlayerStatus.coordMtx->t[1];
    heightMask = 4;
    if (y < -999) {
        heightMask = 2;
        if (y < -3000) {
            heightMask = 1;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->enemies); i++) {
        enemy = work->enemies[i];
        if (enemy != NULL) {
            bits = D_actor_503500_8016E910[i];
            if ((bits & dirMask) != dirMask || gGameSession->eventState != 0) {
                enemy->node.state.parts.flags            |= WORLD_TARGET_NOT_LOCKABLE;
                work->enemies[i]->node.state.parts.flags &= ~WORLD_TARGET_KEEP_SCANNED;
            } else if ((bits & heightMask) != heightMask) {
                enemy->node.state.parts.flags            |= WORLD_TARGET_NOT_LOCKABLE;
                work->enemies[i]->node.state.parts.flags |= WORLD_TARGET_KEEP_SCANNED;
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

/// Boss message handler. Modes 0/1/2 enter `ACTOR_503500_STATE_IDLE`, `_HELD`
/// and `_COLLAPSE`, mode 3 advances the task state, mode 4 saves model part
/// 0's coordinate and `yaw` before entering `ACTOR_503500_STATE_SCRIPTED`, and
/// mode 5 restores both.
s32 func_actor_503500_80135B74(Task* arg0, s32 arg1, ActorCommand* msg, s32 arg3)
{
    Actor503500Work* work;
    GfxCoord*        coord;

    switch (msg->command) {
        case 0:
            _actor503500EnterBossState(arg0, ACTOR_503500_STATE_IDLE);
            break;
        case 1:
            _actor503500EnterBossState(arg0, ACTOR_503500_STATE_HELD);
            break;
        case 2:
            _actor503500EnterBossState(arg0, ACTOR_503500_STATE_COLLAPSE);
            break;
        case 3:
            arg0->state++;
            break;
        case 4:
            work                 = arg0->work;
            coord                = arg0->extra.tmd->coords;
            work->savedYaw       = work->yaw;
            work->savedRootCoord = *coord;
            coord->composeStamp  = GRAPHICS_COORD_DIRTY;
            _actor503500EnterBossState(arg0, ACTOR_503500_STATE_SCRIPTED);
            break;
        case 5:
            work                    = arg0->work;
            coord                   = arg0->extra.tmd->coords;
            work->yaw               = work->savedYaw;
            *coord                  = work->savedRootCoord;
            coord->composeStamp     = GRAPHICS_COORD_DIRTY;
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

static void func_actor_503500_801360A4(s32 arg0, s16 arg1)
{
    D_actor_503500_80176D64[arg0] = arg1;
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

static void func_actor_503500_80136280(Task* arg0)
{
    Actor503500Work* work;
    Enemy*           enemy;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        _actor503500EnterCombatState(arg0, ACTOR_503500_STATE_STUNNED);
        work->stunAnimationTimer = 3;
    }
    flags = enemy->reactionFlags;
    if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

static void func_actor_503500_80136304(Task* arg0)
{
    Actor503500Work* work = arg0->work;
    u16              timer;

    switch (work->state) {
        case ACTOR_503500_STATE_IDLE:
            if (_actor503500CheckPartLossProgress(arg0) == 0) {
                func_actor_503500_80136450(arg0);
            }
            break;
        case ACTOR_503500_STATE_ATTACK:
            _actor503500StepAttackState(arg0);
            break;
        case ACTOR_503500_STATE_PART_LOST:
            func_actor_503500_801369E4(arg0);
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
            func_actor_503500_80134408(arg0);
            break;
        case ACTOR_503500_STATE_HELD:
            func_actor_503500_80136A80(arg0);
            break;
        case ACTOR_503500_STATE_SCRIPTED:
            func_actor_503500_801345F4(arg0);
            break;
        case ACTOR_503500_STATE_COLLAPSE:
            func_actor_503500_80134A24(arg0);
            break;
    }
    if (work->selfAttackCommand != 0) {
        func_actor_503500_80134C68(arg0);
    }
}

static void func_actor_503500_80136450(Task* arg0)
{
    Actor503500Work* work = arg0->work;
    u16              timer;

    if ((s8)work->stateStep == 0 && work->animationId >= 2) {
        actor503500PlayAnimationPreset(arg0, 1, 0x10);
    }
    timer             = work->attackDelay - 1;
    work->attackDelay = timer;
    if ((s16)timer < 0) {
        _actor503500EnterCombatState(arg0, ACTOR_503500_STATE_ATTACK);
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

static void func_actor_503500_801369E4(Task* arg0)
{
    Actor503500Work* work;

    work = arg0->work;
    switch ((s8)work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0, 0xE, 0x10);
            actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
            work->stateStep = work->stateStep + 1;
            break;
        case 1:
            if (actor503500HasAnimationFinished(arg0, 0xE) != 0) {
                work->targetYawOffset = 0;
                _actor503500EnterCombatState(arg0, ACTOR_503500_STATE_IDLE);
            }
            break;
    }
}

static void func_actor_503500_80136A80(Task* arg0)
{
}

/// Ticks the boss's second-body-part countdown down to zero, then re-places
/// that part's display node and its 8-record collision table.
static void func_actor_503500_80136A88(Task* arg0)
{
    Actor503500Work*       work = arg0->work;
    WorldCollisionContact* rec;

    if (work->hitCooldown != 0) {
        work->hitCooldown -= 1;
        if (work->hitCooldown < 0) {
            work->hitCooldown = 0;
        }
    }

    rec = work->contacts;
    func_actor_503500_80134EAC(arg0, &work->body, rec, ARRAY_SIZE(work->contacts));
    worldCollisionClearContacts(rec);
}

/// Copies the actor's attach-coordinate world position into a stack `VECTOR`
/// and hands it to `worldCoordUpdateActorColor` with zero for the unused arguments.
static void func_actor_503500_80136AEC(Task* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra.tmd->coords->workm.t[0];
    vec.vy = arg0->extra.tmd->coords->workm.t[1];
    vec.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
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

/// Per-frame animation tick of the boss block. While the slot array is seeded
/// (`animationStarted`), every slot 1..0x13 is ticked until the first of them reports
/// `ANIMATION_SLOT_SETTLED`; once it holds the clip's boundary pose the clip
/// has finished, and in `ACTOR_503500_STATE_IDLE` the boss resets the slot rates and re-applies
/// preset `D_actor_503500_8016EAD4`.
static void func_actor_503500_80136D30(Task* arg0)
{
    Actor503500Work* work;
    s32              i;

    work = arg0->work;
    if (work->animationStarted != 0) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            if (work->state == ACTOR_503500_STATE_IDLE) {
                func_actor_503500_80137048(arg0, 0);
                actor503500HandlePlayAnimation(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_503500_8016EAD4, 0);
            }
        } else {
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationTickSlot(&work->rig.anim, i);
            }
        }
    }
}

/// Re-applies the boss's per-part scales: for each enabled bit of `scaledParts`,
/// refreshes the private copy of model part 4 or 10 and scales it, and for
/// 0x10000 scales model part 16 in place.
static void func_actor_503500_80136DDC(Task* arg0)
{
    Actor503500Work* work;

    work = arg0->work;
    if (work->scaledParts & 0x20) {
        work->part5Parent = arg0->extra.tmd->coords[4];
        ScaleMatrix(&work->part5Parent.coord, &work->part5Scale);
    }
    if (work->scaledParts & 0x800) {
        work->part11Parent = arg0->extra.tmd->coords[10];
        ScaleMatrix(&work->part11Parent.coord, &work->part11Scale);
    }
    if (work->scaledParts & 0x10000) {
        ScaleMatrix(&arg0->extra.tmd->coords[16].coord, &work->part16Scale);
    }
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

/// Sets `AnimationSlot.rate` -- the per-slot value `animationResetSlot` seeds
/// with 0x10 -- on animation slots 1..16 of the boss block, `rate` of 0
/// meaning that default.
static void func_actor_503500_80137048(Task* arg0, s32 rate)
{
    Actor503500Work* work;
    AnimationSlot*   slot;
    s32              i;

    work = arg0->work;
    slot = &work->rig.slots[1];
    if (rate == 0) {
        rate = ANIMATION_RATE_ONE;
    }
    for (i = 0xF; i >= 0; i--) {
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

/// Places the boss's part at `args`: drops the translation into the root
/// coordinate's local matrix, stores the Euler angles in the coordinate's own
/// `rot` slot and rebuilds the rotation from them, exactly as
/// `actorMsgPlaceEuler` does. It then keeps two derived copies in the work
/// block -- the yaw recovered from the matrix it just built, and the same
/// translation in 16.16 fixed point. Clearing `composeStamp` makes `actorRenderComposeCoordChain`
/// recompute the composed matrix from the new local one.
s32 func_actor_503500_80137088(Task* arg0, s32 arg1, ActorTransform* args, s32 arg3)
{
    Actor503500Work* work;
    GfxCoord*        coord;

    work                = arg0->work;
    coord               = arg0->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->yaw           = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    work->position.vx   = args->pos.vx << 16;
    work->position.vy   = args->pos.vy << 16;
    work->position.vz   = args->pos.vz << 16;
    return 0;
}

s32 func_actor_503500_80137158(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject* ext;
    s32        ret;

    ext = arg0->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            ext->flags = (ext->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(ext);
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            ext->flags                                         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Actor503500Work*)arg0->work)->bufferFreeCountdown = mode;
            ext->flags                                         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            ext->flags = (ext->flags & ~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

void func_actor_503500_80137238(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131E44;
    sp.funcs[task->state](task);
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

/// `Task::state` handlers `func_actor_503500_801384D4` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131F4C = {
    {
        func_actor_503500_801372C8,
        func_actor_503500_8013815C,
        _actor503500PinkFlashEmitterExit,
    },
};

/// State-0 init of the pink-flash emitter, built like
/// `func_actor_503500_8013BEE4`: clears the work block, hangs the task's
/// coordinate off part 8 of the parent's model, republishes the parent's light
/// and colour matrices, links the enemy node and the block's target sphere,
/// and starts the emitter in `ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE`.
static void func_actor_503500_801372C8(Task* arg0)
{
    Enemy*                 enemy;
    Task*                  parent;
    TmdObject*             tmd;
    TmdObject*             parentTmd;
    GfxCoord*              coord;
    WorldCollisionContact* rec;

    enemy     = arg0->spawnArg2.pointer;
    tmd       = arg0->extra.tmd;
    parent    = arg0->parent;
    coord     = tmd->coords;
    parentTmd = parent->extra.tmd;
    memFillBytes(&D_actor_503500_80176D88, 0, sizeof(D_actor_503500_80176D88));
    arg0->work = &D_actor_503500_80176D88;

    coord->parent       = &parent->extra.tmd->coords[8];
    coord->coord.t[0]   = D_actor_503500_8016F060.vx;
    coord->coord.t[1]   = D_actor_503500_8016F060.vy;
    coord->coord.t[2]   = D_actor_503500_8016F060.vz;
    tmd->lightMtx       = parentTmd->lightMtx;
    tmd->colorMtx       = parentTmd->colorMtx;
    tmd->otOffset       = 0x13;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    D_actor_503500_80176D88.bufferFreeCountdown = -1;
    enemy->field_4                              = &coord->coord;
    enemy->field_48                             = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F068.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F068.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F068.vz;
    rec                           = D_actor_503500_80176D88.contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                   = rec;
    enemy->hp                     = enemy->param->hpMax;

    D_actor_503500_80176D88.body.coord            = coord;
    D_actor_503500_80176D88.body.context.contacts = rec;
    D_actor_503500_80176D88.body.key              = 0x30023;
    D_actor_503500_80176D88.body.radius           = 0x320;
    D_actor_503500_80176D88.body.flags            = WORLD_COLLISION_BODY_SPHERE;
    D_actor_503500_80176D88.body.pos.vx           = D_actor_503500_8016F068.vx;
    D_actor_503500_80176D88.body.pos.vy           = D_actor_503500_8016F068.vy;
    D_actor_503500_80176D88.body.pos.vz           = D_actor_503500_8016F068.vz;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_503500_80176D88.body);
    worldCollisionInitContacts(rec, ARRAY_SIZE(D_actor_503500_80176D88.contacts), 0);
    D_actor_503500_80176D88.hitEffect.spawnArgLo = 0x400;
    D_actor_503500_80176D88.hitEffect.coord      = coord;
    D_actor_503500_80176D88.hitEffect.spawnArgHi = 3;
    D_actor_503500_80176D88.body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    _actor503500PinkFlashEmitterEnterState(arg0, ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
    arg0->exitCallback = _actor503500PinkFlashEmitterExit;
    arg0->state       += 1;
}

/// `ACTOR_503500_PINK_FLASH_EMITTER_STATE_ATTACK` step of the pink-flash
/// emitter: step 0 starts animation 0xF on the parent, step 1 spawns the two
/// pink-flash attack tasks hung off this task's coordinate, step 2 waits on
/// the parent's animation 0xF and starts 0x10, step 3 counts
/// `ACTOR_503500_PINK_FLASH_EMITTER_ATTACK_RECOVERY_FRAMES`. Leaves early once
/// `_8013608C` reports the parent recoiling, stunned or defeated.
static void func_actor_503500_801374BC(Task* arg0)
{
    _Actor503500PinkFlashEmitterWork* work;
    Task*                             task;
    GfxCoord*                         coord;
    s32                               i;

    work = arg0->work;
    if (actor503500ShouldInterruptAttack(arg0->parent) != 0) {
        _actor503500PinkFlashEmitterEnterState(arg0, ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
        actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            actor503500PlayAnimationPreset(arg0->parent, 0xF, 0x10);
            work->stateStep++;
            break;
        case 1:
            if (++work->stateFrames > 0) {
                for (i = 0; i < 2; i++) {
                    task = taskSpawnFromTable(D_actor_503500_8016E9F0, 2, i, 0);
                    if (task != NULL) {
                        coord             = task->extra.tmd->coords;
                        coord->parent     = arg0->extra.tmd->coords;
                        coord->coord.t[0] = D_actor_503500_8016F070.vx;
                        coord->coord.t[1] = D_actor_503500_8016F070.vy;
                        coord->coord.t[2] = D_actor_503500_8016F070.vz;
                        taskReparent(arg0, task);
                    }
                }
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 2:
            if (actor503500HasAnimationFinished(arg0->parent, 0xF) != 0) {
                actor503500PlayAnimationPreset(arg0->parent, 0x10, 0x10);
                work->stateStep++;
            }
            break;
        case 3:
            if (++work->stateFrames >= ACTOR_503500_PINK_FLASH_EMITTER_ATTACK_RECOVERY_FRAMES) {
                _actor503500PinkFlashEmitterEnterState(arg0, ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE);
            }
            break;
    }
}

/// `ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING` step of the pink-flash
/// emitter: step 0 unlinks the enemy node, empties the slot, sends the parent
/// into its part-lost recoil and clears the 16.16 `spin` / `velocity` /
/// `positionCarry`; step 2 steps `spin.fixed.vx` down for
/// `ACTOR_503500_PINK_FLASH_EMITTER_DYING_TIP_FRAMES`, then re-parents the
/// coordinate onto the view in world space, points `velocity` along its Z
/// axis, spawns slot 0xC on the parent and plays `SOUND_BRAHMAN_PART_DEATH`;
/// steps 3/4 accelerate `velocity.fixed.vy`, and step 4 fires the light and
/// sound cues on its fade and blacken frames and leaves on its end frame.
/// Every frame the spin and the travel are applied to the coordinate, and
/// every even frame sprays a smoke puff from `D_actor_503500_8016F078`.
static void func_actor_503500_80137678(Task* arg0)
{
    SVECTOR                           rot;
    MATRIX                            m;
    _Actor503500PinkFlashEmitterWork* work;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    s32*                              src;
    s32*                              out;
    s32                               i;

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
            work->stateStep++;
            break;
        case 1:
            work->stateStep++;
            break;
        case 2:
            work->spin.fixed.vx.word -= 0x4000;
            if (++work->stateFrames >= ACTOR_503500_PINK_FLASH_EMITTER_DYING_TIP_FRAMES) {
                // Copy the nine coefficients as four words and a halfword; preserve the alignment halfword.
                src   = (s32*)&m;
                coord = arg0->extra.tmd->coords;
                gfxComposeNodeWorldTransform(coord, &m, &rot);
                out = (s32*)&coord->coord;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                coord->coord.m[2][2]         = m.m[2][2];
                coord->coord.t[0]            = rot.vx;
                coord->coord.t[1]            = rot.vy;
                coord->coord.t[2]            = rot.vz;
                coord->parent                = &gGfxViewCoord;
                work->velocity.fixed.vx.word = 0;
                work->velocity.fixed.vy.word = 0;
                work->velocity.fixed.vz.word = 0x100000;
                ApplyMatrixLV(&m, &work->velocity.vector, &work->velocity.vector);
                actor503500SpawnSlotEnemy(arg0->parent, ACTOR_503500_SLOT_YELLOW_FLASH_EMITTER);
                actorRenderComposeCoord(coord);
                sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 3:
            work->velocity.fixed.vy.word += 0x8000;
            if (++work->stateFrames >= ACTOR_503500_PINK_FLASH_EMITTER_DYING_FALL_FRAMES) {
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 4:
            work->velocity.fixed.vy.word += 0x8000;
            switch (work->stateFrames) {
                case ACTOR_503500_PINK_FLASH_EMITTER_DYING_FADE_FRAME:
                    arg0->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    sndEvtRequestScriptStart(SOUND_COMMON(0x0D), (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                    break;
                case ACTOR_503500_PINK_FLASH_EMITTER_DYING_BLACKEN_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_503500_PINK_FLASH_EMITTER_DYING_END_FRAME:
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
        switch (gDisplayState.animFrame % 6) {
            case 0:
            case 2:
            case 4:
                effectSpawn(EFFECT_SMOKE_PUFF, coord, 0xB0008600,
                            &D_actor_503500_8016F078[work->smokePuffCount++ % 3]);
                break;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep >= 3) {
        sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        arg0->state = 2;
    }
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

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy, like `func_actor_503500_8013EE5C`: each attack id is taken once,
/// only type-2 ids land while the `hitCooldown` countdown is clear, and a hit
/// that empties `hp` starts `ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING` but
/// still applies the id's status effect. The hit effect is pulled to 800 units along the contact offset.
/// `arg1` is passed by the caller but unused.
static void func_actor_503500_80137C90(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    _Actor503500PinkFlashEmitterWork* work;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    s32                               i;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < arg3; i++) {
        _actor503500PinkFlashEmitterHandleHit(arg0, work, enemy, coord, arg2, i);
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
                func_actor_503500_801382F4(arg0);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                func_actor_503500_80138378(arg0);
            }
            func_actor_503500_801382F4(arg0);
            func_actor_503500_801382FC(arg0);
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

static void func_actor_503500_801382F4(Task* arg0)
{
}

/// Steps the pink-flash emitter's `hitCooldown` down to zero, then, unless the
/// global freeze is on, applies the hits in the target sphere's contact table
/// before releasing the table.
static void func_actor_503500_801382FC(Task* arg0)
{
    _Actor503500PinkFlashEmitterWork* work;
    s16                               timer;

    work = arg0->work;
    if (work->hitCooldown != 0) {
        timer             = work->hitCooldown - 1;
        work->hitCooldown = timer;
        if (timer < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        func_actor_503500_80137C90(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

static void func_actor_503500_80138378(Task* arg0)
{
    Enemy* obj;
    u8     flags;
    u8     flags2;

    obj   = arg0->spawnArg2.pointer;
    flags = obj->reactionFlags;
    if (flags & ENEMY_REACTION_STAGGER) {
        obj->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (obj->reactionFlags & ENEMY_REACTION_BUILDUP) {
        obj->reactionFlags = obj->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
    }
    flags2 = obj->reactionFlags;
    if (flags2 & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        obj->reactionFlags = flags2 & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

static void func_actor_503500_801383D0(Task* arg0)
{
    switch (((_Actor503500PinkFlashEmitterWork*)arg0->work)->state) {
        case ACTOR_503500_PINK_FLASH_EMITTER_STATE_IDLE:
            func_actor_503500_80138454(arg0);
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_STATE_ATTACK:
            func_actor_503500_801374BC(arg0);
            break;
        case ACTOR_503500_PINK_FLASH_EMITTER_STATE_DYING:
            func_actor_503500_80137678(arg0);
            break;
    }
}

static void func_actor_503500_80138454(Task* arg0)
{
    if (arg0->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500PinkFlashEmitterEnterState(arg0, ACTOR_503500_PINK_FLASH_EMITTER_STATE_ATTACK);
        arg0->killCountdown = ACTOR_503500_SLOT_COMMAND_NONE;
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

void func_actor_503500_801384D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131F4C;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013AD0C` dispatches through.
static const TaskFuncTable3 D_actor_503500_80131F9C = {
    {
        func_actor_503500_8013852C,
        func_actor_503500_80138898,
        _actor503500LargeChainExit,
    },
};

/// Set-up of a large chain, the enemy in slot 2 or 3: clears the slot's
/// `_Actor503500LargeChainWork` in `D_actor_503500_80176EE8`, hangs the root
/// off part 1 of the boss's model at the slot's offset and rotation, saves
/// parts 1 to 8 as `bindPose`, publishes the block's light and colour
/// matrices, and makes the tip a target with full health, its `tipPosition`
/// and `tipTarget` at the slot's rest offset.
static void func_actor_503500_8013852C(Task* arg0)
{
    Enemy*                      enemy;
    TmdObject*                  tmd;
    GfxCoord*                   coord;
    GfxCoord*                   part;
    _Actor503500LargeChainWork* work;
    WorldCollisionContact*      rec;
    MATRIX                      m;
    s32                         idx;
    s32                         i;

    idx   = arg0->spawnArg1.value - 2;
    enemy = arg0->spawnArg2.pointer;
    work  = &D_actor_503500_80176EE8[idx];
    coord = arg0->extra.tmd->coords;
    tmd   = arg0->extra.tmd;
    memFillBytes(work, 0, sizeof(*work));
    arg0->work = work;

    coord->parent     = &arg0->parent->extra.tmd->coords[1];
    part              = &coord[ACTOR_503500_LARGE_CHAIN_TIP_PART];
    coord->coord.t[0] = D_actor_503500_8016F090[idx].vx;
    coord->coord.t[1] = D_actor_503500_8016F090[idx].vy;
    coord->coord.t[2] = D_actor_503500_8016F090[idx].vz;
    gfxSetRotIdentity(&m);
    RotMatrix(&D_actor_503500_8016F0A0[idx], &m);
    MulMatrix0(&coord->coord, &m, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    for (i = 1; i < ACTOR_503500_LARGE_CHAIN_PART_COUNT; i++) {
        work->bindPose[i] = coord[i].coord;
    }
    work->blendWeight   = 0x1000;
    tmd->colorMtx       = &work->colorMtx;
    tmd->otOffset       = 0x12;
    tmd->lightMtx       = &work->lightMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    work->bufferFreeCountdown = -1;
    enemy->field_4            = &coord->coord;
    enemy->field_48           = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = part;
    enemy->node.state.parts.flags = (enemy->node.state.parts.flags | WORLD_TARGET_HIDE_HP) & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    enemy->bodyPos.vx             = D_actor_503500_8016F0B0.vx;
    enemy->bodyPos.vy             = D_actor_503500_8016F0B0.vy;
    enemy->bodyPos.vz             = D_actor_503500_8016F0B0.vz;
    rec                           = work->contacts;
    enemy->param                  = &D_actor_503500_8016E7EC[arg0->spawnArg1.value];
    enemy->recs                   = rec;
    enemy->hp                     = enemy->param->hpMax;

    work->body.coord            = part;
    work->body.context.contacts = rec;
    work->body.pos.vx           = D_actor_503500_8016F0B0.vx;
    work->body.pos.vy           = D_actor_503500_8016F0B0.vy;
    work->body.pos.vz           = D_actor_503500_8016F0B0.vz;
    work->body.key              = 0x30023;
    work->body.radius           = 0x320;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(rec, ARRAY_SIZE(work->contacts), 0);
    work->hitEffect.spawnArgLo = 0x600;
    work->hitEffect.coord      = part;
    work->hitEffect.spawnArgHi = 3;
    work->body.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->tipTarget.vx         = D_actor_503500_8016F0A8[arg0->spawnArg1.value].vx;
    work->tipTarget.vy         = D_actor_503500_8016F0A8[arg0->spawnArg1.value].vy;
    work->tipTarget.vz         = D_actor_503500_8016F0A8[arg0->spawnArg1.value].vz;
    work->tipPosition.vx       = D_actor_503500_8016F0A8[arg0->spawnArg1.value].vx;
    work->tipPosition.vy       = D_actor_503500_8016F0A8[arg0->spawnArg1.value].vy;
    work->tipPosition.vz       = D_actor_503500_8016F0A8[arg0->spawnArg1.value].vz;
    work->tipSpeedLimit.word   = 0x800000;
    work->tipAdvancing         = 1;
    arg0->exitCallback         = _actor503500LargeChainExit;
    arg0->state               += 1;
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
                func_actor_503500_8013AAC0(arg0);
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags                    |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags |= WORLD_TARGET_NOT_LOCKABLE;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                func_actor_503500_801398D0(arg0);
            }
            func_actor_503500_8013AA44(arg0);
            func_actor_503500_8013A96C(arg0);
            if (work->detached == 0) {
                func_actor_503500_80139EFC(arg0);
                func_actor_503500_8013A0D0(arg0);
            }
            func_actor_503500_8013AAC0(arg0);
            func_actor_503500_8013AB38(arg0);
            break;
    }
}

/// Idle step of a large chain: puts `tipTarget` on the slot's rest offset
/// once, starts the split when health is under half or a shot when the boss
/// commands one, and otherwise circles `tipTarget` around the rest offset:
/// `D_actor_503500_8016F0C8` turned by `tipOrbitAngles`, which then advance.
static void func_actor_503500_80138A30(Task* arg0)
{
    _Actor503500LargeChainWork* work;
    MATRIX                      m;
    SVECTOR                     v;
    s32                         idx;
    s16                         hp;

    work = arg0->work;
    idx  = arg0->spawnArg1.value - 2;
    if (work->stateStep == 0) {
        work->tipTarget.vx = D_actor_503500_8016F0B8[idx].vx;
        work->tipTarget.vy = D_actor_503500_8016F0B8[idx].vy;
        work->tipTarget.vz = D_actor_503500_8016F0B8[idx].vz;
        work->stateStep++;
    }
    hp = ((Enemy*)arg0->spawnArg2.pointer)->hp;
    if (hp < (D_actor_503500_8016E7EC[arg0->spawnArg1.value].hpMax >> 1) && hp > 0) {
        _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_SPLITTING);
        return;
    }
    if (arg0->killCountdown == ACTOR_503500_SLOT_COMMAND_ATTACK) {
        _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_SHOOT);
        return;
    }
    gfxSetRotIdentity(&m);
    RotMatrix(&work->tipOrbitAngles, &m);
    gte_SetRotMatrix(&m);
    gte_ldv0(&D_actor_503500_8016F0C8);
    gte_rtv0();
    gte_stsv(&v);
    work->tipTarget.vx       = D_actor_503500_8016F0B8[idx].vx + v.vx;
    work->tipTarget.vy       = D_actor_503500_8016F0B8[idx].vy + v.vy;
    work->tipTarget.vz       = D_actor_503500_8016F0B8[idx].vz + v.vz;
    work->tipOrbitAngles.vx += 0x20;
    work->tipOrbitAngles.vy += 0x40;
    work->tipOrbitAngles.vz += 0x80;
}

/// Shot step of a large chain. It asks the boss for animation preset 0x11,
/// then keeps `tipTarget` 1000 units above the player, in the frame of the
/// boss part the root hangs from, until `tipArrived`; ninety frames without
/// arriving end the shot. Once `stateFrames`, which that wait has been
/// counting, passes ten, and if the tip's `linkPoints` entry is within 3000 of
/// the player on the ground plane, it spawns entry 0 of
/// `D_actor_503500_8016E9F0` 0x640 along the tip's Z axis, launched at 3000
/// times that distance in 16.16. Eleven frames later the chain idles again. A
/// boss that has lost a part, is stunned or is defeated ends the shot at once.
static void func_actor_503500_80138C08(Task* arg0)
{
    SVECTOR                     pos;
    SVECTOR                     ofs;
    MATRIX                      m;
    MATRIX                      rot;
    MATRIX*                     mat;
    _Actor503500LargeChainWork* work;
    GfxCoord*                   coord;
    GfxCoord*                   dst;
    Task*                       task;
    s32*                        src;
    s32*                        out;
    s32                         dist;
    s32                         i;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (actor503500ShouldInterruptAttack(arg0->parent) != 0) {
        _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
        actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
        return;
    }
    switch (work->stateStep) {
        case 0:
            work->tipArrived = 0;
            actor503500PlayAnimationPreset(arg0->parent, 0x11, 0x10);
            work->stateStep++;
        case 1:
            if (work->tipArrived != 0) {
                work->stateStep++;
                return;
            }
            if (++work->stateFrames >= 0x5B) {
                _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
                return;
            }
            mat = &m;
            gfxComposeNodeWorldTransform(coord->parent, mat, &ofs);
            pos.vx = gPlayerStatus.coordMtx->t[0] - ofs.vx;
            pos.vy = gPlayerStatus.coordMtx->t[1] - ofs.vy - 1000;
            pos.vz = gPlayerStatus.coordMtx->t[2] - ofs.vz;
            gte_TransposeMatrix(mat, &rot);
            gte_SetRotMatrix(&rot);
            gte_ldv0(&pos);
            gte_rtv0();
            gte_stsv(&work->tipTarget);
            break;
        case 2:
            if (++work->stateFrames >= 0xB) {
                pos.vx = gPlayerStatus.coordMtx->t[0] - work->linkPoints[ACTOR_503500_LARGE_CHAIN_TIP_PART].vx;
                pos.vz = gPlayerStatus.coordMtx->t[2] - work->linkPoints[ACTOR_503500_LARGE_CHAIN_TIP_PART].vz;
                dist   = SquareRoot0(pos.vx * pos.vx + pos.vz * pos.vz);
                if (dist < 3000) {
                    task = taskSpawnFromTable(D_actor_503500_8016E9F0, 0, 0, dist * 3000);
                    if (task != NULL) {
                        gfxComposeNodeWorldTransform(&coord[ACTOR_503500_LARGE_CHAIN_TIP_PART], &m, &pos);
                        src    = (s32*)&m;
                        dst    = task->extra.tmd->coords;
                        ofs.vx = 0;
                        ofs.vy = 0;
                        ofs.vz = 0x640;
                        gte_SetRotMatrix(src);
                        gte_ldv0(&ofs);
                        gte_rtv0();
                        gte_stsv(&ofs);
                        dst->coord.t[0] = pos.vx + ofs.vx;
                        dst->coord.t[1] = pos.vy + ofs.vy;
                        dst->coord.t[2] = pos.vz + ofs.vz;
                        out             = (s32*)&dst->coord;
                        for (i = 0; i < 4; i++) {
                            *out++ = *src++;
                        }
                        dst->coord.m[2][2] = m.m[2][2];
                    }
                }
                work->stateFrames = 0;
                work->stateStep++;
            }
            break;
        case 3:
            if (++work->stateFrames >= 0xB) {
                _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            }
            break;
    }
}

/// Dying step of a large chain: stops it being a target, empties its slot,
/// sends `tipTarget` down to Y 5000 at the lower `tipSpeedLimit` and waits for
/// `tipArrived`, then re-parents the root onto the view where it stands, sets
/// `detached` and plays the part's death sound. Step 2 eases every link's
/// Euler angles to zero while the root sinks by 10 a frame; past Y 1000 the
/// root is saved in `unscaledRootMatrix` and step 3 squashes it by
/// `collapseScaleY`, firing the light, sound and effect cues on frames
/// 10/15/30 and leaving on frame 40. Every twelfth frame of steps 0 to 2
/// sprays effects along parts 8 to 1.
static void func_actor_503500_80139014(Task* arg0)
{
    MATRIX                      m;
    VECTOR                      scale;
    SVECTOR                     rot;
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    GfxCoord*                   coord;
    GfxCoord*                   part;
    s16*                        p;
    s32                         phase;
    s32                         i;
    s32                         j;

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
            enemy->reactionFlags    &= ENEMY_REACTION_LOW_CLEAR;
            work->tipTarget.vy       = 0x1388;
            work->tipSpeedLimit.word = 0x300000;
            work->tipArrived         = 0;
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
                work->detached      = 1;
                actorRenderComposeCoord(coord);
                sndEvtRequestScriptStart(SOUND_BRAHMAN_PART_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)(worldCoordGetOriginAudioDepth(coord) / 2));
                work->stateStep++;
            }
            break;
        case 2:
            for (i = 1; i < ACTOR_503500_LARGE_CHAIN_PART_COUNT; i++) {
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
        for (i = ACTOR_503500_LARGE_CHAIN_TIP_PART, j = 0; i > 0; i--) {
            effectSpawn(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[i], 0xB0008600, &D_actor_503500_8016F0D0[j]);
            j++;
            j = (j < 3) ? j : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->stateStep > 0) {
        sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        arg0->state = 2;
    }
}

/// Splitting step of a large chain: stops it being a target and lowers
/// `blendWeight` to 0, folding the model back into `bindPose`; spawns a pair
/// of puffs on the model parts for 26 frames; then spawns the lunging chains
/// of slots 0xD/0xE (slot 2) or 0xF/0x10, each commanded to unfold with half
/// this enemy's `hp`, empties its own slot and hides the model. The task
/// state advances 91 frames later.
static void func_actor_503500_801395BC(Task* arg0)
{
    SVECTOR                     vec;
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    Enemy*                      child;
    GfxCoord*                   coord;
    s32                         phase;
    s32                         a;
    s32                         b;

    work  = arg0->work;
    phase = work->stateStep;
    enemy = arg0->spawnArg2.pointer;
    switch (phase) {
        case 0:
            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldTargetUnlinkNode(&enemy->node);
            enemy->recs       = 0;
            work->hitCooldown = 0;
            work->stateStep++;
        case 1:
            work->blendWeight -= 0x20;
            if (work->blendWeight <= 0) {
                work->blendWeight = 0;
                work->field_248   = NULL;
                work->stateStep++;
            }
            break;
        case 2:
            if (actor503500TryReserveSlotEffects(arg0->spawnArg1.value, 4) != 0) {
                coord  = &arg0->extra.tmd->coords[(s16)(work->stateFrames / 3)];
                vec.vx = 0;
                vec.vy = -700;
                vec.vx = (s16)(work->stateFrames % 3) * 33;
                effectSpawn(EFFECT_HIT_PUFF, coord, 0x11101800, &vec);
                vec.vy = 700;
                effectSpawn(EFFECT_HIT_PUFF, coord, 0x11101800, &vec);
            }
            work->stateFrames++;
            if (work->stateFrames >= 0x1A) {
                a = 0xF;
                if (arg0->spawnArg1.value == phase) {
                    a = 0xD;
                    b = 0xE;
                } else {
                    b = 0x10;
                }
                child = actor503500SpawnSlotEnemy(arg0->parent, a);
                if (child != NULL) {
                    child->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
                    child->hp                  = enemy->hp / 2;
                }
                child = actor503500SpawnSlotEnemy(arg0->parent, b);
                if (child != NULL) {
                    child->task->killCountdown = ACTOR_503500_SLOT_COMMAND_BECOME_TARGET;
                    child->hp                  = enemy->hp / 2;
                }
                actor503500ClearSlotEnemy(arg0->parent, arg0->spawnArg1.value);
                work->bufferFreeCountdown = 3;
                work->stateFrames         = 0;
                work->stateStep++;
            }
            break;
        case 3:
            arg0->extra.tmd->flags |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work->stateFrames++;
            if (work->stateFrames >= 0x5B) {
                actor503500ReleaseSlotEffects(arg0->spawnArg1.value);
                arg0->state++;
            }
            break;
    }
}

static void func_actor_503500_801398D0(Task* arg0)
{
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    s32                         dmg;
    u8                          flags;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if ((actor503500IsDefeated() == 0) && (gGameSession->eventState == 0)) {
        flags = enemy->reactionFlags;
        if (flags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
            _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            work->holdFrames = 5;
            work->slowFrames = 8;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_DAMAGE_OVER_TIME);
            if (damageIsEnemyDamageOverTimeExpired(arg0->spawnArg2.pointer) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            } else {
                dmg = damageTickEnemyDamageOverTime(enemy);
                if (dmg != 0) {
                    enemy->hp -= dmg;
                    worldTargetAddReadoutAmount(&enemy->node, dmg, 0);
                    work->slowFrames = 8;
                    if (enemy->hp <= 0) {
                        enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
                        _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_DYING);
                    } else {
                        _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
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

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy, like `func_actor_503500_80134EAC`: each attack id is taken once,
/// only type-2 ids land while `hitCooldown` is clear, and a hit
/// that empties `hp` starts the dying state. The hit effect is placed at the
/// record's contact point, pulled to 800 units from part 8 along the offset
/// and rotated into its frame. `arg1` is passed by the caller but unused.
static void func_actor_503500_80139A20(Task* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3)
{
    _Actor503500LargeChainWork* work;
    Enemy*                      enemy;
    GfxCoord*                   coord;
    s32                         i;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[ACTOR_503500_LARGE_CHAIN_TIP_PART];
    for (i = 0; i < arg3; i++) {
        _actor503500LargeChainHandleHit(arg0, work, enemy, coord, arg2, i);
    }
}

/// Steers `tipPosition` toward `tipTarget`. Inside the arrival distance (the
/// integer half of `tipSpeedLimit`) it sets `tipArrived` and stops; otherwise
/// `tipSpeed` accelerates toward +/-`tipSpeedLimit` while `tipAdvancing` is
/// set, or decays to 0, and moves `tipPosition` along the normalized offset
/// (at a quarter speed while `slowFrames` runs).
static void func_actor_503500_80139EFC(Task* arg0)
{
    SVECTOR                     d;
    SVECTOR                     n;
    VECTOR                      step;
    _Actor503500LargeChainWork* work;
    s16                         tx;
    s16                         ty;
    s16                         tz;
    s32                         lim;
    s32                         speed;
    s32                         k;

    work = arg0->work;
    tx   = work->tipTarget.vx - work->tipPosition.vx;
    d.vx = tx;
    ty   = work->tipTarget.vy - work->tipPosition.vy;
    d.vy = ty;
    tz   = work->tipTarget.vz - work->tipPosition.vz;
    d.vz = tz;
    if (ABS(tx) + ABS(ty) + ABS(tz) < work->tipSpeedLimit.halves.integer) {
        work->tipArrived = 1;
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
    k                     = speed >> 12;
    step.vx               = n.vx * k;
    step.vy               = n.vy * k;
    step.vz               = n.vz * k;
    work->tipPosition.vx += step.vx >> 16;
    work->tipPosition.vy += step.vy >> 16;
    work->tipPosition.vz += step.vz >> 16;
}

/// Builds the chain polyline `linkPoints[0..8]` from cubic Bezier segments
/// (`bezierCurveEvaluate`): a first curve runs from the root's world
/// position, through a point 1000 units along its Z axis, to the parent-local
/// `tipPosition` point raised in Y; `linkPoints[1..5]` and `linkPoints[6..8]`
/// are then sampled from two curves re-seeded from that first one.
/// `func_actor_503500_8013A470` re-aims the links along the result, and
/// `pulsePhase` advances by 0x80.
static void func_actor_503500_8013A0D0(Task* arg0)
{
    SVECTOR                     ctrl[4];
    SVECTOR                     ofs;
    SVECTOR                     tmp;
    VECTOR                      out[9];
    VECTOR                      v;
    MATRIX                      m;
    GfxCoord*                   coord;
    _Actor503500LargeChainWork* work;
    s32                         i;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    gfxComposeNodeWorldTransform(coord, &m, &ctrl[0]);
    work->linkPoints[0].vx = ctrl[0].vx;
    work->linkPoints[0].vy = ctrl[0].vy;
    work->linkPoints[0].vz = ctrl[0].vz;
    ofs.vx                 = 0;
    ofs.vy                 = 0;
    ofs.vz                 = 1000;
    gte_SetRotMatrix(&m);
    gte_ldv0(&ofs);
    gte_rtv0();
    gte_stsv(&ctrl[1]);
    ctrl[1].vx += ctrl[0].vx;
    ctrl[1].vy += ctrl[0].vy;
    ctrl[1].vz += ctrl[0].vz;
    gfxComposeNodeWorldTransform(coord->parent, &m, &tmp);
    gte_SetRotMatrix(&m);
    gte_ldv0(&work->tipPosition);
    gte_rtv0();
    gte_stsv(&ofs);
    tmp.vx    += ofs.vx;
    tmp.vy    += ofs.vy;
    tmp.vz    += ofs.vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy - 3000;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy - 2000;
    ctrl[3].vz = tmp.vz;
    for (i = 8; i >= 0; i--) {
        bezierCurveEvaluate(ctrl, &ctrl[3], 9, i, &out[i].vx);
    }
    ctrl[0].vx = out[8].vx;
    ctrl[0].vy = out[8].vy;
    ctrl[0].vz = out[8].vz;
    ctrl[1].vx = out[6].vx;
    ctrl[1].vy = out[6].vy + 1000;
    ctrl[1].vz = out[6].vz;
    ctrl[2].vx = out[5].vx;
    ctrl[2].vy = out[5].vy - 2000;
    ctrl[2].vz = out[5].vz;
    ctrl[3].vx = out[4].vx;
    ctrl[3].vy = out[4].vy - 2000;
    ctrl[3].vz = out[4].vz;
    for (i = 4; i >= 0; i--) {
        bezierCurveEvaluate(ctrl, &ctrl[3], 5, i, &v.vx);
        copyVector(&work->linkPoints[5 - i], &v);
    }
    ctrl[0].vx = out[4].vx;
    ctrl[0].vy = out[4].vy - 2000;
    ctrl[0].vz = out[4].vz;
    ctrl[1].vx = out[3].vx;
    ctrl[1].vy = out[3].vy - 2000;
    ctrl[1].vz = out[3].vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy - 1000;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy;
    ctrl[3].vz = tmp.vz;
    for (i = 2; i >= 0; i--) {
        bezierCurveEvaluate(ctrl, &ctrl[3], 16, i + 12, &v.vx);
        copyVector(&work->linkPoints[8 - i], &v);
    }
    func_actor_503500_8013A470(work->linkPoints, arg0->extra.tmd->coords, work->pulsePhase);
    work->pulsePhase = (work->pulsePhase + 0x80) & 0xFFF;
}

/// Scaled variant of `func_actor_503500_8014176C`: re-aims the eight child
/// coordinates along `pts[0..8]`, normalising each basis with `MatrixNormal`,
/// and from the second link on sets the translation to the local segment
/// scaled by `0x1000 + rsin(phase) / 64` (a 1/64 pulse).
static void func_actor_503500_8013A470(SVECTOR* pts, GfxCoord* coords, s32 phase)
{
    Actor503500ChainScratch* s;
    s32                      scale;
    s32                      i;
    s32                      j;

    s        = SCRATCH_STACK_RESERVE_BLOCK(Actor503500ChainScratch);
    s->up.vx = 0;
    s->up.vy = 0x1000;
    s->up.vz = 0;
    gfxComposeNodeWorldTransform(coords->parent, &s->worldRotation, &s->parentTranslation);
    scale = ((rsin(phase) << 6) >> 12) + 0x1000;
    for (i = 0, j = 1; i < 8; i++, j++) {
        s->segment.vx = pts[j].vx - pts[i].vx;
        s->segment.vy = pts[j].vy - pts[i].vy;
        s->segment.vz = pts[j].vz - pts[i].vz;
        gte_SetRotMatrix(&s->worldRotation);
        gte_ldclmv(&coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->worldRotation);
        gte_ldclmv(&coords[i].coord.m[0][1]);
        gte_rtir();
        gte_stclmv(&s->worldRotation.m[0][1]);
        gte_ldclmv(&coords[i].coord.m[0][2]);
        gte_rtir();
        gte_stclmv(&s->worldRotation.m[0][2]);
        gte_TransposeMatrix(&s->worldRotation, &s->inverseRotation);
        gte_SetRotMatrix(&s->inverseRotation);
        gte_ldv0(&s->segment);
        gte_rtv0();
        gte_stlvnl(&s->localSegment);
        VectorNormalS(&s->localSegment, &s->direction);
        gfxBuildOrthonormalBasis(&s->basis, &s->direction, &s->up);
        MatrixNormal(&s->basis, &coords[j].coord);
        if (i != 0) {
            coords[j].coord.t[0] = (s->localSegment.vx * scale) >> 12;
            coords[j].coord.t[1] = (s->localSegment.vy * scale) >> 12;
            coords[j].coord.t[2] = (s->localSegment.vz * scale) >> 12;
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
            func_actor_503500_80138A30(arg0);
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_SHOOT:
            func_actor_503500_80138C08(arg0);
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_HOLD:
            timer            = (u16)work->holdFrames - 1;
            work->holdFrames = timer;
            if (timer < 0) {
                _actor503500LargeChainEnterState(arg0, ACTOR_503500_LARGE_CHAIN_STATE_IDLE);
            }
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_DYING:
            func_actor_503500_80139014(arg0);
            break;
        case ACTOR_503500_LARGE_CHAIN_STATE_SPLITTING:
            func_actor_503500_801395BC(arg0);
            break;
    }
    timer            = (u16)work->slowFrames - 1;
    work->slowFrames = timer;
    if (timer < 0) {
        work->slowFrames = 0;
    }
}

/// Steps a large chain's `hitCooldown` down to zero, then, unless the boss
/// is defeated, applies the hits in the target sphere's `contacts` before
/// emptying the table. Same shape as `func_actor_503500_8013BD0C`.
static void func_actor_503500_8013AA44(Task* arg0)
{
    _Actor503500LargeChainWork* work;
    s16                         timer;

    work = arg0->work;
    if (work->hitCooldown != 0) {
        timer             = (u16)work->hitCooldown - 1;
        work->hitCooldown = timer;
        if (timer < 0) {
            work->hitCooldown = 0;
        }
    }
    if (actor503500IsDefeated() == 0) {
        func_actor_503500_80139A20(arg0, &work->body, work->contacts, ARRAY_SIZE(work->contacts));
    }
    worldCollisionClearContacts(work->contacts);
}

static void func_actor_503500_8013AAC0(Task* arg0)
{
    VECTOR vec;

    vec.vx = arg0->extra.tmd->coords->workm.t[0];
    vec.vy = arg0->extra.tmd->coords->workm.t[1];
    vec.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Blends a large chain's model parts 1..8 toward `bindPose`: while
/// `blendWeight` is below 0x1000, each part's `coord` rotation goes through
/// `gfxBlendOrthonormalRotation` and its translation keeps a `blendWeight / 0x1000`
/// share of its offset from the saved matrix.
static void func_actor_503500_8013AB38(Task* arg0)
{
    VECTOR                      d;
    _Actor503500LargeChainWork* work;
    GfxCoord*                   coord;
    MATRIX*                     mat;
    s32                         t;
    s32                         i;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords + 1;
    if (work->blendWeight < 0x1000) {
        mat = &work->bindPose[1];
        t   = work->blendWeight;
        for (i = 1; i < ACTOR_503500_LARGE_CHAIN_PART_COUNT; i++) {
            gfxBlendOrthonormalRotation(mat, &coord->coord, &coord->coord, t);
            d.vx              = ((coord->coord.t[0] - mat->t[0]) * t) >> 12;
            d.vy              = ((coord->coord.t[1] - mat->t[1]) * t) >> 12;
            d.vz              = ((coord->coord.t[2] - mat->t[2]) * t) >> 12;
            coord->coord.t[0] = mat->t[0] + d.vx;
            coord->coord.t[1] = mat->t[1] + d.vy;
            coord->coord.t[2] = mat->t[2] + d.vz;
            mat++;
            coord++;
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

void func_actor_503500_8013AD0C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131F9C;
    sp.funcs[task->state](task);
}
