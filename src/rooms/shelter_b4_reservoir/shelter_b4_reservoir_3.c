#include "common.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/rooms_shared_8017e690.h"

extern void func_shelter_b4_reservoir_8017E068(void);
void        func_shelter_b4_reservoir_8017E8E4(void);
void        func_shelter_b4_reservoir_8017E8EC(Task* task);

extern s16 D_shelter_b4_reservoir_80184F80;

void func_shelter_b4_reservoir_8017E8E4(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017E8EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017EA00);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017EE04);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017F23C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_3", func_shelter_b4_reservoir_8017F674);

void func_shelter_b4_reservoir_8017FADC(Task* task)
{
    TaskFunc states[2] = { RoomsShared8017e690, func_shelter_b4_reservoir_8017E8EC };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_reservoir_80184F80;
}
