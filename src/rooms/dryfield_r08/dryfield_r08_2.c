#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// 0x14-byte block `func_dryfield_r08_8017EB68` takes from `G_SCRATCH_HEAD`:
/// the projected point's depth, the two on-screen radii derived from it, the
/// GTE flag word and the projected screen position.
typedef struct {
    s32 otz;
    s32 rOuter;
    s32 rInner;
    s32 flag;
    u16 sx;
    u16 sy;
} _DryfieldR08DiscScratch;

extern s32 D_dryfield_r08_80180C24;

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues an eight-wedge gouraud disc plus four
/// inner cross wedges around the projected centre. `arg1` is a
/// signed half-extent; on-screen radii are `(s16)arg1 * 64 / otz` (outer) and
/// `(s16)arg1 * 8 / otz` (inner). `arg2` packs the colour one nibble per
/// channel - bits 8..11 red, 4..7 green, 0..3 blue, each scaled to 8 bits -
/// with bits 12..15 giving the shift for a `gDisplayState.animFrame & 1`
/// flicker added to every channel. The outer disc uses the full colour and
/// the inner cross half of it.
void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    _DryfieldR08DiscScratch* block;
    POLY_G4*                 prim;
    s32                      ang;
    s32                      t;
    s32                      t2;
    s32                      ua;
    s32                      ub;
    s32                      uc;
    s32                      frame;
    s32                      packed;
    s32                      blend;
    s32                      r;
    s32                      g;
    s32                      b;
    s32                      outer;
    s32                      inner;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = (*scratch = (u8*)*scratch - 0x14);
        block   = (_DryfieldR08DiscScratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
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
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        r   = (u8)r >> 1;
        g   = (u8)g >> 1;
        b   = (u8)b >> 1;
        ang = 0x200;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

/// Sets the skip-OT-link byte (`GpSprtCmd.field_4`) of command record
/// `arg0` + 1 in this room's sprite-table command list: non-zero leaves that
/// record's prims out of the ordering table. `arg0` is a view index below
/// 0xB; the record the table yields is larger than its `GpSprtRec` prefix,
/// so `[3].field_4` reaches the command list its tail holds there.
void func_dryfield_r08_8017F340(u8 arg0, u8 arg1)
{
    GpAreaKey* sess;
    GpSprtCmd* cmd;

    sess = &gGameSession->at4.loc;
    if ((u32)(arg0 & 0xFF) < 0xBU) {
        cmd = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1][3].field_4;
        if (arg1 & 0xFF) {
            cmd[arg0 + 1].field_4 = 1;
            return;
        }
        cmd[arg0 + 1].field_4 = 0;
    }
}

void func_dryfield_r08_8017F3B8(u8 arg0, u8 arg1)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* cmd;

    sess = &gGameSession->at4.loc;
    if ((u32)(arg0 & 0xFF) < 3U) {
        rec = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
        if ((u32)(arg0 & 0xFF) == 0U) {
            cmd = rec[1].field_4;
        } else {
            cmd = rec[2].field_4;
        }
        if (arg1 & 0xFF) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}
