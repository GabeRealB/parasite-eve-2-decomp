#include "rooms/shelter_b1_armory.h"

#include "types.h"

#include "shelter_b1_armory_private.h"

#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"

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
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"

/// The 0xFFFF-terminated item id lists `func_shelter_b1_armory_8017D768`
/// chooses from, and the one it returns when no case matches.
static u16 Shop_Data_801815F8[];
static u16 Shop_Data_80181600[];
static u16 Shop_Data_80181608[];
static u16 Shop_Data_80181610[];
static u16 Shop_Data_80181620[];
static u16 Shop_Data_80181630[];
static u16 Shop_Data_80181640[];
static u16 Shop_Data_80181648[];
static u16 Shop_Data_80181658[];
static u16 Shop_Data_80181668[];
static u16 Shop_Data_80181678[];
static u16 Shop_Data_80181680[];
static u16 Shop_Data_80181694[];
static u16 Shop_Data_801816AC[];
static u16 Shop_Data_801816C0[];
static u16 Shop_Data_801816C8[];
static u16 Shop_Data_801816D8[];
static u16 Shop_Data_801816F0[];
static u16 Shop_Data_80181704[];
static u16 Shop_Data_8018170C[];
static u16 Shop_Data_80181720[];
static u16 Shop_Data_8018173C[];
static u16 Shop_Data_8018174C[];
static u16 Shop_Data_80181758[];
static u16 Shop_Data_80181770[];
static u16 Shop_Data_8018178C[];
static u16 Shop_Data_801817A0[];
static u16 Shop_Data_801817A8[];
static u16 Shop_Data_801817BC[];
static u16 Shop_Data_801817DC[];
static u16 Shop_Data_801817EC[];
static u16 Shop_Data_801817F8[];
static u16 Shop_Data_80181810[];
static u16 Shop_Data_80181814[];
static u16 Shop_Data_80181818[];
static u16 Shop_Data_80181820[];
static u16 Shop_Data_80181830[];
static u16 Shop_Data_80181838[];
static u16 Shop_Data_80181840[];
static u16 Shop_Data_80181848[];
static u16 Shop_Data_80181854[];
static u16 Shop_Data_8018185C[];
static u16 Shop_Data_80181868[];
static u16 Shop_Data_80181870[];
static u16 Shop_Data_8018187C[];
static u16 Shop_Data_80181888[];
static u16 Shop_Data_80181890[];
static u16 Shop_Data_80181898[];
static u16 Shop_Data_801818A4[];
static u16 Shop_Data_801818B0[];
static u16 Shop_Data_801818B8[];
static u16 Shop_Data_801818C4[];
static u16 Shop_Data_801818D0[];
static u16 Shop_Data_801818DC[];
static u16 Shop_Data_801818E0[];
static u16 Shop_Data_801818EC[];
static u16 Shop_Data_801818F8[];
static u16 Shop_Data_80181904[];
static u16 Shop_Data_8018190C[];
static u16 Shop_Data_80181918[];
static u16 Shop_Data_80181924[];
static u16 Shop_Data_80181930[];
static u16 Shop_Data_80181938[];
static u16 Shop_Data_80181944[];
static u16 Shop_Data_80181AD4[];

/// The shop's tier ladder.
static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT];

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

/// Messages and labels of the shop's panels.
static u8 Shop_Data_801819F0[];
static u8 Shop_Data_80181A04[];
static u8 Shop_Data_80181A0C[];
static u8 Shop_Data_80181A1C[];
static u8 Shop_Data_80181A20[];
static u8 Shop_Data_80181A5C[];
static u8 Shop_Data_80181A64[];
static u8 Shop_Data_80181A70[];
static u8 Shop_Data_80181A78[];
static u8 Shop_Data_80181A80[];
static u8 Shop_Data_80181A94[];
static u8 Shop_Data_80181AA4[];
static u8 Shop_Data_80181AC4[];
static u8 Shop_Data_80181AD0[];

