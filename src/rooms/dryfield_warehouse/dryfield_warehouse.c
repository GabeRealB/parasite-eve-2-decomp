#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_warehouse.h"
#include "rooms/room_common.h"

extern GpMsgEntry D_dryfield_warehouse_8017F554[];
extern TaskDesc   D_dryfield_warehouse_8017F56C[];
extern TaskDesc   D_dryfield_warehouse_8017FB08;

/// Cutscene task spawned by state 0, polled by `Task_PollKill` in state 1 and
/// killed along with its parent in state 2.
extern Task* D_dryfield_warehouse_801821B4;

/// Volume last asked of the warehouse's ambient track, or 0 when none is
/// playing. Written by `func_dryfield_warehouse_8017D5E8` and cleared by state 0
/// of the same task.
extern s32 D_dryfield_warehouse_801821B8;

/// Warehouse ambience: state 0 clears the recorded volume and advances, state 1
/// maps `gGameSession->field_4` (the area id) to a target volume - 0x32/0x3C/0x64
/// for areas 2/3/4, 0 elsewhere - and, whenever that differs from the recorded
/// one, enqueues the matching fade event: type 6 to start the track, type 7 to
/// stop it, type A to retune it, then records the new volume.
void func_dryfield_warehouse_8017D5E8(Task* task)
{
    s32 vol;

    switch (task->state) {
        case 0:
            D_dryfield_warehouse_801821B8 = 0;
            task->state                   = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    vol = 0;
    if (gGameSession->field_1 == 0) {
        switch (gGameSession->field_4) {
            case 4:
                vol = 0x64;
                break;
            case 3:
                vol = 0x3C;
                break;
            case 2:
                vol = 0x32;
                break;
            default:
                vol = 0;
                break;
        }
    }

    if (vol == D_dryfield_warehouse_801821B8) {
        return;
    }
    if (D_dryfield_warehouse_801821B8 == 0) {
        SndEvt_EnqueueType6(0x52070005, 0, (s8)(((0x64 - vol) * 0x7F) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x52070005, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x52070005, 0, (s8)(((0x64 - vol) * 0x7F) / 100));
    }
    D_dryfield_warehouse_801821B8 = vol;
}

/// Message handler: on msg 0x111, walks the `Gp_PendingObj4C` list looking for
/// an object in mode 5 whose `field_48` is 0xFF and which is still pending, and
/// on a hit sets event nibble 0x3C, flips `gGameSession->field_1` and spawns the
/// warehouse cutscene task. Answers 1 only when it found one.
s32 func_dryfield_warehouse_8017D764(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x111) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            GameFlag_SetNibble(0x3C, 1);
            gGameSession->field_1 = 1;
            Task_SpawnOnDefaultList(D_dryfield_warehouse_8017F56C, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

/// Copies the room message, then answers msg 9 by running CAP command 3 and
/// setting the event's nibble. field_5 suppresses the side effects (the
/// handler only reports what *would* happen); any other message plays the
/// "refused" sound instead.
s32 func_dryfield_warehouse_8017D824(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 9) {
        if (GameFlag_GetNibble(0x3C) != 0) {
            return 1;
        }
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(3);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 0;
    }
    if (in->field_5 == 0) {
        SndEvt_EnqueueType7(0x52070005, 0xF);
    }
    return 1;
}

/// Warehouse cutscene state machine: state 0 blanks the display and spawns the
/// cutscene task, state 1 waits for it to finish, and state 2 kills this task
/// once it has.
void func_dryfield_warehouse_8017D8D4(Task* arg0)
{
    s32 sp10;

    switch (arg0->state) {
        case 0:
            SetDispMask(0);
            D_dryfield_warehouse_801821B4 = Task_SpawnFromTable(&D_dryfield_warehouse_8017FB08, 0, 0, 0);
            arg0->state                  += 1;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_warehouse_801821B4, &sp10) != 0) {
                arg0->state += 1;
                return;
            }
            return;
        case 2:
            Task_Kill(arg0);
            break;
    }
}

void func_dryfield_warehouse_8017D99C(Task* arg0)
{
    arg0->field_24 = D_dryfield_warehouse_8017F554;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_warehouse_8017F56C, 1, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_warehouse_8017D9F8(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", D_dryfield_warehouse_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", jtbl_dryfield_warehouse_8017D5D0);
