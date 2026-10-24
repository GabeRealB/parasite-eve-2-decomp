#include "gameplay/companion_load.h"
#include "stages/akropolis/rooms/plaza.h"

#include "common.h"

#include "gameplay/collision.h"
#include "companion_load.h"
#include "gameplay/display.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "loading.h"
#include "gameplay/message.h"
#include "player_state.h"
#include "gameplay/room.h"
#include "scene_runtime.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"
#include "world_collision.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/game_debug.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/gamemain.h"

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
/// repeatedly. `schedules` and `scheduleFlagId` are evaluated up to twice and
/// must have no side effects; the flag must stay unchanged between reads.
/// The flag nibble must index the schedule table.
/// `stageId`, `areaId` and `presenceMask` are each evaluated at most once,
/// after the preceding tests pass. A matching stage requires a valid one-based
/// area. Family 1 uses its low nibble; families 2/3 use the whole byte.
/// The output receives the selected table even when stage or presence fails;
/// only a NULL table suppresses all remaining tests.
#define COMPANION_SCHEDULE_HAS_AREA(schedules, scheduleFlagId, stageId, areaId, presenceMask, outputAreaPresence) \
    (((outputAreaPresence) = (schedules)[gameFlagGetNibble(scheduleFlagId)].areaPresence),                        \
     (outputAreaPresence) != NULL && (schedules)[gameFlagGetNibble(scheduleFlagId)].stage == (stageId) &&         \
         ((outputAreaPresence)[(areaId) - 1] & (presenceMask)) != 0)

/// Schedule domains, including schedule 0 for absence.
enum {
    COMPANION_FAMILY_1_SCHEDULE_COUNT = 11,
    COMPANION_FAMILY_2_SCHEDULE_COUNT = 11,
    COMPANION_FAMILY_3_SCHEDULE_COUNT = 2,
};

/// Area-table extents: one byte for every one-based area ID in the stage.
enum {
    COMPANION_DRYFIELD_AREA_COUNT        = GAME_AREA_DRYFIELD_UNDERPASS,
    COMPANION_MINE_SHELTER_AREA_COUNT    = GAME_AREA_SHELTER_R49,
    COMPANION_SHELTER_NEO_ARK_AREA_COUNT = GAME_AREA_NEO_ARK_SUBSTATION,
    COMPANION_AREA_PRESENT               = 1,
};

/// Packs a present family-1 area with its resource variant.
///
/// `resourceVariant` is an integer constant in 0..15, evaluated once. The low
/// nibble is presence; the high nibble selects the package requested on entry.
#define COMPANION_PACK_AREA_PRESENCE(resourceVariant) \
    (((resourceVariant) << COMPANION_FAMILY_1_VARIANT_SHIFT) | COMPANION_AREA_PRESENT)

/// Retained byte extent of family 3's schedule-1 storage, including an unknown tail.
enum { COMPANION_FAMILY_3_SCHEDULE_1_STORAGE_BYTES = 36 };

static u8 _gCompanionFamily1Schedule1AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily1Schedule2AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily1Schedule3AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily1Schedule4AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT];
static u8 _gCompanionFamily1Schedule5AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT];
static u8 _gCompanionFamily1Schedule6AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily2Schedule1AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily2Schedule2AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily2Schedule3AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily2Schedule5AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily2Schedule7AreaPresence[COMPANION_DRYFIELD_AREA_COUNT];
static u8 _gCompanionFamily2Schedule8AreaPresence[COMPANION_SHELTER_NEO_ARK_AREA_COUNT];
static u8 _gCompanionFamily2Schedule9AreaPresence[COMPANION_SHELTER_NEO_ARK_AREA_COUNT];
static u8 _gCompanionFamily2Schedule10AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT];
static u8 _gCompanionFamily1Schedule7AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT];
static u8 _gCompanionFamily1Schedule8AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT];
static u8 _gCompanionFamily1Schedule9AreaPresence[COMPANION_SHELTER_NEO_ARK_AREA_COUNT];
static u8 _gCompanionFamily1Schedule10AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT];
static u8 _gCompanionFamily3Schedule1AreaPresence[COMPANION_FAMILY_3_SCHEDULE_1_STORAGE_BYTES];

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

