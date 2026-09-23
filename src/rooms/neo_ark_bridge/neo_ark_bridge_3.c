#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;
extern u32 Gp_LcgState;

extern SVECTOR D_neo_ark_bridge_80181F58;
extern SVECTOR D_neo_ark_bridge_80181F60;
extern SVECTOR D_neo_ark_bridge_80181F68;

void func_neo_ark_bridge_8017EB08(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Bridge effect task tick. State 0 installs the five bridge effect ids
/// (0x601E1, then 0x601FD / 0x60219 / 0x6017A / 0x6017B) and advances. State 1
/// only acts while `Gp_GetViewIndex()` reports the two side views 5 or 6 and no
/// state-1C flag is set: two LCG rolls each spawn effect 0x60070 at
/// `D_neo_ark_bridge_80181F60` / `D_neo_ark_bridge_80181F68` on a 1-in-4, then
/// the sprite at `D_neo_ark_bridge_80181F58` is drawn for 0x600 frames.
void func_neo_ark_bridge_8017E954(Task* arg0)
{
    s32 view;
    u32 rnd;
    u32 rndSpawn;
    u32 rndSpawn2;

    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601E1;
            D_8011572C  = 0x601FD;
            D_80115750  = 0x60219;
            D_8011574C  = 0x6017A;
            D_80115738  = 0x6017B;
            arg0->state = 1;
            /* fallthrough */
        case 1:
            view = Gp_GetViewIndex() & 0xFF;
            if (view < 7) {
                if (view >= 5) {
                    if (Gp_State1C->eventState == 0) {
                        rnd         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 16) & 3) == 0) {
                            rndSpawn    = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rndSpawn;
                            Gp_SpawnEff(0x60070, 0, ((rndSpawn >> 16) & 0x11FF) | 0x22200,
                                        &D_neo_ark_bridge_80181F60);
                        }
                        rnd         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rnd;
                        if (((rnd >> 16) & 3) == 0) {
                            rndSpawn2   = Gp_LcgState * 5 + 0x71357911;
                            Gp_LcgState = rndSpawn2;
                            Gp_SpawnEff(0x60070, 0, ((rndSpawn2 >> 16) & 0x11FF) | 0x22200,
                                        &D_neo_ark_bridge_80181F68);
                        }
                    }
                    func_neo_ark_bridge_8017EB08(&D_neo_ark_bridge_80181F58, 0x600, 0xC0);
                }
            }
            break;
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, if
/// the resulting OTZ is at least 0x11, queues two gouraud `POLY_G4` diamonds
/// and two gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1`
/// scales `gDisplayState.animFrame` into `rsin` so the lit vertex pulses on red
/// as `rsin(...) / 34 + 0x78`.
void func_neo_ark_bridge_8017EB08(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
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

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
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
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
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
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0xC;
}
