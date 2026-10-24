#include "rooms/mist_parking.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "mist_parking_private.h"
#include "mist_parking_head_aim.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
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
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/akropolis_square.h"

#include "rooms/room.h"

#include "rooms/shop_tier.h"
#include "../../../../shared/room/room_cutscene.h"

#define ROOM_PART declarations_mist_parking
#include "assets/rooms/mist/parking/declarations.inc"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

static s32 _gShopPreviewRowId;

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

#include "../../../../shared/room/telephone.h"

/// Binds the shop's animated refill quantity to this room's writable s32 storage.
#define SHOP_REFILL_DISPLAY_QUANTITY gShopRefillDisplayQuantityFixed

/// Binds the refill panel's borrowed catalog selection to this room's pointer storage.
#define SHOP_REFILL_SUPPLY gShopRefillSupply
#include "../../../../shared/room/shop.h"

static s32  _mistParkingHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _mistParkingRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _mistParkingResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _mistParkingHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static void _mistParkingSelectChapterRoom(s32 roomId);
static void _mistParkingSpawnCapEventIfIdle(s32 commandIndex);

#include "../../../../shared/room/shop_data.inc.c"

TaskDesc gMistParkingShopSessionTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shopSessionTask, { .value = 0 } };

#include "../../../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "assets/rooms/mist/parking/clips_1.inc"

/// Room receiver for transition replies, direction actions, key-item refusal and CAP commands.
static TaskMessageEntry _gMistParkingRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mistParkingResolveRoomTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mistParkingHandleRoomAction },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mistParkingRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _mistParkingHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Single player clip installed at extension index zero for room command 1.

#define ROOM_PART _gMistParkingRoom2PlayerPlacement
#include "assets/rooms/mist/parking/events.inc"

#include "assets/rooms/mist/parking/command1_player_bank.inc"

#define ROOM_PART _gMistParkingCommand1PlayerBankCopy
#include "assets/rooms/mist/parking/events.inc"
#include "assets/rooms/mist/parking/hinged_panel.inc"

#include "assets/rooms/mist/parking/clips_2.inc"

static void _mistParkingInitializeRoomTask(Task* task);

static void _mistParkingIdleRoomTask(Task* task);

/// Retained byte following this room's weapon-supply refill title terminator.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\xE2"
#include "../../../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

/// Retained bytes following this room's telephone menu title terminator.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xF2\xEF"
#include "../../../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void mistParkingTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../../../shared/room/telephone_panels.inc.c"

/// The area records applied when the scene hands the Dryfield story on.

#include "../../../../shared/room/room_cutscene_task.inc.c"

/// Queues the parking cutscene and selects its saved/live arrival warp.
static inline void _mistParkingStartCommandCutscene(void)
{
    enum {
        MIST_PARKING_CUTSCENE_DESCRIPTOR = 0,
        MIST_PARKING_CUTSCENE_COMMAND    = 4,
        MIST_PARKING_CUTSCENE_VIEW       = 9,
        MIST_PARKING_CUTSCENE_CAP_SLOT   = 1,
        MIST_PARKING_CUTSCENE_CAP_FILE   = 3,
        MIST_PARKING_CUTSCENE_PLAY       = 0,
        MIST_PARKING_CUTSCENE_WARP       = 2
    };
    GameSession* session;

    // Configure playback before queuing the task that borrows the record.
    gMistParkingCommandCutscene.view            = MIST_PARKING_CUTSCENE_VIEW;
    gMistParkingCommandCutscene.capSlot         = MIST_PARKING_CUTSCENE_CAP_SLOT;
    gMistParkingCommandCutscene.capFile         = MIST_PARKING_CUTSCENE_CAP_FILE;
    gMistParkingCommandCutscene.skipScene       = MIST_PARKING_CUTSCENE_PLAY;
    gMistParkingCommandCutscene.startSound      = MIST_PARKING_SOUND(MIST_PARKING_SOUND_CUTSCENE_START);
    gMistParkingCommandCutscene.endSound        = MIST_PARKING_SOUND(MIST_PARKING_SOUND_CUTSCENE_END);
    gMistParkingCommandCutscene.sceneSound      = MIST_PARKING_SOUND(MIST_PARKING_SOUND_CUTSCENE_PLAYBACK);
    gMistParkingCommandCutscene.afterSceneSound = MIST_PARKING_SOUND(MIST_PARKING_SOUND_CUTSCENE_AFTER);
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, MIST_PARKING_CUTSCENE_DESCRIPTOR, MIST_PARKING_CUTSCENE_COMMAND, &gMistParkingCommandCutscene);
    session                                                = gGameSession;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = MIST_PARKING_CUTSCENE_WARP;
    session->location.warp                                 = MIST_PARKING_CUTSCENE_WARP;
}

