#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_pod_access_tunnel.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc                      D_shelter_b1_pod_access_tunnel_801810CC;
extern s32                           D_shelter_b1_pod_access_tunnel_801810D8;
extern TaskDesc                      D_shelter_b1_pod_access_tunnel_80181108;
extern TaskDesc                      D_shelter_b1_pod_access_tunnel_801811C8;
extern s32                           D_shelter_b1_pod_access_tunnel_80182FFC;
extern s32                           D_shelter_b1_pod_access_tunnel_8018380C;
extern RoomEventMsg                  D_shelter_b1_pod_access_tunnel_80184D04;
extern u8                            D_shelter_b1_pod_access_tunnel_80184D0C;
extern ShelterB1PodAccessTunnelEvent D_shelter_b1_pod_access_tunnel_80184D10;
extern u8                            D_801153F4;
extern s16                           D_80071076;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", func_shelter_b1_pod_access_tunnel_8017D61C);

static __inline__ s32 _shelterB1PodAccessTunnelStartEvent(RoomEventMsg* dst, ShelterB1PodAccessTunnelEvent* event)
{
    D_shelter_b1_pod_access_tunnel_80184D0C = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_pod_access_tunnel_80184D04 = *dst;
            D_shelter_b1_pod_access_tunnel_80184D10 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801810CC, 0, 0, 0);
            D_shelter_b1_pod_access_tunnel_80184D0C = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_b1_pod_access_tunnel_8017D7B4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    ShelterB1PodAccessTunnelEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x2F) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x7A) >= 6) {
            if (in->field_5 == 0) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80181108, 1, 0, 0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0xB3) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(3);
            }
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0xD1) == 0 && GameFlag_GetNibble(0x83) == 0) {
            Gp_FillAllyHp();
            GameFlag_SetNibble(0xD1, 1);
            GameFlag_SetNibble(0x4C, 7);
        }
    }
    if (in->msgId == 0x10) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0xD1) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(4);
            }
            return 2;
        }
        event.field_0 = 6;
        event.field_4 = 0x54110001;
        event.field_8 = 0x130;
        event.field_A = 0;
        return _shelterB1PodAccessTunnelStartEvent(out, &event);
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", D_shelter_b1_pod_access_tunnel_8017D5D8);

void func_shelter_b1_pod_access_tunnel_8017DA74(Task* task)
{
    s32 var_v0;
    s32 room;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(GameFlag_GetNibble(0xFC) != 0 ? 5 : 1);
            D_801153F4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                if (Gp_GetCapEventKey() == 1) {
                    GameFlag_SetNibble(0x1B6, 2);
                }
                D_801153F4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            GameFlag_SetNibble(0x1B6, 0);
            SndEvt_EnqueueType6(0x54110006, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54110006);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x23;
            Mc_SaveData.at4.loc.warp = 3;
            Mc_SaveData.at4.loc.room = 1;
            room                     = GameFlag_GetNibble(0x118);
            if (room == 2) {
                Mc_SaveData.at4.loc.room = room;
            }
            D_80071076 = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

void func_shelter_b1_pod_access_tunnel_8017DC18(Task* task)
{
    s32 var_v0;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(9);
            D_801153F4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                D_801153F4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            SndEvt_EnqueueType6(0x54110004, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54110004);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            GameFlag_SetNibble(0xB4, 1);
            GameFlag_SetNibble(0x1C1, 0);
            Mc_SaveData.sceneEvent   = 0x1C;
            Mc_SaveData.at4.loc.area = 0x17;
            Mc_SaveData.at4.loc.warp = 1;
            Mc_SaveData.at4.loc.room = 1;
            D_80071076               = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b1_pod_access_tunnel_8017DD68(void)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12F) != 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80181108, 0, 0, 0);
        } else {
            GameFlag_SetNibble(0x12F, 1);
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDD8(void)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDE0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 6) {
        SndEvt_EnqueueType6(0x16, 0, 0);
    }
    return 0;
}

void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0)
{
    arg0->msgTable = &D_shelter_b1_pod_access_tunnel_801810D8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x118) == 1) {
        Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801811C8, 0, 0, 0);
        GameFlag_SetNibble(0x118, 2);
        func_800E3FAC(0xA2, 0x37);
    } else if (GameFlag_GetNibble(0x7E) == 0) {
        func_800E8634((s32)&D_shelter_b1_pod_access_tunnel_80182FFC, 0, (s32)&D_shelter_b1_pod_access_tunnel_8018380C);
        func_800E3FAC(0xA2, 0x1E);
        GameFlag_SetNibble(0x7E, 1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel", D_shelter_b1_pod_access_tunnel_8017D610);
