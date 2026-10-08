#include "actor_342400_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/mad_chaser_waves.h"
#include "../../shared/mad_chaser.h"

/// Which coordinate a `_Actor342400CullZone` bounds, and from which side.
enum {
    ACTOR_342400_CULL_ZONE_AXIS_X = 0, // The zone is every x below the limit
    ACTOR_342400_CULL_ZONE_AXIS_Z = 1, // The zone is every z at or above the limit (any nonzero value)
};

/// Model palette offsets and synthetic command context for encounter Sucklercephs.
enum {
    OVERLAY_ENCOUNTER_PAIR_TEXTURE_PAGE_OFFSET = 3,
    OVERLAY_ENCOUNTER_PAIR_CLUT_ROW_OFFSET     = 5,
    OVERLAY_ENCOUNTER_PAIR_COMMAND_STAGE       = 0,
    OVERLAY_ENCOUNTER_PAIR_COMMAND_AREA        = 0x2E
};

/// One region of the room in which the wave spawners kill their enemies.
///
/// `GameSession::enemyCullZone` selects the region in force, 1..16, or 0 for
/// none. A region is everything to one side of a line across the room: a
/// spawner tests its living enemy's position against the line, and once the
/// enemy is inside orders it to die and stops watching it.
typedef struct {
    s16 axis;  // Coordinate the line crosses (ACTOR_342400_CULL_ZONE_AXIS_*)
    s16 limit; // Where the line crosses it, in world units
} _Actor342400CullZone;
STATIC_ASSERT_SIZEOF(_Actor342400CullZone, 0x4);

extern TaskDesc D_actor_207000_801575F0; // absolute, spawned by _actor342400WaveSpawnSlouch
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry     D_actor_342400_8016BF48[2]; // stored into `Task::msgTable` by _actor342400WaveInitialize
extern OverlayEncounterSlot gMadChaserWaveSlots[];
extern TaskDesc             D_actor_342400_8016BFE0[];
extern _Actor342400CullZone D_actor_342400_8016C010[];
extern s16                  D_actor_342400_8016C054[][4]; // spawn variant per player-position band, 4 random picks

                                                          // spawn counter, `<< 12` into `Enemy::placeKey`

static s16  _actor342400PickEncounterSpot(void);
static s16  _actor342400InCullZone(s16 zoneId, s16 worldX, s16 worldZ);
static void _actor342400WaveInitialize(Task* controllerTask);
static void _actor342400WaveArmBattle(Task* controllerTask);
static void _actor342400WaveRun(Task* controllerTask);
static void _actor342400WaveSpawnMadChaser(Task* waveTask);
static void _actor342400WaveRevealMadChaser(Task* waveTask);
static void _actor342400WaveBeginMadChaserWatch(Task* waveTask);
static void _actor342400WaveWatchMadChaser(Task* waveTask);
static void _actor342400WaveSpawnSlouch(Task* waveTask);
static void _actor342400WaveBeginSlouchWatch(Task* waveTask);
static void _actor342400WaveWatchSlouch(Task* waveTask);
static void _actor342400WaveBeginPairReveal(Task* waveTask);
static void _overlayEncounterRevealSlouch(Task* waveTask);
static void _overlayEncounterPairRevealFirst(Task* waveTask);
static void _overlayEncounterPairRevealSecond(Task* waveTask);
static void _overlayEncounterPairWatch(Task* waveTask);

static void _actor342400WaveControllerTask(Task* controllerTask);
static void _overlayEncounterForgetDeadPairMembers(Task* task);
static void _actor342400WaveCullPair(Task* waveTask);
static void _actor342400WaveStopMessage(Task* controllerTask, s32 messageId, const ActorCommand* request, s32 unusedSecondArg);

TaskMessageEntry D_actor_342400_8016BF48[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor342400WaveStopMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

OverlayEncounterSlot gMadChaserWaveSlots[17] = {
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 1), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 2), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 1, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 1), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 0, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 2), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { 2, OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, 0), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
};

static void _actor342400WaveMadChaserTask(Task* waveTask);
static void _actor342400WaveSlouchTask(Task* waveTask);
static void _actor342400WavePairTask(Task* waveTask);

TaskDesc D_actor_342400_8016BFE0[4] = {
    { { { TASK_BODY_NONE, 32 } }, _actor342400WaveControllerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _actor342400WaveMadChaserTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _actor342400WaveSlouchTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _actor342400WavePairTask, { .value = 0 } },
};

