#include "common.h"

#include "gameplay/3688.h"
#include "rooms/shelter_b1_underground_parking.h"

extern u8 D_shelter_b1_underground_parking_8018D788;
extern u8 D_shelter_b1_underground_parking_8018D789;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_80184234);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_80184284);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_80184304);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_801843F0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_80184468);

void func_shelter_b1_underground_parking_80184594(Task* task)
{
    SbupActionPrompt* prompt = &D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->idMap;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screenX, prompt->screenY, work->promptKind);
    task->state = 4;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_801845F8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_801846EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_80184778);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_9", func_shelter_b1_underground_parking_801847D0);

void func_shelter_b1_underground_parking_801848A4(void)
{
    D_shelter_b1_underground_parking_8018D788 = 0xFF;
    D_shelter_b1_underground_parking_8018D789 = 0;
}
