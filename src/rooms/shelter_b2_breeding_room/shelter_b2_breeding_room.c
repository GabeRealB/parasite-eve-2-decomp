#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

/// Completion task requested by `_shelterB2BreedingRoomCommandMessage`
/// immediately after it starts the first-entry scene.
extern TaskDesc D_shelter_b2_breeding_room_80180444[];

/// Message table `_shelterB2BreedingRoomInitializeRoomTask` installs on its task.
extern TaskMessageEntry D_shelter_b2_breeding_room_80180414[];

static s32  _shelterB2BreedingRoomRefuseKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32  _shelterB2BreedingRoomResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB2BreedingRoomCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _shelterB2BreedingRoomIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB2BreedingRoomSoundMsg(Task* task, s32 messageId, s32 cueId, s32 unusedArg);
static void _shelterB2BreedingRoomFinishFirstSceneTask(Task* task);

static SVECTOR _gShelterB2BreedingRoomModel02E04Verts[4];
static TmdBone _gShelterB2BreedingRoomModel02E04Skeleton[1];
static u32     _gShelterB2BreedingRoomModel02E04PartVerts[1];
static u32     _gShelterB2BreedingRoomModel02E04Stream[11];

static TmdBone _gShelterB2BreedingRoomModel02E04Skeleton[1] = {
#include "assets/shelter_b2_breeding_room_model_02E04_skeleton.inc"
};

static u32 _gShelterB2BreedingRoomModel02E04PartVerts[1] = {
#include "assets/shelter_b2_breeding_room_model_02E04_partVerts.inc"
};

static SVECTOR _gShelterB2BreedingRoomModel02E04Verts[4] = {
#include "assets/shelter_b2_breeding_room_model_02E04_verts.inc"
};

static u32 _gShelterB2BreedingRoomModel02E04Stream[11] = {
#include "assets/shelter_b2_breeding_room_model_02E04_stream.inc"
};

TmdSource gShelterB2BreedingRoomModel02E04 = { 0, 40, 0, 1, _gShelterB2BreedingRoomModel02E04PartVerts, _gShelterB2BreedingRoomModel02E04Verts, &_gShelterB2BreedingRoomModel02E04Verts[4], _gShelterB2BreedingRoomModel02E04Skeleton, _gShelterB2BreedingRoomModel02E04Stream };

TaskMessageEntry D_shelter_b2_breeding_room_80180414[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2BreedingRoomResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2BreedingRoomRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2BreedingRoomIgnoreAction },
    { ROOM_MESSAGE_COMMAND, _shelterB2BreedingRoomCommandMessage },
    { ROOM_MESSAGE_SOUND, _shelterB2BreedingRoomSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b2_breeding_room_80180444[1] = {
    { { { TASK_BODY_NONE, 32 } }, _shelterB2BreedingRoomFinishFirstSceneTask, { .value = 0 } },
};

void shelterB2BreedingRoomAreaObjectTask(Task* task)
{
    enum { SHELTER_B2_BREEDING_ROOM_OBJECT_STATE_HIDDEN = 2 };

    TmdObject*   modelObject = task->extra.tmd;
    const Enemy* enemy       = task->spawnArg2.pointer;

    if (areaGetCurrentObjectState((u8)enemy->placeKey) == SHELTER_B2_BREEDING_ROOM_OBJECT_STATE_HIDDEN) {
        modelObject->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        modelObject->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Refuses every key-item use without consuming the item or changing room state.
///
/// Returns `ROOM_KEY_ITEM_USE_REFUSED`; all message inputs are ignored.
static s32 _shelterB2BreedingRoomRefuseKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition after resolving its Mine/Shelter destination variant.
///
/// Borrows a readable request and writable eight-byte reply for this dispatch;
/// they may alias and neither is retained. Queries echo the complete request.
/// Execution may change only the copied room selector according to game progress.
/// The `map_shelter` overlay must be loaded. Always returns 1 (accepted).
static s32 _shelterB2BreedingRoomResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B2_BREEDING_ROOM_TRANSITION_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B2_BREEDING_ROOM_TRANSITION_ACCEPTED;
}

/// Starts the breeding room's first-entry scene or its return-visit dialogue.
///
/// Handles `ROOM_MESSAGE_COMMAND` with command 0x16 in the first payload;
/// other commands do nothing. The receiver and second payload are unused.
/// Commits the first-visit flag and advances objective 0x1E to 0x1F before
/// selecting loaded CAP file 1 and holding player control. Requests playback
/// before spawning the completion task; spawn failure does not undo the flags
/// or restore control. Keep the room and CAP resources loaded through completion.
/// Always returns zero.
static s32 _shelterB2BreedingRoomCommandMessage(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        SHELTER_B2_BREEDING_ROOM_COMMAND_FIRST_SCENE     = 0x16,
        SHELTER_B2_BREEDING_ROOM_CAP_REVISIT             = 0x16,
        SHELTER_B2_BREEDING_ROOM_CAP_FIRST_SCENE         = 1,
        SHELTER_B2_BREEDING_ROOM_CAP_FILE                = 1,
        SHELTER_B2_BREEDING_ROOM_CAP_TEXTURE_X           = 320,
        SHELTER_B2_BREEDING_ROOM_CAP_TEXTURE_Y           = 256,
        SHELTER_B2_BREEDING_ROOM_OBJECTIVE_BEFORE_SCENE  = 0x1E,
        SHELTER_B2_BREEDING_ROOM_OBJECTIVE_AFTER_SCENE   = 0x1F,
        SHELTER_B2_BREEDING_ROOM_FIRST_SCENE_SEEN        = 1,
        SHELTER_B2_BREEDING_ROOM_TASK_FINISH_FIRST_SCENE = 0,
    };

    if (commandId == SHELTER_B2_BREEDING_ROOM_COMMAND_FIRST_SCENE) {
        if (gameFlagGetNibble(GAME_FLAG_BREEDING_ROOM_FIRST_SCENE_SEEN) != 0) {
            capRunCommandWithTransition(SHELTER_B2_BREEDING_ROOM_CAP_REVISIT);
        } else {
            // Bookkeeping precedes playback and is retained if the completion spawn fails.
            gameFlagSetNibble(GAME_FLAG_BREEDING_ROOM_FIRST_SCENE_SEEN, SHELTER_B2_BREEDING_ROOM_FIRST_SCENE_SEEN);
            if (gameFlagGetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE) == SHELTER_B2_BREEDING_ROOM_OBJECTIVE_BEFORE_SCENE) {
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, SHELTER_B2_BREEDING_ROOM_OBJECTIVE_AFTER_SCENE);
            }
            Gp_CapFile = NULL;
            capSelectLoadedFile(SHELTER_B2_BREEDING_ROOM_CAP_FILE);
            capSetTexturePage(SHELTER_B2_BREEDING_ROOM_CAP_TEXTURE_X, SHELTER_B2_BREEDING_ROOM_CAP_TEXTURE_Y);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(SHELTER_B2_BREEDING_ROOM_CAP_FIRST_SCENE);
            taskSpawnFromTable(D_shelter_b2_breeding_room_80180444, SHELTER_B2_BREEDING_ROOM_TASK_FINISH_FIRST_SCENE, 0, 0);
        }
    }
    return 0;
}

