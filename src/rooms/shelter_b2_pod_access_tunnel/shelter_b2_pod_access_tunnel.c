#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_pod_access_tunnel.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc                      D_shelter_b2_pod_access_tunnel_80183BC0;
extern GpMsgEntry                    D_shelter_b2_pod_access_tunnel_80183BCC[];
extern TaskDesc                      D_shelter_b2_pod_access_tunnel_80183BFC;
extern GpStateBD8                    D_shelter_b2_pod_access_tunnel_801856F8;
extern RoomEventMsg                  D_shelter_b2_pod_access_tunnel_80185700;
extern u8                            D_shelter_b2_pod_access_tunnel_80185708;
extern ShelterB2PodAccessTunnelEvent D_shelter_b2_pod_access_tunnel_8018570C;
extern u8                            D_801153F4;
extern u8                            D_80115690;
extern s16                           D_80071076;

/// Runs the room's pending event once its request has been accepted. State 0
/// runs the event's CAP command; state 1 waits for it and, when the event asks
/// for one, spawns helper task 0x31; state 2 queues the event's stage sound,
/// if any, and state 3 waits for that voice to end. State 4 commits the event:
/// it queues a type-7 sound event, copies the destination recorded in
/// `D_shelter_b2_pod_access_tunnel_80185700` into the save location and spawns
/// the room-load task 0x11.
void func_shelter_b2_pod_access_tunnel_8017D62C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_pod_access_tunnel_8018570C.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_pod_access_tunnel_8018570C.field_A != 0) {
                    D_shelter_b2_pod_access_tunnel_801856F8.field_0 = 0;
                    D_shelter_b2_pod_access_tunnel_801856F8.field_1 = 0;
                    D_shelter_b2_pod_access_tunnel_801856F8.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_pod_access_tunnel_801856F8);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_pod_access_tunnel_8018570C.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_pod_access_tunnel_8018570C.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_pod_access_tunnel_8018570C.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_pod_access_tunnel_80185700.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_pod_access_tunnel_80185700.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b2_pod_access_tunnel_80185700.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 _shelterB2PodAccessTunnelStartEvent(RoomEventMsg* dst, ShelterB2PodAccessTunnelEvent* event)
{
    D_shelter_b2_pod_access_tunnel_80185708 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_pod_access_tunnel_80185700 = *dst;
            D_shelter_b2_pod_access_tunnel_8018570C = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BC0, 0, 0, 0);
            D_shelter_b2_pod_access_tunnel_80185708 = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_b2_pod_access_tunnel_8017D7C4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    ShelterB2PodAccessTunnelEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x30) {
        if (GameFlag_GetNibble(0xB4) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(GameFlag_GetNibble(0x7A) < 6 ? 2 : 6);
            }
            return 0;
        }
    }
    if (in->msgId == 0x22) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(4);
            }
            return 2;
        }
        event.field_0 = 5;
        event.field_4 = 0x54230001;
        event.field_8 = 0x132;
        event.field_A = 0;
        return _shelterB2PodAccessTunnelStartEvent(out, &event);
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_access_tunnel/shelter_b2_pod_access_tunnel", D_shelter_b2_pod_access_tunnel_8017D5D8);

void func_shelter_b2_pod_access_tunnel_8017D9A8(Task* task)
{
    s32 var_v0;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(GameFlag_GetNibble(0xFC) != 0 ? 3 : 1);
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
            SndEvt_EnqueueType6(0x54230004, 0, 0);
            GameFlag_SetNibble(0x1B6, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54230004);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x11;
            Mc_SaveData.at4.loc.warp = 3;
            Mc_SaveData.at4.loc.room = 1;
            D_80071076               = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b2_pod_access_tunnel_8017DB28(void)
{
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB30(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BFC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB70(void)
{
    return 0;
}

s32 func_shelter_b2_pod_access_tunnel_8017DB78(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        SndEvt_EnqueueType6(0x16, 0, 0);
    }
    return 0;
}

void func_shelter_b2_pod_access_tunnel_8017DBA8(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_pod_access_tunnel_80183BCC;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 0x16) {
        gGameSession->flowFlags = 3;
    }
    arg0->state = arg0->state + 1;
}

void func_shelter_b2_pod_access_tunnel_8017DC0C(void)
{
}
