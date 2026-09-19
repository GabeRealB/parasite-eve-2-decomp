#include "common.h"

#include "actors/actor_202600.h"
#include "actors/actors_shared_80135b58.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/inline_c.h"

void func_actor_202600_8014C774(Actor202600* arg0, s32 arg1);
void func_actor_202600_8014DA6C(Actor202600* actor);
void Actor05500_Fn03B60(Actor202600* arg0);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

/* Animation id -> slot blend value table in this overlay's own data. */
extern s16 D_actor_202600_80152830[];

/// Per-frame tick of the homing projectile, mirroring the state machine the
/// 0x5500 variant runs: while the global mode is 1 the frame is just drawn, in
/// mode 2 the model's `field_C` is raised, and otherwise the work's `field_39C`
/// state is stepped. State 0 seeds the move matrix, unlinks the work's four
/// display nodes and switches state 1; state 1 runs the release phase, counts
/// `field_39E` frames to a spawn at 0xF and a teardown at 0x3C; state 2 counts
/// down and destroys the enemy.
void func_actor_202600_8014C184(Actor202600Ctx* arg0, Actor202600* arg1)
{
    VECTOR            vec;
    Actor202600Work*  work;
    Actor202600Work*  initialWork;
    Actor202600Work*  dyingWork;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    colorCoord;
    Actor202600Obj2C* obj;
    s16               initialAnim;
    s16               dyingAnim;
    s16               releasePhase;
    s16               state;
    s32               releaseId;
    s32               initialIndex;
    s32               dyingIndex;
    u16               age;
    u16               destroyAge;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    coord = obj->field_8;
    switch ((s32)D_801153F4) {
        case 1:
            vec.vx = coord->workm.t[0];
            vec.vy = coord->workm.t[1];
            vec.vz = coord->workm.t[2];
            Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
            return;
        case 2:
            obj->field_C = 0x80;
            return;
        case 0:
        default:
            state = work->field_39C;
            switch (state) {
                case 0:
                    work->field_3A0 = 0x1000;
                    work->field_370 = coord->coord;
                    arg0->field_54  = 0;
                    Gp_UnlinkNode((GpLinkNode*)&arg0->field_10);
                    Gp_UnlinkObj(&work->field_214);
                    Gp_UnlinkObj(&work->field_294);
                    Gp_UnlinkObj(&work->field_2E4);
                    Gp_UnlinkObj(&work->field_31C);
                    releaseId = 0x37;
                    if (work->field_3C0 == 0) {
                        releaseId = 0x1A;
                    }
                    Gp_ReleaseStateF0Add((GpObj20E*)arg1, releaseId);
                    Gp_SetStateF0Byte3(2);
                    work->field_39E = 0U;
                    work->field_39C = 1;
                    Gp_SetLightMode((GpObj4C*)arg0, 1);
                    if (work->field_3BA != 0) {
                        obj->field_C = 0x80;
                    }
                    work->field_392 = 0xB;
                    initialWork     = arg1->field_1C;
                    initialIndex    = 1;
                    if (initialWork->field_392 != initialWork->field_394) {
                        initialWork->field_394 = (s16)(u16)initialWork->field_392;
                        initialWork->field_396 = 0U;
                        initialAnim            = D_actor_202600_80152830[initialWork->field_392];
                        do {
                            func_800B4114(initialWork, initialIndex, (s32)initialWork->field_392, 0,
                                          (s32)initialAnim);
                            initialIndex += 1;
                        } while (initialIndex < 8);
                    } else {
                        TOUCH_REG(initialIndex);
                        initialWork->field_396 += initialIndex;
                        do {
                            Gp_AnimTickIndex((GpAnimCtx*)initialWork, initialIndex);
                            initialIndex += 1;
                        } while (initialIndex < 8);
                    }
                    colorCoord = arg1->field_2C->field_8;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
                    return;
                case 1:
                    releasePhase = work->field_3BA;
                    if (releasePhase != 0) {
                        if (releasePhase >= 2) {
                            work->field_3BA = 0;
                            Tmd_FreeBuffers((TmdObject*)obj);
                            obj->field_C |= 4;
                            func_actor_202600_8014DA6C(arg1);
                            ActorsShared80135b58((ActorShared80135b58*)arg1);
                        } else {
                            work->field_3BA = (s16)((u16)work->field_3BA + 1);
                        }
                    }
                    Actor05500_Fn03B60(arg1);
                    age             = work->field_39E + 1;
                    work->field_39E = age;
                    if ((s16)age == 0xA) {
                        obj->field_C = 2;
                    }
                    if ((s16)work->field_39E == 0xF) {
                        Gp_SpawnEff(0x600A5, coord, 2, NULL);
                    }
                    if ((s16)work->field_39E >= 0x3C) {
                        work->field_39C = 2;
                        work->field_39E = 0U;
                        obj->field_C    = 0x80;
                    }
                    dyingWork  = arg1->field_1C;
                    dyingIndex = 1;
                    if (dyingWork->field_392 != dyingWork->field_394) {
                        dyingWork->field_394 = (s16)(u16)dyingWork->field_392;
                        dyingWork->field_396 = 0U;
                        dyingAnim            = D_actor_202600_80152830[dyingWork->field_392];
                        do {
                            func_800B4114(dyingWork, dyingIndex, (s32)dyingWork->field_392, 0,
                                          (s32)dyingAnim);
                            dyingIndex += 1;
                        } while (dyingIndex < 8);
                    } else {
                        TOUCH_REG(dyingIndex);
                        dyingWork->field_396 += dyingIndex;
                        do {
                            Gp_AnimTickIndex((GpAnimCtx*)dyingWork, dyingIndex);
                            dyingIndex += 1;
                        } while (dyingIndex < 8);
                    }

                    colorCoord = arg1->field_2C->field_8;
                    vec.vx     = colorCoord->workm.t[0];
                    vec.vy     = colorCoord->workm.t[1];
                    vec.vz     = colorCoord->workm.t[2];
                    Gp_UpdateActorColor((GpEnemy*)arg1->field_20, &vec, 0, 0);
                    return;

                case 2:
                    destroyAge      = work->field_39E + 1;
                    work->field_39E = destroyAge;
                    if ((s16)destroyAge >= 0x3C) {
                        Gp_DestroyEnemy((GpEnemy*)arg0, (Task*)arg1);
                    }
                    return;
            }
            break;
    }
}

