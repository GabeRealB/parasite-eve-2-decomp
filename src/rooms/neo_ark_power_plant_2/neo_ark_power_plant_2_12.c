#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_neo_ark_power_plant_2_8017E098(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_power_plant_2_8017E4C4(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
void func_neo_ark_power_plant_2_8017F3C8(GsCOORDINATE2* coord, s16 arg1, u8* rgb);

/// Flash burst. State 0 derives a per-frame step of `0x100 / spawnArg1` for
/// the brightness and the radius; state 1 spends the spawn argument one frame
/// at a time, drawing a bright disc, a half-bright one at twice the radius and
/// a ring closing in from 0x300, then whites the screen out with a fade quad
/// and moves to state 2. State 2 draws a star-shaped afterglow at three times
/// the radius while dimming it by 0x10 a frame, and releases the work block
/// once it has faded; the block is also released once the room's event state
/// reaches 4.
void func_neo_ark_power_plant_2_8017DDF4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_neo_ark_power_plant_2_8017E4C4(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_power_plant_2_8017E4C4(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_power_plant_2_8017E098(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_neo_ark_power_plant_2_8017F3C8(coord, (s16)((s16)work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}
