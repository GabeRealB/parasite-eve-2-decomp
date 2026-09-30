#include "pe/pyrokinesis.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Collision pair allocated by `func_pyrokinesis_8012EF48` (`memCalloc(0x58)`)
/// and stored in `Task::work`. `obj` is linked on list 1 and carries the
/// packed combo id, `obj2` on list 7 with the 0x4400 flags the cone uses to
/// probe for a wall; both point `field_C` at the one-element `rec` table
/// (terminator `field_0 = 2`).
typedef struct PyroWork {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionBody    obj2;
    /* 0x40 */ WorldCollisionContact rec;
} PyroWork;
STATIC_ASSERT_SIZEOF(PyroWork, 0x58);

static void func_pyrokinesis_80130130(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_pyrokinesis_801304C4(GfxCoord* arg0, s32 arg1);
static void func_pyrokinesis_80130848(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);

/// The `SndEvt_EnqueueType6` id of the ignition roar, three per PE level,
/// indexed by `GpEffWork.index * 3 + Task::spawnArg1` (level by cast variant).
static s32 D_pyrokinesis_80131DD8[] = {
    0xE00B0002,
    0xE00B0002,
    0xE00B0002,
    0xE00E0002,
    0xE00E0002,
    0xE00E0002,
    0xE0110002,
    0xE0110003,
    0xE0110004,
};

static void func_pyrokinesis_80130DC0(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_pyrokinesis_801312B4(GfxCoord* arg0, s16 arg1, s32 arg2, s16 arg3);
static void func_pyrokinesis_80131784(GfxCoord* arg0, s16 arg1, s32 arg2, s32 arg3);

/// Per-flame jitter of the cone, one 8-bit LCG roll each, re-rolled as a block
/// when the cast starts.
static s16 D_pyrokinesis_80131DFC[16] = { 0 };

static void func_pyrokinesis_8012FC34(GfxCoord* arg0, s16 arg1, s16 arg2);

/// Runs one frame of the pyrokinesis cast: a five-state machine driven by
/// `Task::state`. State 0 copies the player rotation onto the effect
/// coordinate, rotates the combo-scaled launch offset into that frame, rolls
/// the 16 per-flame jitters, plays the roar picked by combo level and cast
/// variant, and links a `PyroWork` collision pair (list 1 + list 7) whose
/// packed id is the combo digits plus `0x28000`. State 1 walks the coordinate
/// by that offset each frame, redraws the cone and ring, parks the room light
/// slot on it and burns until the `D_80113D40` budget for the combo level runs
/// out. A `0x30000` hit on `obj` bursts into three `0x600F6` flames and moves
/// to state 3 (or 4 for cast variant 2); a `0x100000` hit on `obj2` means a
/// wall, which drops to state 2 and fades the cone out. States 3 and 4 grow
/// the two rings until they pass the combo radius, state 4 first stepping the
/// brightness down by 8 a frame. Any state releases if the player is dying
/// (`Gp_StateC08.field_3` / `Gp_StateC08.field_3`) or the room is fading (`Gp_State1C`).
void func_pyrokinesis_8012EF48(Task* arg0)
{
    GpEffWork*            mem;
    GfxCoord*             coord;
    PyroWork*             work;
    ModelObjectCoordBody* body;
    GfxCoord*             player;
    GpCoord64*            base;
    GfxCoord*             slotc;
    GpPointLight*         slot;
    GpMtxWords*           dstm;
    GpMtxWords*           srcm;
    GpEffWork*            spawned;
    GfxCoord              ground;
    u8                    rgb[3];
    s32                   i;
    s32                   pan;
    s16                   peEffectControl;
    s32                   tick;
    s32                   radius;
    s32                   next;
    s16                   amp;

    work     = (PyroWork*)arg0->work;
    mem      = arg0->spawnArg2.pointer;
    body     = arg0->extra.coordBody;
    coord    = body->coord;
    mem->age = mem->age + 1;
    base     = Gp_RoomCoords;
    slotc    = &base->light.head.transform.coord;
    slot     = &base->light;
    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.field_3 == -2) || ((peEffectControl = Gp_State1C->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            work = memCalloc(0x58, 0);
            if (work == NULL) {
                mem->age = 0;
                return;
            }
            player              = (gameGetPtrSlot(3))->extra.tmd->coords;
            dstm                = (GpMtxWords*)&coord->coord;
            srcm                = (GpMtxWords*)&player->coord;
            dstm->m00_m01       = srcm->m00_m01;
            dstm->m02_m10       = srcm->m02_m10;
            dstm->m11_m12       = srcm->m11_m12;
            dstm->m20_m21       = srcm->m20_m21;
            dstm->m22           = srcm->m22;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            mem->move.vx = 0;
            mem->move.vy = 0;
            mem->move.vz = (Gp_StateC08.field_0 % 10) * 64 + 0x1C0;
            gte_SetRotMatrix((MATRIX*)srcm);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            for (i = 0; i < 16; i++) {
                Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
                D_pyrokinesis_80131DFC[i] = ((u32)Gp_LcgState >> 16) & 0xFF;
            }
            mem->scale  = 0xC0;
            mem->angle  = 0x500;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->period = ((u32)Gp_LcgState >> 16) & 0xFFF;
            mem->index  = (Gp_StateC08.field_0 % 10) - 1;
            pan         = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(D_pyrokinesis_80131DD8[mem->index * 3 + arg0->spawnArg1.value], pan,
                                (s8)gpGetObjDepth(coord));
            Gp_SpawnPadLerp((s16)(mem->index * 2 + 8), 0xFF, 8);
            if (mem->index == 1) {
                arg0->spawnArg1.value = 1;
            } else if (arg0->spawnArg1.value == 1) {
                arg0->spawnArg1.value = 0;
            }
            arg0->work                 = work;
            work->obj.coord            = coord;
            work->obj.context.contacts = &work->rec;
            work->obj.key              = ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 +
                            ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 +
                            (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
            work->obj.radius = mem->angle;
            work->obj.flags  = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(1, &work->obj);
            work->rec.flags             = 2;
            work->obj2.coord            = coord;
            work->obj2.context.contacts = &work->rec;
            work->obj2.key              = 0;
            work->obj.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->obj2.radius           = (s16)((u16)mem->angle << 16 >> 19);
            work->obj2.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(7, &work->obj2);
            work->obj2.flags = (work->obj2.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_SpawnEff(0x60011, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            arg0->state = 1;
            func_pyrokinesis_80130848(coord, mem->age, mem->angle, mem->period);
            func_pyrokinesis_80130130(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (Gp_CountRec18Hi(work->obj.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&work->obj);
                radius     = (mem->index << 9) + 0x380;
                mem->angle = radius;
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(0x600F6, coord, i, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->task);
                    }
                }
                next = 3;
                if (arg0->spawnArg1.value == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (Gp_FindRec18(work->obj2.context.contacts, 0x100000) != 0) {
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 1:
            if ((Gp_StateC08.field_3 == -2) || ((peEffectControl = Gp_State1C->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            radius              = (mem->index << 9) + 0x380;
            mem->angle          = radius;
            work->obj.radius    = radius;
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            func_pyrokinesis_80130848(coord, mem->age, mem->angle, mem->period);
            func_pyrokinesis_80130130(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (arg0->spawnArg1.value != 0) {
                func_pyrokinesis_80131784(coord, mem->age, mem->angle, 0);
                func_pyrokinesis_80131784(coord, mem->age, mem->angle, 1);
            }
            if (mem->age < 0x1E) {
                spawned = Gp_SpawnEff(0x60069, coord, 0, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (Gp_State1C->groundTraceEnabled != 0) {
                if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                    func_pyrokinesis_801304C4(&ground, mem->angle);
                }
            }
            base->framesLeft    = 4;
            slot->inner         = (mem->index << 9) + 0x200;
            slot->outer         = slot->inner * 16;
            Gp_LcgState         = Gp_LcgState * 5 + 0x71357911;
            amp                 = (((u32)Gp_LcgState >> 16) & 0x700) + 0x800;
            slot->head.color.r  = amp;
            slot->head.color.g  = (u16)slot->head.color.r >> 1;
            slot->head.color.b  = slot->head.color.r >> 2;
            slotc->coord.t[0]   = coord->coord.t[0];
            slotc->coord.t[1]   = coord->coord.t[1];
            slotc->coord.t[2]   = coord->coord.t[2];
            slotc->composeStamp = GRAPHICS_COORD_DIRTY;
            if (Gp_CountRec18Hi(work->obj.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&work->obj);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(0x600F6, coord, i, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->task);
                    }
                }
                next = 3;
                if (arg0->spawnArg1.value == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (Gp_FindRec18(work->obj2.context.contacts, 0x100000) != 0) {
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            tick = mem->age;
            if (tick * 6 > (Gp_AttachParams + 1)[mem->index].dispatch.field_4) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            if (tick < 0x1F) {
                Gp_ClearRec18Occupied(&work->rec);
                return;
            }
            Gp_UnlinkObj(&work->obj);
            Gp_UnlinkObj(&work->obj2);
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        case 2:
            if ((Gp_StateC08.field_3 == -2) || ((peEffectControl = Gp_State1C->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->obj);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            Gp_UpdateCoord(coord);
            radius           = (u16)mem->angle - 0x40;
            mem->angle       = radius;
            work->obj.radius = radius;
            func_pyrokinesis_80130848(coord, mem->age, mem->angle, mem->period);
            func_pyrokinesis_80130130(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (mem->angle >= 0x81) {
                spawned = Gp_SpawnEff(0x60069, coord, 0, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (Gp_CountRec18Hi(work->obj.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&work->obj);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(0x600F6, coord, i, NULL);
                    if (spawned != NULL) {
                        Task_Reparent(arg0, spawned->task);
                    }
                }
                next = 3;
                if (arg0->spawnArg1.value == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (mem->angle < 0x80) {
                Gp_UnlinkObj(&work->obj);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 3:
            if ((Gp_StateC08.field_3 == -2) || ((peEffectControl = Gp_State1C->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_80130848(coord, mem->age, mem->angle, mem->period);
            func_pyrokinesis_80130130(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            func_pyrokinesis_80130130(coord, (s16)((u16)mem->angle * 2),
                                      (s16)((u16)mem->scale << 16 >> 17));
            mem->angle = mem->angle + 0x40;
            if (mem->angle > ((mem->index << 9) + 0x580)) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            return;
        case 4:
            if ((Gp_StateC08.field_3 == -2) || ((peEffectControl = Gp_State1C->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_80130848(coord, mem->age, mem->angle, mem->period);
            func_pyrokinesis_80130130(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            func_pyrokinesis_80130130(coord, (s16)((u16)mem->angle * 2),
                                      (s16)((u16)mem->scale << 16 >> 17));
            mem->angle = mem->angle + 0x40;
            if (mem->angle > ((mem->index << 9) + 0x580)) {
                if (mem->scale >= 9) {
                    mem->scale = mem->scale - 8;
                    return;
                }
                Gp_UnlinkObj(&work->obj2);
                Gp_ReleaseState1CMem(mem, arg0);
            }
            return;
    }
}

void func_pyrokinesis_8012FAC8(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        scene;
    s16        flag;
    s32        state;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.field_3 != -2) {
        scene = Gp_State1C->battleState;
        if (scene == ROOM_EFFECT_BATTLE_ENGAGED) {
            flag = Gp_State1C->peEffectControl;
            if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                    return;
                }
                mem->age = mem->age + 1;
                Gp_UpdateCoord(coord);
                state = arg0->state;
                if (state == scene) {
                    goto L_case1;
                }
                if (state < 2) {
                    if (state == 0) {
                        goto L_case0;
                    }
                    return;
                }
                if (state == 2) {
                    goto L_case2;
                }
                if (state == 3) {
                    goto L_release;
                }
                return;
            L_case0:
                Gp_SpawnEff(0x80060010, coord, 0, 0);
                arg0->state = scene;
                return;
            L_case1:
                if (mem->age == 8) {
                    Gp_SpawnEff(0x80060010, coord, 1, 0);
                    arg0->state = 2;
                }
                return;
            L_case2:
                if (mem->age == 0x10) {
                    Gp_SpawnEff(0x80060000 | 0x10, coord, 2, 0);
                    arg0->state = 3;
                }
                return;
            }
        }
    }
L_release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws the pyrokinesis flame cone: a 16-vertex inner ring of radius `arg1`
/// at depth `0x100` and an outer ring of radius `arg1 + 0x100` at depth 0 are
/// built in the XY plane by `rsin` / `rcos`, rotated by `arg0`'s `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one `POLY_G4`. The inner edge carries the `arg2`
/// ramp `(arg2, arg2 >> 1, arg2 >> 2)` and the outer edge fades to black; a
/// negative `gte_stflg` drops the segment.
static void func_pyrokinesis_8012FC34(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u32            ramp;
    u8             red;
    u8             grn;
    u8             blu;

    /* The ramp halves are unsigned: writing them as `(u16)arg2 >> 1` folds the
     * widening into an `andi`, where the ROM shifts the value up and back. */
    ramp  = (u32)arg2 << 16;
    red   = arg2;
    grn   = ramp >> 17;
    blu   = ramp >> 18;
    r1    = arg1 + 0x100;
    block = SCRATCH_STACK_RESERVE_BLOCK(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = (u16)block->inner[i].vx + (u16)arg0->workm.t[0];
        block->inner[i].vy = (u16)block->inner[i].vy + (u16)arg0->workm.t[1];
        block->inner[i].vz = (u16)block->inner[i].vz + (u16)arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = (u16)block->outer[i].vx + (u16)arg0->workm.t[0];
        op->vy             = (u16)op->vy + (u16)arg0->workm.t[1];
        op->vz             = (u16)op->vz + (u16)arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
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
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpBandScratch);
}

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / otz`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring.
static void func_pyrokinesis_80130130(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

    block         = SCRATCH_STACK_RESERVE_BLOCK(GpRingScratch);
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
            setRGB2(prim, arg2, arg2 >> 1, arg2 >> 2);
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
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Draws the scorch mark the cone leaves on the floor: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size, laid flat into view space with
/// `gGfxViewCoord.workm` (rotation only, translation from `GsWSMATRIX`) and
/// offset by `arg0->workm.t`, then its four corners are projected through
/// `GsWSMATRIX`. On a non-negative `gte_stflg` it queues one semi-transparent
/// `POLY_FT4` (tpage 0x28, clut 0x428C) tinted `(0x30, 0x20, 0x20)`; the frame
/// counter's low bit picks between two 0x1F-wide UV columns at v = 0x38..0x57.
/// Same 0x38 scratch block and body as `Gp_DrawEffSprite7C`.
static void func_pyrokinesis_801304C4(GfxCoord* arg0, s32 arg1)
{
    GpQuadScratch* block;
    s32            i;
    POLY_FT4*      prim;
    s32            u;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        block->vec[i].vx = D_80111E38[i].x * arg1;
        block->vec[i].vy = 0;
        block->vec[i].vz = D_80111E38[i].y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        block->vec[i].vx += arg0->workm.t[0];
        block->vec[i].vy += arg0->workm.t[1];
        block->vec[i].vz += arg0->workm.t[2];
    }

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
        block->otz++;
        prim           = gGpuPrimCursor;
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
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpQuadScratch);
}

/// Draws one billboard flame quad: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and a single semi-transparent `POLY_FT4` (tpage 0x29, clut
/// 0x428C) is spun around it, its four corners offset by the rotated
/// half-extents `(arg2 * 55 / otz) * rsin|rcos` at `arg3` and `arg3 + 0x400`.
/// `arg1`'s low bit picks between two 0x37-wide UV columns at v = 0xC8..0xFF.
/// A negative `gte_stflg` drops the quad.
static void func_pyrokinesis_80130848(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    s32              ang;
    s32              u70;
    s32              t;
    u16              vz;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(GpFxQuadScratch)     = block;
    block->vec.vz                             = vz;
    vecp                                      = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        ang = (s16)arg3;
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x428C;
        t           = (arg1 & 1) * 56;
        u70         = t + 0x70;
        prim->u0    = u70;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->u1    = t - 0x59;
        prim->u2    = u70;
        prim->v2    = 0xFF;
        prim->u3    = t - 0x59;
        prim->v3    = 0xFF;
        block->dx   = ((((s16)arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        prim->y3    = block->sy + (u16)block->dy;
        ang         = ang + 0x400;
        block->dx   = ((((s16)arg2 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg2 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

void func_pyrokinesis_80130C54(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        temp_a1;
    s32        y;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->peEffectControl;
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            mem->age = mem->age + 1;
            if (arg0->state == 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = -(((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                arg0->state  = 1;
            }
            y                   = coord->coord.t[1] + mem->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            Gp_UpdateCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            temp_a1 = mem->index;
            if (temp_a1 < 8) {
                if (mem->age & 1) {
                    func_pyrokinesis_80130DC0(coord, temp_a1, 0x300, mem->scale);
                }
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Flame sprite, identical in Pyrokinesis and Combustion. Links one frame of the flame
/// at `arg0`'s world position: the position is projected through `GsWSMATRIX`
/// by a single `RTPS` and the quad is dropped when that sets a negative
/// `gte_stflg`. `arg1` picks one of the 0x20-wide texture frames on tpage
/// 0x2A, `arg3` spins the quad and `arg2` sizes it: the corners sit
/// `arg2 * 31 / otz` from the projected centre along `arg3` and `arg3 + 0x400`,
/// so the sprite shrinks with depth. Same shape as the gameplay
/// `Gp_DrawFxQuad`, with the CLUT fixed at 0x42C2 instead of picked from
/// `Gp_QuadClutX`.
static void func_pyrokinesis_80130DC0(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    block->vec.vz                             = vz;
    SCRATCH_STACK_CURSOR(GpFxQuadScratch)     = block;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C2;
        u0          = arg1 << 5;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

void func_pyrokinesis_801311B8(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s32        scale;
    s32        angle;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->peEffectControl;
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            if (arg0->state == 0) {
                Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1.value, 0);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                mem->scale          = 0x80;
                mem->angle          = 0x100;
                arg0->state         = 1;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_801312B4(coord, mem->angle, 0x100, mem->scale);
            angle      = (u16)mem->angle;
            scale      = (u16)mem->scale;
            angle     += 0x80;
            scale     -= 8;
            mem->scale = scale;
            mem->angle = angle;
            if ((s16)scale >= 9) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws the pyrokinesis flame band: two 16-vertex rings of radius `arg1`
/// and `arg1 + arg2` are built in the XZ plane by `rsin` / `rcos`, rotated by
/// `arg0`'s `workm` and offset by its translation, then each of the 16
/// segments is projected through `GsWSMATRIX` as one `POLY_G4`. The inner
/// edge carries the `arg3` ramp `(arg3, arg3 >> 1, arg3 >> 2)` and the outer
/// edge fades to black; a negative `gte_stflg` drops the segment.
static void func_pyrokinesis_801312B4(GfxCoord* arg0, s16 arg1, s32 arg2, s16 arg3)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;

    r1    = arg1 + arg2;
    block = SCRATCH_STACK_RESERVE_BLOCK(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx += arg0->workm.t[0];
        block->inner[i].vy += arg0->workm.t[1];
        block->inner[i].vz += arg0->workm.t[2];
        block->outer[i].vx  = (rsin(ang) * r1) >> 12;
        op                  = &block->inner[i] + 16;
        op->vy              = 0;
        op->vz              = (rcos(ang) * r1) >> 12;
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
            setPolyG4(prim);
            setRGB0(prim, arg3, arg3 >> 1, arg3 >> 2);
            setRGB1(prim, arg3, arg3 >> 1, arg3 >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
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
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpBandScratch);
}

/// Draws the pyrokinesis flame tube: two 16-vertex rings in `arg0`'s local XY
/// plane, the rim of radius 0x200 or 0x480 sunk `-back` along local Z and the
/// hub of radius 0x80 or 0x40 at Z=0. `arg3` non-zero picks the short fat
/// tube (`back = arg2 * 2 + arg1 * 256`); zero the long thin one
/// (`back = arg2 + arg1 * 16`). Each of the 16 segments is projected through
/// `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell is one of
/// six 0x28-wide frames picked per vertex by `D_pyrokinesis_80131DFC[i]` plus
/// `arg1`, and a negative `gte_stflg` drops the segment.
static void func_pyrokinesis_80131784(GfxCoord* arg0, s16 arg1, s32 arg2, s32 arg3)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    s32            rimRad;
    s32            hubRad;
    s32            rimSize;
    s32            hubSize;
    s32            i;
    s32            next;
    s32            ang;
    s32            u0;
    u16            back;
    MATRIX*        rot;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpBandScratch);
    if (arg3 != 0) {
        back    = (arg2 << 1) + (arg1 << 8);
        hubSize = 0x80;
        rimSize = 0x200;
    } else {
        back    = arg2 + (arg1 << 4);
        hubSize = 0x40;
        rimSize = 0x480;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    i      = 0;
    rimRad = rimSize;
    rot    = &arg0->workm;
    hubRad = hubSize;
    for (; i < 0x10; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * rimRad) >> 12;
        block->inner[i].vy = (rcos(ang) * rimRad) >> 12;
        block->inner[i].vz = -back;
        gte_SetRotMatrix(rot);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx += arg0->workm.t[0];
        block->inner[i].vy += arg0->workm.t[1];
        block->inner[i].vz += arg0->workm.t[2];
        block->outer[i].vx  = (rsin(ang) * hubRad) >> 12;
        op                  = &block->inner[i] + 16;
        op->vy              = (rcos(ang) * hubRad) >> 12;
        op->vz              = 0;
        gte_SetRotMatrix(rot);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx += arg0->workm.t[0];
        op->vy             += arg0->workm.t[1];
        op->vz             += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 0x10; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
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
            prim->clut  = 0x4282;
            setRGB0(prim, 0x30, 0x30, 0x30);
            setSemiTrans(prim, 1);
            u0 = (s16)((D_pyrokinesis_80131DFC[i] + arg1) % 6) * 40;
            setUV4(prim, u0, 0x60, u0 + 0x27, 0x60, u0, 0x87, u0 + 0x27, 0x87);
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
    SCRATCH_STACK_RELEASE_BLOCK(GpBandScratch);
}

void func_pyrokinesis_80131CE4(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s32        scale;
    s32        angle;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->peEffectControl;
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            mem->age = mem->age + 1;
            if (arg0->state == 0) {
                mem->scale  = 0xC0;
                mem->angle  = 0x100;
                arg0->state = 1;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_8012FC34(arg0->extra.coordBody->coord, mem->angle, mem->scale);
            angle      = (u16)mem->angle;
            scale      = (u16)mem->scale;
            angle     += 0x40;
            scale     -= 0x10;
            mem->scale = scale;
            mem->angle = angle;
            if ((s16)scale >= 0x10) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}