/// Per-frame tick of the homing projectile: while the global mode is 1 the
/// frame is just drawn, in mode 2 nothing happens at all, and otherwise the
/// work is stepped. A live collision record whose kind is not 0x10 drops the
/// object's 0x8000 linked bit and wipes the record, which sends the tick
/// straight past the frame counter. Every other frame the work's flags mirror
/// the low two bits of the counter, the coordinate is advanced along its own
/// forward axis by `field_3A`, and the counter is bumped; at 0xF frames the
/// object is unlinked and the actor switches to state 2, otherwise `field_3A`
/// decays by an LCG-derived 0..0x1F and clamps at zero.
void func_actor_202600_8014C5A0(Actor202600Ctx* arg0, Actor202600* arg1)
{
    Actor202600Work* work;
    GsCOORDINATE2*   coord;
    s16              age;
    s16              speed;
    s32              contact;
    u16              flags;
    u32              random;

    coord = arg1->field_2C->field_8;
    work  = arg1->field_1C;
    switch ((s32)D_801153F4) {
        case 1:
            func_actor_202600_8014C774(arg1, work->field_38);
            return;
        default:
        default_case:
            contact = work->rec.key;
            if (contact != 0) {
                if ((contact & 0xFFFF0000) != 0x100000) {
                    work->obj.flags &= 0x7FFF;
                    Gp_ClearRec18Occupied(&work->rec);
                    goto block_7;
                }
                goto block_11;
            }
        block_7:
            if (!((u16)work->field_38 & 3)) {
                flags = work->obj.flags | 0xC000;
            } else {
                flags = work->obj.flags & 0x3FFF;
            }
            work->obj.flags    = flags;
            coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_3A) >> 0xC;
            coord->coord.t[1] += (s32)(coord->coord.m[1][2] * work->field_3A) >> 0xC;
            coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_3A) >> 0xC;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_202600_8014C774(arg1, work->field_38);
            age            = (u16)work->field_38 + 1;
            work->field_38 = age;
            if (age >= 0xF) {
            block_11:
                Gp_UnlinkObj(&work->obj);
                arg1->field_30 = 2;
                return;
            }
            random         = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState    = random;
            speed          = (u16)work->field_3A - ((random >> 0x10) & 0x1F);
            work->field_3A = speed;
            if (speed < 0) {
                work->field_3A = 0;
            }
            return;
        case 0:
            goto default_case;
        case 2:
            return;
    }
}

