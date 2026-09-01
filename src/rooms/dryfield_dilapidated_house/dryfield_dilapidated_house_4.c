#include "common.h"

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

void func_dryfield_dilapidated_house_801815E8(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_dilapidated_house_8018142C(Task* task);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80180FD8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181028);

void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src)
{
    if (!(src->field_C & 0x80)) {
        dst->field_C &= ~0x80;
        return;
    }
    dst->field_C |= 0x80;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181134);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_8018118C);

void func_dryfield_dilapidated_house_80181264(Task* arg0)
{
    func_dryfield_dilapidated_house_8017EBB8(arg0);
    func_dryfield_dilapidated_house_8017EE58(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181290);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801812E8);

void func_dryfield_dilapidated_house_80181340(Task* arg0)
{
    GsCOORDINATE2* coord;
    void*          work;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Malloc(4, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = work;
    coord->sub  = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->exitCallback = func_dryfield_dilapidated_house_8018142C;
    arg0->state       += 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801813DC);

void func_dryfield_dilapidated_house_8018142C(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord->sub = &Gfx_ViewCoord;
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_8018145C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801814B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181584);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801815B8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801815E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181F08);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801823B8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80182744);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80182A18);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80182F14);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801832A8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183728);

void func_dryfield_dilapidated_house_80183BF8(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    mask  = 1 << (u8)Game_Session->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (mask & 0x84A9C) {
        func_dryfield_dilapidated_house_801815E8(coord, 0);
    }
    if (mask & 0x104B98) {
        func_dryfield_dilapidated_house_801815E8(coord, 8);
    }
    if (mask & 0xA55F8) {
        func_dryfield_dilapidated_house_801815E8(coord, 0x10);
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183C8C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183D5C);