/// Row handlers, lists and panel descriptors of the shop's panels.
static UiListRowCallback Shop_Data_80181AD8[];
static UiList            Shop_Data_80181AE0;
static UiList            Shop_Data_80181B0C;
static UiObjectDesc      Shop_Data_80181B30;
static UiObjectDesc      Shop_Data_80181B4C;
static UiObjectDesc      Shop_Data_80181B68;
static UiObjectDesc      Shop_Data_80181B84;
static UiObjectDesc      Shop_Data_80181BA0;
static UiObjectDesc      Shop_Data_80181BD8;
static UiObjectDesc      Shop_Data_80181BF4;
static UiObjectDesc      Shop_Data_80181C10;

/// Descriptor of the event task the door gate spawns, and the table the
/// room's own tasks are spawned from.
extern TaskDesc gRoomEventTaskDesc;
extern TaskDesc D_shelter_b1_armory_801824E8[];

/// Message handlers the room's controller task installs in pointer slot 7.
extern TaskMessageEntry D_shelter_b1_armory_80182500[];

static void _shelterB1ArmoryInitRoomState(Task* task);
static void _shelterB1ArmoryRoomIdleState(Task* task);

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xD3"
#include "../../shared/shop.h"

static s32 _shelterB1ArmoryUseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _shelterB1ArmoryResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1ArmoryHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);
static s32 _shelterB1ArmoryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg);

static void _shelterB1ArmoryCardkeyEventTask(Task* task);
static void _shelterB1ArmoryShopSceneTask(Task* task);

