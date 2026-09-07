#include "common.h"
#include "rooms/room_common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"

void func_shelter_b1_control_room_access_tunnel_801815D0(GsCOORDINATE2*, s16);

void func_shelter_b1_control_room_access_tunnel_80181424(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    u8             sp10[3];
    u16            temp;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        if (task->state == 0) {
            work->field_22 = 1;
            work->field_24 = 0xE0;
            work->field_26 = 0x80;
            work->field_28 = 0xE0;
            work->field_2A = 0x80;
            task->state    = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]        = (u8)work->field_24;
        sp10[1]        = (u8)(work->field_24 >> 1);
        sp10[2]        = (u8)(work->field_24 >> 2);
        temp           = work->field_26 + 0x10;
        work->field_26 = temp;
        Room_Draw10(coord, (s16)(temp * 2), sp10);
        func_shelter_b1_control_room_access_tunnel_801815D0(coord, (s16)work->field_26);
        if ((s16)work->field_28 >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->field_28;
            sp10[1] = (u8)(work->field_28 >> 1);
            sp10[2] = (u8)(work->field_28 >> 2);
            temp_a1 = (s16)work->field_2A * 3;
            Room_Draw07(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->field_28 -= 0x18;
            work->field_2A += 0x30;
            return;
        }
        temp           = work->field_24 - 0x18;
        work->field_24 = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room_access_tunnel/shelter_b1_control_room_access_tunnel_5", func_shelter_b1_control_room_access_tunnel_801815D0);
