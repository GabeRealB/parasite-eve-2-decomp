#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

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

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

s32 func_shelter_b1_underground_parking_80183284(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x121 || arg2 == 0x122) {
        if (gGameSession->at4.loc.room == 6) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_underground_parking_8018726C, 2, 0, 0);
                gGameSession->hideHud    = 1;
                gGameSession->eventState = 1;
                return 1;
            }
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80183360(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        return 1;
    }
    if (in->field_5 == 0) {
        Gp_RunCapCmd1(0x1E);
    }
    return 2;
}

s32 func_shelter_b1_underground_parking_801833DC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54140010, 0, 0);
    }
    return 0;
}
