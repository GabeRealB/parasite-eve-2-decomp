#include "gameplay/effect_tasks.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

/// Unreferenced nonzero tail; its original purpose is unknown.
extern u32 D_80114B7C;

/// Task bank 10; actors supply the last descriptor's model before spawning.
TaskDesc D_80114B34[6] = {
    { 0, 0xC0, taskKill, { .value = 0 } },
    { 0, 0xC0, taskKill, { .value = 0 } },
    { 0, 0xC0, taskKill, { .value = 0 } },
    { 0, 0xC0, taskKill, { .value = 0 } },
    { 0, 0xC0, NULL, { .value = 0 } },
    { 1, 0x70, Gp_EffAttachTask37, { .model = NULL } },
};

/// Unreferenced nonzero tail; its original purpose is unknown.
u32 D_80114B7C = 0x323010CE;

void Gp_EffAttachTask37(Task* arg0)
{
    SVECTOR    delta;
    SVECTOR    dir;
    SVECTOR    pos;
    VECTOR     scale2;
    VECTOR     scale;
    TmdObject* extra;
    GpEffWork* mem;
    GfxCoord*  coord;
    GfxCoord*  player;
    SVECTOR*   rot;
    MATRIX*    mtx;
    s32        state;
    s16        flag;
    s16        trans;
    s32        temp;

    extra  = arg0->extra.tmd;
    mem    = arg0->spawnArg2.pointer;
    coord  = extra->coords;
    player = (gameGetPtrSlot(3))->extra.tmd->coords;
    flag   = Gp_State1C->effectControl;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    Gp_UpdateCoord(coord);
    mem->age++;
    state = arg0->state;
    switch (state) {
        case 0:
            extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            mem->scale    = 0x100;
            if (arg0->spawnArg1.value & 0xFFF) {
                temp = arg0->spawnArg1.halves.low & 0xFFF;
            } else {
                temp = 0x200;
            }
            mem->angle   = temp;
            mem->period  = 0x800;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = 0x400 - (((u32)Gp_LcgState >> 16) % 0xC00);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
            VectorNormalSS(&mem->move, &mem->move);
            Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
            mem->pos.vx         = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
            mem->pos.vy         = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
            mem->pos.vz         = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            return;
        case 1:
            mtx = &coord->coord;
            rot = &mem->pos;
            Gfx_RotMatrixXYZ(mtx, rot, 0);
            MatrixNormal(mtx, mtx);
            gte_lddp(mem->scale);
            gte_ldsv(&mem->move);
            gte_gpf12();
            gte_stsv(&delta);
            coord->coord.t[0]  += delta.vx;
            coord->coord.t[1]  += delta.vy;
            coord->coord.t[2]  += delta.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&delta);
            gte_rtv0();
            gte_stsv(&dir);
            pos.vx  = coord->workm.t[0];
            pos.vy  = coord->workm.t[1];
            pos.vz  = coord->workm.t[2];
            dir.vx += pos.vx;
            dir.vy += pos.vy;
            dir.vz += pos.vz;
            if (func_800DE7CC(&dir, &pos, &dir, &pos) == state) {
                coord->coord.t[0] -= delta.vx;
                coord->coord.t[1] -= delta.vy;
                coord->coord.t[2] -= delta.vz;
                mem->move.vx       = (pos.vx >> 1) + (mem->move.vx >> 1);
                mem->move.vy       = pos.vy + (mem->move.vy >> 1);
                mem->move.vz       = (pos.vz >> 1) + (mem->move.vz >> 1);
                VectorNormalSS(&mem->move, &mem->move);
                mem->scale >>= 1;
                gte_lddp(mem->scale);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&delta);
                coord->coord.t[0]  += delta.vx;
                coord->coord.t[1]  += delta.vy;
                coord->coord.t[2]  += delta.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (!(mem->age & 3)) {
                    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
                }
                Gp_SpawnEff(0x60055, coord, mem->angle + 0x12200, 0);
                gte_lddp(0x800);
                gte_ldsv(rot);
                gte_gpf12();
                gte_stsv(rot);
                if ((mem->age - mem->step) < 8 && mem->scale < 0x20) {
                    extra->flags |= TMD_OBJECT_SEMI_TRANS;
                    mem->age      = 0;
                    arg0->state   = 2;
                    return;
                }
                mem->step = mem->age;
                return;
            }
            if (mem->scale == 0) {
                return;
            }
            if (mem->age >= 0x4C) {
                goto release;
            }
            if (player->coord.t[1] + 0x100 < coord->coord.t[1]) {
                mem->age += 0xA;
            }
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            }
            mem->move.vy += 0x10000 / mem->scale;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            if (!(((u32)Gp_LcgState >> 16) & 3)) {
                Gp_SpawnEff(0x60042, coord, mem->angle + 0x11000, 0);
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (!(((u32)Gp_LcgState >> 16) & 7)) {
                Gp_SpawnEff(0x60055, coord, mem->angle + 0x11000, 0);
            }
            if (mem->age >= 0x33) {
                extra->flags |= TMD_OBJECT_SEMI_TRANS;
                if (mem->period >= 0x41) {
                    trans       = mem->period - 0x40;
                    mem->period = trans;
                    Gp_SetObjTrans(extra, trans, trans, trans);
                    return;
                }
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            }
            if (mem->age >= 0x10) {
                goto release;
            }
            memset(&scale, 0, 0x10);
            scale.vx = 0x1000;
            scale.vy = (0x10 - mem->age) << 8;
            scale.vz = 0x1000;
            scale2   = scale;
            ScaleMatrix(&coord->coord, &scale2);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (mem->period >= 0x81) {
                trans       = mem->period - 0x80;
                mem->period = trans;
                Gp_SetObjTrans(extra, trans, trans, trans);
            }
            if (mem->age == 8) {
                Gp_SpawnEff(0x600A5, coord, mem->angle >= 0x100, 0);
            }
            return;
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}
