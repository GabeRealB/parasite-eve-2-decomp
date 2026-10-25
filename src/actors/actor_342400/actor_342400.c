#include "actor_342400_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor_07000.h"
#include "actors/actor_342400.h"

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
#include "../../shared/creatures/mad_chaser/mad_chaser.h"

static void _overlayEncounterOpen(Task* controllerTask);
static void _overlayEncounterPairSpawn(Task* waveTask);

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

// Message-table callbacks use the argument views required by this TU.

/// Enemy kinds selected by this encounter's row table.
enum {
    OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER       = 0,
    OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH           = 1,
    OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR = 2,
};

/// Command fields stored before the spawner selects a room spot.
enum {
    ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN = 0,
    ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT        = 0, // Mad Chaser backflip; other kinds have no entry modifier
    ACTOR_342400_ENCOUNTER_ENTRY_ARC_BACK       = 1,
    ACTOR_342400_ENCOUNTER_ENTRY_HIGH_ARC       = 2,
};

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

/// Stop-command messages accepted by the incinerator encounter controller.
///
/// APPLY borrows an ActorCommand synchronously; only STOP suspends feeding.
/// The second entry terminates lookup. The controller borrows this table and
/// its void-returning handler while actor_342400 is loaded; ignore the result.
static TaskMessageEntry _gActor342400EncounterMessages[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor342400WaveStopMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Ordered spawner rows and mutable completion states for the incinerator encounter.
///
/// Rows 0..16 contain seven Mad Chasers, four Slouches and six Sucklerceph pairs.
/// Initialization resets every status; the opening consumes three rows and
/// feeding consumes the remainder. Each spawner marks its row live, then done.
/// Commands supply the entry move; spawning adds a player-band spot in bits
/// 8..11. A pair occupies one row but can receive two placement identities.
static OverlayEncounterSlot _gActor342400EncounterSlots[17] = {
    { OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_ARC_BACK), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_HIGH_ARC), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_ARC_BACK), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_HIGH_ARC), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
    { OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR, OVERLAY_ENCOUNTER_APPEAR_COMMAND(ACTOR_342400_ENCOUNTER_SPOT_CHOSEN_ON_SPAWN, ACTOR_342400_ENCOUNTER_ENTRY_DEFAULT), { 0, 0 }, OVERLAY_ENCOUNTER_SLOT_WAITING },
};

static void _actor342400WaveMadChaserTask(Task* waveTask);
static void _actor342400WaveSlouchTask(Task* waveTask);
static void _actor342400WavePairTask(Task* waveTask);

enum {
    ACTOR_342400_ENCOUNTER_SPAWNER_PRIORITY = 97,
};

TaskDesc gActor342400EncounterTaskDescs[ACTOR_342400_ENCOUNTER_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _actor342400WaveControllerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_342400_ENCOUNTER_SPAWNER_PRIORITY } }, _actor342400WaveMadChaserTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_342400_ENCOUNTER_SPAWNER_PRIORITY } }, _actor342400WaveSlouchTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_342400_ENCOUNTER_SPAWNER_PRIORITY } }, _actor342400WavePairTask, { .value = 0 } },
};

