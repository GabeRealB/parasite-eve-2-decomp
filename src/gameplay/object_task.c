#include "object_task.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

u8 D_80115598;

/// Per-stage task descriptor tables searched by `objectTaskInitializeRoomState`.
extern TaskDesc* D_8010FABC[];

/// Message table of the stand-in room task, used where the stage's table has
/// no task for the current room: a room-transition request is echoed back as
/// its reply, and a key-item use (0x13F1) is refused.
extern TaskMessageEntry D_8010FAD4[];

TaskDesc* D_8010FABC[6] = {
    NULL,
    D_map_akropolis_8017A8AC,
    D_map_dryfield_8017A6A4,
    D_map_dryfield_full_8017A5AC,
    D_map_shelter_8017AB30,
    D_map_neo_ark_8017A804,
};

TaskMessageEntry D_8010FAD4[3] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, objectTaskResolveDefaultRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, objectTaskRefuseDefaultRoomKeyItemUse },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Releases room event presentation holds and applies the persistent music override.
///
/// Requires a live session and resident music selector. Submarine progress uses
/// entry 1 at Dryfield night and suppresses area/ending music elsewhere;
/// parking progress uses entry 9. Other flag values leave the selector intact.
static inline void _objectTaskResetRoomPresentation(void)
{
    enum {
        OBJECT_TASK_MUSIC_OVERRIDE_SUBMARINE   = 1,
        OBJECT_TASK_MUSIC_OVERRIDE_PARKING     = 2,
        OBJECT_TASK_DRYFIELD_NIGHT_MUSIC_ENTRY = 1,
        OBJECT_TASK_PARKING_MUSIC_ENTRY        = 9
    };

    s32 musicOverride;

    gGameSession->eventState = 0;
    gGameSession->hideHud    = 0;
    D_80115598               = 0;
    gGameSession->flowFlags  = 0;
    musicOverride            = gameFlagGetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE);
    switch (musicOverride) {
        case OBJECT_TASK_MUSIC_OVERRIDE_SUBMARINE:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
                gStageSceneMusicEntry = OBJECT_TASK_DRYFIELD_NIGHT_MUSIC_ENTRY;
            } else {
                gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            }
            break;
        case OBJECT_TASK_MUSIC_OVERRIDE_PARKING:
            gStageSceneMusicEntry = OBJECT_TASK_PARKING_MUSIC_ENTRY;
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
    locationKeyBase = GP_TASK_LOC_KEY(gGameSession->location.loc.stage, gGameSession->location.loc.area, 0);
    roomKey         = locationKeyBase + gGameSession->location.loc.room;
    stageRoomTable  = D_8010FABC[gGameSession->location.loc.stage];
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
    task->msgTable = D_8010FAD4;
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
            if (D_80115598 != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_SOUND, task->spawnArg2.value + CAP_EVENT_COMPLETION_SOUND_OFFSET, 0);
            }
            taskKill(task);
            break;
    }
}
