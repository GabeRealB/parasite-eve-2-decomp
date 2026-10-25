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

#include "actors/actor.h"

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
#include "../../shared/effects/water_effects.h"
#include "../../shared/effects/falling_leaves.h"
#include "../../shared/room/roaming_enemies.h"

static void _roamerArmPoolB(Task* task);
static void _roamerArmPoolA(Task* task);

static s32  _roamerAmbushMsg(Task* task, s32 messageId, struct ActorCommand* msg, s32 unusedArg);
static void _roamerBankRetreat(Task* task, s32 messageId, s32 hp, s32 unusedArg);

/// Absolute displacement between two signed coordinate components.
///
/// Both arguments are evaluated twice and must be stable and side-effect-free.
/// Their promoted type must be signed, with both subtraction orders representable;
/// this is not an unsigned-distance or saturating-arithmetic helper.
/// The caller sums XYZ displacements in whole game-coordinate units.
#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

static s32 _neoArkWoodlandPathIgnoreRoamerCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);
static s32 _roamerExtendCooldown(Task* unusedTask, s32 unusedMessageId, s32 unusedEventValue, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolA(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _roamerLatchSpawnRequestPoolB(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static void _neoArkWoodlandPathRoamerPoolATask(Task* task);
static void _neoArkWoodlandPathRoamerPoolBTask(Task* task);

/// Retained six-row attack recipe borrowed by the reserve-enemy parameters.
///
/// Powers are 30, 30, 50, 50, 40 and 40 before HP-band scaling. The first four
/// select category-4 reaction 7 (presentation unproven); the last two select
/// ordinary reaction 0. The pool controllers do not index these attack rows.
static DamageAttack _gNeoArkWoodlandPathRoamerAttacks[6] = {
    { 30, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 30, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 50, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 50, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 40, 0 },
    { 40, 0 },
};

/// Reserve-enemy recipe with a 420-HP starting value and retreat-bank ceiling.
///
/// The pool uses hpMax to seed and cap banked health; the retained attack and
/// reward/reaction tuning is not applied by the controllers to placed enemies.
/// The recipe borrows its carrier's six attack rows for this overlay lifetime.
static EnemyParams _gRoamerParams = {
    .attacks              = _gNeoArkWoodlandPathRoamerAttacks,
    .hpMax                = ROAMER_MAX_HP,
    .exp                  = 115,
    .bp                   = 200,
    .mp                   = 5,
    .critChance           = 100,
    .buildupSteps         = 10,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 10,
};

/// Retained Horned Stranger spawn tunings, unused by the reserve controllers.
///
/// Down-duration bases are ticks; sidestep angles use 4096 units per turn.
/// The delay values and final zero halfwords have no reader in this room.
static ActorHornedStrangerVariant _gNeoArkWoodlandPathUnusedHornedStrangerVariants[3] = {
    { .downFramesBase = 0, .sidestepAngle = 900, .sidestepDelay = 3, .unknown_6 = 0 },
    { .downFramesBase = 0, .sidestepAngle = 800, .sidestepDelay = 5, .unknown_6 = 0 },
    { .downFramesBase = 0, .sidestepAngle = 500, .sidestepDelay = 7, .unknown_6 = 0 },
};

/// Pool-B reserve additions by location variant 0..15, also gating arm/tick work.
///
/// Variants 1, 2 and 3 add 1, 3 and 2 enemies when the saved variant changes;
/// zero disables the pool for this visit. The saved sum precedes the live five-slot clamp.
static u8 _gRoamerPoolBArmCounts[16] = {
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

/// Pool-A reserve additions by location variant 0..13, also gating arm/tick work.
///
/// Variants 1, 2 and 3 add 3, 2 and 4 enemies when the saved variant changes;
/// zero disables the pool for this visit. The saved sum precedes the live five-slot clamp.
static u8 _gRoamerPoolAArmCounts[14] = {
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

/// Live reserve prefix length scanned for an enemy to revive, shared by both pools.
///
/// Arming loads a saved nibble, adds a new variant's count and persists the sum
/// before HP seeding clamps the live count to 0..5. Banking/spawning changes HP
/// slots without decrementing this prefix length. Stores retain signed 16-bit narrowing.
static s16 _gRoamerReserveCount = 0;

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

/// Deferred battle-hold release after retreat HP was banked with fewer than two holds.
///
/// NONE means no release owed; PENDING is cleared by an active pool-A tick once
/// the scene has at least two holds, immediately before releasing one. Multiple
/// deferred retreats still set the same flag; it is not a release count.
static s16 _gRoamerBattleReleasePending = ROAMER_BATTLE_RELEASE_NONE;

/// Pool-A receiver: latch room actions, bank retreat HP and ignore actor commands.
///
/// Installed only in armed variants; the final marker rejects unsupported IDs.
/// The retreat callback has no result, so its sender must ignore the dispatch
/// return. The task borrows this table and its callbacks for the loaded visit.
static TaskMessageEntry _gRoamerPoolAMessages[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolA },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerBankRetreat },
    { ACTOR_COMMAND_MESSAGE_APPLY, _neoArkWoodlandPathIgnoreRoamerCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Pool-A placements for room-action selectors 1..6, followed by an unread marker row.
///
/// Woodland triggers supply 1..6; the tick indexes selector minus one without
/// clamping. X/Z use the root's parent game units at Y=0; yaw is 4096 units per turn.
/// The seven-row extent includes retained marker storage, not another live selector.
static RoamerSpawnPoint _gRoamerPoolASpawnPoints[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { ROAMER_SPAWN_POINT_END, ROAMER_SPAWN_POINT_END, ROAMER_SPAWN_POINT_END, ROAMER_SPAWN_POINT_END },
};

/// Previous active-pool tick's scene battle-hold count, narrowed to a signed halfword.
///
/// A positive sample followed by a zero live count triggers loss persistence and
/// the post-battle cooldown. Both pools share this sample; disabled ticks leave
/// it unchanged. Preserve the signed comparison even though the source is u16.
static s16 _gRoamerPreviousBattleRefs = 0;

/// Pool-B receiver: latch room actions, extend arrival cooldown and handle forest commands.
///
/// Installed only in armed variants; the final marker rejects unsupported IDs.
/// Actor events return 1 after adding cooldown; forest command 0 pauses it and
/// command 2 starts the ambush. The task borrows table/callback storage for this visit.
static TaskMessageEntry _gRoamerPoolBMessages[4] = {
    { DIRECTION_MESSAGE_ROOM_ACTION, _roamerLatchSpawnRequestPoolB },
    { ROOM_MESSAGE_ACTOR_EVENT, _roamerExtendCooldown },
    { ACTOR_COMMAND_MESSAGE_APPLY, _roamerAmbushMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Five live woodland placements for revived pool-B roamers.
///
/// Request selectors 1..4 choose rows 0..3; 5 and the woodland selector 6 use
/// row 4. X/Z use signed room-coordinate units; yaw uses 4096 units per turn.
/// Stored Y is zero and unused: placement forces height zero. The following
/// unread marker is separate storage; no code walks this table to a terminator.
static RoamerSpawnPoint _gNeoArkWoodlandPathRoamerPoolBSpawnPoints[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

/// Unread marker row following pool B's five live spawn points.
///
/// Pool B selects only live rows 0..4. These four halfwords retain the same
/// end marker as pool A, rather than another selectable placement.
static RoamerSpawnPoint _gRoamerPoolBSpawnPointEnd = {
    .x   = ROAMER_SPAWN_POINT_END,
    .y   = ROAMER_SPAWN_POINT_END,
    .z   = ROAMER_SPAWN_POINT_END,
    .yaw = ROAMER_SPAWN_POINT_END,
};

TaskDesc gNeoArkWoodlandPathRoamerControllerTaskDescs[2] = {
    { .header = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } }, .callback = _neoArkWoodlandPathRoamerPoolBTask, .data = { .value = 0 } },
    { .header = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } }, .callback = _neoArkWoodlandPathRoamerPoolATask, .data = { .value = 0 } },
};

ActorCommand gRoamerCommand = { { .loc = { 0, 0 } }, 0 };

s16 gRoamerReserveHp[5];

static void _neoArkWoodlandPathAdvanceRoamerPoolAState(Task* task);

static void _neoArkWoodlandPathAdvanceRoamerPoolBState(Task* task);

static void _roamerTickPoolB(Task* task);

void neoArkWoodlandPathPlayerWaterSplashTask(Task* task)
{
    enum {
        WATER_SPLASH_STATE_NEW          = 0,
        WATER_SPLASH_STATE_ACTIVE       = 1,
        WATER_SPLASH_SAMPLE_COUNT       = ARRAY_SIZE(gNeoArkWoodlandPathPlayerWaterPartHistory),
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
/// Captures work, playerTask, playerRoot, trackedPart, surfaceCoord and this
/// function's WATER_SPLASH_* constants. sampleIndex is a stable, side-effect-free
/// index 0 or 1, evaluated repeatedly; the player rig must include parts 15 and 18.
/// Sums absolute cached XYZ displacement in game units, adds 32 for ripple odds,
/// then narrows to work->angle's signed halfword. Each roll is in 0..511; spray
/// uses the threshold after subtracting 32 with another halfword narrowing.
/// Always consumes two ordered LCG draws and then narrows cached XYZ into history.
/// Surface Y=200 is in the root's parent frame. Spawn snapshots the coordinate
/// but retains its address; these ripple/spray children never read that pointer.
/// Expands to standalone statements for a braced loop body.
#define NEO_ARK_WOODLAND_PATH_EMIT_PART_WATER_EFFECTS(sampleIndex)                                                                                                                                                                                                                                                                                                \
    trackedPart               = &playerTask->extra.tmd->coords[(sampleIndex) * WATER_SPLASH_PART_STRIDE + WATER_SPLASH_FIRST_PART];                                                                                                                                                                                                                               \
    work->angle               = ABS_DIFF(gNeoArkWoodlandPathPlayerWaterPartHistory[(sampleIndex)].vx, trackedPart->workm.t[0]) + ABS_DIFF(gNeoArkWoodlandPathPlayerWaterPartHistory[(sampleIndex)].vy, trackedPart->workm.t[1]) + ABS_DIFF(gNeoArkWoodlandPathPlayerWaterPartHistory[(sampleIndex)].vz, trackedPart->workm.t[2]) + WATER_SPLASH_RIPPLE_ODDS_BIAS; \
    surfaceCoord.parent       = playerRoot->parent;                                                                                                                                                                                                                                                                                                               \
    surfaceCoord.coord        = playerRoot->coord;                                                                                                                                                                                                                                                                                                                \
    surfaceCoord.coord.t[0]   = playerRoot->coord.t[0];                                                                                                                                                                                                                                                                                                           \
    surfaceCoord.coord.t[1]   = WATER_SPLASH_SURFACE_Y;                                                                                                                                                                                                                                                                                                           \
    surfaceCoord.coord.t[2]   = playerRoot->coord.t[2];                                                                                                                                                                                                                                                                                                           \
    surfaceCoord.composeStamp = GRAPHICS_COORD_DIRTY;                                                                                                                                                                                                                                                                                                             \
    actorRenderComposeCoord(&surfaceCoord);                                                                                                                                                                                                                                                                                                                       \
    RANDOM_LCG_ADVANCE();                                                                                                                                                                                                                                                                                                                                         \
    if ((s32)(RANDOM_LCG_DRAW() & WATER_SPLASH_ROLL_MASK) < work->angle) {                                                                                                                                                                                                                                                                                        \
        effectSpawn(gRoomEffectWaterRippleId, &surfaceCoord, WATER_SPLASH_RIPPLE_HALF_SIDE, 0);                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                                                             \
    work->angle -= WATER_SPLASH_RIPPLE_ODDS_BIAS;                                                                                                                                                                                                                                                                                                                 \
    RANDOM_LCG_ADVANCE();                                                                                                                                                                                                                                                                                                                                         \
    if ((s32)(RANDOM_LCG_DRAW() & WATER_SPLASH_ROLL_MASK) < work->angle) {                                                                                                                                                                                                                                                                                        \
        effectSpawn(gRoomEffectWaterSprayId, &surfaceCoord, WATER_SPLASH_SPRAY_SPAWN_ARG, 0);                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                             \
    gNeoArkWoodlandPathPlayerWaterPartHistory[(sampleIndex)].vx = trackedPart->workm.t[0];                                                                                                                                                                                                                                                                        \
    gNeoArkWoodlandPathPlayerWaterPartHistory[(sampleIndex)].vy = trackedPart->workm.t[1];                                                                                                                                                                                                                                                                        \
    gNeoArkWoodlandPathPlayerWaterPartHistory[(sampleIndex)].vz = trackedPart->workm.t[2];

    work       = task->spawnArg2.pointer;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerRoot = playerTask->extra.tmd->coords;
    // Start from the current composed part positions, with no initial movement.
    if (task->state == WATER_SPLASH_STATE_NEW) {
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_WOODLAND_PATH_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_WOODLAND_PATH_WATER_SPRAY;
        task->state              = WATER_SPLASH_STATE_ACTIVE;
        for (sampleIndex = 0; sampleIndex < WATER_SPLASH_SAMPLE_COUNT; sampleIndex++) {
            trackedPart                                               = &playerTask->extra.tmd->coords[sampleIndex * WATER_SPLASH_PART_STRIDE + WATER_SPLASH_FIRST_PART];
            gNeoArkWoodlandPathPlayerWaterPartHistory[sampleIndex].vx = trackedPart->workm.t[0];
            gNeoArkWoodlandPathPlayerWaterPartHistory[sampleIndex].vy = trackedPart->workm.t[1];
            gNeoArkWoodlandPathPlayerWaterPartHistory[sampleIndex].vz = trackedPart->workm.t[2];
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
#undef ABS_DIFF
}

#include "../../shared/effects/falling_leaves_task.inc.c"

void neoArkWoodlandPathLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/effects/falling_leaves_draw_neo_ark.inc.c"

#include "../../shared/effects/water_ripple_task.inc.c"

void neoArkWoodlandPathWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task_u16.inc.c"

void neoArkWoodlandPathWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/effects/water_spin_u16.inc.c"

#include "../../shared/room/roaming_enemies_bank_retreat.inc.c"

/// Pool-A controller states: 0 arm woodland reserve, 1 update, 2 advance, 3 release.
///
/// The dispatcher copies all four callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check. Both pools share the room's reserve
/// storage; keep the room and callbacks loaded throughout controller lifetime.
static const TaskFuncTable4 _gNeoArkWoodlandPathRoamerPoolAStateHandlers = {
    { _roamerArmPoolA, _roamerTickPoolA,
      _neoArkWoodlandPathAdvanceRoamerPoolAState, taskKill }
};

#include "../../shared/room/roaming_enemies.inc.c"

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
/// table (X/Z in whole room units, yaw at 4096 units per turn), and requires a
/// live revived model. Each lookup is retained, including after the yaw call.
/// Expands to statements for a braced switch arm; no actor pointer is retained.
#define ROAMER_PLACE_POOL_B_ENEMY(actorIndex, pointIndex, dirtyBeforeTurn)                                                      \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[0] = _gNeoArkWoodlandPathRoamerPoolBSpawnPoints[pointIndex].x; \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[1] = 0;                                                        \
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[2] = _gNeoArkWoodlandPathRoamerPoolBSpawnPoints[pointIndex].z; \
    if (dirtyBeforeTurn) {                                                                                                      \
        sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                               \
    }                                                                                                                           \
    gfxRotMatrixY(&sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord,                                                  \
                  _gNeoArkWoodlandPathRoamerPoolBSpawnPoints[pointIndex].yaw, GRAPHICS_ROTATION_REPLACE);                       \
    if (!(dirtyBeforeTurn)) {                                                                                                   \
        sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                               \
    }

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (_gRoamerPoolBArmCounts[gGameSession->location.variant] == 0) {
        return;
    }
    if (_gRoamerCooldownFrames > ROAMER_COOLDOWN_READY) {
        _gRoamerCooldownFrames--;
    }
    // Persist the reserve only on the transition out of a live battle.
    if (gSceneCombatState.battleRefs == 0 && _gRoamerPreviousBattleRefs > 0) {
        persistedReserve = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        bankedCount      = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if (gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", persistedReserve - bankedCount);
        previousKills    = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B);
        persistedReserve = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
        bankedCount      = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if (gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B, previousKills + (persistedReserve - bankedCount));
        bankedCount = 0;
        for (reserveSlot = 0; reserveSlot < ROAMER_POOL_B_RESERVE_SLOTS; reserveSlot++) {
            if (gRoamerReserveHp[reserveSlot] > 0) {
                bankedCount++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, bankedCount);
        areaSyncLocationVariant(&gGameSession->location);
        _gRoamerCooldownFrames = ROAMER_POST_BATTLE_COOLDOWN_FRAMES;
    }
    _gRoamerPreviousBattleRefs = gSceneCombatState.battleRefs;
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
                for (reserveIndex = 0; reserveIndex < _gRoamerReserveCount; reserveIndex++) {
                    if (gRoamerReserveHp[reserveIndex] > 0) {
                        enemy->hp                      = (u16)gRoamerReserveHp[reserveIndex];
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

/// Pool-B controller states: 0 arm forest reserve, 1 update, 2 advance, 3 release.
///
/// The dispatcher copies all four callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check. Both pools share the room's reserve
/// storage; keep the room and callbacks loaded throughout controller lifetime.
static const TaskFuncTable4 _gNeoArkWoodlandPathRoamerPoolBStateHandlers = {
    { _roamerArmPoolB, _roamerTickPoolB,
      _neoArkWoodlandPathAdvanceRoamerPoolBState, taskKill }
};

/// Ignores actor commands delivered to the pool-A controller and returns zero.
///
/// Serves `ACTOR_COMMAND_MESSAGE_APPLY` in `_gRoamerPoolAMessages`. All arguments
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
#include "../../shared/room/roaming_enemies_latch_request.inc.c"
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

    handlers = _gNeoArkWoodlandPathRoamerPoolAStateHandlers;
    handlers.funcs[task->state](task);
}

#include "../../shared/room/roaming_enemies_extend_cooldown.inc.c"

/// Selects the private pool-B room-action latch for this fragment inclusion.
#define ROAMER_LATCH_SPAWN_REQUEST _roamerLatchSpawnRequestPoolB
#include "../../shared/room/roaming_enemies_latch_request.inc.c"
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

    handlers = _gNeoArkWoodlandPathRoamerPoolBStateHandlers;
    handlers.funcs[task->state](task);
}
