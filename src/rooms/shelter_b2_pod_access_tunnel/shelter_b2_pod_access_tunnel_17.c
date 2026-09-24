#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern void func_shelter_b2_pod_access_tunnel_80181ED0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3);
void        func_shelter_b2_pod_access_tunnel_801822FC(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void        func_shelter_b2_pod_access_tunnel_80183200(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// A violet flash. The first tick works out the per-tick step from the tick
/// count in the spawn argument. While the count runs down the level and angle
/// grow by that step and the flash is drawn through
/// `func_shelter_b2_pod_access_tunnel_801822FC` at the angle and, half as
/// bright, at twice it, with a ring through
/// `func_shelter_b2_pod_access_tunnel_80181ED0` whose inner radius shrinks as
/// the angle grows. The level then jumps to 0xFF, a full-screen fade quad is
/// queued, and the level fades by 0x10 a tick through
/// `func_shelter_b2_pod_access_tunnel_80183200` before the work block is
/// released, as it is when the room's event state reaches 4. The tint is full
/// red, quarter green and half blue.
void func_shelter_b2_pod_access_tunnel_80181C2C(Task* arg0)
{
    u8                      rgb[3];
    GpEffWork*              mem;
    register GsCOORDINATE2* coord asm("s2");
    s16                     flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        switch (arg0->state) {
            case 0:
                mem->scale  = 0;
                mem->angle  = 0x80;
                mem->step   = 0x100 / arg0->spawnArg1;
                arg0->state = 1;
                return;
            case 1:
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->scale;
                rgb[1]           = (u16)mem->scale >> 2;
                rgb[2]           = (u16)mem->scale >> 1;
                func_shelter_b2_pod_access_tunnel_801822FC(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                func_shelter_b2_pod_access_tunnel_801822FC(coord, (u16)mem->angle * 2, rgb);
                func_shelter_b2_pod_access_tunnel_80181ED0(coord, 0x300 - (u16)mem->angle * 2, 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    rgb[0]      = mem->scale;
                    rgb[1]      = (u16)mem->scale >> 2;
                    rgb[2]      = (u16)mem->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                return;
            case 2:
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale;
                    rgb[1] = (u16)mem->scale >> 2;
                    rgb[2] = (u16)mem->scale >> 1;
                    func_shelter_b2_pod_access_tunnel_80183200(coord, mem->angle * 3, rgb);
                    mem->scale -= 0x10;
                    mem->angle -= 8;
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
