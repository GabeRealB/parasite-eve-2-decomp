#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/shelter_b6_nursery.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80181314);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80181820);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80181EDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80182330);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80182730);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_801829E4);

void func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1)
{
    D_shelter_b6_nursery_801879F0.field_0 = arg0;
    D_shelter_b6_nursery_801879F0.field_2 = arg1;
}
