#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/room_effects.h"
#include "main/fs.h"

static void func_dryfield_night_breezeway_8017D634(Task* task);
static void func_dryfield_night_breezeway_8017D678(Task* task);

/// The room's message table: 0x13EE, 0x13F1, 0x13EF and 0x13F0 to their
/// handlers, terminated by 0x7FFFFFFF.
extern GpMsgEntry D_dryfield_night_breezeway_8017E67C[];

/// The anchor points of the room's lights: one run of eight `SVECTOR`s. The
/// draw below reaches some points through their own address and others by
/// indexing from an earlier one, so the run carries three names.
extern SVECTOR D_dryfield_night_breezeway_8017E6A4[];
extern SVECTOR D_dryfield_night_breezeway_8017E6AC[];
extern SVECTOR D_dryfield_night_breezeway_8017E6C4;

/// The room's event task states: open the message table, idle, then kill the
/// task.
static const TaskFuncTable3 D_dryfield_night_breezeway_8017D5C4 = {
    { func_dryfield_night_breezeway_8017D634, func_dryfield_night_breezeway_8017D678, taskKill },
};

/// The room's 0x13F1 message handler: answers 0 without looking at the
/// message.
s32 func_dryfield_night_breezeway_8017D5D0(void)
{
    return 0;
}

/// The room's 0x13EE message handler: copies the incoming location onto the
/// outgoing record and answers 1.
s32 func_dryfield_night_breezeway_8017D5D8(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// The room's 0x13F0 message handler: when `arg2` is 1, spawns the gameplay
/// event task (`Gp_SpawnIfCapIdle(1, 1)`) unless the cap interpreter is busy.
/// Always answers 0.
s32 func_dryfield_night_breezeway_8017D600(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(1, 1);
    }
    return 0;
}

/// The room's 0x13EF message handler: answers 0 without looking at the
/// message.
s32 func_dryfield_night_breezeway_8017D62C(void)
{
    return 0;
}

/// State 0 of the room's event task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to the
/// next state.
static void func_dryfield_night_breezeway_8017D634(Task* task)
{
    task->msgTable = D_dryfield_night_breezeway_8017E67C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing.
static void func_dryfield_night_breezeway_8017D678(Task* task)
{
}

/// The room's event task: copies the three-state table onto the stack and
/// calls the entry for the task's current state.
void func_dryfield_night_breezeway_8017D680(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_breezeway_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws a pulsing red star at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`; nothing is drawn when the projection flags an error.
/// Two gouraud `POLY_G4` halves of a diamond and two `LINE_G3` diagonals
/// surround the projected point, with radius `(s16)arg2 * 32` over its depth.
/// The lit vertices take a red of `rsin(animFrame * arg1) / 34 + 0x78`, so
/// `arg1` sets the pulse rate. The work block lives on the scratchpad stack.
static void func_dryfield_night_breezeway_8017D6D8(SVECTOR* arg0, s16 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    block = SCRATCH_PUSH(RoomDraw13Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * arg1);
        radius        = ((s16)arg2 * 32) / block->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, pulse, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - block->radius) + (block->radius * twice);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, pulse, 0, 0);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Draws a light shaft between the two world points `arg0[0]` and `arg0[1]`:
/// a fan of gouraud wedges around each projected point, joined by wedges
/// spanning the two, the sweep oriented along the screen-space line between
/// them. Each radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is
/// drawn unless both points project. The lit vertices take a brightness that
/// flickers with the frame counter.
static void func_dryfield_night_breezeway_8017DB4C(SVECTOR* arg0, s32 arg1)
{
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1 = arg0 + 1;
    SCRATCH_PUSH(OverlayPointPairScratch);
    block = SCRATCH_HEAD(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd    = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP(OverlayPointPairScratch);
}

/// Draws a textured, semi-transparent sprite at the world point `arg0`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn when the projection
/// flags an error. `arg1` picks the 40-texel column of tpage 0x2B and the clut
/// `(arg1 & 0x3F) | 0x4380`; the half-size on screen is `(s16)arg2 * 39` over
/// the depth. The grey tint flickers between 0x20 and 0x30 with the display
/// frame counter. The work block lives on the scratchpad stack.
static void func_dryfield_night_breezeway_8017E334(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// The room's light draw: sets `Gp_State1C->roomEffectMode` to 2, then draws
/// the lights the current camera view (`gGameSession->at4.loc.view`) can see.
/// View 2 draws a sprite and a beam; view 3 draws a beam and then everything
/// view 4 draws, a pulsing star and a second beam. Other views draw nothing.
static void func_dryfield_night_breezeway_8017E5BC(void)
{
    Gp_State1C->roomEffectMode = 2;
    switch (gGameSession->at4.loc.view) {
        case 2:
            func_dryfield_night_breezeway_8017E334(&D_dryfield_night_breezeway_8017E6AC[0], 2, 0x400);
            func_dryfield_night_breezeway_8017DB4C(&D_dryfield_night_breezeway_8017E6AC[5], 0x180);
            break;
        case 3:
            func_dryfield_night_breezeway_8017DB4C(&D_dryfield_night_breezeway_8017E6C4, 0x180);
            /* fallthrough */
        case 4:
            func_dryfield_night_breezeway_8017D6D8(&D_dryfield_night_breezeway_8017E6A4[0], 0x600, 0x80);
            func_dryfield_night_breezeway_8017DB4C(&D_dryfield_night_breezeway_8017E6A4[2], 0x180);
            break;
    }
}
