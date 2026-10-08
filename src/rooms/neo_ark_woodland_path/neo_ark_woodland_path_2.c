#include "rooms/neo_ark_woodland_path.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_woodland_path_private.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
#include "../../shared/falling_leaves.h"
#include "../../shared/roaming_enemies.h"

static void _roamerArmPoolB(Task* task);
static void _roamerArmPoolA(Task* task);

static s32  _roamerAmbushMsg(Task* task, s32 messageId, struct ActorCommand* msg, s32 unusedArg);
static void _roamerBankRetreat(Task* task, s32 messageId, s32 hp, s32 unusedArg);

#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

/// The distance between `a` and `b`, spelled as a conditional subtraction.

extern ActorCommand gRoamerCommand;

/// The room's five spawn slots: `_roamerBankRetreat` fills the
/// first free one with a countdown and `_roamerAmbushMsg`
/// hands slot 0 to the spawn it triggers and clears it. Read as `lhu` by the
/// handler and as `lh` by the slot filler, so each site names the view it uses
/// (`[0]` here, an `s16*` cast there).
extern u16 gRoamerReserveHp[5];

/// Ceiling `_roamerBankRetreat` clamps a spawn slot to
/// (0x1A4, 420 frames). Only the first halfword is this unit's; the run
/// continues into the room's parameter block, so the extent is splat's.

/// The same run reached through its leading label, which is how
/// `_roamerArmPoolB` reads the ceiling: element 2 is
/// `D_...8494C[0]`, 420 frames. splat names both addresses because the compiled
/// code names both, and the two are different code - an index keeps this
/// symbol in a register and takes the offset as the load's displacement, while
/// naming `D_...8494C` addresses it directly.
extern EnemyParams  gRoamerParams;
extern DamageAttack D_neo_ark_woodland_path_80184930[6];

/// The room's arming count, packed into game flag 0x10A as a nibble:
/// `_roamerArmPoolB` adds the slot's spawn count to it and
/// then caps it at 5, the number of slots `D_...84A60` has. Signed, though the
/// add reads it as `lhu` - the result is truncated by the following `sh`, so
/// only the low half matters and GCC picks the unsigned load by itself.
extern s16 gRoamerReserveCount;

/// How many spawns each slot arms, indexed by `gGameSession->location.loc.variant` (the
/// slot the session is in): the byte `func_...80180C6C` adds to
/// `D_...80184990`, and the gate `_roamerTickPoolB` tests against zero.
extern u8 gRoamerArmCountsB[];

/// The message-handler table `_roamerArmPoolB` parks in
/// `Task::msgTable`: a placement request (0x13EF,
/// `_roamerLatchSpawnRequestPoolB`), a countdown bump (0x13F4) and the
/// 0x7DB command handler `_roamerAmbushMsg`.
extern TaskMessageEntry gRoamerMsgTableB[];

/// The same gate for the arm-state one step earlier: `func_...80180568` tests
/// it against zero and `func_...801806D8` reads the slot's count from it. One
/// byte per session slot, indexed by `gGameSession->location.loc.variant`, like
/// `D_...84970` above.
extern u8 gRoamerArmCountsA[];

/// `func_...80180568`'s own message-handler table, parked in `Task::msgTable`
/// as `D_...849F4` is by `func_...80180C6C`: the same three ids, answered by
/// the placement request `_roamerLatchSpawnRequestPoolA`, the spawn-slot
/// filler `_roamerBankRetreat` and a 0x7DB handler that
/// ignores the message.
extern TaskMessageEntry gRoamerMsgTableA[];

/// Set once a spawn slot has been armed, read by the room's other states.
extern s16 gRoamerReleasePending;

/// The room's spawn placements, indexed by `_gRoamerPendingSpawnPoint - 1`.
extern RoamerSpawnPoint gRoamerSpawnPointsA[];

/// The second arming sequence's spawn placements, which
/// `_roamerTickPoolB` picks from by `_gRoamerPendingSpawnPoint - 1`. Five of them; any
/// request past the fourth takes the last.
extern RoamerSpawnPoint D_neo_ark_woodland_path_80184A14[5];