/// Projects one frame's sprite into the scratch quad and, when the depth
/// clears the near plane, emits the semi-transparent `POLY_FT4` for it. The
/// model whose texture is drawn is the *parent* task's (`Task::parent`), not
/// this actor's, so the atlas and tpage come from whoever spawned it.
void func_actor_202600_8014C774(Actor202600* actor, s32 frame)
{
    POLY_FT4*               poly;
    GsCOORDINATE2*          coord;
    s32                     depth;
    s32                     screen;
    s32                     y;
    s32                     radius;
    s32                     x;
    s32                     bottom;
    s32                     top;
    s32                     left;
    s32                     right;
    Actor202600QuadScratch* scratchEnd;
    Actor202600QuadScratch* s;
    Actor202600TextureObj*  texture;
    Actor202600Uv*          uv;
    SVECTOR*                projection;

    scratchEnd                     = (Actor202600QuadScratch*)*(u8**)PSX_SCRATCH_ADDR(0x3FC);
    coord                          = actor->field_2C->field_8;
    actor                          = (Actor202600*)((Task*)actor)->parent;
    texture                        = (Actor202600TextureObj*)actor->field_2C;
    scratchEnd[-1].p[0].vx         = (u16)coord->workm.t[0];
    s                              = scratchEnd - 1;
    s->p[0].vy                     = (u16)coord->workm.t[1];
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) = (u8*)s;
    s->p[0].vz                     = (u16)coord->workm.t[2];
    projection                     = &s->p[0];
    SOFT_TOUCH_REG(projection);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projection);
    __asm__ volatile("nop; nop; .word 0x4a180001");
    gte_stsxy(&scratchEnd[-1].screen);
    gte_stszotz(&scratchEnd[-1].depth);
    depth = s->depth;
    if (depth >= 0x14) {
        radius                 = (s32)(D_actor_202600_801528B8[frame] * 0x300) / depth;
        poly                   = gGpuPrimCursor;
        screen                 = s->screen;
        gGpuPrimCursor         = (u8*)poly + 0x28;
        x                      = screen & 0xFFFF;
        y                      = screen >> 0x10;
        left                   = x - radius;
        top                    = y - radius;
        right                  = x + radius;
        bottom                 = y + radius;
        scratchEnd[-1].p[0].vx = left;
        s->p[0].vy             = top;
        s->p[0].vz             = 0;
        s->p[1].vx             = right;
        s->p[1].vy             = top;
        s->p[1].vz             = 0;
        s->p[2].vx             = left;
        s->p[2].vy             = bottom;
        s->p[2].vz             = 0;
        s->p[3].vx             = right;
        s->p[3].vy             = bottom;
        s->p[3].vz             = 0;
        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setRGB0(poly, 0x80, 0x80, 0x80);
        setShadeTex(poly, 1);
        poly->tpage = (s16)(((s32)(((texture->field_24 << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        poly->clut  = (s16)(((s32)(texture->field_25 << 0x18) >> 0x12) + 0x3D40);
        uv          = &D_actor_202600_80152898[frame >> 1];
        poly->u0    = (u8)uv->u;
        poly->v0    = (u8)uv->v;
        poly->u1    = (s8)(uv->u + 0x1F);
        poly->v1    = (u8)uv->v;
        poly->u2    = (u8)uv->u;
        poly->v2    = (s8)(uv->v + 0x1F);
        poly->u3    = (s8)(uv->u + 0x1F);
        poly->v3    = (s8)(uv->v + 0x1F);
        poly->x0    = (u16)scratchEnd[-1].p[0].vx;
        poly->y0    = (u16)s->p[0].vy;
        poly->x1    = (u16)s->p[1].vx;
        poly->y1    = (u16)s->p[1].vy;
        poly->x2    = (u16)s->p[2].vx;
        poly->y2    = (u16)s->p[2].vy;
        poly->x3    = (u16)s->p[3].vx;
        poly->y3    = (u16)s->p[3].vy;
        addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
    }
    *(u8**)PSX_SCRATCH_ADDR(0x3FC) += 0x28;
}