/// Family-2 stage and area schedules selected by `GAME_FLAG_COMPANION_2_SCHEDULE`.
///
/// The selector must be 0..10, as supplied by the story flag writers; extracting
/// a nibble alone does not enforce the bound. Area tables live with gameplay.
/// Entries use whole-byte presence and always select resource variant 0.
static _CompanionSchedule _gCompanionFamily2Schedules[COMPANION_FAMILY_2_SCHEDULE_COUNT] = {
    { NULL, GAME_STAGE_NONE },
    { _gCompanionFamily2Schedule1AreaPresence, GAME_STAGE_DRYFIELD },
    { _gCompanionFamily2Schedule2AreaPresence, GAME_STAGE_DRYFIELD },
    { _gCompanionFamily2Schedule3AreaPresence, GAME_STAGE_DRYFIELD },
    { _gCompanionFamily2Schedule2AreaPresence, GAME_STAGE_DRYFIELD },
    { _gCompanionFamily2Schedule5AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily2Schedule2AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily2Schedule7AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily2Schedule8AreaPresence, GAME_STAGE_SHELTER_NEO_ARK },
    { _gCompanionFamily2Schedule9AreaPresence, GAME_STAGE_SHELTER_NEO_ARK },
    { _gCompanionFamily2Schedule10AreaPresence, GAME_STAGE_MINE_SHELTER },
};
/// Family-1 stage and packed area schedules selected by `GAME_FLAG_COMPANION_1_SCHEDULE`.
///
/// The selector must be 0..10. Schedule 0 is absent; present entries encode
/// resource variants 1..5, with variant 5 remapped to 3 after its package loads.
/// Each borrowed area table remains live for the gameplay overlay's lifetime.
static _CompanionSchedule _gCompanionFamily1Schedules[COMPANION_FAMILY_1_SCHEDULE_COUNT] = {
    { NULL, GAME_STAGE_NONE },
    { _gCompanionFamily1Schedule1AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily1Schedule2AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily1Schedule3AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily1Schedule4AreaPresence, GAME_STAGE_MINE_SHELTER },
    { _gCompanionFamily1Schedule5AreaPresence, GAME_STAGE_MINE_SHELTER },
    { _gCompanionFamily1Schedule6AreaPresence, GAME_STAGE_DRYFIELD_NIGHT },
    { _gCompanionFamily1Schedule7AreaPresence, GAME_STAGE_MINE_SHELTER },
    { _gCompanionFamily1Schedule8AreaPresence, GAME_STAGE_MINE_SHELTER },
    { _gCompanionFamily1Schedule9AreaPresence, GAME_STAGE_SHELTER_NEO_ARK },
    { _gCompanionFamily1Schedule10AreaPresence, GAME_STAGE_MINE_SHELTER },
};
/// Family-3 absence/presence schedules selected by `GAME_FLAG_COMPANION_3_SCHEDULE`.
///
/// The selector must be 0 or 1. Schedule 1 uses whole-byte presence in
/// Shelter/Neo Ark and resource variant 0, borrowing its gameplay-owned table.
static _CompanionSchedule _gCompanionFamily3Schedules[COMPANION_FAMILY_3_SCHEDULE_COUNT] = {
    { NULL, GAME_STAGE_NONE },
    { _gCompanionFamily3Schedule1AreaPresence, GAME_STAGE_SHELTER_NEO_ARK },
};
/// Night Dryfield area presence for family-1 schedule 1, using resource variant 1.
static u8 _gCompanionFamily1Schedule1AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1]       = COMPANION_PACK_AREA_PRESENCE(1),
    [GAME_AREA_DRYFIELD_NIGHT_GENERAL_STORE - 1]     = COMPANION_PACK_AREA_PRESENCE(1),
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET - 1]       = COMPANION_PACK_AREA_PRESENCE(1),
    [GAME_AREA_DRYFIELD_NIGHT_SOUVENIR_SHOP - 1]     = COMPANION_PACK_AREA_PRESENCE(1),
    [GAME_AREA_DRYFIELD_NIGHT_WAREHOUSE - 1]         = COMPANION_PACK_AREA_PRESENCE(1),
    [GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE - 1] = COMPANION_PACK_AREA_PRESENCE(1),
};