_Actor342400CullZone D_actor_342400_8016C010[17] = {
    { ACTOR_342400_CULL_ZONE_AXIS_X, 0 }, // 0 selects no zone, so this row is never read
    { ACTOR_342400_CULL_ZONE_AXIS_X, 1295 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 2700 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 4300 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 5700 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 7295 },
    { ACTOR_342400_CULL_ZONE_AXIS_X, 8700 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -7800 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -9210 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -10805 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -12210 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -15760 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -17170 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -18825 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -20235 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -23800 },
    { ACTOR_342400_CULL_ZONE_AXIS_Z, -25205 },
};

s16 D_actor_342400_8016C054[6][4] = {
    { 2, 3, 11, 12 },
    { 3, 4, 12, 5 },
    { 5, 6, 7, 13 },
    { 7, 8, 13, 14 },
    { 8, 9, 14, 15 },
    { 9, 15, 9, 15 },
};

static TmdBone _gActor342400MadChaserBurstHeadSkeleton[1] = {
#include "assets/mad_chaser_burst_head_skeleton.inc"
};

static u32 _gActor342400MadChaserBurstHeadPartVerts[1] = {
#include "assets/mad_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBurstHeadVerts[48] = {
#include "assets/mad_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor342400MadChaserBurstHeadNormals[53] = {
#include "assets/mad_chaser_burst_head_normals.inc"
};

static u32 _gActor342400MadChaserBurstHeadStream[486] = {
#include "assets/mad_chaser_burst_head_stream.inc"
};

TmdSource gMadChaserChunkModel0 = {
    0,
    3260,
    0,
    1,
    _gActor342400MadChaserBurstHeadPartVerts,
    _gActor342400MadChaserBurstHeadVerts,
    _gActor342400MadChaserBurstHeadNormals,
    _gActor342400MadChaserBurstHeadSkeleton,
    _gActor342400MadChaserBurstHeadStream,
};

static TmdBone _gActor342400MadChaserBurstArmSkeleton[1] = {
#include "assets/mad_chaser_burst_arm_skeleton.inc"
};

static u32 _gActor342400MadChaserBurstArmPartVerts[1] = {
#include "assets/mad_chaser_burst_arm_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBurstArmVerts[28] = {
#include "assets/mad_chaser_burst_arm_verts.inc"
};

static SVECTOR _gActor342400MadChaserBurstArmNormals[37] = {
#include "assets/mad_chaser_burst_arm_normals.inc"
};

static u32 _gActor342400MadChaserBurstArmStream[276] = {
#include "assets/mad_chaser_burst_arm_stream.inc"
};

TmdSource gMadChaserChunkModel1 = {
    0,
    1828,
    0,
    1,
    _gActor342400MadChaserBurstArmPartVerts,
    _gActor342400MadChaserBurstArmVerts,
    _gActor342400MadChaserBurstArmNormals,
    _gActor342400MadChaserBurstArmSkeleton,
    _gActor342400MadChaserBurstArmStream,
};

static TmdBone _gActor342400MadChaserBurstTailSkeleton[1] = {
#include "assets/mad_chaser_burst_tail_skeleton.inc"
};

static u32 _gActor342400MadChaserBurstTailPartVerts[1] = {
#include "assets/mad_chaser_burst_tail_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBurstTailVerts[25] = {
#include "assets/mad_chaser_burst_tail_verts.inc"
};

static SVECTOR _gActor342400MadChaserBurstTailNormals[32] = {
#include "assets/mad_chaser_burst_tail_normals.inc"
};

static u32 _gActor342400MadChaserBurstTailStream[215] = {
#include "assets/mad_chaser_burst_tail_stream.inc"
};

TmdSource gMadChaserChunkModel2 = {
    0,
    1448,
    0,
    1,
    _gActor342400MadChaserBurstTailPartVerts,
    _gActor342400MadChaserBurstTailVerts,
    _gActor342400MadChaserBurstTailNormals,
    _gActor342400MadChaserBurstTailSkeleton,
    _gActor342400MadChaserBurstTailStream,
};

static TmdBone _gActor342400MadChaserBodySkeleton[9] = {
#include "assets/mad_chaser_body_skeleton.inc"
};

static u32 _gActor342400MadChaserBodyPartVerts[9] = {
#include "assets/mad_chaser_body_partVerts.inc"
};

static SVECTOR _gActor342400MadChaserBodyVerts[160] = {
#include "assets/mad_chaser_body_verts.inc"
};

static SVECTOR _gActor342400MadChaserBodyNormals[206] = {
#include "assets/mad_chaser_body_normals.inc"
};

