#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern s16 D_80071076;
extern u8  D_80115690;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

/// The room's message table, which the room setup task installs.
extern GpMsgEntry D_neo_ark_forest_zone_80181DC8[];

extern s32      D_neo_ark_forest_zone_80181E30;
extern s32      D_neo_ark_forest_zone_80181E38;
extern Task*    D_neo_ark_forest_zone_80181E68;
extern u8       D_neo_ark_forest_zone_80181E6C[];
extern TaskDesc D_neo_ark_forest_zone_80182E18;

extern TaskDesc         D_neo_ark_forest_zone_80181DBC;
extern GpSaveLoc        D_neo_ark_forest_zone_80182E38;
extern s8               D_neo_ark_forest_zone_80182E40;
extern RoomLatchedEvent D_neo_ark_forest_zone_80182E48;

/// Payload handed to the helper task 0x31 the event may start.
extern GpFadeWork D_neo_ark_forest_zone_80182E30;

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `D_neo_ark_forest_zone_8018209C` is
/// `[1]` under its own name, which the per-frame path reads directly.
extern SVECTOR D_neo_ark_forest_zone_80182094[];
extern SVECTOR D_neo_ark_forest_zone_8018209C;

void func_neo_ark_forest_zone_8017DA80(Task* arg0);
void func_neo_ark_forest_zone_8017DB40(Task* arg0);
void func_neo_ark_forest_zone_8017E074(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
void func_neo_ark_forest_zone_8017E6C4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_forest_zone_8017EAF0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_neo_ark_forest_zone_8017F374(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_neo_ark_forest_zone_8017F9F4(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

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
                    D_neo_ark_forest_zone_80182E30.field_0 = 0;
                    D_neo_ark_forest_zone_80182E30.field_1 = 0;
                    D_neo_ark_forest_zone_80182E30.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_neo_ark_forest_zone_80182E30);
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
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_neo_ark_forest_zone_80182E38.field_0;
            Mc_SaveData.at4.loc.warp = D_neo_ark_forest_zone_80182E38.field_2;
            Mc_SaveData.at4.loc.room = D_neo_ark_forest_zone_80182E38.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_forest_zone_8017D7DC(void)
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
    D_neo_ark_forest_zone_80182E40 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_neo_ark_forest_zone_80182E38 = *dst;
            D_neo_ark_forest_zone_80182E48 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_neo_ark_forest_zone_80181DBC, 0, 0, 0);
            D_neo_ark_forest_zone_80182E40 = 1;
        }
        return 2;
    }
    return 1;
}

/// Room handler for the save-location message: copies the incoming record onto
/// the outgoing one and forwards both to `func_80179B14`. On a first pass
/// (`field_5` clear, the flag that asks a handler to only report what *would*
/// happen) it also restarts the room's ambience sound. Message 0x1D builds the
/// room's event record - cap command 2, the stage sound, flag 0x140 - and hands
/// it to `NeoArkForestZone_StartEvent`; every other message is not consumed and
/// answers 1.
s32 func_neo_ark_forest_zone_8017D7E4(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_80179B14(in, out);
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

s32 func_neo_ark_forest_zone_8017D950(void)
{
    return 0;
}

/// Room message handler: on the first-visit sub-id (`field_2 == 1`) with flag
/// 0xBD unset and the session's visit count equal to that sub-id, latches flag
/// 0xBD and starts the room's fade with the record at `D_..._80181E6C`. Then
/// forwards the message to the room's own task, answering -1 while that task
/// does not exist yet.
s32 func_neo_ark_forest_zone_8017D958(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 visit;

    visit = in->field_2;
    if (visit == 1) {
        if (GameFlag_GetNibble(0xBD) == 0 && gGameSession->at4.loc.place == visit) {
            GameFlag_SetNibble(0xBD, 1);
            func_800E8614((s32)D_neo_ark_forest_zone_80181E6C, 0);
        }
    }
    if (D_neo_ark_forest_zone_80181E68 != NULL) {
        return Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, arg1, (s32)in, (s32)out);
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
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E38, 0);
    }
}