/// Night gas-station presence for family-1 schedule 2, using resource variant 1.
static u8 _gCompanionFamily1Schedule2AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION - 1] = COMPANION_PACK_AREA_PRESENCE(1),
};

/// Night garage presence for family-1 schedule 3, using resource variant 1.
static u8 _gCompanionFamily1Schedule3AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_NIGHT_GARAGE - 1] = COMPANION_PACK_AREA_PRESENCE(1),
};

/// Mine mesa presence for family-1 schedule 4, using resource variant 1.
static u8 _gCompanionFamily1Schedule4AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT] = {
    [GAME_AREA_MINE_MESA - 1] = COMPANION_PACK_AREA_PRESENCE(1),
};

/// Shelter B2 elevator and B3/B4 presence for family-1 schedule 5, using variant 3.
static u8 _gCompanionFamily1Schedule5AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT] = {
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                 = COMPANION_PACK_AREA_PRESENCE(3),
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1] = COMPANION_PACK_AREA_PRESENCE(3),
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]            = COMPANION_PACK_AREA_PRESENCE(3),
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]              = COMPANION_PACK_AREA_PRESENCE(3),
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]              = COMPANION_PACK_AREA_PRESENCE(3),
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                = COMPANION_PACK_AREA_PRESENCE(3),
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]             = COMPANION_PACK_AREA_PRESENCE(3),
};

/// Night Water Hole presence for family-1 schedule 6, loading variant 5 before remapping to 3.
static u8 _gCompanionFamily1Schedule6AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE - 1] = COMPANION_PACK_AREA_PRESENCE(5),
};

/// Day factory, garage, driveway and junk-yard presence for family-2 schedule 1.
static u8 _gCompanionFamily2Schedule1AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_FACTORY - 1]   = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_GARAGE - 1]    = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_DRIVEWAY - 1]  = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1] = COMPANION_AREA_PRESENT,
};

/// Trailer-coach presence shared by family-2 schedules 2/4 (day) and 6 (night).
static u8 _gCompanionFamily2Schedule2AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1] = COMPANION_AREA_PRESENT,
};

/// Day junk-yard and trailer-coach presence for family-2 schedule 3.
static u8 _gCompanionFamily2Schedule3AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_JUNK_YARD - 1]     = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_TRAILER_COACH - 1] = COMPANION_AREA_PRESENT,
};

/// Night main-street presence for family-2 schedule 5.
static u8 _gCompanionFamily2Schedule5AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET - 1] = COMPANION_AREA_PRESENT,
};

/// Night water-tower, breezeway, factory and driveway presence for family-2 schedule 7.
static u8 _gCompanionFamily2Schedule7AreaPresence[COMPANION_DRYFIELD_AREA_COUNT] = {
    [GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER - 1] = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_NIGHT_BREEZEWAY - 1]   = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_NIGHT_FACTORY - 1]     = COMPANION_AREA_PRESENT,
    [GAME_AREA_DRYFIELD_NIGHT_DRIVEWAY - 1]    = COMPANION_AREA_PRESENT,
};

/// Shelter 1F heliport presence for family-2 schedule 8.
static u8 _gCompanionFamily2Schedule8AreaPresence[COMPANION_SHELTER_NEO_ARK_AREA_COUNT] = {
    [GAME_AREA_SHELTER_1F_HELIPORT - 1] = COMPANION_AREA_PRESENT,
};

