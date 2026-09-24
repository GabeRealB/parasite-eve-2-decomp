#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_shelter_b1_pod_access_tunnel_8017F3DC(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b1_pod_access_tunnel_8017F808(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
void func_shelter_b1_pod_access_tunnel_8018070C(GsCOORDINATE2* coord, s16 arg1, u8* rgb);

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_shelter_b1_pod_access_tunnel_8017F138(Task* task)
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
                func_shelter_b1_pod_access_tunnel_8017F808(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b1_pod_access_tunnel_8017F808(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b1_pod_access_tunnel_8017F3DC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_shelter_b1_pod_access_tunnel_8018070C(coord, (s16)((s16)work->angle * 3), rgb);
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
