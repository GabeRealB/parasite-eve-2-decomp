#include "rooms/neo_ark_forest_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_forest_zone_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_neo_ark_forest_zone_80182E40[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_neo_ark_forest_zone_80182E40_value __asm__("D_neo_ark_forest_zone_80182E40");

extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage gRoomEventFade;

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `RoomFx_TrailOffsets[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_forest_zone_8017DA80(Task* arg0);
static void func_neo_ark_forest_zone_8017DB40(Task* arg0);
static void func_neo_ark_forest_zone_8017E074(GfxCoord* arg0, s32 arg1, s16 arg2);

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_neo_ark_forest_zone_80182E40[4] = {
    0,
    123,
    4,
    20,
};

ActorCommand D_neo_ark_forest_zone_80182E44 = { 0 };

RoomLatchedEvent gRoomEventLatched = { 0 };

u16 D_neo_ark_forest_zone_80182E54[5] = {
    0,
    0,
    0,
    0,
    0,
};

static __inline__ s32 NeoArkForestZone_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_neo_ark_forest_zone_8017DBAC(Task* task);

#include "../../shared/room_event_staged_task.inc.c"

s32 func_neo_ark_forest_zone_8017D7DC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Latches the room's pending event and starts the controller that runs it:
/// clears the "event running" flag, and once the event's flag nibble is clear
/// (or the event carries no flag) and `dst->queryOnly` does not ask for the side
/// effects to be suppressed, commits `dst` and the event and spawns the
/// controller task. Answers 2 for a started event, 1 when `queryOnly` held it
/// back.
static __inline__ s32 NeoArkForestZone_StartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_neo_ark_forest_zone_80182E40_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *dst;
            gRoomEventLatched   = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_neo_ark_forest_zone_80181DBC, 0, 0, 0);
            D_neo_ark_forest_zone_80182E40_value = 1;
        }
        return 2;
    }
    return 1;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `func_map_neo_ark_80179B14`. On a first pass
/// (`queryOnly` clear, the flag that asks a handler to only report what *would*
/// happen) it also restarts the room's ambience sound. Message 0x1D builds the
/// room's event record - cap command 2, the stage sound, flag 0x140 - and hands
/// it to `NeoArkForestZone_StartEvent`; every other message is not consumed and
/// answers 1.
s32 func_neo_ark_forest_zone_8017D7E4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        SndEvt_EnqueueType7(0x550B0006, 0x3C);
    }
    if (in->areaId != 0x1D) {
        return 1;
    }
    event.capCmd   = 2;
    event.stageSnd = 0x550B0003;
    event.flagId   = 0x140;
    event.fade     = 0;
    return NeoArkForestZone_StartEvent(out, &event);
}

s32 func_neo_ark_forest_zone_8017D950(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Room message handler: on the first-visit sub-id (`warp == 1`) with flag
/// 0xBD unset and the session's visit count equal to that sub-id, latches flag
/// 0xBD and starts the room's fade with the record at `D_..._80181E6C`. Then
/// forwards the message to the room's own task, answering -1 while that task
/// does not exist yet.
s32 func_neo_ark_forest_zone_8017D958(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 visit;

    visit = in->warp;
    if (visit == 1) {
        if (GameFlag_GetNibble(0xBD) == 0 && gGameSession->location.loc.variant == visit) {
            GameFlag_SetNibble(0xBD, 1);
            func_800E8614(D_neo_ark_forest_zone_80181E6C, 0);
        }
    }
    if (D_neo_ark_forest_zone_80181E68 != NULL) {
        return Gp_DispatchMsgPtrs(D_neo_ark_forest_zone_80181E68, arg1, in, out);
    }
    return -1;
}

/// 0x13F4 handler of the room's message table: passes the message on to the
/// room's own task, answering -1 while that task does not exist.
s32 func_neo_ark_forest_zone_8017DA14(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 ret;

    if (D_neo_ark_forest_zone_80181E68 == NULL) {
        ret = -1;
    } else {
        ret = Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, msgId, arg2, arg3);
    }
    return ret;
}

/// Once the room's own task exists, broadcast message 0x7DB to it, carrying
/// the room's payload record as `arg2`.
void func_neo_ark_forest_zone_8017DA48(void)
{
    if (D_neo_ark_forest_zone_80181E68 != 0) {
        Gp_DispatchMsgPtr(D_neo_ark_forest_zone_80181E68, 0x7DB, &D_neo_ark_forest_zone_80181E38, 0);
    }
}

/// State 0 of the room setup task: installs the message table and pointer
/// slot 7, starts the ambience, spawns the room's own task, and on the first
/// visit (`gGameSession->location.loc.variant == 1`) with flag 0xBD unset has the
/// slot-4 task relay message 0x7DA carrying the first payload record. Then
/// advances state.
static void func_neo_ark_forest_zone_8017DA80(Task* arg0)
{
    arg0->msgTable = D_neo_ark_forest_zone_80181DC8;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550B0006, 0, 0);
    D_neo_ark_forest_zone_80181E68 = Task_SpawnFromTable(&D_neo_ark_forest_zone_80182E18, 0, 0, 0);
    if (gGameSession->location.loc.variant == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &D_neo_ark_forest_zone_80181E30, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// State 1 of the room setup task: on the first visit
/// (`gGameSession->location.loc.variant == 1`) with flag 0xBD unset, sends message
/// 0x7DB to the room's own task carrying its first payload record, then
/// advances state.
static void func_neo_ark_forest_zone_8017DB40(Task* arg0)
{
    if (gGameSession->location.loc.variant == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsgPtr(D_neo_ark_forest_zone_80181E68, 0x7DB, &D_neo_ark_forest_zone_80181E30, 0);
    }
    arg0->state = arg0->state + 1;
}

/// State 2 of the room setup task: does nothing until the task is killed. The
/// unused local reproduces the original's stack frame.
static void func_neo_ark_forest_zone_8017DBAC(Task* task)
{
    char pad[0x10];
}

/// State table of the room setup task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D5D8 = { {
    func_neo_ark_forest_zone_8017DA80,
    func_neo_ark_forest_zone_8017DB40,
    func_neo_ark_forest_zone_8017DBAC,
    taskKill,
} };

/// The room setup task: runs the state handler its state selects, through a
/// copy of the state table on the stack.
void func_neo_ark_forest_zone_8017DBBC(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D5D8;
    sp.funcs[task->state](task);
}

/// A drifting flake. State 0 gives it a size of 0x20, random X and Z tilts and
/// a random velocity. State 1 moves and tilts it each frame: the vertical
/// speed eases towards 0x1C, the sideways speeds decay and get a fresh random
/// kick whenever they reach zero, and the tilts wobble. Once it drops below
/// the ground plane (`t[1] > 0`) state 2 holds it while `angle` counts up to
/// 0x80, and state 3 fades it out, drawn semi-transparent at `angle` as that
/// counts back down, before releasing the work block. Until the fade it is
/// drawn as an opaque textured quad.
void func_neo_ark_forest_zone_8017DC20(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        vy;
    s32        vx;
    s32        vz;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale   = 0x20;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state   = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            Gfx_RotMatrixX(&coord->coord, work->period, 0);
            Gfx_RotMatrixZ(&coord->coord, work->step, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;

            vy = work->move.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->move.vy = vy;

            vx = work->move.vx;
            if (vx == 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->move.vx = vx;
            }

            vz = work->move.vz;
            if (vz == 0) {
                work->move.vz += work->step % 32;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->move.vz = vz;
            }

            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step   += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_neo_ark_forest_zone_8017E074(coord, work->scale, 0);
            break;
        case 2:
            if (work->angle < 0x80) {
                work->angle += 0x10;
            } else {
                task->state = 3;
            }
            func_neo_ark_forest_zone_8017E074(coord, work->scale, 0);
            break;
        case 3:
            if (work->angle >= 0x11) {
                work->angle -= 0x10;
                func_neo_ark_forest_zone_8017E074(coord, work->scale, work->angle);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws the drifting flake as one textured quad lying in the coordinate's
/// local XZ plane: the unit quad `D_80111E38`, scaled by `arg1`, is turned by
/// the coordinate's world matrix, moved to its translation and projected
/// through `GsWSMATRIX`. Unless the GTE flags the projection a `POLY_FT4`
/// (tpage 0x2B, clut 0x4390, an 8x8 texel cell at (0, 0x28)) is queued, raw
/// textured when `arg2` is zero and otherwise semi-transparent at grey level
/// `arg2`.
static void func_neo_ark_forest_zone_8017E074(GfxCoord* arg0, s32 arg1, s16 arg2)
{
    GpQuadScratch* block;
    s32            i;
    POLY_FT4*      prim;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpQuadScratch);
    for (i = 0; i < 4; i++) {
        block->vec[i].vx = D_80111E38[i].x * arg1;
        block->vec[i].vy = 0;
        block->vec[i].vz = D_80111E38[i].y * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        block->vec[i].vx += arg0->workm.t[0];
        block->vec[i].vy += arg0->workm.t[1];
        block->vec[i].vz += arg0->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (arg2 != 0) {
            setSemiTrans(prim, 1);
            setRGB0(prim, arg2, arg2, arg2);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        prim->v0    = 0x28;
        prim->v1    = 0x28;
        prim->u0    = 0;
        prim->u1    = 7;
        prim->u2    = 0;
        prim->v2    = 0x2F;
        prim->u3    = 7;
        prim->v3    = 0x2F;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpQuadScratch);
}

/// Keeps `Gp_State1C->roomEffectMode` at 2 every frame and, on its first run,
/// stores 0x601D9, 0x601F5 and 0x60211 in three gameplay globals; the values
/// have the form `Gp_SpawnEff` takes as effect ids.
void func_neo_ark_forest_zone_8017E3C0(Task* arg0)
{
    Gp_State1C->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    if (arg0->state == 0) {
        D_80115758  = 0x601D9;
        D_8011572C  = 0x601F5;
        D_80115750  = 0x60211;
        arg0->state = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_forest_zone_8017E420(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_forest_zone_8017EE84(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_forest_zone_8017F76C(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