/// World-coordinate limits of the incinerator's enemy-culling regions.
///
/// The session's enemyCullZone selects rows 1..16; 0 disables culling.
/// X rows cull x below the limit, Z rows cull z at or above the limit.
/// Positions and limits are signed-halfword world units; row 0 is not read.
static _Actor342400CullZone _gActor342400EncounterCullZones[17] = {
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

/// Four equiprobable encounter-spot choices for each incinerator player-position band.
///
/// In order: x <= 5000, x <= 8000, x <= 11000, z >= -5500, z >= -8500,
/// then all remaining positions, after excluding earlier bands. Coordinates
/// are signed-halfword world units. Values index room spots 2..15, not variants;
/// the repeated final-row entries give spots 9 and 15 half the probability each.
static s16 _gActor342400EncounterSpotChoices[6][4] = {
    { 2, 3, 11, 12 },
    { 3, 4, 12, 5 },
    { 5, 6, 7, 13 },
    { 7, 8, 13, 14 },
    { 8, 9, 14, 15 },
    { 9, 15, 9, 15 },
};

#include "assets/enemies/chaser/mad/burst_head.inc"

#include "assets/enemies/chaser/mad/burst_arm.inc"

#include "assets/enemies/chaser/mad/burst_tail.inc"

#include "assets/enemies/chaser/mad/body.inc"

/// Mad Chaser contact attack: index 0, base power 22, ordinary player hit without status.
///
/// Enemy parameters borrow this row while the carrier is loaded. Its key is
/// shared by the attack sphere; HP-band scaling is applied by the victim.
static DamageAttack _gMadChaserAttacks[1] = {
    { 22, GAME_ACTOR_REACTION_ORDINARY },
};

EnemyParams gActor342400MadChaserEnemyParams = { _gMadChaserAttacks, 110, 20, 40, 1, 100, 10, 100, 0 };

static void _actor342400SpawnEncounterSlot(s16 slotIndex, s16 enemyKind, s16 command);

/// Allocates a pair spawner and creates up to two one-HP Sucklercephs.
///
/// Requires the loaded Sucklerceph descriptor and a spawn argument whose signed
/// high halfword is a valid encounter-row index (0..16 in this carrier).
/// The low halfword is retained for later reveal stages. Allocation failure
/// or two failed enemy spawns kills the spawner without marking its row live.
/// Task teardown owns the zeroed primary-heap work; it borrows any spawned
/// enemies until the later death/cull stages forget them. Successful enemies
/// receive sequential placement keys and the encounter palette offsets. The
/// placement halfword retains only the counter's low four bits in its index.
static void _overlayEncounterPairSpawn(Task* waveTask)
{
    enum {
        OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX = 1,
        OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT    = 1,
        OVERLAY_ENCOUNTER_PAIR_INITIAL_HP       = 1,
    };
    OverlayEncounterPairWork* work;
    Enemy*                    enemy;
    Task*                     enemyTask;
    TmdObject*                model;

/// Assigns one spawned Sucklerceph its encounter identity, palette and one HP.
///
/// `member` is a non-NULL Enemy* expression evaluated once, with a live TMD
/// task. Overwrites the caller's `enemy`, `enemyTask` and `model` locals and
/// reads the `OVERLAY_ENCOUNTER_PAIR_*` constants. Consumes and increments
/// `gActor342400EncounterNextPlaceIndex` modulo 65536; the u16 place key retains
/// only the counter's low four bits shifted by `ENEMY_PLACE_INDEX_SHIFT`.
/// Model texture/CLUT offsets are set before HP. Borrows the enemy and model;
/// neither is allocated or retained here. Expands to one statement.
#define OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(member)                                     \
    do {                                                                                  \
        enemy           = (member);                                                       \
        enemy->placeKey = gActor342400EncounterNextPlaceIndex << ENEMY_PLACE_INDEX_SHIFT; \
        gActor342400EncounterNextPlaceIndex++;                                            \
        enemyTask                = enemy->task;                                           \
        model                    = enemyTask->extra.tmd;                                  \
        model->texturePageOffset = OVERLAY_ENCOUNTER_PAIR_TEXTURE_PAGE_OFFSET;            \
        model->clutRowOffset     = OVERLAY_ENCOUNTER_PAIR_CLUT_ROW_OFFSET;                \
        enemy->hp                = OVERLAY_ENCOUNTER_PAIR_INITIAL_HP;                     \
    } while (0)

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(waveTask);
        return;
    }
    // The spawner owns its work; the two independently spawned enemies are borrowed.
    waveTask->work = work;
    work->enemy0   = enemySpawnFromTable(gActor207000SucklercephTaskDescs, OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX, OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT, 0);
    work->enemy1   = enemySpawnFromTable(gActor207000SucklercephTaskDescs, OVERLAY_ENCOUNTER_PAIR_DESCRIPTOR_INDEX, OVERLAY_ENCOUNTER_PAIR_SOUND_VARIANT, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
        taskKill(waveTask);
        return;
    }
    if (work->enemy0 != NULL) {
        OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(work->enemy0);
    }
    if (work->enemy1 != NULL) {
        OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER(work->enemy1);
    }
    _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
    waveTask->state++;
}

