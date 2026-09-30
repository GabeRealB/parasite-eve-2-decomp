#include "object_task.h"

#include "types.h"

#include "gameplay/captions.h"
#include "captions.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/scene_tasks.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

#include "mapui/map_akropolis.h"

#include "mapui/map_dryfield.h"

#include "mapui/map_dryfield_full.h"

#include "mapui/map_neo_ark.h"

#include "mapui/map_shelter.h"

/// Fallback message handlers installed by `func_800E31E8` for pointer slot 7.
typedef struct {
    s32 id;
    union {
        s32 (*location)(Task*, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*empty)(void);
    } handler;
} GpLocationMsgEntry;

u8 D_80115598;

/// Per-stage task descriptor tables searched by `func_800E31E8`.
extern GpTaskDesc* D_8010FABC[];

extern GpLocationMsgEntry D_8010FAD4[];

GpTaskDesc* D_8010FABC[6] = {
    NULL,
    D_map_akropolis_8017A8AC,
    D_map_dryfield_8017A6A4,
    D_map_dryfield_full_8017A5AC,
    D_map_shelter_8017AB30,
    D_map_neo_ark_8017A804,
};

GpLocationMsgEntry D_8010FAD4[3] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, { .location = func_800E3FF0 } },
    { 5105, { .empty = func_800E4018 } },
    { 0x7FFFFFFF, { .empty = NULL } },
};

void func_800E31E8(Task* arg0)
{
    s32         flag;
    s32         index;
    s32         area;
    s32         room;
    s32         base;
    s32         kind;
    GpTaskDesc* table;
    GpTaskDesc* desc;

    gGameSession->eventState = 0;
    gGameSession->hideHud    = 0;
    D_80115598               = 0;
    gGameSession->flowFlags  = 0;
    flag                     = GameFlag_GetNibble(0x11F);
    switch (flag) {
        case 1:
            if (gGameSession->location.loc.stage == 3) {
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
    kind  = 0x200000;
loop:
    if (desc->flagsAndPriority == kind &&
        (desc->task.arg.value == room || desc->task.arg.value == area)) {
        Task_SpawnFromTable(&table->task, index, 0, 0);
        arg0->state++;
        return;
    }
    if ((u16)(desc++)->flagsAndPriority != 0xFFFF) {
        index++;
        goto loop;
    }
    arg0->msgTable = D_8010FAD4;
    Game_SetPtrSlot(arg0, 7);
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
                Gp_MsgPlayerWeapon(0);
                Gp_StateF0.field_4 = flag;
            }
            if (flags & 2) {
                Gp_MsgPlayer3F3(0);
            }
            if (flags & 4) {
                mode = 2;
            } else if (bit0 == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            Gp_RunCapCmd(arg0->spawnArg2.value, mode);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (flags & 1) {
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
            }
            if (flags & 2) {
                Gp_MsgPlayer3F3(1);
            }
            if (D_80115598 != 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F2, arg0->spawnArg2.value + 0x64, 0);
            }
            taskKill(arg0);
            break;
    }
}
