#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1A8.h"
#include "gameplay/gameplay.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "rooms/acropolis_fire_escape.h"

extern TaskFuncTable3 RoomsShared8017d878Table;
extern s8             D_8007272D;
extern TaskDesc       D_acropolis_fire_escape_80181D18;
extern GpMsgEntry     D_acropolis_fire_escape_80181D3C[];
extern TaskDesc       D_acropolis_fire_escape_80181D64;
extern GpObj4A        D_acropolis_fire_escape_801826A8;
extern s32            D_acropolis_fire_escape_80183040;

s32 func_acropolis_fire_escape_8017F9F8(Task* task, s32 msgId, s32 event, s32 arg3)
{
    Task* slot;
    s32   cap;
    s32   result;

    if (event == 4) {
        if (GameFlag_GetNibble(0x16A) == 0) {
            GameFlag_SetNibble(0x16A, 1);
            Gp_RunCapCmd1(0xB);
            return 0;
        }
        D_acropolis_fire_escape_80183048.field_0  = 9;
        D_acropolis_fire_escape_80183048.field_1  = 1;
        D_acropolis_fire_escape_80183048.field_3  = 1;
        D_acropolis_fire_escape_80183048.field_2  = 0;
        D_acropolis_fire_escape_80183048.field_4  = 0x510F0001;
        D_acropolis_fire_escape_80183048.field_8  = 0x510F0004;
        D_acropolis_fire_escape_80183048.field_10 = 0x510F0007;
        D_acropolis_fire_escape_80183048.field_C  = 0x510F0008;
        Task_SpawnFromTable(&D_acropolis_fire_escape_80181D18, 0, 3, (s32)&D_acropolis_fire_escape_80183048);
    }
    if (event == 3) {
        if (GameFlag_GetNibble(0x155) < 6) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
        }
        Gp_SpawnIfCapIdle(3, 1);
        func_800E3FAC(0xA2, 7);
    }
    if (event == 1) {
        slot = (Task*)Gp_LookupSlot4(0);
        cap  = 1;
        if (slot != NULL) {
            result = Gp_DispatchMsg(slot, 0x7D6, 0, 0);
            cap    = 9;
            if (result == 0) {
                cap = 1;
            }
        }
        Gp_SpawnIfCapIdle(cap, 1);
    }
    return 0;
}

void func_acropolis_fire_escape_8017FB40(Task* task)
{
    GpSndParam* pair;
    s32         vol;
    s32         prev;

    switch (task->state) {
        case 0:
            D_acropolis_fire_escape_80183040 = 0;
            task->state                      = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    switch ((u8)Game_Session->field_4) {
        case 8:
            vol = 0x64;
            if (Mc_SaveData.field_5C5 == 5) {
                Mc_SaveData.field_5C5 = 7;
                pair                  = (GpSndParam*)&D_8007A39C;
                pair->field_0         = 1;
                pair->field_2         = 1;
                Task_SpawnFromTable(&D_80062774, 0, 0, 0);
                Game_Session->field_69 = 0;
            }
            break;
        case 2:
        case 3:
            vol = 0x1E;
            break;
        case 4:
        case 5:
            vol = 0xF;
            break;
        default:
            vol = 0;
            break;
    }

    prev = D_acropolis_fire_escape_80183040;
    if (vol == prev) {
        return;
    }
    if (prev == 0) {
        SndEvt_EnqueueType6(0x510F0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x510F0005, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x510F0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    }
    D_acropolis_fire_escape_80183040 = vol;
}
