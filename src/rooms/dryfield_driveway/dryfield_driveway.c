#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// An event the room's message handler latches for its own event task. The
/// handler builds it on the stack and copies it whole. `field_0` is the CAP
/// command the task runs and `field_4` the stage sound it then plays;
/// `flagId` is the game-flag nibble set once the event has fired (zero: none);
/// a non-zero `field_A` makes the task start helper task 0x31.
typedef struct {
    s32 field_0;
    s32 field_4;
    s16 flagId;
    u8  field_A;
} _LatchedEvent;

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern s32 D_dryfield_driveway_8017E384;
extern s32 D_dryfield_driveway_8017E4FC;
extern s32 D_dryfield_driveway_8017E67C;

extern TaskDesc D_dryfield_driveway_8017E2F0;
extern TaskDesc D_dryfield_driveway_8017E2FC[];

extern GpStateBD8    D_dryfield_driveway_80180680;
extern RoomEventMsg  D_dryfield_driveway_80180688;
extern u8            D_dryfield_driveway_80180690;
extern _LatchedEvent D_dryfield_driveway_80180694;

/// The room's event task, spawned by its message handler for a latched event.
/// State 0 runs the event's CAP command; state 1 waits for it and, when the
/// event asks for it, starts helper task 0x31; states 2 and 3 play the event's
/// stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_dryfield_driveway_8017D5E4(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_driveway_80180694.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_driveway_80180694.field_A != 0) {
                    D_dryfield_driveway_80180680.field_0 = 0;
                    D_dryfield_driveway_80180680.field_1 = 0;
                    D_dryfield_driveway_80180680.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_driveway_80180680);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_driveway_80180694.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_driveway_80180694.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_driveway_80180694.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_driveway_80180688.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_driveway_80180688.field_2;
            Mc_SaveData.at4.loc.room = D_dryfield_driveway_80180688.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// The room's message handler. Every message is answered by editing the copy
/// in `out`. Message 0x17 reports the state of flag nibble 0x47 and, unless
/// nibble 0x30 is set, latches an event for `func_dryfield_driveway_8017D5E4`;
/// message 0x20 reports nibbles 0x51 and 0x53 and, before nibble 0x3A reaches
/// 2, either spawns the second cutscene task of `D_dryfield_driveway_8017E2FC`
/// or runs CAP command 1; message 2 runs CAP command 6 once nibble 0x61 is set.
s32 func_dryfield_driveway_8017D77C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    _LatchedEvent  req;
    _LatchedEvent* p;
    s32            fl;

    *out = *in;
    if (in->msgId == 0x17 && in->field_5 == 0) {
        fl           = GameFlag_GetNibble(0x47) == 0;
        out->field_3 = fl ? 1 : 2;
    }
    if (in->msgId == 0x20 && in->field_5 == 0) {
        fl           = GameFlag_GetNibble(0x51) == 0;
        out->field_3 = fl ? 2 : 1;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = out->field_3 + 2;
        }
    }
    if (in->msgId == 2 && GameFlag_GetNibble(0x61) != 0) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(6);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 2;
    }
    if (in->msgId == 0x20) {
        if (GameFlag_GetNibble(0x3A) != 2) {
            if (in->field_5 == 0) {
                if (gGameSession->at4.loc.stage == 2) {
                    if (gGameSession->at4.loc.place == 1) {
                        if (GameFlag_GetNibble(0x50) == 0) {
                            Task_SpawnFromTable(D_dryfield_driveway_8017E2FC, 1, 0, 0);
                            return 0;
                        }
                    }
                }
                if (gGameSession->at4.loc.place == 1 && Gp_StateF0.field_0 == gGameSession->at4.loc.place) {
                    return 0;
                }
                Gp_RunCapCmd1(1);
                return 0;
            }
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x4B) == 1) {
            GameFlag_SetNibble(0x4B, 2);
        }
    }
    if (in->msgId == 0x17) {
        if (GameFlag_GetNibble(0x30) == 1) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(2);
                return 2;
            }
            return 2;
        }
        req.field_0                  = 9;
        req.field_4                  = 0x52190003;
        req.flagId                   = 0x11C;
        req.field_A                  = 0;
        p                            = &req;
        D_dryfield_driveway_80180690 = 0;
        if (GameFlag_GetNibble(p->flagId) == 0 || p->flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_driveway_80180688 = *out;
                D_dryfield_driveway_80180694 = req;
                if (p->flagId != 0) {
                    GameFlag_SetNibble(p->flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_driveway_8017E2F0, 0, 0, 0);
                D_dryfield_driveway_80180690 = 1;
                return 2;
            }
            return 2;
        }
    }
    return 1;
}

/// First cutscene task: on its first tick it hides the HUD and the display
/// and starts the captioned cutscene; on the next it kills itself and clears
/// collected bit 0x114. Either way it advances its state.
void func_dryfield_driveway_8017DAD0(Task* arg0)
{
    if (arg0->state == 0) {
        gGameSession->hideHud = 1;
        D_80115768            = 1;
        SetDispMask(0);
        func_800E3FAC(0xA2, 0x10);
        func_800E8634((s32)&D_dryfield_driveway_8017E4FC, 0, (s32)&D_dryfield_driveway_8017E67C);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x114);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Second cutscene task: state 0 queues the weapon message and CAP command 1,
/// state 1 waits a tick, state 2 starts the cutscene at
/// `D_dryfield_driveway_8017E384`, and state 3 - reached by falling out of
/// state 2 - clears area flag 4 for the current location and kills the task
/// once `eventState` is zero.
void func_dryfield_driveway_8017DB68(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            arg0->state += 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            func_800E8614((s32)&D_dryfield_driveway_8017E384, 0);
            arg0->state += 1;
            /* fallthrough */
        case 3:
            if (gGameSession->eventState == 0) {
                Gp_ClearAreaFlag4((GpAreaKey*)&gGameSession->at4.loc);
                taskKill(arg0);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_driveway/dryfield_driveway", D_dryfield_driveway_8017D5D8);
