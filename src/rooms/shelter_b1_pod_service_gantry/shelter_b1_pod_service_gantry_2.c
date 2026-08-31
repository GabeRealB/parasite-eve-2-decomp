#include "common.h"
#include "main/fs.h"
#include "gameplay/gameplay.h"
extern void D_shelter_b1_pod_service_gantry_8017FAF4;

s32 func_shelter_b1_pod_service_gantry_8017D80C(void)
{
    return 0;
}

s32 func_shelter_b1_pod_service_gantry_8017D814(void)
{
    return 0;
}

void func_shelter_b1_pod_service_gantry_8017D81C(Task* arg0)
{
    TaskIdMap* temp_v0;

    arg0->field_24 = &D_shelter_b1_pod_service_gantry_8017FAF4;
    Game_SetPtrSlot(arg0, 7);
    temp_v0     = Mem_Malloc(8U, false);
    arg0->idMap = temp_v0;
    if (temp_v0 == NULL) {
        Task_Kill(arg0);
        return;
    }
    Mem_Set(temp_v0, 0U, 8U);
    SetDispMask(0);
    arg0->state += 1;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017D89C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017D8F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017DF70);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017E400);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017E880);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017ED3C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017F160);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017F450);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017F8C8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry_2", func_shelter_b1_pod_service_gantry_8017FA7C);
