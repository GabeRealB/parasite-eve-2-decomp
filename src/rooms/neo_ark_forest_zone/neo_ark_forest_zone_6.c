#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_forest_zone.h"
#include "rooms/room_common.h"

extern s32 Gp_LcgState;

/// A drifting flake. State 0 gives it a size of 0x20, random X and Z tilts and
/// a random velocity. State 1 moves and tilts it each frame: the vertical
/// speed eases towards 0x1C, the sideways speeds decay and get a fresh random
/// kick whenever they reach zero, and the tilts wobble. Once it drops below
/// the ground plane (`t[1] > 0`) state 2 holds it while `field_26` counts up to
/// 0x80, and state 3 fades it out, drawn semi-transparent at `field_26` as that
/// counts back down, before releasing the work block. Until the fade it is
/// drawn as an opaque textured quad.
void func_neo_ark_forest_zone_8017DC20(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s32            vy;
    s32            vx;
    s32            vz;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24    = 0x20;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_2A    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state       = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->field_10.vx;
            coord->coord.t[1] += work->field_10.vy;
            coord->coord.t[2] += work->field_10.vz;
            Gfx_RotMatrixX(&coord->coord, (s16)work->field_28, 0);
            Gfx_RotMatrixZ(&coord->coord, (s16)work->field_2A, 0);
            coord->flg = 0;

            vy = work->field_10.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->field_10.vy = vy;

            vx = work->field_10.vx;
            if (vx == 0) {
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->field_10.vx = vx;
            }

            vz = work->field_10.vz;
            if (vz == 0) {
                work->field_10.vz += (s16)work->field_2A % 32;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->field_10.vz = vz;
            }

            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_28 += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_neo_ark_forest_zone_8017E074(coord, (s16)work->field_24, 0);
            break;
        case 2:
            if ((s16)work->field_26 < 0x80) {
                work->field_26 += 0x10;
            } else {
                task->state = 3;
            }
            func_neo_ark_forest_zone_8017E074(coord, (s16)work->field_24, 0);
            break;
        case 3:
            if ((s16)work->field_26 >= 0x11) {
                work->field_26 -= 0x10;
                func_neo_ark_forest_zone_8017E074(coord, (s16)work->field_24, (s16)work->field_26);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}
