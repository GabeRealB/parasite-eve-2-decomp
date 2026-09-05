#include "common.h"
#include "main/task.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/stream.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "gameplay/gameplay.h"
#include "rooms/acropolis_fountain.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s16 D_80071076;
extern s32 D_80070F70;

extern u8    D_acropolis_fountain_80183BB0;
extern Task* D_acropolis_fountain_80183BB4;

extern GpMsgEntry D_acropolis_fountain_8017E764[];

extern GpObj4A D_acropolis_fountain_8017E7A4;
extern GpObj4A D_acropolis_fountain_8017FB3C;

extern SVECTOR  D_acropolis_fountain_8017E7F0;
extern s16      D_acropolis_fountain_8017E7F8;
extern TaskDesc D_acropolis_fountain_8017E7FC;

void func_acropolis_fountain_8017DA1C(void);
void func_acropolis_fountain_8017E15C(Task* task, s32 view);

void func_acropolis_fountain_8017DA1C(void)
{
    Gp_UnlinkObj4A(0, &D_acropolis_fountain_8017FB3C);
    D_acropolis_fountain_8017E7A4.field_8 = &Gfx_ViewCoord;
    Gp_LinkObj4A(0, &D_acropolis_fountain_8017E7A4);
    D_acropolis_fountain_8017E7A4.field_4A |= 0x40;
}

void func_acropolis_fountain_8017DA78(void)
{
    Task_Spawn(2, 0xE, 0, 0);
}

