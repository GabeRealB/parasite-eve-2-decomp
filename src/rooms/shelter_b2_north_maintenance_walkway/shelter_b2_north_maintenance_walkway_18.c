#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Per-palette channel shifts for the halo, indexed by the palette the spawn
/// argument selects.
extern RoomsShared8017e4f8Shade D_shelter_b2_north_maintenance_walkway_80183C38[];

extern void func_shelter_b2_north_maintenance_walkway_8017FB20(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
void        func_shelter_b2_north_maintenance_walkway_8017FF44(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void        func_shelter_b2_north_maintenance_walkway_8018081C(GsCOORDINATE2* coord, s16 frame);
void        func_shelter_b2_north_maintenance_walkway_801810C0(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// An expanding halo. The first tick parents the effect frame to its anchor
/// at the spawn position and splits the spawn argument into a palette index
/// and a frame count. While the count runs down, the level and the angle grow
/// by 0x100 / count each tick, drawing the halo (plus a half-bright echo on odd
/// ticks) tinted by the palette and a black-edged ring shrinking in from 0x300.
/// It then fades from full level through the afterglow, 0x10 a tick, and
/// releases its work block. It pauses while the room's event state is set and
/// releases the block when that state reaches 4.
void func_shelter_b2_north_maintenance_walkway_801802D8(Task* arg0)
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
                rgb[0]           = mem->scale >> D_shelter_b2_north_maintenance_walkway_80183C38[mem->index].r;
                rgb[1]           = mem->scale >> D_shelter_b2_north_maintenance_walkway_80183C38[mem->index].g;
                rgb[2]           = mem->scale >> D_shelter_b2_north_maintenance_walkway_80183C38[mem->index].b;
                func_shelter_b2_north_maintenance_walkway_8017FF44(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b2_north_maintenance_walkway_8017FF44(coord, mem->angle + 0x100, rgb);
                }
                func_shelter_b2_north_maintenance_walkway_8017FB20(coord, 0x300 - (u16)mem->angle * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b2_north_maintenance_walkway_80183C38[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b2_north_maintenance_walkway_80183C38[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b2_north_maintenance_walkway_80183C38[mem->index].b;
                    func_shelter_b2_north_maintenance_walkway_801810C0(coord, (u16)mem->angle * 4, rgb);
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

/// A twin-ring burst in orange. Each tick draws the halo and a second ring at a
/// growing angle while a wider, dimmer echo ring expands and fades behind them;
/// once the echo is gone the main level falls 0x18 a tick and the work block
/// is released. It pauses while the room's event state is set and releases the
/// block when that state reaches 4.
void func_shelter_b2_north_maintenance_walkway_80180670(Task* arg0)
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
        func_shelter_b2_north_maintenance_walkway_8017FF44(coord, step * 2, rgb);
        func_shelter_b2_north_maintenance_walkway_8018081C(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_shelter_b2_north_maintenance_walkway_8017FB20(coord, mem->step * 3 / 2, 0x60, rgb);
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
