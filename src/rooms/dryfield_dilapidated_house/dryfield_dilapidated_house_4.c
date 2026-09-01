#include "common.h"

#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"

void func_dryfield_dilapidated_house_801815E8(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_dilapidated_house_8018142C(Task* task);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80180FD8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181028);

void func_dryfield_dilapidated_house_801810F8(GameActorExt* dst, GameActorExt* src)
{
    if (!(src->field_C & 0x80)) {
        dst->field_C &= ~0x80;
        return;
    }
    dst->field_C |= 0x80;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181134);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_8018118C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181264);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181290);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801812E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181340);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801813DC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_8018142C);

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183BF8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183C8C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183D5C);
