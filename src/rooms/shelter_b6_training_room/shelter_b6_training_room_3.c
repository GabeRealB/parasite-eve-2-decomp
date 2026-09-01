#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"

extern Task* D_shelter_b6_training_room_80185C5C;

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_3", func_shelter_b6_training_room_8017DAF8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_3", func_shelter_b6_training_room_8017DB28);

void func_shelter_b6_training_room_8017DB70(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
    SndEvt_EnqueueType7(0x80000000, 1);
}

void func_shelter_b6_training_room_8017DBB0(void)
{
    D_shelter_b6_training_room_80185C5C = NULL;
}
