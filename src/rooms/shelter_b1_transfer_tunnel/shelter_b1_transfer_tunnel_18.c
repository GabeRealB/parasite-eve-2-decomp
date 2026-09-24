#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"
#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern RoomsShared8017e4f8Shade D_shelter_b1_transfer_tunnel_80182930[];

extern void func_shelter_b1_transfer_tunnel_8017E898(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
void        func_shelter_b1_transfer_tunnel_8017ECBC(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void        func_shelter_b1_transfer_tunnel_8017F594(GsCOORDINATE2* coord, s16 frame);
void        func_shelter_b1_transfer_tunnel_8017FE38(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// Expanding halo: state 0 anchors the effect's coordinate to its parent at
/// the spawn position and takes the palette row and duration from the spawn
/// argument; state 1 ramps the level up, drawing the halo, a half-bright echo
/// on odd ticks and a shrinking ring; state 2 fades the afterglow out and then
/// releases the work block.
void func_shelter_b1_transfer_tunnel_8017F050(Task* arg0)
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
                rgb[0]           = mem->scale >> D_shelter_b1_transfer_tunnel_80182930[mem->index].r;
                rgb[1]           = mem->scale >> D_shelter_b1_transfer_tunnel_80182930[mem->index].g;
                rgb[2]           = mem->scale >> D_shelter_b1_transfer_tunnel_80182930[mem->index].b;
                func_shelter_b1_transfer_tunnel_8017ECBC(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b1_transfer_tunnel_8017ECBC(coord, mem->angle + 0x100, rgb);
                }
                func_shelter_b1_transfer_tunnel_8017E898(coord, 0x300 - (u16)mem->angle * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b1_transfer_tunnel_80182930[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b1_transfer_tunnel_80182930[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b1_transfer_tunnel_80182930[mem->index].b;
                    func_shelter_b1_transfer_tunnel_8017FE38(coord, (u16)mem->angle * 4, rgb);
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

/// Twin-ring burst: seeds two levels on the first tick, then each tick draws
/// the halo and a second ring at the growing angle while a wider, dimmer ring
/// fades out behind them, releasing the work block once the main level runs
/// down (or the room's event state reaches 4).
void func_shelter_b1_transfer_tunnel_8017F3E8(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

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
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = (u16)mem->scale >> 1;
        rgb[2]     = (u16)mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b1_transfer_tunnel_8017ECBC(coord, step * 2, rgb);
        func_shelter_b1_transfer_tunnel_8017F594(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_shelter_b1_transfer_tunnel_8017E898(coord, mem->step * 3 / 2, 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}
