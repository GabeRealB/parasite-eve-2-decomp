#include "rooms/shelter_1f_guardroom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// The room's message table, installed on its event task in state 0.
extern GpMsgEntry D_shelter_1f_guardroom_8017DA30[];
extern TaskDesc   D_shelter_1f_guardroom_8017DA60;
extern TaskDesc   D_shelter_1f_guardroom_8017DA6C;
extern Task*      D_shelter_1f_guardroom_8017E014;

static void func_shelter_1f_guardroom_8017D824(Task* arg0);
static void func_shelter_1f_guardroom_8017D878(Task* task);
static void func_shelter_1f_guardroom_8017D9CC(s32 arg0);

/// The event task's three states: set-up, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_guardroom_8017D5C4 = {
    {
        func_shelter_1f_guardroom_8017D824,
        func_shelter_1f_guardroom_8017D878,
        taskKill,
    },
};

extern GpGridParams               D_shelter_1f_guardroom_8017DBF0[1];
extern GpObj4C                    D_shelter_1f_guardroom_8017DE3C[2];
extern GpObj4C                    D_shelter_1f_guardroom_8017DED4[3];
extern WorldCoordRoomAmbientEntry D_shelter_1f_guardroom_8017DFB8[4];
extern GpRoomCoordSet             D_shelter_1f_guardroom_8017DE24[1];
s32                               func_shelter_1f_guardroom_8017D73C(Task*, s32, GpMessageArg, GpMessageArg);
s32                               func_shelter_1f_guardroom_8017D744(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                               func_shelter_1f_guardroom_8017D788(Task*, s32, s32, GpMessageArg);
s32                               func_shelter_1f_guardroom_8017D7E8(Task*, s32, GpMessageArg, GpMessageArg);
s32                               func_shelter_1f_guardroom_8017D7F0(Task*, s32, s32, GpMessageArg);
void                              func_shelter_1f_guardroom_8017D5E8(Task*);
void                              func_shelter_1f_guardroom_8017D8D8(Task*);

GpMsgEntry D_shelter_1f_guardroom_8017DA30[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_guardroom_8017D744 },
    { 5105, func_shelter_1f_guardroom_8017D73C },
    { 5103, func_shelter_1f_guardroom_8017D7E8 },
    { 5104, func_shelter_1f_guardroom_8017D788 },
    { 5106, func_shelter_1f_guardroom_8017D7F0 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_1f_guardroom_8017DA60 = { 0, 32, func_shelter_1f_guardroom_8017D5E8, { .model = NULL } };

TaskDesc D_shelter_1f_guardroom_8017DA6C = { 0, 192, func_shelter_1f_guardroom_8017D8D8, { .model = NULL } };

GpRoomObjRec D_shelter_1f_guardroom_8017DA78[1] = {
    { D_shelter_1f_guardroom_8017DBF0, D_shelter_1f_guardroom_8017DE3C, D_shelter_1f_guardroom_8017DED4, NULL },
};

GpRoomCoordRec D_shelter_1f_guardroom_8017DA88[1] = {
    { D_shelter_1f_guardroom_8017DE24, D_shelter_1f_guardroom_8017DFB8 },
};

u8* D_shelter_1f_guardroom_8017DA90[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_guardroom_8017DA94[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_shelter_1f_guardroom_8017DA98[1] = {
    { { .words = { 2048, -9000, 0, -3620 } }, { 0, 0, 0, 0 }, { .words = { 2048, -9000, 0, -3620 } }, { 0, 0, 0, 0 }, 0x55060002, 0x55060001, 0, 2, 0, 0 },
};

SVECTOR D_shelter_1f_guardroom_8017DAD0[7] = {
#include "assets/shelter_1f_guardroom_collision_00630_normals.inc"
};

SVECTOR D_shelter_1f_guardroom_8017DB08[14] = {
#include "assets/shelter_1f_guardroom_collision_00630_verts.inc"
};

GpGridFace D_shelter_1f_guardroom_8017DB78[8] = {
#include "assets/shelter_1f_guardroom_collision_00630_faces.inc"
};

s16 D_shelter_1f_guardroom_8017DBD8[10] = {
#include "assets/shelter_1f_guardroom_collision_00630_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_1f_guardroom_8017DBD8[i])
s16* D_shelter_1f_guardroom_8017DBEC[1] = {
#include "assets/shelter_1f_guardroom_collision_00630_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_1f_guardroom_8017DBF0[1] = {
    { NULL, D_shelter_1f_guardroom_8017DAD0, D_shelter_1f_guardroom_8017DB08, D_shelter_1f_guardroom_8017DB78, D_shelter_1f_guardroom_8017DBEC, 0x2904, 4500, 1, 1, 4000, 8 },
};

GpViewRec D_shelter_1f_guardroom_8017DC14[3] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 597, 0, 4052 }, { 2175, 3455, -320 }, { -3418, 2198, 504 } }, { 6020, 2440, 4040 } }, 207 },
    { { { { 756, 0, -4025 }, { -1977, 3567, -371 }, { 3506, 2012, 658 } }, { 9590, 2480, 4320 } }, 257 },
};

