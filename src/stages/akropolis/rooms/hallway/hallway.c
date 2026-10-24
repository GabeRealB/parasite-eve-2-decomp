#include "rooms/akropolis_hallway.h"

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

static u32     _gAkropolisHallwayModel023C0PartVerts[1];
static SVECTOR _gAkropolisHallwayModel023C0Verts[21];
static SVECTOR _gAkropolisHallwayModel023C0Normals[6];
static TmdBone _gAkropolisHallwayModel023C0Skeleton[1];
static u32     _gAkropolisHallwayModel023C0Stream[42];

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

static TmdBone _gAkropolisHallwayModel023C0Skeleton[1] = {
#include "assets/akropolis_hallway_model_023C0_skeleton.inc"
};

static u32 _gAkropolisHallwayModel023C0PartVerts[1] = {
#include "assets/akropolis_hallway_model_023C0_partVerts.inc"
};

static SVECTOR _gAkropolisHallwayModel023C0Verts[21] = {
#include "assets/akropolis_hallway_model_023C0_verts.inc"
};

static SVECTOR _gAkropolisHallwayModel023C0Normals[6] = {
#include "assets/akropolis_hallway_model_023C0_normals.inc"
};

static u32 _gAkropolisHallwayModel023C0Stream[42] = {
#include "assets/akropolis_hallway_model_023C0_stream.inc"
};

static TmdSource _gAkropolisHallwayModel023C0 = {
    0,
    312,
    0,
    1,
    _gAkropolisHallwayModel023C0PartVerts,
    _gAkropolisHallwayModel023C0Verts,
    _gAkropolisHallwayModel023C0Normals,
    _gAkropolisHallwayModel023C0Skeleton,
    _gAkropolisHallwayModel023C0Stream,
};

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

/// Plays room sound cues 6 and 7, the key box opening and closing.
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

/// Registers the hallway's room-message receiver and enters its idle state.
static void _akropolisHallwayInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisHallwayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Leaves the hallway room task untouched in its idle state (state 1).
static void _akropolisHallwayIdleRoomTask(Task* unusedTask)
{
}

void akropolisHallwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisHallwayRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void akropolisHallwayEffectControlTask84(Task* unusedTask)
{
}

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

void akropolisHallwayBlueKeyPickupTask(Task* task)
{
    enum {
        AKROPOLIS_HALLWAY_BLUE_KEY_VIEW      = 5,
        AKROPOLIS_HALLWAY_BLUE_KEY_COLLECTED = 2,
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
    if (viewGetMappedIndex() == AKROPOLIS_HALLWAY_BLUE_KEY_VIEW) {
        model->flags = TMD_OBJECT_FLAGGED_PASS;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (placementState == AKROPOLIS_HALLWAY_BLUE_KEY_COLLECTED) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Retains an unreferenced placed-model visibility callback in the room image.
static void _akropolisHallwayUnusedObjectVisibilityTask(Task* task)
{
    enum {
        AKROPOLIS_HALLWAY_OBJECT_COLLECTED = 2,
    };

    Enemy*     enemy;
    TmdObject* model;
    s32        placementState;

    enemy          = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)enemy->placeKey);
    viewGetMappedIndex();
    if (placementState == AKROPOLIS_HALLWAY_OBJECT_COLLECTED) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}