// Card-event spawnArg1 packs a CAP slot above a 16-bit result selector.
enum {
    SHELTER_B1_ARMORY_TASK_CARDKEY_EVENT       = 0,
    SHELTER_B1_ARMORY_TASK_SHOP_SCENE          = 1,
    SHELTER_B1_ARMORY_CARD_CAP_SHIFT           = 16,
    SHELTER_B1_ARMORY_CARD_RESULT_REFUSED      = 1,
    SHELTER_B1_ARMORY_CARD_RESULT_UNLOCKED     = 2,
    SHELTER_B1_ARMORY_CARD_RESULT_ALREADY_OPEN = 3,
    SHELTER_B1_ARMORY_CAP_ALREADY_OPEN         = 0x17,
    SHELTER_B1_ARMORY_CAP_UNLOCK               = 0x18,
    SHELTER_B1_ARMORY_CAP_REFUSE_CARD          = 0x19,
    SHELTER_B1_ARMORY_EVENT_IDLE               = 0,
    SHELTER_B1_ARMORY_EVENT_ACTIVE             = 1,
};

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_shelter_b1_armory_801824D0 = { { { TASK_BODY_NONE, 192 } }, _shopSessionTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b1_armory_801824E8[2] = {
    { { { TASK_BODY_NONE, 192 } }, _shelterB1ArmoryCardkeyEventTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterB1ArmoryShopSceneTask, { .value = 0 } },
};

TaskMessageEntry D_shelter_b1_armory_80182500[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1ArmoryResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1ArmoryUseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1ArmoryHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1ArmoryHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// State handlers of the room's controller task: installing its message
/// table, an idle tick, and the kill.
static const TaskFuncTable3 D_shelter_b1_armory_8017D714 = {
    {
        _shelterB1ArmoryInitRoomState,
        _shelterB1ArmoryRoomIdleState,
        taskKill,
    },
};

/// Plays the card-reader result after allowing the item-use menu to close.
///
/// `spawnArg1.value` holds the CAP slot in its high halfword and a
/// `SHELTER_B1_ARMORY_CARD_RESULT_*` selector in its low halfword. The caller
/// holds the session event gate before spawning on the default task list.
/// Three task ticks hold menu transitions and the player's normal state tick;
/// playback then holds scripted control until CAP is idle. Unlock completion
/// identifies the Armory Cardkey. The task releases the event gate and kills
/// itself; the room, sound bank and CAP resources must remain loaded until then.
static void _shelterB1ArmoryCardkeyEventTask(Task* task)
{
    enum {
        SHELTER_B1_ARMORY_CARD_STATE_HOLD_MENU = 0,
        SHELTER_B1_ARMORY_CARD_STATE_WAIT_1    = 1,
        SHELTER_B1_ARMORY_CARD_STATE_WAIT_2    = 2,
        SHELTER_B1_ARMORY_CARD_STATE_PLAY      = 3,
        SHELTER_B1_ARMORY_CARD_STATE_WAIT_CAP  = 4,
        SHELTER_B1_ARMORY_PLAYER_TICK_HOLD     = 1,
        SHELTER_B1_ARMORY_PLAYER_TICK_RESUME   = 0,
        SHELTER_B1_ARMORY_ITEM_IDENTIFIED      = 1,
        SHELTER_B1_ARMORY_SOUND_CARD_REFUSED   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 8),
        SHELTER_B1_ARMORY_SOUND_CARD_UNLOCK    = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 9),
    };

    switch (task->state) {
        case SHELTER_B1_ARMORY_CARD_STATE_HOLD_MENU:
            displayAcquireMenuHold();
            D_80115768 = SHELTER_B1_ARMORY_PLAYER_TICK_HOLD;
            task->state++;
            break;
        case SHELTER_B1_ARMORY_CARD_STATE_PLAY:
            // Hand control from the closing menu to the card-reader caption.
            displayReleaseMenuHold();
            D_80115768 = SHELTER_B1_ARMORY_PLAYER_TICK_RESUME;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            if ((u16)task->spawnArg1.value == SHELTER_B1_ARMORY_CARD_RESULT_REFUSED) {
                sndEvtRequestScriptStart(SHELTER_B1_ARMORY_SOUND_CARD_REFUSED, 0, 0);
            }
            if ((u16)task->spawnArg1.value == SHELTER_B1_ARMORY_CARD_RESULT_UNLOCKED) {
                sndEvtRequestScriptStart(SHELTER_B1_ARMORY_SOUND_CARD_UNLOCK, 0, 0);
            }
            capStartSequenceSlot(task->spawnArg1.value >> SHELTER_B1_ARMORY_CARD_CAP_SHIFT, CAP_PLAYBACK_IN_PLACE, 0);
            task->state++;
            break;
        case SHELTER_B1_ARMORY_CARD_STATE_WAIT_1:
        case SHELTER_B1_ARMORY_CARD_STATE_WAIT_2:
            task->state++;
            break;
        case SHELTER_B1_ARMORY_CARD_STATE_WAIT_CAP:
            if (capIsBusy() == 0) {
                if ((u16)task->spawnArg1.value == SHELTER_B1_ARMORY_CARD_RESULT_UNLOCKED) {
                    itemSetIdentified(INVENTORY_COLLECTION_ID_ARMORY_CARDKEY, SHELTER_B1_ARMORY_ITEM_IDENTIFIED);
                }
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gGameSession->eventState = SHELTER_B1_ARMORY_EVENT_IDLE;
                taskKill(task);
            }
            break;
    }
}

/// Saves the live view and selects the armory shop's presentation view.
static inline void _shelterB1ArmorySelectShopView(void)
{
    enum { SHELTER_B1_ARMORY_SHOP_VIEW = 13 };
    McSaveData* liveSave;
    u8          savedView;

    liveSave                          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    savedView                         = liveSave->state.location.loc.view;
    liveSave->state.location.loc.view = SHELTER_B1_ARMORY_SHOP_VIEW;
    D_shelter_b1_armory_8018557C.view = savedView;
}