/// Shelter 1F parking-garage through airlock presence for family-2 schedule 9.
static u8 _gCompanionFamily2Schedule9AreaPresence[COMPANION_SHELTER_NEO_ARK_AREA_COUNT] = {
    [GAME_AREA_SHELTER_1F_PARKING_GARAGE - 1]    = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK - 1] = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_1F_BULWARK - 1]           = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_1F_HELIPORT - 1]          = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_1F_AIRLOCK - 1]           = COMPANION_AREA_PRESENT,
};

/// Shelter B1 corridor, sterilization, access, parking and transfer presence for family-2 schedule 10.
static u8 _gCompanionFamily2Schedule10AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT] = {
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR - 1]       = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_B1_STERILIZATION_ROOM - 1]  = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL - 1]       = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING - 1] = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL - 1]     = COMPANION_AREA_PRESENT,
};

/// Shelter R47 presence for family-1 schedule 7, using resource variant 2.
static u8 _gCompanionFamily1Schedule7AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT] = {
    [GAME_AREA_SHELTER_R47 - 1] = COMPANION_PACK_AREA_PRESENCE(2),
};

/// Shelter B1/B2 route and R47 presence for family-1 schedule 8, using variant 2.
static u8 _gCompanionFamily1Schedule8AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT] = {
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL - 1] = COMPANION_PACK_AREA_PRESENCE(2),
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR - 1]     = COMPANION_PACK_AREA_PRESENCE(2),
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK - 1]       = COMPANION_PACK_AREA_PRESENCE(2),
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL - 1] = COMPANION_PACK_AREA_PRESENCE(2),
    [GAME_AREA_SHELTER_R47 - 1]                  = COMPANION_PACK_AREA_PRESENCE(2),
};

/// Neo Ark observatory presence for family-1 schedule 9, using resource variant 2.
static u8 _gCompanionFamily1Schedule9AreaPresence[COMPANION_SHELTER_NEO_ARK_AREA_COUNT] = {
    [GAME_AREA_NEO_ARK_OBSERVATORY - 1] = COMPANION_PACK_AREA_PRESENCE(2),
};

/// Shelter B2 elevator and B3/B4 presence for family-1 schedule 10, using variant 4.
static u8 _gCompanionFamily1Schedule10AreaPresence[COMPANION_MINE_SHELTER_AREA_COUNT] = {
    [GAME_AREA_SHELTER_B2_ELEVATOR - 1]                 = COMPANION_PACK_AREA_PRESENCE(4),
    [GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM - 1] = COMPANION_PACK_AREA_PRESENCE(4),
    [GAME_AREA_SHELTER_B3_ELEVATOR_HALL - 1]            = COMPANION_PACK_AREA_PRESENCE(4),
    [GAME_AREA_SHELTER_B4_LOWER_SEWER - 1]              = COMPANION_PACK_AREA_PRESENCE(4),
    [GAME_AREA_SHELTER_B4_UPPER_SEWER - 1]              = COMPANION_PACK_AREA_PRESENCE(4),
    [GAME_AREA_SHELTER_B4_RESERVOIR - 1]                = COMPANION_PACK_AREA_PRESENCE(4),
    [GAME_AREA_SHELTER_B4_WATER_SUPPLY - 1]             = COMPANION_PACK_AREA_PRESENCE(4),
};

/// Shelter B6 nursery and growth-room presence for family-3 schedule 1.
///
/// Only the first `COMPANION_SHELTER_NEO_ARK_AREA_COUNT` bytes are area entries.
/// The final three bytes are retained storage of unproven role and ownership;
/// they do not extend the valid area domain.
static u8 _gCompanionFamily3Schedule1AreaPresence[COMPANION_FAMILY_3_SCHEDULE_1_STORAGE_BYTES] = {
    [GAME_AREA_SHELTER_B6_NURSERY - 1]         = COMPANION_AREA_PRESENT,
    [GAME_AREA_SHELTER_B6_GROWTH_ROOM - 1]     = COMPANION_AREA_PRESENT,
    [COMPANION_SHELTER_NEO_ARK_AREA_COUNT]     = 0x3D,
    [COMPANION_SHELTER_NEO_ARK_AREA_COUNT + 1] = 0xF0,
    [COMPANION_SHELTER_NEO_ARK_AREA_COUNT + 2] = 0x71,
};

