#include "common.h"
#include "rooms/room_common.h"
#include "gameplay/D4.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// Ambient effect emitter positions for the airlock, selected by view index.
/// `D_shelter_1f_airlock_8017E4BC` / `_8017E4C4` / `_8017E4D4` are successive
/// labels into one contiguous run of `SVECTOR`s, so the per-view lists overlap.
extern SVECTOR D_shelter_1f_airlock_8017E4BC[];
extern SVECTOR D_shelter_1f_airlock_8017E4C4[];
extern SVECTOR D_shelter_1f_airlock_8017E4D4[];

void func_shelter_1f_airlock_8017D8A8(SVECTOR* pos, s32 arg1, s32 arg2);

void Room_Draw29(SVECTOR* pos, s32 arg1, s32 arg2);

void func_shelter_1f_airlock_8017D6D0(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            Room_Draw29(&D_shelter_1f_airlock_8017E4C4[0], 0x200, 0x111);
            Room_Draw29(&D_shelter_1f_airlock_8017E4C4[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[3], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[5], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[7], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[9], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[11], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[17], 0x180, 0x1011);
            break;
        case 4:
            Room_Draw29(&D_shelter_1f_airlock_8017E4BC[0], 0x200, 0x111);
            Room_Draw29(&D_shelter_1f_airlock_8017E4BC[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[4], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[6], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[8], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[10], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[12], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[14], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[16], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[18], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[20], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[22], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[24], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[26], 0x180, 0x1011);
            break;
        case 5:
            Room_Draw29(&D_shelter_1f_airlock_8017E4D4[0], 0x200, 0x200);
            break;
    }
}

/// Projects two adjacent positions and draws a colored glow between them.
/// The RGB nibbles in arg2 gain an alternating frame contribution whose shift
/// is selected by bits 12..15. Uses the two-point RoomDraw08Scratch layout.
void func_shelter_1f_airlock_8017D8A8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                conn;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        u8* tmp;
        *scratch = (tmp = head - 0x1C);
        block    = (RoomDraw08Scratch*)tmp;
        SOFT_TOUCH_REG(block);
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps_real();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &Display_State;
            SOFT_BARRIER();
            ang    = (s16)ang;
            packed = arg2 << 16;
            blend  = (ds->field_8 & 1) << (packed >> 28);
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend + tr;
            g      = blend + tg;
            b      = blend + ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)Gpu_PrimCursor;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
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
                    addPrim((u_long*)(((((u32)block->otz0 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)Gpu_PrimCursor;
                    USE_REG(prim);
                    t    = ang - angStart;
                    t  <<= 1;
                    conn = angStart + t;
                    TOUCH_REG(conn);
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << Display_State.field_128) >> 2) & 0xFFC) +
                                      (s32)Gpu_CurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)Gpu_PrimCursor;
                    t              = t3;
                    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
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
                    addPrim((u_long*)(((((u32)block->otz1 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SOFT_BARRIER();
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
