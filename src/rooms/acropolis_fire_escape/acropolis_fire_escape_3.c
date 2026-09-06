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
extern GpMsgEntry     D_acropolis_fire_escape_80181D3C[];
extern TaskDesc       D_acropolis_fire_escape_80181D64;
extern GpObj4A        D_acropolis_fire_escape_801826A8;
extern s32            D_acropolis_fire_escape_80183040;

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
