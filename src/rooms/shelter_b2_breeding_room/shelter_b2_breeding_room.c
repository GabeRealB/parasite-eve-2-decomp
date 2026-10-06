#include "rooms/shelter_b2_breeding_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/captions.h"
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

/// Task table spawned by `func_shelter_b2_breeding_room_8017D6A4` once the
/// breeding-room script has run.
extern TaskDesc D_shelter_b2_breeding_room_80180444[];

/// Message table `func_shelter_b2_breeding_room_8017D7EC` installs on its task.
extern TaskMessageEntry D_shelter_b2_breeding_room_80180414[];

s32  func_shelter_b2_breeding_room_8017D658(Task*, s32, s32, s32);
s32  func_shelter_b2_breeding_room_8017D660(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_breeding_room_8017D6A4(Task*, s32, s32, s32);
s32  func_shelter_b2_breeding_room_8017D750(Task*, s32, s32, s32);
s32  func_shelter_b2_breeding_room_8017D758(Task*, s32, s32, s32);
void func_shelter_b2_breeding_room_8017D7A8(Task*);

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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_breeding_room_8017D660 },
    { 5105, func_shelter_b2_breeding_room_8017D658 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_breeding_room_8017D750 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_breeding_room_8017D6A4 },
    { ROOM_MESSAGE_SOUND, func_shelter_b2_breeding_room_8017D758 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b2_breeding_room_80180444[1] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_breeding_room_8017D7A8, { .value = 0 } },
};

static void func_shelter_b2_breeding_room_8017D7EC(Task* arg0);
static void func_shelter_b2_breeding_room_8017D838(Task* task);

/// Hides the task's model while the 2-bit game flag its spawn argument names
/// reads 2, and shows it otherwise.
void func_shelter_b2_breeding_room_8017D5F8(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (Gp_GetCurBit2Flag((u8)((Enemy*)task->spawnArg2.pointer)->placeKey) == 2) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

s32 func_shelter_b2_breeding_room_8017D658(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// passes both to `func_map_shelter_80179A04`, returning 1.
s32 func_shelter_b2_breeding_room_8017D660(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

/// Handler for msg `0x16`: the first entry into the breeding room. Runs the
/// scripted scene once, then replays cap script `0x16` on later visits.
s32 func_shelter_b2_breeding_room_8017D6A4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 0x16) {
        if (gameFlagGetNibble(GAME_FLAG_BREEDING_ROOM_FIRST_SCENE_SEEN) != 0) {
            Gp_RunCapCmd1(0x16);
        } else {
            gameFlagSetNibble(GAME_FLAG_BREEDING_ROOM_FIRST_SCENE_SEEN, 1);
            if (func_800E3FCC(0xA2) == 0x1E) {
                func_800E3FAC(0xA2, 0x1F);
            }
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            capSetTexturePage(0x140, 0x100);
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            taskSpawnFromTable(D_shelter_b2_breeding_room_80180444, 0, 0, 0);
        }
    }
    return 0;
}

s32 func_shelter_b2_breeding_room_8017D750(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b2_breeding_room_8017D758(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 7:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 7), 0, 0);
            break;
        case 0x68:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 8), 0, 0);
            break;
    }
    return 0;
}

void func_shelter_b2_breeding_room_8017D7A8(Task* arg0)
{
    if (capIsBusy() == 0) {
        Gp_ResetCap();
        Gp_MsgPlayerWeapon(1);
        taskKill(arg0);
    }
}

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7, sets `D_80115598` and advances to the next state.
static void func_shelter_b2_breeding_room_8017D7EC(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_breeding_room_80180414;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

static void func_shelter_b2_breeding_room_8017D838(Task* task)
{
}

/// The room task's three states, dispatched by
/// `func_shelter_b2_breeding_room_8017D840`: install the message table, idle,
/// end.
static const TaskFuncTable3 D_shelter_b2_breeding_room_8017D5C4 = {
    { func_shelter_b2_breeding_room_8017D7EC, func_shelter_b2_breeding_room_8017D838, taskKill }
};

/// Runs the handler for the task's state from the room's three-entry state
/// table, copied onto the stack first.
void func_shelter_b2_breeding_room_8017D840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_breeding_room_8017D5C4;
    sp.funcs[task->state](task);
}
