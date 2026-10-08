#include "rooms/dryfield_night_general_store.h"

#include "types.h"

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
#include "gameplay/object_task.h"
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
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

static s32  _roomVariantGeneralStoreMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static void _generalStoreUnderpassTransitionTask(Task* task);
static void _generalStoreToggleFlagTask(Task* task);
static s32  _generalStoreCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);

static s32 _generalStoreSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's two spawnable tasks: entry 0 the CAP-command task
/// `_generalStoreToggleFlagTask`, entry 1 the cutscene task
/// `_generalStoreUnderpassTransitionTask`.
extern TaskDesc gStoreTaskDescs[];

/// The room's message table, installed by the room task's entry state.
extern TaskMessageEntry D_dryfield_night_general_store_8017E7BC[];

static void _dryfieldNightGeneralStoreInitializeRoomTask(Task* task);
static void _dryfieldNightGeneralStoreRoomIdle(Task* unusedTask);

static s32 _dryfieldNightGeneralStoreRejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightGeneralStoreIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gStoreTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, _generalStoreToggleFlagTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _generalStoreUnderpassTransitionTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_night_general_store_8017E7BC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGeneralStoreMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGeneralStoreRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightGeneralStoreIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _generalStoreCommandMsg },
    { ROOM_MESSAGE_SOUND, _generalStoreSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/general_store_door_msg.inc.c"

#include "../../shared/general_store_cutscene_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `dryfieldNightGeneralStoreRoomTask`: the entry state
/// `_dryfieldNightGeneralStoreInitializeRoomTask`, the idle state
/// `_dryfieldNightGeneralStoreRoomIdle`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_general_store_8017D5F4 = {
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
    task->msgTable = D_dryfield_night_general_store_8017E7BC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
    D_80115598  = true;
}

/// Keeps the General Store room task alive to receive messages.
static void _dryfieldNightGeneralStoreRoomIdle(Task* unusedTask)
{
}

void dryfieldNightGeneralStoreRoomTask(Task* task)
{
    TaskFuncTable3 roomStates;

    roomStates = D_dryfield_night_general_store_8017D5F4;
    roomStates.funcs[task->state](task);
}
