#include "common.h"
#include "main/task.h"
#include "rooms/shelter_b1_underground_parking.h"

extern s32      D_shelter_b1_underground_parking_8018D758;
extern TaskDesc D_shelter_b1_underground_parking_8018726C[];

void func_shelter_b1_underground_parking_8018390C(void)
{
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        D_shelter_b1_underground_parking_8018D758 = 1;
        func_shelter_b1_underground_parking_80186890(1);
        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 5, 0, 0);
    }
}
