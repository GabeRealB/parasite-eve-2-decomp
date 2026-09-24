#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_1f_bulwark.h"

extern s8  D_801153F4;
extern u8  D_80115690;
extern s16 D_80071076;

extern GpStateBD8 D_shelter_1f_bulwark_80180EBC;

/// The room's event task, spawned by its message handler for a latched event.
/// State 0 runs the event's CAP command; state 1 waits for it to finish and,
/// when the event asks for it, starts helper task 0x31; states 2 and 3 play
/// the event's stage sound, if any, and wait for it to end; state 4 moves the
/// save location to the latched message's area, warp and room and hands over
/// to task type 0x11.
void func_shelter_1f_bulwark_8017D61C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_bulwark_80180ED0.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_bulwark_80180ED0.field_A != 0) {
                    D_shelter_1f_bulwark_80180EBC.field_0 = 0;
                    D_shelter_1f_bulwark_80180EBC.field_1 = 0;
                    D_shelter_1f_bulwark_80180EBC.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_bulwark_80180EBC);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_bulwark_80180ED0.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_bulwark_80180ED0.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_bulwark_80180ED0.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_1f_bulwark_80180EC4.field_0;
            Mc_SaveData.at4.loc.warp = D_shelter_1f_bulwark_80180EC4.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_1f_bulwark_80180EC4.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 Bulwark_StartEvent(GpSaveLoc* dst, BulwarkEvent* event)
{
    D_shelter_1f_bulwark_80180ECC = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_bulwark_80180EC4 = *dst;
            D_shelter_1f_bulwark_80180ED0 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_bulwark_80180320, 0, 0, 0);
            D_shelter_1f_bulwark_80180ECC = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_bulwark_8017D7B4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    BulwarkEvent event;

    *dst = *src;
    func_80179B14(src, dst);
    if (*(u16*)src == 4) {
        if (GameFlag_GetNibble(0x15D) == 0) {
            Gp_SpawnIfCapIdle(1, 0);
            return 2;
        }
        if (GameFlag_GetNibble(0x7A) < 6) {
            if (src->field_5 == 0) {
                GameFlag_SetNibble(0x7A, 6);
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_1f_bulwark_80180354, 0, 0, 0);
            }
            return 0;
        }
        event.field_0 = 1;
        event.field_4 = 0x55030003;
        event.field_8 = 0;
        event.field_A = 1;
        return Bulwark_StartEvent(dst, &event);
    }
    if (*(u16*)src == 2) {
        event.field_0 = 6;
        event.field_4 = 0x55030001;
        event.field_8 = 0x15C;
        event.field_A = 0;
        return Bulwark_StartEvent(dst, &event);
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", D_shelter_1f_bulwark_8017D5D8);

void func_shelter_1f_bulwark_8017DA60(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(1, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_TriggerPeIfArmed();
            goto advance;
        case 2:
            D_shelter_1f_bulwark_80180EC0.field_0 = 0;
            D_shelter_1f_bulwark_80180EC0.field_1 = 0;
            D_shelter_1f_bulwark_80180EC0.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_bulwark_80180EC0);
            arg0->killCountdown = 0;
            SndEvt_EnqueueType6(0x55030003, 0, 0);
            goto advance;
        case 3:
            arg0->killCountdown++;
            if (arg0->killCountdown < 0x1F) {
                break;
            }
            goto advance;
        case 5:
            GameFlag_SetNibble(0x7A, 6);
            Task_SpawnFromTable(&D_shelter_1f_bulwark_80180360, 0, 0, 0);
        case 4:
        case 6:
        advance:
            arg0->state++;
            break;
        case 7:
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_1f_bulwark_8017DBBC(void)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBC4(void)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBCC(void)
{
    return 0;
}
