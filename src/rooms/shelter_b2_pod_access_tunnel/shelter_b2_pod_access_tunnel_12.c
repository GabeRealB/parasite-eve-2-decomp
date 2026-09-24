#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017dcb8.h"

extern s32 Gp_LcgState;

void func_shelter_b2_pod_access_tunnel_8017F8D4(GsCOORDINATE2*, s32, s32, s32);

/// A drifting mote. The first tick unpacks the spawn argument: with either of
/// its low two bits set the mote starts at full brightness 0x80 and moves at
/// the packed speed (upwards when bit 1 is set); otherwise it starts dim at
/// 0x20 and rises at the packed speed plus a random 0..0x3F. Each later tick
/// moves the task's coordinate by that speed and, every other tick, draws it
/// through `func_shelter_b2_pod_access_tunnel_8017F8D4`. The dim kind brightens
/// towards 0x80 until eight ticks before its lifetime runs out, and both fade
/// by 0x10 a tick from then on. The work block is released once the mote is
/// dark, or when the room's event state reaches 4.
void func_shelter_b2_pod_access_tunnel_8017F608(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s32            lifetime;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1 & 3) {
                    work->field_24    = 0x80;
                    work->field_26    = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->field_28    = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime          = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->field_2A    = lifetime;
                    work->field_10.vx = 0;
                    work->field_10.vy = ((RoomMoteArg*)&task->spawnArg1)->speed;
                    work->field_10.vz = 0;
                    if (task->spawnArg1 & 2) {
                        work->field_10.vy = -work->field_10.vy;
                    }
                    task->state = 2;
                } else {
                    work->field_24    = 0x20;
                    work->field_26    = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->field_28    = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime          = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->field_2A    = lifetime;
                    work->field_10.vx = 0;
                    work->field_10.vy = -((RoomMoteArg*)&task->spawnArg1)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->field_10.vz = 0;
                    task->state       = (task->spawnArg1 & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->field_10.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->field_22 & 1) {
                    work->field_20++;
                    func_shelter_b2_pod_access_tunnel_8017F8D4(coord, work->field_20, work->field_26 | 0x1000, work->field_24 | work->field_28);
                }
                if ((s16)work->field_24 > 0) {
                    if ((s16)work->field_2A - 8 < (s16)work->field_22) {
                        work->field_24 -= 0x10;
                    } else if ((s16)work->field_24 < 0x80) {
                        work->field_24 += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->field_10.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->field_22 & 1) {
                    work->field_20++;
                    func_shelter_b2_pod_access_tunnel_8017F8D4(coord, work->field_20, work->field_26, work->field_24 | work->field_28);
                }
                if ((s16)work->field_24 > 0) {
                    if ((s16)work->field_2A - 8 < (s16)work->field_22) {
                        work->field_24 -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}
