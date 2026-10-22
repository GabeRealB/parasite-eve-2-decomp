#include "rooms/acropolis_hallway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
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

#include "mapui/map_akropolis.h"

#include "overlay.h"
#include "../../shared/actor/actor_contacts.h"

#include "assets/rooms/akropolis/hallway/declarations.inc"

extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void _acropolisHallwayInitializeRoomTask(Task* task);
static void _acropolisHallwayIdleRoomTask(Task* unusedTask);

/// Room message carrying the selected inventory item ID in its first argument.
enum { ACROPOLIS_HALLWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Room-task states 0..2: register the message receiver, idle, release the task.
///
/// Copied by value before unchecked dispatch. The task state must stay in 0..2;
/// the callback code must remain loaded through dispatch.
static const TaskFuncTable3 _gAcropolisHallwayRoomTaskStates = {
    { _acropolisHallwayInitializeRoomTask, _acropolisHallwayIdleRoomTask, taskKill },
};

static u32     _gAcropolisHallwayModel023C0PartVerts[1];
static SVECTOR _gAcropolisHallwayModel023C0Verts[21];
static SVECTOR _gAcropolisHallwayModel023C0Normals[6];
static TmdBone _gAcropolisHallwayModel023C0Skeleton[1];
static u32     _gAcropolisHallwayModel023C0Stream[42];

static s32 _acropolisHallwayResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisHallwayRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _acropolisHallwayHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg);

/// Synchronous room messages for transition resolution, key-item refusal and sound cues.
///
/// Transition resolution borrows a RoomEventMsg request and writable reply;
/// key-item use takes an integer item ID, and sound takes an integer cue.
/// The fourth record ends lookup and makes unsupported IDs return zero. The
/// room task borrows this table and its callbacks while the overlay is loaded.
static TaskMessageEntry _gAcropolisHallwayRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisHallwayResolveRoomTransition },
    { ACROPOLIS_HALLWAY_MESSAGE_USE_KEY_ITEM, _acropolisHallwayRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisHallwayHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// One-based hallway endpoints selected by departure triggers and arrival warps.
enum {
    ACROPOLIS_HALLWAY_WARP_FOUNTAIN        = 1,
    ACROPOLIS_HALLWAY_WARP_SECURITY_ROOM   = 2,
    ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW3 = 3,
    ACROPOLIS_HALLWAY_WARP_CAFETERIA_VIEW2 = 4,
    ACROPOLIS_HALLWAY_WARP_ENDPOINT_SHIFT  = 4,
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

#include "assets/rooms/akropolis/hallway/acropolis_hallway_model_01AE0.inc"

static TmdBone _gAcropolisHallwayModel023C0Skeleton[1] = {
#include "assets/acropolis_hallway_model_023C0_skeleton.inc"
};

static u32 _gAcropolisHallwayModel023C0PartVerts[1] = {
#include "assets/acropolis_hallway_model_023C0_partVerts.inc"
};

static SVECTOR _gAcropolisHallwayModel023C0Verts[21] = {
#include "assets/acropolis_hallway_model_023C0_verts.inc"
};

static SVECTOR _gAcropolisHallwayModel023C0Normals[6] = {
#include "assets/acropolis_hallway_model_023C0_normals.inc"
};

static u32 _gAcropolisHallwayModel023C0Stream[42] = {
#include "assets/acropolis_hallway_model_023C0_stream.inc"
};

static TmdSource _gAcropolisHallwayModel023C0 = {
    0,
    312,
    0,
    1,
    _gAcropolisHallwayModel023C0PartVerts,
    _gAcropolisHallwayModel023C0Verts,
    _gAcropolisHallwayModel023C0Normals,
    _gAcropolisHallwayModel023C0Skeleton,
    _gAcropolisHallwayModel023C0Stream,
};

SVECTOR ActorContact_ScratchPosition = { 0 };

static void _acropolisHallwayUnusedObjectVisibilityTask(Task* task);

/// Resolves hallway exits and blocks the early cafeteria warp-3 departure.
///
/// Copies complete borrowed request/reply records, which may alias. Returns
/// 0 for the blocked exit, starting CAP command 1 only on execution; other
/// exits return 1. Executed fountain exits select room 2 after the second
/// security lock, and cafeteria exits select rooms 3/4 from story progress.
/// Queries only copy the request. The receiver and message ID are unused.
static s32 _acropolisHallwayResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_HALLWAY_SECOND_LOCK_BIT          = 2,
        ACROPOLIS_HALLWAY_CAFETERIA_GATED_WARP     = 3,
        ACROPOLIS_HALLWAY_CAFETERIA_EXIT_COMMAND   = 1,
        ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS  = 3,
        ACROPOLIS_HALLWAY_CAFETERIA_SCENE_PROGRESS = 2,
    };

    u16 destinationArea;

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_FOUNTAIN) {
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & ACROPOLIS_HALLWAY_SECOND_LOCK_BIT) && request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->roomState = 2;
        }
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_CAFETERIA && request->warp == ACROPOLIS_HALLWAY_CAFETERIA_GATED_WARP && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(ACROPOLIS_HALLWAY_CAFETERIA_EXIT_COMMAND);
        }
        return 0;
    }
    if (request->areaId == GAME_AREA_AKROPOLIS_FOUNTAIN && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS) {
        return 1;
    }
    destinationArea = request->areaId;
    if (destinationArea == GAME_AREA_AKROPOLIS_CAFETERIA && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= ACROPOLIS_HALLWAY_CAFETERIA_EXIT_PROGRESS) {
            reply->roomState = destinationArea;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == ACROPOLIS_HALLWAY_CAFETERIA_SCENE_PROGRESS) {
            reply->roomState = 3;
        }
    }
    return 1;
}