static u32 _gActor342400MadChaserBodyStream[2105] = {
#include "assets/mad_chaser_body_stream.inc"
};

TmdSource gActor342400MadChaserBody = {
    0,
    11212,
    3088,
    9,
    _gActor342400MadChaserBodyPartVerts,
    _gActor342400MadChaserBodyVerts,
    _gActor342400MadChaserBodyNormals,
    _gActor342400MadChaserBodySkeleton,
    _gActor342400MadChaserBodyStream,
};

DamageAttack D_actor_342400_80170584[1] = {
    { 22, 0 },
};

EnemyParams gMadChaserEnemyParams = { D_actor_342400_80170584, 110, 20, 40, 1, 100, 10, 100, 0 };

static void _actor342400SpawnEncounterSlot(s16 slotIndex, s16 enemyKind, s16 command);

#include "../../shared/mad_chaser_waves_pair_spawn.inc.c"

/// Releases dead or culled Sucklercephs from a pair spawner's borrowed pointers.
///
/// Requires initialized pair work and live enemy tasks for non-null pointers.
/// Cull tests narrow the root's world X/Z to signed halfwords. A culled enemy
/// receives the puffing command; its gone bit is set on the following call,
/// after the pointer has already been cleared. Enemy teardown remains separate.
static void _actor342400WaveCullPair(Task* waveTask)
{
    enum { ACTOR_342400_WAVE_SUCKLERCEPH_RESTART_PUFFING = 5 };
    OverlayEncounterPairWork* work = waveTask->work;
    Enemy*                    enemy;
    Task*                     enemyTask;
    GfxCoord*                 enemyCoord;
    ActorCommand              request;

    // Tests and releases one pair member, then records its gone bit a tick later.
    // enemyMember is an Enemy* member token of work; goneBit is an s16-compatible
    // bit expression evaluated once only on the null-pointer arm. Use standalone.
    // Captures work,
    // enemy, enemyTask, enemyCoord and request, which are scratch locals above.
#define ACTOR_342400_WAVE_CULL_PAIR_ENEMY(enemyMember, goneBit)                                                           \
    if (work->enemyMember != NULL) {                                                                                      \
        enemy      = work->enemyMember;                                                                                   \
        enemyTask  = enemy->task;                                                                                         \
        enemyCoord = enemyTask->extra.tmd->coords;                                                                        \
        if (enemy->hp <= 0) {                                                                                             \
            work->enemyMember = NULL;                                                                                     \
        } else if (_actor342400InCullZone(gGameSession->enemyCullZone, enemyCoord->coord.t[0], enemyCoord->coord.t[2])) { \
            request.context.loc.stage = 0;                                                                                \
            request.context.loc.area  = 0;                                                                                \
            request.command           = ACTOR_342400_WAVE_SUCKLERCEPH_RESTART_PUFFING;                                    \
            TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);                           \
            work->enemyMember = NULL;                                                                                     \
        }                                                                                                                 \
    } else {                                                                                                              \
        work->goneMask |= (goneBit);                                                                                      \
    }

    ACTOR_342400_WAVE_CULL_PAIR_ENEMY(enemy0, OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0);
    ACTOR_342400_WAVE_CULL_PAIR_ENEMY(enemy1, OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1);
#undef ACTOR_342400_WAVE_CULL_PAIR_ENEMY
}

/// Counts encounter rows whose spawner has made an enemy live.
static inline s16 _actor342400WaveCountLiveSlots(void)
{
    s16 liveSlotCount = 0;
    s16 slotIndex;

    for (slotIndex = 0; slotIndex < (s32)ARRAY_SIZE(gMadChaserWaveSlots); slotIndex++) {
        if (gMadChaserWaveSlots[slotIndex].status == OVERLAY_ENCOUNTER_SLOT_LIVE) {
            liveSlotCount++;
        }
    }
    return liveSlotCount;
}

