#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ff88.h"

extern void func_mine_cavern_8017F7D0(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern RoomsShared8017ff88Shade D_mine_cavern_80188FCC[];
extern void                     func_mine_cavern_8017FBF4(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
extern void                     func_mine_cavern_80180D70(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// Frame callback of an expanding halo. State 0 parks the coordinate on its
/// parent at the spawn position and derives the ramp step from the spawn
/// argument; state 1 grows the halo's brightness and size, drawing the
/// `func_mine_cavern_8017FBF4` wedge ring (with a half-bright echo on odd
/// ticks) and a `func_mine_cavern_8017F7D0` ring; state 2 fades it out through
/// `func_mine_cavern_80180D70` and then releases the work block. The shade row
/// `D_mine_cavern_80188FCC[index]` tints each channel.
void func_mine_cavern_8017FF88(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s16            flag;
    s32            shift;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[0] = mem->pos.vx;
                coord->coord.t[1] = mem->pos.vy;
                coord->coord.t[2] = mem->pos.vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index      = shift;
                arg0->spawnArg1 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
                arg0->state     = 1;
                mem->step       = 0x100 / arg0->spawnArg1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->scale >> D_mine_cavern_80188FCC[mem->index].r;
                rgb[1]           = mem->scale >> D_mine_cavern_80188FCC[mem->index].g;
                rgb[2]           = mem->scale >> D_mine_cavern_80188FCC[mem->index].b;
                func_mine_cavern_8017FBF4(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_mine_cavern_8017FBF4(coord, (s16)((u16)mem->angle + 0x100), rgb);
                }
                func_mine_cavern_8017F7D0(coord, 0x300 - (u16)mem->angle * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_mine_cavern_80188FCC[mem->index].r;
                    rgb[1] = mem->scale >> D_mine_cavern_80188FCC[mem->index].g;
                    rgb[2] = mem->scale >> D_mine_cavern_80188FCC[mem->index].b;
                    func_mine_cavern_80180D70(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
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
