#include "pe/energyball.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// One 4-byte row of `D_energyball_80131194`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the full size the ball grows
/// to before it is launched (`GpEffWork.angle`; half of it is the linked
/// `GpObj.radius`, twice it the burst's final size) and `field_2` the
/// per-frame growth step, also the initial upward speed while charging.
typedef struct EnergyBallStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
} EnergyBallStep;
STATIC_ASSERT_SIZEOF(EnergyBallStep, 4);

/// Collision block allocated by `func_energyball_8012F180` (`memCalloc(0x38)`)
/// and stored in `Task::work`: `obj` is linked on list 1 with `ctx.recs`
/// pointing at the one-element `rec` table (terminator `field_0 = 2`).
typedef struct EnergyBallWork {
    /* 0x00 */ GpObj                 obj;
    /* 0x20 */ WorldCollisionContact rec;
} EnergyBallWork;
STATIC_ASSERT_SIZEOF(EnergyBallWork, 0x38);

static void func_energyball_8012FFD0(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_energyball_8013035C(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_energyball_801307D4(GfxCoord* arg0, s32 arg1);
static void func_energyball_80130B54(GfxCoord* arg0, s16 arg1, s16 arg2);

/// The energy ball's `SndEvt` ids. Only the first three are read, indexed by
/// the cast's level: the cast starts its entry with `SndEvt_EnqueueType6` and
/// later passes the same id to `SndEvt_EnqueueType7`.
static s32 D_energyball_8013117C[] = {
    0xE02B0002,
    0xE02E0002,
    0xE0310002,
    0xE02B0001,
    0xE02E0001,
    0xE0310001,
};

/// Per-level radius/step pairs for the ball, one row per PE level 1-3,
/// weakest first.
static EnergyBallStep D_energyball_80131194[] = {
    { 0x0400, 0x0040 },
    { 0x0480, 0x0048 },
    { 0x0500, 0x0050 },
};

/// Sixteen 8-bit draws from `Gp_LcgState`, refilled once per cast by
/// `func_energyball_8012EF48` and consumed by the GTE pass in
/// `func_energyball_80130B54` as the per-vertex jitter of the ball's surface.
static s16 D_energyball_801311A0[16];

/// Fires the energy ball: on the first frame it picks the charge level from the
/// combo counter, plays the matching loop sound, refills the surface-jitter
/// table and spawns one ball per charge level, fanning them out by 0x555 of
/// yaw each while `D_80115724` (the number of balls already in flight) allows
/// it. Every later frame just releases the work block.

void func_energyball_8012EF48(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s32        i;
    s32        level;
    s32        rng;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->index = Gp_StateC08.field_0 % 10 - 1;
            level      = mem->index;
            mem->angle = (level << 8) + 0x300;
            if (D_80115724 < 0) {
                D_80115724 = 0;
            }
            if (D_80115724 == 0) {
                SndEvt_EnqueueType6(D_energyball_8013117C[mem->index], 0, 0);
            }
            for (i = 0; i < 0x10; i++) {
                rng                      = Gp_LcgState * 5 + 0x71357911;
                D_energyball_801311A0[i] = ((u32)rng >> 16) & 0xFF;
                Gp_LcgState              = rng;
            }
            for (i = 0; i < mem->index + 1; i++) {
                if (D_80115724 + i >= 3) {
                    break;
                }
                mem->scale   = i * 0x555 - mem->index * 0x2AA;
                mem->move.vx = (mem->angle * rsin(mem->scale)) >> 12;
                mem->move.vz = (mem->angle * rcos(mem->scale)) >> 12;
                Gp_SpawnEff(0x800600F8, coord, i, &mem->move);
            }
            arg0->state = 1;
            return;
        case 1:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

/// One ball of the energy ball cast; `spawnArg1` picks the `Gp_RoomCoords`
/// slot it owns and `spawnArg2` the `GpEffWork` block. While the room is
/// fading (`Gp_State1C->fadeState`) it only redraws, and drops the ball once the
/// fade passes 4. Otherwise it walks `Task::state`: 0 allocates the
/// `EnergyBallWork` collision block, picks the charge row of
/// `D_energyball_80131194` from the combo counter and seeds a random spin
/// `period`; 1 grows the ball by the row's `field_2` per frame until it
/// reaches `field_0`, then links it on list 1 with a random direction; 2
/// flies it, re-aiming at the player every eighth frame and nudging each
/// velocity component by 0x10 on odd frames, bursting into three 0x600F9
/// effects on a hit (`Gp_CountRec18Hi`) or unlinking when the room's
/// `field_16` drops; 3 and 4 fade the burst out, growing to twice the row's
/// size or shrinking below one step. Cancel (`Gp_StateC08.field_3 == -2` or
/// the fade at 4 or more) anywhere but combo 0x2B lets the ball go: the last
/// ball in flight (`D_80115724`) queues the row's stop sound.
void func_energyball_8012F180(Task* arg0)
{
    GpEffWork*      mem;
    GfxCoord*       coord;
    EnergyBallWork* work;
    GpCoord64*      slot;
    GfxCoord*       sc;
    GpPointLight*   tail;
    GfxCoord        ground;
    VECTOR          vec;
    GfxCoord*       player;
    GpEffWork*      spawned;
    SVECTOR*        dir;
    u16             r;
    s32*            snd;
    s16             fade;
    s32             cur;

    slot  = &Gp_RoomCoords[arg0->spawnArg1.value + 4];
    sc    = &slot->light.head.u.coord;
    tail  = &slot->light;
    coord = arg0->extra.coordBody->coord;
    fade  = Gp_State1C->fadeState;
    work  = (EnergyBallWork*)arg0->work;
    mem   = arg0->spawnArg2.pointer;
    if (fade != 0) {
        if (fade >= 4) {
            if (D_80115724 > 0) {
                D_80115724 -= 1;
                if (D_80115724 == 0) {
                    SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                }
            }
            if (arg0->state != 0) {
                Gp_UnlinkObj(&work->obj);
            }
            goto release;
        }
        Gp_UpdateCoord(coord);
        func_energyball_8013035C(coord, mem->age, mem->angle, mem->period);
        func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
        if ((arg0->state < 3) && (Gp_State1C->groundTrace != 0) &&
            (Gp_TraceGroundCoord(coord, &ground) == 1)) {
            func_energyball_801307D4(&ground, mem->angle);
        }
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            work = memCalloc(0x38, 0);
            if (work == NULL) {
                mem->age = 0;
                return;
            }
            arg0->work   = work;
            mem->index   = (Gp_StateC08.field_0 % 10) - 1;
            mem->move.vx = 0;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = -(u16)D_energyball_80131194[mem->index].field_2;
            mem->move.vz = 0;
            mem->angle   = 0;
            mem->period  = ((u32)Gp_LcgState >> 16) & 0xFFF;
            D_80115724  += 1;
            mem->scale   = 0xC0;
            mem->step    = 0x20;
            arg0->state  = 1;
            /* fallthrough */
        case 1:
            if (mem->angle < D_energyball_80131194[mem->index].field_0) {
                mem->angle          = mem->angle + (u16)D_energyball_80131194[mem->index].field_2;
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
            } else {
                Gp_UpdateCoord(coord);
                arg0->work         = work;
                work->obj.ctx.recs = &work->rec;
                work->obj.coord    = coord;
                work->obj.key      = ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 +
                                ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 +
                                (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
                work->obj.radius = mem->angle >> 1;
                work->obj.flags  = 1;
                Gp_LinkObj(1, &work->obj);
                dir              = &mem->move;
                work->rec.flags  = 2;
                work->obj.flags |= 0x8000;
                arg0->state      = 2;
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx     = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy     = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz     = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                VectorNormalSS(dir, dir);
                gte_lddp(mem->step);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                mem->pos.vx = 0;
                mem->pos.vy = -(u16)D_energyball_80131194[mem->index].field_2;
                mem->pos.vz = 0;
            }
            slot->framesLeft = 2;
            tail->inner      = 0x100;
            tail->outer      = 0x1000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            r                = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->head.g     = r;
            tail->head.r     = (u16)tail->head.g >> 1;
            tail->head.b     = tail->head.g >> 1;
            sc->coord.t[0]   = coord->coord.t[0];
            sc->coord.t[1]   = coord->coord.t[1];
            sc->coord.t[2]   = coord->coord.t[2];
            sc->composeStamp = GRAPHICS_COORD_DIRTY;
            func_energyball_8013035C(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            if ((Gp_State1C->groundTrace != 0) && (Gp_TraceGroundCoord(coord, &ground) == 1)) {
                func_energyball_801307D4(&ground, mem->angle);
            }
            coord->workm.t[1] += D_energyball_80131194[mem->index].field_2 * mem->age;
            func_energyball_80130B54(coord, mem->angle,
                                     (D_energyball_80131194[mem->index].field_0 - mem->angle) / 5);
            coord->workm.t[1] -= D_energyball_80131194[mem->index].field_2 * mem->age;
            if ((u16)(Gp_StateC08.field_0 / 10) != 0x2B) {
                if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->fadeState >= 4)) {
                    if (D_80115724 > 0) {
                        D_80115724 -= 1;
                        if (D_80115724 == 0) {
                            SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                        }
                    }
                    Gp_UnlinkObj(&work->obj);
                    Gp_ReleaseState1CMem(mem, arg0);
                    return;
                }
            }
            return;
        case 2:
            if ((mem->age & 7) == 0) {
                player = &(gameGetPtrSlot(3))->extra.tmd->coords[1];
                vec.vx = player->workm.t[0] - coord->workm.t[0];
                vec.vy = player->workm.t[1] - coord->workm.t[1];
                vec.vz = player->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                mem->pos.vx = vec.vx;
                mem->pos.vy = vec.vy;
                mem->pos.vz = vec.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->pos);
                gte_rtv0();
                gte_stsv(&mem->pos);
                gte_lddp(mem->index * 0x180 + 0xA00);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
            }
            if (mem->age & 1) {
                cur          = mem->move.vx;
                mem->move.vx = (cur < mem->pos.vx) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vy;
                mem->move.vy = (cur < mem->pos.vy) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vz;
                mem->move.vz = (cur < mem->pos.vz) ? cur + 0x10 : cur - 0x10;
            }
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            slot->framesLeft = 2;
            tail->inner      = 0x100;
            tail->outer      = 0x1000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            r                = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->head.g     = r;
            tail->head.r     = (u16)tail->head.g >> 1;
            tail->head.b     = tail->head.g >> 1;
            sc->coord.t[0]   = coord->coord.t[0];
            sc->coord.t[1]   = coord->coord.t[1];
            sc->coord.t[2]   = coord->coord.t[2];
            sc->composeStamp = GRAPHICS_COORD_DIRTY;
            func_energyball_8013035C(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            if (Gp_State1C->groundTrace != 0) {
                if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                    func_energyball_801307D4(&ground, mem->angle);
                }
            }
            if ((u16)(Gp_StateC08.field_0 / 10) != 0x2B) {
                if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->fadeState >= 4)) {
                    if (D_80115724 > 0) {
                        D_80115724 -= 1;
                        if (D_80115724 == 0) {
                            SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                        }
                    }
                    Gp_UnlinkObj(&work->obj);
                    Gp_ReleaseState1CMem(mem, arg0);
                    return;
                }
            }
            if (Gp_CountRec18Hi(work->obj.ctx.recs, 0x30000) != 0) {
                spawned = Gp_SpawnEff(0x600F9, coord, 0, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
                spawned = Gp_SpawnEff(0x600F9, coord, 0x2AA, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
                spawned = Gp_SpawnEff(0x600F9, coord, 0x555, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
                snd = D_energyball_8013117C;
                SndEvt_EnqueueType6(snd[mem->index + 3], 0, 0);
                Gp_UnlinkObj(&work->obj);
                mem->angle  = (u16)D_energyball_80131194[mem->index].field_0;
                arg0->state = 3;
                return;
            }
            if (Gp_State1C->battleState != 1) {
                Gp_UnlinkObj(&work->obj);
                arg0->state = 4;
                return;
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 3:
            Gp_UpdateCoord(coord);
            func_energyball_8013035C(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            func_energyball_8012FFD0(coord, (u16)mem->angle * 2, mem->scale >> 2);
            mem->angle = mem->angle + (u16)D_energyball_80131194[mem->index].field_2;
            if (((u16)(Gp_StateC08.field_0 / 10) != 0x2B) &&
                ((Gp_StateC08.field_3 == -2) || (Gp_State1C->fadeState >= 4))) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (D_energyball_80131194[mem->index].field_0 * 2 < mem->angle) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            return;
        case 4:
            Gp_UpdateCoord(coord);
            func_energyball_8013035C(coord, mem->age, mem->angle, mem->period);
            func_energyball_8012FFD0(coord, mem->angle, mem->scale >> 2);
            func_energyball_8012FFD0(coord, (u16)mem->angle * 2, mem->scale >> 2);
            mem->angle = mem->angle - (u16)D_energyball_80131194[mem->index].field_2;
            if (((u16)(Gp_StateC08.field_0 / 10) != 0x2B) &&
                ((Gp_StateC08.field_3 == -2) || (Gp_State1C->fadeState >= 4))) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (mem->angle < D_energyball_80131194[mem->index].field_2) {
                if (D_80115724 > 0) {
                    D_80115724 -= 1;
                    if (D_80115724 == 0) {
                        SndEvt_EnqueueType7(D_energyball_8013117C[mem->index], 1);
                    }
                }
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            return;
        default:
            return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Overlay copy of `Gp_DrawRing` with a flat tint: draws an eight-segment
/// gouraud ring centred on `arg0`'s world position. The position is projected
/// through `GsWSMATRIX` by one `RTPS` and the ring is dropped when that sets a
/// negative `gte_stflg`. `arg1` is the radius in world units (scaled by 64 and
/// divided by the projected OTZ) and `arg2` the brightness: only the inner
/// vertex of each `POLY_G4` is lit, `(arg2 / 2, arg2, arg2 / 2)`, so every
/// wedge fades from green at the centre to black at the rim. Each wedge gets
/// the semi-transparent tpage of `Gp_AddTpageShift` at its OTZ.
static void func_energyball_8012FFD0(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->step = (arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2 >> 1, arg2, arg2 >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Links one frame of the energy ball's core sprite at `arg0`'s world
/// position. The position is projected through `GsWSMATRIX` by a single `RTPS`
/// and the quad is dropped when that sets a negative `gte_stflg`. `arg1` is the
/// effect's frame counter and its low bit alternates the two looks: odd frames
/// draw the raw, semi-transparent 0x428B cell, even frames the 0x428C cell
/// tinted `(0x40, 0xC0, 0x60)`. `arg3` spins the quad and `arg2` sizes it: the
/// corners sit `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
static void func_energyball_8013035C(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s32              ang;

    block         = SCRATCH_PUSH(GpFxQuadScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        if (arg1 & 1) {
            setSemiTrans(prim, 1);
            setShadeTex(prim, 1);
            prim->tpage = 0x29;
            prim->clut  = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            setRGB0(prim, 0x40, 0xC0, 0x60);
            prim->tpage = 0x29;
            prim->clut  = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            setSemiTrans(prim, 1);
        }
        ang       = arg3;
        block->dx = (((arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang       = ang + 0x400;
        block->dx = (((arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpFxQuadScratch);
}

/// Draws a ground-plane quad at `arg0`'s `workm` translation: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size (Y stays 0), rotated flat by
/// `gGfxViewCoord.workm`, then projected through `GsWSMATRIX`. One `RTPS` plus
/// one `RTPT` project the four corners; a negative `gte_stflg` drops the
/// quad. The texture is the two-frame tpage-0x28 strip at rows 0x38..0x57,
/// the frame picked by the low bit of `gDisplayState.animFrame`, tinted
/// `(0x20, 0x30, 0x20)`.
static void func_energyball_801307D4(GfxCoord* arg0, s32 arg1)
{
    OverlayGroundScratch* sc;
    POLY_FT4*             prim;
    s32                   i;
    s32                   otz;
    s32                   flag;
    s32                   u;

    sc = SCRATCH_PUSH(OverlayGroundScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        sc->vec[i].vx = D_80111E38[i].x * arg1;
        sc->vec[i].vy = 0;
        sc->vec[i].vz = D_80111E38[i].y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&sc->vec[i]);
        gte_rtv0();
        gte_stsv(&sc->vec[i]);
        sc->vec[i].vx += arg0->workm.t[0];
        sc->vec[i].vy += arg0->workm.t[1];
        sc->vec[i].vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec[0]);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_ldv3(&sc->vec[1], &sc->vec[2], &sc->vec[3]);
    gte_rtpt();
    gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
    gte_stflg(&flag);
    if (flag >= 0) {
        gte_stszotz(&otz);
        otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->r0    = 0x20;
        prim->g0    = 0x30;
        prim->b0    = 0x20;
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
        prim->x0    = sc->sxy0.vx;
        prim->y0    = sc->sxy0.vy;
        prim->x1    = sc->sxy1.vx;
        prim->y1    = sc->sxy1.vy;
        prim->x2    = sc->sxy2.vx;
        prim->y2    = sc->sxy2.vy;
        prim->x3    = sc->sxy3.vx;
        prim->y3    = sc->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(OverlayGroundScratch);
}

/// Draws the energy ball's surface: two 16-vertex rings of the same radius
/// sit `arg1 * 2` apart in `arg0`'s local Y, are rotated by its `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell
/// is one of six 0x28-wide frames picked per vertex by the jitter table
/// `D_energyball_801311A0` plus the frame counter, the quad is tinted
/// `(arg2 >> 1, arg2, arg2 >> 1)`, and a negative `gte_stflg` drops the
/// segment.
static void func_energyball_80130B54(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    s32            i;
    s32            next;
    s32            ang;
    s32            u;
    s16            idx;

    block = SCRATCH_PUSH(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (u32)(rsin(ang) * 3) >> 5;
        block->inner[i].vy = -(arg1 * 2);
        block->inner[i].vz = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx += arg0->workm.t[0];
        block->inner[i].vy += arg0->workm.t[1];
        block->inner[i].vz += arg0->workm.t[2];
        block->outer[i].vx  = (u32)(rsin(ang) * 3) >> 5;
        op                  = &block->inner[i] + 16;
        op->vy              = 0;
        op->vz              = (u32)(rcos(ang) * 3) >> 5;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx += arg0->workm.t[0];
        op->vy             += arg0->workm.t[1];
        op->vz             += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        idx = (u32)(D_energyball_801311A0[i] + gDisplayState.animFrame) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setRGB0(prim, arg2 >> 1, arg2, arg2 >> 1);
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            setSemiTrans(prim, 1);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_POP(GpBandScratch);
}

void func_energyball_8013107C(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    u8         rgb[3];
    s32        scale;
    s32        angle;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->fadeState;
    coord = arg0->extra.coordBody->coord;
    if (flag != 0) {
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1.value & 0xFFF, 0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->scale          = 0x80;
        mem->angle          = 0x100;
        arg0->state         = 1;
    }

    Gp_UpdateCoord(coord);
    rgb[0] = mem->scale >> 1;
    rgb[1] = (u8)mem->scale;
    rgb[2] = mem->scale >> 1;
    Gp_DrawBandEx(coord, mem->angle, 0x180, rgb);

    angle      = (u16)mem->angle;
    scale      = (u16)mem->scale;
    angle     += 0x80;
    scale     -= 8;
    mem->scale = scale;
    mem->angle = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
