#include "object_task.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/object_task.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

#include "stages/akropolis/stage.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

u8 gCapRoomCompletionSoundsEnabled;

/// Stage-indexed room-task descriptor lists; slot zero is NULL, stages 1..5 are valid.
///
/// The room selector takes the first descriptor matching the exact room key
/// or area-default key in list order. Lists end at `TASK_DESC_END` and belong to
/// the loaded map overlay; selected room callback code must also remain loaded.
static TaskDesc* const _gObjectTaskStageRoomTables[6] __attribute__((section(".data"))) = {
    NULL,
    gAkropolisStageRoomTaskDescriptors,
    gMapDryfieldRoomTaskDescriptors,
    gMapDryfieldFullRoomTaskDescriptors,
    gMapShelterRoomTaskDescriptors,
    gMapNeoArkRoomTaskDescriptors,
};

/// Fallback receiver when no room-task descriptor matches the current location.
///
/// Resolves a transition by copying the borrowed RoomEventMsg request to its
/// writable reply unchanged; refuses every collected-key-item request with 0.
/// Other IDs return zero at `TASK_MESSAGE_TABLE_END`. Installed on the selector
/// itself in the room task slot, with no retained message payloads.
static const TaskMessageEntry _gObjectTaskDefaultRoomMessages[3] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, objectTaskResolveDefaultRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, objectTaskRefuseDefaultRoomKeyItemUse },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Resets room event presentation and reapplies the saved music policy.
///
/// Requires a live session. Clears event gating, HUD suppression, CAP completion
/// sound cues and all session music/weapon flow options before room task selection.
/// Music override 1 (submarine progress) selects countdown-table entry 1 at
/// Dryfield night and suppresses battle-start/area-restoration music elsewhere;
/// override 2 (parking progress) selects entry 9. Other nibble values preserve
/// the existing entry. Selection and load policy take effect on later requests.
static inline void _objectTaskResetRoomPresentation(void)
{
    enum {
        OBJECT_TASK_ROOM_EVENT_IDLE                = 0,
        OBJECT_TASK_CAP_COMPLETION_SOUNDS_DISABLED = 0,
        OBJECT_TASK_MUSIC_OVERRIDE_SUBMARINE       = 1,
        OBJECT_TASK_MUSIC_OVERRIDE_PARKING         = 2,
        OBJECT_TASK_DRYFIELD_NIGHT_MUSIC_ENTRY     = 1,
        OBJECT_TASK_PARKING_MUSIC_ENTRY            = 9
    };

    s32 musicOverride;

    // The selected room task can opt into CAP completion sound cues after setup.
    gGameSession->eventState        = OBJECT_TASK_ROOM_EVENT_IDLE;
    gGameSession->hideHud           = false;
    gCapRoomCompletionSoundsEnabled = OBJECT_TASK_CAP_COMPLETION_SOUNDS_DISABLED;
    gGameSession->flowFlags         = 0;
    musicOverride                   = gameFlagGetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE);
    switch (musicOverride) {
        case OBJECT_TASK_MUSIC_OVERRIDE_SUBMARINE:
            if (gGameSession->location.stage == GAME_STAGE_DRYFIELD_NIGHT) {
                gStageCountdownMusicEntry = OBJECT_TASK_DRYFIELD_NIGHT_MUSIC_ENTRY;
            } else {
                gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            }
            break;
        case OBJECT_TASK_MUSIC_OVERRIDE_PARKING:
            gStageCountdownMusicEntry = OBJECT_TASK_PARKING_MUSIC_ENTRY;
            break;
    }
}

void objectTaskInitializeRoomState(Task* task)
{
    enum {
        OBJECT_TASK_ROOM_DESCRIPTOR_HEADER = (0x20 << 16) | TASK_BODY_NONE
    };
    s32       descriptorIndex;
    s32       areaKey;
    s32       roomKey;
    s32       locationKeyBase;
    s32       roomTaskHeader;
    TaskDesc* stageRoomTable;
    TaskDesc* descriptor;

    _objectTaskResetRoomPresentation();
    // Table order decides between a room-specific descriptor and its area default.
    descriptorIndex = 0;
    locationKeyBase = OBJECT_TASK_ROOM_LOCATION_KEY(gGameSession->location.stage, gGameSession->location.area, 0);
    roomKey         = locationKeyBase + gGameSession->location.roomState;
    stageRoomTable  = _gObjectTaskStageRoomTables[gGameSession->location.stage];
    areaKey         = locationKeyBase;
    descriptor      = stageRoomTable;
    roomTaskHeader  = OBJECT_TASK_ROOM_DESCRIPTOR_HEADER;
searchDescriptor:
    if (descriptor->header.word == roomTaskHeader &&
        (descriptor->data.value == roomKey || descriptor->data.value == areaKey)) {
        taskSpawnFromTable(stageRoomTable, descriptorIndex, 0, 0);
        task->state++;
        return;
    }
    if ((descriptor++)->header.fields.flags != TASK_DESC_END) {
        descriptorIndex++;
        goto searchDescriptor;
    }
    // Keep the selector itself as the fallback room receiver when no descriptor matches.
    task->msgTable = _gObjectTaskDefaultRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

void capEventTask(Task* task)
{
    enum {
        CAP_EVENT_START                   = 0,
        CAP_EVENT_WAIT                    = 1,
        CAP_EVENT_FINISH                  = 2,
        CAP_EVENT_COMPLETION_SOUND_OFFSET = 100
    };
    s32 eventFlags;
    s32 pauseActors;
    s32 playbackMode;
    s32 pausedActorControl;

    pausedActorControl = SCENE_COMBAT_ACTORS_PAUSED;
    eventFlags         = task->spawnArg1.value;
    switch (task->state) {
        case CAP_EVENT_START:
            // Hold/hide the player before selecting and starting the CAP command.
            pauseActors = eventFlags & CAP_EVENT_PAUSE_ACTORS;
            if (pauseActors != 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                gSceneCombatState.actorControl = pausedActorControl;
            }
            if (eventFlags & CAP_EVENT_HIDE_PLAYER) {
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            }
            if (eventFlags & CAP_EVENT_ACTION_CAPTURE) {
                playbackMode = CAP_PLAYBACK_ACTION_CAPTURE;
            } else if (pauseActors == 0) {
                playbackMode = CAP_PLAYBACK_CLEAR_IF_UNSTARTED;
            } else {
                playbackMode = CAP_PLAYBACK_IN_PLACE;
            }
            capRunCommand(task->spawnArg2.value, playbackMode);
            task->state++;
            break;
        case CAP_EVENT_WAIT:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case CAP_EVENT_FINISH:
            // Release the requested presentation holds, then send the optional room cue.
            if (eventFlags & CAP_EVENT_PAUSE_ACTORS) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            }
            if (eventFlags & CAP_EVENT_HIDE_PLAYER) {
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            }
            if (gCapRoomCompletionSoundsEnabled != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_SOUND, task->spawnArg2.value + CAP_EVENT_COMPLETION_SOUND_OFFSET, 0);
            }
            taskKill(task);
            break;
    }
}
