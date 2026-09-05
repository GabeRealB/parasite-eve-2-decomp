#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"

#include "rooms/rooms_shared_801807d4.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_5", func_shelter_b6_training_room_8017DAF8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_5", func_shelter_b6_training_room_8017DB28);

void func_shelter_b6_training_room_8017DB70(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
    SndEvt_EnqueueType7(0x80000000, 1);
}

void func_shelter_b6_training_room_8017DBB0(void)
{
    RoomsShared801807d4Task = NULL;
}
