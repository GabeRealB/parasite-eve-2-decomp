#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Spawn payload handed to `RoomsShared80181228Desc` as
/// `Task_SpawnFromTable` arg3 by the day-13 branch of
/// `func_shelter_b1_underground_parking_80182A60`. `field_0` is the script id
/// and `field_4`..`field_10` are the four cap keys it replays.
typedef struct {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ byte unknown_14[0xC];
} ShelterParkingSpawnArg;

extern s32                    D_shelter_b1_underground_parking_8018D758;
extern ShelterParkingSpawnArg D_shelter_b1_underground_parking_8018D75C;
extern TaskDesc               RoomsShared80181228Desc[];
extern TaskDesc               D_shelter_b1_underground_parking_80187260[];
extern TaskDesc               D_shelter_b1_underground_parking_8018726C[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_6", func_shelter_b1_underground_parking_80183284);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_6", func_shelter_b1_underground_parking_80183360);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_6", func_shelter_b1_underground_parking_801833DC);