/// Starts at most one later encounter row when fewer than three rows are live.
///
/// Requires initialized controller work, the loaded incinerator descriptors and
/// a live player model. `nextSlot` is a nonnegative row cursor through the full
/// slot table; spawning requires at least 61 frames remaining on the scene clock.
/// Each recognized kind consumes one random spot pick and packs the row into
/// the high spawn halfword.
/// Advances the cursor even for an unknown kind or a failed spawn; this call
/// does not mark the row live, so pending spawners are excluded from the limit.
static void _actor342400WaveFeed(Task* controllerTask)
{
    enum {
        ACTOR_342400_WAVE_LIVE_SLOT_LIMIT            = 3,
        ACTOR_342400_WAVE_FEED_MIN_SCENE_CLOCK       = 61,
        OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER       = 0,
        OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH           = 1,
        OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR = 2,
        ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR   = 1,
        ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR       = 2,
        ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR         = 3,
        OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT           = 16,
        OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT           = 8,
        ACTOR_342400_SIGNED_HALFWORD_SHIFT           = 16,
    };
    OverlayEncounterControllerWork* work = controllerTask->work;
    s16                             liveSlotCount;
    s16                             nextSlotIndex;
    s16                             enemyKind;
    s16                             command;

    liveSlotCount = _actor342400WaveCountLiveSlots();
    if (liveSlotCount < ACTOR_342400_WAVE_LIVE_SLOT_LIMIT) {
        nextSlotIndex = work->nextSlot;
        if (nextSlotIndex < (s32)ARRAY_SIZE(gMadChaserWaveSlots) && gGameSession->sceneClock >= ACTOR_342400_WAVE_FEED_MIN_SCENE_CLOCK) {
            enemyKind = gMadChaserWaveSlots[nextSlotIndex].kind;
            command   = gMadChaserWaveSlots[nextSlotIndex].command;
            // Preserve the signed-halfword spot expansion before adding command bits.
            switch (enemyKind) {
                case OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER:
                    taskSpawnFromTable(D_actor_342400_8016BFE0, ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR, (nextSlotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << ACTOR_342400_SIGNED_HALFWORD_SHIFT >> (ACTOR_342400_SIGNED_HALFWORD_SHIFT - OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT)), 0);
                    break;
                case OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH:
                    taskSpawnFromTable(D_actor_342400_8016BFE0, ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR, (nextSlotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << ACTOR_342400_SIGNED_HALFWORD_SHIFT >> (ACTOR_342400_SIGNED_HALFWORD_SHIFT - OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT)), 0);
                    break;
                case OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR:
                    taskSpawnFromTable(D_actor_342400_8016BFE0, ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR, (nextSlotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << ACTOR_342400_SIGNED_HALFWORD_SHIFT >> (ACTOR_342400_SIGNED_HALFWORD_SHIFT - OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT)), 0);
                    break;
            }
            work->nextSlot++;
        }
    }
}

