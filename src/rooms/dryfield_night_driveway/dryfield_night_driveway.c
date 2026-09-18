#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/libgpu.h>

typedef struct {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 flagId;
    /* 0xA */ u8  field_A;
} DrivewayReq;

extern s32 D_dryfield_night_driveway_8017F3D4;
extern s32 D_dryfield_night_driveway_8017F54C;
extern s32 D_dryfield_night_driveway_8017F6CC;

extern RoomEventMsg D_dryfield_night_driveway_80182118;
extern u8           D_dryfield_night_driveway_80182120;
extern DrivewayReq  D_dryfield_night_driveway_80182124;
extern TaskDesc     D_dryfield_night_driveway_8017E678;
extern TaskDesc     D_dryfield_night_driveway_8017F34C;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", D_dryfield_night_driveway_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", func_dryfield_night_driveway_8017D608);

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
        Task_Kill(arg0);
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
                Task_Kill(arg0);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_driveway/dryfield_night_driveway", RoomsShared8017d878Table);