/// Refuses every key-item use request in the hallway.
///
/// All arguments are ignored. Returning 0 selects the inventory's "No use now"
/// notice without consuming the selected item or starting a room event.
static s32 _acropolisHallwayRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { ACROPOLIS_HALLWAY_KEY_ITEM_USE_REFUSED = 0 };

    return ACROPOLIS_HALLWAY_KEY_ITEM_USE_REFUSED;
}

/// Queues hallway sound-bank entry 6 or 7 for the corresponding room sound cue.
///
/// Other cues do nothing. Uses each script's base mix; queue failures are
/// discarded. Receiver, ID and second payload are unused; always returns zero.
static s32 _acropolisHallwayHandleSoundCue(Task* unusedTask, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_6 = 6,
        ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_7 = 7
    };

    switch (soundCue) {
        case ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_6:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 6), 0, 0);
            break;
        case ACROPOLIS_HALLWAY_SOUND_CUE_ENTRY_7:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HALLWAY, 7), 0, 0);
            break;
    }
    return 0;
}

/// Registers the hallway's room-message receiver and enters its idle state.
///
/// Requires a live task in state 0 and loaded room resources. Borrows the message
/// table, publishes the task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _acropolisHallwayInitializeRoomTask(Task* task)
{
    task->msgTable = _gAcropolisHallwayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Leaves the hallway room task untouched in its idle state (state 1).
static void _acropolisHallwayIdleRoomTask(Task* unusedTask)
{
}

void acropolisHallwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisHallwayRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void acropolisHallwayEffectControlTask84(Task* unusedTask)
{
}

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/actor/actor_contacts_push.inc.c"

void acropolisHallwayBlueKeyPickupTask(Task* task)
{
    enum {
        ACROPOLIS_HALLWAY_BLUE_KEY_VIEW      = 5,
        ACROPOLIS_HALLWAY_BLUE_KEY_COLLECTED = 2,
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
    if (viewGetMappedIndex() == ACROPOLIS_HALLWAY_BLUE_KEY_VIEW) {
        model->flags = TMD_OBJECT_FLAGGED_PASS;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (placementState == ACROPOLIS_HALLWAY_BLUE_KEY_COLLECTED) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Retains an unreferenced placed-model visibility callback in the room image.
///
/// No placement, call or relocation selects it. With an Enemy-compatible
/// placement argument it would disable automatic buffers for object state 2,
/// otherwise select the flagged draw pass and allocate a primitive buffer.
/// The discarded view query is retained. Its intended object is unproven.
static void _acropolisHallwayUnusedObjectVisibilityTask(Task* task)
{
    enum {
        ACROPOLIS_HALLWAY_OBJECT_COLLECTED = 2,
    };

    Enemy*     enemy;
    TmdObject* model;
    s32        placementState;

    enemy          = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)enemy->placeKey);
    viewGetMappedIndex();
    if (placementState == ACROPOLIS_HALLWAY_OBJECT_COLLECTED) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}
