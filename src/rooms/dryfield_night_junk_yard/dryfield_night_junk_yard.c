#include "rooms/dryfield_night_junk_yard.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/sprites.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/areas.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

static s32 _junkYardCommandMessage(Task* task, s32 messageId, s32 command, s32 unusedSecondArg);

static s32 _dryfieldNightJunkYardRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _roomVariantNightJunkYardMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightJunkYardHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static void _dryfieldNightJunkYardSetRoomNumber(u8 roomNumber);

/// Room-local actor commands and encounter-script selectors.
enum {
    DRYFIELD_NIGHT_JUNK_YARD_STALKER_COMMAND_JUNK_YARD_DROP = 0,
    DRYFIELD_NIGHT_JUNK_YARD_STALKER_COMMAND_FLOOR_DROP     = 1,
    DRYFIELD_NIGHT_JUNK_YARD_STALKER_COMMAND_FLOOR_START    = 2,
    DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_CAP_COMMAND          = 13,
    DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_PLAYER_CLIP          = 1,
    DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_ROOM                 = 2,
    DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_VIEW                 = 4,
    DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_END_WAIT_FRAMES      = 3,
};

/// Message callbacks borrowed by the room task until its teardown.
///
/// Four supported IDs precede the end marker; payloads follow each handler's
/// contract. The room overlay must remain loaded while the task receives messages.
static TaskMessageEntry _gDryfieldNightJunkYardMessageHandlers[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantNightJunkYardMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightJunkYardRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightJunkYardHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _junkYardCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Player clip 1 requested at the start of the Stalker encounter.
///
/// The interpreter copies the request and replaces the stored bank selector
/// with the equipped weapon's bank. Playback resets with grid collision enabled.
static AnimationPlayRequest _gDryfieldNightJunkYardEncounterPlayerAnimation = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Broadcast command starting the first Stalker's elevated Junk Yard entrance.
///
/// The second Stalker keeps waiting. The script borrows this record until dispatch.
static ActorCommand _gDryfieldNightJunkYardStalkerJunkYardDropCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD } },
    .command = DRYFIELD_NIGHT_JUNK_YARD_STALKER_COMMAND_JUNK_YARD_DROP,
};

/// Broadcast command starting the second Stalker's floor-drop entrance.
///
/// Issued after battle engagement and player release; borrowed until dispatch.
static ActorCommand _gDryfieldNightJunkYardStalkerFloorDropCommand = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD } },
    .command = DRYFIELD_NIGHT_JUNK_YARD_STALKER_COMMAND_FLOOR_DROP,
};

/// Broadcast command restoring both Stalkers at their floor starts on room re-entry.
///
/// Variant 1 sends it when the encounter flag is set. The scene forwards the
/// borrowed record synchronously. The record keeps four-byte storage alignment.
static ActorCommand _gDryfieldNightJunkYardStalkerFloorStartCommand __attribute__((aligned(4))) = {
    .context = { .loc = { GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD } },
    .command = DRYFIELD_NIGHT_JUNK_YARD_STALKER_COMMAND_FLOOR_START,
};

/// CAP-cued two-Stalker encounter for night Junk Yard placement variant 1.
///
/// Starts CAP command 13 and the first drop, engages battle, selects room 2/view 4,
/// releases player control and starts the second drop. All seventeen records
/// and their payloads must remain loaded until the END instruction executes.
static EvsCommand _gDryfieldNightJunkYardEncounterScript[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldNightJunkYardEncounterPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightJunkYardStalkerJunkYardDropCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _dryfieldNightJunkYardSetRoomNumber }, { .value = DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_ROOM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &_gDryfieldNightJunkYardStalkerFloorDropCommand } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_END_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static void _dryfieldNightJunkYardInitializeRoom(Task* task);
static void _dryfieldNightJunkYardRoomIdle(Task* unusedTask);

#include "../../shared/junk_yard_cap_msg.inc.c"

