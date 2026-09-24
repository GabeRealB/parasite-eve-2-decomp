#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_storeroom.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 D_80115730;
extern s32 Gp_LcgState;

extern s16 D_shelter_b1_storeroom_80184B44[][3];

/// Draws a gouraud flare at the coordinate's world position: a two-layer disc
/// of eight wedges and a ring of star points, sized by `arg1` over depth,
/// coloured from `arg2` at the centre and fading to black outward.
void func_shelter_b1_storeroom_801823A0(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
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
    gte_rtps_real();
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Effect task of a glowing disc attached to its parent at the work's
/// position. It grows the disc, spawning effect `D_80115730` every fourth tick
/// at a random joint of the player's model; later adds a flickering second
/// disc; and finally drifts the disc away while it fades inside an expanding
/// ring, then releases itself. The spawn argument picks the disc's colour
/// shifts from `D_shelter_b1_storeroom_80184B44`.
void func_shelter_b1_storeroom_80182D60(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][2];
            func_shelter_b1_storeroom_80183B84(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][2];
            func_shelter_b1_storeroom_80183B84(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b1_storeroom_80183B84(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b1_storeroom_80184B44[arg0->spawnArg1][2];
            func_shelter_b1_storeroom_80183B84(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b1_storeroom_80183760(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}
