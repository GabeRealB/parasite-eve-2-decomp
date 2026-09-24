#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actors_shared_80149e54.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern s16 D_800691CA;
extern u8  D_80070F87;
extern s32 D_80070F70;
extern s8  D_801153F1;
extern u8  D_801153F4;
extern u32 Gp_LcgState;

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_shelter_b6_corridor_8017EF20;

/// The room task's message records.
extern GpMsgEntry D_shelter_b6_corridor_8017EF24[];

extern s32 D_shelter_b6_corridor_8017F354;
extern s32 D_shelter_b6_corridor_8017F684;

extern SVECTOR D_shelter_b6_corridor_8017F834[];
extern SVECTOR D_shelter_b6_corridor_8017F844[];
extern SVECTOR D_shelter_b6_corridor_8017F874[];

/// The context the wave task was spawned with: its ramp limit, peak, mode and
/// tint.
extern ActorWaveCtx* D_shelter_b6_corridor_80180568;

/// Phase records of the wave's 9 column edges and 30 row edges.
extern ActorWaveRec D_shelter_b6_corridor_8018056C[9];
extern ActorWaveRec D_shelter_b6_corridor_801805BC[30];

/// The two 8 by 30 quad grids, one per frame-buffer half.
extern POLY_FT4 D_shelter_b6_corridor_801807EC[][30][8];

extern s16 D_shelter_b6_corridor_801851B0;
extern s32 D_shelter_b6_corridor_801851B8;

