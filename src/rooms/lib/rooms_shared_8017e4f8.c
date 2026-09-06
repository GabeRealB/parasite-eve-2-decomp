#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

extern void Room_Draw09(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);

#include <psyq/libgs.h>
#include <psyq/libgte.h>

void RoomsShared8017e4f8(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s16            flag;
    s32            shift;

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
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->field_8;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[0] = mem->field_18;
                coord->coord.t[1] = mem->field_1A;
                coord->coord.t[2] = mem->field_1C;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->field_20   = shift;
                arg0->spawnArg1 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
                arg0->state     = 1;
                mem->field_2A   = 0x100 / arg0->spawnArg1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->field_24   += mem->field_2A;
                mem->field_26   += mem->field_2A;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->field_24 >> RoomsShared8017e4f8Shades[mem->field_20].r;
                rgb[1]           = mem->field_24 >> RoomsShared8017e4f8Shades[mem->field_20].g;
                rgb[2]           = mem->field_24 >> RoomsShared8017e4f8Shades[mem->field_20].b;
                RoomsShared8017e4f8Halo(coord, mem->field_26, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->field_22 & 1) {
                    RoomsShared8017e4f8Halo(coord, mem->field_26 + 0x100, rgb);
                }
                Room_Draw09(coord, 0x300 - (u16)mem->field_26 * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->field_24 = 0xFF;
                    arg0->state   = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->field_24 >= 0x11) {
                    rgb[0] = mem->field_24 >> RoomsShared8017e4f8Shades[mem->field_20].r;
                    rgb[1] = mem->field_24 >> RoomsShared8017e4f8Shades[mem->field_20].g;
                    rgb[2] = mem->field_24 >> RoomsShared8017e4f8Shades[mem->field_20].b;
                    RoomsShared8017e4f8Fade(coord, (u16)mem->field_26 * 4, rgb);
                    mem->field_24 -= 0x10;
                    mem->field_26 += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}
