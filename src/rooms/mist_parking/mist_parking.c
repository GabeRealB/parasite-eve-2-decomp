#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "mist_parking_private.h"
#include "mist_parking_head_aim.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/akropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
#include "../../shared/room/room_cutscene.h"

#define ROOM_PART declarations_mist_parking
#include "assets/rooms/mist/parking/declarations.inc"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

static s32 _gShopPreviewRowId;

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

#include "../../shared/room/telephone.h"

/// Binds the shop's animated refill quantity to this room's writable s32 storage.
///
/// Defined in mist_parking_5.c and declared in the private header.
/// Uses eight fractional bits; keep the binding through shop.inc.c and the
/// storage alive through panel closure. Only one refill panel may use it.
#define SHOP_REFILL_DISPLAY_QUANTITY gShopRefillDisplayQuantityFixed

/// Binds the refill panel's borrowed catalog selection to this room's pointer storage.
///
/// Expands to the writable `const EquipmentWeaponSupply*` defined in
/// `mist_parking_5.c` and declared in the private header. Keep this binding through
/// `shop.inc.c`; `shop.h` specifies selection, lifetime and use by one panel at a time.
#define SHOP_REFILL_SUPPLY gShopRefillSupply
#include "../../shared/room/shop.h"

static s32  _mistParkingHandleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _mistParkingRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _mistParkingCopyRoomEventReply(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _mistParkingHandleDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static void _mistParkingSelectChapterRoom(s32 roomId);
static void _mistParkingSpawnCapEventIfIdle(s32 commandIndex);

#include "../../shared/room/shop_data.inc.c"

enum { MIST_PARKING_SHOP_SESSION_TASK_PRIORITY = 192 };

TaskDesc gMistParkingShopSessionTaskDesc = { { { TASK_BODY_NONE, MIST_PARKING_SHOP_SESSION_TASK_PRIORITY } }, _shopSessionTask, { .value = 0 } };

#include "../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ROOM_CUTSCENE_TASK_PRIORITY } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "assets/rooms/mist/parking/clips_1.inc"

/// Room receiver for transition replies, direction actions, key-item refusal and CAP commands.
///
/// The four callbacks interpret their payloads by message ID. The terminal
/// entry makes every unsupported message return zero. Borrowed by the room task.
static TaskMessageEntry _gMistParkingRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mistParkingCopyRoomEventReply },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mistParkingHandleDirectionAction },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mistParkingRefuseKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _mistParkingHandleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Single player clip installed at extension index zero for room command 1.
///
/// Playback borrows this room's clip data after the pointer word is copied.

#define ROOM_PART _gMistParkingRoom2PlayerPlacement
#include "assets/rooms/mist/parking/events.inc"

#include "assets/rooms/mist/parking/mist_parking_command1_player_bank.inc"

enum {
    MIST_PARKING_PLAYER_ROOM_TRANSITION_CLIP = 1,
    MIST_PARKING_ROOM_CHANGE_CAP_COMMAND     = 19,
    MIST_PARKING_ROOM_CHANGE_FADE_FRAMES     = 32,
    MIST_PARKING_ROOM_1_VIEW                 = 2,
    MIST_PARKING_ROOM_2_VIEW                 = 3,
    MIST_PARKING_COMMAND1_CAP_SLOT           = 1,
    MIST_PARKING_COMMAND1_RETURN_VIEW        = 5,
    MIST_PARKING_COMMAND1_AMBIENT_RGB        = 100,
    MIST_PARKING_COMMAND1_RESTORE_UPDATES    = 3,
    MIST_PARKING_EARLY_CHAPTER_ROOM_1        = 1,
    MIST_PARKING_EARLY_CHAPTER_ROOM_2        = 2
};

#define ROOM_PART _gMistParkingCommand1PlayerBankCopy
#include "assets/rooms/mist/parking/events.inc"
#include "assets/rooms/mist/parking/mist_parking_model_09B9C.inc"

#include "assets/rooms/mist/parking/clips_2.inc"

static void _mistParkingInitializeRoomTask(Task* task);

static void _mistParkingIdleRoomState(Task* task);

/// Retained byte following this room's weapon-supply refill title terminator.
///
/// One-byte string-literal binding for `shop.inc.c`; label drawing does not read it.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\xE2"
#include "../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xF2\xEF"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void mistParkingTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