void func_shelter_b6_corridor_8017E360(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Task that ripples the whole screen. On its first frame it gives each of the
/// 9 column and 30 row edges a random phase offset and speed, takes its
/// context from `spawnArg2`, passes -8 to `Display_ClampField126` and builds
/// two 8 by 30 grids of `POLY_FT4`s sampling the two frame-buffer halves,
/// tinted with the context's colour when its tint flag is set. Afterwards the
/// context's mode ramps the strength up to its limit (mode 0), back down to
/// zero and on to mode 2 (mode 1), or ends the task and passes 0 back
/// (mode 2); the displacement is the ramp's share of the context's peak. The
/// edges advance while `Gp_StateF0.field_4` is clear, and the current
/// buffer's grid is drawn with every corner pushed by sine waves of that
/// displacement, bracketed by draw-mode packets that turn mask-bit setting on
/// at the back of the order table and off again at the front.
void func_shelter_b6_corridor_8017D5D0(Task* arg0)
{
    ActorWaveScratch* scratch;
    ActorWaveScratch* head;
    ActorWaveCtx*     ctx;
    ActorWaveRec*     cols;
    POLY_FT4*         p;
    DR_STP*           stp;
    s32               i;
    s32               j;
    s32               k;
    s32               rowIndex;
    s32               rowBack;
    s32               u0;
    s32               u1;
    s32               v0;
    s32               v1;
    s32               waveX0;
    s32               waveY0;
    s32               waveX1;
    s32               waveY1;
    s32               waveX2;
    s32               waveY2;
    s32               waveX3;
    s32               waveY3;
    ActorWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                                = *(ActorWaveScratch**)G_SCRATCH_HEAD;
    D_800691CA                          = 2;
    *(ActorWaveScratch**)G_SCRATCH_HEAD = head - 1;
    cols                                = head[-1].cols;
    scratch                             = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                D_shelter_b6_corridor_8018056C[i].phase  = 0;
                D_shelter_b6_corridor_8018056C[i].offset = (u32)rand() >> 3;
                D_shelter_b6_corridor_8018056C[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b6_corridor_801805BC[i].phase  = 0;
                D_shelter_b6_corridor_801805BC[i].offset = (u32)rand() >> 3;
                D_shelter_b6_corridor_801805BC[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            D_shelter_b6_corridor_8017EF20          = 0;
            D_shelter_b6_corridor_80180568          = arg0->spawnArg2;
            D_shelter_b6_corridor_80180568->field_6 = 0;
            D_shelter_b6_corridor_80180568->field_4 = 0;
            Display_ClampField126(-8);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = D_shelter_b6_corridor_801807EC[i];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (D_shelter_b6_corridor_80180568->field_8 == 0) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = D_shelter_b6_corridor_80180568->field_9;
                            p->g0 = D_shelter_b6_corridor_80180568->field_A;
                            p->b0 = D_shelter_b6_corridor_80180568->field_B;
                        }
                        u0 = k * 40;
                        u1 = (k + 1) * 40;
                        if (u1 == 320) {
                            u1 = 319;
                        }
                        if (u0 < 128) {
                            p->tpage = tpage0;
                        } else {
                            p->tpage = tpage1;
                            u0      -= 128;
                            u1      -= 128;
                        }
                        v1 = (j + 1) * 8 + i * 16;
                        if (j != -1) {
                            v0 = j * 8 + i * 16;
                        } else {
                            v0 = i * 16 + 8;
                            v1 = i * 16;
                        }
                        p->u0 = u0;
                        p->v0 = v0;
                        p->u1 = u1;
                        p->v1 = v0;
                        do {
                            p->u2 = u0;
                            p->v2 = v1;
                            p->u3 = u1;
                        } while (0);
                        p->v3 = v1;
                    }
                }
            }
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b6_corridor_80180568;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        if (D_801153F4 == 0) {
                            ctx->field_6--;
                        }
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_shelter_b6_corridor_8017EF20 = D_shelter_b6_corridor_80180568->field_6 * D_shelter_b6_corridor_80180568->field_2 / D_shelter_b6_corridor_80180568->field_0;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b6_corridor_8018056C[i].phase += D_shelter_b6_corridor_8018056C[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&D_shelter_b6_corridor_8018056C[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b6_corridor_801805BC[i].phase += D_shelter_b6_corridor_801805BC[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&D_shelter_b6_corridor_801805BC[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = D_shelter_b6_corridor_801807EC[D_80070F87];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = D_shelter_b6_corridor_8017EF20 * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b6_corridor_8017EF20 * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b6_corridor_8017EF20 * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b6_corridor_8017EF20 * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        ActorWaveRec* next = row + 1;
                        waveX2             = D_shelter_b6_corridor_8017EF20 * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2              = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2             = D_shelter_b6_corridor_8017EF20 * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2              = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3             = D_shelter_b6_corridor_8017EF20 * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3              = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3             = D_shelter_b6_corridor_8017EF20 * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3              = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
    *(ActorWaveScratch**)G_SCRATCH_HEAD += 1;
}

s32 func_shelter_b6_corridor_8017DEA8(void)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017DEB0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 id;
    s32 k;

    *out = *in;
    func_80179B14(in, out);
    k  = in->msgId;
    id = k;
    k  = 0x19;
    if (id == 9) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (id == k) {
        return Gp_StateF0.field_0 != 1;
    }
    return 1;
}

s32 func_shelter_b6_corridor_8017DF48(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0x144) != 0) {
                Gp_RunCapCmd1(5);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(2);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x145) != 0) {
                Gp_RunCapCmd1(6);
            } else if (Gp_StateF0.field_0 == 1) {
                Gp_RunCapCmd1(3);
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x146) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.field_0 != 1) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

s32 func_shelter_b6_corridor_8017E020(void)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017E028(void)
{
    func_800E8634((s32)&D_shelter_b6_corridor_8017F354, 0, (s32)&D_shelter_b6_corridor_8017F684);
    func_800E3FAC(0xA2, 0x2F);
    return 0;
}