s32 companionSelectForArea(void)
{
    McSaveData* save;
    u8*         areaPresence;
    s32         stage;
    u8          variant;

    save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    stage = save->state.location.stage;
    // The first scheduled family present wins: 2, then 1, then 3.
    if (COMPANION_SCHEDULE_HAS_AREA(_gCompanionFamily2Schedules, GAME_FLAG_COMPANION_2_SCHEDULE, stage, save->state.location.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
        GameSession* session = gGameSession;

        save->state.companionType    = COMPANION_TYPE_FAMILY_2;
        save->state.companionVariant = COMPANION_BASE_RESOURCE_VARIANT;
        return (session->companionType != COMPANION_TYPE_FAMILY_2) * COMPANION_TYPE_FAMILY_2;
    }

    if (COMPANION_SCHEDULE_HAS_AREA(_gCompanionFamily1Schedules, GAME_FLAG_COMPANION_1_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area, COMPANION_FAMILY_1_PRESENCE_MASK, areaPresence)) {
        GameSession* session = gGameSession;

        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType = COMPANION_TYPE_FAMILY_1;
        if (session->companionType == COMPANION_TYPE_FAMILY_1) {
            variant = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area - 1] >> COMPANION_FAMILY_1_VARIANT_SHIFT;
            if (session->companionVariant == variant) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = variant;
                return COMPANION_NO_RESOURCE_LOAD;
            }
        }
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area - 1] >> COMPANION_FAMILY_1_VARIANT_SHIFT;
        gGameSession->companionVariant                            = areaPresence[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area - 1] >> COMPANION_FAMILY_1_VARIANT_SHIFT;
        return COMPANION_TYPE_FAMILY_1;
    }

    if (COMPANION_SCHEDULE_HAS_AREA(_gCompanionFamily3Schedules, GAME_FLAG_COMPANION_3_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
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
    stage = save->state.location.stage;
    if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, 0, 0)) {
        if (COMPANION_SCHEDULE_HAS_AREA(_gCompanionFamily2Schedules, GAME_FLAG_COMPANION_2_SCHEDULE, stage, save->state.location.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
            return 1;
        }
        if (COMPANION_SCHEDULE_HAS_AREA(_gCompanionFamily1Schedules, GAME_FLAG_COMPANION_1_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area, COMPANION_FAMILY_1_PRESENCE_MASK, areaPresence)) {
            return 1;
        }
        if (COMPANION_SCHEDULE_HAS_AREA(_gCompanionFamily3Schedules, GAME_FLAG_COMPANION_3_SCHEDULE, stage, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area, COMPANION_FULL_PRESENCE_MASK, areaPresence)) {
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

    bank                  = gGameFlagStageHeaders[stageId];
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

    bank = gGameFlagStageHeaders[location->stage];
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
    TaskFuncTable3 stateHandlers;

    stateHandlers = gGameFlowReloadStateHandlers;
    padStartInputBlock(0);
    // Keep the pause store before the state read that selects the next phase.
    *(volatile u8*)&gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
    stateHandlers.funcs[((volatile Task*)task)->state](task);
}

void loadingFinishSessionLoadTask(Task* task)
{
    enum {
        FADE_SESSION_RESUME_TASK_BANK      = 0,
        FADE_SESSION_RESUME_TASK_SLOT      = 0x21,
        AKROPOLIS_PLAZA_OPENING_TASK_INDEX = AKROPOLIS_PLAZA_TASK_SEQUENCE,
    };
    if (gCdCmdQueue.bootLoadActive == 0) {
        // Retire loading before room startup can reuse its task storage.
        gpuClearFrameOrderingTable(0);
        gpuClearFrameOrderingTable(1);
        gGameDebugState->loadingActive = GAME_DEBUG_LOADING_IDLE;
        taskKill(task);
        if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 0, 0)) {
            areaStartRoomRuntime(AREA_ROOM_START_SKIP_VIEW_GATE);
        } else {
            areaStartRoomRuntime(AREA_ROOM_START_WITH_VIEW_GATE);
        }
        gDisplayState.holdCount  = 0;
        gDisplayState.holdState &= DISPLAY_HOLD_MODE_MASK;
        displayAcquireMenuHold();
        taskSpawn(FADE_SESSION_RESUME_TASK_BANK, FADE_SESSION_RESUME_TASK_SLOT, 0, 0);
        if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 0, 0)) {
            taskSpawnFromTable(gAkropolisPlazaSequenceTaskDescriptors, AKROPOLIS_PLAZA_OPENING_TASK_INDEX, 0, 0);
            cdCmdReservePlaybackBuffers();
            cdCmdSelectMovieWorkspace();
        }
    }
}