/// Presents the armory shop's caption and view, then restores the saved view.
///
/// Starts with scripted player control already held. Saves the live-save view
/// in the room's singleton `RoomSavedViewStorage`, selects view 13, hides the
/// player and HUD, and waits for CAP command 22 before queuing armory stock.
/// After the shop transition resumes this task, restores visibility, control
/// and the original view. State 4 is inert; this task does not kill itself.
/// The saved-view slot and room/CAP resources must outlive this task; concurrent
/// shop scenes would overwrite the singleton view.
static void _shelterB1ArmoryShopSceneTask(Task* task)
{
    enum {
        SHELTER_B1_ARMORY_SHOP_STATE_PREPARE      = 0,
        SHELTER_B1_ARMORY_SHOP_STATE_WAIT_CAP     = 1,
        SHELTER_B1_ARMORY_SHOP_STATE_RESUME       = 2,
        SHELTER_B1_ARMORY_SHOP_STATE_RESTORE_VIEW = 3,
        SHELTER_B1_ARMORY_CAP_OPEN_SHOP           = 0x16,
        SHELTER_B1_ARMORY_HUD_HIDDEN              = 1,
        SHELTER_B1_ARMORY_HUD_VISIBLE             = 0,
    };
    switch (task->state) {
        case SHELTER_B1_ARMORY_SHOP_STATE_PREPARE:
            // Keep the shop's presentation view out of the resumed game state.
            gGameSession->eventState = SHELTER_B1_ARMORY_EVENT_ACTIVE;
            gGameSession->hideHud    = SHELTER_B1_ARMORY_HUD_HIDDEN;
            _shelterB1ArmorySelectShopView();
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            capRunCommand(SHELTER_B1_ARMORY_CAP_OPEN_SHOP, CAP_PLAYBACK_IN_PLACE);
            task->state = task->state + 1;
            break;
        case SHELTER_B1_ARMORY_SHOP_STATE_WAIT_CAP:
            if (capIsBusy() != 0) {
                break;
            }
            shopOpenSession(SHOP_STOCK_ARMORY);
            task->state = task->state + 1;
            break;
        case SHELTER_B1_ARMORY_SHOP_STATE_RESUME:
            task->state = SHELTER_B1_ARMORY_SHOP_STATE_RESTORE_VIEW;
            // Fall through to restore the presentation after the shop returns.
        case SHELTER_B1_ARMORY_SHOP_STATE_RESTORE_VIEW:
            gGameSession->eventState = SHELTER_B1_ARMORY_EVENT_IDLE;
            gGameSession->hideHud    = SHELTER_B1_ARMORY_HUD_VISIBLE;
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b1_armory_8018557C.view;
            task->state                                                = task->state + 1;
            break;
    }
}

/// Tests whether a hit pending room-action trigger selects the cardkey event.
///
/// Borrows the live, null-terminated pending-trigger list for this call without
/// consuming its hits or retaining a node. Returns 1 on a hit, otherwise 0.
static inline s32 _shelterB1ArmoryRoomTriggerHit(void)
{
    const WorldCollisionTrigger* trigger;

    for (trigger = Gp_PendingObj4C; trigger != NULL; trigger = trigger->next) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return 1;
        }
    }
    return 0;
}

