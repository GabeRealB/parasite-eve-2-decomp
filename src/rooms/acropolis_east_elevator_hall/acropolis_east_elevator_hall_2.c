#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_east_elevator_hall.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

extern s32     D_acropolis_east_elevator_hall_8017D5E0;
extern SVECTOR D_acropolis_east_elevator_hall_8017D5E8;
extern s32     D_acropolis_east_elevator_hall_80185C8C;
extern s32     D_acropolis_east_elevator_hall_80185D54;
extern s32     D_acropolis_east_elevator_hall_801860B4;
extern s32     D_acropolis_east_elevator_hall_8018621C;
extern s32     D_acropolis_east_elevator_hall_801862F4;
extern s32     D_acropolis_east_elevator_hall_8018631C;

s32 func_acropolis_east_elevator_hall_8017F378(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0 && GameFlag_GetNibble(0) == 0 && D_acropolis_east_elevator_hall_8018631C == 0) {
        func_800E8634((s32)&D_acropolis_east_elevator_hall_80185D54, 0, (s32)&D_acropolis_east_elevator_hall_801860B4);
        D_acropolis_east_elevator_hall_8018631C = 1;
        GameFlag_SetNibble(0, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 3);
        GameFlag_SetNibble(8, 2);
        func_800E3FAC(0xA2, 2);
    }
    return 0;
}

s32 func_acropolis_east_elevator_hall_8017F420(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        func_800E8614((s32)&D_acropolis_east_elevator_hall_8018621C, 0);
    }
    return 0;
}

void func_acropolis_east_elevator_hall_8017F450(void)
{
    Gp_StartCapSlot(0x10, 1, 0);
}

void func_acropolis_east_elevator_hall_8017F478(Task* task)
{
    task->field_24 = &D_acropolis_east_elevator_hall_801862F4;
    Game_SetPtrSlot(task, 7);
    Gp_MsgSlot4Chain(0, 1);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_acropolis_east_elevator_hall_80185C8C, 0);
    task->state++;
}

void func_acropolis_east_elevator_hall_8017F4E8(void)
{
    if (Display_State.field_112 != 0) {
        func_807245E4(Game_GetPtrSlot(3));
        if (Display_State.field_112 != 0) {
            func_80724608(Game_GetPtrSlot(3), -0x8C, -0x32, &D_acropolis_east_elevator_hall_8017D5E0);
        }
    }
}