/// The area records applied when the scene hands the Dryfield story on.

#include "../../shared/room/room_cutscene_task.inc.c"

/// Queues the parking cutscene and selects its saved/live arrival warp.
///
/// Borrows the room's singleton cutscene record until playback completes.
/// The loaded view/CAP/sound resources and parking overlay must stay live;
/// both warp selectors change even if the task cannot be allocated.
/// Requires exclusive use of the record until the queued task finishes.
static inline void _mistParkingStartCommandCutscene(void)
{
    enum {
        MIST_PARKING_CUTSCENE_DESCRIPTOR  = 0,
        MIST_PARKING_CUTSCENE_COMMAND     = 4,
        MIST_PARKING_CUTSCENE_VIEW        = 9,
        MIST_PARKING_CUTSCENE_CAP_SLOT    = 1,
        MIST_PARKING_CUTSCENE_CAP_FILE    = 3,
        MIST_PARKING_CUTSCENE_PLAY        = 0,
        MIST_PARKING_CUTSCENE_WARP        = 2,
        MIST_PARKING_CUTSCENE_START_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 3),
        MIST_PARKING_CUTSCENE_END_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 4),
        MIST_PARKING_CUTSCENE_SCENE_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 0xB),
        MIST_PARKING_CUTSCENE_AFTER_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_PARKING, 0x12)
    };
    GameSession* session;

    // Configure playback before queuing the task that borrows the record.
    gMistParkingCommandCutscene.view            = MIST_PARKING_CUTSCENE_VIEW;
    gMistParkingCommandCutscene.capSlot         = MIST_PARKING_CUTSCENE_CAP_SLOT;
    gMistParkingCommandCutscene.capFile         = MIST_PARKING_CUTSCENE_CAP_FILE;
    gMistParkingCommandCutscene.skipScene       = MIST_PARKING_CUTSCENE_PLAY;
    gMistParkingCommandCutscene.startSound      = MIST_PARKING_CUTSCENE_START_SOUND;
    gMistParkingCommandCutscene.endSound        = MIST_PARKING_CUTSCENE_END_SOUND;
    gMistParkingCommandCutscene.sceneSound      = MIST_PARKING_CUTSCENE_SCENE_SOUND;
    gMistParkingCommandCutscene.afterSceneSound = MIST_PARKING_CUTSCENE_AFTER_SOUND;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, MIST_PARKING_CUTSCENE_DESCRIPTOR, MIST_PARKING_CUTSCENE_COMMAND, &gMistParkingCommandCutscene);
    session                                                = gGameSession;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = MIST_PARKING_CUTSCENE_WARP;
    session->location.warp                                 = MIST_PARKING_CUTSCENE_WARP;
}

