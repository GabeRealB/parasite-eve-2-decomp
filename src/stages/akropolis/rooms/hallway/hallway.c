#include "stages/akropolis/rooms/hallway.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/abs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../../../shared/actor/actor_contacts.h"

#include "assets/rooms/akropolis/hallway/declarations.inc"

extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void _akropolisHallwayInitializeRoomTask(Task* task);
static void _akropolisHallwayIdleRoomTask(Task* unusedTask);

/// Room task states: initialize, idle, release.
static const TaskFuncTable3 _gAkropolisHallwayRoomTaskStates = {
    { _akropolisHallwayInitializeRoomTask, _akropolisHallwayIdleRoomTask, taskKill },
};

static s32 _akropolisHallwayResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisHallwayRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _akropolisHallwayHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);

/// Messages of the room task.
static TaskMessageEntry _gAkropolisHallwayRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisHallwayResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisHallwayRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisHallwayHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "assets/rooms/akropolis/hallway/header.inc"
#include "assets/rooms/akropolis/hallway/geometry.inc"
#include "assets/rooms/akropolis/hallway/view_boundaries.inc"
#include "assets/rooms/akropolis/hallway/action_triggers.inc"
#include "assets/rooms/akropolis/hallway/placements.inc"
#include "assets/rooms/akropolis/hallway/lights.inc"
#include "assets/rooms/akropolis/hallway/sprites.inc"
#include "assets/rooms/akropolis/hallway/cameras.inc"
#include "assets/rooms/akropolis/hallway/surfaces.inc"

#include "assets/rooms/akropolis/hallway/blue_key_pickup.inc"

#include "assets/rooms/akropolis/hallway/pain_reliever_pickup.inc"

SVECTOR ActorContact_ScratchPosition = { 0 };

static void _akropolisHallwayUnusedObjectVisibilityTask(Task* task);

/// The doors: one to the cafeteria is held until MIST is contacted; the fountain's leads to its state after the red
/// key, and the cafeteria's to its state after the boss.
static s32 _akropolisHallwayResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    u16 destinationArea;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FOUNTAIN) {
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED) && request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->roomState = AKROPOLIS_ROOM_STATE_FOUNTAIN_RED_KEY_INSERTED;
        }
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_CAFETERIA && request->warp == AKROPOLIS_HALLWAY_CAFETERIA_HELD_WARP && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(AKROPOLIS_HALLWAY_CAFETERIA_HELD_COMMAND);
        }
        return ROOM_EVENT_STAY;
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_FOUNTAIN && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
        return ROOM_EVENT_DEPART;
    }
    destinationArea = request->areaId;
    if (destinationArea == AKROPOLIS_ROOM_ID_CAFETERIA && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
            // The cafeteria's room id, 4, is also its state after MIST is contacted.
            reply->roomState = destinationArea;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
            reply->roomState = AKROPOLIS_ROOM_STATE_CAFETERIA_POST_BOSS;
        }
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisHallwayRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Plays room sound cues 6 and 7, the key box opening ("A key is hanging here.", CAP 2) and closing.
static s32 _akropolisHallwayHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    switch (cueIndex) {
        case AKROPOLIS_HALLWAY_SOUND_KEY_BOX_OPEN:
            sndEvtRequestScriptStart(AKROPOLIS_HALLWAY_SOUND(AKROPOLIS_HALLWAY_SOUND_KEY_BOX_OPEN), 0, 0);
            break;
        case AKROPOLIS_HALLWAY_SOUND_KEY_BOX_CLOSE:
            sndEvtRequestScriptStart(AKROPOLIS_HALLWAY_SOUND(AKROPOLIS_HALLWAY_SOUND_KEY_BOX_CLOSE), 0, 0);
            break;
    }
    return 0;
}

/// Installs the room's messages.
static void _akropolisHallwayInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisHallwayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// The room task's idle state.
static void _akropolisHallwayIdleRoomTask(Task* unusedTask)
{
}

void akropolisHallwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisHallwayRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void akropolisHallwayRoomEffectTask(Task* unusedTask)
{
}

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

void akropolisHallwayBlueKeyPickupTask(Task* task)
{
    enum {
        VIEW      = 5,
        COLLECTED = 2,
    };

    Enemy*     enemy;
    TmdObject* model;
    s32        placementState;

    enemy          = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)enemy->placeKey);
    if (task->state == 0) {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        task->state++;
    }
    if (viewGetMappedIndex() == VIEW) {
        model->flags = TMD_OBJECT_FLAGGED_PASS;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (placementState == COLLECTED) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// A placed model's draw task, like the blue key's but without the view test. Nothing installs it.
static void _akropolisHallwayUnusedObjectVisibilityTask(Task* task)
{
    enum {
        OBJECT_COLLECTED = 2,
    };

    Enemy*     enemy;
    TmdObject* model;
    s32        placementState;

    enemy          = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)enemy->placeKey);
    viewGetMappedIndex();
    if (placementState == OBJECT_COLLECTED) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}
