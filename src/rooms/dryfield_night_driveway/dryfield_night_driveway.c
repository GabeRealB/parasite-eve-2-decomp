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

/// An event the event gate latches for the room's event task. The gate builds
/// it on the stack and copies it whole. `field_0` is the CAP command the task
/// runs and `field_4` the stage sound it then plays; `flagId` is the game-flag
/// nibble set once the event has fired (zero: none); a non-zero `field_A` makes
/// the task start helper task 0x31.
typedef struct {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 flagId;
    /* 0xA */ u8  field_A;
} DrivewayReq;

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern s32 D_dryfield_night_driveway_8017F3D4;
extern s32 D_dryfield_night_driveway_8017F54C;
extern s32 D_dryfield_night_driveway_8017F6CC;

/// Spawn argument of the helper task 0x31 the event task starts.
extern GpStateBD8 D_dryfield_night_driveway_80182110;

/// The message and the event the event gate latched for the event task, and
/// the flag it sets when it latches one.
extern RoomEventMsg D_dryfield_night_driveway_80182118;
extern u8           D_dryfield_night_driveway_80182120;
extern DrivewayReq  D_dryfield_night_driveway_80182124;
extern TaskDesc     D_dryfield_night_driveway_8017E678;
extern TaskDesc     D_dryfield_night_driveway_8017F34C;

/// The room's event task, spawned by the event gate. State 0 runs the latched
/// event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound and wait for it (state 2 skips to 4 when there is none); state 4
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11.
void func_dryfield_night_driveway_8017D608(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_night_driveway_80182124.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_night_driveway_80182124.field_A != 0) {
                    D_dryfield_night_driveway_80182110.field_0 = 0;
                    D_dryfield_night_driveway_80182110.field_1 = 0;
                    D_dryfield_night_driveway_80182110.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_night_driveway_80182110);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_night_driveway_80182124.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_night_driveway_80182124.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_night_driveway_80182124.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_driveway_80182118.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_driveway_80182118.field_2;
            Mc_SaveData.at4.loc.room = D_dryfield_night_driveway_80182118.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Event gate for the driveway. Every message is answered by editing the copy
/// in `out`; the two that matter are message 0x17, which reports whether the
/// road flag is clear and otherwise stages the pending request at
/// `D_dryfield_night_driveway_80182124` for `func_dryfield_night_driveway_8017D608`
/// to replay as a CAP command, and message 0x20, which reports the gate flag and
/// spawns the cutscene task at `D_dryfield_night_driveway_8017F34C`.
s32 func_dryfield_night_driveway_8017D7A0(s32 arg0, s32 arg1, RoomEventMsg* in,
                                          RoomEventMsg* out)
{
    DrivewayReq  req;
    DrivewayReq* p;
    s32          fl;

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
                            Task_SpawnFromTable(&D_dryfield_night_driveway_8017F34C, 1, 0, 0);
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
        req.field_0                        = 9;
        req.field_4                        = 0x52190003;
        req.flagId                         = 0x11C;
        req.field_A                        = 0;
        p                                  = &req;
        D_dryfield_night_driveway_80182120 = 0;
        if (GameFlag_GetNibble(p->flagId) == 0 || p->flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_driveway_80182118 = *out;
                D_dryfield_night_driveway_80182124 = req;
                if (p->flagId != 0) {
                    GameFlag_SetNibble(p->flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_driveway_8017E678, 0, 0, 0);
                D_dryfield_night_driveway_80182120 = 1;
                return 2;
            }
            return 2;
        }
    }
    return 1;
}

/// Task callback: on its first tick it hides the display and hands control to
/// the captioned cutscene; on every later tick it kills the task and clears the
/// collected bit. Either way it advances its own state.
void func_dryfield_night_driveway_8017DAF4(Task* arg0)
{
    if (arg0->state == 0) {
        gGameSession->hideHud = 1;
        D_80115768            = 1;
        SetDispMask(0);
        func_800E3FAC(0xA2, 0x10);
        func_800E8634((s32)&D_dryfield_night_driveway_8017F54C, 0, (s32)&D_dryfield_night_driveway_8017F6CC);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x114);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Task callback: a four-step script. State 0 queues the weapon message and the
/// captioned command, state 1 waits one tick, state 2 starts the cutscene at
/// `D_dryfield_night_driveway_8017F3D4`, and state 3 - reached by falling out of
/// state 2 - clears area flag 4 for the current location and kills the task once
/// `eventState` is zero.
void func_dryfield_night_driveway_8017DB8C(Task* arg0)
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
            func_800E8614((s32)&D_dryfield_night_driveway_8017F3D4, 0);
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

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", D_dryfield_night_driveway_8017D5D8);
