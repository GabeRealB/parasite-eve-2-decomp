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

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_neo_ark_forest_zone_80182E40[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_neo_ark_forest_zone_80182E40_value __asm__("D_neo_ark_forest_zone_80182E40");

extern GpSaveLoc        D_neo_ark_forest_zone_80182E38;
extern RoomLatchedEvent D_neo_ark_forest_zone_80182E48;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage D_neo_ark_forest_zone_80182E30;

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `D_neo_ark_forest_zone_80182094[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_forest_zone_8017DA80(Task* arg0);
static void func_neo_ark_forest_zone_8017DB40(Task* arg0);
static void func_neo_ark_forest_zone_8017E074(GfxCoord* arg0, s32 arg1, s16 arg2);
static void func_neo_ark_forest_zone_8017E6C4(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_forest_zone_8017EAF0(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_forest_zone_8017F374(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_forest_zone_8017F9F4(GfxCoord* arg0, s16 arg1, u8* arg2);

RoomFadeStorage D_neo_ark_forest_zone_80182E30 = { 0 };

GpSaveLoc D_neo_ark_forest_zone_80182E38 = { 0 };

s8 D_neo_ark_forest_zone_80182E40[4] = {
    0,
    123,
    4,
    20,
};

GpCmdArg D_neo_ark_forest_zone_80182E44 = { 0 };

RoomLatchedEvent D_neo_ark_forest_zone_80182E48 = { 0 };

u16 D_neo_ark_forest_zone_80182E54[5] = {
    0,
    0,
    0,
    0,
    0,
};

static __inline__ s32 NeoArkForestZone_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event);
static void           func_neo_ark_forest_zone_8017DBAC(Task* task);

/// The room's event task, spawned when the room latches an event. State 0 runs
/// the event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound, if any, and wait for it; state 4 writes the latched destination into
/// the save data and hands over to task type 0x11.
void func_neo_ark_forest_zone_8017D644(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_neo_ark_forest_zone_80182E48.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_neo_ark_forest_zone_80182E48.fade != 0) {
                    D_neo_ark_forest_zone_80182E30.fade.field_0 = 0;
                    D_neo_ark_forest_zone_80182E30.fade.field_1 = 0;
                    D_neo_ark_forest_zone_80182E30.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_neo_ark_forest_zone_80182E30.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_neo_ark_forest_zone_80182E48.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_neo_ark_forest_zone_80182E48.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_neo_ark_forest_zone_80182E48.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_neo_ark_forest_zone_80182E38.prefix.bytes.field_0;
            Mc_SaveData[0].state.at4.loc.warp = D_neo_ark_forest_zone_80182E38.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_neo_ark_forest_zone_80182E38.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_forest_zone_8017D7DC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Latches the room's pending event and starts the controller that runs it:
/// clears the "event running" flag, and once the event's flag nibble is clear
/// (or the event carries no flag) and `dst->field_5` does not ask for the side
/// effects to be suppressed, commits `dst` and the event and spawns the
/// controller task. Answers 2 for a started event, 1 when `field_5` held it
/// back.
static __inline__ s32 NeoArkForestZone_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event)
{
    D_neo_ark_forest_zone_80182E40_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_neo_ark_forest_zone_80182E38 = *dst;
            D_neo_ark_forest_zone_80182E48 = *event;
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
/// (`field_5` clear, the flag that asks a handler to only report what *would*
/// happen) it also restarts the room's ambience sound. Message 0x1D builds the
/// room's event record - cap command 2, the stage sound, flag 0x140 - and hands
/// it to `NeoArkForestZone_StartEvent`; every other message is not consumed and
/// answers 1.
s32 func_neo_ark_forest_zone_8017D7E4(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->field_5 == 0) {
        SndEvt_EnqueueType7(0x550B0006, 0x3C);
    }
    if (*(u16*)in != 0x1D) {
        return 1;
    }
    event.capCmd   = 2;
    event.stageSnd = 0x550B0003;
    event.flagId   = 0x140;
    event.fade     = 0;
    return NeoArkForestZone_StartEvent(out, &event);
}

s32 func_neo_ark_forest_zone_8017D950(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room message handler: on the first-visit sub-id (`field_2 == 1`) with flag
/// 0xBD unset and the session's visit count equal to that sub-id, latches flag
/// 0xBD and starts the room's fade with the record at `D_..._80181E6C`. Then
/// forwards the message to the room's own task, answering -1 while that task
/// does not exist yet.
s32 func_neo_ark_forest_zone_8017D958(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 visit;

    visit = in->field_2;
    if (visit == 1) {
        if (GameFlag_GetNibble(0xBD) == 0 && gGameSession->at4.loc.variant == visit) {
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
/// visit (`gGameSession->at4.loc.variant == 1`) with flag 0xBD unset has the
/// slot-4 task relay message 0x7DA carrying the first payload record. Then
/// advances state.
static void func_neo_ark_forest_zone_8017DA80(Task* arg0)
{
    arg0->msgTable = D_neo_ark_forest_zone_80181DC8;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550B0006, 0, 0);
    D_neo_ark_forest_zone_80181E68 = Task_SpawnFromTable(&D_neo_ark_forest_zone_80182E18, 0, 0, 0);
    if (gGameSession->at4.loc.variant == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &D_neo_ark_forest_zone_80181E30, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// State 1 of the room setup task: on the first visit
/// (`gGameSession->at4.loc.variant == 1`) with flag 0xBD unset, sends message
/// 0x7DB to the room's own task carrying its first payload record, then
/// advances state.
static void func_neo_ark_forest_zone_8017DB40(Task* arg0)
{
    if (gGameSession->at4.loc.variant == 1 && GameFlag_GetNibble(0xBD) == 0) {
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

    block = SCRATCH_PUSH(GpQuadScratch);
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
    SCRATCH_POP(GpQuadScratch);
}

/// Keeps `Gp_State1C->roomEffectMode` at 2 every frame and, on its first run,
/// stores 0x601D9, 0x601F5 and 0x60211 in three gameplay globals; the values
/// have the form `Gp_SpawnEff` takes as effect ids.
void func_neo_ark_forest_zone_8017E3C0(Task* arg0)
{
    Gp_State1C->roomEffectMode = 2;
    if (arg0->state == 0) {
        D_80115758  = 0x601D9;
        D_8011572C  = 0x601F5;
        D_80115750  = 0x60211;
        arg0->state = 1;
    }
}

/// Expanding flash burst. State 0 derives the per-frame step from the spawn
/// argument (the frame count). State 1 grows the level and radius each frame,
/// drawing a disc at the radius, a half-bright one at twice it and a ring
/// closing in from 0x300, and on its last frame lays a full-screen fade quad.
/// State 2 draws a star glow at three times the radius while the level falls
/// back to 0x10, then releases the work block. While the room's event state is
/// set it draws nothing, and releases the block once that reaches 4.
void func_neo_ark_forest_zone_8017E420(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
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
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_neo_ark_forest_zone_8017EAF0(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_forest_zone_8017EAF0(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_forest_zone_8017E6C4(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_neo_ark_forest_zone_8017F9F4(coord, (s16)(work->angle * 3), rgb);
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

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` segments
/// forming a ring between two radii, `arg1` and `arg1 + arg2` in world units
/// scaled by depth. The edge at `arg1` is black and the edge at `arg1 + arg2`
/// carries `rgb`, so the ring fades out towards `arg1`.
static void func_neo_ark_forest_zone_8017E6C4(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
static void func_neo_ark_forest_zone_8017EAF0(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Twin smoke trail. State 0 allocates sixteen `GfxCoord`s, eight per
/// trail, and seeds them all from the two spawn offsets so each trail starts
/// collapsed on its origin. State 1 advances one slot of each trail per frame,
/// re-derives all sixteen against the view and draws them. The task frees
/// itself once its age reaches the spawn argument, and idles while the room's
/// event state is 2 or more.
void func_neo_ark_forest_zone_8017EE84(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_neo_ark_forest_zone_80182094[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_forest_zone_80182094[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_forest_zone_80182094[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_forest_zone_80182094[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_neo_ark_forest_zone_80182094[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_forest_zone_8017F374(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the trail
/// colour, packed as red from bit 8 up, green in bits 4-5 and blue in bits
/// 0-1, each multiplying that fade. A quad is dropped when `gte_stflg` is
/// negative.
static void func_neo_ark_forest_zone_8017F374(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Spark burst. State 0 fires the burst's effect, then a non-zero spawn
/// argument starts a stream of jittered sparks (state 1) and a zero one a pair
/// of rings whose radius grows and brightness falls each frame (state 2).
/// Either way the task reaches state 3 after seven frames and releases its
/// work block, or earlier once the room's event state reaches 4.
void func_neo_ark_forest_zone_8017F76C(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_neo_ark_forest_zone_8017E6C4(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_forest_zone_8017E6C4(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges: sixteen around the circle, alternating full radius at half
/// intensity and half radius at full intensity, then four spikes a quarter
/// turn apart, two reaching the full radius and two twice it. `arg1` sizes it
/// in world units scaled by depth; every wedge fades to black at its rim.
static void func_neo_ark_forest_zone_8017F9F4(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