/// Handles card use at the armory reader, including refusal captions.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM` with an integer catalogue `itemId`.
/// A hit cardkey trigger accepts Armory Cardkey, Bowman's Card or Yoshida's
/// Card and requests a result task on the default list. Only Armory Cardkey
/// unlocks the reader; any of the three selects the already-open caption after
/// unlocking. Sets the event gate before the unchecked spawn and commits the
/// unlock flag afterward, even if allocation failed. Returns
/// `ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE` on a requested event,
/// even a refusal or failed allocation, otherwise `ROOM_KEY_ITEM_USE_REFUSED`.
/// Other arguments are unused. Trigger and room resources must remain live
/// through the resulting event; no payload pointer is retained.
static s32 _shelterB1ArmoryUseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    enum {
        SHELTER_B1_ARMORY_ITEM_YOSHIDAS_CARD = 0x122,
        SHELTER_B1_ARMORY_UNLOCK_FLAG_SET    = 1,
    };

    if (_shelterB1ArmoryRoomTriggerHit() != 0) {
        if (itemId == INVENTORY_COLLECTION_ID_ARMORY_CARDKEY) {
            gGameSession->eventState = SHELTER_B1_ARMORY_EVENT_ACTIVE;
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) != 0) {
                taskSpawnFromTableOnDefaultList(D_shelter_b1_armory_801824E8, SHELTER_B1_ARMORY_TASK_CARDKEY_EVENT,
                                                (SHELTER_B1_ARMORY_CAP_ALREADY_OPEN << SHELTER_B1_ARMORY_CARD_CAP_SHIFT) | SHELTER_B1_ARMORY_CARD_RESULT_ALREADY_OPEN, 0);
            } else {
                taskSpawnFromTableOnDefaultList(D_shelter_b1_armory_801824E8, SHELTER_B1_ARMORY_TASK_CARDKEY_EVENT,
                                                (SHELTER_B1_ARMORY_CAP_UNLOCK << SHELTER_B1_ARMORY_CARD_CAP_SHIFT) | SHELTER_B1_ARMORY_CARD_RESULT_UNLOCKED, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED, SHELTER_B1_ARMORY_UNLOCK_FLAG_SET);
            }
            return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
        }
        if (itemId == INVENTORY_COLLECTION_ID_BOWMANS_CARD || itemId == SHELTER_B1_ARMORY_ITEM_YOSHIDAS_CARD) {
            gGameSession->eventState = SHELTER_B1_ARMORY_EVENT_ACTIVE;
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) != 0) {
                taskSpawnFromTableOnDefaultList(D_shelter_b1_armory_801824E8, SHELTER_B1_ARMORY_TASK_CARDKEY_EVENT,
                                                (SHELTER_B1_ARMORY_CAP_ALREADY_OPEN << SHELTER_B1_ARMORY_CARD_CAP_SHIFT) | SHELTER_B1_ARMORY_CARD_RESULT_ALREADY_OPEN, 0);
            } else {
                taskSpawnFromTableOnDefaultList(D_shelter_b1_armory_801824E8, SHELTER_B1_ARMORY_TASK_CARDKEY_EVENT,
                                                (SHELTER_B1_ARMORY_CAP_REFUSE_CARD << SHELTER_B1_ARMORY_CARD_CAP_SHIFT) | SHELTER_B1_ARMORY_CARD_RESULT_REFUSED, 0);
            }
            return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves destinations and gates the storeroom door and locked armory entry.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; request and reply borrow complete
/// eight-byte records and may alias. Copies the request before resolving its
/// room variant. Storeroom entry returns 1 when already unlocked or 2 when
/// eligible for its unlock event; it requires no collection bit. Other
/// destinations return 1 except locked armory entry, which returns 0.
/// Queries suppress CAP and flag effects; executing a
/// refused armory entry writes 2 to the optional map flag and runs its caption.
/// Receiver and message ID are unused. Keep room and map_shelter resources
/// loaded through any deferred door event; the gate copies its inputs.
static s32 _shelterB1ArmoryResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B1_ARMORY_ENTRY_BLOCKED         = 0,
        SHELTER_B1_ARMORY_ENTRY_ALLOWED         = 1,
        SHELTER_B1_ARMORY_CAP_UNLOCK_STOREROOM  = 4,
        SHELTER_B1_ARMORY_CAP_STOREROOM_REFUSAL = 1,
        SHELTER_B1_ARMORY_CAP_ENTRY_LOCKED      = 0xD,
    };
    RoomEventReq doorEvent;

    // Resolve a complete reply before a gate can retain it for a later task.
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_STOREROOM) {
        doorEvent.capCmd        = SHELTER_B1_ARMORY_CAP_UNLOCK_STOREROOM;
        doorEvent.missingCapCmd = SHELTER_B1_ARMORY_CAP_STOREROOM_REFUSAL;
        doorEvent.firstSnd      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 5);
        doorEvent.secondSnd     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 1);
        doorEvent.flagId        = GAME_FLAG_B1_ARMORY_STOREROOM_DOOR_UNLOCKED;
        doorEvent.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&doorEvent, reply);
    }
    if (request->areaId != GAME_AREA_SHELTER_B1_ARMORY) {
        return SHELTER_B1_ARMORY_ENTRY_ALLOWED;
    }
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) != 0) {
        return SHELTER_B1_ARMORY_ENTRY_ALLOWED;
    }
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        gameFlagSetNibbleIfPresent(request->flagId, ROOM_EVENT_GATE_REFUSAL_FLAG_VALUE);
        capRunCommandWithTransition(SHELTER_B1_ARMORY_CAP_ENTRY_LOCKED);
    }
    return SHELTER_B1_ARMORY_ENTRY_BLOCKED;
}

