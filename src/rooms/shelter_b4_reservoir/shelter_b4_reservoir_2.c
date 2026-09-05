#include "common.h"

extern void func_shelter_b4_reservoir_8017E068(void);
void        func_shelter_b4_reservoir_8017E8E4(void);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_2", func_shelter_b4_reservoir_8017E7C8);

void func_shelter_b4_reservoir_8017E864(void)
{
    func_shelter_b4_reservoir_8017E068();
    func_shelter_b4_reservoir_8017E8E4();
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir_2", jtbl_shelter_b4_reservoir_8017D604);