/// Routes parking CAP commands to the current variant's talks and cutscene.
///
/// Command 15 advances variant-2 follow-ups before offering departure, or offers
/// the other variant's departure menu when its talk flag is set. Command 18
/// starts the variant-appropriate Jodie shop talk, command 8 starts the configured
/// view-9 cutscene and sets saved/live warp 2, and command 1 starts its script.
/// Requires loaded room scripts, task tables and CAP resources. Receiver,
/// message ID and second word are ignored. Always returns zero, including
/// unrecognized commands and task-allocation failure.
static s32 _mistParkingHandleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        MIST_PARKING_COMMAND_SCRIPT                 = 1,
        MIST_PARKING_COMMAND_CUTSCENE               = 8,
        MIST_PARKING_COMMAND_DEPARTURE_MENU         = 15,
        MIST_PARKING_COMMAND_SHOP_TALK              = 18,
        MIST_PARKING_FOLLOWUP_VARIANT               = 2,
        MIST_PARKING_SHOP_VARIANT                   = 1,
        MIST_PARKING_DEPARTURE_TALK_ENABLED         = 1,
        MIST_PARKING_DEPARTURE_MENU_DESCRIPTOR      = 8,
        MIST_PARKING_SHOP_DEPARTURE_MENU_DESCRIPTOR = 4,
        MIST_PARKING_SHOP_TALK_DESCRIPTOR           = 3,
        MIST_PARKING_JODIE_TALK_DESCRIPTOR          = 7
    };
    u8 areaVariant;

    switch (commandId) {
        case MIST_PARKING_COMMAND_DEPARTURE_MENU:
            areaVariant = gGameSession->location.variant;
            if (areaVariant == MIST_PARKING_FOLLOWUP_VARIANT) {
                if (gameFlagGetNibble(GAME_FLAG_0F1) == MIST_PARKING_CONVERSATION_INTRO_COMPLETE) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    evsStartScript(gMistParkingFirstFollowupScript, EVENT_SCRIPT_HUD_KEEP);
                    gameFlagSetNibble(GAME_FLAG_0F1, MIST_PARKING_CONVERSATION_FIRST_FOLLOWUP_STARTED);
                } else if (gameFlagGetNibble(GAME_FLAG_0F1) == areaVariant) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    evsStartScript(gMistParkingSecondFollowupScript, EVENT_SCRIPT_HUD_KEEP);
                    gameFlagSetNibble(GAME_FLAG_0F1, MIST_PARKING_CONVERSATION_SECOND_FOLLOWUP_STARTED);
                } else if (gameFlagGetNibble(GAME_FLAG_0F1) == MIST_PARKING_CONVERSATION_SECOND_FOLLOWUP_STARTED) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_DEPARTURE_MENU_DESCRIPTOR, 0, 0);
                }
            } else if (gameFlagGetNibble(GAME_FLAG_0ED) == MIST_PARKING_DEPARTURE_TALK_ENABLED) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(gMistParkingShopTaskDescriptors, MIST_PARKING_SHOP_DEPARTURE_MENU_DESCRIPTOR, 0, 0);
            }
            break;
        case MIST_PARKING_COMMAND_CUTSCENE:
            _mistParkingStartCommandCutscene();
            break;
        case MIST_PARKING_COMMAND_SHOP_TALK:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            if (gGameSession->location.variant == MIST_PARKING_SHOP_VARIANT) {
                taskSpawnFromTable(gMistParkingShopTaskDescriptors, MIST_PARKING_SHOP_TALK_DESCRIPTOR, 0, 0);
            } else {
                taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_JODIE_TALK_DESCRIPTOR, 0, 0);
            }
            break;
        case MIST_PARKING_COMMAND_SCRIPT:
            evsStartScript(_gMistParkingCommand1Script, EVENT_SCRIPT_HUD_KEEP);
            break;
    }
    return 0;
}

/// Room task's three state handlers: initialize, keep the receiver alive and release.
///
/// Indexed by state 0..2 without bounds checks; copied to the dispatcher's stack.
static const TaskFuncTable3 _gMistParkingRoomStateHandlers = {
    {
        _mistParkingInitializeRoomTask,
        _mistParkingIdleRoomState,
        taskKill,
    },
};

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Refuses every key-item use request without changing the room or inventory.
///
/// All arguments are ignored; returns `ROOM_KEY_ITEM_USE_REFUSED` to the item menu.
static s32 _mistParkingRefuseKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room-transition request with its destination selectors unchanged.
///
/// Copies the complete eight-byte request into the reply and returns 1.
/// Both pointers must be live and may alias. The receiver and message ID are ignored.
static s32 _mistParkingCopyRoomEventReply(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return 1;
}

/// Starts the chapter-dependent room-change script selected by a direction trigger.
///
/// Action 1 selects base room 2, action 2 base room 1 and enables the parking
/// talk flag; later chapters add two to the room. Other actions do nothing.
/// Borrows the four-byte request through dispatch; its control and argument
/// are ignored, as are the receiver, message ID and second payload. Returns 1.
static s32 _mistParkingHandleDirectionAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    enum {
        MIST_PARKING_ACTION_SELECT_ROOM_2 = 1,
        MIST_PARKING_ACTION_SELECT_ROOM_1 = 2,
        MIST_PARKING_TALK_ENABLED         = 1
    };

    if (request->actionId == MIST_PARKING_ACTION_SELECT_ROOM_2) {
        evsStartScript(_gMistParkingSelectRoom2Script, EVENT_SCRIPT_HUD_KEEP);
    }
    if (request->actionId == MIST_PARKING_ACTION_SELECT_ROOM_1) {
        evsStartScript(_gMistParkingSelectRoom1Script, EVENT_SCRIPT_HUD_KEEP);
        gameFlagSetNibble(GAME_FLAG_0ED, MIST_PARKING_TALK_ENABLED);
    }
    return 1;
}

