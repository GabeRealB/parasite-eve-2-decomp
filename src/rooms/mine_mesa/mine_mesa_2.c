#include "common.h"

#include "main/task.h"
#include "gameplay/3A34.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/mine_mesa.h"

extern s32        func_80179A04(GpSaveLoc* in, GpSaveLoc* out);
extern TaskDesc   D_mine_mesa_801818F8;
extern TaskDesc   D_mine_mesa_80189B2C;
extern Task*      D_mine_mesa_80189B4C;
extern s32        D_mine_mesa_80189B50;
extern GpObj4A    D_mine_mesa_801890EC[4];
extern GpMsgEntry D_mine_mesa_80181904[];
extern TaskDesc   D_mine_mesa_80181990;
extern s16        D_80072830;

extern void func_800E8614(s32 arg0, s32 arg1);
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32  D_mine_mesa_80184D9C;
extern s32  D_mine_mesa_80184FF4;
extern s32  D_mine_mesa_801850E4;
extern s32  D_mine_mesa_801854BC;
extern s32  D_mine_mesa_801856B4;

void func_mine_mesa_8017DD44(void);
void func_mine_mesa_8017EB38(void);
void func_mine_mesa_801817BC(void);

static __inline__ s32 MineMesa_StartEvent(GpSaveLoc* dst, MineMesaEvent* event)
{
    D_mine_mesa_80189B48 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_mine_mesa_80189B40 = *dst;
            D_mine_mesa_80189B60 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_mine_mesa_801818F8, 0, 0, 0);
            D_mine_mesa_80189B48 = 1;
        }
        return 2;
    }
    return 1;
}

/// Handler id 0x13EE of this room's copy of the `GpMsgEntry` table
/// `D_mine_mesa_80181904`: copies the requested location to `dst` and forwards
/// both to `func_80179A04`. A stage-3 request latches the outgoing location and
/// the event parameters below into the room's pending event and starts the
/// controller task; `field_5` set only suppresses that side effect. Answers 0
/// without side effects while the request is already in flight (`field_9` is 1
/// and `Gp_StateF0.field_0` agrees with it), 2 for a stage-3 request and 1 for
/// every other one.
s32 func_mine_mesa_8017D8F8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    MineMesaEvent event;
    u8            field9;

    *out = *in;
    func_80179A04(in, out);
    if (*(u16*)in != 3) {
        return 1;
    }
    field9 = Game_Session->field_9;
    if (field9 == 1 && Gp_StateF0.field_0 == field9) {
        return 0;
    }
    event.field_0 = 0xE;
    event.field_4 = 0x54010001;
    event.field_8 = 0x171;
    event.field_A = 0;
    return MineMesa_StartEvent(out, &event);
}

s32 func_mine_mesa_8017DA7C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xD) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x11A) >= 2 ? 0xD : 0xC);
    }
    return 0;
}

s32 func_mine_mesa_8017DABC(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    switch (msg->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x71) == 0) {
                if (Game_GetPtrSlot(0xA) != NULL) {
                    func_800E8614((s32)&D_mine_mesa_801850E4, 0);
                }
                func_800E3FAC(0xA2, 0x1C);
                GameFlag_SetNibble(0x71, 1);
                func_mine_mesa_8017DD44();
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0x71) <= 0) {
                if (GameFlag_GetNibble(0x91) == 0) {
                    if (Game_GetPtrSlot(0xA) != NULL) {
                        func_800E8634((s32)&D_mine_mesa_80184D9C, 1, (s32)&D_mine_mesa_80184FF4);
                    }
                    GameFlag_SetNibble(0x91, 1);
                } else if (Game_GetPtrSlot(0xA) != NULL) {
                    func_800E8634((s32)&D_mine_mesa_801854BC, 1, (s32)&D_mine_mesa_801856B4);
                }
            }
            break;
    }
    return 0;
}

s32 func_mine_mesa_8017DBC4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8 field9;

    field9 = Game_Session->field_9;
    if (field9 == 1) {
        if (GameFlag_GetNibble(0xCD) == 0) {
            if (Game_GetPtrSlot(0xA) != NULL) {
                Gp_StateC08.field_6 |= 1;
                Gp_PulseState1C();
                D_mine_mesa_80189B50 = field9;
                GameFlag_SetNibble(0xCD, 1);
            }
        } else if (D_mine_mesa_80189B4C != NULL) {
            Gp_DispatchMsg(D_mine_mesa_80189B4C, 0x13F4, arg2, arg3);
        }
    }
    return 0;
}

void func_mine_mesa_8017DC80(Task* arg0)
{
    arg0->field_24 = D_mine_mesa_80181904;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x90) == 0) {
        if (Game_GetPtrSlot(0xA) != NULL) {
            D_80072830 = 5;
            Task_SpawnFromTable(&D_mine_mesa_80181990, 0, 0, 0);
        }
        GameFlag_SetNibble(0x1BD, 0);
    } else {
        func_mine_mesa_8017DD44();
    }
    D_80062735 = 1;
    func_mine_mesa_8017EB38();
    D_mine_mesa_80189B4C = NULL;
    func_mine_mesa_801817BC();
    arg0->state          = arg0->state + 1;
    D_mine_mesa_80189B50 = 0;
}

void func_mine_mesa_8017DD44(void)
{
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[0]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[1]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[2]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[3]);
}
