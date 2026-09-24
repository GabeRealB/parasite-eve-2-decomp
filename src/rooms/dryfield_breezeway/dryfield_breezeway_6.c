#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/dryfield_breezeway.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_dryfield_breezeway_80181938(Task* task, u8* color);

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

/// Draws a red light shaft at a point. `data` is rotated by
/// `coord`'s `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` into a 0x14-byte `G_SCRATCH_HEAD` block; nothing is drawn when
/// `otz` is 0x10 or less. Two gouraud `POLY_G4` halves of half width
/// `(s16)arg3 * 32 / otz` and two `LINE_G3` diagonals meet at the projected
/// point, whose vertex pulses red as `rsin(animFrame * arg2) / 34 + 0x78`.
void func_dryfield_breezeway_8018034C(GsCOORDINATE2* coord, u8* data, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               i;
    s32               color;
    s32               pulse;
    s32               twice;
    s32               t;
    s32               t2;

    Gp_UpdateCoord(coord);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomShaftScratch*)(head - 0x14);

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0_real();
    gte_stsv(&((RoomShaftScratch*)(head - 0x14))->vec);
    block->vec.vx = *(u16*)&block->vec.vx + *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&block->vec.vy + *(u16*)&coord->workm.t[1];
    block->vec.vz = *(u16*)&block->vec.vz + *(u16*)&coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse            = rsin(gDisplayState.animFrame * (s16)arg2);
        i                = 0;
        block->halfWidth = ((s16)arg3 << 5) / ((RoomShaftScratch*)(head - 0x14))->otz;
        color            = pulse / 34 + 0x78;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - *(u16*)&block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + *(u16*)&block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            twice                          = i << 1;
            prim->y1                       = (block->sy - *(u16*)&block->halfWidth) + block->halfWidth * twice;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
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
            setRGB1(line, color, 0, 0);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

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

