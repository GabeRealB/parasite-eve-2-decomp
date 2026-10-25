#include "rooms/dryfield_night_general_store.h"

#include "common.h"

#include "dryfield_night_general_store_private.h"

#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"

static s32  _roomVariantGeneralStoreMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _generalStoreUnderpassTransitionTask(Task* task);
static void _generalStoreToggleFlagTask(Task* task);
static s32  _generalStoreCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

static s32 _generalStoreSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

static void _dryfieldNightGeneralStoreInitializeRoomTask(Task* task);
static void _dryfieldNightGeneralStoreRoomIdle(Task* unusedTask);

static s32 _dryfieldNightGeneralStoreRejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightGeneralStoreIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// General Store prompt recipes: flag toggle at slot 0, underpass departure at slot 1.
///
/// Both are bodyless at priority `TASK_PRIORITY_SCENE`; slot 2 is a task-bank end marker. Toggle
/// payloads are a flag index and CAP command, while departure payloads are unused.
/// Spawn reads these recipes synchronously; callbacks and latched state stay loaded.
static TaskDesc _gGeneralStoreTaskDescriptors[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _generalStoreToggleFlagTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _generalStoreUnderpassTransitionTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Room-task message interface, borrowed from this overlay until receiver teardown.
///
/// Transition resolution, refused key-item use, direction actions, CAP commands and sound cues.
/// The final `TASK_MESSAGE_TABLE_END` record terminates dispatch; it is not a handler.
static const TaskMessageEntry _gDryfieldNightGeneralStoreMessageTable[6] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGeneralStoreMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGeneralStoreRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightGeneralStoreIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _generalStoreCommandMsg },
    { ROOM_MESSAGE_SOUND, _generalStoreSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room/room_event.inc.c"

/// Selects the night store's writable underpass arrival ID latch.
///
/// Bind the same `u8` lvalue for both departure and prompt fragments. Executing
/// an unblocked departure latches the request's 1-based arrival ID without
/// validation; prompt acceptance commits it to the live save. Keep the slot
/// unchanged and loaded until the prompt ends; cancellation leaves it intact.
#define GENERAL_STORE_UNDERPASS_ARRIVAL_WARP gGeneralStoreUnderpassArrivalWarp

/// Selects the night store's writable underpass room-selector latch.
///
/// Bind the same stable `u8` lvalue without side effects through both departure
/// and prompt fragments, then undefine the binding. Unblocked execution stores
/// `RoomEventMsg::roomState` after resolving the reply: aliased records supply room
/// 1..6, while distinct records retain the input room byte. Acceptance copies
/// that byte to the live save without range validation. Keep the overlay loaded
/// and the singleton unchanged until the prompt ends; cancellation leaves its
/// value intact. Each access evaluates the lvalue once; the binding allocates
/// no storage.
#define GENERAL_STORE_UNDERPASS_ROOM gGeneralStoreUnderpassRoom
#include "../../shared/general_store_door_msg.inc.c"

/// Binds the night store's fade control for an accepted underpass departure.
///
/// Supply a stable, addressable, writable `ScreenFade` lvalue without side
/// effects around `general_store_cutscene_task.inc.c`, then undefine it.
/// Acceptance sets subtractive blend, running phase and an eight-frame ramp,
/// passing the record's address as the bank-1 fade task's second spawn payload.
/// Cancellation leaves it untouched. The prompt requests reload on its next
/// tick without waiting for the fade or requesting a return ramp.
/// Keep the singleton writable, loaded and reserved for that departure until
/// the fade task is retired by reload. Each occurrence evaluates the binding
/// once; the binding allocates no storage and copies no record.
#define GENERAL_STORE_UNDERPASS_FADE (gGeneralStoreUnderpassFade)

/// Selects the night store's writable capture of the pre-prompt logical view.
///
/// Bind a stable `u8` lvalue without side effects around the shared underpass
/// prompt fragment, then undefine it. State 0 saves the live save's room-local
/// view before switching to view 16; cancellation restores the byte verbatim.
/// Acceptance neither restores the view nor clears the capture before reload.
/// One pending prompt owns the slot: keep it loaded and unchanged after capture
/// until the prompt ends. Each read or write evaluates the binding once; it
/// allocates no storage and does not validate the view ID.
#define GENERAL_STORE_UNDERPASS_SAVED_VIEW (gGeneralStoreUnderpassSavedView)
#include "../../shared/general_store_cutscene_task.inc.c"
#undef GENERAL_STORE_UNDERPASS_SAVED_VIEW
#undef GENERAL_STORE_UNDERPASS_ARRIVAL_WARP
#undef GENERAL_STORE_UNDERPASS_ROOM
#undef GENERAL_STORE_UNDERPASS_FADE

/// Room-task lifecycle: state 0 initializes, state 1 waits or updates, state 2 releases.
///
/// The dispatcher copies all three callback pointers before indexing by `Task::state`.
/// The selector must be in 0..2; there is no terminator or bounds check.
/// Keep this overlay loaded until the room task is released.
static const TaskFuncTable3 _gDryfieldNightGeneralStoreRoomTaskStates = {
    { _dryfieldNightGeneralStoreInitializeRoomTask, _dryfieldNightGeneralStoreRoomIdle, taskKill },
};

#include "../../shared/general_store_toggle_task.inc.c"

#include "../../shared/general_store_action_msg.inc.c"

#include "../../shared/general_store_sound_msg.inc.c"

/// Refuses key-item use in the night General Store without changing inventory.
///
/// All arguments are ignored. The item menu supplies the selected collected-item
/// ID as `itemId` and zero as `unusedSecondArg`; the refused reply shows its
/// cannot-use notice.
static s32 _dryfieldNightGeneralStoreRejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Ignores the General Store's direction-action requests and returns zero.
///
/// The first payload is a borrowed `DirectionActionRequest`, left unread;
/// all four arguments are ignored and no room event is started.
static s32 _dryfieldNightGeneralStoreIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the night General Store's room receiver and enables CAP completion cues.
///
/// Called in state 0, with this overlay's message table loaded. Publishes a
/// borrowed task in `GAME_TASK_SLOT_ROOM` and advances to idle state 1.
/// Stop sending room messages before releasing the task; teardown leaves the slot.
static void _dryfieldNightGeneralStoreInitializeRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightGeneralStoreMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state                     = task->state + 1;
    gCapRoomCompletionSoundsEnabled = CAP_ROOM_COMPLETION_SOUNDS_ENABLED;
}

/// Keeps the General Store room task alive to receive messages.
static void _dryfieldNightGeneralStoreRoomIdle(Task* unusedTask)
{
}

void dryfieldNightGeneralStoreRoomTask(Task* task)
{
    TaskFuncTable3 roomStates;

    roomStates = _gDryfieldNightGeneralStoreRoomTaskStates;
    roomStates.funcs[task->state](task);
}