void func_shelter_b6_corridor_8017E064(Task* arg0)
{
    u16* ptr;
    s32  i;

    arg0->msgTable = D_shelter_b6_corridor_8017EF24;
    Game_SetPtrSlot(arg0, 7);
    ptr = (u16*)Fs_ImgBuffers;
    i   = 0;
    do {
        *ptr = (u16)(*ptr | 0x8000);
        i   += 1;
        ptr += 1;
    } while (i <= 0x12BFF);
    D_shelter_b6_corridor_801851B0 = 2;
    if (gGameSession->at4.loc.place == 1) {
        D_80062735               = 2;
        gGameSession->flowFlags |= 1;
        gGameSession->flowFlags |= 2;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b6_corridor_8017E12C(Task* task)
{
    char pad[0x10];

    D_800691CA = 2;
}

/// The room task's three states: set the room up, the per-frame state, end.
const TaskFuncTable3 D_shelter_b6_corridor_8017D5C4 = {
    { func_shelter_b6_corridor_8017E064, func_shelter_b6_corridor_8017E12C, taskKill },
};

/// Runs the room task's current state from its three-entry table, which it
/// copies onto the stack before the call.
void func_shelter_b6_corridor_8017E144(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_corridor_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_corridor_8017E19C(s32 arg0)
{
    if (!(gGameSession->flowFlags & 0x80)) {
        gGameSession->flowFlags |= 0x80;
        D_801153F1               = arg0;
        Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(1), 0x31);
        Task_CallExit((Task*)Gp_LookupSlot4(1));
    }
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_shelter_b6_corridor_8017E204(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_shelter_b6_corridor_8017E238(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b6_corridor_801851B8 = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[0], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[2], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[8], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[10], 0x140, 0x442);
            break;
        case 3:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[0], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[2], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[4], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[8], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[10], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[12], 0x140, 0x442);
            break;
        case 4:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F874[0], 0x140, 0x442);
            break;
    }
}

/// Draws a glowing bar between the world points `arg0[0]` and `arg0[1]`,
/// projected through `Gfx_ViewWorldMtx`; nothing is drawn when either
/// projection flags an error. Each end gets a half-disc of gouraud wedges of
/// radius `(s16)arg1 * 64` over its depth, and a strip of quads joins them
/// across the bar. The lit vertices take the colour packed in `arg2`, one
/// nibble per channel shifted into the high nibble, with bit 3 following the
/// animation frame.
void func_shelter_b6_corridor_8017E360(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    s32                sum;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
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
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
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
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_shelter_b6_corridor_8017EBA4(Task* task)
{
    GsCOORDINATE2* coord;
    u8             rgb[3];
    u32            shade;

    coord = ((TmdObject*)task->extra)->coords + 1;
    if (Gp_State1C->eventState == 0) {
        shade  = ((D_80070F70 & 1) << 4) + 0x40;
        rgb[0] = shade;
        rgb[1] = shade;
        rgb[2] = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords + (((Gp_LcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void func_shelter_b6_corridor_8017ECA8(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            eventState;
    u8             rgb[3];

    mem        = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = ((TmdObject*)task->extra)->coords;
    if (eventState != 0) {
        if (eventState < 4) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_corridor_801851B8++;
        task->state     = 1;
        task->spawnArg1 = D_shelter_b6_corridor_801851B8;
    }
    if (task->spawnArg1 != D_shelter_b6_corridor_801851B8) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = (u16)mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        Gp_ReleaseState1CMem(mem, task);
    }
}

void func_shelter_b6_corridor_8017EE08(s32 arg0, s32 arg1)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    GpSprtCmd* cmd;
    s32        run = arg0 & 0xFF;
    s32        flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[1].field_4;
            cmd[1].field_4 = 1;
            return;
        }
        if (flag == 1) {
            cmd            = rec[1].field_4;
            cmd[1].field_4 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[1].field_4;
            cmd[2].field_4 = run;
            cmd            = rec[2].field_4;
            cmd[2].field_4 = run;
            return;
        }
        if (flag == run) {
            cmd            = rec[1].field_4;
            cmd[2].field_4 = 0;
            cmd            = rec[2].field_4;
            cmd[2].field_4 = 0;
            return;
        }
    } else if (run == 2) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = 1;
            return;
        }
        if (flag == 1) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = 0;
        }
    }
}
