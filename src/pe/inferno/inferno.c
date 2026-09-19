#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/inferno.h"

/// The two fan shapes the inferno wall sweeps through.
InfernoFanParam D_inferno_801304E4[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
};

/// The `SndEvt_EnqueueType6` id for each inferno stage.
s32 D_inferno_801304F0[] = { 0xE0100001, 0xE0130001, 0xE00D0001 };

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// `gpf 12` / `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0`. The `inline_c.h` macros
/// of those names assemble to different words, so spell the instructions out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")

void func_inferno_8012F3EC(s16 arg0);
void func_inferno_8012F978(GpEffWork* mem, GsCOORDINATE2* coord, s32 kind, InfernoIdMap* map);
void func_inferno_8012FF34(GpEffWork* mem, GsCOORDINATE2* coord, s32 kind, InfernoIdMap* map);

/// Runs one frame of the inferno cast: a state machine driven by
/// `Task::state`, with the chain it takes chosen in state 0 from
/// `Gp_StateC08.field_0 % 10 - 1` (the combo counter), which also picks the
/// roar from `D_inferno_801304F0` and lands the task on state 1, 5 or 9.
/// State 1 spawns the two ignition effects, state 5 fans six flames around a
/// 0x400 step, state 9 the ground burst; states 10 and 11 fade the effect
/// brightness scalar (`GpEffWork::angle`) down and back up and each fire one ring of
/// flames on their own tick, and state 12 fades out and releases. Every state
/// updates the effect coordinate first, and any state releases immediately if
/// the player is dying (`D_80114C0B`) or the room is fading (`Gp_State1C`).
void func_inferno_8012EF88(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            i;
    s32            pan;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((D_80114C0B == -2) || (Gp_State1C->fadeState >= 4)) {
        goto release;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    mem->age = (u16)mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->scale = 0x200;
            mem->angle = 0xFF;
            pan        = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(D_inferno_801304F0[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)gpGetObjDepth(coord));
            arg0->state = ((u16)(Gp_StateC08.field_0 % 10) - 1) * 4 + 1;
            return;
        case 1:
            Gp_SpawnEff(0x800600DA, coord, 3, NULL);
            Gp_SpawnEff(0x800600DA, coord, 5, NULL);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x10, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 5:
            i = 0x200;
            func_inferno_8012F3EC(mem->angle);
            Gp_SpawnEff(0x800600DA, coord, 3, NULL);
            mem->scale = 0x600;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x400;
                Gp_SpawnEff(0x800600DA, coord, 4, &mem->move);
            } while (i < 0x1200);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x14, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 9:
            Gp_SpawnEff(0x800600DA, coord, 0, NULL);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xA;
            return;
        case 10:
            func_inferno_8012F3EC(mem->angle);
            mem->angle = (u16)mem->angle - 0x10;
            if (mem->age != 0xC) {
                return;
            }
            mem->scale = 0x600;
            i          = 0x155;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x2AA;
                Gp_SpawnEff(0x800600DA, coord, 1, &mem->move);
            } while (i < 0x1151);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xB;
            return;
        case 11:
            func_inferno_8012F3EC(mem->angle);
            if (mem->angle < 0xF0) {
                mem->angle = (u16)mem->angle + 0x10;
            }
            if (mem->age != 0x18) {
                return;
            }
            mem->scale = 0x900;
            i          = 0;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x2AA;
                Gp_SpawnEff(0x800600DA, coord, 2, &mem->move);
            } while (i < 0xFFC);
            Gp_StateC08.field_6 |= 8;
            Gp_SpawnPadLerp(0x18, 0xFF, 8);
            arg0->state = 0xC;
            mem->angle  = 0xFF;
            return;
        case 12:
            func_inferno_8012F3EC(mem->angle);
            if (mem->angle >= 9) {
                mem->angle = (u16)mem->angle - 8;
                return;
            }
            break;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Full-screen wash quad drawn by the inferno cast: an unshaded `POLY_F4`