/// `gSceneCombatState.battleRefs` as `func_...801806D8` saw it on the previous frame, so
/// that it can tell the reference count was non-zero before the frame began.
extern s16 gRoamerPrevBattleRefs;

static s32 _neoArkWoodlandPathIgnoreRoamerCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);
static s32 _roamerExtendCooldown(Task* unusedTask, s32 unusedMessageId, s32 unusedEventValue, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolA(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolB(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static void _neoArkWoodlandPathRoamerPoolATask(Task* task);
static void _neoArkWoodlandPathRoamerPoolBTask(Task* task);

DamageAttack D_neo_ark_woodland_path_80184930[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams gRoamerParams = { D_neo_ark_woodland_path_80184930, 420, 115, 200, 5, 100, 10, 100, 10 };

// Retained numeric records following the enemy parameters.
u16 D_neo_ark_woodland_path_80184958[3][4] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

u8 gRoamerArmCountsB[16] = {
    0,
    1,
    3,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 gRoamerArmCountsA[14] = {
    0,
    3,
    2,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Signed countdown shared by the pools, gating spawn requests and battle reset.
///
/// Active per-frame states decrement positive values; zero is ready and -1
/// pauses the countdown. Arming or ambush restart sets 90 frames, spawning or
/// retreat adds 90, and battle completion sets 150. Arithmetic stores narrow
/// back to a signed halfword.
static s16 _gRoamerCooldownFrames = ROAMER_INITIAL_COOLDOWN_FRAMES;

s16 gRoamerReserveCount = 0;

/// One-based spawn-point selector pending for the next active pool tick.
///
/// Zero means none. Room actions select 1..6 here; pool A uses that table row,
/// while pool B uses its fifth row for selectors above four. Each active tick
/// clears the selector even if no enemy was revived. The latch also clears it
/// for repeated actions or while the cooldown is not zero.
static s16 _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;

/// Last room-action ID observed by either pool's spawn-request latch.
///
/// Starts at zero and records the borrowed request's byte even when cooldown
/// suppresses it, so the same action must change before it can request a spawn.
static s16 _gRoamerLastActionId = ROAMER_SPAWN_POINT_NONE;

s16 gRoamerReleasePending = 0;

TaskMessageEntry gRoamerMsgTableA[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolA },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerBankRetreat },
    { ACTOR_COMMAND_MESSAGE_APPLY, _neoArkWoodlandPathIgnoreRoamerCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint gRoamerSpawnPointsA[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
};

s16 gRoamerPrevBattleRefs = 0;

TaskMessageEntry gRoamerMsgTableB[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolB },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerExtendCooldown },
    { ACTOR_COMMAND_MESSAGE_APPLY, _roamerAmbushMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

RoamerSpawnPoint D_neo_ark_woodland_path_80184A14[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

s16 D_neo_ark_woodland_path_80184A3C[4] = {
    0x7FFF,
    0x7FFF,
    0x7FFF,
    0x7FFF,
};

TaskDesc D_neo_ark_woodland_path_80184A44[2] = {
    { { { TASK_BODY_NONE, 32 } }, _neoArkWoodlandPathRoamerPoolBTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _neoArkWoodlandPathRoamerPoolATask, { .value = 0 } },
};

ActorCommand gRoamerCommand = { { .loc = { 0, 0 } }, 0 };

u16 gRoamerReserveHp[5];

static void _neoArkWoodlandPathAdvanceRoamerPoolAState(Task* task);

static void _neoArkWoodlandPathAdvanceRoamerPoolBState(Task* task);

static void _roamerTickPoolB(Task* task);

void neoArkWoodlandPathPlayerWaterSplashTask(Task* task)
{
    enum {
        WATER_SPLASH_STATE_NEW          = 0,
        WATER_SPLASH_STATE_ACTIVE       = 1,
        WATER_SPLASH_SAMPLE_COUNT       = ARRAY_SIZE(D_neo_ark_woodland_path_80181684),
        WATER_SPLASH_FIRST_PART         = 15,
        WATER_SPLASH_PART_STRIDE        = 3,
        WATER_SPLASH_VIEW_Y_LIMIT       = 17,
        WATER_SPLASH_MIN_ROOT_Y         = 300,
        WATER_SPLASH_SURFACE_Y          = 200,
        WATER_SPLASH_ROLL_MASK          = 0x1FF,
        WATER_SPLASH_RIPPLE_ODDS_BIAS   = 32,
        WATER_SPLASH_RIPPLE_HALF_SIDE   = 64,
        WATER_SPLASH_SPRAY_SIZE         = 384,
        WATER_SPLASH_SPRAY_CELL_UPDATES = 2,
        WATER_SPLASH_SPRAY_LAUNCH_SPEED = 32,
        WATER_SPLASH_SPRAY_UPWARD_BURST = 1,
        WATER_SPLASH_SPRAY_SPAWN_ARG    = WATER_SPLASH_SPRAY_SIZE |
                                       (WATER_SPLASH_SPRAY_CELL_UPDATES << 12) |
                                       (WATER_SPLASH_SPRAY_LAUNCH_SPEED << 16) |
                                       (WATER_SPLASH_SPRAY_UPWARD_BURST << 24)
    };
    EffectWork* work;
    Task*       playerTask;
    GfxCoord*   playerRoot;
    GfxCoord*   trackedPart;
    GfxCoord    surfaceCoord;
    s32         sampleIndex;

/// Rolls ripple then spray for one tracked player part and advances its history.
///
/// Captures work, playerTask, playerRoot, trackedPart and surfaceCoord from
/// this function. sampleIndex must be a side-effect-free index 0 or 1 and is
/// evaluated repeatedly. Expands to statements for a braced loop body; the
/// coordinate is copied by spawn and the emitted children do not follow it.
#define NEO_ARK_WOODLAND_PATH_EMIT_PART_WATER_EFFECTS(sampleIndex)                                                                                                                                                                                                                                                                     \
    trackedPart               = &playerTask->extra.tmd->coords[(sampleIndex) * WATER_SPLASH_PART_STRIDE + WATER_SPLASH_FIRST_PART];                                                                                                                                                                                                    \
    work->angle               = ABS_DIFF(D_neo_ark_woodland_path_80181684[(sampleIndex)].vx, trackedPart->workm.t[0]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[(sampleIndex)].vy, trackedPart->workm.t[1]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[(sampleIndex)].vz, trackedPart->workm.t[2]) + WATER_SPLASH_RIPPLE_ODDS_BIAS; \
    surfaceCoord.parent       = playerRoot->parent;                                                                                                                                                                                                                                                                                    \
    surfaceCoord.coord        = playerRoot->coord;                                                                                                                                                                                                                                                                                     \
    surfaceCoord.coord.t[0]   = playerRoot->coord.t[0];                                                                                                                                                                                                                                                                                \
    surfaceCoord.coord.t[1]   = WATER_SPLASH_SURFACE_Y;                                                                                                                                                                                                                                                                                \
    surfaceCoord.coord.t[2]   = playerRoot->coord.t[2];                                                                                                                                                                                                                                                                                \
    surfaceCoord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                                                                                                                                                                                                                                  \
    actorRenderComposeCoord(&surfaceCoord);                                                                                                                                                                                                                                                                                            \
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                                                                                                                                                                                                                                  \
    if ((s32)((gRandomLcgState >> 16) & WATER_SPLASH_ROLL_MASK) < work->angle) {                                                                                                                                                                                                                                                       \
        effectSpawn(gRoomEffectWaterRippleId, &surfaceCoord, WATER_SPLASH_RIPPLE_HALF_SIDE, 0);                                                                                                                                                                                                                                        \
    }                                                                                                                                                                                                                                                                                                                                  \
    work->angle    -= WATER_SPLASH_RIPPLE_ODDS_BIAS;                                                                                                                                                                                                                                                                                   \
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                                                                                                                                                                                                                                  \
    if ((s32)((gRandomLcgState >> 16) & WATER_SPLASH_ROLL_MASK) < work->angle) {                                                                                                                                                                                                                                                       \
        effectSpawn(gRoomEffectWaterSprayId, &surfaceCoord, WATER_SPLASH_SPRAY_SPAWN_ARG, 0);                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                                  \
    D_neo_ark_woodland_path_80181684[(sampleIndex)].vx = trackedPart->workm.t[0];                                                                                                                                                                                                                                                      \
    D_neo_ark_woodland_path_80181684[(sampleIndex)].vy = trackedPart->workm.t[1];                                                                                                                                                                                                                                                      \
    D_neo_ark_woodland_path_80181684[(sampleIndex)].vz = trackedPart->workm.t[2];

    work       = task->spawnArg2.pointer;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerRoot = playerTask->extra.tmd->coords;
    // Start from the current composed part positions, with no initial movement.
    if (task->state == WATER_SPLASH_STATE_NEW) {
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_WOODLAND_PATH_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_WOODLAND_PATH_WATER_SPRAY;
        task->state              = WATER_SPLASH_STATE_ACTIVE;
        for (sampleIndex = 0; sampleIndex < WATER_SPLASH_SAMPLE_COUNT; sampleIndex++) {
            trackedPart                                      = &playerTask->extra.tmd->coords[sampleIndex * WATER_SPLASH_PART_STRIDE + WATER_SPLASH_FIRST_PART];
            D_neo_ark_woodland_path_80181684[sampleIndex].vx = trackedPart->workm.t[0];
            D_neo_ark_woodland_path_80181684[sampleIndex].vy = trackedPart->workm.t[1];
            D_neo_ark_woodland_path_80181684[sampleIndex].vz = trackedPart->workm.t[2];
        }
    }
    gRoomEffectState->roomEffectMode = (playerRoot->coord.t[1] < WATER_SPLASH_VIEW_Y_LIMIT) * ROOM_EFFECT_VIEW_ENABLED;
    // Inactive ticks retain the last emitting positions, including across suspension.
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && playerRoot->coord.t[1] >= WATER_SPLASH_MIN_ROOT_Y) {
        for (sampleIndex = 0; sampleIndex < WATER_SPLASH_SAMPLE_COUNT; sampleIndex++) {
            NEO_ARK_WOODLAND_PATH_EMIT_PART_WATER_EFFECTS(sampleIndex);
        }
    }

#undef NEO_ARK_WOODLAND_PATH_EMIT_PART_WATER_EFFECTS
}

#include "../../shared/falling_leaves_task.inc.c"

void neoArkWoodlandPathLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/falling_leaves_draw_neo_ark.inc.c"

#include "../../shared/water_ripple_task.inc.c"

void neoArkWoodlandPathWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void neoArkWoodlandPathWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

#include "../../shared/roaming_enemies_bank_retreat.inc.c"

#include "../../shared/roaming_enemies_seed_reserve_hp.inc.c"

#include "../../shared/roaming_enemies_arm_pool_a.inc.c"

#include "../../shared/roaming_enemies_tick_pool_a.inc.c"

/// State handlers of the first arming sequence's entry task
/// `_neoArkWoodlandPathRoamerPoolATask`: arm, run, advance, then kill.
static const TaskFuncTable4 D_neo_ark_woodland_path_8017D638 = {
    { _roamerArmPoolA, _roamerTickPoolA,
      _neoArkWoodlandPathAdvanceRoamerPoolAState, taskKill }
};

#include "../../shared/roaming_enemies_ambush_msg.inc.c"

#include "../../shared/roaming_enemies_arm_pool_b.inc.c"

/// Updates the roaming reserve feeding the Neo Ark forest zone and revives one enemy.
///
/// Requires initialized shared pool storage, a location variant in 0..15, and
/// a reserve count in 0..5. A zero arming count leaves all pool state unchanged.
/// Active ticks count down positive cooldowns, persist surviving banked slots
/// when the battle-reference count falls to zero, and clear a pending battle
/// reset at cooldown zero. Signed HP tests treat nonpositive slots as empty.
///
/// A nonzero request selects rows 1..4 directly and row 5 for larger selectors.
/// The first available placed actor among indices 0 and 1 receives the first
/// positive reserve HP and a leap-in command, then is placed at Y=0 with the
/// selected X/Z and yaw (4096 units per turn). Every active tick clears the
/// request, even if no actor was revived. The task argument is unused.
static void _roamerTickPoolB(Task* task)
{
    enum {
        ROAMER_POOL_B_RESERVE_SLOTS   = ARRAY_SIZE(gRoamerReserveHp),
        ROAMER_POOL_B_PLACED_ACTORS   = 2,
        ROAMER_POOL_B_HP_AVAILABLE    = -999,
        ROAMER_POOL_B_COMMAND_LEAP_IN = 11
    };
    s16    actorIndex;
    s16    bankedCount;
    s32    previousKills;
    s32    persistedReserve;
    Enemy* enemy;
    s16    reserveIndex;
    s16    reserveSlot;

/// Places the revived actor without caching or reordering its lookups.
///
/// actorIndex and pointIndex are side-effect-free indices in 0..1 and 0..4;
/// they are evaluated repeatedly. dirtyBeforeTurn must be constant 0 or 1 and
/// preserves the row's dirty-stamp ordering. Uses the room's borrowed spawn
/// table; expands to statements and must be invoked as the complete statements of a switch arm.
#define ROAMER_PLACE_POOL_B_ENEMY(actorIndex, pointIndex, dirtyBeforeTurn)                                            \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[pointIndex].x; \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[1] = 0;                                              \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[pointIndex].z; \
    if (dirtyBeforeTurn) {                                                                                            \
        sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                     \
    }                                                                                                                 \
    gfxRotMatrixY(&sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord,                                        \
                  D_neo_ark_woodland_path_80184A14[pointIndex].yaw, GRAPHICS_ROTATION_REPLACE);                       \
    if (!(dirtyBeforeTurn)) {                                                                                         \
        sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                     \
    }

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (_gRoamerCooldownFrames > ROAMER_COOLDOWN_READY) {
        _gRoamerCooldownFrames--;
    }
    // Persist the reserve only on the transition out of a live battle.
    if (gSceneCombatState.battleRefs == 0 && gRoamerPrevBattleRefs > 0) {
        persistedReserve = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        bankedCount      = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if ((s16)gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", persistedReserve - bankedCount);
        previousKills    = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B);
        persistedReserve = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        bankedCount      = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if ((s16)gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B, previousKills + (persistedReserve - bankedCount));
        bankedCount = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if ((s16)gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, bankedCount);
        areaSyncLocationVariant(&gGameSession->location.loc);
        _gRoamerCooldownFrames = ROAMER_POST_BATTLE_COOLDOWN_FRAMES;
    }
    gRoamerPrevBattleRefs = gSceneCombatState.battleRefs;
    // Defer clearing battle rewards until the post-battle cooldown expires.
    if (gGameSession->battleResetPending == 1 && _gRoamerCooldownFrames == ROAMER_COOLDOWN_READY) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.peTargetCount             = 0;
        gSceneCombatState.battleRefs                = 0;
        gSceneCombatState.expReward                 = 0;
        gSceneCombatState.bpReward                  = 0;
        gSceneCombatState.mpReward                  = 0;
        gGameSession->battleResetPending            = 0;
    }
    // Consume at most one banked slot and revive the first available actor.
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED && _gRoamerPendingSpawnPoint != ROAMER_SPAWN_POINT_NONE) {
        gRoamerCommand.context.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
        gRoamerCommand.context.loc.area  = GAME_AREA_NEO_ARK_FOREST_ZONE;
        gRoamerCommand.command           = ROAMER_POOL_B_COMMAND_LEAP_IN;
        for (actorIndex = 0; actorIndex < ROAMER_POOL_B_PLACED_ACTORS; actorIndex++) {
            if (sceneFindPlacedActor(actorIndex) == 0) {
                break;
            }
            enemy = sceneFindPlacedActor(actorIndex)->spawnArg2.pointer;
            if (enemy == NULL) {
                break;
            }
            if (enemy->hp == ROAMER_POOL_B_HP_AVAILABLE) {
                for (reserveIndex = 0; reserveIndex < gRoamerReserveCount; reserveIndex++) {
                    if ((s16)gRoamerReserveHp[reserveIndex] > 0) {
                        enemy->hp                      = gRoamerReserveHp[reserveIndex];
                        enemy->reactionFlags           = 0;
                        gRoamerReserveHp[reserveIndex] = 0;
                        break;
                    }
                }
                if (enemy->hp > 0) {
                    sceneAcquireBattleRef(0);
                    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(actorIndex), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    switch ((s16)(_gRoamerPendingSpawnPoint - 1)) {
                        case 0:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 0, 1);
                            break;
                        case 1:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 1, 1);
                            break;
                        case 2:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 2, 0);
                            break;
                        case 3:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 3, 0);
                            break;
                        case 4:
                        default:
                            ROAMER_PLACE_POOL_B_ENEMY(actorIndex, 4, 0);
                            break;
                    }
                }
                break;
            }
        }
    }
    _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;
#undef ROAMER_PLACE_POOL_B_ENEMY
}

/// State handlers of the second arming sequence's entry task
/// `_neoArkWoodlandPathRoamerPoolBTask`: arm, run, advance, then kill.
static const TaskFuncTable4 D_neo_ark_woodland_path_8017D684 = {
    { _roamerArmPoolB, _roamerTickPoolB,
      _neoArkWoodlandPathAdvanceRoamerPoolBState, taskKill }
};

/// Ignores actor commands delivered to the pool-A controller and returns zero.
///
/// Serves `ACTOR_COMMAND_MESSAGE_APPLY` in `gRoamerMsgTableA`. All arguments
/// are unused: the command is neither read, modified nor retained, and the
/// controller's task state is unchanged.
static s32 _neoArkWoodlandPathIgnoreRoamerCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg)
{
    return 0;
}