/// Room commands: 15 the follow-ups and the departure menu, 18 Jodie's talk, 8 the cutscene, 1 its script.
static s32 _mistParkingHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
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

    switch (command) {
        case MIST_PARKING_COMMAND_DEPARTURE_MENU:
            areaVariant = gGameSession->location.variant;
            if (areaVariant == MIST_PARKING_FOLLOWUP_VARIANT) {
                if (gameFlagGetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE) == MIST_PARKING_CONVERSATION_INTRO_COMPLETE) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    evsStartScript(gMistParkingFirstFollowupScript, EVENT_SCRIPT_HUD_KEEP);
                    gameFlagSetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE, MIST_PARKING_CONVERSATION_FIRST_FOLLOWUP_STARTED);
                } else if (gameFlagGetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE) == areaVariant) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    evsStartScript(gMistParkingSecondFollowupScript, EVENT_SCRIPT_HUD_KEEP);
                    gameFlagSetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE, MIST_PARKING_CONVERSATION_SECOND_FOLLOWUP_STARTED);
                } else if (gameFlagGetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE) == MIST_PARKING_CONVERSATION_SECOND_FOLLOWUP_STARTED) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_DEPARTURE_MENU_DESCRIPTOR, 0, 0);
                }
            } else if (gameFlagGetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT) == MIST_PARKING_DEPARTURE_TALK_ENABLED) {
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
static const TaskFuncTable3 _gMistParkingRoomStateHandlers = {
    {
        _mistParkingInitializeRoomTask,
        _mistParkingIdleRoomTask,
        taskKill,
    },
};

#include "../../../../shared/room/room_cutscene_sound_task.inc.c"

/// No key item can be used here.
static s32 _mistParkingRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Every door departs as it is.
static s32 _mistParkingResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return ROOM_EVENT_DEPART;
}

/// Starts the chapter-dependent room-change script selected by a direction trigger.
static s32 _mistParkingHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
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
        gameFlagSetNibble(EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT, MIST_PARKING_TALK_ENABLED);
    }
    return 1;
}

/// Selects the parking room for the current story chapter and requests its objects again.
static void _mistParkingSelectChapterRoom(s32 roomId)
{
    enum { MIST_PARKING_LATER_CHAPTER_ROOM_OFFSET = 2 };

    if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) != EVENT_FLAG_STORY_CHAPTER_MIST_ARRIVAL) {
        roomId += MIST_PARKING_LATER_CHAPTER_ROOM_OFFSET;
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = roomId;
    gGameSession->location.roomState                            = roomId;
    gGameSession->roomObjsDirty                                 = 1;
}

/// Queues the selected CAP command while idle, without event hold/hide flags.
static void _mistParkingSpawnCapEventIfIdle(s32 commandIndex)
{
    capSpawnEventIfIdle(commandIndex, CAP_EVENT_NO_FLAGS);
}

/// Registers the room receiver and starts its pending arrival conversation.
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
        (gameFlagGetNibble(EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE) == MIST_PARKING_CONVERSATION_INTRO_PENDING)) {
        // Arrival has a skip path; other entries use the shorter room-entry script.
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == MIST_PARKING_ARRIVAL_TALK_WARP) {
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, MIST_PARKING_ARRIVAL_TALK_OBJECTIVE);
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
static void _mistParkingIdleRoomTask(Task* task)
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