/// covering the 320x240 view in `arg0` / `arg0 >> 1` / `arg0 >> 2` red-amber,
/// added to OT slot 0x30 and followed by a shifted-tpage semi-trans packet.
void func_inferno_8012F3EC(s16 arg0)
{
    POLY_F4*      p;
    DisplayState* ds;
    s32           x0;
    s32           x1;
    s32           yTop;
    s32           yBot;
    s32           z;

    ds   = &gDisplayState;
    x0   = -0xA0;
    x1   = 0xA0;
    yTop = -0x78;
    yBot = 0x78;
    z    = 0x30;

    p              = (POLY_F4*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyF4(p);
    setRGB0(p, arg0, arg0 >> 1, arg0 >> 2);
    p->x0 = x0;
    p->y0 = yTop - ds->vramYOffset;
    p->x1 = x1;
    p->y1 = yTop - ds->vramYOffset;
    p->x2 = x0;
    p->y2 = yBot - ds->vramYOffset;
    p->x3 = x1;
    p->y3 = yBot - ds->vramYOffset;
    addPrim((u_long*)(((((u32)z << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), p);
    Gp_AddTpageShift((P_TAG*)p, 1, z);
}

/// Trailing zero word in this unit's rodata.
const u32 D_inferno_8012EF68 = 0;

/// Companion inferno-cast task: state 0 allocates a 12-byte `InfernoIdMap`
/// of LCG jitter, scales `GpEffWork::pos` by 0x80 (`gte_gpf12`) and
/// rotates it into `move`. States 1–6 fade `scale` while spinning
/// `angle` / `period` / `step` and drawing through
/// `func_inferno_8012F978` (kind 0) and `func_inferno_8012FF34` (kind 1).
/// State 3 also walks the effect coordinate by `move`. Releases if the
/// player is dying, the room is fading, or the state's brightness floor is
/// hit. `Task::spawnArg1 + 1` selects the chain from state 0.
void func_inferno_8012F530(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    InfernoIdMap*  map;
    u8*            p;
    s32            i;
    s32            rng;
    s32            tz;

    map   = (InfernoIdMap*)arg0->work;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((D_80114C0B == -2) || (Gp_State1C->fadeState >= 4)) {
        goto release;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    mem->age = (u16)mem->age + 1;
    switch (arg0->state) {
        case 0:
            map = memCalloc(0xC, 0);
            if (map == NULL) {
                mem->age = 0;
                return;
            }
            arg0->work = (TaskIdMap*)map;
            mem->scale = 0x80;
            i          = 0;
            do {
                p           = &map->field_0[0][i];
                rng         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rng;
                p[0]        = (u32)rng >> 16;
                i++;
                rng         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rng;
                p[6]        = (u32)rng >> 16;
            } while (i < 6);
            arg0->state = arg0->spawnArg1 + 1;
            gte_lddp(0x80);
            gte_ldsv(&mem->pos);
            gte_gpf12_real();
            gte_stsv(&mem->move);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&mem->move);
            gte_rtv0_real();
            gte_stsv(&mem->move);
            return;
        case 1:
            if (mem->scale >= 5) {
                if (mem->period < 0xC00) {
                    mem->period = (u16)mem->period + 0xC0;
                } else {
                    mem->scale = (u16)mem->scale - 4;
                }
                mem->angle = (u16)mem->angle + 0x20;
                mem->step  = (u16)mem->step + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 2:
            if (mem->scale >= 9) {
                mem->scale  = (u16)mem->scale - 8;
                mem->angle  = (u16)mem->angle + 0x20;
                mem->period = (u16)mem->period + 0xC0;
                mem->step   = (u16)mem->step + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 3:
            coord->coord.t[0] += mem->move.vx;
            coord->coord.t[1] += mem->move.vy;
            tz                 = coord->coord.t[2] + mem->move.vz;
            coord->flg         = 0;
            coord->coord.t[2]  = tz;
            if (mem->scale >= 9) {
                mem->scale  = (u16)mem->scale - 8;
                mem->angle  = (u16)mem->angle + 0x20;
                mem->period = (u16)mem->period + 0xC0;
                mem->step   = (u16)mem->step + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 4:
            if (mem->scale >= 7) {
                mem->scale  = (u16)mem->scale - 6;
                mem->angle  = (u16)mem->angle + 0x40;
                mem->period = (u16)mem->period + 0xC0;
                mem->step   = (u16)mem->step + 0x10;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 5:
            if (mem->scale >= 7) {
                mem->scale  = (u16)mem->scale - 6;
                mem->angle  = (u16)mem->angle + 0x40;
                mem->period = (u16)mem->period + 0x40;
                mem->step   = (u16)mem->step + 0x18;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        case 6:
            if (mem->scale >= 7) {
                mem->scale  = (u16)mem->scale - 6;
                mem->angle  = (u16)mem->angle + 0x80;
                mem->period = (u16)mem->period + 0x20;
                mem->step   = (u16)mem->step + 0x20;
                func_inferno_8012F978(mem, coord, 0, map);
                func_inferno_8012FF34(mem, coord, 1, map);
                return;
            }
            break;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws the lifted ring of the inferno's ground fan, the twin of
/// `func_inferno_8012FF34`: identical geometry and prim setup, except the
/// inner rim is lifted `GpEffWork::period + field_2` along local Y instead
/// of `field_2` alone, so the ring rises as the caster's `period` winds up.
/// `kind` picks the row of `D_inferno_801304E4` that sizes it.
void func_inferno_8012F978(GpEffWork* mem, GsCOORDINATE2* coord, s32 kind, InfernoIdMap* map)
{
    void**             scratch;
    u8*                head;
    InfernoFanScratch* block;
    InfernoFanParam*   row;
    InfernoFanParam*   tbl;
    SVECTOR*           op;
    POLY_FT4*          prim;
    s32                flag;
    s32                otz;
    s32                i;
    s32                next;
    s32                ang;
    s32                u;
    s16                inner;
    s16                outer;
    u16                h;
    u16                frame;

    scratch  = (void**)G_SCRATCH_HEAD;
    tbl      = D_inferno_801304E4;
    row      = &tbl[kind];
    h        = (u16)mem->period + row->field_2;
    inner    = (u16)mem->angle + row->field_0;
    outer    = row->field_4 + (inner + (u16)mem->step);
    head     = (u8*)*scratch;
    *scratch = head - 0x70;
    block    = (InfernoFanScratch*)(head - 0x70);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        ang                = i * 0x2AA;
        block->inner[i].vx = (rsin(ang) * outer) >> 12;
        block->inner[i].vy = -h;
        block->inner[i].vz = (rcos(ang) * outer) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&coord->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&coord->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&coord->workm.t[2];
        block->outer[i].vx = (rsin(ang) * inner) >> 12;
        op                 = &block->inner[i] + 6;
        op->vy             = 0;
        op->vz             = (rcos(ang) * inner) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        frame = (map->field_0[kind][i] + mem->age) % 6;
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->inner[next % 6], &block->outer[i], &block->outer[next % 6]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2E);
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = block->sxy0;
            prim->y0 = block->sxy0 >> 16;
            prim->x1 = block->sxy1;
            prim->y1 = block->sxy1 >> 16;
            prim->x2 = block->sxy2;
            prim->y2 = block->sxy2 >> 16;
            prim->x3 = block->sxy3;
            prim->y3 = block->sxy3 >> 16;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x70;
}

/// Draws one ring of the inferno's ground fan. `kind` picks the row of
/// `D_inferno_801304E4` that sizes it: six inner rim points of radius
/// `angle + field_0` lifted `field_2` along local Y and six outer rim
/// points of radius `angle + field_0 + step + field_4` in the local XY
/// plane are built by `rsin` / `rcos` a sixth of a turn apart, rotated by
/// `coord`'s `workm` and offset by its translation. Each of the six segments
/// is then projected through `GsWSMATRIX` and linked as one semi-transparent
/// `POLY_FT4`; `map` and `GpEffWork::age` pick which of the six 0x28-wide
/// texture frames it uses, and a negative `gte_stflg` drops the segment.
void func_inferno_8012FF34(GpEffWork* mem, GsCOORDINATE2* coord, s32 kind, InfernoIdMap* map)
{
    void**             scratch;
    u8*                head;
    InfernoFanScratch* block;
    InfernoFanParam*   row;
    InfernoFanParam*   tbl;
    SVECTOR*           op;
    POLY_FT4*          prim;
    s32                flag;
    s32                otz;
    s32                i;
    s32                next;
    s32                ang;
    s32                u;
    s16                inner;
    s16                outer;
    u16                h;
    u16                frame;

    scratch  = (void**)G_SCRATCH_HEAD;
    tbl      = D_inferno_801304E4;
    row      = &tbl[kind];
    inner    = (u16)mem->angle + row->field_0;
    outer    = row->field_4 + (inner + (u16)mem->step);
    h        = row->field_2;
    head     = (u8*)*scratch;
    *scratch = head - 0x70;
    block    = (InfernoFanScratch*)(head - 0x70);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        ang                = i * 0x2AA;
        block->inner[i].vx = (rsin(ang) * outer) >> 12;
        block->inner[i].vy = -h;
        block->inner[i].vz = (rcos(ang) * outer) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&coord->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&coord->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&coord->workm.t[2];
        block->outer[i].vx = (rsin(ang) * inner) >> 12;
        op                 = &block->inner[i] + 6;
        op->vy             = 0;
        op->vz             = (rcos(ang) * inner) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        frame = (map->field_0[kind][i] + mem->age) % 6;
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->inner[next % 6], &block->outer[i], &block->outer[next % 6]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2E);
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = block->sxy0;
            prim->y0 = block->sxy0 >> 16;
            prim->x1 = block->sxy1;
            prim->y1 = block->sxy1 >> 16;
            prim->x2 = block->sxy2;
            prim->y2 = block->sxy2 >> 16;
            prim->x3 = block->sxy3;
            prim->y3 = block->sxy3 >> 16;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x70;
}
