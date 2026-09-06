#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"
#include "rooms/rooms_shared_8017e890.h"

#include <psyq/libgte.h>

extern void Room_Draw09(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);

void RoomsShared8017e890(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->field_22++;
        if (arg0->state == 0) {
            mem->field_22 = 1;
            mem->field_24 = 0xE0;
            mem->field_26 = 0x80;
            mem->field_28 = 0xE0;
            mem->field_2A = 0x80;
            arg0->state   = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]        = mem->field_24;
        rgb[1]        = (u16)mem->field_24 >> 1;
        rgb[2]        = (u16)mem->field_24 >> 2;
        step          = mem->field_26 + 0x10;
        mem->field_26 = step;
        RoomsShared8017e4f8Halo(coord, step * 2, rgb);
        RoomsShared8017e890Draw(coord, mem->field_26);
        if (mem->field_28 >= 0x19) {
            rgb[0] = mem->field_28;
            rgb[1] = (u16)mem->field_28 >> 1;
            rgb[2] = (u16)mem->field_28 >> 2;
            Room_Draw09(coord, mem->field_2A * 3 / 2, 0x60, rgb);
            mem->field_28 -= 0x18;
            mem->field_2A += 0x30;
            return;
        }
        mem->field_24 -= 0x18;
        if (mem->field_24 < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}
