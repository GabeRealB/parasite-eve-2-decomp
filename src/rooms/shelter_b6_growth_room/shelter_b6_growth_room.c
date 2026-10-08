#include "rooms/shelter_b6_growth_room.h"

#include "types.h"

#include "shelter_b6_growth_room_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "actors/actor_450900.h"

#include "mapui/map_neo_ark.h"

extern TaskDesc         D_actor_450900_80135E78[];
extern TaskMessageEntry D_shelter_b6_growth_room_8017F16C[];

extern EvsCommand D_actor_450900_80136110[];
extern EvsCommand D_actor_450900_80136308[];

static s32 _shelterB6GrowthRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterB6GrowthRoomResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination);
s32        func_shelter_b6_growth_room_8017D634(Task*, s32, s32, s32);
s32        func_shelter_b6_growth_room_8017D6C8(Task*, s32, RoomEventMsg*, s32);

TaskMessageEntry D_shelter_b6_growth_room_8017F16C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB6GrowthRoomResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB6GrowthRoomRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b6_growth_room_8017D6C8 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b6_growth_room_8017D634 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_shelter_b6_growth_room_8017D71C(Task* arg0);
static void _shelterB6GrowthRoomMessageIdle(Task* task);

/// Refuses key-item use in the growth room without consuming the item.
static s32 _shelterB6GrowthRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a room-transition destination from Neo Ark's saved progress.
///
/// Borrows an eight-byte request and writable destination for this dispatch;
/// they may alias. Copies the complete request, resolves its room variant and
/// returns 1. The records are not retained.
static s32 _shelterB6GrowthRoomResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination)
{
    *destination = *request;
    mapNeoArkResolveRoomVariant(request, destination);
    return 1;
}

s32 func_shelter_b6_growth_room_8017D634(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_0D8) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_actor_450900_80135E78, 3, 0, 0);
        } else {
            capRunCommandWithTransition(1);
        }
    }
    if (arg2 == 0x10) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_0D8) == 0 ? 0x10 : 0x11, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

s32 func_shelter_b6_growth_room_8017D6C8(Task* arg0, s32 arg1, RoomEventMsg* arg2, s32 arg3)
{
    if (arg2->warp == 1) {
        actor450900StartCompanionConversation();
    }
    if (arg2->warp == 2) {
        actor450900HandleDepartureTrigger();
    }
    return 0;
}

static void func_shelter_b6_growth_room_8017D71C(Task* arg0)
{
    arg0->msgTable = D_shelter_b6_growth_room_8017F16C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    companionRestoreFullHp();
    areaApplySavedUpdates(D_shelter_b6_growth_room_801807C8);
    evsStartScriptWithSkip(D_actor_450900_80136110, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_450900_80136308);
    taskSpawnFromTable(D_actor_450900_80135E78, 1, 0, 0);
    taskSpawnFromTable(D_actor_450900_80135E78, 2, 0, 0);
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x33);
    arg0->state = (s32)(arg0->state + 1);
}

/// Keeps the room task available for messages after entry setup.
static void _shelterB6GrowthRoomMessageIdle(Task* task)
{
}

/// State table of the room task: set-up, idle, kill.
static const TaskFuncTable3 D_shelter_b6_growth_room_8017D5C4 = {
    {
        func_shelter_b6_growth_room_8017D71C,
        _shelterB6GrowthRoomMessageIdle,
        taskKill,
    },
};

void shelterB6GrowthRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b6_growth_room_8017D5C4;
    states.funcs[task->state](task);
}
