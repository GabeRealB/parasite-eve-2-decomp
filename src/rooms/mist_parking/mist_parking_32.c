#include "common.h"

#include "main/task.h"
#include "rooms/mist_parking.h"

/// Spawns entry 4 of `D_mist_parking_8018D75C` and keeps its handle in
/// `D_mist_parking_80195324`.
void func_mist_parking_80183600(void)
{
    D_mist_parking_80195324 = Task_SpawnFromTable(&D_mist_parking_8018D75C, 4, 0, 0);
}
