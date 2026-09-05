#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"
#include "rooms/acropolis_cafeteria.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern GsCOORDINATE2 Gfx_ViewCoord;

void func_acropolis_cafeteria_80181E3C(Task* arg0);

extern SVECTOR RoomsShared8017d830Delta;

extern MATRIX D_acropolis_cafeteria_8018D5A0;
extern MATRIX D_acropolis_cafeteria_8018D5C0;
extern MATRIX D_acropolis_cafeteria_8018D5E0;
extern MATRIX D_acropolis_cafeteria_8018D600;
extern MATRIX D_acropolis_cafeteria_8018D620;
extern MATRIX D_acropolis_cafeteria_8018D640;

void func_acropolis_cafeteria_801818DC(Task* task)
{
    TmdObject*                obj;
    GsCOORDINATE2*            coord;
    AcropolisCafeteriaDebris* work;
    GsCOORDINATE2*            player;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0xD8, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap        = (TaskIdMap*)work;
    task->exitCallback = func_acropolis_cafeteria_80181E3C;
    task->state        = task->state + 1;
    Mem_Set(work, 0, 0xD8);
    coord->sub   = &Gfx_ViewCoord;
    coord->flg   = 0;
    obj->field_C = 0;
    RotMatrix(&work->field_C4, &coord->coord);
    work->field_B0     = (rand() & 0xFFF) + 0x3000;
    player             = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    coord->coord.t[0]  = player->coord.t[0];
    coord->coord.t[1]  = player->coord.t[1] - 0x800;
    coord->coord.t[2]  = player->coord.t[2] + 0x800;
    work->obj.field_C  = work->slots;
    work->obj.field_18 = 0x50000;
    work->obj.field_1C = 0xFA;
    work->obj.field_8  = coord;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.flags    = 1;
    Gp_LinkObj(4, &work->obj);
    Gp_InitRec18Table(work->obj.field_C, 6, 0);
    work->obj.flags |= 0x8000;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_8", func_acropolis_cafeteria_80181A3C);

void func_acropolis_cafeteria_80181E30(Task* arg0)
{
    arg0->state = 3;
}

void func_acropolis_cafeteria_80181E3C(Task* arg0)
{
    Gp_UnlinkObj(arg0->idMap);
    Task_Kill(arg0);
}

extern TaskFuncTable4 D_acropolis_cafeteria_8017D69C;

/// Per-frame entry point: runs the task's current state. The table is a local,
/// so GCC copies it from `.rodata` onto the stack every frame.
void func_acropolis_cafeteria_80181E70(Task* task)
{
    TaskFuncTable4 states;

    states = D_acropolis_cafeteria_8017D69C;
    states.funcs[task->state](task);
}
