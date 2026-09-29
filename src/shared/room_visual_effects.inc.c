#include "room_visual_effects.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017dcb8.h"

static void RoomFx_DrawMote(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void RoomFx_DrawHaloRing(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void RoomFx_DrawHaloDisc(GpCoord* arg0, s16 arg1, u8* rgb);
static void RoomFx_DrawBurstGlow(GpCoord* coord, s16 size);
static void RoomFx_DrawGroundQuad(GpCoord* arg0, s32 arg1);
static void RoomFx_DrawFlashStar(GpCoord* arg0, s16 arg1, u8* arg2);
static void RoomFx_DrawFlashRing(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void RoomFx_DrawFlashDisc(GpCoord* arg0, s16 arg1, u8* rgb);
static void RoomFx_DrawTwinTrail(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
static void RoomFx_DrawBurstStar(GpCoord* arg0, s16 arg1, u8* arg2);
static void RoomFx_DrawFlyingSpark(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void RoomFx_DrawFlyingRing(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void RoomFx_DrawFlyingDisc(GpCoord* arg0, s32 arg1, u8* rgb);
static void RoomFx_DrawBurst2Glow(GpCoord* coord, s16 size);
static void RoomFx_DrawGround2Quad(GpCoord* arg0, s32 arg1);

/// A drifting mote. The first tick unpacks the spawn argument: a mote with
/// either low bit set starts at full brightness and moves at its given
/// vertical speed (upwards when bit 1 is set) in state 2; otherwise it starts
/// dim, rises at its speed plus a random 0..0x3F and brightens as it goes, in
/// state 1. Every tick moves it, every other tick advances its drawing phase
/// and draws it, and within eight ticks of its lifetime it fades out, releasing
/// its work block once dark. It pauses while the room's event state is set
/// and releases the block when that state reaches 4.
static inline void RoomFx_MoteTask(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    RoomFx_DrawMote(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    RoomFx_DrawMote(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}