/// Per-frame update for a bouncing sprite particle drawn by
/// `func_dryfield_breezeway_80181938`. The first frame resets the model's
/// rotation, rolls a frame period, start frame, angle and spin from the LCG,
/// picks a random direction in `field_10` when none was supplied, and
/// normalises it. Afterwards it steps along `field_10` at speed `field_24`
/// and tests the step with `func_800DE7CC`; a hit undoes the step, blends the
/// direction with the returned vector, halves speed and spin, and spawns
/// effect 0x60054 while the particle is young, settling into state 2 once hits
/// come close together at low speed. A miss adds `0x5000 / field_24` to the
/// direction's y component. Age is `field_22`: the sprite fades from 30 to 60 and
/// is then released. The age does not advance while an event is running.
void func_dryfield_breezeway_80181264(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    MATRIX*        m;
    SVECTOR        delta;
    SVECTOR        dir;
    SVECTOR        pos;
    u8             color[3];

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->field_22++;

    switch (task->state) {
        case 0:
            m                  = &coord->coord;
            *(s32*)&m->m[0][0] = 0x1000;
            *(s32*)&m->m[0][2] = 0;
            *(s32*)&m->m[1][1] = 0x1000;
            *(s32*)&m->m[2][0] = 0;
            m->m[2][2]         = 0x1000;
            work->field_18     = (u16)task->spawnArg1 & 0xFFF;
            work->field_24     = 0x50;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1A     = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_20     = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1C     = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_28     = 0x200 - (((u32)Gp_LcgState >> 16) & 0x3FF);
            if ((work->field_10.vx | work->field_10.vy | work->field_10.vz) == 0) {
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vy = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                gte_SetRotMatrix(&work->field_8->coord);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&work->field_10);
            }
            VectorNormalSS(&work->field_10, &work->field_10);
            coord->flg  = 0;
            task->state = 1;
            break;
        case 1:
            if (Gp_State1C->eventState != 0) {
                work->field_22--;
            } else {
                work->field_1C += work->field_28;
                if ((s16)work->field_1A != 0 && (s16)work->field_22 % (s16)work->field_1A == 0) {
                    work->field_20++;
                }
                gte_lddp(work->field_24);
                gte_ldsv(&work->field_10);
                gte_gpf12_real();
                gte_stsv(&delta);
                coord->coord.t[0] += delta.vx;
                coord->coord.t[1] += delta.vy;
                coord->coord.t[2] += delta.vz;
                coord->flg         = 0;
                gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                gte_ldv0(&delta);
                gte_rtv0_real();
                gte_stsv(&dir);
                pos.vx  = coord->workm.t[0];
                pos.vy  = coord->workm.t[1];
                pos.vz  = coord->workm.t[2];
                dir.vx += pos.vx;
                dir.vy += pos.vy;
                dir.vz += pos.vz;
                if (func_800DE7CC(&dir, &pos, &dir, &pos) == 1) {
                    coord->coord.t[0] -= delta.vx;
                    coord->coord.t[1] -= delta.vy;
                    coord->coord.t[2] -= delta.vz;
                    work->field_10.vx  = (pos.vx >> 1) + (work->field_10.vx >> 1);
                    work->field_10.vy  = pos.vy + (work->field_10.vy >> 1);
                    work->field_10.vz  = (pos.vz >> 1) + (work->field_10.vz >> 1);
                    VectorNormalSS(&work->field_10, &work->field_10);
                    work->field_24 = (s16)work->field_24 >> 1;
                    work->field_28 = (s16)work->field_28 >> 1;
                    gte_lddp(work->field_24);
                    gte_ldsv(&work->field_10);
                    gte_gpf12_real();
                    gte_stsv(&delta);
                    coord->coord.t[0] += delta.vx;
                    coord->coord.t[1] += delta.vy;
                    coord->coord.t[2] += delta.vz;
                    if ((s16)work->field_22 < 60) {
                        Gp_SpawnEff(0x60054, coord, (s16)work->field_18 + 0x2100, NULL);
                    }
                    if ((s16)work->field_22 - (s16)work->field_2A < 8 && (s16)work->field_24 < 0x20) {
                        task->state = 2;
                    } else {
                        work->field_2A = work->field_22;
                    }
                } else if ((s16)work->field_24 > 0) {
                    work->field_10.vy += 0x5000 / (s16)work->field_24;
                }
            }
            if ((s16)work->field_22 < 30) {
                func_dryfield_breezeway_80181938(task, NULL);
            } else if ((s16)work->field_22 < 60) {
                color[0] = color[1] = color[2] = (60 - (s16)work->field_22) * 4;
                func_dryfield_breezeway_80181938(task, color);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
        case 2:
            if (Gp_State1C->eventState != 0) {
                work->field_22--;
            }
            if ((s16)work->field_22 < 30) {
                func_dryfield_breezeway_80181938(task, NULL);
            } else if ((s16)work->field_22 < 60) {
                color[0] = color[1] = color[2] = (60 - (s16)work->field_22) * 4;
                func_dryfield_breezeway_80181938(task, color);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws `task`'s effect as a camera-facing 16x16 `POLY_FT4` sprite at the
/// translation of its model's coordinate, through a 0x1C-byte `G_SCRATCH_HEAD`
/// block. Nothing is drawn when the projection flags a negative result. The
/// frame is `field_20 & 7` along row 0xF0 of texture page 0x2B, and the quad's
/// half extent is `field_18 * 23 / otz`, rotated by the angle in `field_1C`.
/// A non-null `color` tints the sprite and makes it semi-transparent.
void func_dryfield_breezeway_80181938(Task* task, u8* color)
{
    TmdObject*        extra = (TmdObject*)task->extra;
    RoomEffWork*      work  = task->spawnArg2;
    void**            scratch;
    GsCOORDINATE2*    coord;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    coord                                      = (GsCOORDINATE2*)extra->coords;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
    __asm__("move %0,%1" : "=r"(block) : "r"(vecp));
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x43C0;
        prim->u0    = (work->field_20 & 7) * 16;
        prim->v0    = 0xF0;
        prim->u1    = (work->field_20 & 7) * 16 + 0xF;
        prim->v1    = 0xF0;
        prim->u2    = (work->field_20 & 7) * 16;
        prim->v2    = 0xFF;
        prim->u3    = (work->field_20 & 7) * 16 + 0xF;
        prim->v3    = 0xFF;
        block->dx   = ((((s16)work->field_18 * 0x17) / block->otz) * rsin((s16)work->field_1C)) >> 12;
        block->dy   = ((((s16)work->field_18 * 0x17) / block->otz) * rcos((s16)work->field_1C)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = ((((s16)work->field_18 * 0x17) / block->otz) * rsin((s16)work->field_1C + 0x400)) >> 12;
        block->dy   = ((((s16)work->field_18 * 0x17) / block->otz) * rcos((s16)work->field_1C + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
