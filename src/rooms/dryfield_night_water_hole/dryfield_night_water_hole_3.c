#include "common.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_water_hole.h"
#include "rooms/rooms_shared_8017e690.h"

/// Applies the override list `func_dryfield_night_water_hole_8017D958` holds:
/// each entry replaces the room's parameter slot, both the `GpRoomParamRec`
/// pointer and the byte `Gp_LoadRoomParams` would have copied into
/// `Gp_RoomParams` out of it. The list ends at the first NULL record.
void func_dryfield_night_water_hole_8017DE88(DnwhParamOverride* list)
{
    GpAreaKey*       sess;
    s32              i;
    GpRoomParamRec** recs;

    sess = &gGameSession->at4.loc;
    for (i = 0; list[i].rec != 0; i++) {
        recs                         = Gp_RoomParamTables[sess->stage - 1][sess->area - 1];
        recs[list[i].index]          = list[i].rec;
        Gp_RoomParams[list[i].index] = recs[list[i].index]->field_3;
    }
}

void func_dryfield_night_water_hole_8017DF28(Task* task);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_water_hole/dryfield_night_water_hole_3", func_dryfield_night_water_hole_8017DF28);

void func_dryfield_night_water_hole_8017E630(Task* task)
{
    TaskFunc states[2] = { RoomsShared8017e690, func_dryfield_night_water_hole_8017DF28 };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}