/// Picks an incinerator encounter spot from the live player's position band.
///
/// Requires a live player TMD root. World X/Z are narrowed to signed halfwords;
/// the six bands each supply four spot indices (2..15). Advances the shared
/// LCG exactly once and selects a column with bits 16..17. The caller packs
/// the result into command bits 8..11 for the room's encounter-spot table.
static s16 _actor342400PickEncounterSpot(void)
{
    enum {
        ACTOR_342400_ENCOUNTER_RANDOM_SHIFT = 16,
        ACTOR_342400_ENCOUNTER_PICK_MASK    = ARRAY_SIZE(D_actor_342400_8016C054[0]) - 1,
    };
    GfxCoord* playerCoord = (*gPlayerActorTasks)->extra.tmd->coords;
    s16       playerX     = playerCoord->coord.t[0];
    s16       playerZ     = playerCoord->coord.t[2];

    if (playerX <= 5000) {
        return D_actor_342400_8016C054[0][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerX <= 8000) {
        return D_actor_342400_8016C054[1][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerX <= 11000) {
        return D_actor_342400_8016C054[2][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -5500) {
        return D_actor_342400_8016C054[3][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -8500) {
        return D_actor_342400_8016C054[4][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -11500) {
        return D_actor_342400_8016C054[5][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -24500) {
        return D_actor_342400_8016C054[5][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    return D_actor_342400_8016C054[5][(gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
}

/// Latches a stop command that suspends the controller's feeding stage.
///
/// Borrows `request` through synchronous dispatch; ignores its context and all
/// commands except `OVERLAY_ENCOUNTER_COMMAND_STOP`. The two unused ABI words
/// are retained, and callers must ignore the dispatch result: this handler
/// returns no value. Stopping neither kills the controller nor releases battle.
static void _actor342400WaveStopMessage(Task* controllerTask, s32 messageId, const ActorCommand* request, s32 unusedSecondArg)
{
    OverlayEncounterControllerWork* work      = controllerTask->work;
    u16                             commandId = request->command;

    if (commandId == OVERLAY_ENCOUNTER_COMMAND_STOP) {
        work->stop = commandId;
    }
}

/// Tests signed-halfword world X/Z against the selected incinerator cull region.
///
/// `zoneId` must be nonnegative, as it is at callers using the session byte.
/// Zero or a value beyond the table returns 0; rows 1..16 return 1 for X below
/// an X limit or Z at/above a Z limit. This tests the region to cull, rather
/// than the region in which an enemy may remain.
static s16 _actor342400InCullZone(s16 zoneId, s16 worldX, s16 worldZ)
{
    if (zoneId == 0 || zoneId > (s32)ARRAY_SIZE(D_actor_342400_8016C010) - 1) {
        return 0;
    }
    if (D_actor_342400_8016C010[zoneId].axis == ACTOR_342400_CULL_ZONE_AXIS_X) {
        if (worldX < D_actor_342400_8016C010[zoneId].limit) {
            return 1;
        }
    } else {
        if (worldZ >= D_actor_342400_8016C010[zoneId].limit) {
            return 1;
        }
    }
    return 0;
}

/// The controller task's four state handlers, dispatched by
/// `_actor342400WaveControllerTask` on `Task::state`.
static const TaskFuncTable4 D_actor_342400_80161E24 = { {
    _actor342400WaveInitialize,
    madChaserWaveOpen,
    _actor342400WaveArmBattle,
    _actor342400WaveRun,
} };

/// Dispatches the encounter controller while scene actors are running.
///
/// State must be 0..3: initialize, open the first slots, arm battle, then feed
/// and finish the encounter. Paused or hidden actors leave the controller idle.
static void _actor342400WaveControllerTask(Task* controllerTask)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_342400_80161E24;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[controllerTask->state](controllerTask);
    }
}

/// The first spawner task's four state handlers, dispatched by
/// `_actor342400WaveMadChaserTask`.
static const TaskFuncTable4 D_actor_342400_80161E34 = { {
    _actor342400WaveSpawnMadChaser,
    _actor342400WaveRevealMadChaser,
    _actor342400WaveBeginMadChaserWatch,
    _actor342400WaveWatchMadChaser,
} };

/// Dispatches one Mad Chaser slot's spawn, reveal, transition or watch stage.
///
/// State must be 0..3. The spawn argument carries a slot index 0..16 in its
/// high halfword and the packed emerge command in its low halfword.
static void _actor342400WaveMadChaserTask(Task* waveTask)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_342400_80161E34;
    handlers.funcs[waveTask->state](waveTask);
}

/// The second spawner task's four state handlers, dispatched by
/// `_actor342400WaveSlouchTask`.
static const TaskFuncTable4 D_actor_342400_80161E44 = { {
    _actor342400WaveSpawnSlouch,
    _overlayEncounterRevealSlouch,
    _actor342400WaveBeginSlouchWatch,
    _actor342400WaveWatchSlouch,
} };

/// Dispatches one Slouch slot's spawn, reveal, transition or watch stage.
///
/// State must be 0..3. The spawn argument carries a slot index 0..16 in its
/// high halfword and the packed emerge command in its low halfword. Dispatch
/// continues independently of the controller's scene-actor pause gate.
static void _actor342400WaveSlouchTask(Task* waveTask)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_342400_80161E44;
    handlers.funcs[waveTask->state](waveTask);
}

/// The five state handlers `_actor342400WavePairTask` dispatches through by
/// `Task::state`.
static const TaskFuncTable5 D_actor_342400_80161E54 = { {
    madChaserWavePairSpawn,
    _actor342400WaveBeginPairReveal,
    _overlayEncounterPairRevealFirst,
    _overlayEncounterPairRevealSecond,
    _overlayEncounterPairWatch,
} };

/// Dispatches a Sucklerceph pair's spawn, transition, two reveals or watch stage.
///
/// State must be 0..4. The spawn argument carries a slot index 0..16 in its
/// high halfword and the packed emerge command in its low halfword. One failed
/// enemy spawn is tolerated by the pair handlers; they borrow surviving enemies.
/// Dispatch continues independently of the controller's scene-actor pause gate.
static void _actor342400WavePairTask(Task* waveTask)
{
    TaskFuncTable5 handlers;

    handlers = D_actor_342400_80161E54;
    handlers.funcs[waveTask->state](waveTask);
}

/// Initializes the incinerator's encounter controller or kills an ineligible task.
///
/// Requires the earlier spawn phase to have started and this phase not to be
/// complete. Allocates zeroed primary-heap work owned by task teardown, resets
/// every slot and the enemy counter, and installs the stop-message table.
static void _actor342400WaveInitialize(Task* controllerTask)
{
    OverlayEncounterControllerWork* work;
    s32                             slotIndex;

    if (gGameSession->spawnPhase[1] == GAME_SESSION_SPAWN_COMPLETE || gGameSession->spawnPhase[0] == GAME_SESSION_SPAWN_IDLE ||
        (work = memCalloc(sizeof(*work), false)) == NULL) {
        taskKill(controllerTask);
        return;
    }
    for (slotIndex = (s32)ARRAY_SIZE(gMadChaserWaveSlots) - 1; slotIndex >= 0; slotIndex--) {
        gMadChaserWaveSlots[slotIndex].status = OVERLAY_ENCOUNTER_SLOT_WAITING;
    }
    gMadChaserWaveEnemyCount = 0;
    controllerTask->work     = work;
    controllerTask->msgTable = D_actor_342400_8016BF48;
    controllerTask->state++;
}

/// Selects the private incinerator row spawner for the encounter opening.
///
/// Requires a declared `void (s16 slotIndex, s16 enemyKind, s16 command)`
/// callback and its live descriptor/placement resources. The opening fragment
/// evaluates each argument once and undefines this identifier binding.
#define OVERLAY_ENCOUNTER_SPAWN_SLOT _actor342400SpawnEncounterSlot
#include "../../shared/mad_chaser_waves_open.inc.c"

/// Arms the encounter on its fifteenth controller tick after the opening slots.
///
/// Requires initialized controller work with a zeroed frame count. Acquires
/// the battle reference, records the armed spawn phase and advances to feeding.
static void _actor342400WaveArmBattle(Task* controllerTask)
{
    enum { ACTOR_342400_WAVE_ARM_DELAY_FRAMES = 15 };
    OverlayEncounterControllerWork* work = controllerTask->work;

    if (++work->frames == ACTOR_342400_WAVE_ARM_DELAY_FRAMES) {
        sceneAcquireBattleRef(0);
        gGameSession->spawnPhase[1] = GAME_SESSION_SPAWN_ARMED;
        sceneEngageBattle(1);
        controllerTask->state++;
    }
}

/// Feeds the incinerator encounter and releases its battle hold when every row is done.
///
/// Requires initialized controller work and the live slot table. A stop command
/// suspends both feeding and completion without releasing the controller. On
/// normal completion clears rewards, records spawn phase 1 as complete and kills
/// the controller, whose teardown releases its work. Does not kill enemy tasks.
static void _actor342400WaveRun(Task* controllerTask)
{
    OverlayEncounterControllerWork* work = controllerTask->work;
    s16                             doneSlotCount;
    s32                             slotIndex;

    doneSlotCount = 0;
    if (work->stop != OVERLAY_ENCOUNTER_COMMAND_STOP) {
        _actor342400WaveFeed(controllerTask);
        for (slotIndex = 0; slotIndex < (s32)ARRAY_SIZE(gMadChaserWaveSlots); slotIndex++) {
            if (gMadChaserWaveSlots[slotIndex].status == OVERLAY_ENCOUNTER_SLOT_DONE) {
                doneSlotCount++;
            }
        }
        if (doneSlotCount == (s32)ARRAY_SIZE(gMadChaserWaveSlots)) {
            sceneReleaseBattleRefAndClearRewards(controllerTask, 0);
            gGameSession->spawnPhase[1] = GAME_SESSION_SPAWN_COMPLETE;
            taskKill(controllerTask);
        }
    }
}

/// Spawns the hidden Mad Chaser for an encounter slot and begins its reveal delay.
///
/// The high spawn halfword must name a slot 0..16. Owns zeroed primary-heap
/// work through task teardown and borrows the enemy from its own task. Marks
/// the slot live only on success; failure kills this task and leaves it waiting.
static void _actor342400WaveSpawnMadChaser(Task* waveTask)
{
    enum { ACTOR_342400_WAVE_MAD_CHASER_HIDDEN_TASK = 1 };
    OverlayEncounterSingleWork* work;
    Enemy*                      enemy;

    work = memCalloc(sizeof(*work), false);
    if (work != NULL) {
        // Attach the work before spawning so task teardown also handles failure.
        waveTask->work = work;
        enemy          = enemySpawnFromTable(D_actor_342400_80173A54, ACTOR_342400_WAVE_MAD_CHASER_HIDDEN_TASK, 0, NULL);
        if (enemy != NULL) {
            gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            work->enemy                                                        = enemy;
            enemy->placeKey                                                    = gMadChaserWaveEnemyCount << ENEMY_PLACE_INDEX_SHIFT;
            gMadChaserWaveEnemyCount++;
            waveTask->state++;
            return;
        }
    }
    taskKill(waveTask);
}

/// Reveals the slot's Mad Chaser once its frame count exceeds sixty ticks.
///
/// Requires a successfully spawned enemy. Selects its wave palette, marks it
/// as a dynamic enemy and synchronously sends the low spawn halfword as its
/// emerge command in synthetic stage 0/area 44. Then advances out of the delay.
static void _actor342400WaveRevealMadChaser(Task* waveTask)
{
    enum {
        ACTOR_342400_WAVE_REVEAL_DELAY_FRAMES     = 60,
        ACTOR_342400_WAVE_MAD_CHASER_CLUT_ROW     = 2,
        ACTOR_342400_WAVE_MAD_CHASER_TEXTURE_PAGE = 0,
        ACTOR_342400_WAVE_MAD_CHASER_COMMAND_AREA = 0x2C,
    };
    OverlayEncounterSingleWork* work = waveTask->work;
    Enemy*                      enemy;
    Task*                       enemyTask;
    TmdObject*                  model;
    ActorCommand                request;

    enemy     = work->enemy;
    enemyTask = enemy->task;
    if (++work->frames > ACTOR_342400_WAVE_REVEAL_DELAY_FRAMES) {
        // Select the wave appearance before the command makes the enemy visible.
        model                     = enemyTask->extra.tmd;
        model->clutRowOffset      = ACTOR_342400_WAVE_MAD_CHASER_CLUT_ROW;
        model->texturePageOffset  = ACTOR_342400_WAVE_MAD_CHASER_TEXTURE_PAGE;
        enemy->workType           = ENEMY_WORK_PLAIN;
        request.context.loc.stage = 0;
        request.context.loc.area  = ACTOR_342400_WAVE_MAD_CHASER_COMMAND_AREA;
        request.command           = waveTask->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        waveTask->state++;
    }
}

/// Advances the Mad Chaser spawner from state 2 to watching on the next tick.
static void _actor342400WaveBeginMadChaserWatch(Task* waveTask)
{
    waveTask->state++;
}

/// Completes a Mad Chaser slot when its enemy is dead or inside the cull region.
///
/// Requires live single-enemy work and a slot index 0..16 in the high spawn
/// halfword. A living culled enemy receives shrink death; this spawner marks
/// the slot done and ends immediately, leaving the enemy to finish its death.
static void _actor342400WaveWatchMadChaser(Task* waveTask)
{
    enum { ACTOR_342400_WAVE_MAD_CHASER_COMMAND_AREA = 0x2C };
    OverlayEncounterSingleWork* work = waveTask->work;
    Enemy*                      enemy;
    Task*                       enemyTask;
    GfxCoord*                   enemyCoord;
    ActorCommand                request;

    enemy      = work->enemy;
    enemyTask  = enemy->task;
    enemyCoord = enemyTask->extra.tmd->coords;
    if (enemy->hp <= 0) {
        gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
        return;
    }
    if (_actor342400InCullZone(gGameSession->enemyCullZone, enemyCoord->coord.t[0], enemyCoord->coord.t[2])) {
        request.context.loc.stage = 0;
        request.context.loc.area  = ACTOR_342400_WAVE_MAD_CHASER_COMMAND_AREA;
        request.command           = MAD_CHASER_COMMAND_SHRINK_DEATH;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
    }
}

/// Spawns the hidden Slouch for a slot, selects its palette and begins its delay.
///
/// The high spawn halfword must name a slot 0..16. Owns zeroed primary-heap
/// work through task teardown and borrows the enemy from its own task. Marks
/// the slot live only on success; failure kills this task and leaves it waiting.
static void _actor342400WaveSpawnSlouch(Task* waveTask)
{
    enum {
        ACTOR_342400_WAVE_SLOUCH_HIDDEN_TASK  = 2,
        ACTOR_342400_WAVE_SLOUCH_TEXTURE_PAGE = 2,
        ACTOR_342400_WAVE_SLOUCH_CLUT_ROW     = 4,
    };
    OverlayEncounterSingleWork* work;
    Enemy*                      enemy;
    TmdObject*                  model;

    work = memCalloc(sizeof(*work), false);
    if (work != NULL) {
        // Attach the work before spawning so task teardown also handles failure.
        waveTask->work = work;
        enemy          = enemySpawnFromTable(&D_actor_207000_801575F0, ACTOR_342400_WAVE_SLOUCH_HIDDEN_TASK, 0, NULL);
        if (enemy != NULL) {
            gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            work->enemy                                                        = enemy;
            enemy->placeKey                                                    = gMadChaserWaveEnemyCount << ENEMY_PLACE_INDEX_SHIFT;
            gMadChaserWaveEnemyCount++;
            model                    = enemy->task->extra.tmd;
            model->texturePageOffset = ACTOR_342400_WAVE_SLOUCH_TEXTURE_PAGE;
            model->clutRowOffset     = ACTOR_342400_WAVE_SLOUCH_CLUT_ROW;
            waveTask->state++;
            return;
        }
    }
    taskKill(waveTask);
}

#include "../../shared/mad_chaser_waves_reveal_second.inc.c"

/// Advances the Slouch spawner from state 2 to watching on the next tick.
static void _actor342400WaveBeginSlouchWatch(Task* waveTask)
{
    waveTask->state++;
}

/// Completes a Slouch slot when its enemy is dead or inside the cull region.
///
/// Requires live single-enemy work and a slot index 0..16 in the high spawn
/// halfword. A living culled enemy receives the puffing command; the spawner
/// immediately marks its slot done and ends without destroying the enemy.
static void _actor342400WaveWatchSlouch(Task* waveTask)
{
    enum { ACTOR_342400_WAVE_SLOUCH_RESTART_PUFFING = 5 };
    OverlayEncounterSingleWork* work = waveTask->work;
    Enemy*                      enemy;
    Task*                       enemyTask;
    GfxCoord*                   enemyCoord;
    ActorCommand                request;

    enemy      = work->enemy;
    enemyTask  = enemy->task;
    enemyCoord = enemyTask->extra.tmd->coords;
    if (enemy->hp <= 0) {
        gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
        return;
    }
    if (_actor342400InCullZone(gGameSession->enemyCullZone, enemyCoord->coord.t[0], enemyCoord->coord.t[2])) {
        request.context.loc.stage = 0;
        request.context.loc.area  = 0;
        request.command           = ACTOR_342400_WAVE_SLOUCH_RESTART_PUFFING;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
    }
}

/// Advances the pair spawner from state 1 to revealing its first Sucklerceph.
///
/// The reveal runs on the next tick, preserving the gap after spawning.
static void _actor342400WaveBeginPairReveal(Task* waveTask)
{
    waveTask->state++;
}

#include "../../shared/mad_chaser_waves_pair_reveal_first.inc.c"

#include "../../shared/mad_chaser_waves_pair_reveal_second.inc.c"

#include "../../shared/mad_chaser_waves_pair_watch.inc.c"

/// Selects the normal or reveal camera mapping for the incinerator encounter.
///
/// Zero maps logical view 2 to view 2; any nonzero signed halfword maps it to
/// view 17. Requires the loaded room's writable shared room-1/room-3 view map.
/// Changes only the mapping byte; it neither reloads nor dirties the live view.
static void _actor342400SetEncounterRevealView(s16 revealEnabled)
{
    if (revealEnabled == 0) {
        gShelterB3GarbageIncineratorRoom1And3ViewMap[SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_VIEW_OFFSET] = SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_NORMAL_VIEW;
        return;
    }
    gShelterB3GarbageIncineratorRoom1And3ViewMap[SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_VIEW_OFFSET] = SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_REVEAL_VIEW;
}

/// Starts an incinerator encounter row with a randomly chosen entry spot.
///
/// `slotIndex` must be 0..16. Kinds 0/1/2 select one Mad Chaser, one Slouch or
/// a Sucklerceph pair; other kinds do nothing and consume no random draw.
/// Packs the slot in bits 16..31 and adds a spot (2..15) in bits 8..11 to
/// `command`, preserving any spot bits already present in that signed word.
/// Spawn failure is ignored; the controller advances its row separately.
/// Requires the encounter descriptor table, room spots and live player model.
static void _actor342400SpawnEncounterSlot(s16 slotIndex, s16 enemyKind, s16 command)
{
    enum {
        OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER       = 0,
        OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH           = 1,
        OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR = 2,
        ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR   = 1,
        ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR       = 2,
        ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR         = 3,
        OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT           = 16,
        OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT           = 8,
    };
    switch (enemyKind) {
        case OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER:
            taskSpawnFromTable(D_actor_342400_8016BFE0, ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR, (slotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT), 0);
            break;
        case OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH:
            taskSpawnFromTable(D_actor_342400_8016BFE0, ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR, (slotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT), 0);
            break;
        case OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR:
            taskSpawnFromTable(D_actor_342400_8016BFE0, ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR, (slotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT), 0);
            break;
    }
}

#include "../../shared/mad_chaser_waves_pair_drop_dead.inc.c"