/// Ignores trigger actions and returns zero without reading the borrowed request.
static s32 _shelterB2BreedingRoomIgnoreAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Maps room sound cues 7 and 104 to this room's sound scripts 7 and 8.
///
/// Event completion sends the event's second payload plus 100 as a cue;
/// this handler recognizes 104. Other cues do nothing.
/// Uses the base pan and gain; the room sound bank must be loaded. Returns zero
/// regardless of whether the sound request was queued.
static s32 _shelterB2BreedingRoomSoundMsg(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum {
        SHELTER_B2_BREEDING_ROOM_SOUND_CUE_7    = 7,
        SHELTER_B2_BREEDING_ROOM_SOUND_CUE_104  = 104,
        SHELTER_B2_BREEDING_ROOM_SOUND_SCRIPT_7 = 7,
        SHELTER_B2_BREEDING_ROOM_SOUND_SCRIPT_8 = 8
    };

    switch (cueId) {
        case SHELTER_B2_BREEDING_ROOM_SOUND_CUE_7:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, SHELTER_B2_BREEDING_ROOM_SOUND_SCRIPT_7), 0, 0);
            break;
        case SHELTER_B2_BREEDING_ROOM_SOUND_CUE_104:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, SHELTER_B2_BREEDING_ROOM_SOUND_SCRIPT_8), 0, 0);
            break;
    }
    return 0;
}

/// Waits for the first room scene to finish, then restores dialogue and player control.
///
/// Spawned after the room command selects the scene's CAP resource and holds
/// the player. Requires the live player and the room's default CAP resource.
/// Resets CAP only after its selected sequence is released, then kills this
/// bodyless task; do not access the task after completion.
static void _shelterB2BreedingRoomFinishFirstSceneTask(Task* task)
{
    if (capIsBusy() == 0) {
        capReset();
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        taskKill(task);
    }
}

/// Publishes the room's message handlers and enables CAP-completion sound cues.
///
/// Runs at state 0 and advances to idle state 1. The task and this overlay's
/// message table must remain live while `GAME_TASK_SLOT_ROOM` receives messages.
static void _shelterB2BreedingRoomInitializeRoomTask(Task* task)
{
    enum { SHELTER_B2_BREEDING_ROOM_CAP_COMPLETION_SOUNDS_ENABLED = 1 };

    task->msgTable = D_shelter_b2_breeding_room_80180414;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    D_80115598 = SHELTER_B2_BREEDING_ROOM_CAP_COMPLETION_SOUNDS_ENABLED;
}

/// Keeps the room task available for messages without per-frame work.
static void _shelterB2BreedingRoomIdleRoomTask(Task* task)
{
}

/// The room task's three states, dispatched by
/// `shelterB2BreedingRoomRoomTask`: install the message table, idle,
/// end.
static const TaskFuncTable3 D_shelter_b2_breeding_room_8017D5C4 = {
    { _shelterB2BreedingRoomInitializeRoomTask, _shelterB2BreedingRoomIdleRoomTask, taskKill }
};

void shelterB2BreedingRoomRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b2_breeding_room_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