void func_acropolis_fountain_8017DAA4(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    slot         = Game_GetPtrSlot(3);
    msg.field_10 = 0;
    msg.field_12 = 0x800;
    msg.field_14 = 0;
    Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_fountain_8017DB00(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_acropolis_fountain_8017DB54(Task* arg0)
{
    s32   args[2];
    Task* slot;

    slot    = Game_GetPtrSlot(3);
    args[0] = 0;
    args[1] = 1;
    Gp_DispatchMsg(slot, 0x3EF, (s32)args, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_fountain_8017DBAC(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_acropolis_fountain_8017DC00(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    Game_GetPtrSlot(3);
    slot        = Game_GetPtrSlot(3);
    msg.field_0 = 0xA27;
    msg.field_4 = -0xC8;
    msg.field_8 = -0x17A6;
    Gp_DispatchMsg(slot, 0x3F2, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_fountain_8017DC6C(Task* arg0)
{
    Task* temp_v0;

    temp_v0 = Game_GetPtrSlot(3);
    if (Gp_DispatchMsg(temp_v0, 0x3F0, 0, 0) == 0) {
        Gp_DispatchMsg(temp_v0, 0x3F1, 0, 0);
        Task_Kill(arg0);
    }
}

/// Six-state dispatcher of the fountain cutscene task; the handler table is
/// built on the stack from the overlay's rodata block.
void func_acropolis_fountain_8017DCD4(Task* arg0)
{
    TaskFunc states[6] = {
        func_acropolis_fountain_8017DAA4,
        func_acropolis_fountain_8017DB00,
        func_acropolis_fountain_8017DB54,
        func_acropolis_fountain_8017DBAC,
        func_acropolis_fountain_8017DC00,
        func_acropolis_fountain_8017DC6C,
    };

    states[arg0->state](arg0);
}

/// Draws the fountain's water-spray sprite for the current frame. The task's
/// coordinate is refreshed and projected through `GsWSMATRIX` into a
/// `G_SCRATCH_HEAD` block; the resulting screen point becomes the centre of a
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x4382, the 0x28x0x27 cell at
/// u 0x50) whose half-extent is `0x4E00 / otz`, so the spray shrinks with
/// distance and is dropped entirely inside `otz` 0x11. The grey level
/// alternates between 0x40 and 0x50 with the frame counter's low bit, which
/// makes the spray flicker. Only the eight camera views in the `0x1040C0` mask
/// see the fountain, and the whole draw is skipped once `Gp_State1C::field_4`
/// reaches 4 (the room is fading out).
void func_acropolis_fountain_8017DD44(Task* task)
{
    void**                                  scratch;
    u8*                                     head;
    AcropolisFountainSprayScratch*          blk;
    register AcropolisFountainSprayScratch* p asm("a0");
    GsCOORDINATE2*                          coord;
    POLY_FT4*                               prim;
    s16                                     x;
    s16                                     y;
    u16                                     vz;
    s32                                     level;

    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 < 4 && ((0x1040C0 >> ((u8)Game_Session->field_4 - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch     = (void**)G_SCRATCH_HEAD;
        head        = *scratch;
        blk         = (AcropolisFountainSprayScratch*)(head - 0x14);
        blk->pos.vx = *(u16*)&coord->workm.t[0];
        blk->pos.vy = *(u16*)&coord->workm.t[1];
        vz          = *(u16*)&coord->workm.t[2];
        *scratch    = blk;
        p           = blk;
        blk->pos.vz = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&((AcropolisFountainSprayScratch*)(head - 0x14))->pos);
        gte_rtps_real();
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&((AcropolisFountainSprayScratch*)(head - 0x14))->sx);
        gte_stszotz(&p->otz);
        if (((AcropolisFountainSprayScratch*)(head - 0x14))->otz >= 0x11) {
            level       = (((u8)Display_State.field_8 & 1) << 4) + 0x40;
            prim->tpage = 0x2B;
            prim->clut  = 0x4382;
            prim->u0    = 0x50;
            prim->v0    = 0;
            prim->u1    = 0x77;
            prim->v1    = 0;
            prim->u2    = 0x50;
            prim->v2    = 0x27;
            prim->u3    = 0x77;
            prim->v3    = 0x27;
            prim->r0    = level;
            prim->g0    = level;
            prim->b0    = level;
            prim->code |= 2;
            blk->half   = 0x4E00 / ((AcropolisFountainSprayScratch*)(head - 0x14))->otz;
            x           = blk->sx - (u16)blk->half;
            prim->x2    = x;
            prim->x0    = x;
            x           = blk->sx + (u16)blk->half;
            prim->x3    = x;
            prim->x1    = x;
            y           = blk->sy - (u16)blk->half;
            prim->y1    = y;
            prim->y0    = y;
            y           = blk->sy + (u16)blk->half;
            prim->y3    = y;
            prim->y2    = y;
            addPrim((u_long*)(((((u32)((AcropolisFountainSprayScratch*)(head - 0x14))->otz
                                 << Display_State.field_128) >>
                                2) &
                               0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    }
}

void func_acropolis_fountain_8017E014(Task* task)
{
    AcropolisFountainSplash* splash;
    GsCOORDINATE2*           coord;
    s32                      view;
    s32                      one;
    s32                      mask;
    s32                      bit;
    s16                      id;

    coord  = ((TmdObject*)task->extra)->field_8;
    splash = task->spawnArg2;
    view   = Gp_GetViewIndex();
    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60088, coord, 0, &D_acropolis_fountain_8017E7F0);
            splash->viewIndex = view & 0xFF;
            task->state       = 1;
            /* fallthrough */
        case 1:
            mask = 0x100FE;
            bit  = 1 << (splash->viewIndex - 1);
            if (bit & mask) {
                Task_SpawnFromTable(&D_acropolis_fountain_8017E7FC, 0, 0, 0);
            }
            task->state = 2;
            break;
        case 2:
            one = 1;
            id  = splash->viewIndex;
            bit = one << (id - 1);
            if (id != (view & 0xFF)) {
                if (bit & 0x100FE) {
                    Task_SpawnFromTable(&D_acropolis_fountain_8017E7FC, 1, 0, 0);
                }
                splash->viewIndex = (u8)view;
                task->state       = 1;
            }
            break;
    }
}

void func_acropolis_fountain_8017E15C(Task* task, s32 view)
{
    AcropolisFountainSndWork* work;
    CdCmdQueue*               queue;
    u16                       frame;

    queue = &CdCmd_Queue;
    work  = (AcropolisFountainSndWork*)task->idMap;
    switch (work->state) {
        case 0:
            frame = queue->field_1EA;
            if (frame >= 0xF0) {
                if (D_acropolis_fountain_8017E7F8 != 0) {
                    SndEvt_EnqueueType7(0x51080001, 0x14);
                    D_acropolis_fountain_8017E7F8 = 0;
                }
            } else if ((u16)(frame - 0xF) < 0xE2) {
                if (work->started == 0 || D_acropolis_fountain_8017E7F8 == 0) {
                    work->started = 1;
                    work->state   = work->state + 1;
                }
            }
            break;

        case 1:
            work->state = 2;
            break;

        case 2:
            switch ((u16)view) {
                case 2:
                case 3:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(0x51080001, 7, 2);
                    } else {
                        SndEvt_EnqueueType6(0x51080001, 7, 2);
                    }
                    break;
                case 4:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(0x51080001, 0, 0);
                    } else {
                        SndEvt_EnqueueType6(0x51080001, 0, 0);
                    }
                    break;
                case 5:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(0x51080001, -7, 2);
                    } else {
                        SndEvt_EnqueueType6(0x51080001, -7, 2);
                    }
                    break;
                case 6:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(0x51080001, -7, 0);
                    } else {
                        SndEvt_EnqueueType6(0x51080001, -7, 0);
                    }
                    break;
                case 7:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(0x51080001, 0, 2);
                    } else {
                        SndEvt_EnqueueType6(0x51080001, 0, 2);
                    }
                    break;
                case 8:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(0x51080001, 7, 3);
                    } else {
                        SndEvt_EnqueueType6(0x51080001, 7, 3);
                    }
                    break;
            }
            D_acropolis_fountain_8017E7F8 = 1;
            work->state                   = 0;
            break;
    }
}

void func_acropolis_fountain_8017E3D4(Task* task)
{
    CdCmdQueue* queue;
    CdCmdQueue* queue2;
    StreamSlot* slot;
    StreamSlot* slot2;
    SPRT*       p;
    DR_TPAGE*   dr;
    GBytes8     key;
    GBytes8     key2;
    s16         view;
    s32         ot;
    u16         count;
    u32         tpage;

    D_acropolis_fountain_80183BB4 = task;
    queue                         = &CdCmd_Queue;
    if (Game_Session->field_5 != 1) {
        return;
    }
    switch (task->state) {
        case 0:
            task->idMap = Mem_Calloc(4, 0);
            if (task->idMap == NULL) {
                Task_Kill(task);
                return;
            }
            Mem_Set(task->idMap, 0, 4);
            task->state = task->state + 1;
            break;

        case 1:
            view = Gp_FindViewIndex((u8)Game_Session->field_4);
            func_acropolis_fountain_8017E15C(task, (u16)view);
            switch ((u16)view) {
                case 3:
                case 5:
                    if (queue->field_20A != 0) {
                        return;
                    }
                    Game_Session->field_4E = 1;
                    key                    = ((SessionBytesAt4*)Game_Session)->field_4;
                    key.data[0]            = view;
                    slot                   = Stream_GetSlot(Stream_FindSlotByKey(key.data) & 0xFFFF);

                    p              = (SPRT*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
                    setlen(p, 4);
                    setcode(p, 0x65);
                    p->u0 = 0;
                    p->v0 = 0;
                    if ((u16)view == 3) {
                        ot    = 0x23;
                        p->x0 = 0x70;
                        p->y0 = 8;
                    } else if ((u16)view == 5) {
                        ot    = 0x1D;
                        p->x0 = -0xA0;
                        p->y0 = 0x2A;
                    }
                    dr   = Gpu_PrimCursor;
                    p->w = slot->field_12;
                    p->h = slot->field_14;
                    addPrim(&Gpu_CurrentOt[ot], p);

                    Gpu_PrimCursor = dr + 1;
                    setlen(dr, 1);
                    tpage       = (u32)(slot->field_18 & 0x100) >> 4;
                    dr->code[0] = tpage | (((u32)(slot->field_16 & 0x3FF) >> 6) | 0x100) |
                                  ((slot->field_18 & 0x200) * 4) | 0xE1000000;
                    addPrim(&Gpu_CurrentOt[ot], dr);
                    break;

                case 2:
                case 8:
                    if (D_80070F70 & 1) {
                        queue2            = &CdCmd_Queue;
                        key2              = ((SessionBytesAt4*)Game_Session)->field_4;
                        key2.data[0]      = Gp_FindViewIndex(4);
                        slot2             = Stream_GetSlot(Stream_FindSlot(key2.data, 0, 1) & 0xFFFF);
                        count             = queue2->field_1EA + 1;
                        queue2->field_1EA = count;
                        if (count >= slot2->field_1A - 0xA) {
                            queue2->field_1EA = 1;
                        }
                    }
                    break;
            }
            break;
    }
}

void func_acropolis_fountain_8017E72C(Task* arg0)
{
    Task_Kill(D_acropolis_fountain_80183BB4);
    Task_Kill(arg0);
}
