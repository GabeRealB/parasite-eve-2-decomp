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

extern RoomsShared8017e4f8Shade D_shelter_b2_pod_access_tunnel_80183DC8[];

extern void func_shelter_b2_pod_access_tunnel_8017FB98(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
void        func_shelter_b2_pod_access_tunnel_8017FFBC(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void        func_shelter_b2_pod_access_tunnel_80180894(GsCOORDINATE2* coord, s16 frame);
void        func_shelter_b2_pod_access_tunnel_80181138(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// An expanding halo. The first tick hangs the effect's coordinate off its
/// parent at the spawn position, takes the tint row and the tick count from
/// the spawn argument and works out the per-tick step. While the count runs
/// down the level and angle grow by that step and the halo is drawn through
/// `func_shelter_b2_pod_access_tunnel_8017FFBC`, with a half-bright copy 0x100
/// further round on every odd tick, and a ring through
/// `func_shelter_b2_pod_access_tunnel_8017FB98` whose inner radius shrinks as
/// the angle grows. The level then jumps to 0xFF and fades by 0x10 a tick
/// through `func_shelter_b2_pod_access_tunnel_80181138` before the work block
/// is released, as it is when the room's event state reaches 4. Each channel
/// is the level shifted right by the tint row's entry.
void func_shelter_b2_pod_access_tunnel_80180350(Task* arg0)
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
                rgb[0]           = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8[mem->index].r;
                rgb[1]           = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8[mem->index].g;
                rgb[2]           = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8[mem->index].b;
                func_shelter_b2_pod_access_tunnel_8017FFBC(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b2_pod_access_tunnel_8017FFBC(coord, mem->angle + 0x100, rgb);
                }
                func_shelter_b2_pod_access_tunnel_8017FB98(coord, 0x300 - (u16)mem->angle * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8[mem->index].b;
                    func_shelter_b2_pod_access_tunnel_80181138(coord, (u16)mem->angle * 4, rgb);
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

/// A twin-ring burst. The first tick seeds a bright level and a dimmer echo
/// level. Each tick then draws the halo through
/// `func_shelter_b2_pod_access_tunnel_8017FFBC` and a ring through
/// `func_shelter_b2_pod_access_tunnel_80180894` at a growing angle, in an
/// orange tint (full red, half green, quarter blue), while the echo is drawn
/// as a widening ring through `func_shelter_b2_pod_access_tunnel_8017FB98`
/// until it has faded. The work block is released once the main level falls
/// below 0x18, or when the room's event state reaches 4.
void func_shelter_b2_pod_access_tunnel_801806E8(Task* arg0)
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
        func_shelter_b2_pod_access_tunnel_8017FFBC(coord, step * 2, rgb);
        func_shelter_b2_pod_access_tunnel_80180894(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_shelter_b2_pod_access_tunnel_8017FB98(coord, mem->step * 3 / 2, 0x60, rgb);
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
