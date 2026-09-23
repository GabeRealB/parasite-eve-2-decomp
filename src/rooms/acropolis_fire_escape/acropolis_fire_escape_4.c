#include "common.h"

#include "decomp/common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1A8.h"
#include "gameplay/gameplay.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "rooms/acropolis_fire_escape.h"

extern TaskFuncTable3 RoomsShared8017d878Table;
extern s8             D_8007272D;
extern GpMsgEntry     D_acropolis_fire_escape_80181D3C[];
extern TaskDesc       D_acropolis_fire_escape_80181D64;
extern GpObj4A        D_acropolis_fire_escape_801826A8;
extern s32            D_acropolis_fire_escape_80183040;
extern s16            D_acropolis_fire_escape_80181D7C[];
extern u32            Gp_LcgState;
extern s32            D_80070F70;

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

void func_acropolis_fire_escape_8017FF7C(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    switch (task->state) {
        case 0:
            work->field_10.vx = 0xB58;
            work->field_10.vy = -0x822;
            work->field_10.vz = -0xE5;
            Gp_SpawnEff(0x6008C, coord, 0x42000, &work->field_10);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_State1C->eventState < 4) {
                if ((u8)gGameSession->at4.loc.view == 3) {
                    work->field_10.vx = 0x48F;
                    work->field_10.vy = -0x391;
                    work->field_10.vz = 0x686;
                    Gp_SpawnEff(0x6004F, coord, 0x60E, &work->field_10);
                }
                if ((u8)gGameSession->at4.loc.view == 8) {
                    work->field_10.vx = 0x48F;
                    work->field_10.vy = -0x391;
                    work->field_10.vz = 0x686;
                    Gp_SpawnEff(0x6004F, coord, 0x8000030E, &work->field_10);
                }
                if ((u8)gGameSession->at4.loc.view == 6) {
                    work->field_10.vx = -0xC1F;
                    work->field_10.vy = -0xD10;
                    work->field_10.vz = 0x8E0;
                    Gp_SpawnEff(0x6004F, coord, 0x10408, &work->field_10);
                }
                if ((u8)gGameSession->at4.loc.view == 9) {
                    work->field_10.vx = -0xC1F;
                    work->field_10.vy = -0xD10;
                    work->field_10.vz = 0x8E0;
                    Gp_SpawnEff(0x6004F, coord, 0x80010208, &work->field_10);
                }
            }
            break;
    }
}
/// Draws a flickering glow at the task's coordinate while the scene is not in
/// a cutscene and the camera is on views 2, 3 or 7. The coordinate is
/// projected through `GsWSMATRIX` and nothing is drawn unless its biased depth
/// stays beyond 0x10. Every 32 frames `field_20` picks one of four flicker
/// modes, which set the brightness `field_24` each frame: random 0/0x10/0x20/0x30,
/// the same but changing only on odd frames, a steady 0x30, or random 0/0x10.
/// Rising from 0x10 or less to 0x20 or more plays a crackle sound panned to the
/// coordinate. The glow is eight `POLY_G4` wedges around the projected point,
/// taken two steps at a time from the 16-step circle table, each lit at the
/// centre and black at the rim; it is drawn three times, at the full radius
/// with half the brightness, at half the radius with the brightness, and at an
/// eighth of the radius with four times the brightness (wrapping in a byte).
/// Every wedge takes the semi-transparent tpage of `Gp_AddTpageShift`.
void func_acropolis_fire_escape_80180154(Task* task)
{
    RoomEffWork*                    work;
    GsCOORDINATE2*                  coord;
    void**                          scratch;
    u8*                             head;
    u8*                             tmp;
    AcropolisFireEscapeGlowScratch* block;
    POLY_G4*                        prim;
    s32                             play;
    s32                             i;
    u16                             level;
    u16                             vx;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    play  = 0;
    if (Gp_State1C->eventState < 4 && ((0x46 >> (gGameSession->at4.loc.view - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        vx      = coord->workm.t[0];
        tmp     = head - 0x18;
        SOFT_USE_REG(tmp); /* keeps the carve apart from `block`, so the head store takes the copy */
        block         = (AcropolisFireEscapeGlowScratch*)tmp;
        block->vec.vx = vx;
        block->vec.vy = coord->workm.t[1];
        *scratch      = block;
        block->vec.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&block->sx);
        gte_stszotz(&block->otz);
        block->otz -= 0x20;
        if (block->otz > 0x10) {
            if (!(gDisplayState.animFrame & 0x1F)) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->field_20 = (Gp_LcgState >> 16) & 3;
            }
            level = work->field_24;
            if ((s16)work->field_24 < 0x11) {
                play = 1;
            }
            switch ((s16)work->field_20) {
                case 0:
                    Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                    work->field_24 = (Gp_LcgState >> 16) & 0x30;
                    break;
                case 1:
                    if (gDisplayState.animFrame & 1) {
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        level       = (Gp_LcgState >> 16) & 0x30;
                    }
                    work->field_24 = level;
                    break;
                case 2:
                    work->field_24 = 0x30;
                    break;
                case 3:
                    Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                    work->field_24 = (Gp_LcgState >> 16) & 0x10;
                    break;
            }
            if (play && (s16)work->field_24 >= 0x20) {
                SndEvt_EnqueueType6(0x510F0006, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            block->radius  = (((task->spawnArg1 >> 8) & 0xFF) * 0x600) / block->otz;
            block->radius2 = (((task->spawnArg1 >> 8) & 0xFF) * 0xC0) / block->otz;
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, work->field_24 >> 1, work->field_24 >> 1, work->field_24 >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->y0 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                prim->x1 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 12);
                prim->y1 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 12);
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 12);
                prim->y3 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift(prim, 1, block->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, work->field_24, work->field_24, work->field_24);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 13);
                prim->y1 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 13);
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 13);
                prim->y3 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift(prim, 1, block->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (u8)work->field_24 * 4, (u8)work->field_24 * 4, (u8)work->field_24 * 4);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 15);
                prim->y0 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i]) >> 15);
                prim->x1 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 15);
                prim->y1 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 15);
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 15);
                prim->y3 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 15);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift(prim, 1, block->otz);
            }
        }
        *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x18;
    }
}