/// State 0 of the room setup task: installs the message table and pointer
/// slot 7, starts the ambience, spawns the room's own task, and on the first
/// visit (`gGameSession->at4.loc.place == 1`) with flag 0xBD unset has the
/// slot-4 task relay message 0x7DA carrying the first payload record. Then
/// advances state.
void func_neo_ark_forest_zone_8017DA80(Task* arg0)
{
    arg0->msgTable = D_neo_ark_forest_zone_80181DC8;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550B0006, 0, 0);
    D_neo_ark_forest_zone_80181E68 = Task_SpawnFromTable(&D_neo_ark_forest_zone_80182E18, 0, 0, 0);
    if (gGameSession->at4.loc.place == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_neo_ark_forest_zone_80181E30, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}

/// State 1 of the room setup task: on the first visit
/// (`gGameSession->at4.loc.place == 1`) with flag 0xBD unset, sends message
/// 0x7DB to the room's own task carrying its first payload record, then
/// advances state.
void func_neo_ark_forest_zone_8017DB40(Task* arg0)
{
    if (gGameSession->at4.loc.place == 1 && GameFlag_GetNibble(0xBD) == 0) {
        Gp_DispatchMsg(D_neo_ark_forest_zone_80181E68, 0x7DB, (s32)&D_neo_ark_forest_zone_80181E30, 0);
    }
    arg0->state = arg0->state + 1;
}

/// State 2 of the room setup task: does nothing until the task is killed. The
/// unused local reproduces the original's stack frame.
void func_neo_ark_forest_zone_8017DBAC(Task* task)
{
    char pad[0x10];
}

/// State table of the room setup task, indexed by `Task::state`.
const TaskFuncTable4 D_neo_ark_forest_zone_8017D5D8 = { {
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
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    s32            vy;
    s32            vx;
    s32            vz;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
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
            coord->flg = 0;

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
void func_neo_ark_forest_zone_8017E074(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;
    s32            prod;

    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &arg0->workm;
    tbl     = D_80111E38;
    head    = SCRATCH_HEAD_AT(scratch, u8) - sizeof(GpQuadScratch);
    SOFT_TOUCH_REG(head);
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

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
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
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
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_neo_ark_forest_zone_8017EAF0(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_forest_zone_8017EAF0(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_forest_zone_8017E6C4(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
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
void func_neo_ark_forest_zone_8017E6C4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
void func_neo_ark_forest_zone_8017EAF0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Twin smoke trail. State 0 allocates sixteen `GsCOORDINATE2`s, eight per
/// trail, and seeds them all from the two spawn offsets so each trail starts
/// collapsed on its origin. State 1 advances one slot of each trail per frame,
/// re-derives all sixteen against the view and draws them. The task frees
/// itself once its age reaches the spawn argument, and idles while the room's
/// event state is 2 or more.
void func_neo_ark_forest_zone_8017EE84(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_neo_ark_forest_zone_80182094[0].vx;
                objCoord->coord.t[1] = D_neo_ark_forest_zone_80182094[0].vy;
                objCoord->coord.t[2] = D_neo_ark_forest_zone_80182094[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_neo_ark_forest_zone_80182094[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_neo_ark_forest_zone_8018209C.vx;
                coord.coord.t[1] = D_neo_ark_forest_zone_8018209C.vy;
                coord.coord.t[2] = D_neo_ark_forest_zone_8018209C.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_neo_ark_forest_zone_8017F374(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
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
void func_neo_ark_forest_zone_8017F374(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
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

    {
        register u8* tmp asm("v0");

        tmp                = SCRATCH_HEAD(u8) - sizeof(RoomDraw03Scratch);
        blk                = (RoomDraw03Scratch*)tmp;
        SCRATCH_HEAD(void) = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
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
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
    work     = (GpEffWork*)task->spawnArg2;

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
            if (task->spawnArg1 != 0) {
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
void func_neo_ark_forest_zone_8017F9F4(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}
