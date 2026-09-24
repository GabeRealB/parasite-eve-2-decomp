#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"
#include "rooms/dryfield_motel_balcony.h"

/// Per-tint channel shifts for the halo task, indexed by the tint the spawn
/// argument selects.
extern RoomsShared8017e4f8Shade D_dryfield_motel_balcony_801822AC[];

extern void func_dryfield_motel_balcony_8017E248(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);

/// An expanding halo effect task. State 0 parks the effect's coordinate on its
/// parent at the work block's position and derives the fade step from the
/// spawn argument; state 1 ramps the level up, drawing the halo (plus a
/// half-bright copy on odd ticks) and a ring shrinking inwards; state 2 fades
/// the level out through the afterglow draw before the work block is released.
void func_dryfield_motel_balcony_8017EA00(Task* arg0)
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
                rgb[0]           = mem->scale >> D_dryfield_motel_balcony_801822AC[mem->index].r;
                rgb[1]           = mem->scale >> D_dryfield_motel_balcony_801822AC[mem->index].g;
                rgb[2]           = mem->scale >> D_dryfield_motel_balcony_801822AC[mem->index].b;
                func_dryfield_motel_balcony_8017E66C(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_dryfield_motel_balcony_8017E66C(coord, mem->angle + 0x100, rgb);
                }
                func_dryfield_motel_balcony_8017E248(coord, 0x300 - (u16)mem->angle * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_dryfield_motel_balcony_801822AC[mem->index].r;
                    rgb[1] = mem->scale >> D_dryfield_motel_balcony_801822AC[mem->index].g;
                    rgb[2] = mem->scale >> D_dryfield_motel_balcony_801822AC[mem->index].b;
                    func_dryfield_motel_balcony_8017F7E8(coord, (u16)mem->angle * 4, rgb);
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

/// A twin-ring burst effect task: each frame it draws the halo and a second
/// ring at a growing angle, and while its echo level lasts a wider ring
/// fading out behind them; the work block is released once the main level
/// runs down.
void func_dryfield_motel_balcony_8017ED98(Task* arg0)
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
        func_dryfield_motel_balcony_8017E66C(coord, step * 2, rgb);
        func_dryfield_motel_balcony_8017EF44(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_dryfield_motel_balcony_8017E248(coord, mem->step * 3 / 2, 0x60, rgb);
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
