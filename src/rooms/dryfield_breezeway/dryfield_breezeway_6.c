#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/dryfield_breezeway.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

void func_dryfield_breezeway_8017FF7C(Task* task)
{
    s32            mask;
    RoomEffWork*   eff;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    s32            limit;
    s32            pan;

    mask   = 1 << gGameSession->at4.loc.view;
    eff    = task->spawnArg2;
    coord  = ((TmdObject*)task->extra)->coords;
    player = ((GpActorWork*)gameGetPtrSlot(3))->extra->coords;
    if (mask & 0x18) {
        func_dryfield_breezeway_8018034C(coord, D_dryfield_breezeway_80183164, 0x600, 0x80);
    } else if (mask & 0x20) {
        func_dryfield_breezeway_80180858(coord, D_dryfield_breezeway_80183164, 0x600, 0x10);
    }
    if (Gp_State1C->eventState != 0) {
        return;
    }
    Gp_State1C->roomEffectMode = 2;
    if (GameFlag_GetNibble(0x5D) == 0) {
        if (gGameSession->at4.loc.view == 2) {
            limit            = (player->coord.t[0] - 5856) >> 7;
            eff->field_10.vx = 12000;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 3000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            if ((u16)((Gp_LcgState >> 16) % 100) < limit) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003C, coord, (s32)(Gp_LcgState >> 16) % limit + 0x40, &eff->field_10);
            }
            if ((s16)eff->field_2A == 0) {
                SndEvt_EnqueueType6(0x5216000A, 0, 0);
                eff->field_2A = 1;
            }
        } else if (gGameSession->at4.loc.view == 3) {
            eff->field_24    = 0x10;
            eff->field_10.vx = player->coord.t[0] + 0x100;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 3000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0x7F) + 0x40, &eff->field_10);
            if ((s16)eff->field_2A < 2) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (!((Gp_LcgState >> 16) & 3)) {
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(0x5216000B, pan, (s8)gpGetObjDepth(coord));
                    eff->field_2A = 2;
                }
            }
        }
    } else if (GameFlag_GetNibble(0x5D) == 1) {
        if ((s16)eff->field_2A != 0) {
            SndEvt_EnqueueType7(0x5216000A, 0);
            eff->field_2A = 0;
        }
        if ((s16)eff->field_24 != 0) {
            eff->field_24--;
            eff->field_10.vx = 16000;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 2750;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0xFF) + 0x40, &eff->field_10);
            eff->field_10.vx = 17000;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 4000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0xFF) + 0x40, &eff->field_10);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_8018034C);

void func_dryfield_breezeway_80180858(GsCOORDINATE2* coord, u8* data, s32 arg2, s32 arg3)
{
    u8*             head;
    DbwGlowScratch* block;
    POLY_G4*        prim;
    s32             pulse;
    s32             color;
    s32             half;
    s32             size;
    s32             ang;
    s32             t;
    s32             t2;
    s32             u;

    Gp_UpdateCoord(coord);
    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (DbwGlowScratch*)tmp;
    }

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0_real();
    gte_stsv(&((DbwGlowScratch*)(head - 0x18))->vec);
    block->vec.vx += coord->workm.t[0];
    block->vec.vy += coord->workm.t[1];
    block->vec.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((DbwGlowScratch*)(head - 0x18))->vec);
    gte_rtps_real();
    gte_stsxy(&((DbwGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&block->otz);
    if (((DbwGlowScratch*)(head - 0x18))->otz > 16) {
        pulse         = rsin(gDisplayState.animFrame * (s16)arg2);
        ang           = 0;
        size          = (s16)arg3;
        block->rOuter = (size * 64) / ((DbwGlowScratch*)(head - 0x18))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((DbwGlowScratch*)(head - 0x18))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, half, 0, 0);
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
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
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
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = (u16)half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
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
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
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
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_80181264);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_80181938);