#undef OVERLAY_ENCOUNTER_PREPARE_PAIR_MEMBER

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

    /// Releases a dead or culled pair member, recording its gone bit on a later call.
    ///
    /// `enemyMember` is an Enemy* member token of live `work`; `goneBit` is
    /// evaluated once only when that pointer was already NULL at entry.
    /// Captures work, enemy, enemyTask, enemyCoord and request. The command
    /// dispatch consumes request synchronously. Use as a standalone statement;
    /// clearing the borrowed pointer does not destroy the enemy task.
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

/// Returns the number of live spawner rows in the incinerator encounter.
///
/// Counts rows marked `OVERLAY_ENCOUNTER_SLOT_LIVE`, yielding 0..17. A row
/// spawning a pair counts once, even if one member has already died. Waiting
/// and completed rows do not count; enemy health and visibility are not tested.
/// Reads the spawners' bookkeeping without changing it.
static inline s16 _actor342400WaveCountLiveSlots(void)
{
    s16 liveSlotCount = 0;
    s16 slotIndex;

    for (slotIndex = 0; slotIndex < (s32)ARRAY_SIZE(_gActor342400EncounterSlots); slotIndex++) {
        if (_gActor342400EncounterSlots[slotIndex].status == OVERLAY_ENCOUNTER_SLOT_LIVE) {
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
        ACTOR_342400_WAVE_LIVE_SLOT_LIMIT      = 3,
        ACTOR_342400_WAVE_FEED_MIN_SCENE_CLOCK = 61,
        OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT     = 16,
        OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT     = 8,
        ACTOR_342400_SIGNED_HALFWORD_SHIFT     = 16,
    };
    OverlayEncounterControllerWork* work = controllerTask->work;
    s16                             liveSlotCount;
    s16                             nextSlotIndex;
    s16                             enemyKind;
    s16                             command;

    liveSlotCount = _actor342400WaveCountLiveSlots();
    if (liveSlotCount < ACTOR_342400_WAVE_LIVE_SLOT_LIMIT) {
        nextSlotIndex = work->nextSlot;
        if (nextSlotIndex < (s32)ARRAY_SIZE(_gActor342400EncounterSlots) && gGameSession->sceneClock >= ACTOR_342400_WAVE_FEED_MIN_SCENE_CLOCK) {
            enemyKind = _gActor342400EncounterSlots[nextSlotIndex].kind;
            command   = _gActor342400EncounterSlots[nextSlotIndex].command;
            // Preserve the signed-halfword spot expansion before adding command bits.
            switch (enemyKind) {
                case OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER:
                    taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR, (nextSlotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << ACTOR_342400_SIGNED_HALFWORD_SHIFT >> (ACTOR_342400_SIGNED_HALFWORD_SHIFT - OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT)), 0);
                    break;
                case OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH:
                    taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR, (nextSlotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << ACTOR_342400_SIGNED_HALFWORD_SHIFT >> (ACTOR_342400_SIGNED_HALFWORD_SHIFT - OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT)), 0);
                    break;
                case OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR:
                    taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR, (nextSlotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << ACTOR_342400_SIGNED_HALFWORD_SHIFT >> (ACTOR_342400_SIGNED_HALFWORD_SHIFT - OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT)), 0);
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
        ACTOR_342400_ENCOUNTER_PICK_MASK    = ARRAY_SIZE(_gActor342400EncounterSpotChoices[0]) - 1,
    };
    GfxCoord* playerCoord = (*gPlayerActorTasks)->extra.tmd->coords;
    s16       playerX     = playerCoord->coord.t[0];
    s16       playerZ     = playerCoord->coord.t[2];

    if (playerX <= 5000) {
        return _gActor342400EncounterSpotChoices[0][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerX <= 8000) {
        return _gActor342400EncounterSpotChoices[1][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerX <= 11000) {
        return _gActor342400EncounterSpotChoices[2][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -5500) {
        return _gActor342400EncounterSpotChoices[3][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -8500) {
        return _gActor342400EncounterSpotChoices[4][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -11500) {
        return _gActor342400EncounterSpotChoices[5][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    if (playerZ >= -24500) {
        return _gActor342400EncounterSpotChoices[5][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
    }
    return _gActor342400EncounterSpotChoices[5][(RANDOM_LCG_ADVANCE()) >> ACTOR_342400_ENCOUNTER_RANDOM_SHIFT & ACTOR_342400_ENCOUNTER_PICK_MASK];
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
    if (zoneId == 0 || zoneId > (s32)ARRAY_SIZE(_gActor342400EncounterCullZones) - 1) {
        return 0;
    }
    if (_gActor342400EncounterCullZones[zoneId].axis == ACTOR_342400_CULL_ZONE_AXIS_X) {
        if (worldX < _gActor342400EncounterCullZones[zoneId].limit) {
            return 1;
        }
    } else {
        if (worldZ >= _gActor342400EncounterCullZones[zoneId].limit) {
            return 1;
        }
    }
    return 0;
}

/// States of the incinerator encounter controller.
///
/// `Task::state` selects 0 initialize, 1 open three rows, 2 arm battle, 3 feed and
/// finish. Dispatch copies all four non-NULL void(Task*) callbacks before the
/// scene-actor running gate; the index must be 0..3 and task work initialized
/// for its state. The carrier remains loaded through controller teardown.
static const TaskFuncTable4 _gActor342400EncounterControllerStates = { {
    _actor342400WaveInitialize,
    _overlayEncounterOpen,
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

    handlers = _gActor342400EncounterControllerStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        handlers.funcs[controllerTask->state](controllerTask);
    }
}

/// States of one incinerator Mad Chaser row spawner.
///
/// `Task::state` selects 0 spawn, 1 delayed reveal, 2 advance, 3 watch death/cull.
/// Dispatch copies all four non-NULL void(Task*) callbacks; the index must be
/// 0..3. Spawn initializes owned work and borrows the enemy for later states;
/// keep the carrier and that enemy's task loaded while they are used.
static const TaskFuncTable4 _gActor342400EncounterMadChaserStates = { {
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

    handlers = _gActor342400EncounterMadChaserStates;
    handlers.funcs[waveTask->state](waveTask);
}

/// States of one incinerator Slouch row spawner.
///
/// `Task::state` selects 0 spawn, 1 delayed reveal, 2 advance, 3 watch death/cull.
/// Dispatch copies all four non-NULL void(Task*) callbacks; the index must be
/// 0..3. Spawn initializes owned work and borrows the enemy for later states;
/// keep the carrier and that enemy's task loaded while they are used.
static const TaskFuncTable4 _gActor342400EncounterSlouchStates = { {
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

    handlers = _gActor342400EncounterSlouchStates;
    handlers.funcs[waveTask->state](waveTask);
}

/// States of one incinerator Sucklerceph-pair row spawner.
///
/// `Task::state` selects 0 spawn, 1 advance, 2 reveal first, 3 reveal second,
/// 4 watch death/cull. Dispatch copies all five non-NULL void(Task*) callbacks;
/// the index must be 0..4. Spawn owns pair work and borrows surviving enemies;
/// keep their tasks and both actor overlays live while the pointers are used.
static const TaskFuncTable5 _gActor342400EncounterPairStates = { {
    _overlayEncounterPairSpawn,
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

    handlers = _gActor342400EncounterPairStates;
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
    for (slotIndex = (s32)ARRAY_SIZE(_gActor342400EncounterSlots) - 1; slotIndex >= 0; slotIndex--) {
        _gActor342400EncounterSlots[slotIndex].status = OVERLAY_ENCOUNTER_SLOT_WAITING;
    }
    gActor342400EncounterNextPlaceIndex = 0;
    controllerTask->work                = work;
    controllerTask->msgTable            = _gActor342400EncounterMessages;
    controllerTask->state++;
}

/// Binds the encounter opening's row-spawn operation to the incinerator spawner.
///
/// The value is a declared void(s16 slotIndex, s16 enemyKind, s16 command)
/// function identifier, with live descriptor and placement resources. The
/// opening fragment calls it for slots 0..2: kinds 0/1/2 select a Mad Chaser,
/// a Slouch or a Sucklerceph pair, and command is the packed signed-halfword
/// emergence command. Each call argument is evaluated once and narrowed to s16.
/// The controller advances its cursor even if spawning fails. The fragment
/// undefines this inclusion-local binding after its body.
#define OVERLAY_ENCOUNTER_SPAWN_SLOT _actor342400SpawnEncounterSlot
/// Starts the encounter's first three rows and advances its controller.
///
/// Requires live controller work initialized with nextSlot 0 and at least
/// three complete `_gActor342400EncounterSlots` rows. The carrier binds the declared
/// void(s16, s16, s16) `OVERLAY_ENCOUNTER_SPAWN_SLOT` callback around inclusion.
/// Each argument is evaluated once; the binding is undefined after the body.
/// Advances the cursor even when the requested row fails to spawn.
static void _overlayEncounterOpen(Task* controllerTask)
{
    enum {
        OVERLAY_ENCOUNTER_OPEN_SLOT_COUNT = 3,
    };
    s32                             openingSlot;
    OverlayEncounterControllerWork* work = controllerTask->work;
    OverlayEncounterSlot*           slot;

    for (openingSlot = 0; openingSlot < OVERLAY_ENCOUNTER_OPEN_SLOT_COUNT; openingSlot++) {
        slot = &_gActor342400EncounterSlots[work->nextSlot];
        OVERLAY_ENCOUNTER_SPAWN_SLOT(work->nextSlot, slot->kind, slot->command);
        work->nextSlot++;
    }
    controllerTask->state++;
}

#undef OVERLAY_ENCOUNTER_SPAWN_SLOT

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
        for (slotIndex = 0; slotIndex < (s32)ARRAY_SIZE(_gActor342400EncounterSlots); slotIndex++) {
            if (_gActor342400EncounterSlots[slotIndex].status == OVERLAY_ENCOUNTER_SLOT_DONE) {
                doneSlotCount++;
            }
        }
        if (doneSlotCount == (s32)ARRAY_SIZE(_gActor342400EncounterSlots)) {
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
        enemy          = enemySpawnFromTable(gActor342400MadChaserTaskDescs, ACTOR_342400_WAVE_MAD_CHASER_HIDDEN_TASK, 0, NULL);
        if (enemy != NULL) {
            _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            work->enemy                                                                = enemy;
            enemy->placeKey                                                            = gActor342400EncounterNextPlaceIndex << ENEMY_PLACE_INDEX_SHIFT;
            gActor342400EncounterNextPlaceIndex++;
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
        enemy->workType           = ENEMY_WORK_ACTOR;
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
        _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
        return;
    }
    if (_actor342400InCullZone(gGameSession->enemyCullZone, enemyCoord->coord.t[0], enemyCoord->coord.t[2])) {
        request.context.loc.stage = 0;
        request.context.loc.area  = ACTOR_342400_WAVE_MAD_CHASER_COMMAND_AREA;
        request.command           = MAD_CHASER_COMMAND_SHRINK_DEATH;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
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
        enemy          = enemySpawnFromTable(gActor207000SlouchTaskDescs, ACTOR_342400_WAVE_SLOUCH_HIDDEN_TASK, 0, NULL);
        if (enemy != NULL) {
            _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_LIVE;
            work->enemy                                                                = enemy;
            enemy->placeKey                                                            = gActor342400EncounterNextPlaceIndex << ENEMY_PLACE_INDEX_SHIFT;
            gActor342400EncounterNextPlaceIndex++;
            model                    = enemy->task->extra.tmd;
            model->texturePageOffset = ACTOR_342400_WAVE_SLOUCH_TEXTURE_PAGE;
            model->clutRowOffset     = ACTOR_342400_WAVE_SLOUCH_CLUT_ROW;
            waveTask->state++;
            return;
        }
    }
    taskKill(waveTask);
}

#include "../../shared/creatures/mad_chaser/mad_chaser_waves_reveal_second.inc.c"

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
        _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
        return;
    }
    if (_actor342400InCullZone(gGameSession->enemyCullZone, enemyCoord->coord.t[0], enemyCoord->coord.t[2])) {
        request.context.loc.stage = 0;
        request.context.loc.area  = 0;
        request.command           = ACTOR_342400_WAVE_SLOUCH_RESTART_PUFFING;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
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

#include "../../shared/creatures/mad_chaser/mad_chaser_waves_pair_reveal_first.inc.c"

/// Forgets dead pair members, then reveals the second Sucklerceph after its delay.
///
/// Requires initialized pair work with the counter reset by the first reveal.
/// Borrowed enemies and their model tasks must remain live until forgotten.
/// A surviving second enemy is revealed on update 61 with the low spawn
/// halfword's command; a missing one skips the wait. Dispatch consumes the
/// stack request synchronously. Completion resets the counter and advances.
static void _overlayEncounterPairRevealSecond(Task* waveTask)
{
    enum { OVERLAY_ENCOUNTER_PAIR_REVEAL_WAIT_UPDATES = 60 };
    OverlayEncounterPairWork* work = waveTask->work;
    Enemy*                    enemy;
    Task*                     enemyTask;
    TmdObject*                model;
    ActorCommand              request;

    // Retain the partner only if the HP check leaves its borrowed pointer live.
    enemy = work->enemy1;
    _overlayEncounterForgetDeadPairMembers(waveTask);
    if (work->enemy1 != NULL) {
        if (++work->frames <= OVERLAY_ENCOUNTER_PAIR_REVEAL_WAIT_UPDATES) {
            return;
        }
        enemyTask                 = work->enemy1->task;
        model                     = enemyTask->extra.tmd;
        model->texturePageOffset  = OVERLAY_ENCOUNTER_PAIR_TEXTURE_PAGE_OFFSET;
        model->clutRowOffset      = OVERLAY_ENCOUNTER_PAIR_CLUT_ROW_OFFSET;
        enemy->workType           = ENEMY_WORK_ACTOR;
        request.context.loc.stage = OVERLAY_ENCOUNTER_PAIR_COMMAND_STAGE;
        request.context.loc.area  = OVERLAY_ENCOUNTER_PAIR_COMMAND_AREA;
        request.command           = waveTask->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
    }
    work->frames = 0;
    waveTask->state++;
}

/// Completes the encounter row once both Sucklercephs have died or been culled.
///
/// Requires live pair work, borrowed enemy tasks and a high spawn halfword
/// selecting row 0..16. The carrier's cull check releases enemy pointers and
/// records their gone bits on the next update. Enemy teardown is separate;
/// killing the spawner releases its owned work after marking the row done.
static void _overlayEncounterPairWatch(Task* waveTask)
{
    OverlayEncounterPairWork* work = waveTask->work;

    _actor342400WaveCullPair(waveTask);
    if (work->goneMask == OVERLAY_ENCOUNTER_PAIR_GONE_BOTH) {
        _gActor342400EncounterSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
    }
}

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
        OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT = 16,
        OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT = 8,
    };
    switch (enemyKind) {
        case OVERLAY_ENCOUNTER_SLOT_KIND_MAD_CHASER:
            taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR, (slotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT), 0);
            break;
        case OVERLAY_ENCOUNTER_SLOT_KIND_SLOUCH:
            taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR, (slotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT), 0);
            break;
        case OVERLAY_ENCOUNTER_SLOT_KIND_SUCKLERCEPH_PAIR:
            taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR, (slotIndex << OVERLAY_ENCOUNTER_SLOT_INDEX_SHIFT) + command + (_actor342400PickEncounterSpot() << OVERLAY_ENCOUNTER_SPOT_INDEX_SHIFT), 0);
            break;
    }
}

#include "../../shared/creatures/mad_chaser/overlay_encounter_forget_dead_pair_members.inc.c"
