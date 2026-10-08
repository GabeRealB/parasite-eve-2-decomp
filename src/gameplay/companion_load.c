#include "gameplay/companion_load.h"
#include "rooms/acropolis_plaza.h"

#include "common.h"

#include "gameplay/collision.h"
#include "companion_load.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "loading.h"
#include "gameplay/message.h"
#include "player_state.h"
#include "gameplay/room.h"
#include "scene_runtime.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"
#include "world_collision.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Resource families selected by the area schedules, independent of actor IDs.
enum {
    COMPANION_TYPE_NONE     = 0,
    COMPANION_TYPE_FAMILY_1 = 1,
    COMPANION_TYPE_FAMILY_2 = 2,
    COMPANION_TYPE_FAMILY_3 = 3,
};

/// The base-only variant and the result that requests no resource load.
enum {
    COMPANION_BASE_RESOURCE_VARIANT = 0,
    COMPANION_NO_RESOURCE_LOAD      = 0,
};

/// Area-presence masks and family 1's packed resource-variant shift.
enum {
    COMPANION_FAMILY_1_PRESENCE_MASK = 0xF,
    COMPANION_FAMILY_1_VARIANT_SHIFT = 4,
    COMPANION_FULL_PRESENCE_MASK     = 0xFF,
};

/// One schedule of a companion: the stage it appears in, and in which of that
/// stage's areas.
///
/// Each companion family has a table of these, and the family's
/// `GAME_FLAG_COMPANION_1_SCHEDULE` / `_2_` / `_3_` nibble selects the schedule
/// in force. Schedule 0 places the companion nowhere.
///
/// Families 2 and 3 treat a presence byte as a whole. Family 1 keeps presence
/// in the low nibble and the `companionVariant` that appears in the high one.
typedef struct {
    u8* areaPresence; // One byte per area of `stage`, indexed by area ID - 1 (0 absent); `NULL` when the companion appears nowhere
    u8  stage;        // `GAME_STAGE_*` whose areas `areaPresence` lists
} _CompanionSchedule;
STATIC_ASSERT_SIZEOF(_CompanionSchedule, 8);

/// Tests a schedule's stage and area presence, retaining its borrowed area table.
///
/// `outputAreaPresence` must be a simple u8* local; it is assigned, then read
/// repeatedly. `schedules` and `scheduleFlagId` are evaluated twice and must
/// have no side effects. The flag nibble must index the schedule table.
/// `stageId`, `areaId` and `presenceMask` are each evaluated at most once,
/// after the preceding tests pass. A matching stage requires a valid one-based
/// area. Family 1 uses its low nibble; families 2/3 use the whole byte.
#define COMPANION_SCHEDULE_HAS_AREA(schedules, scheduleFlagId, stageId, areaId, presenceMask, outputAreaPresence) \
    (((outputAreaPresence) = (schedules)[gameFlagGetNibble(scheduleFlagId)].areaPresence),                        \
     (outputAreaPresence) != NULL && (schedules)[gameFlagGetNibble(scheduleFlagId)].stage == (stageId) &&         \
         ((outputAreaPresence)[(areaId) - 1] & (presenceMask)) != 0)

extern _CompanionSchedule D_80114198[];

extern _CompanionSchedule D_801141F0[];

extern _CompanionSchedule D_80114248[];

extern u8 D_80114258[38];

extern u8 D_80114280[38];

extern u8 D_801142A8[38];

extern u8 D_801142D0[49];

extern u8 D_80114304[49];

extern u8 D_80114338[38];

extern u8 D_80114360[38];

extern u8 D_80114388[38];

extern u8 D_801143B0[38];

extern u8 D_801143D8[38];

extern u8 D_80114400[38];

extern u8 D_80114428[33];

extern u8 D_8011444C[33];

extern u8 D_80114470[49];

extern u8 D_801144A4[49];

extern u8 D_801144D8[49];

extern u8 D_8011450C[33];

extern u8 D_80114530[49];

/// 33 room flags, then the unexplained 3D F0 71 tail.
/// The tail is not a room-mask entry.
extern u8 D_80114564[36];

void func_80724E2C(void);