/// Refuses key-item use in the night Junk Yard without changing inventory.
///
/// The item menu supplies the collected-item ID and a zero second payload.
/// All arguments are ignored; the refused reply requests the cannot-use notice.
static s32 _dryfieldNightJunkYardRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves trailer-coach departures and their one-time events from the night junk yard.
///
/// Receives `ROOM_EVENT_MESSAGE_RESOLVE` with a borrowed eight-byte request
/// and writable reply, which may alias. Returns 0 while flag 0x46 is 1,
/// starting CAP departure command 4 and setting the optional flag to 2 only
/// for execution. Otherwise returns 1 and selects first-entry or post-Burner
/// arrival records. Queries copy the request and suppress event effects.
static s32 _roomVariantNightJunkYardMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        DRYFIELD_NIGHT_JUNK_YARD_GARAGE_CHAPTER        = 4,
        DRYFIELD_NIGHT_JUNK_YARD_EVENT_CHAPTER         = 3,
        DRYFIELD_NIGHT_JUNK_YARD_DEPARTURE_CAP_COMMAND = 4,
        DRYFIELD_NIGHT_JUNK_YARD_FIRST_ENTRY_WARP      = 2,
    };
    RoomEventMsg unused; // Unused stack record retained for the matching frame.
    s32          burnerDefeated;
    s32          storyChapter;

    // Preserve the pre-copy store: aliased request/reply retains this room choice.
    if (request->areaId == GAME_AREA_DRYFIELD_NIGHT_GARAGE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) >= DRYFIELD_NIGHT_JUNK_YARD_GARAGE_CHAPTER) {
            reply->roomState = 2;
        } else {
            reply->roomState = 1;
        }
    }
    *reply = *request;
    if (request->areaId != GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_046) == 1) {
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        capRunCommandWithTransition(DRYFIELD_NIGHT_JUNK_YARD_DEPARTURE_CAP_COMMAND);
        gameFlagSetNibbleIfPresent(request->flagId, 2);
        return 0;
    }
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_FIRST_ENTRY) == 0 && request->queryOnly == ROOM_EVENT_EXECUTE) {
        reply->warp = DRYFIELD_NIGHT_JUNK_YARD_FIRST_ENTRY_WARP;
        gameFlagSetNibble(GAME_FLAG_NIGHT_TRAILER_COACH_FIRST_ENTRY, 1);
    }
    burnerDefeated = gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED);
    if (burnerDefeated != 1) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != burnerDefeated) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_NIGHT_EVENT_ENTRY) != 0) {
        return 1;
    }
    storyChapter = gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER);
    if (storyChapter != DRYFIELD_NIGHT_JUNK_YARD_EVENT_CHAPTER) {
        return 1;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return 1;
    }
    reply->warp = storyChapter;
    gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_NIGHT_EVENT_ENTRY, 1);
    return 1;
}

/// Starts the junk-yard encounter once when its direction trigger is entered.
///
/// Receives `DIRECTION_MESSAGE_ROOM_ACTION`, borrowing its four-byte request
/// only during dispatch. Action 3 in variant 1 latches the event before
/// starting its script with HUD restoration. Other actions do nothing;
/// the second payload is ignored and the result is always 0.
static s32 _dryfieldNightJunkYardHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { DRYFIELD_NIGHT_JUNK_YARD_ACTION_START_ENCOUNTER = 3,
           DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_VARIANT      = 1 };

    if ((request->actionId == DRYFIELD_NIGHT_JUNK_YARD_ACTION_START_ENCOUNTER) && (gGameSession->location.variant == DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_VARIANT) && (gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN, 1);
        evsStartScript(_gDryfieldNightJunkYardEncounterScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Sets the room number in both the active session and the live save.
///
/// The event script passes room number 2 after engaging the battle. The byte
/// is stored unchanged, without loading resources or validating the room number.
static void _dryfieldNightJunkYardSetRoomNumber(u8 roomNumber)
{
    gGameSession->location.roomState                            = roomNumber;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomNumber;
}

/// Registers the room and restores the encounter's actor and sprite state.
///
/// State 0 publishes the message table and room slot. In variant 1 after the
/// event, broadcasts the saved actor command to the live scene manager.
/// Applies the event flag to the view-7 sprite batch and advances to idle.
static void _dryfieldNightJunkYardInitializeRoom(Task* task)
{
    enum { DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_VARIANT = 1 };
    u8 variant;

    task->msgTable = _gDryfieldNightJunkYardMessageHandlers;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    variant = gGameSession->location.variant;
    if (variant == DRYFIELD_NIGHT_JUNK_YARD_ENCOUNTER_VARIANT && gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN) == variant) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gDryfieldNightJunkYardStalkerFloorStartCommand, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    dryfieldNightJunkYardSetEventSpriteBatchHidden(gameFlagGetNibble(GAME_FLAG_NIGHT_JUNK_YARD_EVENT_SEEN));
    task->state = task->state + 1;
}

/// Keeps the Junk Yard room task alive to receive messages.
static void _dryfieldNightJunkYardRoomIdle(Task* unusedTask)
{
}

/// Room-task callbacks indexed by `Task::state` (0 initialize, 1 idle, 2 teardown).
static const TaskFuncTable3 _gDryfieldNightJunkYardRoomTaskStates = {
    .funcs = { _dryfieldNightJunkYardInitializeRoom, _dryfieldNightJunkYardRoomIdle, taskKill },
};

void dryfieldNightJunkYardRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gDryfieldNightJunkYardRoomTaskStates;
    states.funcs[task->state](task);
}

void dryfieldNightJunkYardSetEventSpriteBatchHidden(u8 hidden)
{
    enum { DRYFIELD_NIGHT_JUNK_YARD_EVENT_SPRITE_VIEW  = 7,
           DRYFIELD_NIGHT_JUNK_YARD_EVENT_SPRITE_BATCH = 5 };
    const GameLocationKey* location = &gGameSession->location;
    SpriteBatch*           batches;

    batches = gSpriteAreaTables[location->stage - 1][0].areaViews[location->area - 1][DRYFIELD_NIGHT_JUNK_YARD_EVENT_SPRITE_VIEW - 1].batches;
    if (hidden == 0) {
        batches[DRYFIELD_NIGHT_JUNK_YARD_EVENT_SPRITE_BATCH].hidden = 0;
    } else {
        batches[DRYFIELD_NIGHT_JUNK_YARD_EVENT_SPRITE_BATCH].hidden = 1;
    }
}