SpriteBatch D_shelter_1f_guardroom_8017DC80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_guardroom_8017DC90[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_1f_guardroom_8017DCA0[2] = {
    { 142, 0x3FC0, { .fields = { 72, 160 } }, -72, -120, 2500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 160 } }, 0, -120, 2500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_1f_guardroom_8017DCC8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_1f_guardroom_8017DCE0[3] = {
    { { .empty = D_shelter_1f_guardroom_8017DC80 }, D_shelter_1f_guardroom_8017DC80, NULL },
    { { .empty = D_shelter_1f_guardroom_8017DC90 }, D_shelter_1f_guardroom_8017DC90, NULL },
    { { .elements = D_shelter_1f_guardroom_8017DCA0 }, D_shelter_1f_guardroom_8017DCC8, NULL },
};

GpPointLight D_shelter_1f_guardroom_8017DD04[3] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9420, -2000, -3790 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 2867, 2457, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7690, -2000, -3790 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 2867, 2457, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -2110, -3400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 0, 0, { 0, 0 } }, 1000, 1500 },
};

GpRoomCoordSet D_shelter_1f_guardroom_8017DE24[1] = {
    { 0, NULL, 3, D_shelter_1f_guardroom_8017DD04, 0, NULL },
};

GpObj4C D_shelter_1f_guardroom_8017DE3C[2] = {
    { NULL, NULL, NULL, { -7645, -1408, -3616, 0 }, { { 75, -1872, -1898, 0 }, { -87, -1872, 1885, 0 }, { 75, 1872, -1898, 0 }, { -87, 1872, 1885, 0 } }, { 4098, 0, 175, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -7472, -1440, -3600, 0 }, { { -78, -1840, 1851, 0 }, { 62, -1840, -1863, 0 }, { -78, 1840, 1851, 0 }, { 62, 1840, -1863, 0 } }, { -4095, 0, -155, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 2, 3, 129, 0 },
};

GpObj4C D_shelter_1f_guardroom_8017DED4[3] = {
    { NULL, NULL, NULL, { -9360, -48, -3520, 0 }, { { -656, 0, -224, 0 }, { 656, 0, -224, 0 }, { -656, 0, 224, 0 }, { 656, 0, 224, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 692, 0, 2, 18, 2, 0 },
    { NULL, NULL, NULL, { -7488, -64, -3136, 0 }, { { -656, 0, -224, 0 }, { 656, 0, -224, 0 }, { -656, 0, 224, 0 }, { 656, 0, 224, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 692, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -6960, -64, -3728, 0 }, { { -288, 0, -816, 0 }, { 288, 0, -816, 0 }, { -288, 0, 816, 0 }, { 288, 0, 816, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 863, 2, 2, 255, 130, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_guardroom_8017DFB8[4] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_guardroom_8017DFB8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 300, 300, 300, 300 } },
};

s32 D_shelter_1f_guardroom_8017DFD8[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_shelter_1f_guardroom_8017DFE4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_1f_guardroom_8017DFEC[1] = {
    { 0, 0, 1, 0, D_shelter_1f_guardroom_8017DFD8 },
};

GpRoomParamRec* D_shelter_1f_guardroom_8017DFF4[8] = {
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFEC,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
    D_shelter_1f_guardroom_8017DFE4,
};

Task* D_shelter_1f_guardroom_8017E014 = NULL;

/// Cutscene task spawned from the 0x13F0 handler: runs cap command 2, waits for
/// it, and when the cap event key reads 0xB hides the HUD and runs the task
/// described at `D_shelter_1f_guardroom_8017DA6C` until it is killed. It then
/// restores the HUD, calls `func_shelter_1f_guardroom_8017D9CC(1)`, sets game
/// nibble 0xB2 to 1 and hands the weapon back. Any other key ends it at once.
void func_shelter_1f_guardroom_8017D5E8(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(2);
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                goto next;
            }
            break;
        case 2:
            Gp_CapCmds[2].command->field_4 = 1;
            if (Gp_GetCapEventKey() != 0xB) {
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            gGameSession->hideHud           = 1;
            D_shelter_1f_guardroom_8017E014 = Task_SpawnFromTable(&D_shelter_1f_guardroom_8017DA6C, 0, 0, 0);
            task->state++;
            break;
        case 3:
            if (Task_PollKill(D_shelter_1f_guardroom_8017E014, &poll) == 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            gGameSession->hideHud = 0;
            func_shelter_1f_guardroom_8017D9CC(1);
            GameFlag_SetNibble(0xB2, 1);
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_1f_guardroom_8017D73C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming `RoomEventMsg` onto
/// the outgoing one, passes both to `func_map_neo_ark_80179B14`, and returns 1.
s32 func_shelter_1f_guardroom_8017D744(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: when its third argument is 2 and game
/// nibble 0xB2 is still clear, takes the weapon away and spawns the cutscene task
/// `func_shelter_1f_guardroom_8017D5E8`; once the nibble is set it runs cap
/// command 3 instead. Returns 0.
s32 func_shelter_1f_guardroom_8017D788(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 2) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_1f_guardroom_8017DA60, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(3);
        }
    }
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_1f_guardroom_8017D7E8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13F2: when its third argument is 3, queues
/// sound event 0x55060003. Returns 0.
s32 func_shelter_1f_guardroom_8017D7F0(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x55060003, 0, 0);
    }
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7, passes game nibble 0xB2 to
/// `func_shelter_1f_guardroom_8017D9CC` and advances to state 1.
static void func_shelter_1f_guardroom_8017D824(Task* arg0)
{
    arg0->msgTable = D_shelter_1f_guardroom_8017DA30;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_1f_guardroom_8017D9CC(GameFlag_GetNibble(0xB2) & 0xFF);
    arg0->state = (s32)(arg0->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_shelter_1f_guardroom_8017D878(Task* task)
{
}

/// The room's event task: copies its state table onto the stack and calls the
/// entry for the current state.
void func_shelter_1f_guardroom_8017D880(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_guardroom_8017D5C4;
    sp.funcs[task->state](task);
}

/// Task the cutscene task spawns: calls `func_shelter_1f_guardroom_8017D9CC(0)`,
/// raises the CD queue's `field_1EA`, enqueues CD command 0x61 for the stream
/// slot of the session's current location, waits for the queue's `field_1FA`,
/// then for the CD to go idle, and requests its own kill.
void func_shelter_1f_guardroom_8017D8D8(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    switch (arg0->state) {
        case 0:
            func_shelter_1f_guardroom_8017D9CC(0);
            queue->movieFrame = 1;
            slotParam[0]      = Stream_FindSlot((u8*)&gGameSession->at4, 0, 0);
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            arg0->state++;
            break;
        case 1:
            if (queue->movieReady != 0) {
                arg0->state = 2;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Task_RequestKill(arg0, 0);
            }
            break;
    }
}

/// Sets `field_4` of the second sprite command in entry 2 of the current
/// area's sprite table: 1 when the low byte of `arg0` is zero, 0 otherwise.
static void func_shelter_1f_guardroom_8017D9CC(s32 arg0)
{
    GameLocationKey* sess = &gGameSession->at4.loc;
    SpriteBatch*     batches;

    batches = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][2].field_4;
    if ((arg0 & 0xFF) == 0) {
        batches[1].hidden = 1;
    } else {
        batches[1].hidden = 0;
    }
}

void func_shelter_1f_guardroom_8017DA28(Task* unused)
{
}