/// Finishes the loaded session's reveal fade and tears down its task.
///
/// `fadeTask` must be the live bodyless resume task after its final overlay;
/// `queue` borrows the writable session-loading state in `gCdCmdQueue`.
/// A nonzero `releasePauseBlockAfterFade` is consumed and clears `blockGamePause`;
/// a zero request preserves the existing pause block. Always releases one menu
/// hold acquired for the load before task teardown. Call once and do not access
/// `fadeTask` afterward; immediate teardown can free it before this returns.
static inline void _fadeFinishSessionResume(Task* fadeTask, CdCmdQueue* queue)
{
    if ((s16)queue->releasePauseBlockAfterFade != 0) {
        queue->releasePauseBlockAfterFade = 0;
        queue->blockGamePause             = 0;
    }
    displayReleaseMenuHold();
    taskKill(fadeTask);
}

_CompanionSchedule D_80114198[11] = {
    { NULL, GAME_STAGE_NONE },
    { D_80114360, GAME_STAGE_DRYFIELD },
    { D_80114388, GAME_STAGE_DRYFIELD },
    { D_801143B0, GAME_STAGE_DRYFIELD },
    { D_80114388, GAME_STAGE_DRYFIELD },
    { D_801143D8, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114388, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114400, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114428, GAME_STAGE_SHELTER_NEO_ARK },
    { D_8011444C, GAME_STAGE_SHELTER_NEO_ARK },
    { D_80114470, GAME_STAGE_MINE_SHELTER },
};
_CompanionSchedule D_801141F0[11] = {
    { NULL, GAME_STAGE_NONE },
    { D_80114258, GAME_STAGE_DRYFIELD_NIGHT },
    { D_80114280, GAME_STAGE_DRYFIELD_NIGHT },
    { D_801142A8, GAME_STAGE_DRYFIELD_NIGHT },
    { D_801142D0, GAME_STAGE_MINE_SHELTER },
    { D_80114304, GAME_STAGE_MINE_SHELTER },
    { D_80114338, GAME_STAGE_DRYFIELD_NIGHT },
    { D_801144A4, GAME_STAGE_MINE_SHELTER },
    { D_801144D8, GAME_STAGE_MINE_SHELTER },
    { D_8011450C, GAME_STAGE_SHELTER_NEO_ARK },
    { D_80114530, GAME_STAGE_MINE_SHELTER },
};
_CompanionSchedule D_80114248[2] = {
    { NULL, GAME_STAGE_NONE },
    { D_80114564, GAME_STAGE_SHELTER_NEO_ARK },
};
u8 D_80114258[38] = { 17, 0, 17, 0, 17, 17, 17, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114280[38] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801142A8[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801142D0[49] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114304[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 49, 49, 49, 49, 49, 0, 0, 0 };
u8 D_80114338[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0 };
u8 D_80114360[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114388[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801143B0[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801143D8[38] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114400[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114428[33] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_8011444C[33] = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114470[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801144A4[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0 };
u8 D_801144D8[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 33, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0 };
u8 D_8011450C[33] = { 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114530[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65, 65, 65, 65, 65, 65, 0, 0, 0 };
/// 33 room flags, then the unexplained 3D F0 71 tail.
/// The tail is not a room-mask entry.
u8 D_80114564[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 240, 113 };

s32 companionSelectForArea(void)
{
    McSaveData* save;
    u8*         areaPresence;
    s32         stage;
    u8          variant;

    save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    stage = save->state.location.loc.stage;
    // The first scheduled family present wins: 2, then 1, then 3.
    if (COMPANION_SCHEDULE_HAS_AREA(D_80114198, GAME_FLAG_COMPANION_2_SCHEDULE, stage, save->state.location.loc.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
        GameSession* session = gGameSession;

        save->state.companionType    = COMPANION_TYPE_FAMILY_2;
        save->state.companionVariant = COMPANION_BASE_RESOURCE_VARIANT;
        return (session->companionType != COMPANION_TYPE_FAMILY_2) * COMPANION_TYPE_FAMILY_2;
    }

    if (COMPANION_SCHEDULE_HAS_AREA(D_801141F0, GAME_FLAG_COMPANION_1_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area, COMPANION_FAMILY_1_PRESENCE_MASK, areaPresence)) {
        GameSession* session = gGameSession;

        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType = COMPANION_TYPE_FAMILY_1;
        if (session->companionType == COMPANION_TYPE_FAMILY_1) {
            variant = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] >> COMPANION_FAMILY_1_VARIANT_SHIFT;
            if (session->companionVariant == variant) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = variant;
                return COMPANION_NO_RESOURCE_LOAD;
            }
        }
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] >> COMPANION_FAMILY_1_VARIANT_SHIFT;
        gGameSession->companionVariant                            = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area - 1] >> COMPANION_FAMILY_1_VARIANT_SHIFT;
        return COMPANION_TYPE_FAMILY_1;
    }

    if (COMPANION_SCHEDULE_HAS_AREA(D_80114248, GAME_FLAG_COMPANION_3_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
        GameSession* session = gGameSession;

        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType    = COMPANION_TYPE_FAMILY_3;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = COMPANION_BASE_RESOURCE_VARIANT;
        if (session->companionType == COMPANION_TYPE_FAMILY_3) {
            return COMPANION_NO_RESOURCE_LOAD;
        }
        return COMPANION_TYPE_FAMILY_3;
    }

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType    = COMPANION_TYPE_NONE;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = COMPANION_BASE_RESOURCE_VARIANT;
    gGameSession->companionType                               = COMPANION_TYPE_NONE;
    gGameSession->companionVariant                            = COMPANION_BASE_RESOURCE_VARIANT;
    return COMPANION_NO_RESOURCE_LOAD;
}

/// Tests whether the destination's scheduled companion needs the first character sound bank retained.
///
/// Uses the live save's stage/area, except that the live session's nighttime
/// Water Hole disables retention. Schedule indices must be 0..10 for families 1/2
/// and 0..1 for family 3; the area ID must address the selected stage's table.
static inline s32 _companionShouldRetainSoundBank(void)
{
    McSaveData* save;
    u8*         areaPresence;
    s32         stage;

    save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    stage = save->state.location.loc.stage;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 0, 0)) {
        if (COMPANION_SCHEDULE_HAS_AREA(D_80114198, GAME_FLAG_COMPANION_2_SCHEDULE, stage, save->state.location.loc.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
            return 1;
        }
        if (COMPANION_SCHEDULE_HAS_AREA(D_801141F0, GAME_FLAG_COMPANION_1_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area, COMPANION_FAMILY_1_PRESENCE_MASK, areaPresence)) {
            return 1;
        }
        if (COMPANION_SCHEDULE_HAS_AREA(D_80114248, GAME_FLAG_COMPANION_3_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
            return 1;
        }
    }
    return 0;
}

void companionConfigureSoundBankRetention(void)
{
    sndLoadSetFirstCharacterBankRetention(_companionShouldRetainSoundBank());
}

void companionSpawnScheduledActor(const ActorSpawnTransform* spawnTransform, ActorSpawnOptions* options)
{
    enum { COMPANION_SPAWN_DEFAULT_SCHEDULE = 0 };
    const McSaveData* save;
    s32               companionType;

    save          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    companionType = save->state.companionType;
    if (companionType != COMPANION_TYPE_NONE) {
        // The actor's fixed schedule is distinct from the resource variant.
        if (companionType == COMPANION_TYPE_FAMILY_2) {
            companionSpawnActor(spawnTransform, save->state.companionType, gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE), options);
        } else {
            companionSpawnActor(spawnTransform, companionType, COMPANION_SPAWN_DEFAULT_SCHEDULE, options);
        }
    }
}

/// Clears a stage's two visited-area bit words, preserving its object and placement state.
///
/// `stageId` must be an active stage ID (1..5). This retained standalone helper
/// has no current callers and does not clear the live save's stage-visit bit.
static void _areaClearStageVisits(s32 stageId)
{
    GameFlagStageHeader* bank;

    bank                  = Gp_FlagBanks[stageId];
    bank->visitedAreas[0] = 0;
    bank->visitedAreas[1] = 0;
}

void areaMarkVisited(const GameLocationKey* location)
{
    enum { AREA_VISIT_BITS_PER_WORD = 32 };
    McSaveData*          save;
    GameFlagStageHeader* bank;
    s32                  visitWordIndex;
    s32                  visitBitIndex;
    s32                  mask;
    s32                  visitedAreas;

    bank = Gp_FlagBanks[location->stage];
    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    if ((((s8)save->state.visitFlags >> location->stage) & 1) == 0) {
        save->state.visitFlags |= 1 << location->stage;
        if (gDisplayState.debugMode != 0) {
            func_80724E2C();
        }
    }

    // Area IDs are one-based; each word records 32 consecutive areas.
    visitWordIndex = 0;
    if (location->area >= AREA_VISIT_BITS_PER_WORD + 1) {
        visitWordIndex = 1;
        visitBitIndex  = location->area - (AREA_VISIT_BITS_PER_WORD + 1);
    } else {
        visitBitIndex = location->area - 1;
    }

    mask         = 1;
    visitedAreas = bank->visitedAreas[visitWordIndex];
    if (((mask << visitBitIndex) & visitedAreas) == 0) {
        bank->visitedAreas[visitWordIndex] = visitedAreas | (mask << visitBitIndex);
        areaRequestSavedPoseReset(location);
    }
}

void func_800ABFF8(void)
{
}

void func_800AC000(void)
{
}

void gameFlowHoldSessionDisplayTask(Task* task)
{
    DisplayState* displayState;
    s32           displayMode;

    displayState             = &gDisplayState;
    displayState->skipDraw   = 1;
    displayState->holdState |= DISPLAY_HOLD_ACTIVE;
    displayMode              = task->spawnArg1.value & GAME_FLOW_RELOAD_DISPLAY_MODE_MASK;
    switch (displayMode) {
        case GAME_FLOW_RELOAD_CAPTURE_FRAME:
            break;
        case GAME_FLOW_RELOAD_BLANK_DISPLAY:
            displayState->control.flags.imageSource = DISPLAY_IMAGE_NONE;
            break;
    }
    task->state++;
}

void gameFlowPrepareSessionReloadTask(Task* task)
{
    sndScriptSetTypeRequestsEnabled(false, SOUND_BANK_TYPE_CHARACTER_ALL);
    if (gGameSession->deathVariant != 0) {
        taskKill(task);
        return;
    }
    if ((task->spawnArg1.value & GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE) == 0) {
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_RESUMED;
        }
        sceneQueueBattleEscapeResult();
    }
    task->state++;
}

void gameFlowReloadSessionTask(Task* task)
{
    TaskFuncTable3 states;

    states = Gp_SessionStates;
    padStartInputBlock(0);
    // Keep the pause store before the state read that selects the next phase.
    *(volatile u8*)&gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
    states.funcs[((volatile Task*)task)->state](task);
}

void Gp_LoadFinishTask(Task* task)
{
    if (gCdCmdQueue.bootLoadActive == 0) {
        gpuClearFrameOrderingTable(0);
        gpuClearFrameOrderingTable(1);
        Pad_RemapState->loadingActive = GAME_DEBUG_LOADING_IDLE;
        taskKill(task);
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            areaStartRoomRuntime(AREA_ROOM_START_SKIP_VIEW_GATE);
        } else {
            areaStartRoomRuntime(AREA_ROOM_START_WITH_VIEW_GATE);
        }
        gDisplayState.holdCount  = 0;
        gDisplayState.holdState &= DISPLAY_HOLD_MODE_MASK;
        displayAcquireMenuHold();
        taskSpawn(0, 0x21, 0, 0);
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
            taskSpawnFromTable(D_acropolis_plaza_80183824, 0, 0, 0);
            cdCmdReservePlaybackBuffers();
            cdCmdSelectMovieWorkspace();
        }
    }
}

void Gp_LoadStateTask(Task* task)
{
    TaskFuncTable8 sp;
    DisplayState*  ds;

    sp = Gp_LoadStateFns;
    padStartInputBlock(0);
    ds = &gDisplayState;
    if (ds->demoScene != DISPLAY_DEMO_NONE) {
        if (padReadRawButtons(0) & PAD_BUTTON_START) {
            if (cdCmdIsIdle() & 0xFFFF) {
                Wip_SysFlags.skipTitleIntro = 1;
                ds->gameMode                = DISPLAY_GAME_RESTART;
                return;
            }
        }
    }
    sp.funcs[task->state](task);
}

void fadeResumeSessionTask(Task* task)
{
    enum {
        FADE_RESUME_INIT          = 0,
        FADE_RESUME_HOLD_BLACK    = 1,
        FADE_RESUME_REVEAL        = 2,
        FADE_RESUME_BLACK_TICKS   = 3,
        FADE_RESUME_MAX_DARKNESS  = 255,
        FADE_RESUME_DARKNESS_STEP = 30,
    };
    CdCmdQueue* queue;
    u8          darkness;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case FADE_RESUME_INIT:
            task->killCountdown = 0;
            task->state++;
            // Include the initialization tick in the full-black hold.
        case FADE_RESUME_HOLD_BLACK:
            fadeDrawOverlay(FADE_RESUME_MAX_DARKNESS, FADE_RESUME_MAX_DARKNESS, FADE_RESUME_MAX_DARKNESS, GPU_BLEND_SUBTRACT);
            task->killCountdown++;
            if (task->killCountdown < FADE_RESUME_BLACK_TICKS) {
                return;
            }
            task->killCountdown = FADE_RESUME_MAX_DARKNESS;
            task->state++;
            break;
        case FADE_RESUME_REVEAL:
            // Draw before stepping: the last overlay has darkness 15, then the counter crosses zero.
            darkness = task->killCountdown;
            fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
            task->killCountdown -= FADE_RESUME_DARKNESS_STEP;
            if (task->killCountdown > 0) {
                return;
            }
            _fadeFinishSessionResume(task, queue);
            break;
    }
}

s32 taskMessageDispatch(Task* receiver, s32 messageId, s32 firstArg, s32 secondArg)
{
    const TaskMessageEntry* entry;

    if (receiver->msgTable == NULL) {
        return 0;
    }
    entry = receiver->msgTable;

    // Match the ID before testing the reserved end marker.
    if (entry->messageId != messageId) {
        do {
            if (entry->messageId == TASK_MESSAGE_TABLE_END) {
                return 0;
            }
            entry++;
        } while (entry->messageId != messageId);
    }
    return entry->handler(receiver, messageId, firstArg, secondArg);
}

/// Binds borrowed collision records for initial room setup without clearing lists.
///
/// The loaded stage directory must provide valid one-based area/room indices.
/// Each non-NULL trigger/occluder list must contain its LAST-marked entry and
/// remain writable and loaded until unlinked. Existing registrations must be
/// empty or disposable; NULL resources retain the existing grid publication.
static inline void _loadingBindInitialRoomCollisionResources(const GameLocationKey* location, const WorldCollisionRoomResources* roomResources)
{
    enum { WORLD_COLLISION_OCCLUDER_LIST_ACTIVE = 0 };
    WorldCollisionGrid*     grid;
    WorldCollisionTrigger*  viewBoundaryTriggers;
    WorldCollisionTrigger*  actionTriggers;
    WorldCollisionOccluder* occluders;
    s32                     resourceIndex;

    if (roomResources != NULL) {
        grid                 = roomResources[location->room - 1].grid;
        viewBoundaryTriggers = roomResources[location->room - 1].viewBoundaryTriggers;
        actionTriggers       = roomResources[location->room - 1].actionTriggers;
        occluders            = roomResources[location->room - 1].occluders;
        if (grid != NULL) {
            // Bind the room mesh to the current view before publishing it.
            grid->viewCoord = &gGfxViewCoord;
            Gp_GridParams   = grid;
        }
        if (viewBoundaryTriggers != NULL) {
            for (resourceIndex = 0;; resourceIndex++) {
                viewBoundaryTriggers[resourceIndex].coord = &gGfxViewCoord;
                worldCollisionLinkTrigger(WORLD_COLLISION_TRIGGER_LIST_VIEW_BOUNDARIES, &viewBoundaryTriggers[resourceIndex]);
                viewBoundaryTriggers[resourceIndex].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (viewBoundaryTriggers[resourceIndex].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (actionTriggers != NULL) {
            for (resourceIndex = 0;; resourceIndex++) {
                actionTriggers[resourceIndex].coord = &gGfxViewCoord;
                worldCollisionLinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &actionTriggers[resourceIndex]);
                actionTriggers[resourceIndex].flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                if (actionTriggers[resourceIndex].flags & WORLD_COLLISION_TRIGGER_LAST) {
                    break;
                }
            }
        }
        if (occluders != NULL) {
            for (resourceIndex = 0;; resourceIndex++) {
                worldCollisionLinkOccluder(WORLD_COLLISION_OCCLUDER_LIST_ACTIVE, &occluders[resourceIndex]);
                occluders[resourceIndex].flags |= WORLD_COLLISION_OCCLUDER_ENABLED;
                if (occluders[resourceIndex].flags & WORLD_COLLISION_OCCLUDER_LAST) {
                    break;
                }
            }
        }
    }
}

void loadingInitRoomResourcesTask(Task* task)
{
    enum {
        LOADING_VIEW_SPRITE_TASK_BANK = 0,
        LOADING_VIEW_SPRITE_TASK_TYPE = 0x1B
    };
    const GameLocationKey*             location;
    const WorldCollisionRoomResources* roomResources;
    Task*                              viewSpriteTask;

    location      = &gGameSession->location.loc;
    roomResources = Gp_RoomObjTables[location->stage - 1]->areaRooms[location->area - 1];
    _loadingBindInitialRoomCollisionResources(location, roomResources);
    // The room task owns the sprite task in its teardown tree.
    viewSpriteTask = taskSpawn(LOADING_VIEW_SPRITE_TASK_BANK, LOADING_VIEW_SPRITE_TASK_TYPE, 0, 0);
    if (viewSpriteTask != NULL) {
        taskReparent(task, viewSpriteTask);
    }
    gGameSession->roomObjsDirty = 0;
    task->state++;
}