void func_acropolis_fire_escape_80180B20(Task* task)
{
    u8*                             head;
    u8*                             raw;
    AcropolisFireEscapeGlowScratch* blk;
    POLY_G4*                        prim;
    LINE_G3*                        line;
    GsCOORDINATE2*                  coord;
    void*                           mem;
    u16                             vz;
    s32                             i;
    s32                             pulse;
    s32                             level;
    s32                             height;
    s16                             amp;
    s16                             flip;
    s32                             ampSi;
    s32                             ampHalf;
    u8                              red;
    u8                              cyan;
    s32                             z;
    s32                             shift;
    u32                             depth;
    u32                             tag;
    u_long*                         ot;

    coord = ((TmdObject*)task->extra)->coords;
    mem   = task->spawnArg2;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x18;
    SOFT_TOUCH_REG(raw);
    blk                     = (AcropolisFireEscapeGlowScratch*)raw;
    blk->vec.vx             = *(u16*)&coord->workm.t[0];
    blk->vec.vy             = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = blk;
    blk->vec.vz             = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisFireEscapeGlowScratch*)(head - 0x18))->vec);
    gte_rtps_real();
    gte_stsxy(&((AcropolisFireEscapeGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&blk->otz);
    if (((AcropolisFireEscapeGlowScratch*)(head - 0x18))->otz >= 0x11) {
        pulse  = D_80070F70;
        pulse *= task->spawnArg1 & 0xFF;
        flip   = (task->spawnArg1 >> 16) & 1;
        if (pulse & 0x80) {
            level = ~pulse & 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        amp   = level * 2;
        level = task->spawnArg1;
        if (level < 0) {
            height       = (level >> 8) & 0xFF;
            blk->radius  = (height << 10) / blk->otz;
            blk->radius2 = (((task->spawnArg1 >> 8) & 0xFF) << 7) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                ampSi          = amp;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (ampSi * (flip ^ 1)) >> 1, (flip * ampSi) >> 1, (flip * ampSi) >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, ampSi * (flip ^ 1), flip * ampSi, flip * ampSi);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            ampHalf = amp >> 1;
            for (i = 2; i < 0x10; i += 8) {
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    red  = ampHalf * (flip ^ 1);
                    cyan = flip * ampHalf;
                    setRGB2(prim, red, cyan, cyan);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                    prim->y0 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i - 4]) >> 12);
                    prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 11);
                    prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i]) >> 11);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 8]) >> 12);
                    prim->y3 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                    shift    = gDisplayState.otDepthShift;
                    depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                    __asm__("" : "+r"(depth) : "r"(shift), "m"(gDisplayState.otDepthShift));
                    setaddr(prim, getaddr((u_long*)(depth + (s32)gGpuCurrentOt)));
                    ot  = (u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt);
                    tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                    *ot = tag;
                    z   = blk->otz;
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG_USE(z, tag);
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, cyan, cyan);
                } while (0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 8]) >> 12);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 0xC]) >> 13);
                prim->y3 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 8]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                z = blk->otz;
                __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_fire_escape_80181D7C[i]));
                __asm__("" : "+r"(prim) : "r"(z), "r"(cyan));
                Gp_AddTpageShift((P_TAG*)prim, 1, z);
            }
        } else {
            blk->radius  = (((level >> 8) & 0xFF) << 9) / blk->otz;
            blk->radius2 = (((task->spawnArg1 >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, amp * (flip ^ 1), flip * amp, flip * amp);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->radius;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->radius;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->radius2) + blk->radius2 * (i + i);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (task->spawnArg1 & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)line + sizeof(LINE_G3);
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, amp * (flip ^ 1), flip * amp, flip * amp);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->radius * (i * 3 - 1);
                    line->y0 = blk->sy - blk->radius2 * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->radius * (i * 3 - 1);
                    line->y2 = blk->sy + blk->radius2 * (i + 1);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    Gp_ReleaseState1CMem(mem, task);
}
