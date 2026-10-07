#include "object_task.h"

#include "types.h"

#include "gameplay/captions.h"
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

/// Per-stage task descriptor tables searched by `func_800E31E8`.
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

void func_800E31E8(Task* arg0)
{
    enum { TASK_DESC_LOCATION_TASK_HEADER = (0x20 << 16) | TASK_BODY_NONE };

    s32       flag;
    s32       index;
    s32       area;
    s32       room;
    s32       base;
    s32       kind;
    TaskDesc* table;
    TaskDesc* desc;

    gGameSession->eventState = 0;
    gGameSession->hideHud    = 0;
    D_80115598               = 0;
    gGameSession->flowFlags  = 0;
    flag                     = gameFlagGetNibble(GAME_FLAG_SCENE_MUSIC_OVERRIDE);
    switch (flag) {
        case 1:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD_NIGHT) {
                gStageSceneMusicEntry = 1;
            } else {
                gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            }
            break;
        case 2:
            gStageSceneMusicEntry = 9;
            break;
    }
    index = 0;
    base  = gGameSession->location.loc.stage * 10000 + gGameSession->location.loc.area * 100;
    room  = base + gGameSession->location.loc.room;
    table = D_8010FABC[gGameSession->location.loc.stage];
    area  = base;
    desc  = table;
    kind  = TASK_DESC_LOCATION_TASK_HEADER;
loop:
    if (desc->header.word == kind &&
        (desc->data.value == room || desc->data.value == area)) {
        taskSpawnFromTable(table, index, 0, 0);
        arg0->state++;
        return;
    }
    if ((u16)(desc++)->header.word != TASK_DESC_END) {
        index++;
        goto loop;
    }
    arg0->msgTable = D_8010FAD4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state++;
}

void Gp_EvtCapTask(Task* arg0)
{
    s32 flags;
    s32 bit0;
    s32 mode;
    s32 flag;

    flag  = 1;
    flags = arg0->spawnArg1.value;
    switch (arg0->state) {
        case 0:
            bit0 = flags & 1;
            if (bit0 != 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                gSceneCombatState.actorControl = flag;
            }
            if (flags & 2) {
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            }
            if (flags & 4) {
                mode = 2;
            } else if (bit0 == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            capRunCommand(arg0->spawnArg2.value, mode);
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (flags & 1) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            }
            if (flags & 2) {
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            }
            if (D_80115598 != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_SOUND, arg0->spawnArg2.value + 0x64, 0);
            }
            taskKill(arg0);
            break;
    }
}
