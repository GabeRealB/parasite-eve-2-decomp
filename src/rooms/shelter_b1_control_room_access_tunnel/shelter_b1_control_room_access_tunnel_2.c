#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

extern s32 D_80070F70;
extern s32 D_80115730;

/// Per-colour channel shifts for the glowing disc, indexed by the spawn
/// argument.
extern RoomHaloShade D_shelter_b1_control_room_access_tunnel_80181EF4[];

void func_shelter_b1_control_room_access_tunnel_801809E8(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b1_control_room_access_tunnel_80180C6C(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b1_control_room_access_tunnel_80181090(GpCoord* arg0, s32 arg1, u8* rgb);
void func_shelter_b1_control_room_access_tunnel_801815D0(GpCoord* coord, s16 size);
void func_shelter_b1_control_room_access_tunnel_80181AFC(GpCoord* arg0, s32 arg1);

/// Glowing disc anchored to its parent at the work block's position. State 1
/// grows the disc and, every fourth tick, spawns the effect `D_80115730` names
/// at a random joint of the player's model and adopts it as a child task;
/// state 2 keeps growing it and adds a half-bright second disc on odd ticks;
/// state 3 drifts the disc away while it fades inside an expanding ring, then
/// releases the work block, as does state 4. The spawn argument picks the
/// disc's colour. Nothing runs while the room's event state is set, and the
/// block is released once that state reaches 4.
void func_shelter_b1_control_room_access_tunnel_8018026C(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
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
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
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
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b1_control_room_access_tunnel_80181090(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b1_control_room_access_tunnel_80181EF4[arg0->spawnArg1].b;
            func_shelter_b1_control_room_access_tunnel_80181090(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b1_control_room_access_tunnel_80180C6C(coord, (s16)(mem->period + 0x80), 0x100, col);
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

/// Moves the effect's coordinate toward the coordinate passed as the spawn
/// argument. State 0 turns their world-space displacement into the effect's
/// frame and scales it by 0xCC / 0x1000; state 1 adds that step each tick and
/// draws the sprite on odd ticks, advancing its animation cell. The work block
/// is released at tick 20, or once the room's event state reaches 4.
void func_shelter_b1_control_room_access_tunnel_801807C4(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    GpCoord*   target;
    VECTOR     delta;

    work   = task->spawnArg2;
    coord  = task->extra.tmd->coords;
    target = (GpCoord*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += work->pos.vx;
                coord->coord.t[1] += work->pos.vy;
                coord->coord.t[2] += work->pos.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b1_control_room_access_tunnel_801809E8(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues one semi-transparent textured square centred
/// on it, of half-size `arg2 * 23 / (otz + 1)`. `arg1` picks one of four
/// 24-texel animation cells and `arg3` is the grey level.
void func_shelter_b1_control_room_access_tunnel_801809E8(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            u0;
    s32            u1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        u0          = t + 0x60;
        u1          = t + 0x77;
        SOFT_USE_REG(u1);
        prim->u0 = u0;
        prim->u2 = u0;
        prim->v2 = 0x17;
        prim->v3 = 0x17;
        sarg     = (s16)arg2;
        prim->u1 = u1;
        prim->u3 = u1;
        t        = sarg * 24;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0    = 0;
        prim->v1    = 0;
        block->step = (t - sarg) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// The same ring as `func_shelter_b1_control_room_access_tunnel_8017E57C`,
/// over a scratch block laid out differently: black at radius
/// `arg1 * 64 / (otz + 1)`, shading to `rgb` at radius
/// `(arg1 + arg2) * 64 / (otz + 1)`, drawn unless the GTE flags an error.
void func_shelter_b1_control_room_access_tunnel_80180C6C(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch*   block;
    POLY_G4*        prim;
    s32             ang;
    register void** scratch asm("a1");
    register s32    saved asm("t1");
    register u8*    head asm("t0");
    register s32    sum asm("a1");
    register s32    otz asm("v0");
    register s32    rOuter asm("a0");
    register s32    rInner asm("v1");
    register u8*    color asm("s4");
    s32             t;
    u16             vz;
    u32             maskLo;
    u32             maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                     = (u16)arg0->workm.t[0];
        ((GpArcScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (GpArcScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpArcScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpArcScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpArcScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner       = rInner / otz;
        ang          = 0;
        block->inner = rOuter;
        block->outer = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// The same disc as `func_shelter_b1_control_room_access_tunnel_8017E9A8`,
/// over a scratch block laid out differently: unless the GTE flags an error,
/// eight gouraud `POLY_G4` wedges of radius `arg1 * 64 / (otz + 1)`, coloured
/// `rgb` at the centre and black at the rim.
void func_shelter_b1_control_room_access_tunnel_80181090(GpCoord* arg0, s32 arg1, u8* rgb)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = (u16)arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = (u16)block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = (u16)block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = (u16)block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = (u16)block->sx;
            prim->y2 = (u16)block->sy;
            ang2     = ang + 0x200;
            prim->x3 = (u16)block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = (u16)block->sy + ((block->step * rcos(ang2)) >> 12);
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Flare at the task's coordinate: each tick grows a size by 0x10 and draws a
/// disc of twice that radius and the glow at that size. While the ring's level
/// is above 0x18 it also draws a ring that widens as it fades; after that the
/// disc dims by 0x18 a tick and the work block is released once it drops below
/// 0x18, or once the room's event state reaches 4.
void func_shelter_b1_control_room_access_tunnel_80181424(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         sp10[3];
    u16        temp;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age    = 1;
            work->scale  = 0xE0;
            work->angle  = 0x80;
            work->period = 0xE0;
            work->step   = 0x80;
            task->state  = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]     = (u8)work->scale;
        sp10[1]     = (u8)(work->scale >> 1);
        sp10[2]     = (u8)(work->scale >> 2);
        temp        = work->angle + 0x10;
        work->angle = temp;
        func_shelter_b1_control_room_access_tunnel_80181090(coord, (s16)(temp * 2), sp10);
        func_shelter_b1_control_room_access_tunnel_801815D0(coord, work->angle);
        if (work->period >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->period;
            sp10[1] = (u8)(work->period >> 1);
            sp10[2] = (u8)(work->period >> 2);
            temp_a1 = work->step * 3;
            func_shelter_b1_control_room_access_tunnel_80180C6C(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->period -= 0x18;
            work->step   += 0x30;
            return;
        }
        temp        = work->scale - 0x18;
        work->scale = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Glow at a coordinate: two camera-facing textured squares, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2` (each scaled by
/// 0x37 / otz), plus the flat sprite on the ground beneath it when a floor is
/// found. Also points the `Gp_RoomCoords[2]` light at the coordinate with a
/// flickering intensity. Nothing is drawn when the GTE flags the projection.
void func_shelter_b1_control_room_access_tunnel_801815D0(GpCoord* coord, s16 size)
{
    GpCoord        ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = (void**)G_SCRATCH_HEAD;
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
    block->vec.vx               = (u16)coord->workm.t[0];
    alias                       = block;
    vy                          = (u16)coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = (u16)coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2EU;
        *(u16*)&prim->tpage  = 0x29;
        if (D_80070F70 & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2F;
        prim->tpage          = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b1_control_room_access_tunnel_80181AFC(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flat textured sprite at the coordinate's world position: the unit
/// quad `D_80111E38`, scaled by `arg1` and turned by `Gfx_ViewWorldMtx`'s
/// rotation, is offset to `arg0->workm.t` and projected through `GsWSMATRIX`.
/// Unless the GTE flags an error, one semi-transparent `POLY_FT4` is queued,
/// its texture alternating between two 32-texel frames on odd and even
/// frames.
void func_shelter_b1_control_room_access_tunnel_80181AFC(GpCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = (u16)block->sxy0.vx;
        prim->y0    = (u16)block->sxy0.vy;
        prim->x1    = (u16)block->sxy1.vx;
        prim->y1    = (u16)block->sxy1.vy;
        prim->x2    = (u16)block->sxy2.vx;
        prim->y2    = (u16)block->sxy2.vy;
        prim->x3    = (u16)block->sxy3.vx;
        prim->y3    = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