void loadingSessionLoadTask(Task* task)
{
    TaskFuncTable8 stateHandlers;
    DisplayState*  displayState;

    stateHandlers = gLoadingSessionStateHandlers;
    padStartInputBlock(0);
    displayState = &gDisplayState;
    if (displayState->demoScene != DISPLAY_DEMO_NONE) {
        if (padReadRawButtons(0) & PAD_BUTTON_START) {
            if (cdCmdIsIdle()) {
                gGameMainPersistentState.skipTitleIntro = 1;
                displayState->gameMode                  = DISPLAY_GAME_RESTART;
                return;
            }
        }
    }
    stateHandlers.funcs[task->state](task);
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
/// `roomResources` is NULL or a loaded room array containing `location->roomState - 1`;
/// the room index is one-based. Both pointers are borrowed for this call.
/// Each non-NULL trigger/occluder list must contain its LAST-marked entry and
/// remain writable and loaded until unlinked. Each record must be unlinked or
/// already linked in its selected list; existing registrations remain live.
/// A NULL array or grid retains the published grid.
/// A non-NULL grid is writable and must stay loaded until replaced or retired.
/// Triggers borrow the current view coordinate; occluder coordinates are unchanged.
static inline void _loadingBindInitialRoomCollisionResources(const GameLocationKey* location, const WorldCollisionRoomResources* roomResources)
{
    enum { WORLD_COLLISION_OCCLUDER_LIST_ACTIVE = 0 };
    WorldCollisionGrid*     grid;
    WorldCollisionTrigger*  viewBoundaryTriggers;
    WorldCollisionTrigger*  actionTriggers;
    WorldCollisionOccluder* occluders;
    s32                     resourceIndex;

    if (roomResources != NULL) {
        grid                 = roomResources[location->roomState - 1].grid;
        viewBoundaryTriggers = roomResources[location->roomState - 1].viewBoundaryTriggers;
        actionTriggers       = roomResources[location->roomState - 1].actionTriggers;
        occluders            = roomResources[location->roomState - 1].occluders;
        if (grid != NULL) {
            // Bind the room mesh to the current view before publishing it.
            grid->viewCoord           = &gGfxViewCoord;
            gWorldCollisionActiveGrid = grid;
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

    location      = &gGameSession->location;
    roomResources = gWorldCollisionStageResources[location->stage - 1]->areaRooms[location->area - 1];
    _loadingBindInitialRoomCollisionResources(location, roomResources);
    // The room task owns the sprite task in its teardown tree.
    viewSpriteTask = taskSpawn(LOADING_VIEW_SPRITE_TASK_BANK, LOADING_VIEW_SPRITE_TASK_TYPE, 0, 0);
    if (viewSpriteTask != NULL) {
        taskReparent(task, viewSpriteTask);
    }
    gGameSession->roomObjsDirty = 0;
    task->state++;
}