/// Selects the parking room for the current story chapter and requests its objects again.
///
/// The scripts supply room 1 or 2. A nonzero chapter selects rooms 3 or 4;
/// both the live session and saved location receive the low byte of the result.
static void _mistParkingSelectChapterRoom(s32 roomId)
{
    enum { MIST_PARKING_LATER_CHAPTER_ROOM_OFFSET = 2 };

    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) != 0) {
        roomId += MIST_PARKING_LATER_CHAPTER_ROOM_OFFSET;
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomId;
    gGameSession->location.roomState                            = roomId;
    gGameSession->roomObjsDirty                                 = 1;
}

/// Queues the selected CAP command while idle, without event hold/hide flags.
///
/// `commandIndex` must select a live loaded CAP command through playback.
/// Busy playback and allocation failure silently skip the request.
static void _mistParkingSpawnCapEventIfIdle(s32 commandIndex)
{
    capSpawnEventIfIdle(commandIndex, CAP_EVENT_NO_FLAGS);
}

/// Registers the room receiver and starts its pending arrival conversation.
///
/// State 0 advances to idle. Variant 2 with no intro progress starts the full
/// skippable arrival at warp 3, otherwise its shorter entry script. Setup
/// forgets the appropriate head-aim handles without releasing their tasks.
static void _mistParkingInitializeRoomTask(Task* task)
{
    enum {
        MIST_PARKING_ARRIVAL_TALK_VARIANT   = 2,
        MIST_PARKING_ARRIVAL_TALK_WARP      = 3,
        MIST_PARKING_ARRIVAL_TALK_OBJECTIVE = 0x3C
    };

    task->msgTable = _gMistParkingRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.variant == MIST_PARKING_ARRIVAL_TALK_VARIANT) &&
        (gameFlagGetNibble(GAME_FLAG_0F1) == MIST_PARKING_CONVERSATION_INTRO_PENDING)) {
        // Arrival has a skip path; other entries use the shorter room-entry script.
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == MIST_PARKING_ARRIVAL_TALK_WARP) {
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, MIST_PARKING_ARRIVAL_TALK_OBJECTIVE);
            mistParkingResetCutsceneTaskHandles(0);
            evsStartScriptWithSkip(gMistParkingArrivalConversationScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gMistParkingArrivalSkipScript);
        } else {
            mistParkingForgetShopHeadAimTaskHandle(0);
            evsStartScript(gMistParkingArrivalRoomEntryScript, EVENT_SCRIPT_HUD_KEEP);
        }
    }
    task->state = task->state + 1;
}

/// Keeps the room's message receiver alive between setup and teardown.
///
/// The otherwise empty callback retains its original 16-byte stack frame.
static void _mistParkingIdleRoomState(Task* task)
{
    char stackReservation[0x10];
}

void mistParkingRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gMistParkingRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void mistParkingAimPlayerHeadAtTalkPartnerTask(Task* task)
{
    const GameActor* player;
    Enemy*           talkPartner;
    s32              extensionIndex;
    s32              animationRequestsAim;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    if (gEvsPlaybackPaused == 0) {
        // Extension index zero has no aim flag; the copied bank bounds the lookup.
        extensionIndex = player->animationSlots[1].nextPose.indices.setIndex - ANIMATION_BANK_BASE_SET_COUNT;
        if ((extensionIndex > 0) && (extensionIndex < gMistParkingPlayerConversationBankCopy.count)) {
            animationRequestsAim = gMistParkingPlayerAnimationHeadAimFlags[extensionIndex];
        } else {
            animationRequestsAim = 0;
        }
        if (task->state == MIST_PARKING_HEAD_AIM_UPDATE) {
            _mistParkingRampPlayerHeadAimBlend(task, animationRequestsAim);
            // Placement index zero supplies the talk partner for this stage and area.
            talkPartner = sceneFindEnemyByPlaceKey(gGameSession->location.area | (gGameSession->location.stage << 8));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), talkPartner->task,
                                   MIST_PARKING_HEAD_AIM_MAX_YAW, MIST_PARKING_HEAD_AIM_MAX_PITCH, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}