/// Selects alternate room captions from unlock and control-room-return progress.
///
/// Handles `ROOM_MESSAGE_COMMAND` with an integer CAP command index. Commands
/// 12 and 10 select alternate captions from the armory-unlocked and control-room
/// return flags. Requests an actor-pausing event only while CAP is idle; busy
/// playback, allocation failure and other commands do nothing. Always returns
/// 0; the receiver, message ID and second argument are unused. The room's CAP
/// resources must remain loaded through any requested playback.
static s32 _shelterB1ArmoryHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum {
        SHELTER_B1_ARMORY_CAP_READER_LOCKED              = 0xC,
        SHELTER_B1_ARMORY_CAP_BEFORE_CONTROL_ROOM_RETURN = 0xA,
        SHELTER_B1_ARMORY_CAP_AFTER_CONTROL_ROOM_RETURN  = 0x10,
    };

    switch (commandIndex) {
        case SHELTER_B1_ARMORY_CAP_READER_LOCKED:
            capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) == 0 ? SHELTER_B1_ARMORY_CAP_READER_LOCKED : SHELTER_B1_ARMORY_CAP_ALREADY_OPEN, CAP_EVENT_PAUSE_ACTORS);
            break;
        case SHELTER_B1_ARMORY_CAP_BEFORE_CONTROL_ROOM_RETURN:
            capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0 ? SHELTER_B1_ARMORY_CAP_AFTER_CONTROL_ROOM_RETURN : SHELTER_B1_ARMORY_CAP_BEFORE_CONTROL_ROOM_RETURN, CAP_EVENT_PAUSE_ACTORS);
            break;
    }
    return 0;
}

/// Starts the armory shop scene for directed room action 1.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request,
/// read only during dispatch. Action 1 holds scripted player control and spawns
/// the shop scene on the selected task list; allocation failure still leaves
/// control held. Other actions do nothing. Always returns 0; all other
/// arguments and request fields are unused. Keep room resources loaded while
/// the spawned scene uses them; no request pointer is retained.
static s32 _shelterB1ArmoryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg)
{
    enum { SHELTER_B1_ARMORY_ACTION_OPEN_SHOP = 1 };

    if (actionRequest->actionId == SHELTER_B1_ARMORY_ACTION_OPEN_SHOP) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(D_shelter_b1_armory_801824E8, SHELTER_B1_ARMORY_TASK_SHOP_SCENE, 0, 0);
    }
    return 0;
}

/// Registers the armory controller and its message table, then enters idle.
///
/// Called in initial state 0 of `shelterB1ArmoryRoomTask`; the singleton room
/// slot borrows this live task while the armory overlay remains loaded.
static void _shelterB1ArmoryInitRoomState(Task* task)
{
    task->msgTable = D_shelter_b1_armory_80182500;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the armory room task available for messages in state 1.
///
/// Leaves its state and message table intact; room teardown is owned by the caller.
static void _shelterB1ArmoryRoomIdleState(Task* task)
{
}

void shelterB1ArmoryRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b1_armory_8017D714;
    states.funcs[task->state](task);
}
