#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern u32 Gp_LcgState;

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017D760);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017D850);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017DECC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017E334);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017E788);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017EC78);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017EEAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017F448);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8017F994);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8018016C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_801805A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80180898);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80180A4C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80180F10);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_8018101C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80181940);

void func_shelter_b2_pod_bottom_80181A48(Task* arg0)
{
    GsCOORDINATE2* coord;
    u32            rnd;

    if (Gp_State1C->eventState == 0) {
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        coord       = &((TmdObject*)arg0->extra)->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(0x600F4, coord, 0x8600, 0);
        rnd         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rnd;
        if (!((rnd >> 16) & 1)) {
            Gp_SpawnEff(0x600F4, coord, 0x8600, 0);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", func_shelter_b2_pod_bottom_80181B48);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_pod_bottom/shelter_b2_pod_bottom_2", D_shelter_b2_pod_bottom_8017D5EC);
