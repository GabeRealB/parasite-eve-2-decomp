#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern s8 D_8007218A;
extern u8 D_80073BA9;

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131EC4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131FBC);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801320E0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132284);

INCLUDE_RODATA("actors/nonmatchings/actor_136100/actor_136100", D_actor_136100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801323F8);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132748);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132BC0);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132E78);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133238);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133558);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133690);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_8013379C);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133904);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133A88);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133BC8);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_801344AC);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80134588);

void func_actor_136100_8013467C(void)
{
    GpRec14 rec;
    s32     weaponId;
    s32     id;

    weaponId     = D_80073BA9;
    id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0  = id;
    rec.field_4  = 1;
    rec.field_8  = 0;
    rec.field_C  = 0;
    rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
}