/// Selects the pool-A room-action latch emitted by the shared fragment.
///
/// The callback type and inclusion requirements are documented in
/// `roaming_enemies_latch_request.inc.c`.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolA
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the live pool-A controller from its pre-kill state 2 to state 3.
///
/// The following controller tick runs `taskKill`; this tick only increments
/// the state and leaves the task and its message table live.
static void _neoArkWoodlandPathAdvanceRoamerPoolAState(Task* task)
{
    task->state++;
}

/// Dispatches the woodland path controller for the roaming reserve feeding area 29.
///
/// Requires a live no-body task with state 0..3 and location variant 0..13.
/// State 0 arms the shared reserve and installs pool A messages, state 1 updates
/// it, state 2 advances to teardown, and state 3 kills the task. The two pools
/// share banked HP, cooldown and pending requests for the room's lifetime.
static void _neoArkWoodlandPathRoamerPoolATask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_woodland_path_8017D638;
    handlers.funcs[task->state](task);
}

#include "../../shared/roaming_enemies_extend_cooldown.inc.c"

/// Selects the private pool-B room-action latch for this fragment inclusion.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolB
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef ROAMER_LATCH_SPAWN_REQUEST

/// Advances the live pool-B controller from its pre-kill state 2 to state 3.
///
/// The following controller tick runs `taskKill`; this tick only increments
/// the state and leaves the task and its message table live.
static void _neoArkWoodlandPathAdvanceRoamerPoolBState(Task* task)
{
    task->state++;
}

/// Dispatches the woodland path controller for the roaming reserve feeding area 11.
///
/// Requires a live no-body task with state 0..3 and location variant 0..15.
/// State 0 arms the shared reserve and installs pool B messages, state 1 updates
/// it, state 2 advances to teardown, and state 3 kills the task. The two pools
/// share banked HP, cooldown and pending requests for the room's lifetime.
static void _neoArkWoodlandPathRoamerPoolBTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_neo_ark_woodland_path_8017D684;
    handlers.funcs[task->state](task);
}
