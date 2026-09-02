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

extern TaskFuncTable3 D_acropolis_fire_escape_8017D6A4;
extern s8             D_8007272D;
extern GpMsgEntry     D_acropolis_fire_escape_80181D3C[];
extern TaskDesc       D_acropolis_fire_escape_80181D64;
extern GpObj4A        D_acropolis_fire_escape_801826A8;
extern s32            D_acropolis_fire_escape_80183040;

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_5", func_acropolis_fire_escape_8017F9F8);

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
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_5", func_acropolis_fire_escape_8017FD08);

s32 func_acropolis_fire_escape_8017FD98(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (src->field_5 == 0) {
        SndEvt_EnqueueType7(0x510F0005, 0xF);
    }
    if (*(u16*)src == 0xE && src->field_5 == 0) {
        if (GameFlag_GetNibble(2) == 3) {
            dst->field_3 = 2;
        } else {
            dst->field_3 = 1;
        }
    }
    return 1;
}
s32 func_acropolis_fire_escape_8017FE40(void)
{
    return 0;
}

s32 func_acropolis_fire_escape_8017FE48(void)
{
    return 0;
}

void func_acropolis_fire_escape_8017FE50(Task* task)
{
    task->field_24 = D_acropolis_fire_escape_80181D3C;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(&D_acropolis_fire_escape_80181D64, 0, 0, 0);
    if (D_8007272D == 5) {
        Game_Session->field_69 = 8;
    }
    task->state = task->state + 1;
}
void func_acropolis_fire_escape_8017FECC(void)
{
    Task* slot;

    slot = (Task*)Gp_LookupSlot4(0);
    if (slot == NULL || Gp_DispatchMsg(slot, 0x7D6, 0, 0) == 0) {
        D_acropolis_fire_escape_801826A8.field_4A &= 0xBF;
    }
}
void func_acropolis_fire_escape_8017FF24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fire_escape_8017D6A4;
    sp.funcs[task->state](task);
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_5", func_acropolis_fire_escape_8017FF7C);
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_5", func_acropolis_fire_escape_80180154);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_5", func_acropolis_fire_escape_80180B20);
